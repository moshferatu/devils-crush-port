// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// The graphics driver's port-streaming loops and adjacent work-RAM
// stream decoder. They consume MPR-mapped byte streams without reading a
// device register, so unlike the interrupt dispatcher above they are fully
// expressible.
//
//   00:E34C  write one VDC address word from $31/$32, then advance the
//            $2B:$2C transfer counter, including the closing BNE.
//   00:E365 / 00:E37E  copy an MPR-mapped word stream to the VDC, in bounded
//            32-word segments at the existing loop head. Preserve Y and the
//            source high-byte carry across segments; the core retains RTS.
//   00:E3C0  clear 512 VRAM words from $7800 through the VDC data ports
//   00:E3FC  clear 1,024 VCE colour entries through $0404/$0405
//   00:EF2A  copy one 32-word VDC row, advance both pointers by $40 and
//            decrement the live row count; re-enter at the real back-edge.
//   00:E41F  the generic word-stream decoder: $2D/$2E is the destination VRAM
//            address and $2F/$30 an MPR-mapped source pointer. Command $00
//            terminates, $01-$7F copies that many literal little-endian words,
//            and $81-$FF repeats the following word (command & $7F) times --
//            $80 repeating it 256 times.
//   00:E4E3  the byte-stream decoder: the same pointer pair names a work-RAM
//            destination and MPR-mapped source. Positive commands copy that
//            many literal bytes, negative commands repeat the following byte,
//            and zero terminates.
//   00:FA40  the one-shot title/setup tile uploader: copy one 64-byte source
//            row plus its packed attribute nibbles to the VDC, then advance
//            the source, attribute, and VRAM pointers. The outer loop has 26
//            rows; FA6A/FA6E are the two sides of its hardware-executed BBR0.
//
// The decoder is 6.9M of the region's 7.0M master cycles across 41 calls, which
// is 167,000 cycles a call: 3,400 port writes and 12,400 instructions against a
// channel of 512 writes and 2,048 checkpoints. No transcript can hold a call, so
// this module is the first one that chunks. Six entry points, but only two
// kinds: a routine's head, and the head of the loop its transcripts stop inside.
//
//   00:E3C0 / 00:E3D7   the VRAM clear and its loop
//   00:E3FC / 00:E403   the VCE clear and its loop
//   00:E41F / 00:E439   the decoder and its stream-record loop
//
// A chunk ends in front of the backward branch that closes the loop -- 00:E3E4,
// 00:E41C, or the 00:E479 that starts the next stream record. The core executes
// that branch itself, lands on the loop head, and calls this module again for
// the next chunk. The cost is the branch, plus the CLY at 00:E438 for the
// decoder: eighteen master cycles per chunk, about 350 chunks on the route.
//
// Where the decoder chunks is a budget decision rather than a fixed count.
// 00:E479 reads the next record's command byte before committing to it and
// stops if that record would not fit in what is left of either channel, so a
// transcript never has to abandon work it has already transcribed. A record is
// at most 127 words on this route -- 258 writes and 1,036 checkpoints -- so the
// first record of a chunk always fits and the walk always makes progress. A
// $80 or $FF command would not fit; none occurs, and if one did the hard caps
// below decline the transcript and the assembly runs the record instead.
//
// The decoder's zero page is shared -- $2F/$30 in particular is the game's
// general-purpose source pointer, written from 340 sites across eight banks --
// so the transcripts are only safe if nothing writes it under them. Measured on
// an assembly write trace, bracketing each of the 41 calls between its header's
// $06 store at 00:E422 and its last data-port write: 23,300 writes to
// $2D-$30/$37/$38 inside those windows, every one from the decoder itself. No
// interrupt-context writer touches any of them.
//
// The stores at $0000-$0003 and $0402-$0405 are the VDC and VCE register files.
// The core prices the cycle each device steals, so they are transcribed at
// eighteen master cycles and sample the interrupt two CPU cycles early. Writing
// the VDC's high data port additionally wait-states the CPU while the VDC
// reaches VRAM, which is why 00:E449 measures twenty cycles and 00:E443
// eighteen; that steal lands on the real timestamp inside the declared offset,
// exactly as it does in the assembly, and is not the transcript's to predict.
constexpr std::uint8_t kBank = 0x00U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF;
// both are ordinary work RAM reached through segment 1.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// Room left for one more stream record after a chunk decides to continue. The
// hard limits are the core's 512 writes and 2,048 checkpoints.
constexpr std::size_t kWriteBudget = 460U;
constexpr std::size_t kCheckpointBudget = 1980U;

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
    return logical_pc == 0xE271U || logical_pc == 0xE34CU || logical_pc == 0xE365U
        || logical_pc == 0xE37EU || logical_pc == 0xE3C0U
        || logical_pc == 0xE3D7U
        || logical_pc == 0xE3FCU || logical_pc == 0xE403U
        || logical_pc == 0xE41FU || logical_pc == 0xE439U
        || logical_pc == 0xE4E3U || logical_pc == 0xEF2AU || logical_pc == 0xFA40U
        || logical_pc == 0xFA6AU || logical_pc == 0xFA6EU;
}

// Returns retained by this walk. The address-fill case separately hands back
// E364 after its final word; neither loop absorbs a caller's stack frame.
[[nodiscard]] constexpr bool is_return_instruction(
    const std::uint16_t logical_pc)
{
    return logical_pc == 0xE292U || logical_pc == 0xE39CU || logical_pc == 0xE3E6U || logical_pc == 0xE41EU
        || logical_pc == 0xE47CU || logical_pc == 0xE532U;
}

} // namespace

