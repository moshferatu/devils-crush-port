// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// The VDC interrupt handler. 00:E058 takes the interrupt, reads the VDC status
// register at $0000 into $01 and dispatches on it: the VBlank bit sends it to
// 00:E0EB, everything else to 00:E136.
//
// One instruction of that is unportable, and for four slices the whole
// dispatcher was recorded as blocked on its account. `LDA $0000` cannot be
// transcribed for two reasons that are worth keeping apart. The value is
// device state a replacement callback cannot see -- that is the reason usually
// given, and it is the weaker one, because the entry could carry a peek the
// way it carries the frame's pad state. The reason that does not lift is that
// the read is the acknowledge: it clears every latched status source and is
// the only way the VDC's interrupt line falls. The replacement contract is a
// write channel, and a write to $0000 selects a register rather than
// acknowledging one, so a transcript that predicted the byte and skipped the
// read would hand the core back an interrupt that had never been taken.
//
// The routine was the wrong unit. The instruction is unportable; the routine
// is not. The four register saves above the read touch nothing but the stack
// page, and below it the dispatcher never needs the device again, because the
// byte it branches on is in the accumulator by then. So the head walks to the
// read and stops, the core executes it -- at the right cycle, with its
// acknowledge -- and the tail picks the arm up from the accumulator.
//
// Nine entry points share one walk: the dispatcher's own two halves, the two
// arms it hands off to, the two addresses the VBlank arm's calls come back to,
// the secondary handler's prologue and epilogue, and the pad reader at
// 00:E195 described further down. The handler's work is interleaved with calls
// the core keeps, which is why there are so many:
//
//   00:E058  the dispatcher's head: push P, A, X and Y, then stop in front of
//            the LDA $0000
//   00:E05F  its tail: stash the status byte at $01, test the VBlank bit and
//            pick the arm from the raster mode at $54
//
//   00:E0EB  VBlank prologue: program VDC registers $08/$07/$05 from the RAM
//            shadows and clear bit 6 of $02, then stop at the JSR $EB30
//   00:E117  after the pad read: bump the frame counter at $00 and either take
//            the bank wrapper at 00:ECC6 or set $DA and fall into the epilogue
//   00:E123  after that wrapper: restore MPR6 and join the epilogue
//   00:E136  the non-VBlank arm: on scanline modes 0 and 2 reprogram the split
//            registers from $0B/$0C, then bump the 16-bit tick at $222A
//   00:E179  the secondary handler's prologue, down to its JSR $ECC6
//   00:E183  its epilogue: restore MPR6, rewrite $0000 twice and unwind
//
// The epilogue at 00:E12C is shared by every arm. It ends one instruction short
// of the RTI, in front of the PLP at 00:E134, and 00:E193 is the second such
// stop. That is not the usual "let the core execute the return": PLP is the one
// instruction here that changes the I flag, and the core's LastCycle -- where
// the IRQ is sampled -- runs before PLP pops. A checkpoint carries one register
// set and samples the interrupt with it, so a transcript covering PLP samples
// with the restored I rather than the one the instruction ran under, and an
// interrupt that assembly services between the PLP and the RTI is missed. Two
// instructions of coverage buy exactness.
//
// That matters because the handler does not run masked throughout. It is
// entered with I set, but bank 45 ends three of its critical sections with an
// unconditional CLI rather than restoring the caller's mask -- 45:4872, 45:49F2
// and 45:47EE, 376 executions between them -- so the music tick below the
// wrapper at 00:ECC6 hands the handler back with interrupts enabled. Counting
// interrupt entries in an assembly write trace of the stack page puts 78 of
// them inside the six body walks, every one at or after 00:E123 and none
// before a JSR $ECC6. The dispatcher's two halves run before any of that and
// are masked throughout, so none land there.
//
// The stores at $0000, $0002 and $0003 are the VDC's register-select and data
// ports. They are on the write channel already, and the core prices the cycle
// the VDC steals from each one, so they are transcribed at eighteen master
// cycles and sample the interrupt two CPU cycles early.
constexpr std::uint8_t kBank = 0x00U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kBreak = 0x10U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF;
// both are ordinary work RAM reached through segment 1.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status,
    const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    result = static_cast<std::uint8_t>(
        result | static_cast<std::uint8_t>(value & kNegative));
    return result;
}

[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0xE058U || logical_pc == 0xE05FU
        || logical_pc == 0xE0EBU || logical_pc == 0xE117U
        || logical_pc == 0xE123U || logical_pc == 0xE136U
        || logical_pc == 0xE179U || logical_pc == 0xE183U
        || logical_pc == 0xE195U;
}

// Where each walk stops. Three are the calls the handler exists to make -- the
// palette upload, the pad reader and the bank wrapper. Two are the PLP that
// restores the interrupted status byte, one instruction short of each RTI, for
// the reason in the note above. One is the pad reader's own RTS. The last is
// the LDA $0000 the dispatcher's head stops in front of. The core executes all
// seven.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xE05CU || logical_pc == 0xE111U
        || logical_pc == 0xE120U || logical_pc == 0xE134U
        || logical_pc == 0xE180U || logical_pc == 0xE193U
        || logical_pc == 0xE1F7U;
}

// The joypad at $1000, which is the one device this module reads rather than
// only writes.
//
// A replacement callback cannot read a device register, and that is why the
// pad reader sat unported while the rest of the handler around it went native.
// The way out is not to read it: the byte the core would return is a pure
// function of the frame's pad state, which the entry now carries, and of the
// two latch lines the routine itself drives. Both are things the walk knows.
//
// INPUT_Read answers with the selected pad's four button lines in bits 0-3,
// active low, over a fixed high nibble: bit 7 set because no CD unit is
// attached, bits 4 and 5 always set, and bit 6 -- the region bit -- left
// clear. The byte is therefore $B0 | nibble. SEL picks which nibble: set
// reads the four directions, clear reads I, II, SELECT and RUN.
//
// Only bits 0-3 survive into memory, because the routine masks the byte both
// times it reads. The high bits still have to be right: every instruction here
// carries a checkpoint, and a checkpoint publishes the status register, so an
// interrupt landing on the load would restore an N flag derived from the whole
// byte.
constexpr std::uint16_t kJoypadPort = 0x1000U;