std::optional<RoutineReplacement> replace_vram_upload(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is something these
    // loops are written to run under; T would redirect the accumulator loads
    // through zero page and D would change the pointer advance at 00:E46F.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }
    // The VDC and VCE ports only reach their devices while the hardware page is
    // mapped at segment 0 -- the same condition the core's write validator
    // imposes.
    if (entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }
    if ((entry.routine.logical_pc == 0xE271U
         || entry.routine.logical_pc == 0xE34CU
         || entry.routine.logical_pc == 0xE365U
         || entry.routine.logical_pc == 0xE37EU
         || entry.routine.logical_pc == 0xE4E3U
         || entry.routine.logical_pc == 0xEF2AU
         || entry.routine.logical_pc >= 0xFA40U)
        && entry.registers.mpr[1] != 0xF8U) {
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

    // Reads the byte a logical address currently maps to. The decoder's source
    // pointer is resolved through the live MPRs rather than assumed: the route
    // reaches it through MPR4 and MPR5 for the most part, but a stream that
    // runs past $BFFF continues through whatever the next slot holds.
    const auto read_mapped =
        [&registers, &ram, rom, &overflow](
            const std::uint16_t address) -> std::uint8_t {
            const auto bank = registers.mpr[address >> 13U];
            if (bank == 0xF8U) {
                return ram[address & 0x1FFFU];
            }
            if (bank >= 0x80U || rom.size() != 0x60000U) {
                overflow = true;
                return 0U;
            }
            const auto rom_bank = bank < 0x40U
                ? static_cast<std::size_t>(bank & 0x1FU)
                : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
            const auto offset = rom_bank * 0x2000U
                + static_cast<std::size_t>(address & 0x1FFFU);
            if (offset >= rom.size()) {
                overflow = true;
                return 0U;
            }
            return std::to_integer<std::uint8_t>(rom[offset]);
        };

    // Stores land on their instruction's final cycle, so a write shares the
    // offset and the instruction of the checkpoint that follows it. A push is
    // the same shape.
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
    // which is where the core's LastCycle sits. A store to a VDC or VCE port is
    // the exception: its last cycle is the one the device steals after the bus
    // write, so it samples two CPU cycles early and passes sample_back = 6.
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

    // The VDC's register-select port at $0000 and its data ports at $0002 and
    // $0003, and the VCE's colour-table ports at $0402-$0405. Nothing here
    // models either device; the transcript only has to emit the writes in the
    // order and at the offsets the instructions produce them.
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
    // The decoder's source pointer, indexed by Y. Both halves come from the
    // snapshot, so a chunk that resumes mid-stream picks up whatever the
    // previous chunk stored.
    const auto source_byte = [&read_ram, &read_mapped, &registers, &zp]() {
        const auto pointer = static_cast<std::uint16_t>(
            read_ram(zp(0x2FU))
            | static_cast<std::uint16_t>(read_ram(zp(0x30U)) << 8U));
        return read_mapped(
            static_cast<std::uint16_t>(pointer + registers.y));
    };
    const auto indexed_pointer_byte =
        [&read_ram, &read_mapped, &registers, &zp](
            const std::uint8_t pointer_zp) {
            const auto pointer = static_cast<std::uint16_t>(
                read_ram(zp(pointer_zp))
                | static_cast<std::uint16_t>(
                    read_ram(zp(static_cast<std::uint8_t>(pointer_zp + 1U)))
                    << 8U));
            return read_mapped(
                static_cast<std::uint16_t>(pointer + registers.y));
        };
    const auto destination_address = [&read_ram, &registers, &zp]() {
        const auto pointer = static_cast<std::uint16_t>(
            read_ram(zp(0x2DU))
            | static_cast<std::uint16_t>(read_ram(zp(0x2EU)) << 8U));
        return static_cast<std::uint16_t>(pointer + registers.y);
    };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };

    const auto shift_right_accumulator = [&registers, &load_accumulator]() {
        const auto carry = static_cast<std::uint8_t>(
            registers.accumulator & 0x01U);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        registers.status = static_cast<std::uint8_t>(
            registers.status | carry);
        load_accumulator(
            static_cast<std::uint8_t>(registers.accumulator >> 1U));
    };
    const auto shift_left_accumulator = [&registers, &load_accumulator]() {
        const bool carry = (registers.accumulator & 0x80U) != 0U;
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if (carry) {
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
        }
        load_accumulator(
            static_cast<std::uint8_t>(registers.accumulator << 1U));
    };
    const auto compare_y = [&registers](const std::uint8_t operand) {
        const auto difference = static_cast<std::uint8_t>(registers.y - operand);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if (registers.y >= operand) {
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
        }
        registers.status = set_nz(registers.status, difference);
    };

    // ADC in binary mode, with the overflow bit the core computes. Nothing in
    // this region tests V, but a checkpoint that publishes the wrong one is
    // abandoned at the core's register comparison, so it has to be exact.
    const auto add_with_carry = [&registers](const std::uint8_t operand) {
        const auto sum = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            + static_cast<std::uint16_t>(operand)
            + static_cast<std::uint16_t>(registers.status & kCarry));
        const auto result = static_cast<std::uint8_t>(sum);
        const bool signed_overflow =
            ((registers.accumulator ^ result)
             & (operand ^ result) & kNegative) != 0U;
        registers.status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFFU) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
        }
        if (signed_overflow) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | kOverflow);
        }
        registers.accumulator = result;
        registers.status = set_nz(registers.status, result);
    };

    auto pc = entry.routine.logical_pc;
    if (pc == 0xEF2AU) {
        // One complete row is 74 writes / 283 checkpoints. Re-read mapped
        // source bytes after staging the register-select shadow writes: RAM
        // aliases of $06 and the pointers must observe the same live bytes.
        // No look-ahead into the next row, no simulated device reads or RTS.
        do {
            const auto instruction = pc;
            std::uint32_t cost = 6U;
            std::uint32_t sample_back = 3U;
            switch (pc) {
            case 0xEF2AU: load_accumulator(0U); pc = 0xEF2CU; break;
            case 0xEF3BU: load_accumulator(2U); pc = 0xEF3DU; break;
            case 0xEF2CU: case 0xEF3DU:
                store(pc, zp(0x06U), registers.accumulator);
                pc = static_cast<std::uint16_t>(pc + 2U); cost = 12U; break;
            case 0xEF31U: case 0xEF36U: case 0xEF54U: case 0xEF60U: {
                const auto operand = pc == 0xEF31U || pc == 0xEF60U ? 0x2DU
                    : pc == 0xEF36U ? 0x2EU : 0x2FU;
                load_accumulator(read_ram(zp(static_cast<std::uint8_t>(operand))));
                pc = static_cast<std::uint16_t>(pc + 2U); cost = 12U; break;
            }
            case 0xEF2EU: case 0xEF33U: case 0xEF38U:
            case 0xEF3FU: case 0xEF45U: case 0xEF4BU: {
                const auto port = pc == 0xEF2EU || pc == 0xEF3FU ? 0U
                    : pc == 0xEF33U || pc == 0xEF45U ? 2U : 3U;
                store_port(pc, static_cast<std::uint16_t>(port), registers.accumulator);
                pc = static_cast<std::uint16_t>(pc + 3U);
                cost = 18U; sample_back = 6U; break;
            }
            case 0xEF42U: registers.y = 0U; pc = 0xEF43U; break; // CLY: no flags
            case 0xEF43U: case 0xEF49U:
                load_accumulator(source_byte());
                pc = static_cast<std::uint16_t>(pc + 2U); cost = 21U; break;
            case 0xEF48U: case 0xEF4EU:
                load_y(static_cast<std::uint8_t>(registers.y + 1U));
                pc = static_cast<std::uint16_t>(pc + 1U); break;
            case 0xEF4FU: compare_y(0x40U); pc = 0xEF51U; break;
            case 0xEF51U:
                pc = registers.y != 0x40U ? 0xEF43U : 0xEF53U;
                cost = registers.y != 0x40U ? 12U : 6U; break;
            case 0xEF53U: case 0xEF5FU:
                registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
                pc = static_cast<std::uint16_t>(pc + 1U); break;
            case 0xEF56U: case 0xEF62U:
                add_with_carry(0x40U); pc = static_cast<std::uint16_t>(pc + 2U); break;
            case 0xEF58U: case 0xEF5DU: case 0xEF64U: case 0xEF69U: {
                const auto operand = pc == 0xEF58U ? 0x2FU : pc == 0xEF5DU ? 0x30U
                    : pc == 0xEF64U ? 0x2DU : 0x2EU;
                store(pc, zp(static_cast<std::uint8_t>(operand)), registers.accumulator);
                pc = static_cast<std::uint16_t>(pc + 2U); cost = 12U; break;
            }
            case 0xEF5AU: case 0xEF66U: // CLA: no flags, preserve low-byte carry
                registers.accumulator = 0U; pc = static_cast<std::uint16_t>(pc + 1U); break;
            case 0xEF5BU: case 0xEF67U:
                add_with_carry(read_ram(zp(pc == 0xEF5BU ? 0x30U : 0x2EU)));
                pc = static_cast<std::uint16_t>(pc + 2U); cost = 12U; break;
            case 0xEF6BU:
                registers.x = static_cast<std::uint8_t>(registers.x - 1U);
                registers.status = set_nz(registers.status, registers.x);
                pc = 0xEF6CU; break;
            case 0xEF6CU:
                pc = registers.x != 0U ? 0xEF2AU : 0xEF6EU;
                cost = registers.x != 0U ? 12U : 6U; break;
            default: return std::nullopt;
            }
            if (overflow) { return std::nullopt; }
            append_checkpoint(instruction, pc, cost, sample_back);
        } while (pc != 0xEF2AU && pc != 0xEF6EU);
        replacement.return_instruction = {kBank, pc};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
        return replacement;
    }
    std::size_t transferred_words = 0U;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 2000U
            || replacement.work_ram_writes.size() > 500U) {
            return std::nullopt;
        }

        const auto finish = [&](const std::uint16_t stop_pc) {
            registers.program_counter = stop_pc;
            replacement.return_instruction = {kBank, stop_pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
        };

        if (is_return_instruction(pc)) {
            finish(pc);
            return replacement;
        }
        // BBR0 is internally two independently sampled core operations. Hand
        // it to a fresh entry, whose native bit-branch helper requires the
        // entire opcode to finish before either event horizon; otherwise the
        // core preserves both samples and re-enters at the selected arm. The
        // outer BNE is the natural row boundary and likewise re-enters FA40.
        if (pc == 0xFA67U || pc == 0xFAA5U) {
            finish(pc);
            return replacement;
        }
        // A clear loop chunks whenever either channel is full. The core takes
        // the branch and re-enters at the loop head.
        if ((pc == 0xE3E4U || pc == 0xE41CU)
            && (replacement.work_ram_writes.size() >= kWriteBudget
                || replacement.checkpoints.size() >= kCheckpointBudget)) {
            finish(pc);
            return replacement;
        }
        // The decoder chunks between stream records, and only when the record
        // it would start next does not fit. Reading its command byte here is
        // the one lookahead in the walk.
        if (pc == 0xE479U && !replacement.work_ram_writes.empty()) {
            const auto pointer = static_cast<std::uint16_t>(
                read_ram(zp(0x2FU))
                | static_cast<std::uint16_t>(read_ram(zp(0x30U)) << 8U));
            const auto command = read_mapped(pointer);
            if (command != 0U) {
                const std::size_t words = (command & 0x80U) != 0U
                    ? ((command & 0x7FU) != 0U
                           ? static_cast<std::size_t>(command & 0x7FU)
                           : 256U)
                    : static_cast<std::size_t>(command);
                if (replacement.work_ram_writes.size() + 2U * words + 4U
                        > kWriteBudget
                    || replacement.checkpoints.size() + 8U * words + 24U
                        > kCheckpointBudget) {
                    finish(pc);
                    return replacement;
                }
            }
        }

        switch (pc) {
        // ---- 00:FA40  title/setup tile row upload ------------------------
        case 0xFA40U:  // LDA #$00 -- select MAWR
            load_accumulator(0x00U);
            append_checkpoint(0xFA40U, 0xFA42U, 6U);
            pc = 0xFA42U;
            break;
        case 0xFA42U:  // STA $06
            store(0xFA42U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xFA42U, 0xFA44U, 12U);
            pc = 0xFA44U;
            break;
        case 0xFA44U:  // STA $0000
            store_port(0xFA44U, 0x0000U, registers.accumulator);
            append_checkpoint(0xFA44U, 0xFA47U, 18U, 6U);
            pc = 0xFA47U;
            break;
        case 0xFA47U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xFA47U, 0xFA49U, 12U);
            pc = 0xFA49U;
            break;
        case 0xFA49U:  // STA $0002
            store_port(0xFA49U, 0x0002U, registers.accumulator);
            append_checkpoint(0xFA49U, 0xFA4CU, 18U, 6U);
            pc = 0xFA4CU;
            break;
        case 0xFA4CU:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xFA4CU, 0xFA4EU, 12U);
            pc = 0xFA4EU;
            break;
        case 0xFA4EU:  // STA $0003
            store_port(0xFA4EU, 0x0003U, registers.accumulator);
            append_checkpoint(0xFA4EU, 0xFA51U, 18U, 6U);
            pc = 0xFA51U;
            break;
        case 0xFA51U:  // LDA #$02 -- select VWR
            load_accumulator(0x02U);
            append_checkpoint(0xFA51U, 0xFA53U, 6U);
            pc = 0xFA53U;
            break;
        case 0xFA53U:  // STA $06
            store(0xFA53U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xFA53U, 0xFA55U, 12U);
            pc = 0xFA55U;
            break;
        case 0xFA55U:  // STA $0000
            store_port(0xFA55U, 0x0000U, registers.accumulator);
            append_checkpoint(0xFA55U, 0xFA58U, 18U, 6U);
            pc = 0xFA58U;
            break;
        case 0xFA58U:  // CLY -- no flags
            registers.y = 0U;
            append_checkpoint(0xFA58U, 0xFA59U, 6U);
            pc = 0xFA59U;
            break;
        case 0xFA59U:  // LDA ($2F),Y -- tile low byte
            load_accumulator(indexed_pointer_byte(0x2FU));
            append_checkpoint(0xFA59U, 0xFA5BU, 21U);
            pc = 0xFA5BU;
            break;
        case 0xFA5BU:  // STA $0002
            store_port(0xFA5BU, 0x0002U, registers.accumulator);
            append_checkpoint(0xFA5BU, 0xFA5EU, 18U, 6U);
            pc = 0xFA5EU;
            break;
        case 0xFA5EU:  // PHY
            push(0xFA5EU, registers.y);
            append_checkpoint(0xFA5EU, 0xFA5FU, 9U);
            pc = 0xFA5FU;
            break;
        case 0xFA5FU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0xFA5FU, 0xFA60U, 6U);
            pc = 0xFA60U;
            break;
        case 0xFA60U:  // LSR A -- word index
            shift_right_accumulator();
            append_checkpoint(0xFA60U, 0xFA61U, 6U);
            pc = 0xFA61U;
            break;
        case 0xFA61U:  // STA $26 -- low bit selects packed nibble
            store(0xFA61U, zp(0x26U), registers.accumulator);
            append_checkpoint(0xFA61U, 0xFA63U, 12U);
            pc = 0xFA63U;
            break;
        case 0xFA63U:  // LSR A -- attribute byte index
            shift_right_accumulator();
            append_checkpoint(0xFA63U, 0xFA64U, 6U);
            pc = 0xFA64U;
            break;
        case 0xFA64U:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0xFA64U, 0xFA65U, 6U);
            pc = 0xFA65U;
            break;
        case 0xFA65U:  // LDA ($35),Y -- packed attribute byte
            load_accumulator(indexed_pointer_byte(0x35U));
            append_checkpoint(0xFA65U, 0xFA67U, 21U);
            pc = 0xFA67U;
            break;

        // The core executes BBR0 $26 at FA67, preserving every event sample.
        case 0xFA6AU:  // ASL A -- odd word uses the high nibble
        case 0xFA6BU:
        case 0xFA6CU:
        case 0xFA6DU: {
            const auto next = static_cast<std::uint16_t>(pc + 1U);
            shift_left_accumulator();
            append_checkpoint(pc, next, 6U);
            pc = next;
            break;
        }
        case 0xFA6EU:  // AND #$F0
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF0U));
            append_checkpoint(0xFA6EU, 0xFA70U, 6U);
            pc = 0xFA70U;
            break;
        case 0xFA70U:  // STA $26
            store(0xFA70U, zp(0x26U), registers.accumulator);
            append_checkpoint(0xFA70U, 0xFA72U, 12U);
            pc = 0xFA72U;
            break;
        case 0xFA72U:  // PLY -- restore byte-stream index
            load_y(pop());
            append_checkpoint(0xFA72U, 0xFA73U, 12U);
            pc = 0xFA73U;
            break;
        case 0xFA73U:  // INY
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xFA73U, 0xFA74U, 6U);
            pc = 0xFA74U;
            break;
        case 0xFA74U:  // LDA ($2F),Y -- tile high byte
            load_accumulator(indexed_pointer_byte(0x2FU));
            append_checkpoint(0xFA74U, 0xFA76U, 21U);
            pc = 0xFA76U;
            break;
        case 0xFA76U:  // ORA $26
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0x26U))));
            append_checkpoint(0xFA76U, 0xFA78U, 12U);
            pc = 0xFA78U;
            break;
        case 0xFA78U:  // STA $0003 -- complete the VDC word
            store_port(0xFA78U, 0x0003U, registers.accumulator);
            append_checkpoint(0xFA78U, 0xFA7BU, 18U, 6U);
            pc = 0xFA7BU;
            break;
        case 0xFA7BU:  // INY
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xFA7BU, 0xFA7CU, 6U);
            pc = 0xFA7CU;
            break;
        case 0xFA7CU:  // CPY #$40
            compare_y(0x40U);
            append_checkpoint(0xFA7CU, 0xFA7EU, 6U);
            pc = 0xFA7EU;
            break;
        case 0xFA7EU: {  // BNE $FA59
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = static_cast<std::uint16_t>(
                taken ? 0xFA59U : 0xFA80U);
            append_checkpoint(0xFA7EU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }
        case 0xFA80U:  // CLC -- advance source by 64 bytes
        case 0xFA8CU:
        case 0xFA98U:
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(pc + 1U);
            break;
        case 0xFA81U:  // LDA $2F
        case 0xFA8DU:  // LDA $2D
        case 0xFA99U: { // LDA $35
            const auto address = pc == 0xFA81U ? 0x2FU
                : (pc == 0xFA8DU ? 0x2DU : 0x35U);
            load_accumulator(read_ram(zp(static_cast<std::uint8_t>(address))));
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;
        }
        case 0xFA83U:  // ADC #$40
        case 0xFA8FU:
            add_with_carry(0x40U);
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 6U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;
        case 0xFA9BU:  // ADC #$10
            add_with_carry(0x10U);
            append_checkpoint(0xFA9BU, 0xFA9DU, 6U);
            pc = 0xFA9DU;
            break;
        case 0xFA85U:  // STA $2F
        case 0xFA91U:  // STA $2D
        case 0xFA9DU: { // STA $35
            const auto address = pc == 0xFA85U ? 0x2FU
                : (pc == 0xFA91U ? 0x2DU : 0x35U);
            store(pc, zp(static_cast<std::uint8_t>(address)), registers.accumulator);
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;
        }
        case 0xFA87U:  // CLA -- carry survives into high-byte add
        case 0xFA93U:
        case 0xFA9FU:
            registers.accumulator = 0U;
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(pc + 1U);
            break;
        case 0xFA88U:  // ADC $30
        case 0xFA94U:  // ADC $2E
        case 0xFAA0U: { // ADC $36
            const auto address = pc == 0xFA88U ? 0x30U
                : (pc == 0xFA94U ? 0x2EU : 0x36U);
            add_with_carry(read_ram(zp(static_cast<std::uint8_t>(address))));
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;
        }
        case 0xFA8AU:  // STA $30
        case 0xFA96U:  // STA $2E
        case 0xFAA2U: { // STA $36
            const auto address = pc == 0xFA8AU ? 0x30U
                : (pc == 0xFA96U ? 0x2EU : 0x36U);
            store(pc, zp(static_cast<std::uint8_t>(address)), registers.accumulator);
            append_checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 12U);
            pc = static_cast<std::uint16_t>(pc + 2U);
            break;
        }
        case 0xFAA4U:  // DEX -- BNE $FA40 is executed by the core
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xFAA4U, 0xFAA5U, 6U);
            pc = 0xFAA5U;
            break;

        // Twelve setup-register triples, with the original port timing and
        // an interrupt checkpoint at every instruction. RTS stays in the core.
        case 0xE271U: {
            load_accumulator(0x93U);
            append_checkpoint(0xE271U, 0xE273U, 6U);
            store(0xE273U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xE273U, 0xE275U, 12U);
            load_accumulator(0xE2U);
            append_checkpoint(0xE275U, 0xE277U, 6U);
            store(0xE277U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xE277U, 0xE279U, 12U);
            registers.y = 0U; // CLY does not change flags.
            append_checkpoint(0xE279U, 0xE27AU, 6U);
            do {
                load_accumulator(source_byte());
                append_checkpoint(0xE27AU, 0xE27CU, 21U);
                load_y(static_cast<std::uint8_t>(registers.y + 1U));
                append_checkpoint(0xE27CU, 0xE27DU, 6U);
                store(0xE27DU, zp(0x06U), registers.accumulator);
                append_checkpoint(0xE27DU, 0xE27FU, 12U);
                store_port(0xE27FU, 0x0000U, registers.accumulator);
                append_checkpoint(0xE27FU, 0xE282U, 18U, 6U);
                load_accumulator(source_byte());
                append_checkpoint(0xE282U, 0xE284U, 21U);
                load_y(static_cast<std::uint8_t>(registers.y + 1U));
                append_checkpoint(0xE284U, 0xE285U, 6U);
                store_port(0xE285U, 0x0002U, registers.accumulator);
                append_checkpoint(0xE285U, 0xE288U, 18U, 6U);
                load_accumulator(source_byte());
                append_checkpoint(0xE288U, 0xE28AU, 21U);
                load_y(static_cast<std::uint8_t>(registers.y + 1U));
                append_checkpoint(0xE28AU, 0xE28BU, 6U);
                store_port(0xE28BU, 0x0003U, registers.accumulator);
                append_checkpoint(0xE28BU, 0xE28EU, 18U, 6U);
                compare_y(0x24U);
                append_checkpoint(0xE28EU, 0xE290U, 6U);
                append_checkpoint(0xE290U,
                    registers.y == 0x24U ? 0xE292U : 0xE27AU,
                    registers.y == 0x24U ? 6U : 12U);
            } while (registers.y != 0x24U);
            pc = 0xE292U;
            break;
        }

        // ---- 00:E34C  submit one VDC address word and advance its count ---
        //
        // Each word remains a transaction, now including its closing branch.
        // The branch checkpoint retains its real interrupt sampling horizon.
        case 0xE34CU: {
            load_accumulator(read_ram(zp(0x31U)));  // LDA $31
            append_checkpoint(0xE34CU, 0xE34EU, 12U);

            store_port(0xE34EU, 0x0002U, registers.accumulator);  // STA $0002
            append_checkpoint(0xE34EU, 0xE351U, 18U, 6U);

            load_accumulator(read_ram(zp(0x32U)));  // LDA $32
            append_checkpoint(0xE351U, 0xE353U, 12U);

            store_port(0xE353U, 0x0003U, registers.accumulator);  // STA $0003
            append_checkpoint(0xE353U, 0xE356U, 18U, 6U);

            load_accumulator(read_ram(zp(0x2BU)));  // LDA $2B
            append_checkpoint(0xE356U, 0xE358U, 12U);

            const bool lower_byte_nonzero = registers.accumulator != 0U;
            append_checkpoint(
                0xE358U,
                lower_byte_nonzero ? 0xE35CU : 0xE35AU,
                lower_byte_nonzero ? 12U : 6U);  // BNE $E35C

            if (!lower_byte_nonzero) {
                const auto decremented_high = static_cast<std::uint8_t>(
                    read_ram(zp(0x2CU)) - 1U);
                store(0xE35AU, zp(0x2CU), decremented_high);  // DEC $2C
                registers.status = set_nz(registers.status, decremented_high);
                append_checkpoint(0xE35AU, 0xE35CU, 18U);
            }

            const auto decremented_low = static_cast<std::uint8_t>(
                read_ram(zp(0x2BU)) - 1U);
            store(0xE35CU, zp(0x2BU), decremented_low);  // DEC $2B
            registers.status = set_nz(registers.status, decremented_low);
            append_checkpoint(0xE35CU, 0xE35EU, 18U);

            load_accumulator(read_ram(zp(0x2BU)));  // LDA $2B
            append_checkpoint(0xE35EU, 0xE360U, 12U);

            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0x2CU))));  // ORA $2C
            append_checkpoint(0xE360U, 0xE362U, 12U);

            const bool repeat = (registers.status & kZero) == 0U;
            const auto next = static_cast<std::uint16_t>(
                repeat ? 0xE34CU : 0xE364U);
            append_checkpoint(0xE362U, next, repeat ? 12U : 6U);
            finish(next);
            return replacement;
        }

        // ---- 00:E365  uncompressed VDC word stream ----------------------
        case 0xE365U:
            load_accumulator(0U);  // LDA #$00
            append_checkpoint(0xE365U, 0xE367U, 6U);
            store(0xE367U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE367U, 0xE369U, 12U);
            store_port(0xE369U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE369U, 0xE36CU, 18U, 6U);
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xE36CU, 0xE36EU, 12U);
            store_port(0xE36EU, 0x0002U, registers.accumulator);
            append_checkpoint(0xE36EU, 0xE371U, 18U, 6U);
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xE371U, 0xE373U, 12U);
            store_port(0xE373U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE373U, 0xE376U, 18U, 6U);
            load_accumulator(2U);
            append_checkpoint(0xE376U, 0xE378U, 6U);
            store(0xE378U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE378U, 0xE37AU, 12U);
            store_port(0xE37AU, 0x0000U, registers.accumulator);
            append_checkpoint(0xE37AU, 0xE37DU, 18U, 6U);
            registers.y = 0U;  // CLY does not update N/Z on HuC6280.
            append_checkpoint(0xE37DU, 0xE37EU, 6U);
            pc = 0xE37EU;
            break;

        case 0xE37EU: {
            load_accumulator(source_byte());
            append_checkpoint(0xE37EU, 0xE380U, 21U);
            store_port(0xE380U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE380U, 0xE383U, 18U, 6U);
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xE383U, 0xE384U, 6U);
            load_accumulator(source_byte());
            append_checkpoint(0xE384U, 0xE386U, 21U);
            store_port(0xE386U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE386U, 0xE389U, 18U, 6U);
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xE389U, 0xE38AU, 6U);
            const bool y_nonzero = (registers.status & kZero) == 0U;
            append_checkpoint(0xE38AU, y_nonzero ? 0xE38EU : 0xE38CU,
                y_nonzero ? 12U : 6U);
            if (!y_nonzero) {
                const auto high = static_cast<std::uint8_t>(read_ram(zp(0x30U)) + 1U);
                store(0xE38CU, zp(0x30U), high);
                registers.status = set_nz(registers.status, high);
                append_checkpoint(0xE38CU, 0xE38EU, 18U);
            }
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xE38EU, 0xE390U, 12U);
            const bool low_nonzero = (registers.status & kZero) == 0U;
            append_checkpoint(0xE390U, low_nonzero ? 0xE394U : 0xE392U,
                low_nonzero ? 12U : 6U);
            if (!low_nonzero) {
                const auto high = static_cast<std::uint8_t>(read_ram(zp(0x2CU)) - 1U);
                store(0xE392U, zp(0x2CU), high);
                registers.status = set_nz(registers.status, high);
                append_checkpoint(0xE392U, 0xE394U, 18U);
            }
            const auto low = static_cast<std::uint8_t>(read_ram(zp(0x2BU)) - 1U);
            store(0xE394U, zp(0x2BU), low);
            registers.status = set_nz(registers.status, low);
            append_checkpoint(0xE394U, 0xE396U, 18U);
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xE396U, 0xE398U, 12U);
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0x2CU))));
            append_checkpoint(0xE398U, 0xE39AU, 12U);
            const bool repeat = (registers.status & kZero) == 0U;
            pc = repeat ? 0xE37EU : 0xE39CU;
            append_checkpoint(0xE39AU, pc, repeat ? 12U : 6U);
            if (overflow) {
                return std::nullopt;
            }
            // A real back-edge, not an extra instruction/interrupt horizon.
            // 32 words leave ample room for page carry and counter borrow in
            // both channels, even when the incoming count is zero (65,536).
            if (++transferred_words == 32U) {
                finish(pc);
                return replacement;
            }
            break;
        }

        // ---- 00:E3C0  clear 512 VRAM words from $7800 --------------------
        case 0xE3C0U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0xE3C0U, 0xE3C2U, 6U);
            pc = 0xE3C2U;
            break;

        case 0xE3C2U:  // STA $06
            store(0xE3C2U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE3C2U, 0xE3C4U, 12U);
            pc = 0xE3C4U;
            break;

        case 0xE3C4U:  // STA $0000 -- select MAWR
            store_port(0xE3C4U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE3C4U, 0xE3C7U, 18U, 6U);
            pc = 0xE3C7U;
            break;

        case 0xE3C7U:  // STZ $0002
            store_port(0xE3C7U, 0x0002U, 0x00U);
            append_checkpoint(0xE3C7U, 0xE3CAU, 18U, 6U);
            pc = 0xE3CAU;
            break;

        case 0xE3CAU:  // LDA #$78
            load_accumulator(0x78U);
            append_checkpoint(0xE3CAU, 0xE3CCU, 6U);
            pc = 0xE3CCU;
            break;

        case 0xE3CCU:  // STA $0003 -- MAWR = $7800
            store_port(0xE3CCU, 0x0003U, registers.accumulator);
            append_checkpoint(0xE3CCU, 0xE3CFU, 18U, 6U);
            pc = 0xE3CFU;
            break;

        case 0xE3CFU:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xE3CFU, 0xE3D1U, 6U);
            pc = 0xE3D1U;
            break;

        case 0xE3D1U:  // STA $06
            store(0xE3D1U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE3D1U, 0xE3D3U, 12U);
            pc = 0xE3D3U;
            break;

        case 0xE3D3U:  // STA $0000 -- select VWR
            store_port(0xE3D3U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE3D3U, 0xE3D6U, 18U, 6U);
            pc = 0xE3D6U;
            break;

        case 0xE3D6U:  // CLY -- 256 iterations of two words
            registers.y = 0U;
            append_checkpoint(0xE3D6U, 0xE3D7U, 6U);
            pc = 0xE3D7U;
            break;

        case 0xE3D7U:  // STZ $0002
            store_port(0xE3D7U, 0x0002U, 0x00U);
            append_checkpoint(0xE3D7U, 0xE3DAU, 18U, 6U);
            pc = 0xE3DAU;
            break;

        case 0xE3DAU:  // STZ $0003
            store_port(0xE3DAU, 0x0003U, 0x00U);
            append_checkpoint(0xE3DAU, 0xE3DDU, 18U, 6U);
            pc = 0xE3DDU;
            break;

        case 0xE3DDU:  // STZ $0002
            store_port(0xE3DDU, 0x0002U, 0x00U);
            append_checkpoint(0xE3DDU, 0xE3E0U, 18U, 6U);
            pc = 0xE3E0U;
            break;

        case 0xE3E0U:  // STZ $0003
            store_port(0xE3E0U, 0x0003U, 0x00U);
            append_checkpoint(0xE3E0U, 0xE3E3U, 18U, 6U);
            pc = 0xE3E3U;
            break;

        case 0xE3E3U:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE3E3U, 0xE3E4U, 6U);
            pc = 0xE3E4U;
            break;

        case 0xE3E4U: {  // BNE $E3D7
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE3D7U : 0xE3E6U;
            append_checkpoint(0xE3E4U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        // ---- 00:E3FC  clear the VCE colour table -------------------------
        case 0xE3FCU:  // STZ $0402 -- colour-table address low
            store_port(0xE3FCU, 0x0402U, 0x00U);
            append_checkpoint(0xE3FCU, 0xE3FFU, 18U, 6U);
            pc = 0xE3FFU;
            break;

        case 0xE3FFU:  // STZ $0403
            store_port(0xE3FFU, 0x0403U, 0x00U);
            append_checkpoint(0xE3FFU, 0xE402U, 18U, 6U);
            pc = 0xE402U;
            break;

        case 0xE402U:  // CLY -- 256 iterations of four entries
            registers.y = 0U;
            append_checkpoint(0xE402U, 0xE403U, 6U);
            pc = 0xE403U;
            break;

        case 0xE403U:  // STZ $0404
            store_port(0xE403U, 0x0404U, 0x00U);
            append_checkpoint(0xE403U, 0xE406U, 18U, 6U);
            pc = 0xE406U;
            break;

        case 0xE406U:  // STZ $0405
            store_port(0xE406U, 0x0405U, 0x00U);
            append_checkpoint(0xE406U, 0xE409U, 18U, 6U);
            pc = 0xE409U;
            break;

        case 0xE409U:  // STZ $0404
            store_port(0xE409U, 0x0404U, 0x00U);
            append_checkpoint(0xE409U, 0xE40CU, 18U, 6U);
            pc = 0xE40CU;
            break;

        case 0xE40CU:  // STZ $0405
            store_port(0xE40CU, 0x0405U, 0x00U);
            append_checkpoint(0xE40CU, 0xE40FU, 18U, 6U);
            pc = 0xE40FU;
            break;

        case 0xE40FU:  // STZ $0404
            store_port(0xE40FU, 0x0404U, 0x00U);
            append_checkpoint(0xE40FU, 0xE412U, 18U, 6U);
            pc = 0xE412U;
            break;

        case 0xE412U:  // STZ $0405
            store_port(0xE412U, 0x0405U, 0x00U);
            append_checkpoint(0xE412U, 0xE415U, 18U, 6U);
            pc = 0xE415U;
            break;

        case 0xE415U:  // STZ $0404
            store_port(0xE415U, 0x0404U, 0x00U);
            append_checkpoint(0xE415U, 0xE418U, 18U, 6U);
            pc = 0xE418U;
            break;

        case 0xE418U:  // STZ $0405
            store_port(0xE418U, 0x0405U, 0x00U);
            append_checkpoint(0xE418U, 0xE41BU, 18U, 6U);
            pc = 0xE41BU;
            break;

        case 0xE41BU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE41BU, 0xE41CU, 6U);
            pc = 0xE41CU;
            break;

        case 0xE41CU: {  // BNE $E403
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE403U : 0xE41EU;
            append_checkpoint(0xE41CU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        // ---- 00:E41F  the generic word-stream decoder --------------------
        case 0xE41FU:  // PHY
            push(0xE41FU, registers.y);
            append_checkpoint(0xE41FU, 0xE420U, 9U);
            pc = 0xE420U;
            break;

        case 0xE420U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0xE420U, 0xE422U, 6U);
            pc = 0xE422U;
            break;

        case 0xE422U:  // STA $06
            store(0xE422U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE422U, 0xE424U, 12U);
            pc = 0xE424U;
            break;

        case 0xE424U:  // STA $0000 -- select MAWR
            store_port(0xE424U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE424U, 0xE427U, 18U, 6U);
            pc = 0xE427U;
            break;

        case 0xE427U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xE427U, 0xE429U, 12U);
            pc = 0xE429U;
            break;

        case 0xE429U:  // STA $0002
            store_port(0xE429U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE429U, 0xE42CU, 18U, 6U);
            pc = 0xE42CU;
            break;

        case 0xE42CU:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xE42CU, 0xE42EU, 12U);
            pc = 0xE42EU;
            break;

        case 0xE42EU:  // STA $0003 -- MAWR = $2D:$2E
            store_port(0xE42EU, 0x0003U, registers.accumulator);
            append_checkpoint(0xE42EU, 0xE431U, 18U, 6U);
            pc = 0xE431U;
            break;

        case 0xE431U:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xE431U, 0xE433U, 6U);
            pc = 0xE433U;
            break;

        case 0xE433U:  // STA $06
            store(0xE433U, zp(0x06U), registers.accumulator);
            append_checkpoint(0xE433U, 0xE435U, 12U);
            pc = 0xE435U;
            break;

        case 0xE435U:  // STA $0000 -- select VWR
            store_port(0xE435U, 0x0000U, registers.accumulator);
            append_checkpoint(0xE435U, 0xE438U, 18U, 6U);
            pc = 0xE438U;
            break;

        case 0xE438U:  // CLY -- every record is indexed from its own head
            registers.y = 0U;
            append_checkpoint(0xE438U, 0xE439U, 6U);
            pc = 0xE439U;
            break;

        case 0xE439U:  // LDA ($2F),Y -- the record's command byte
            load_accumulator(source_byte());
            append_checkpoint(0xE439U, 0xE43BU, 21U);
            pc = 0xE43BU;
            break;

        case 0xE43BU: {  // BEQ $E47B -- command $00 terminates
            const bool taken = (registers.status & kZero) != 0U;
            const std::uint16_t next = taken ? 0xE47BU : 0xE43DU;
            append_checkpoint(0xE43BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE43DU: {  // BMI $E452 -- commands $80-$FF repeat
            const bool taken = (registers.status & kNegative) != 0U;
            const std::uint16_t next = taken ? 0xE452U : 0xE43FU;
            append_checkpoint(0xE43DU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE43FU:  // TAX -- literal word count
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE43FU, 0xE440U, 6U);
            pc = 0xE440U;
            break;

        case 0xE440U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE440U, 0xE441U, 6U);
            pc = 0xE441U;
            break;

        case 0xE441U:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE441U, 0xE443U, 21U);
            pc = 0xE443U;
            break;

        case 0xE443U:  // STA $0002
            store_port(0xE443U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE443U, 0xE446U, 18U, 6U);
            pc = 0xE446U;
            break;

        case 0xE446U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE446U, 0xE447U, 6U);
            pc = 0xE447U;
            break;

        case 0xE447U:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE447U, 0xE449U, 21U);
            pc = 0xE449U;
            break;

        case 0xE449U:  // STA $0003 -- the word reaches VRAM here
            store_port(0xE449U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE449U, 0xE44CU, 18U, 6U);
            pc = 0xE44CU;
            break;

        case 0xE44CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE44CU, 0xE44DU, 6U);
            pc = 0xE44DU;
            break;

        case 0xE44DU:  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE44DU, 0xE44EU, 6U);
            pc = 0xE44EU;
            break;

        case 0xE44EU: {  // BNE $E441
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE441U : 0xE450U;
            append_checkpoint(0xE44EU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE450U:  // BRA $E46D
            append_checkpoint(0xE450U, 0xE46DU, 12U);
            pc = 0xE46DU;
            break;

        case 0xE452U:  // AND #$7F -- repeat count, zero meaning 256
            registers.accumulator =
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU);
            registers.status = set_nz(registers.status, registers.accumulator);
            append_checkpoint(0xE452U, 0xE454U, 6U);
            pc = 0xE454U;
            break;

        case 0xE454U:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE454U, 0xE455U, 6U);
            pc = 0xE455U;
            break;

        case 0xE455U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE455U, 0xE456U, 6U);
            pc = 0xE456U;
            break;

        case 0xE456U:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE456U, 0xE458U, 21U);
            pc = 0xE458U;
            break;

        case 0xE458U:  // STA $37 -- the repeated word's low byte
            store(0xE458U, zp(0x37U), registers.accumulator);
            append_checkpoint(0xE458U, 0xE45AU, 12U);
            pc = 0xE45AU;
            break;

        case 0xE45AU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE45AU, 0xE45BU, 6U);
            pc = 0xE45BU;
            break;

        case 0xE45BU:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE45BU, 0xE45DU, 21U);
            pc = 0xE45DU;
            break;

        case 0xE45DU:  // STA $38
            store(0xE45DU, zp(0x38U), registers.accumulator);
            append_checkpoint(0xE45DU, 0xE45FU, 12U);
            pc = 0xE45FU;
            break;

        case 0xE45FU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE45FU, 0xE460U, 6U);
            pc = 0xE460U;
            break;

        case 0xE460U:  // LDA $37
            load_accumulator(read_ram(zp(0x37U)));
            append_checkpoint(0xE460U, 0xE462U, 12U);
            pc = 0xE462U;
            break;

        case 0xE462U:  // STA $0002
            store_port(0xE462U, 0x0002U, registers.accumulator);
            append_checkpoint(0xE462U, 0xE465U, 18U, 6U);
            pc = 0xE465U;
            break;

        case 0xE465U:  // LDA $38
            load_accumulator(read_ram(zp(0x38U)));
            append_checkpoint(0xE465U, 0xE467U, 12U);
            pc = 0xE467U;
            break;

        case 0xE467U:  // STA $0003
            store_port(0xE467U, 0x0003U, registers.accumulator);
            append_checkpoint(0xE467U, 0xE46AU, 18U, 6U);
            pc = 0xE46AU;
            break;

        case 0xE46AU:  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE46AU, 0xE46BU, 6U);
            pc = 0xE46BU;
            break;

        case 0xE46BU: {  // BNE $E460
            const bool taken = (registers.status & kZero) == 0U;
            const std::uint16_t next = taken ? 0xE460U : 0xE46DU;
            append_checkpoint(0xE46BU, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE46DU:  // TYA -- advance the source pointer past the record
            load_accumulator(registers.y);
            append_checkpoint(0xE46DU, 0xE46EU, 6U);
            pc = 0xE46EU;
            break;

        case 0xE46EU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE46EU, 0xE46FU, 6U);
            pc = 0xE46FU;
            break;

        case 0xE46FU:  // ADC $2F
            add_with_carry(read_ram(zp(0x2FU)));
            append_checkpoint(0xE46FU, 0xE471U, 12U);
            pc = 0xE471U;
            break;

        case 0xE471U:  // STA $2F
            store(0xE471U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xE471U, 0xE473U, 12U);
            pc = 0xE473U;
            break;

        case 0xE473U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xE473U, 0xE475U, 12U);
            pc = 0xE475U;
            break;

        case 0xE475U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xE475U, 0xE477U, 6U);
            pc = 0xE477U;
            break;

        case 0xE477U:  // STA $30
            store(0xE477U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xE477U, 0xE479U, 12U);
            pc = 0xE479U;
            break;

        case 0xE479U:  // BRA $E438
            append_checkpoint(0xE479U, 0xE438U, 12U);
            pc = 0xE438U;
            break;

        case 0xE47BU:  // PLY
            registers.y = pop();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE47BU, 0xE47CU, 12U);
            pc = 0xE47CU;
            break;

        // ---- 00:E4E3  byte stream into work RAM --------------------------
        // One transcript owns exactly one stream record. The backward BRA at
        // E530 re-enters this registered address, which keeps the largest
        // literal record below both transaction-channel limits.
        case 0xE4E3U:  // CLY
            registers.y = 0U;
            append_checkpoint(0xE4E3U, 0xE4E4U, 6U);
            pc = 0xE4E4U;
            break;

        case 0xE4E4U:  // LDA ($2F),Y -- command byte
            load_accumulator(source_byte());
            append_checkpoint(0xE4E4U, 0xE4E6U, 21U);
            pc = 0xE4E6U;
            break;

        case 0xE4E6U: {  // BEQ $E532 -- end of stream
            const bool taken = (registers.status & kZero) != 0U;
            const auto next = static_cast<std::uint16_t>(
                taken ? 0xE532U : 0xE4E8U);
            append_checkpoint(0xE4E6U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE4E8U: {  // BMI $E508 -- repeated byte record
            const bool taken = (registers.status & kNegative) != 0U;
            const auto next = static_cast<std::uint16_t>(
                taken ? 0xE508U : 0xE4EAU);
            append_checkpoint(0xE4E8U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE4EAU:  // STA $37 -- literal byte count
            store(0xE4EAU, zp(0x37U), registers.accumulator);
            append_checkpoint(0xE4EAU, 0xE4ECU, 12U);
            pc = 0xE4ECU;
            break;

        case 0xE4ECU:  // CLX
            registers.x = 0U;
            append_checkpoint(0xE4ECU, 0xE4EDU, 6U);
            pc = 0xE4EDU;
            break;

        case 0xE4EDU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE4EDU, 0xE4EEU, 6U);
            pc = 0xE4EEU;
            break;

        case 0xE4EEU:  // LDA ($2F),Y
            load_accumulator(source_byte());
            append_checkpoint(0xE4EEU, 0xE4F0U, 21U);
            pc = 0xE4F0U;
            break;

        case 0xE4F0U: {  // SXY -- destination index into Y
            const auto swapped = registers.x;
            registers.x = registers.y;
            registers.y = swapped;
            append_checkpoint(0xE4F0U, 0xE4F1U, 9U);
            pc = 0xE4F1U;
            break;
        }

        case 0xE4F1U:  // STA ($2D),Y
            store(0xE4F1U, destination_address(), registers.accumulator);
            append_checkpoint(0xE4F1U, 0xE4F3U, 21U);
            pc = 0xE4F3U;
            break;

        case 0xE4F3U:  // INY -- destination index
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE4F3U, 0xE4F4U, 6U);
            pc = 0xE4F4U;
            break;

        case 0xE4F4U:  // INX -- source index
            registers.x = static_cast<std::uint8_t>(registers.x + 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE4F4U, 0xE4F5U, 6U);
            pc = 0xE4F5U;
            break;

        case 0xE4F5U: {  // SXY -- restore source Y and destination X
            const auto swapped = registers.x;
            registers.x = registers.y;
            registers.y = swapped;
            append_checkpoint(0xE4F5U, 0xE4F6U, 9U);
            pc = 0xE4F6U;
            break;
        }

        case 0xE4F6U: {  // DEC $37
            const auto value = static_cast<std::uint8_t>(
                read_ram(zp(0x37U)) - 1U);
            store(0xE4F6U, zp(0x37U), value);
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0xE4F6U, 0xE4F8U, 18U);
            pc = 0xE4F8U;
            break;
        }

        case 0xE4F8U: {  // BNE $E4EE
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = static_cast<std::uint16_t>(
                taken ? 0xE4EEU : 0xE4FAU);
            append_checkpoint(0xE4F8U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE4FAU:  // TXA -- literal count advances destination
            load_accumulator(registers.x);
            append_checkpoint(0xE4FAU, 0xE4FBU, 6U);
            pc = 0xE4FBU;
            break;

        case 0xE4FBU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE4FBU, 0xE4FCU, 6U);
            pc = 0xE4FCU;
            break;

        case 0xE4FCU:  // ADC $2D
            add_with_carry(read_ram(zp(0x2DU)));
            append_checkpoint(0xE4FCU, 0xE4FEU, 12U);
            pc = 0xE4FEU;
            break;

        case 0xE4FEU:  // STA $2D
            store(0xE4FEU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xE4FEU, 0xE500U, 12U);
            pc = 0xE500U;
            break;

        case 0xE500U:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xE500U, 0xE502U, 12U);
            pc = 0xE502U;
            break;

        case 0xE502U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xE502U, 0xE504U, 6U);
            pc = 0xE504U;
            break;

        case 0xE504U:  // STA $2E
            store(0xE504U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xE504U, 0xE506U, 12U);
            pc = 0xE506U;
            break;

        case 0xE506U:  // BRA $E524
            append_checkpoint(0xE506U, 0xE524U, 12U);
            pc = 0xE524U;
            break;

        case 0xE508U:  // AND #$7F -- repeat count, zero means 256
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator & 0x7FU));
            append_checkpoint(0xE508U, 0xE50AU, 6U);
            pc = 0xE50AU;
            break;

        case 0xE50AU:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE50AU, 0xE50BU, 6U);
            pc = 0xE50BU;
            break;

        case 0xE50BU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE50BU, 0xE50CU, 6U);
            pc = 0xE50CU;
            break;

        case 0xE50CU:  // LDA ($2F),Y -- repeated value
            load_accumulator(source_byte());
            append_checkpoint(0xE50CU, 0xE50EU, 21U);
            pc = 0xE50EU;
            break;

        case 0xE50EU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE50EU, 0xE50FU, 6U);
            pc = 0xE50FU;
            break;

        case 0xE50FU:  // PHY -- preserve the two-byte source length
            push(0xE50FU, registers.y);
            append_checkpoint(0xE50FU, 0xE510U, 9U);
            pc = 0xE510U;
            break;

        case 0xE510U:  // CLY
            registers.y = 0U;
            append_checkpoint(0xE510U, 0xE511U, 6U);
            pc = 0xE511U;
            break;

        case 0xE511U:  // STA ($2D),Y
            store(0xE511U, destination_address(), registers.accumulator);
            append_checkpoint(0xE511U, 0xE513U, 21U);
            pc = 0xE513U;
            break;

        case 0xE513U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE513U, 0xE514U, 6U);
            pc = 0xE514U;
            break;

        case 0xE514U:  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xE514U, 0xE515U, 6U);
            pc = 0xE515U;
            break;

        case 0xE515U: {  // BNE $E511
            const bool taken = (registers.status & kZero) == 0U;
            const auto next = static_cast<std::uint16_t>(
                taken ? 0xE511U : 0xE517U);
            append_checkpoint(0xE515U, next, taken ? 12U : 6U);
            pc = next;
            break;
        }

        case 0xE517U:  // TYA -- repeat count advances destination
            load_accumulator(registers.y);
            append_checkpoint(0xE517U, 0xE518U, 6U);
            pc = 0xE518U;
            break;

        case 0xE518U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE518U, 0xE519U, 6U);
            pc = 0xE519U;
            break;

        case 0xE519U:  // ADC $2D
            add_with_carry(read_ram(zp(0x2DU)));
            append_checkpoint(0xE519U, 0xE51BU, 12U);
            pc = 0xE51BU;
            break;

        case 0xE51BU:  // STA $2D
            store(0xE51BU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xE51BU, 0xE51DU, 12U);
            pc = 0xE51DU;
            break;

        case 0xE51DU:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xE51DU, 0xE51FU, 12U);
            pc = 0xE51FU;
            break;

        case 0xE51FU:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xE51FU, 0xE521U, 6U);
            pc = 0xE521U;
            break;

        case 0xE521U:  // STA $2E
            store(0xE521U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xE521U, 0xE523U, 12U);
            pc = 0xE523U;
            break;

        case 0xE523U:  // PLY -- restore source-record length
            registers.y = pop();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0xE523U, 0xE524U, 12U);
            pc = 0xE524U;
            break;

        case 0xE524U:  // TYA -- advance source pointer
            load_accumulator(registers.y);
            append_checkpoint(0xE524U, 0xE525U, 6U);
            pc = 0xE525U;
            break;

        case 0xE525U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xE525U, 0xE526U, 6U);
            pc = 0xE526U;
            break;

        case 0xE526U:  // ADC $2F
            add_with_carry(read_ram(zp(0x2FU)));
            append_checkpoint(0xE526U, 0xE528U, 12U);
            pc = 0xE528U;
            break;

        case 0xE528U:  // STA $2F
            store(0xE528U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xE528U, 0xE52AU, 12U);
            pc = 0xE52AU;
            break;

        case 0xE52AU:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xE52AU, 0xE52CU, 12U);
            pc = 0xE52CU;
            break;

        case 0xE52CU:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xE52CU, 0xE52EU, 6U);
            pc = 0xE52EU;
            break;

        case 0xE52EU:  // STA $30
            store(0xE52EU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xE52EU, 0xE530U, 12U);
            pc = 0xE530U;
            break;

        case 0xE530U:  // BRA $E4E3 -- next record is a new transaction
            append_checkpoint(0xE530U, 0xE4E3U, 12U);
            pc = 0xE4E3U;
            finish(pc);
            return replacement;

        default:
            return std::nullopt;
        }
    }
}

std::optional<RoutineReplacement> replace_bank1_vdc_row_upload(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // 01:DE9C prepares MAWR twice and emits two $E2xx words per pass.  The
    // backwards BNE at DF02 returns to DEA2, so the complete row is bounded
    // by the caller's $2D55 count and fits the replacement channels.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xDE9CU};
    constexpr std::uint8_t kBank1 = 0x01U;
    // Each pass has twelve VDC writes and eight work-RAM stores.  Keeping the
    // cap at 24 leaves room below the core's 512-write channel limit even for
    // the terminal flag store; larger caller counts fall back to assembly.
    constexpr std::uint8_t kMaximumRows = 24U;
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || rom.size() != 0x60000U || entry.registers.mpr[0] != 0xFFU
        || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }
    const auto ram_at = [&ram](const std::uint16_t address) {
        return ram[address - kWorkRamBase];
    };
    const auto zp = [](const std::uint8_t offset) {
        return static_cast<std::uint16_t>(kWorkRamBase + offset);
    };
    if (const auto rows = ram_at(0x2D55U);
        rows == 0U || rows > kMaximumRows) {
        return std::nullopt;
    }

    RoutineReplacement replacement;
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    std::size_t committed_writes = 0U;
    bool overflow = false;
    const auto commit_writes = [&replacement, &committed_writes](
                                   const std::uint32_t at_cycle) {
        while (committed_writes < replacement.work_ram_writes.size()) {
            replacement.work_ram_writes[committed_writes]
                .master_cycle_offset = at_cycle;
            ++committed_writes;
        }
    };
    const auto checkpoint = [&replacement, &registers, &cycle, &commit_writes](
                                const std::uint16_t instruction_pc,
                                const std::uint16_t resume_pc,
                                const std::uint32_t instruction_cycles,
                                const std::uint32_t sample_back = 3U) {
        cycle += instruction_cycles;
        commit_writes(cycle);
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - sample_back,
            .master_cycle_offset = cycle,
            .instruction = {kBank1, instruction_pc},
            .registers = registers,
        });
    };
    const auto write = [&replacement](const std::uint16_t instruction_pc,
                                      const std::uint16_t address,
                                      const std::uint8_t value) {
        replacement.work_ram_writes.push_back({
            .instruction = {kBank1, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = 0U,
        });
    };
    const auto store_ram = [&ram, &write, &checkpoint](
                               const std::uint16_t instruction_pc,
                               const std::uint16_t resume_pc,
                               const std::uint16_t address,
                               const std::uint8_t value) {
        ram[address - kWorkRamBase] = value;
        write(instruction_pc, address, value);
        checkpoint(instruction_pc, resume_pc, 12U);
    };
    const auto store_port = [&write, &checkpoint](
                                const std::uint16_t instruction_pc,
                                const std::uint16_t resume_pc,
                                const std::uint16_t address,
                                const std::uint8_t value) {
        write(instruction_pc, address, value);
        checkpoint(instruction_pc, resume_pc, 18U, 6U);
    };
    const auto load = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_x = [&registers](const std::uint8_t value) {
        registers.x = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto adc = [&registers](const std::uint8_t operand) {
        const auto left = registers.accumulator;
        const auto sum = static_cast<std::uint16_t>(left)
            + operand + (registers.status & kCarry);
        const auto value = static_cast<std::uint8_t>(sum);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFFU) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        if (((left ^ value) & (operand ^ value) & kNegative) != 0U) {
            status = static_cast<std::uint8_t>(status | kOverflow);
        }
        registers.accumulator = value;
        registers.status = set_nz(status, value);
    };
    const auto sbc = [&registers](const std::uint8_t operand) {
        const auto left = registers.accumulator;
        const auto borrow = (registers.status & kCarry) == 0U ? 1U : 0U;
        const auto difference = static_cast<std::uint16_t>(left) - operand - borrow;
        const auto value = static_cast<std::uint8_t>(difference);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (static_cast<std::uint16_t>(left)
            >= static_cast<std::uint16_t>(operand) + borrow) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        if (((left ^ operand) & (left ^ value) & kNegative) != 0U) {
            status = static_cast<std::uint8_t>(status | kOverflow);
        }
        registers.accumulator = value;
        registers.status = set_nz(status, value);
    };
    const auto mapped_source = [&registers, &ram, rom, &overflow](
                                   const std::uint16_t address) {
        const auto bank = registers.mpr[address >> 13U];
        if (bank == 0xF8U) {
            return ram[address & 0x1FFFU];
        }
        if (bank >= 0x80U) {
            overflow = true;
            return static_cast<std::uint8_t>(0U);
        }
        const auto rom_bank = bank < 0x40U
            ? static_cast<std::size_t>(bank & 0x1FU)
            : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
        return std::to_integer<std::uint8_t>(
            rom[rom_bank * 0x2000U + (address & 0x1FFFU)]);
    };
    const auto source = [&ram_at, &zp, &registers, &mapped_source]() {
        const auto pointer = static_cast<std::uint16_t>(
            ram_at(zp(0x92U))
            | static_cast<std::uint16_t>(ram_at(zp(0x93U)) << 8U));
        return mapped_source(static_cast<std::uint16_t>(pointer + registers.y));
    };

    load_x(ram_at(0x2D55U));
    checkpoint(0xDE9CU, 0xDE9FU, 15U);
    load_y(ram_at(0x2D65U));
    checkpoint(0xDE9FU, 0xDEA2U, 15U);
    while (!overflow && registers.x != 0U) {
        load(0U);
        checkpoint(0xDEA2U, 0xDEA4U, 6U);
        store_ram(0xDEA4U, 0xDEA6U, zp(0x06U), registers.accumulator);
        store_port(0xDEA6U, 0xDEA9U, 0x0000U, registers.accumulator);
        load(ram_at(zp(0x2DU))); checkpoint(0xDEA9U, 0xDEABU, 12U);
        store_port(0xDEABU, 0xDEAEU, 0x0002U, registers.accumulator);
        load(ram_at(zp(0x2EU))); checkpoint(0xDEAEU, 0xDEB0U, 12U);
        store_port(0xDEB0U, 0xDEB3U, 0x0003U, registers.accumulator);
        load(2U); checkpoint(0xDEB3U, 0xDEB5U, 6U);
        store_ram(0xDEB5U, 0xDEB7U, zp(0x06U), registers.accumulator);
        store_port(0xDEB7U, 0xDEBAU, 0x0000U, registers.accumulator);
        load(source()); checkpoint(0xDEBAU, 0xDEBCU, 21U);
        store_port(0xDEBCU, 0xDEBFU, 0x0002U, registers.accumulator);
        load(0xE2U); checkpoint(0xDEBFU, 0xDEC1U, 6U);
        store_port(0xDEC1U, 0xDEC4U, 0x0003U, registers.accumulator);
        load_y(static_cast<std::uint8_t>(registers.y + 1U));
        checkpoint(0xDEC4U, 0xDEC5U, 6U);
        registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        checkpoint(0xDEC5U, 0xDEC6U, 6U);
        load(ram_at(zp(0x2DU))); checkpoint(0xDEC6U, 0xDEC8U, 12U);
        adc(0x40U); checkpoint(0xDEC8U, 0xDECAU, 6U);
        store_ram(0xDECAU, 0xDECCU, zp(0x2DU), registers.accumulator);
        registers.accumulator = 0U; // CLA preserves P, including low-byte carry.
        checkpoint(0xDECCU, 0xDECDU, 6U);
        adc(ram_at(zp(0x2EU))); checkpoint(0xDECDU, 0xDECFU, 12U);
        store_ram(0xDECFU, 0xDED1U, zp(0x2EU), registers.accumulator);

        load(0U); checkpoint(0xDED1U, 0xDED3U, 6U);
        store_ram(0xDED3U, 0xDED5U, zp(0x06U), registers.accumulator);
        store_port(0xDED5U, 0xDED8U, 0x0000U, registers.accumulator);
        load(ram_at(zp(0x2DU))); checkpoint(0xDED8U, 0xDEDAU, 12U);
        store_port(0xDEDAU, 0xDEDDU, 0x0002U, registers.accumulator);
        load(ram_at(zp(0x2EU))); checkpoint(0xDEDDU, 0xDEDFU, 12U);
        store_port(0xDEDFU, 0xDEE2U, 0x0003U, registers.accumulator);
        load(2U); checkpoint(0xDEE2U, 0xDEE4U, 6U);
        store_ram(0xDEE4U, 0xDEE6U, zp(0x06U), registers.accumulator);
        store_port(0xDEE6U, 0xDEE9U, 0x0000U, registers.accumulator);
        load(source()); checkpoint(0xDEE9U, 0xDEEBU, 21U);
        store_port(0xDEEBU, 0xDEEEU, 0x0002U, registers.accumulator);
        load(0xE2U); checkpoint(0xDEEEU, 0xDEF0U, 6U);
        store_port(0xDEF0U, 0xDEF3U, 0x0003U, registers.accumulator);
        load_y(static_cast<std::uint8_t>(registers.y + 1U));
        checkpoint(0xDEF3U, 0xDEF4U, 6U);
        registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        checkpoint(0xDEF4U, 0xDEF5U, 6U);
        load(ram_at(zp(0x2DU))); checkpoint(0xDEF5U, 0xDEF7U, 12U);
        sbc(0x3FU); checkpoint(0xDEF7U, 0xDEF9U, 6U);
        store_ram(0xDEF9U, 0xDEFB, zp(0x2DU), registers.accumulator);
        load(ram_at(zp(0x2EU))); checkpoint(0xDEFBU, 0xDEFD, 12U);
        sbc(0U); checkpoint(0xDEFD, 0xDEFF, 6U);
        store_ram(0xDEFFU, 0xDF01U, zp(0x2EU), registers.accumulator);
        load_x(static_cast<std::uint8_t>(registers.x - 1U));
        checkpoint(0xDF01U, 0xDF02U, 6U);
        const bool repeat = registers.x != 0U;
        checkpoint(0xDF02U, repeat ? 0xDEA2U : 0xDF04U, repeat ? 12U : 6U);
    }
    if (overflow || replacement.work_ram_writes.size() > 512U
        || replacement.checkpoints.size() > 2048U) {
        return std::nullopt;
    }

    load(source()); checkpoint(0xDF04U, 0xDF06U, 21U);
    if ((registers.status & kZero) == 0U) {
        checkpoint(0xDF06U, 0xDF0EU, 12U);
        replacement.return_instruction = {kBank1, 0xDF0EU};
    } else {
        checkpoint(0xDF06U, 0xDF08U, 6U);
        load(1U); checkpoint(0xDF08U, 0xDF0AU, 6U);
        store_ram(0xDF0AU, 0xDF0DU, 0x2D68U, registers.accumulator);
        replacement.return_instruction = {kBank1, 0xDF0DU};
    }
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

std::optional<RoutineReplacement> replace_vdc_sprite_attribute_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // 01:D950 builds a sprite-attribute-table address from the caller's
    // $35/$36 pair, merges the nibble selected by $27, and then programs MAWR
    // before reading the two live VDC data bytes at DA06/DA0B.  The data reads
    // deliberately remain core instructions: they consume the VDC's selected
    // register/read buffer.  This transcript owns only the deterministic head
    // through DA06, and retains the two early exits to DA3E as core resumes.
    constexpr RoutineKey kRoutine{.physical_bank = 0x01U, .logical_pc = 0xD950U};
    constexpr std::uint8_t kBank1 = 0x01U;
    const bool attribute_tail = entry.routine == RoutineKey{1U, 0xDA0EU};
    if ((entry.routine != kRoutine && !attribute_tail) || work_ram.size() < 0x2000U
        || rom.size() != 0x60000U || entry.registers.mpr[0] != 0xFFU
        || (attribute_tail && entry.registers.mpr[6] != 1U)
        || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }
    RoutineReplacement replacement{};
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    std::size_t committed_writes = 0U;
    bool overflow = false;
    const auto set_nz_local = [=](const std::uint8_t status, const std::uint8_t value) {
        auto result = static_cast<std::uint8_t>(status & static_cast<std::uint8_t>(~(kZero | kNegative)));
        if (value == 0U) {
            result = static_cast<std::uint8_t>(result | kZero);
        }
        return static_cast<std::uint8_t>(result | (value & kNegative));
    };
    const auto zp = [=](const std::uint8_t offset) {
        return static_cast<std::uint16_t>(kWorkRamBase + offset);
    };
    const auto read_ram = [&ram, &overflow](const std::uint16_t address) {
        if (address < kWorkRamBase || address >= kWorkRamEnd) {
            overflow = true;
            return static_cast<std::uint8_t>(0U);
        }
        return ram[address - kWorkRamBase];
    };
    const auto mapped_read = [&registers, &ram, &rom, &overflow](const std::uint16_t address) {
        const auto bank = registers.mpr[address >> 13U];
        if (bank == 0xF8U) {
            return ram[address & 0x1FFFU];
        }
        if (bank >= 0x80U) {
            overflow = true;
            return static_cast<std::uint8_t>(0U);
        }
        const auto rom_bank = bank < 0x40U
            ? static_cast<std::size_t>(bank & 0x1FU)
            : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
        return std::to_integer<std::uint8_t>(rom[rom_bank * 0x2000U + (address & 0x1FFFU)]);
    };
    const auto commit_writes = [&replacement, &committed_writes](const std::uint32_t at_cycle) {
        while (committed_writes < replacement.work_ram_writes.size()) {
            replacement.work_ram_writes[committed_writes++].master_cycle_offset = at_cycle;
        }
    };
    const auto checkpoint = [&replacement, &registers, &cycle, &commit_writes](
                                const std::uint16_t instruction_pc,
                                const std::uint16_t resume_pc,
                                const std::uint32_t instruction_cycles,
                                const std::uint32_t sample_back = 3U) {
        cycle += instruction_cycles;
        commit_writes(cycle);
        registers.program_counter = resume_pc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - sample_back,
            .master_cycle_offset = cycle,
            .instruction = {kBank1, instruction_pc},
            .registers = registers,
        });
    };
    const auto write = [&replacement](const std::uint16_t instruction_pc,
                                      const std::uint16_t address,
                                      const std::uint8_t value) {
        replacement.work_ram_writes.push_back({
            .instruction = {kBank1, instruction_pc},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = 0U,
        });
    };
    const auto store_ram = [&ram, &overflow, &write](const std::uint16_t pc,
                                                       const std::uint16_t address,
                                                       const std::uint8_t value) {
        if (address < kWorkRamBase || address >= kWorkRamEnd) {
            overflow = true;
            return;
        }
        ram[address - kWorkRamBase] = value;
        write(pc, address, value);
    };
    const auto indirect_address = [&read_ram, &registers, &zp]() {
        const auto base = static_cast<std::uint16_t>(
            read_ram(zp(0x2DU))
            | static_cast<std::uint16_t>(read_ram(zp(0x2EU)) << 8U));
        return static_cast<std::uint16_t>(base + registers.y);
    };
    const auto load = [&registers, &set_nz_local](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz_local(registers.status, value);
    };
    const auto load_y = [&registers, &set_nz_local](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz_local(registers.status, value);
    };
    const auto rol_ram = [&registers, &ram, &set_nz_local, &write](
                             const std::uint16_t pc,
                             const std::uint16_t address) {
        const auto value = ram[address - kWorkRamBase];
        const bool carry = (value & 0x80U) != 0U;
        const auto result = static_cast<std::uint8_t>((value << 1U) | (registers.status & kCarry));
        ram[address - kWorkRamBase] = result;
        registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if (carry) {
            registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        }
        registers.status = set_nz_local(registers.status, result);
        write(pc, address, result);
    };
    const auto asl_accumulator = [&registers, &set_nz_local]() {
        const bool carry = (registers.accumulator & 0x80U) != 0U;
        registers.accumulator = static_cast<std::uint8_t>(registers.accumulator << 1U);
        registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if (carry) {
            registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        }
        registers.status = set_nz_local(registers.status, registers.accumulator);
    };
    const auto asl_ram = [&registers, &ram, &set_nz_local, &write](
                             const std::uint16_t pc,
                             const std::uint16_t address) {
        const auto value = ram[address - kWorkRamBase];
        const bool carry = (value & 0x80U) != 0U;
        const auto result = static_cast<std::uint8_t>(value << 1U);
        ram[address - kWorkRamBase] = result;
        registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if (carry) {
            registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        }
        registers.status = set_nz_local(registers.status, result);
        write(pc, address, result);
    };
    const auto ror_ram = [&registers, &ram, &set_nz_local, &write](
                             const std::uint16_t pc,
                             const std::uint16_t address) {
        const auto value = ram[address - kWorkRamBase];
        const bool carry = (value & 0x01U) != 0U;
        const auto result = static_cast<std::uint8_t>((value >> 1U) | ((registers.status & kCarry) << 7U));
        ram[address - kWorkRamBase] = result;
        registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if (carry) {
            registers.status = static_cast<std::uint8_t>(registers.status | kCarry);
        }
        registers.status = set_nz_local(registers.status, result);
        write(pc, address, result);
    };
    const auto adc = [&registers, &set_nz_local](const std::uint8_t operand) {
        const auto left = registers.accumulator;
        const auto sum = static_cast<std::uint16_t>(left) + operand + (registers.status & kCarry);
        const auto value = static_cast<std::uint8_t>(sum);
        registers.status = static_cast<std::uint8_t>(registers.status & ~(kCarry | kOverflow));
        if (sum > 0xFFU) { registers.status = static_cast<std::uint8_t>(registers.status | kCarry); }
        if (((left ^ value) & (operand ^ value) & kNegative) != 0U) { registers.status = static_cast<std::uint8_t>(registers.status | kOverflow); }
        registers.accumulator = value;
        registers.status = set_nz_local(registers.status, value);
    };
    const auto sbc = [&registers, &set_nz_local](const std::uint8_t operand) {
        const auto left = registers.accumulator;
        const auto borrow = (registers.status & kCarry) == 0U ? 1U : 0U;
        const auto difference = static_cast<std::uint16_t>(left) - operand - borrow;
        const auto value = static_cast<std::uint8_t>(difference);
        registers.status = static_cast<std::uint8_t>(registers.status & ~(kCarry | kOverflow));
        if (static_cast<std::uint16_t>(left) >= static_cast<std::uint16_t>(operand) + borrow) { registers.status = static_cast<std::uint8_t>(registers.status | kCarry); }
        if (((left ^ operand) & (left ^ value) & kNegative) != 0U) { registers.status = static_cast<std::uint8_t>(registers.status | kOverflow); }
        registers.accumulator = value;
        registers.status = set_nz_local(registers.status, value);
    };
    const auto finish = [&replacement, &registers, &cycle](const std::uint16_t pc) {
        registers.program_counter = pc;
        replacement.return_instruction = {kBank1, pc};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
    };

    if (attribute_tail) {
        // DA06/DA0B have already consumed the live device read buffer.
        // Resume at the independent live-stack PLY; DA3F then owns its RTS.
        load(static_cast<std::uint8_t>(registers.accumulator & 0x0FU));
        checkpoint(0xDA0E, 0xDA10, 6);
        for (std::uint16_t pc = 0xDA10; pc < 0xDA18; pc += 2U) {
            asl_ram(pc, zp(0x29)); checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 18);
        }
        load(static_cast<std::uint8_t>(registers.accumulator | read_ram(zp(0x29))));
        checkpoint(0xDA18, 0xDA1A, 12);
        store_ram(0xDA1A, zp(0x29), registers.accumulator); checkpoint(0xDA1A, 0xDA1C, 12);
        load(0); checkpoint(0xDA1C, 0xDA1E, 6);
        store_ram(0xDA1E, zp(6), 0); checkpoint(0xDA1E, 0xDA20, 12);
        write(0xDA20, 0, 0); checkpoint(0xDA20, 0xDA23, 18, 6);
        load(read_ram(zp(0x2D))); checkpoint(0xDA23, 0xDA25, 12);
        write(0xDA25, 2, registers.accumulator); checkpoint(0xDA25, 0xDA28, 18, 6);
        load(read_ram(zp(0x2E))); checkpoint(0xDA28, 0xDA2A, 12);
        write(0xDA2A, 3, registers.accumulator); checkpoint(0xDA2A, 0xDA2D, 18, 6);
        load(2); checkpoint(0xDA2D, 0xDA2F, 6);
        store_ram(0xDA2F, zp(6), 2); checkpoint(0xDA2F, 0xDA31, 12);
        write(0xDA31, 0, 2); checkpoint(0xDA31, 0xDA34, 18, 6);
        load(read_ram(zp(0x28))); checkpoint(0xDA34, 0xDA36, 12);
        write(0xDA36, 2, registers.accumulator); checkpoint(0xDA36, 0xDA39, 18, 6);
        load(read_ram(zp(0x29))); checkpoint(0xDA39, 0xDA3B, 12);
        write(0xDA3B, 3, registers.accumulator); checkpoint(0xDA3B, 0xDA3E, 18, 6);
        finish(0xDA3E);
        return replacement;
    }

    // PHY; copy and scale the caller's sprite coordinates.
    store_ram(0xD950U, static_cast<std::uint16_t>(kStackBase + registers.stack_pointer), registers.y);
    registers.stack_pointer = static_cast<std::uint8_t>(registers.stack_pointer - 1U);
    checkpoint(0xD950U, 0xD951U, 9U);
    load(read_ram(zp(0x37U))); checkpoint(0xD951U, 0xD953U, 12U);
    store_ram(0xD953U, zp(0x26U), registers.accumulator); checkpoint(0xD953U, 0xD955U, 12U);
    store_ram(0xD955U, zp(0x29U), registers.accumulator); checkpoint(0xD955U, 0xD957U, 12U);
    load(read_ram(zp(0x35U))); checkpoint(0xD957U, 0xD959U, 12U);
    store_ram(0xD959U, zp(0x27U), registers.accumulator); checkpoint(0xD959U, 0xD95BU, 12U);
    load(read_ram(zp(0x36U))); checkpoint(0xD95BU, 0xD95DU, 12U);
    store_ram(0xD95DU, zp(0x2EU), 0U); checkpoint(0xD95DU, 0xD95FU, 12U);
    for (const auto instruction : {0xD95FU, 0xD962U, 0xD965U, 0xD968U}) {
        asl_accumulator(); checkpoint(static_cast<std::uint16_t>(instruction), static_cast<std::uint16_t>(instruction + 1U), 6U);
        rol_ram(static_cast<std::uint16_t>(instruction + 1U), zp(0x2EU)); checkpoint(static_cast<std::uint16_t>(instruction + 1U), static_cast<std::uint16_t>(instruction + 3U), 18U);
    }
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry); checkpoint(0xD96BU, 0xD96CU, 6U);
    adc(0x59U); checkpoint(0xD96CU, 0xD96EU, 6U);
    store_ram(0xD96EU, zp(0x2DU), registers.accumulator); checkpoint(0xD96EU, 0xD970U, 12U);
    load(read_ram(zp(0x2EU))); checkpoint(0xD970U, 0xD972U, 12U);
    adc(0x22U); checkpoint(0xD972U, 0xD974U, 6U);
    store_ram(0xD974U, zp(0x2EU), registers.accumulator); checkpoint(0xD974U, 0xD976U, 12U);
    ror_ram(0xD976U, zp(0x27U)); checkpoint(0xD976U, 0xD978U, 18U);
    load_y(read_ram(zp(0x27U))); checkpoint(0xD978U, 0xD97AU, 12U);
    if ((registers.status & kCarry) != 0U) {
        checkpoint(0xD97AU, 0xD97CU, 6U);
        load(mapped_read(indirect_address())); checkpoint(0xD97CU, 0xD97EU, 21U);
        load(static_cast<std::uint8_t>(registers.accumulator & 0xF0U)); checkpoint(0xD97EU, 0xD980U, 6U);
        load(static_cast<std::uint8_t>(registers.accumulator | read_ram(zp(0x26U)))); checkpoint(0xD980U, 0xD982U, 12U);
        store_ram(0xD982U, indirect_address(), registers.accumulator); checkpoint(0xD982U, 0xD984U, 21U);
        checkpoint(0xD984U, 0xD996U, 12U);
    } else {
        checkpoint(0xD97AU, 0xD986U, 12U);
        for (const auto instruction : {0xD986U, 0xD988U, 0xD98AU, 0xD98CU}) {
            asl_ram(static_cast<std::uint16_t>(instruction), zp(0x26U)); checkpoint(static_cast<std::uint16_t>(instruction), static_cast<std::uint16_t>(instruction + 2U), 18U);
        }
        load(mapped_read(indirect_address())); checkpoint(0xD98EU, 0xD990U, 21U);
        load(static_cast<std::uint8_t>(registers.accumulator & 0x0FU)); checkpoint(0xD990U, 0xD992U, 6U);
        load(static_cast<std::uint8_t>(registers.accumulator | read_ram(zp(0x26U)))); checkpoint(0xD992U, 0xD994U, 12U);
        store_ram(0xD994U, indirect_address(), registers.accumulator); checkpoint(0xD994U, 0xD996U, 21U);
    }
    load(read_ram(zp(0x36U))); checkpoint(0xD996U, 0xD998U, 12U);
    store_ram(0xD998U, zp(0x2EU), 0U); checkpoint(0xD998U, 0xD99AU, 12U);
    for (const auto instruction : {0xD99AU, 0xD99DU, 0xD9A0U}) {
        asl_accumulator(); checkpoint(static_cast<std::uint16_t>(instruction), static_cast<std::uint16_t>(instruction + 1U), 6U);
        rol_ram(static_cast<std::uint16_t>(instruction + 1U), zp(0x2EU)); checkpoint(static_cast<std::uint16_t>(instruction + 1U), static_cast<std::uint16_t>(instruction + 3U), 18U);
    }
    store_ram(0xD9A3U, zp(0x2DU), registers.accumulator); checkpoint(0xD9A3U, 0xD9A5U, 12U);
    load(read_ram(0x2D53U)); checkpoint(0xD9A5U, 0xD9A8U, 15U);
    if (registers.accumulator == 0U) {
        checkpoint(0xD9A8U, 0xD9AAU, 6U);
        registers.status = static_cast<std::uint8_t>(registers.status | kCarry); checkpoint(0xD9AAU, 0xD9ABU, 6U);
        load(read_ram(zp(0x2DU))); checkpoint(0xD9ABU, 0xD9ADU, 12U);
        sbc(read_ram(zp(0xA9U))); checkpoint(0xD9ADU, 0xD9AFU, 12U);
        store_ram(0xD9AFU, zp(0x2DU), registers.accumulator); checkpoint(0xD9AFU, 0xD9B1U, 12U);
        load(read_ram(zp(0x2EU))); checkpoint(0xD9B1U, 0xD9B3U, 12U);
        sbc(read_ram(zp(0xAAU))); checkpoint(0xD9B3U, 0xD9B5U, 12U);
        store_ram(0xD9B5U, zp(0x2EU), registers.accumulator); checkpoint(0xD9B5U, 0xD9B7U, 12U);
        checkpoint(0xD9B7U, 0xD9C8U, 12U);
    } else {
        checkpoint(0xD9A8U, 0xD9B9U, 12U);
        registers.status = static_cast<std::uint8_t>(registers.status | kCarry); checkpoint(0xD9B9U, 0xD9BAU, 6U);
        load(read_ram(zp(0x2DU))); checkpoint(0xD9BAU, 0xD9BCU, 12U);
        sbc(read_ram(0x2CF0U)); checkpoint(0xD9BCU, 0xD9BFU, 15U);
        store_ram(0xD9BFU, zp(0x2DU), registers.accumulator); checkpoint(0xD9BFU, 0xD9C1U, 12U);
        load(read_ram(zp(0x2EU))); checkpoint(0xD9C1U, 0xD9C3U, 12U);
        sbc(read_ram(0x2CF1U)); checkpoint(0xD9C3U, 0xD9C6U, 15U);
        store_ram(0xD9C6U, zp(0x2EU), registers.accumulator); checkpoint(0xD9C6U, 0xD9C8U, 12U);
    }
    if ((registers.status & kNegative) != 0U) { checkpoint(0xD9C8U, 0xDA3EU, 12U); finish(0xDA3EU); return overflow ? std::nullopt : std::optional<RoutineReplacement>{replacement}; }
    checkpoint(0xD9C8U, 0xD9CAU, 6U);
    if ((registers.status & kZero) != 0U) { checkpoint(0xD9CAU, 0xD9CCU, 6U); } else { checkpoint(0xD9CAU, 0xDA3EU, 12U); finish(0xDA3EU); return overflow ? std::nullopt : std::optional<RoutineReplacement>{replacement}; }
    load(read_ram(zp(0x2DU))); checkpoint(0xD9CCU, 0xD9CEU, 12U);
    const auto difference = static_cast<std::uint8_t>(registers.accumulator - 0xF0U);
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
    if (registers.accumulator >= 0xF0U) { registers.status = static_cast<std::uint8_t>(registers.status | kCarry); }
    registers.status = set_nz_local(registers.status, difference); checkpoint(0xD9CEU, 0xD9D0U, 6U);
    if ((registers.status & kCarry) != 0U) { checkpoint(0xD9D0U, 0xDA3EU, 12U); finish(0xDA3EU); return overflow ? std::nullopt : std::optional<RoutineReplacement>{replacement}; }
    checkpoint(0xD9D0U, 0xD9D2U, 6U);
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry); checkpoint(0xD9D2U, 0xD9D3U, 6U);
    load(read_ram(zp(0x0BU))); checkpoint(0xD9D3U, 0xD9D5U, 12U);
    adc(read_ram(zp(0x2DU))); checkpoint(0xD9D5U, 0xD9D7U, 12U);
    load(static_cast<std::uint8_t>(registers.accumulator & 0xF8U)); checkpoint(0xD9D7U, 0xD9D9U, 6U);
    store_ram(0xD9D9U, zp(0x2EU), 0U); checkpoint(0xD9D9U, 0xD9DBU, 12U);
    for (const auto instruction : {0xD9DBU, 0xD9DEU, 0xD9E1U}) {
        asl_accumulator(); checkpoint(static_cast<std::uint16_t>(instruction), static_cast<std::uint16_t>(instruction + 1U), 6U);
        rol_ram(static_cast<std::uint16_t>(instruction + 1U), zp(0x2EU)); checkpoint(static_cast<std::uint16_t>(instruction + 1U), static_cast<std::uint16_t>(instruction + 3U), 18U);
    }
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry); checkpoint(0xD9E4U, 0xD9E5U, 6U);
    adc(read_ram(zp(0x35U))); checkpoint(0xD9E5U, 0xD9E7U, 12U);
    store_ram(0xD9E7U, zp(0x2DU), registers.accumulator); checkpoint(0xD9E7U, 0xD9E9U, 12U);
    registers.accumulator = 0U; checkpoint(0xD9E9U, 0xD9EAU, 6U);
    adc(read_ram(zp(0x2EU))); checkpoint(0xD9EAU, 0xD9ECU, 12U);
    store_ram(0xD9ECU, zp(0x2EU), registers.accumulator); checkpoint(0xD9ECU, 0xD9EEU, 12U);
    load(1U); checkpoint(0xD9EEU, 0xD9F0U, 6U);
    store_ram(0xD9F0U, zp(0x06U), registers.accumulator); checkpoint(0xD9F0U, 0xD9F2U, 12U);
    write(0xD9F2U, 0x0000U, registers.accumulator); checkpoint(0xD9F2U, 0xD9F5U, 18U, 6U);
    load(read_ram(zp(0x2DU))); checkpoint(0xD9F5U, 0xD9F7U, 12U);
    write(0xD9F7U, 0x0002U, registers.accumulator); checkpoint(0xD9F7U, 0xD9FAU, 18U, 6U);
    load(read_ram(zp(0x2EU))); checkpoint(0xD9FAU, 0xD9FCU, 12U);
    write(0xD9FCU, 0x0003U, registers.accumulator); checkpoint(0xD9FCU, 0xD9FFU, 18U, 6U);
    load(2U); checkpoint(0xD9FFU, 0xDA01U, 6U);
    store_ram(0xDA01U, zp(0x06U), registers.accumulator); checkpoint(0xDA01U, 0xDA03U, 12U);
    write(0xDA03U, 0x0000U, registers.accumulator); checkpoint(0xDA03U, 0xDA06U, 18U, 6U);
    finish(0xDA06U);
    if (overflow || replacement.work_ram_writes.size() > 512U || replacement.checkpoints.size() > 2048U) {
        return std::nullopt;
    }
    return replacement;
}