[[nodiscard]] std::uint8_t joypad_read(
    const std::uint16_t buttons,
    const bool select)
{
    const auto nibble = static_cast<std::uint8_t>(
        select ? (buttons >> 4U) & 0x0FU : buttons & 0x0FU);
    return static_cast<std::uint8_t>(
        0xB0U | static_cast<std::uint8_t>(0x0FU ^ nibble));
}

} // namespace

std::optional<RoutineReplacement> replace_vblank_handler(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The eight lines a two-button pad sends all live in bits 0-7. A
    // six-button pad announces itself in bit 12 and makes the port answer
    // from a second set of lines; nothing here models that, so refuse rather
    // than answer for a pad this walk does not understand.
    if ((entry.input.player_one & 0xFF00U) != 0U
        || (entry.input.player_two & 0xFF00U) != 0U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is something the
    // handler is written to run under; T would redirect its loads through zero
    // page. Both are clear on entry to an interrupt in any case.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }
    // The VDC ports only reach the device while the hardware page is mapped at
    // segment 0 -- the same condition the core's write validator imposes.
    if (entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement;
    CpuRegisters registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool overflow = false;

    const auto read_ram =
        [&ram, &overflow](const std::uint16_t address) -> std::uint8_t {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return 0U;
            }
            return ram[address - kWorkRamBase];
        };

    // Stores land on their instruction's final cycle, so a write shares the
    // offset and the instruction of the checkpoint that follows it. A push and
    // a read-modify-write are the same shape.
    std::size_t committed_writes = 0U;
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };

    // An instruction samples the interrupt one CPU cycle before its own last,
    // which is where the core's LastCycle sits. A store to a VDC port is the
    // exception: its last cycle is the one the VDC steals after the bus write,
    // so it samples two CPU cycles early and passes sample_back = 6.
    const auto append_checkpoint =
        [&replacement, &registers, &cycle, &commit_pending_writes](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles,
            const std::uint32_t sample_back = 3U) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - sample_back,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };

    const auto store =
        [&replacement, &ram, &overflow](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return;
            }
            ram[address - kWorkRamBase] = value;
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };

    // The VDC's register-select port at $0000 and its two data ports at $0002
    // and $0003. Nothing here models the VDC; the transcript only has to emit
    // the writes in the order and at the offsets the instructions produce them.
    const auto store_port =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };

    // PUSH writes $2100 + S and then decrements S; POP increments S first.
    // Deriving both from the live stack pointer is what keeps a transcript
    // valid at a depth other than the one it was recorded at.
    const auto push =
        [&registers, &store](
            const std::uint16_t instruction_pc,
            const std::uint8_t value) {
            store(
                instruction_pc,
                static_cast<std::uint16_t>(kStackBase + registers.stack_pointer),
                value);
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer - 1U);
        };
    const auto pop = [&registers, &read_ram]() {
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer + 1U);
        return read_ram(
            static_cast<std::uint16_t>(kStackBase + registers.stack_pointer));
    };

    // Zero page lives at $2000; an absolute address is used as written.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };

    // The pad-enable table at $E1F8 -- one bit per port, $01 $02 $04 $08 $10 --
    // read through whichever bank MPR7 holds, which is bank 0 for the whole of
    // the interrupt handler.
    const auto read_rom =
        [&registers, rom, &overflow](
            const std::uint16_t address) -> std::uint8_t {
            const auto bank = registers.mpr[address >> 13U];
            const auto offset = static_cast<std::size_t>(
                (bank < 0x40U ? (bank & 0x1FU) : ((bank & 0x0FU) + 0x20U)))
                    * 0x2000U
                + (address & 0x1FFFU);
            if (bank >= 0x80U || offset >= rom.size()) {
                overflow = true;
                return 0U;
            }
            return std::to_integer<std::uint8_t>(rom[offset]);
        };

    // The multitap's read cursor and the two latch lines, moved exactly the
    // way MultiTapWrite moves them: CLR rising while SEL is high returns the
    // cursor to the first pad, and SEL rising while CLR is and stays low steps
    // it on by one. The walk has to track them because the routine drives both
    // from $1000 and the cursor decides which pad a read answers for.
    //
    // Nothing needs to be assumed about the state the port was left in. The
    // store at E198 clears CLR whatever it held, so the store at E19D always
    // sees CLR low and always resets the cursor -- and from there the loop
    // steps it once per pass, so the cursor is the loop counter.
    std::uint8_t pad_cursor = 0U;
    bool pad_select = false;
    bool pad_clear = false;
    const auto write_joypad =
        [&pad_cursor, &pad_select, &pad_clear](const std::uint8_t value) {
            const bool select = (value & 0x01U) != 0U;
            const bool clear = (value & 0x02U) != 0U;
            if (select && !pad_clear && clear) {
                pad_cursor = 0U;
            } else if (!pad_clear && !clear && !pad_select && select
                && pad_cursor < 0xFFU) {
                ++pad_cursor;
            }
            pad_select = select;
            pad_clear = clear;
        };
    // Ports three, four and five have no controller, and a port past the fifth
    // reads as nothing at all rather than as an idle pad.
    const auto read_joypad = [&entry, &pad_cursor, &pad_select]() {
        if (pad_cursor > 4U) {
            return static_cast<std::uint8_t>(0xB0U);
        }
        const std::uint16_t buttons = pad_cursor == 0U
            ? entry.input.player_one
            : (pad_cursor == 1U ? entry.input.player_two : 0U);
        return joypad_read(buttons, pad_select);
    };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto compare_accumulator = [&registers](const std::uint8_t value) {
        const auto difference =
            static_cast<std::uint8_t>(registers.accumulator - value);
        registers.status = set_nz(registers.status, difference);
        registers.status = registers.accumulator >= value
            ? static_cast<std::uint8_t>(registers.status | kCarry)
            : static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
    };
    const auto compare_index_y = [&registers](const std::uint8_t value) {
        const auto difference = static_cast<std::uint8_t>(registers.y - value);
        registers.status = set_nz(registers.status, difference);
        registers.status = registers.y >= value
            ? static_cast<std::uint8_t>(registers.status | kCarry)
            : static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
    };
    const auto shift_accumulator_left = [&registers]() {
        const auto value = registers.accumulator;
        registers.accumulator = static_cast<std::uint8_t>(value << 1U);
        registers.status = (value & 0x80U) != 0U
            ? static_cast<std::uint8_t>(registers.status | kCarry)
            : static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
        registers.status = set_nz(registers.status, registers.accumulator);
    };
    // INC on memory: the incremented byte sets N and Z and is written back on
    // the instruction's last cycle, exactly as a store is.
    const auto increment_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto value =
                static_cast<std::uint8_t>(read_ram(address) + 1U);
            store(instruction_pc, address, value);
            registers.status = set_nz(registers.status, value);
        };
    // TMA copies the highest MPR slot the operand names into the accumulator
    // and changes no flag; TAM maps the accumulator into every slot it names.
    // Only #$40 -- MPR6, the bank the sequencer runs from -- occurs here.
    const auto transfer_mpr_to_accumulator =
        [&registers](const std::uint8_t mask) {
            for (std::size_t slot = 0U; slot < registers.mpr.size(); ++slot) {
                if ((mask & static_cast<std::uint8_t>(1U << slot)) != 0U) {
                    registers.accumulator = registers.mpr[slot];
                }
            }
        };
    const auto transfer_accumulator_to_mpr =
        [&registers](const std::uint8_t mask) {
            for (std::size_t slot = 0U; slot < registers.mpr.size(); ++slot) {
                if ((mask & static_cast<std::uint8_t>(1U << slot)) != 0U) {
                    registers.mpr[slot] = registers.accumulator;
                }
            }
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 1800U
            || replacement.work_ram_writes.size() > 480U) {
            return std::nullopt;
        }

        // A walk ends either in front of an instruction the core executes or
        // at the head of another entry in this same family, which is how the
        // dispatcher's tail hands each arm over as its own transaction rather
        // than swallowing it. The first test is what keeps an entry point from
        // stopping on itself before it has walked anything.
        if (pc != entry.routine.logical_pc
            && (is_stop_instruction(pc) || is_owned_entry(pc))) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;

            // 00:E1F7 is the one return in this family. The other two `RTI`
            // paths stop a whole instruction earlier, at the `PLP`, and an
            // `RTI` is not a return this technique expresses -- it pops a
            // status byte and an address the machine pushed for itself rather
            // than a frame a `JSR` left.
            //
            // Only 00:E195 can reach it. Every other entry in the family is at
            // a lower address and the walk stops in front of the next entry it
            // meets, so a walk that would run into the pad reader hands over to
            // it instead. The reader balances its own stack -- the `PHA` and
            // `PLA` at 00:E1A5, like the `NOP` after them, are there to let the
            // joypad's latch settle rather than to save anything -- so the
            // frame is where the entry pointer says.
            //
            // This return is taken in interrupt context, and it is the frame
            // the handler's own `JSR` pushed that it pops.
            if (pc == 0xE1F7U
                && registers.stack_pointer == entry.registers.stack_pointer
                && (!replacement.checkpoints.empty()
                    || absorbed_return_fits_atomically(entry, cycle))) {
                absorb_return(
                    replacement,
                    work_ram,
                    entry.registers.stack_pointer,
                    {kBank, pc},
                    cycle,
                    registers);
            }
            return replacement;
        }

        switch (pc) {
        // ---- 00:E058  the dispatcher's head -------------------------------
        // Four register saves, and then the LDA $0000 the walk stops in front
        // of. Nothing here is device work: the pushes land on the stack page
        // wherever the interrupt left S, exactly as 00:E179's do.
        case 0xE058U:  // PHP
            push(
                0xE058U,
                static_cast<std::uint8_t>(registers.status | kBreak));
            append_checkpoint(0xE058U, 0xE059U, 9U);
            pc = 0xE059U;
            break;

        case 0xE059U:  // PHA
            push(0xE059U, registers.accumulator);
            append_checkpoint(0xE059U, 0xE05AU, 9U);
            pc = 0xE05AU;
            break;

        case 0xE05AU:  // PHX
            push(0xE05AU, registers.x);
            append_checkpoint(0xE05AU, 0xE05BU, 9U);
            pc = 0xE05BU;
            break;

        case 0xE05BU:  // PHY
            push(0xE05BU, registers.y);
            append_checkpoint(0xE05BU, 0xE05CU, 9U);
            pc = 0xE05CU;
            break;

        // ---- 00:E05F  the dispatcher's tail -------------------------------
        // The byte the whole handler branches on is a device register, and it
        // arrives here in the accumulator because the core executed the read
        // itself one instruction ago. From this address down the dispatcher is
        // ordinary work: stash the status byte at $01, test the VBlank bit,
        // and pick the arm from the scene's raster mode at $54.
        case 0xE05FU:  // STA $01
            store(0xE05FU, zp(0x01U), registers.accumulator);
            append_checkpoint(0xE05FU, 0xE061U, 12U);
            pc = 0xE061U;
            break;

        case 0xE061U:  // AND #$20 -- the VBlank bit
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x20U));
            append_checkpoint(0xE061U, 0xE063U, 6U);
            pc = 0xE063U;
            break;

        case 0xE063U: {  // BNE $E068
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE068U : 0xE065U;
            append_checkpoint(0xE063U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE065U:  // JMP $E136 -- the raster arm
            append_checkpoint(0xE065U, 0xE136U, 12U);
            pc = 0xE136U;
            break;

        case 0xE068U:  // LDA $54 -- the scene's raster mode
            load_accumulator(read_ram(zp(0x54U)));
            append_checkpoint(0xE068U, 0xE06AU, 12U);
            pc = 0xE06AU;
            break;

        case 0xE06AU: {  // BNE $E06F
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE06FU : 0xE06CU;
            append_checkpoint(0xE06AU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE06CU:  // JMP $E0EB -- mode 0, the ordinary VBlank prologue
            append_checkpoint(0xE06CU, 0xE0EBU, 12U);
            pc = 0xE0EBU;
            break;

        // Modes 0 and 3 are handled here, straight to 00:E0EB and 00:E111.
        // Mode 1 goes to 00:E0D0 and mode 2 to 00:E09E; anything else falls
        // through 00:E07E to the mode-5 test and 00:E085. None of those three
        // addresses is in this walk, so reaching one lands on the default
        // below and declines the whole transaction. Nothing on the census
        // route takes them: of the 3,599 VBlank entries, 2,748 are mode 0 and
        // the remaining 851 are mode 3.
        case 0xE06FU:  // CMP #$01
            compare_accumulator(0x01U);
            append_checkpoint(0xE06FU, 0xE071U, 6U);
            pc = 0xE071U;
            break;

        case 0xE071U: {  // BEQ $E0D0
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE0D0U : 0xE073U;
            append_checkpoint(0xE071U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE073U:  // CMP #$02
            compare_accumulator(0x02U);
            append_checkpoint(0xE073U, 0xE075U, 6U);
            pc = 0xE075U;
            break;

        case 0xE075U: {  // BEQ $E09E
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE09EU : 0xE077U;
            append_checkpoint(0xE075U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE077U:  // CMP #$03
            compare_accumulator(0x03U);
            append_checkpoint(0xE077U, 0xE079U, 6U);
            pc = 0xE079U;
            break;

        case 0xE079U: {  // BNE $E07E
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE07EU : 0xE07BU;
            append_checkpoint(0xE079U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE07BU:  // JMP $E111 -- mode 3 skips the prologue's VDC writes
            append_checkpoint(0xE07BU, 0xE111U, 12U);
            pc = 0xE111U;
            break;

        // ---- 00:E0EB  VBlank prologue -------------------------------------
        // Registers $08 and $07 are the two scroll pairs and $05 is the
        // control register; clearing bit 6 of the $02 shadow turns the raster
        // compare off for the frame.
        case 0xE0EBU:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xE0EBU, 0xE0EDU, 6U);
            pc = 0xE0EDU;
            break;

        case 0xE0EDU:  // STA $0000
            store_port(0xE0EDU, 0x0000U, registers.accumulator);
            append_checkpoint(0xE0EDU, 0xE0F0U, 18U, 6U);
            pc = 0xE0F0U;
            break;

        case 0xE0F0U:  // STZ $0002
            store_port(0xE0F0U, 0x0002U, 0x00U);
            append_checkpoint(0xE0F0U, 0xE0F3U, 18U, 6U);
            pc = 0xE0F3U;
            break;

        case 0xE0F3U:  // STZ $0003
            store_port(0xE0F3U, 0x0003U, 0x00U);
            append_checkpoint(0xE0F3U, 0xE0F6U, 18U, 6U);
            pc = 0xE0F6U;
            break;

        case 0xE0F6U:  // LDA #$07
            load_accumulator(0x07U);
            append_checkpoint(0xE0F6U, 0xE0F8U, 6U);
            pc = 0xE0F8U;
            break;

        case 0xE0F8U:  // STA $0000
            store_port(0xE0F8U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE0F8U, 0xE0FBU, 18U, 6U);
            pc = 0xE0FBU;
            break;

        case 0xE0FBU:  // STZ $0002
            store_port(0xE0FBU, 0x0002U, 0x00U);
            append_checkpoint(0xE0FBU, 0xE0FEU, 18U, 6U);
            pc = 0xE0FEU;
            break;

        case 0xE0FEU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xE0FEU, 0xE100U, 6U);
            pc = 0xE100U;
            break;

        case 0xE100U:  // STA $0003
            store_port(0xE100U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE100U, 0xE103U, 18U, 6U);
            pc = 0xE103U;
            break;

        case 0xE103U:  // LDA #$05
            load_accumulator(0x05U);
            append_checkpoint(0xE103U, 0xE105U, 6U);
            pc = 0xE105U;
            break;

        case 0xE105U:  // STA $0000
            store_port(0xE105U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE105U, 0xE108U, 18U, 6U);
            pc = 0xE108U;
            break;

        case 0xE108U:  // LDA $02
            load_accumulator(read_ram(zp(0x02U)));
            append_checkpoint(0xE108U, 0xE10AU, 12U);
            pc = 0xE10AU;
            break;

        case 0xE10AU:  // AND #$BF
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xBFU));
            append_checkpoint(0xE10AU, 0xE10CU, 6U);
            pc = 0xE10CU;
            break;

        case 0xE10CU:  // STA $02
            store(0xE10CU, zp(0x02U), registers.accumulator);
            append_checkpoint(0xE10CU, 0xE10EU, 12U);
            pc = 0xE10EU;
            break;

        case 0xE10EU:  // STA $0002
            store_port(0xE10EU, 0x0002U, registers.accumulator);
            append_checkpoint(0xE10EU, 0xE111U, 18U, 6U);
            pc = 0xE111U;
            break;

        // ---- 00:E117  after the pad read ----------------------------------
        // $00 is the frame counter the spin loop at 00:E2B7 waits on. $D9
        // suppresses the sequencer tick while a track is being loaded.
        case 0xE117U:  // INC $00
            increment_memory(0xE117U, zp(0x00U));
            append_checkpoint(0xE117U, 0xE119U, 18U);
            pc = 0xE119U;
            break;

        case 0xE119U:  // LDA $D9
            load_accumulator(read_ram(zp(0xD9U)));
            append_checkpoint(0xE119U, 0xE11BU, 12U);
            pc = 0xE11BU;
            break;

        case 0xE11BU: {  // BNE $E128
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE128U : 0xE11DU;
            append_checkpoint(0xE11BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE11DU:  // TMA #$40 -- save MPR6 across the wrapper
            transfer_mpr_to_accumulator(0x40U);
            append_checkpoint(0xE11DU, 0xE11FU, 12U);
            pc = 0xE11FU;
            break;

        case 0xE11FU:  // PHA
            push(0xE11FU, registers.accumulator);
            append_checkpoint(0xE11FU, 0xE120U, 9U);
            pc = 0xE120U;
            break;

        case 0xE128U:  // LDA #$FF
            load_accumulator(0xFFU);
            append_checkpoint(0xE128U, 0xE12AU, 6U);
            pc = 0xE12AU;
            break;

        case 0xE12AU:  // STA $DA
            store(0xE12AU, zp(0xDAU), registers.accumulator);
            append_checkpoint(0xE12AU, 0xE12CU, 12U);
            pc = 0xE12CU;
            break;

        // ---- 00:E123  after the bank wrapper ------------------------------
        case 0xE123U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE123U, 0xE124U, 12U);
            pc = 0xE124U;
            break;

        case 0xE124U:  // TAM #$40 -- MPR6
            transfer_accumulator_to_mpr(0x40U);
            append_checkpoint(0xE124U, 0xE126U, 15U);
            pc = 0xE126U;
            break;

        case 0xE126U:  // BRA $E12C
            append_checkpoint(0xE126U, 0xE12CU, 12U);
            pc = 0xE12CU;
            break;

        // ---- 00:E12C  the epilogue every arm shares -----------------------
        // $06 is the shadow of the VDC's register-select latch, restored so
        // the interrupted code finds the port as it left it.
        case 0xE12CU:  // LDA $06
            load_accumulator(read_ram(zp(0x06U)));
            append_checkpoint(0xE12CU, 0xE12EU, 12U);
            pc = 0xE12EU;
            break;

        case 0xE12EU:  // STA $0000
            store_port(0xE12EU, 0x0000U, registers.accumulator);
            append_checkpoint(0xE12EU, 0xE131U, 18U, 6U);
            pc = 0xE131U;
            break;

        case 0xE131U:  // PLY
            registers.y = pop();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE131U, 0xE132U, 12U);
            pc = 0xE132U;
            break;

        case 0xE132U:  // PLX
            registers.x = pop();
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE132U, 0xE133U, 12U);
            pc = 0xE133U;
            break;

        case 0xE133U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE133U, 0xE134U, 12U);
            pc = 0xE134U;
            break;

        // ---- 00:E136  the non-VBlank arm ----------------------------------
        // $54 is the scene's raster mode. Modes 0 and 2 reprogram the split
        // registers; every other mode only ticks the counter at $222A.
        case 0xE136U:  // LDA $54
            load_accumulator(read_ram(zp(0x54U)));
            append_checkpoint(0xE136U, 0xE138U, 12U);
            pc = 0xE138U;
            break;

        case 0xE138U: {  // BEQ $E140
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE140U : 0xE13AU;
            append_checkpoint(0xE138U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE13AU:  // CMP #$02
            compare_accumulator(0x02U);
            append_checkpoint(0xE13AU, 0xE13CU, 6U);
            pc = 0xE13CU;
            break;

        case 0xE13CU: {  // BEQ $E140
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE140U : 0xE13EU;
            append_checkpoint(0xE13CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE13EU: {  // BNE $E16E
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE16EU : 0xE140U;
            append_checkpoint(0xE13EU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        // $01 holds the status byte 00:E058 read from the VDC; bit 2 is the
        // raster-compare flag, and without it there is no split to program.
        case 0xE140U:  // LDA $01
            load_accumulator(read_ram(zp(0x01U)));
            append_checkpoint(0xE140U, 0xE142U, 12U);
            pc = 0xE142U;
            break;

        case 0xE142U:  // AND #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x04U));
            append_checkpoint(0xE142U, 0xE144U, 6U);
            pc = 0xE144U;
            break;

        case 0xE144U: {  // BEQ $E12C
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE12CU : 0xE146U;
            append_checkpoint(0xE144U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE146U:  // LDA #$07
            load_accumulator(0x07U);
            append_checkpoint(0xE146U, 0xE148U, 6U);
            pc = 0xE148U;
            break;

        case 0xE148U:  // STA $0000
            store_port(0xE148U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE148U, 0xE14BU, 18U, 6U);
            pc = 0xE14BU;
            break;

        case 0xE14BU:  // STZ $0002
            store_port(0xE14BU, 0x0002U, 0x00U);
            append_checkpoint(0xE14BU, 0xE14EU, 18U, 6U);
            pc = 0xE14EU;
            break;

        case 0xE14EU:  // STZ $0003
            store_port(0xE14EU, 0x0003U, 0x00U);
            append_checkpoint(0xE14EU, 0xE151U, 18U, 6U);
            pc = 0xE151U;
            break;

        case 0xE151U:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xE151U, 0xE153U, 6U);
            pc = 0xE153U;
            break;

        case 0xE153U:  // STA $0000
            store_port(0xE153U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE153U, 0xE156U, 18U, 6U);
            pc = 0xE156U;
            break;

        case 0xE156U:  // LDA $0B
            load_accumulator(read_ram(zp(0x0BU)));
            append_checkpoint(0xE156U, 0xE158U, 12U);
            pc = 0xE158U;
            break;

        case 0xE158U:  // STA $0002
            store_port(0xE158U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE158U, 0xE15BU, 18U, 6U);
            pc = 0xE15BU;
            break;

        case 0xE15BU:  // LDA $0C
            load_accumulator(read_ram(zp(0x0CU)));
            append_checkpoint(0xE15BU, 0xE15DU, 12U);
            pc = 0xE15DU;
            break;

        case 0xE15DU:  // STA $0003
            store_port(0xE15DU, 0x0003U, registers.accumulator);
            append_checkpoint(0xE15DU, 0xE160U, 18U, 6U);
            pc = 0xE160U;
            break;

        case 0xE160U:  // LDA #$05
            load_accumulator(0x05U);
            append_checkpoint(0xE160U, 0xE162U, 6U);
            pc = 0xE162U;
            break;

        case 0xE162U:  // STA $0000
            store_port(0xE162U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE162U, 0xE165U, 18U, 6U);
            pc = 0xE165U;
            break;

        case 0xE165U:  // LDA $02
            load_accumulator(read_ram(zp(0x02U)));
            append_checkpoint(0xE165U, 0xE167U, 12U);
            pc = 0xE167U;
            break;

        case 0xE167U:  // ORA #$40 -- re-arm the raster compare
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x40U));
            append_checkpoint(0xE167U, 0xE169U, 6U);
            pc = 0xE169U;
            break;

        case 0xE169U:  // STA $02
            store(0xE169U, zp(0x02U), registers.accumulator);
            append_checkpoint(0xE169U, 0xE16BU, 12U);
            pc = 0xE16BU;
            break;

        case 0xE16BU:  // STA $0002
            store_port(0xE16BU, 0x0002U, registers.accumulator);
            append_checkpoint(0xE16BU, 0xE16EU, 18U, 6U);
            pc = 0xE16EU;
            break;

        case 0xE16EU:  // INC $222A -- low byte of the 16-bit interrupt tick
            increment_memory(0xE16EU, 0x222AU);
            append_checkpoint(0xE16EU, 0xE171U, 21U);
            pc = 0xE171U;
            break;

        case 0xE171U: {  // BNE $E176
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE176U : 0xE173U;
            append_checkpoint(0xE171U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE173U:  // INC $222B
            increment_memory(0xE173U, 0x222BU);
            append_checkpoint(0xE173U, 0xE176U, 21U);
            pc = 0xE176U;
            break;

        case 0xE176U:  // JMP $E12C
            append_checkpoint(0xE176U, 0xE12CU, 12U);
            pc = 0xE12CU;
            break;

        // ---- 00:E179  the secondary handler -------------------------------
        // Reached only while $D9 suppresses the VBlank arm's own call, which
        // is why its 387 entries match 00:E128's exactly.
        case 0xE179U:  // PHP
            push(
                0xE179U,
                static_cast<std::uint8_t>(registers.status | kBreak));
            append_checkpoint(0xE179U, 0xE17AU, 9U);
            pc = 0xE17AU;
            break;

        case 0xE17AU:  // PHA
            push(0xE17AU, registers.accumulator);
            append_checkpoint(0xE17AU, 0xE17BU, 9U);
            pc = 0xE17BU;
            break;

        case 0xE17BU:  // PHX
            push(0xE17BU, registers.x);
            append_checkpoint(0xE17BU, 0xE17CU, 9U);
            pc = 0xE17CU;
            break;

        case 0xE17CU:  // PHY
            push(0xE17CU, registers.y);
            append_checkpoint(0xE17CU, 0xE17DU, 9U);
            pc = 0xE17DU;
            break;

        case 0xE17DU:  // TMA #$40
            transfer_mpr_to_accumulator(0x40U);
            append_checkpoint(0xE17DU, 0xE17FU, 12U);
            pc = 0xE17FU;
            break;

        case 0xE17FU:  // PHA
            push(0xE17FU, registers.accumulator);
            append_checkpoint(0xE17FU, 0xE180U, 9U);
            pc = 0xE180U;
            break;

        // ---- 00:E183  the secondary handler's epilogue --------------------
        case 0xE183U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE183U, 0xE184U, 12U);
            pc = 0xE184U;
            break;

        case 0xE184U:  // TAM #$40 -- MPR6
            transfer_accumulator_to_mpr(0x40U);
            append_checkpoint(0xE184U, 0xE186U, 15U);
            pc = 0xE186U;
            break;

        case 0xE186U:  // LDA $06
            load_accumulator(read_ram(zp(0x06U)));
            append_checkpoint(0xE186U, 0xE188U, 12U);
            pc = 0xE188U;
            break;

        // The select latch is written twice. The second store is not dead: the
        // VDC latches on the write, and the pair is how the handler leaves the
        // port settled before the interrupted code touches it again.
        case 0xE188U:  // STA $0000
            store_port(0xE188U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE188U, 0xE18BU, 18U, 6U);
            pc = 0xE18BU;
            break;

        case 0xE18BU:  // LDA $06
            load_accumulator(read_ram(zp(0x06U)));
            append_checkpoint(0xE18BU, 0xE18DU, 12U);
            pc = 0xE18DU;
            break;

        case 0xE18DU:  // STA $0000
            store_port(0xE18DU, 0x0000U, registers.accumulator);
            append_checkpoint(0xE18DU, 0xE190U, 18U, 6U);
            pc = 0xE190U;
            break;

        case 0xE190U:  // PLY
            registers.y = pop();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE190U, 0xE191U, 12U);
            pc = 0xE191U;
            break;

        case 0xE191U:  // PLX
            registers.x = pop();
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE191U, 0xE192U, 12U);
            pc = 0xE192U;
            break;

        case 0xE192U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE192U, 0xE193U, 12U);
            pc = 0xE193U;
            break;

        // ---- 00:E195  the pad reader --------------------------------------
        // Five ports are polled in one loop. Each pass reads the selected pad
        // twice -- directions with SEL high, buttons with SEL low -- folds the
        // two nibbles into one byte, inverts it so a pressed line reads as a
        // one, and writes the frame's state to $2013,Y, the previous frame's
        // to $200E,Y and the newly-pressed edges to $2018,Y. The layout the
        // fold produces is the Button enum exactly: I, II, SELECT and RUN in
        // the low nibble, UP, RIGHT, DOWN and LEFT in the high one.
        case 0xE195U:  // CLY
            registers.y = 0U;
            registers.status = set_nz(registers.status, 0U);
            append_checkpoint(0xE195U, 0xE196U, 6U);
            pc = 0xE196U;
            break;

        case 0xE196U:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xE196U, 0xE198U, 6U);
            pc = 0xE198U;
            break;

        case 0xE198U:  // STA $1000 -- SEL high, CLR low
            write_joypad(registers.accumulator);
            store_port(0xE198U, kJoypadPort, registers.accumulator);
            append_checkpoint(0xE198U, 0xE19BU, 15U);
            pc = 0xE19BU;
            break;

        case 0xE19BU:  // LDA #$03
            load_accumulator(0x03U);
            append_checkpoint(0xE19BU, 0xE19DU, 6U);
            pc = 0xE19DU;
            break;

        case 0xE19DU:  // STA $1000 -- CLR rises, cursor back to the first pad
            write_joypad(registers.accumulator);
            store_port(0xE19DU, kJoypadPort, registers.accumulator);
            append_checkpoint(0xE19DU, 0xE1A0U, 15U);
            pc = 0xE1A0U;
            break;

        case 0xE1A0U:  // LDA #$01 -- loop head
            load_accumulator(0x01U);
            append_checkpoint(0xE1A0U, 0xE1A2U, 6U);
            pc = 0xE1A2U;
            break;

        case 0xE1A2U:  // STA $1000 -- SEL rises, stepping the cursor on
            write_joypad(registers.accumulator);
            store_port(0xE1A2U, kJoypadPort, registers.accumulator);
            append_checkpoint(0xE1A2U, 0xE1A5U, 15U);
            pc = 0xE1A5U;
            break;

        // A push and a pull with a NOP behind them. Nothing is being saved:
        // the three instructions are twenty-seven master cycles of settling
        // time for the line the store above just moved, and they have to be
        // transcribed rather than skipped because the push writes the stack.
        case 0xE1A5U:  // PHA
            push(0xE1A5U, registers.accumulator);
            append_checkpoint(0xE1A5U, 0xE1A6U, 9U);
            pc = 0xE1A6U;
            break;

        case 0xE1A6U:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE1A6U, 0xE1A7U, 12U);
            pc = 0xE1A7U;
            break;

        case 0xE1A7U:  // NOP
            append_checkpoint(0xE1A7U, 0xE1A8U, 6U);
            pc = 0xE1A8U;
            break;

        case 0xE1A8U:  // LDA $2013,Y
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x2013U + registers.y)));
            append_checkpoint(0xE1A8U, 0xE1ABU, 15U);
            pc = 0xE1ABU;
            break;

        case 0xE1ABU:  // STA $200E,Y -- last frame's state
            store(
                0xE1ABU,
                static_cast<std::uint16_t>(0x200EU + registers.y),
                registers.accumulator);
            append_checkpoint(0xE1ABU, 0xE1AEU, 15U);
            pc = 0xE1AEU;
            break;

        case 0xE1AEU:  // LDA $1000 -- the four directions
            load_accumulator(read_joypad());
            append_checkpoint(0xE1AEU, 0xE1B1U, 15U);
            pc = 0xE1B1U;
            break;

        case 0xE1B1U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xE1B1U, 0xE1B2U, 6U);
            pc = 0xE1B2U;
            break;

        case 0xE1B2U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xE1B2U, 0xE1B3U, 6U);
            pc = 0xE1B3U;
            break;

        case 0xE1B3U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xE1B3U, 0xE1B4U, 6U);
            pc = 0xE1B4U;
            break;

        case 0xE1B4U:  // ASL A -- the nibble is now the high one
            shift_accumulator_left();
            append_checkpoint(0xE1B4U, 0xE1B5U, 6U);
            pc = 0xE1B5U;
            break;

        case 0xE1B5U:  // STA $2013,Y
            store(
                0xE1B5U,
                static_cast<std::uint16_t>(0x2013U + registers.y),
                registers.accumulator);
            append_checkpoint(0xE1B5U, 0xE1B8U, 15U);
            pc = 0xE1B8U;
            break;

        case 0xE1B8U:  // STZ $1000 -- SEL falls, selecting the buttons
            write_joypad(0x00U);
            store_port(0xE1B8U, kJoypadPort, 0x00U);
            append_checkpoint(0xE1B8U, 0xE1BBU, 15U);
            pc = 0xE1BBU;
            break;

        case 0xE1BBU:  // PHA
            push(0xE1BBU, registers.accumulator);
            append_checkpoint(0xE1BBU, 0xE1BCU, 9U);
            pc = 0xE1BCU;
            break;

        case 0xE1BCU:  // PLA
            load_accumulator(pop());
            append_checkpoint(0xE1BCU, 0xE1BDU, 12U);
            pc = 0xE1BDU;
            break;

        case 0xE1BDU:  // NOP
            append_checkpoint(0xE1BDU, 0xE1BEU, 6U);
            pc = 0xE1BEU;
            break;

        case 0xE1BEU:  // LDA $1000 -- I, II, SELECT and RUN
            load_accumulator(read_joypad());
            append_checkpoint(0xE1BEU, 0xE1C1U, 15U);
            pc = 0xE1C1U;
            break;

        case 0xE1C1U:  // AND #$0F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x0FU));
            append_checkpoint(0xE1C1U, 0xE1C3U, 6U);
            pc = 0xE1C3U;
            break;

        case 0xE1C3U:  // ORA $2013,Y -- fold the directions back in
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator
                | read_ram(
                    static_cast<std::uint16_t>(0x2013U + registers.y))));
            append_checkpoint(0xE1C3U, 0xE1C6U, 15U);
            pc = 0xE1C6U;
            break;

        case 0xE1C6U:  // EOR #$FF -- the port is active low
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator ^ 0xFFU));
            append_checkpoint(0xE1C6U, 0xE1C8U, 6U);
            pc = 0xE1C8U;
            break;

        case 0xE1C8U:  // STA $2013,Y -- what is held now
            store(
                0xE1C8U,
                static_cast<std::uint16_t>(0x2013U + registers.y),
                registers.accumulator);
            append_checkpoint(0xE1C8U, 0xE1CBU, 15U);
            pc = 0xE1CBU;
            break;

        case 0xE1CBU:  // EOR $200E,Y -- what changed
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator
                ^ read_ram(
                    static_cast<std::uint16_t>(0x200EU + registers.y))));
            append_checkpoint(0xE1CBU, 0xE1CEU, 15U);
            pc = 0xE1CEU;
            break;

        case 0xE1CEU:  // AND $2013,Y -- and of that, what is down
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator
                & read_ram(
                    static_cast<std::uint16_t>(0x2013U + registers.y))));
            append_checkpoint(0xE1CEU, 0xE1D1U, 15U);
            pc = 0xE1D1U;
            break;

        case 0xE1D1U:  // STA $2018,Y -- this frame's presses
            store(
                0xE1D1U,
                static_cast<std::uint16_t>(0x2018U + registers.y),
                registers.accumulator);
            append_checkpoint(0xE1D1U, 0xE1D4U, 15U);
            pc = 0xE1D4U;
            break;

        case 0xE1D4U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE1D4U, 0xE1D5U, 6U);
            pc = 0xE1D5U;
            break;

        case 0xE1D5U:  // CPY #$05
            compare_index_y(0x05U);
            append_checkpoint(0xE1D5U, 0xE1D7U, 6U);
            pc = 0xE1D7U;
            break;

        case 0xE1D7U: {  // BCC $E1A0
            const bool taken = (registers.status & kCarry) == 0U;
            const auto target =
                static_cast<std::uint16_t>(taken ? 0xE1A0U : 0xE1D9U);
            append_checkpoint(0xE1D7U, target, taken ? 12U : 6U);
            pc = target;
            break;
        }

        // The second pass is the soft-reset check: for each port the game has
        // enabled in $0D, a frame whose new press is SELECT alone ($04) while
        // SELECT and RUN together are what is held ($0C) leaves through $E032.
        case 0xE1D9U:  // CLY
            registers.y = 0U;
            registers.status = set_nz(registers.status, 0U);
            append_checkpoint(0xE1D9U, 0xE1DAU, 6U);
            pc = 0xE1DAU;
            break;

        case 0xE1DAU:  // LDA $0D
            load_accumulator(read_ram(zp(0x0DU)));
            append_checkpoint(0xE1DAU, 0xE1DCU, 12U);
            pc = 0xE1DCU;
            break;

        case 0xE1DCU:  // AND $E1F8,Y -- is this port enabled
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator
                & read_rom(
                    static_cast<std::uint16_t>(0xE1F8U + registers.y))));
            append_checkpoint(0xE1DCU, 0xE1DFU, 15U);
            pc = 0xE1DFU;
            break;

        case 0xE1DFU: {  // BEQ $E1F2
            const bool taken = (registers.status & kZero) != 0U;
            const auto target =
                static_cast<std::uint16_t>(taken ? 0xE1F2U : 0xE1E1U);
            append_checkpoint(0xE1DFU, target, taken ? 12U : 6U);
            pc = target;
            break;
        }

        case 0xE1E1U:  // LDA $2018,Y
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x2018U + registers.y)));
            append_checkpoint(0xE1E1U, 0xE1E4U, 15U);
            pc = 0xE1E4U;
            break;

        case 0xE1E4U:  // CMP #$04
            compare_accumulator(0x04U);
            append_checkpoint(0xE1E4U, 0xE1E6U, 6U);
            pc = 0xE1E6U;
            break;

        case 0xE1E6U: {  // BNE $E1F2
            const bool taken = (registers.status & kZero) == 0U;
            const auto target =
                static_cast<std::uint16_t>(taken ? 0xE1F2U : 0xE1E8U);
            append_checkpoint(0xE1E6U, target, taken ? 12U : 6U);
            pc = target;
            break;
        }

        case 0xE1E8U:  // LDA $2013,Y
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x2013U + registers.y)));
            append_checkpoint(0xE1E8U, 0xE1EBU, 15U);
            pc = 0xE1EBU;
            break;

        case 0xE1EBU:  // CMP #$0C
            compare_accumulator(0x0CU);
            append_checkpoint(0xE1EBU, 0xE1EDU, 6U);
            pc = 0xE1EDU;
            break;

        case 0xE1EDU: {  // BNE $E1F2
            const bool taken = (registers.status & kZero) == 0U;
            const auto target =
                static_cast<std::uint16_t>(taken ? 0xE1F2U : 0xE1EFU);
            append_checkpoint(0xE1EDU, target, taken ? 12U : 6U);
            pc = target;
            break;
        }

        // $E1EF is a JMP out of the routine, abandoning the frame the handler
        // was in the middle of. A transcript that has to stop in front of an
        // RTS cannot express leaving through somewhere else, so this hands the
        // whole call back rather than guessing. It is never reached on the
        // recorded route: SELECT and RUN together with nothing else held is
        // the soft reset.
        case 0xE1EFU:  // JMP $E032
            return std::nullopt;

        case 0xE1F2U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE1F2U, 0xE1F3U, 6U);
            pc = 0xE1F3U;
            break;

        case 0xE1F3U:  // CPY #$05
            compare_index_y(0x05U);
            append_checkpoint(0xE1F3U, 0xE1F5U, 6U);
            pc = 0xE1F5U;
            break;

        case 0xE1F5U: {  // BCC $E1DA
            const bool taken = (registers.status & kCarry) == 0U;
            const auto target =
                static_cast<std::uint16_t>(taken ? 0xE1DAU : 0xE1F7U);
            append_checkpoint(0xE1F5U, target, taken ? 12U : 6U);
            pc = target;
            break;
        }

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