std::optional<RoutineReplacement> replace_actor_script_record_loader(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kLoader{0x00U, 0xF605U};
    constexpr RoutineKey kDecoderCall{0x00U, 0xF660U};
    constexpr RoutineKey kReturnTail{0x00U, 0xF664U};
    if ((entry.routine != kLoader && entry.routine != kDecoderCall
         && entry.routine != kReturnTail)
        || work_ram.size() < 0x2000U || rom.size() != 0x60000U
        || entry.registers.mpr[1U] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    auto ram = std::array<std::uint8_t, 0x2000U>{};
    for (std::size_t index = 0U; index < ram.size(); ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement;
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool unsupported = false;
    std::size_t committed_writes = 0U;

    const auto read_ram = [&ram, &unsupported](const std::uint16_t address) {
        if (address < kWorkRamBase || address >= kWorkRamEnd) {
            unsupported = true;
            return std::uint8_t{0U};
        }
        return ram[address - kWorkRamBase];
    };
    const auto write_ram =
        [&replacement, &ram, &unsupported](const std::uint16_t instruction,
                                            const std::uint16_t address,
                                            const std::uint8_t value) {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                unsupported = true;
                return;
            }
            ram[address - kWorkRamBase] = value;
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };
    const auto mapped_read =
        [&registers, rom, &unsupported](const std::uint16_t address) {
            const auto bank = registers.mpr[address >> 13U];
            if (bank >= 0x80U) {
                unsupported = true;
                return std::uint8_t{0U};
            }
            const auto rom_bank = bank < 0x40U
                ? static_cast<std::size_t>(bank & 0x1FU)
                : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
            const auto offset = rom_bank * 0x2000U
                + static_cast<std::size_t>(address & 0x1FFFU);
            if (offset >= rom.size()) {
                unsupported = true;
                return std::uint8_t{0U};
            }
            return std::to_integer<std::uint8_t>(rom[offset]);
        };
    const auto commit = [&replacement, &committed_writes](
                            const std::uint32_t at_cycle) {
        while (committed_writes < replacement.work_ram_writes.size()) {
            replacement.work_ram_writes[committed_writes]
                .master_cycle_offset = at_cycle;
            ++committed_writes;
        }
    };
    const auto checkpoint =
        [&replacement, &registers, &cycle, &commit](
            const std::uint16_t instruction,
            const std::uint16_t resume,
            const std::uint32_t master_cycles) {
            cycle += master_cycles;
            commit(cycle);
            registers.program_counter = resume;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {0x00U, instruction},
                .registers = registers,
            });
        };
    const auto set_register_nz = [&registers](const std::uint8_t value) {
        registers.status = set_nz(registers.status, value);
    };
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };
    const auto push = [&registers, &write_ram](const std::uint16_t instruction,
                                                const std::uint8_t value) {
        write_ram(
            instruction,
            static_cast<std::uint16_t>(kStackBase + registers.stack_pointer),
            value);
        registers.stack_pointer = static_cast<std::uint8_t>(
            registers.stack_pointer - 1U);
    };

    if (entry.routine == kReturnTail) {
        registers.stack_pointer = static_cast<std::uint8_t>(
            registers.stack_pointer + 1U);
        registers.x = read_ram(static_cast<std::uint16_t>(
            kStackBase + registers.stack_pointer));
        set_register_nz(registers.x);
        checkpoint(0xF664U, 0xF665U, 12U);
        if (unsupported) {
            return std::nullopt;
        }
        absorb_return(
            replacement,
            work_ram,
            registers.stack_pointer,
            {0x00U, 0xF665U},
            cycle,
            registers);
        return replacement;
    }

    if (entry.routine == kDecoderCall) {
        push(0xF660U, registers.x);  // PHX
        checkpoint(0xF660U, 0xF661U, 9U);
        const auto pushed = std::uint16_t{0xF663U};
        push(0xF661U, static_cast<std::uint8_t>(pushed >> 8U));
        push(0xF661U, static_cast<std::uint8_t>(pushed));
        checkpoint(0xF661U, 0xE41FU, 21U);
        if (unsupported) {
            return std::nullopt;
        }
        replacement.return_instruction = {
            registers.mpr[0xE41FU >> 13U], 0xE41FU};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    // The zero record form enters the alternate F666 decoder setup. It was not
    // present in the deterministic corpus, so preserve the original code for
    // that branch instead of speculating about its longer record layout.
    registers.accumulator = read_ram(static_cast<std::uint16_t>(
        0x3080U + registers.x));
    set_register_nz(registers.accumulator);
    checkpoint(0xF605U, 0xF608U, 15U);
    if ((registers.status & kNegative) == 0U) {
        checkpoint(0xF608U, 0xF60AU, 6U);
        if (unsupported) {
            return std::nullopt;
        }
        absorb_return(
            replacement, work_ram, entry.registers.stack_pointer,
            {0x00U, 0xF60AU}, cycle, registers);
        return replacement;
    }
    checkpoint(0xF608U, 0xF60BU, 12U);

    registers.accumulator = read_ram(static_cast<std::uint16_t>(
        0x3800U + registers.x));
    set_register_nz(registers.accumulator);
    checkpoint(0xF60BU, 0xF60EU, 15U);
    registers.accumulator = static_cast<std::uint8_t>(
        registers.accumulator & 0xBFU);
    set_register_nz(registers.accumulator);
    checkpoint(0xF60EU, 0xF610U, 6U);
    write_ram(
        0xF610U, static_cast<std::uint16_t>(0x3800U + registers.x),
        registers.accumulator);
    checkpoint(0xF610U, 0xF613U, 15U);
    registers.y = read_ram(static_cast<std::uint16_t>(0x3900U + registers.x));
    set_register_nz(registers.y);
    checkpoint(0xF613U, 0xF616U, 15U);
    registers.accumulator = mapped_read(static_cast<std::uint16_t>(
        0xF6A6U + registers.y));
    set_register_nz(registers.accumulator);
    checkpoint(0xF616U, 0xF619U, 15U);
    write_ram(0xF619U, zp(0x35U), registers.accumulator);
    checkpoint(0xF619U, 0xF61BU, 12U);
    registers.accumulator = mapped_read(static_cast<std::uint16_t>(
        0xF6D0U + registers.y));
    set_register_nz(registers.accumulator);
    checkpoint(0xF61BU, 0xF61EU, 15U);
    write_ram(0xF61EU, zp(0x36U), registers.accumulator);
    checkpoint(0xF61EU, 0xF620U, 12U);
    registers.y = 0U;
    set_register_nz(registers.y);
    checkpoint(0xF620U, 0xF621U, 6U);
    const auto source = [&]() {
        const auto pointer = static_cast<std::uint16_t>(
            read_ram(zp(0x35U))
            | (static_cast<std::uint16_t>(read_ram(zp(0x36U))) << 8U));
        return mapped_read(static_cast<std::uint16_t>(pointer + registers.y));
    };
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF621U, 0xF623U, 21U);
    if (registers.accumulator == 0U) {
        return std::nullopt;
    }
    checkpoint(0xF623U, 0xF628U, 12U);
    write_ram(0xF628U, zp(0x26U), registers.accumulator);
    checkpoint(0xF628U, 0xF62AU, 12U);
    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    set_register_nz(registers.y);
    checkpoint(0xF62AU, 0xF62BU, 6U);
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
    checkpoint(0xF62BU, 0xF62CU, 6U);
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF62CU, 0xF62EU, 21U);
    {
        const auto left = registers.accumulator;
        const auto right = read_ram(zp(0x4FU));
        const auto sum = static_cast<std::uint16_t>(left) + right;
        registers.accumulator = static_cast<std::uint8_t>(sum);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFFU) registers.status |= kCarry;
        if (((left ^ registers.accumulator)
             & (right ^ registers.accumulator) & kNegative) != 0U) {
            registers.status |= kOverflow;
        }
        set_register_nz(registers.accumulator);
    }
    checkpoint(0xF62EU, 0xF630U, 12U);
    write_ram(0xF630U, zp(0x2DU), registers.accumulator);
    checkpoint(0xF630U, 0xF632U, 12U);
    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    set_register_nz(registers.y);
    checkpoint(0xF632U, 0xF633U, 6U);
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF633U, 0xF635U, 21U);
    {
        const auto left = registers.accumulator;
        const auto right = read_ram(zp(0x50U));
        const auto sum = static_cast<std::uint16_t>(left) + right
            + static_cast<std::uint16_t>(registers.status & kCarry);
        registers.accumulator = static_cast<std::uint8_t>(sum);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFFU) registers.status |= kCarry;
        if (((left ^ registers.accumulator)
             & (right ^ registers.accumulator) & kNegative) != 0U) {
            registers.status |= kOverflow;
        }
        set_register_nz(registers.accumulator);
    }
    checkpoint(0xF635U, 0xF637U, 12U);
    write_ram(0xF637U, zp(0x2EU), registers.accumulator);
    checkpoint(0xF637U, 0xF639U, 12U);
    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    set_register_nz(registers.y);
    checkpoint(0xF639U, 0xF63AU, 6U);
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF63AU, 0xF63CU, 21U);
    write_ram(0xF63CU, zp(0x2BU), registers.accumulator);
    checkpoint(0xF63CU, 0xF63EU, 12U);
    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    set_register_nz(registers.y);
    checkpoint(0xF63EU, 0xF63FU, 6U);
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF63FU, 0xF641U, 21U);
    write_ram(0xF641U, zp(0x2CU), registers.accumulator);
    checkpoint(0xF641U, 0xF643U, 12U);
    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    set_register_nz(registers.y);
    checkpoint(0xF643U, 0xF644U, 6U);
    registers.accumulator = read_ram(zp(0x26U));
    set_register_nz(registers.accumulator);
    checkpoint(0xF644U, 0xF646U, 12U);
    const auto difference = static_cast<std::uint16_t>(
        registers.accumulator - 0x02U);
    registers.status = set_nz(
        static_cast<std::uint8_t>(registers.status & ~kCarry),
        static_cast<std::uint8_t>(difference));
    if ((difference & 0x100U) == 0U) registers.status |= kCarry;
    checkpoint(0xF646U, 0xF648U, 6U);
    if (registers.accumulator == 0x02U) {
        checkpoint(0xF648U, 0xF654U, 12U);
    } else {
        checkpoint(0xF648U, 0xF64AU, 6U);
        registers.accumulator = read_ram(static_cast<std::uint16_t>(
            0x3980U + registers.x));
        set_register_nz(registers.accumulator);
        checkpoint(0xF64AU, 0xF64DU, 15U);
        const auto before_shift = registers.accumulator;
        registers.accumulator = static_cast<std::uint8_t>(before_shift << 1U);
        registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if ((before_shift & kNegative) != 0U) registers.status |= kCarry;
        set_register_nz(registers.accumulator);
        checkpoint(0xF64DU, 0xF64EU, 6U);
        write_ram(0xF64EU, zp(0x27U), registers.accumulator);
        checkpoint(0xF64EU, 0xF650U, 12U);
        registers.accumulator = registers.y;
        set_register_nz(registers.accumulator);
        checkpoint(0xF650U, 0xF651U, 6U);
        const auto left = registers.accumulator;
        const auto right = read_ram(zp(0x27U));
        const auto sum = static_cast<std::uint16_t>(left) + right
            + static_cast<std::uint16_t>(registers.status & kCarry);
        registers.accumulator = static_cast<std::uint8_t>(sum);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (sum > 0xFFU) registers.status |= kCarry;
        if (((left ^ registers.accumulator)
             & (right ^ registers.accumulator) & kNegative) != 0U) {
            registers.status |= kOverflow;
        }
        set_register_nz(registers.accumulator);
        checkpoint(0xF651U, 0xF653U, 12U);
        registers.y = registers.accumulator;
        set_register_nz(registers.y);
        checkpoint(0xF653U, 0xF654U, 6U);
    }
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF654U, 0xF656U, 21U);
    write_ram(0xF656U, zp(0x2FU), registers.accumulator);
    checkpoint(0xF656U, 0xF658U, 12U);
    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    set_register_nz(registers.y);
    checkpoint(0xF658U, 0xF659U, 6U);
    registers.accumulator = source();
    set_register_nz(registers.accumulator);
    checkpoint(0xF659U, 0xF65BU, 21U);
    write_ram(0xF65BU, zp(0x30U), registers.accumulator);
    checkpoint(0xF65BU, 0xF65DU, 12U);
    const auto pushed = std::uint16_t{0xF65FU};
    push(0xF65DU, static_cast<std::uint8_t>(pushed >> 8U));
    push(0xF65DU, static_cast<std::uint8_t>(pushed));
    checkpoint(0xF65DU, 0xE1FDU, 21U);
    if (unsupported || replacement.checkpoints.size() > 2048U
        || replacement.work_ram_writes.size() > 512U) {
        return std::nullopt;
    }
    replacement.return_instruction = {
        registers.mpr[0xE1FDU >> 13U], 0xE1FDU};
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

std::optional<RoutineReplacement> replace_resident_pattern_loader(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // The resident pattern selector is intentionally split at both children:
    // 00:E1FD maps the selected pack and 00:E41F performs its bounded VDC
    // stream decode.  Keeping those calls visible preserves their existing
    // interrupt horizons and avoids making the 1,008-word upload atomic.
    constexpr RoutineKey kHead{0x00U, 0xF768U};
    constexpr RoutineKey kDecoderCall{0x00U, 0xF778U};
    constexpr RoutineKey kReturnTail{0x00U, 0xF781U};
    if ((entry.routine != kHead && entry.routine != kDecoderCall
         && entry.routine != kReturnTail)
        || work_ram.size() < 0x2000U || rom.size() != 0x60000U
        || entry.registers.mpr[0U] != 0xFFU
        || entry.registers.mpr[1U] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    RoutineReplacement replacement;
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool unsupported = false;
    std::size_t committed_writes = 0U;

    const auto set_nz = [&registers](const std::uint8_t value) {
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kNegative | kZero)));
        if (value == 0U) {
            registers.status = static_cast<std::uint8_t>(registers.status | kZero);
        } else {
            registers.status = static_cast<std::uint8_t>(
                registers.status | (value & kNegative));
        }
    };
    const auto read_ram = [&work_ram, &unsupported](const std::uint16_t address) {
        if (address < kWorkRamBase || address >= 0x4000U) {
            unsupported = true;
            return std::uint8_t{0U};
        }
        return std::to_integer<std::uint8_t>(work_ram[address - kWorkRamBase]);
    };
    const auto read_mapped = [&registers, rom, &unsupported](const std::uint16_t address) {
        const auto bank = registers.mpr[address >> 13U];
        if (bank >= 0x80U) {
            unsupported = true;
            return std::uint8_t{0U};
        }
        const auto rom_bank = bank < 0x40U
            ? static_cast<std::size_t>(bank & 0x1FU)
            : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
        const auto offset = rom_bank * 0x2000U
            + static_cast<std::size_t>(address & 0x1FFFU);
        if (offset >= rom.size()) {
            unsupported = true;
            return std::uint8_t{0U};
        }
        return std::to_integer<std::uint8_t>(rom[offset]);
    };
    const auto write_ram = [&replacement, &unsupported](
                               const std::uint16_t instruction,
                               const std::uint16_t address,
                               const std::uint8_t value) {
        if (address < kWorkRamBase || address >= 0x4000U) {
            unsupported = true;
            return;
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, instruction},
            .logical_address = address,
            .value = value,
            .master_cycle_offset = 0U,
        });
    };
    const auto commit = [&replacement, &committed_writes](const std::uint32_t at_cycle) {
        while (committed_writes < replacement.work_ram_writes.size()) {
            replacement.work_ram_writes[committed_writes].master_cycle_offset = at_cycle;
            ++committed_writes;
        }
    };
    const auto checkpoint = [&replacement, &registers, &cycle, &commit](
                                const std::uint16_t instruction,
                                const std::uint16_t resume,
                                const std::uint32_t master_cycles) {
        cycle += master_cycles;
        commit(cycle);
        registers.program_counter = resume;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {0x00U, instruction},
            .registers = registers,
        });
    };
    const auto push = [&registers, &write_ram](
                          const std::uint16_t instruction,
                          const std::uint8_t value) {
        write_ram(
            instruction,
            static_cast<std::uint16_t>(kStackBase + registers.stack_pointer),
            value);
        registers.stack_pointer = static_cast<std::uint8_t>(
            registers.stack_pointer - 1U);
    };

    if (entry.routine == kReturnTail) {
        registers.stack_pointer = static_cast<std::uint8_t>(
            registers.stack_pointer + 1U);
        registers.x = read_ram(static_cast<std::uint16_t>(
            kStackBase + registers.stack_pointer));
        set_nz(registers.x);
        checkpoint(0xF781U, 0xF782U, 12U);  // PLX
        if (unsupported) {
            return std::nullopt;
        }
        absorb_return(
            replacement,
            work_ram,
            registers.stack_pointer,
            {0x00U, 0xF782U},
            cycle,
            registers);
        return replacement;
    }

    if (entry.routine == kDecoderCall) {
        write_ram(0xF778U, static_cast<std::uint16_t>(kWorkRamBase + 0x2DU), 0U);
        checkpoint(0xF778U, 0xF77AU, 12U);  // STZ $2D
        registers.accumulator = 0x7CU;
        set_nz(registers.accumulator);
        checkpoint(0xF77AU, 0xF77CU, 6U);  // LDA #$7C
        write_ram(0xF77CU, static_cast<std::uint16_t>(kWorkRamBase + 0x2EU), registers.accumulator);
        checkpoint(0xF77CU, 0xF77EU, 12U);  // STA $2E
        const auto pushed = std::uint16_t{0xF780U};
        push(0xF77EU, static_cast<std::uint8_t>(pushed >> 8U));
        push(0xF77EU, static_cast<std::uint8_t>(pushed));
        commit(cycle);  // JSR writes its return address before advancing time.
        checkpoint(0xF77EU, 0xE41FU, 21U);  // JSR $E41F
        if (unsupported) {
            return std::nullopt;
        }
        replacement.return_instruction = {
            registers.mpr[0xE41FU >> 13U], 0xE41FU};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    push(0xF768U, registers.x);  // PHX: F781 restores this actor slot.
    checkpoint(0xF768U, 0xF769U, 9U);
    // ASL A / TAY selects the two-byte descriptor for the requested pack.
    // These are separate one-byte instructions, not an immediate load.
    const auto selector = registers.accumulator;
    registers.status = static_cast<std::uint8_t>(
        (registers.status & ~kCarry) | ((selector & 0x80U) != 0U ? kCarry : 0U));
    registers.accumulator = static_cast<std::uint8_t>(selector << 1U);
    set_nz(registers.accumulator);
    checkpoint(0xF769U, 0xF76AU, 6U);
    registers.y = registers.accumulator;
    set_nz(registers.y);
    checkpoint(0xF76AU, 0xF76BU, 6U);
    const auto selector_address = static_cast<std::uint16_t>(0xF783U + registers.y);
    registers.accumulator = read_mapped(selector_address);
    set_nz(registers.accumulator);
    checkpoint(0xF76BU, 0xF76EU, 15U);
    write_ram(0xF76EU, static_cast<std::uint16_t>(kWorkRamBase + 0x2FU), registers.accumulator);
    checkpoint(0xF76EU, 0xF770U, 12U);
    registers.accumulator = read_mapped(static_cast<std::uint16_t>(selector_address + 1U));
    set_nz(registers.accumulator);
    checkpoint(0xF770U, 0xF773U, 15U);
    write_ram(0xF773U, static_cast<std::uint16_t>(kWorkRamBase + 0x30U), registers.accumulator);
    checkpoint(0xF773U, 0xF775U, 12U);
    const auto pushed = std::uint16_t{0xF777U};
    push(0xF775U, static_cast<std::uint8_t>(pushed >> 8U));
    push(0xF775U, static_cast<std::uint8_t>(pushed));
    commit(cycle);
    checkpoint(0xF775U, 0xE1FDU, 21U);  // JSR $E1FD
    if (unsupported) {
        return std::nullopt;
    }
    replacement.return_instruction = {
        registers.mpr[0xE1FDU >> 13U], 0xE1FDU};
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

} // namespace devils_crush::app::detail
