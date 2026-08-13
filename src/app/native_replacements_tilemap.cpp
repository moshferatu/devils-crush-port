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

// The background tilemap upload, 00:F261-F4F1. This is the second half of the
// graphics driver's port work and the last coherent video row in the inventory:
// where 00:E41F streams a prepared word list into VRAM, this decides which row
// or column of the scrolling table has just come into view and pushes it.
//
// Two routines, not one:
//
//   00:F3E7  the row writer. $2D/$2E is a VRAM address, $2F/$30 an MPR-mapped
//            tile-index source and $2B/$2C a work-RAM attribute nibble array.
//            It walks Y from $00 to $3E in twos, writing 32 words through the
//            VDC data ports: the low byte straight from the source, the high
//            byte the next source byte with an attribute nibble folded in. It
//            returns early when $2F/$30 still equals the $A4/$A5 it uploaded
//            last time, which never happens on the census route.
//
//   00:F261  the scroll driver. It subtracts the camera position from the two
//            scroll limits at $3300/$3380, decides whether the camera has
//            crossed a tile boundary downward or upward, resolves the tile
//            source through the bank helper at 00:E1FD, computes the VRAM and
//            attribute addresses, and calls the writer once or twice. The tails
//            at 00:F43D and 00:F48D then fold the movement back into the camera
//            at $0A-$0C and $A8-$AA.
//
// Nothing in the region reads a device register, so the whole 5.7M row is
// phase-1 work. Every load is immediate, zero page, absolute, or through one of
// the three pointers above, and each pointer is resolved against the live MPRs
// rather than assumed -- 00:F3E7 reaches its tile source through MPR4 and MPR5,
// which 00:E20D pointed at the scene's bank pair one instruction earlier, while
// its attribute array stays in work RAM through segment 1.
//
// Ten entry points, because a transcript stops in front of a call and this
// family makes six of them. Each entry is the head of a routine or the address
// the core arrives at after an instruction it executes itself:
//
//   00:F261  head            stops at 00:F28F, 00:F2A8 or 00:F35C
//   00:F2AB  after JSR $E1FD stops at 00:F2FE
//   00:F301  after JSR $F3E7 stops at 00:F32D, or runs the tail to 00:F4F2
//   00:F330  after JSR $F3E7 runs the tail to 00:F4F2
//   00:F35F  after JSR $E1FD stops at 00:F3B2
//   00:F3B5  after JSR $F3E7 stops at 00:F3E1, or runs the tail to 00:F4F2
//   00:F3E4  after JSR $F3E7 runs the tail to 00:F4F2
//   00:F3E7  the row writer   stops at 00:F3F3 or 00:F423
//   00:F426 / 00:F42A  the two arms of the BBR at 00:F423, which the core
//            executes -- see is_stop_instruction below. Each runs the rest of
//            its loop iteration and stops at the next 00:F423, or at 00:F43C.
//
// Two of those calls -- 00:F2A8 and 00:F35C, both JSR $E1FD -- land on the bank
// helper at 00:E20D and 00:E200, which have been native since the call-wrapper
// slice. Stopping in front of them re-enters that module rather than
// transcribing the same eleven instructions again.
//
// The walk ends at 00:F4F2 rather than at an RTS: 00:F261 falls through into the
// camera-visibility sweep, which has been native since its own slice. The
// transcript hands 00:F4F2 back and the sweep's replacement picks it up there.
//
// This family does not need to chunk. One 00:F3E7 call is 168 writes and about
// 660 checkpoints against a channel of 512 and 2,048, and every 00:F261
// transcript is under sixty instructions, so each of the eight fits whole.
//
// Its zero page is shared -- $2B-$30 is the game's general-purpose pointer
// area, and $A2-$AA the camera block -- so the transcripts are only safe if
// nothing writes them underneath. Measured on an assembly write trace against
// exact call windows from a 00:F261 observer rather than inferred from the
// writes themselves: across all 2,649 calls, 92,978 writes inside the windows
// belong to the routine, and the only others are 8,118 interrupt-frame pushes
// below the live stack pointer from the timer driver and the music sequencer,
// plus four stores to $20 by the VDC handler's bank wrapper -- an address this
// region writes only inside the 00:E1FD it stops in front of, and never reads.
// No interrupt-context writer touches $0A-$0C, $26-$27, $2B-$30, $35-$36,
// $A2-$AA, the attribute array, or the four scroll-limit inputs.
//
// The stores at $0000-$0003 are the VDC register file. The core prices the
// cycle the device steals, so they are transcribed at eighteen master cycles
// and sample the interrupt two CPU cycles early; the further wait state the VDC
// imposes while it reaches VRAM lands on the real timestamp inside the declared
// offset, exactly as it does in the assembly.
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

// Master cycles per instruction. Every one is three times the core's ADDCYC(n)
// plus the LastCycle it always charges.
constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kZeroPageReadModifyWrite = 18U;
constexpr std::uint32_t kIndirectIndexed = 21U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
constexpr std::uint32_t kJump = 12U;
constexpr std::uint32_t kPushY = 9U;
constexpr std::uint32_t kPullY = 12U;
// A store to a VDC port is an ordinary absolute store plus the cycle the device
// steals, which moves its interrupt sample two CPU cycles back from the end.
constexpr std::uint32_t kPortStore = 18U;
constexpr std::uint32_t kPortSampleBack = 6U;

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
    return logical_pc == 0xF261U || logical_pc == 0xF2ABU
        || logical_pc == 0xF301U || logical_pc == 0xF330U
        || logical_pc == 0xF35FU || logical_pc == 0xF3B5U
        || logical_pc == 0xF3E4U || logical_pc == 0xF3E7U
        || logical_pc == 0xF426U || logical_pc == 0xF42AU;
}

// Where each walk stops. Three are RTS instructions, five are the calls this
// family makes -- which the core executes, re-entering whichever replacement
// owns the target -- and 00:F4F2 is the camera sweep 00:F261 falls into.
//
// 00:F423 is the tenth, and it is there for a different reason. Every other
// instruction in the region charges its cycles in one ADDCYC and one
// LastCycle, which is exactly the shape a checkpoint expresses: advance to the
// sample, sample, advance to the end. BBR is assembled from two of them --
// LD_ZP reads $27 and samples, then JR branches and samples again -- so the
// machine crosses this instruction in four clock steps where a checkpoint
// crosses it in two. The interrupt outcome is the same either way, but the
// steps are where the core tests next_event, and a coarser step runs the video
// EventHandler at a later timestamp. That shifted a VDC event, which moved a
// timer interrupt one instruction early four times in 3,600 frames -- twice
// inside this walk and twice in assembly downstream of it. Handing 00:F423
// back costs 365,664 master cycles of the region and is the only faithful
// expression available: an extra checkpoint would make the boundary an extra
// interrupt opportunity the machine does not have.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xF28FU || logical_pc == 0xF2A8U
        || logical_pc == 0xF2FEU || logical_pc == 0xF32DU
        || logical_pc == 0xF35CU || logical_pc == 0xF3B2U
        || logical_pc == 0xF3E1U || logical_pc == 0xF3F3U
        || logical_pc == 0xF423U || logical_pc == 0xF43CU
        || logical_pc == 0xF4F2U;
}

// All three of those RTS instructions are returns, and each says how much stack
// the walk that reaches it has given back, so the frame can be read from the
// pointer at the return rather than the one at the entry.
//
//   00:F28F  from 00:F261, which touches no stack
//   00:F3F3  from 00:F3E7, which touches no stack
//   00:F43C  from 00:F426 and 00:F42A, which pull one byte back
//
// The last is the row-upload loop and the only one that is not obvious. The
// loop pushes Y at 00:F41A and pulls it back at 00:F42E, which is balanced
// across an iteration but not across a transcript: 00:F423 sits between the two
// and is handed to the core for the reason above, so a transcript entered at
// 00:F42A begins with the pull and ends either at the next push -- back where
// it started -- or, when Y reaches $40, at 00:F43C one byte higher. Both of the
// entries below the branch reach it the same way, so the count is a property of
// the return and not of which arm was taken.
[[nodiscard]] constexpr std::optional<std::uint8_t> absorbable_return_pops(
    const std::uint16_t logical_pc)
{
    switch (logical_pc) {
    case 0xF28FU:
    case 0xF3F3U:
        return static_cast<std::uint8_t>(0U);
    case 0xF43CU:
        return static_cast<std::uint8_t>(1U);
    default:
        return std::nullopt;
    }
}

} // namespace

std::optional<RoutineReplacement> replace_tilemap_upload(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is something this
    // region is written to run under; T would redirect the accumulator through
    // zero page for every AND, EOR, ORA and ADC below.
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

    // Reads the byte a logical address currently maps to. The row writer's tile
    // source lives in whichever bank pair 00:E20D put behind MPR4 and MPR5, and
    // its attribute array in work RAM through segment 1, so both are resolved
    // from the registers rather than assumed.
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
    // which is where the core's LastCycle sits.
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
    // $0003. Nothing here models the device; the transcript only has to emit
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
    // A zero-page pointer pair indexed by Y. Both halves come from the
    // snapshot, so the address is whatever the driver last stored there.
    const auto indirect_y =
        [&read_ram, &registers, &zp](const std::uint8_t pointer) {
            const auto base = static_cast<std::uint16_t>(
                read_ram(zp(pointer))
                | static_cast<std::uint16_t>(
                    read_ram(zp(static_cast<std::uint8_t>(pointer + 1U)))
                    << 8U));
            return static_cast<std::uint16_t>(base + registers.y);
        };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
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
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
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

    // SBC in binary mode. The carry runs the other way: it is a borrow going
    // in, and set on the way out when the subtraction did not borrow.
    const auto subtract_with_carry = [&registers](const std::uint8_t operand) {
        const auto difference = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            - static_cast<std::uint16_t>(operand)
            - static_cast<std::uint16_t>((registers.status & kCarry) ^ kCarry));
        const auto result = static_cast<std::uint8_t>(difference);
        const bool signed_overflow =
            ((registers.accumulator ^ result)
             & (registers.accumulator ^ operand) & kNegative) != 0U;
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if ((difference & 0x100U) == 0U) {
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

    // CMP and CPY set the same three flags a subtraction would but keep the
    // accumulator, and leave V alone.
    const auto compare =
        [](const std::uint8_t status,
           const std::uint8_t reg,
           const std::uint8_t operand) {
            const auto difference = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(reg)
                - static_cast<std::uint16_t>(operand));
            auto result =
                set_nz(status, static_cast<std::uint8_t>(difference));
            result = static_cast<std::uint8_t>(
                result & static_cast<std::uint8_t>(~kCarry));
            if ((difference & 0x100U) == 0U) {
                result = static_cast<std::uint8_t>(result | kCarry);
            }
            return result;
        };

    // ASL A, and ROL on a zero-page byte. The driver builds its VRAM addresses
    // by shifting a byte left through the carry into a high byte.
    const auto shift_accumulator_left = [&registers]() {
        const auto value = registers.accumulator;
        const auto result = static_cast<std::uint8_t>(value << 1U);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if ((value & kNegative) != 0U) {
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
        }
        registers.accumulator = result;
        registers.status = set_nz(registers.status, result);
    };
    const auto rotate_memory_left =
        [&registers, &read_ram, &store, &zp](
            const std::uint16_t instruction_pc,
            const std::uint8_t address) {
            const auto value = read_ram(zp(address));
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(value << 1U)
                | (registers.status & kCarry));
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if ((value & kNegative) != 0U) {
                registers.status =
                    static_cast<std::uint8_t>(registers.status | kCarry);
            }
            registers.status = set_nz(registers.status, result);
            store(instruction_pc, zp(address), result);
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        // The core's channels are 512 writes and 2,048 checkpoints. Nothing
        // here comes close, so exceeding either means the walk has gone
        // somewhere it was not written for and the assembly should run.
        if (overflow
            || replacement.checkpoints.size() > 1800U
            || replacement.work_ram_writes.size() > 480U) {
            return std::nullopt;
        }

        if (is_stop_instruction(pc)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;

            // Nothing composes this walk, so it absorbs its own return. The
            // pointer tested is the one the walk arrived at the RTS with, and
            // it has to be exactly the entry's plus what that stop's walk
            // pulled back.
            if (const auto pops = absorbable_return_pops(pc);
                pops.has_value()
                && registers.stack_pointer
                    == static_cast<std::uint8_t>(
                        entry.registers.stack_pointer + *pops)
                && (!replacement.checkpoints.empty()
                    || absorbed_return_fits_atomically(entry, cycle))) {
                absorb_return(
                    replacement,
                    work_ram,
                    registers.stack_pointer,
                    {kBank, pc},
                    cycle,
                    registers);
            }
            return replacement;
        }

        switch (pc) {
        // ---- 00:F261  which way the camera crossed a tile boundary ---------
        // $3300/$3380 is the scroll limit and $A9/$AA the camera position; the
        // difference decides whether a row has come into view below the camera
        // ($2F/$30 >= $00C0), above it (< $0040), or neither.
        case 0xF261U:  // LDX #$00
            registers.x = 0x00U;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0xF261U, 0xF263U, kImmediate);
            pc = 0xF263U;
            break;

        case 0xF263U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF263U, 0xF264U, kImplied);
            pc = 0xF264U;
            break;

        case 0xF264U:  // LDA $3300,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3300U + registers.x)));
            append_checkpoint(0xF264U, 0xF267U, kAbsolute);
            pc = 0xF267U;
            break;

        case 0xF267U:  // SBC $A9
            subtract_with_carry(read_ram(zp(0xA9U)));
            append_checkpoint(0xF267U, 0xF269U, kZeroPage);
            pc = 0xF269U;
            break;

        case 0xF269U:  // STA $2F
            store(0xF269U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF269U, 0xF26BU, kZeroPage);
            pc = 0xF26BU;
            break;

        case 0xF26BU:  // LDA $3380,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3380U + registers.x)));
            append_checkpoint(0xF26BU, 0xF26EU, kAbsolute);
            pc = 0xF26EU;
            break;

        case 0xF26EU:  // SBC $AA
            subtract_with_carry(read_ram(zp(0xAAU)));
            append_checkpoint(0xF26EU, 0xF270U, kZeroPage);
            pc = 0xF270U;
            break;

        case 0xF270U:  // STA $30
            store(0xF270U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF270U, 0xF272U, kZeroPage);
            pc = 0xF272U;
            break;

        case 0xF272U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF272U, 0xF273U, kImplied);
            pc = 0xF273U;
            break;

        // The two comparisons below discard their difference and keep only the
        // sign, which is why each loads the byte again rather than carrying it.
        case 0xF273U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF273U, 0xF275U, kZeroPage);
            pc = 0xF275U;
            break;

        case 0xF275U:  // SBC #$C0
            subtract_with_carry(0xC0U);
            append_checkpoint(0xF275U, 0xF277U, kImmediate);
            pc = 0xF277U;
            break;

        case 0xF277U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF277U, 0xF279U, kZeroPage);
            pc = 0xF279U;
            break;

        case 0xF279U:  // SBC #$00
            subtract_with_carry(0x00U);
            append_checkpoint(0xF279U, 0xF27BU, kImmediate);
            pc = 0xF27BU;
            break;

        case 0xF27BU:  // BPL $F290
            if ((registers.status & kNegative) == 0U) {
                append_checkpoint(0xF27BU, 0xF290U, kBranchTaken);
                pc = 0xF290U;
                break;
            }
            append_checkpoint(0xF27BU, 0xF27DU, kBranchNotTaken);
            pc = 0xF27DU;
            break;

        case 0xF27DU:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF27DU, 0xF27EU, kImplied);
            pc = 0xF27EU;
            break;

        case 0xF27EU:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF27EU, 0xF280U, kZeroPage);
            pc = 0xF280U;
            break;

        case 0xF280U:  // SBC #$40
            subtract_with_carry(0x40U);
            append_checkpoint(0xF280U, 0xF282U, kImmediate);
            pc = 0xF282U;
            break;

        case 0xF282U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF282U, 0xF284U, kZeroPage);
            pc = 0xF284U;
            break;

        case 0xF284U:  // SBC #$00
            subtract_with_carry(0x00U);
            append_checkpoint(0xF284U, 0xF286U, kImmediate);
            pc = 0xF286U;
            break;

        case 0xF286U:  // BPL $F28B
            if ((registers.status & kNegative) == 0U) {
                append_checkpoint(0xF286U, 0xF28BU, kBranchTaken);
                pc = 0xF28BU;
                break;
            }
            append_checkpoint(0xF286U, 0xF288U, kBranchNotTaken);
            pc = 0xF288U;
            break;

        case 0xF288U:  // JMP $F333
            append_checkpoint(0xF288U, 0xF333U, kJump);
            pc = 0xF333U;
            break;

        // The camera has not crossed a boundary: clear the movement and return.
        case 0xF28BU:  // STZ $A6
            store(0xF28BU, zp(0xA6U), 0x00U);
            append_checkpoint(0xF28BU, 0xF28DU, kZeroPage);
            pc = 0xF28DU;
            break;

        case 0xF28DU:  // STZ $A7
            store(0xF28DU, zp(0xA7U), 0x00U);
            append_checkpoint(0xF28DU, 0xF28FU, kZeroPage);
            pc = 0xF28FU;
            break;

        // ---- 00:F290  a row came into view below the camera ----------------
        case 0xF290U:  // LDA $3580,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3580U + registers.x)));
            append_checkpoint(0xF290U, 0xF293U, kAbsolute);
            pc = 0xF293U;
            break;

        case 0xF293U:  // BPL $F298
            if ((registers.status & kNegative) == 0U) {
                append_checkpoint(0xF293U, 0xF298U, kBranchTaken);
                pc = 0xF298U;
                break;
            }
            append_checkpoint(0xF293U, 0xF295U, kBranchNotTaken);
            pc = 0xF295U;
            break;

        case 0xF295U:  // JMP $F28B
            append_checkpoint(0xF295U, 0xF28BU, kJump);
            pc = 0xF28BU;
            break;

        case 0xF298U:  // LDA $3500,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3500U + registers.x)));
            append_checkpoint(0xF298U, 0xF29BU, kAbsolute);
            pc = 0xF29BU;
            break;

        case 0xF29BU:  // STA $A6
            store(0xF29BU, zp(0xA6U), registers.accumulator);
            append_checkpoint(0xF29BU, 0xF29DU, kZeroPage);
            pc = 0xF29DU;
            break;

        case 0xF29DU:  // LDA $3580,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3580U + registers.x)));
            append_checkpoint(0xF29DU, 0xF2A0U, kAbsolute);
            pc = 0xF2A0U;
            break;

        case 0xF2A0U:  // STA $A7
            store(0xF2A0U, zp(0xA7U), registers.accumulator);
            append_checkpoint(0xF2A0U, 0xF2A2U, kZeroPage);
            pc = 0xF2A2U;
            break;

        // $0014 is the reference the bank helper resolves into a real pointer.
        case 0xF2A2U:  // STZ $2F
            store(0xF2A2U, zp(0x2FU), 0x00U);
            append_checkpoint(0xF2A2U, 0xF2A4U, kZeroPage);
            pc = 0xF2A4U;
            break;

        case 0xF2A4U:  // LDA #$14
            load_accumulator(0x14U);
            append_checkpoint(0xF2A4U, 0xF2A6U, kImmediate);
            pc = 0xF2A6U;
            break;

        case 0xF2A6U:  // STA $30
            store(0xF2A6U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF2A6U, 0xF2A8U, kZeroPage);
            pc = 0xF2A8U;
            break;

        // ---- 00:F2AB  the downward VRAM and attribute addresses ------------
        case 0xF2ABU:  // LDA $A9
            load_accumulator(read_ram(zp(0xA9U)));
            append_checkpoint(0xF2ABU, 0xF2ADU, kZeroPage);
            pc = 0xF2ADU;
            break;

        case 0xF2ADU:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF2ADU, 0xF2AFU, kImmediate);
            pc = 0xF2AFU;
            break;

        case 0xF2AFU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF2AFU, 0xF2B0U, kImplied);
            pc = 0xF2B0U;
            break;

        case 0xF2B0U:  // ADC #$F0
            add_with_carry(0xF0U);
            append_checkpoint(0xF2B0U, 0xF2B2U, kImmediate);
            pc = 0xF2B2U;
            break;

        case 0xF2B2U:  // STA $2D
            store(0xF2B2U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF2B2U, 0xF2B4U, kZeroPage);
            pc = 0xF2B4U;
            break;

        case 0xF2B4U:  // STA $2B
            store(0xF2B4U, zp(0x2BU), registers.accumulator);
            append_checkpoint(0xF2B4U, 0xF2B6U, kZeroPage);
            pc = 0xF2B6U;
            break;

        case 0xF2B6U:  // LDA $AA
            load_accumulator(read_ram(zp(0xAAU)));
            append_checkpoint(0xF2B6U, 0xF2B8U, kZeroPage);
            pc = 0xF2B8U;
            break;

        case 0xF2B8U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xF2B8U, 0xF2BAU, kImmediate);
            pc = 0xF2BAU;
            break;

        case 0xF2BAU:  // STA $2E
            store(0xF2BAU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xF2BAU, 0xF2BCU, kZeroPage);
            pc = 0xF2BCU;
            break;

        case 0xF2BCU:  // STA $2C
            store(0xF2BCU, zp(0x2CU), registers.accumulator);
            append_checkpoint(0xF2BCU, 0xF2BEU, kZeroPage);
            pc = 0xF2BEU;
            break;

        // The tile row times eight, in three shifts through $2E, is its offset
        // into the source the helper just resolved.
        case 0xF2BEU:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF2BEU, 0xF2C0U, kZeroPage);
            pc = 0xF2C0U;
            break;

        case 0xF2C0U:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF2C0U, 0xF2C2U, kImmediate);
            pc = 0xF2C2U;
            break;

        case 0xF2C2U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2C2U, 0xF2C3U, kImplied);
            pc = 0xF2C3U;
            break;

        case 0xF2C3U:  // ROL $2E
            rotate_memory_left(0xF2C3U, 0x2EU);
            append_checkpoint(0xF2C3U, 0xF2C5U, kZeroPageReadModifyWrite);
            pc = 0xF2C5U;
            break;

        case 0xF2C5U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2C5U, 0xF2C6U, kImplied);
            pc = 0xF2C6U;
            break;

        case 0xF2C6U:  // ROL $2E
            rotate_memory_left(0xF2C6U, 0x2EU);
            append_checkpoint(0xF2C6U, 0xF2C8U, kZeroPageReadModifyWrite);
            pc = 0xF2C8U;
            break;

        case 0xF2C8U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2C8U, 0xF2C9U, kImplied);
            pc = 0xF2C9U;
            break;

        case 0xF2C9U:  // ROL $2E
            rotate_memory_left(0xF2C9U, 0x2EU);
            append_checkpoint(0xF2C9U, 0xF2CBU, kZeroPageReadModifyWrite);
            pc = 0xF2CBU;
            break;

        case 0xF2CBU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF2CBU, 0xF2CCU, kImplied);
            pc = 0xF2CCU;
            break;

        case 0xF2CCU:  // ADC $2F
            add_with_carry(read_ram(zp(0x2FU)));
            append_checkpoint(0xF2CCU, 0xF2CEU, kZeroPage);
            pc = 0xF2CEU;
            break;

        case 0xF2CEU:  // STA $2F
            store(0xF2CEU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF2CEU, 0xF2D0U, kZeroPage);
            pc = 0xF2D0U;
            break;

        case 0xF2D0U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF2D0U, 0xF2D2U, kZeroPage);
            pc = 0xF2D2U;
            break;

        case 0xF2D2U:  // ADC $2E
            add_with_carry(read_ram(zp(0x2EU)));
            append_checkpoint(0xF2D2U, 0xF2D4U, kZeroPage);
            pc = 0xF2D4U;
            break;

        case 0xF2D4U:  // STA $30
            store(0xF2D4U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF2D4U, 0xF2D6U, kZeroPage);
            pc = 0xF2D6U;
            break;

        // The attribute array is the same row halved, based at $2259.
        case 0xF2D6U:  // LDA $2B
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xF2D6U, 0xF2D8U, kZeroPage);
            pc = 0xF2D8U;
            break;

        case 0xF2D8U:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF2D8U, 0xF2DAU, kImmediate);
            pc = 0xF2DAU;
            break;

        case 0xF2DAU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2DAU, 0xF2DBU, kImplied);
            pc = 0xF2DBU;
            break;

        case 0xF2DBU:  // ROL $2C
            rotate_memory_left(0xF2DBU, 0x2CU);
            append_checkpoint(0xF2DBU, 0xF2DDU, kZeroPageReadModifyWrite);
            pc = 0xF2DDU;
            break;

        case 0xF2DDU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF2DDU, 0xF2DEU, kImplied);
            pc = 0xF2DEU;
            break;

        case 0xF2DEU:  // ADC #$59
            add_with_carry(0x59U);
            append_checkpoint(0xF2DEU, 0xF2E0U, kImmediate);
            pc = 0xF2E0U;
            break;

        case 0xF2E0U:  // STA $2B
            store(0xF2E0U, zp(0x2BU), registers.accumulator);
            append_checkpoint(0xF2E0U, 0xF2E2U, kZeroPage);
            pc = 0xF2E2U;
            break;

        case 0xF2E2U:  // LDA $2C
            load_accumulator(read_ram(zp(0x2CU)));
            append_checkpoint(0xF2E2U, 0xF2E4U, kZeroPage);
            pc = 0xF2E4U;
            break;

        case 0xF2E4U:  // ADC #$22
            add_with_carry(0x22U);
            append_checkpoint(0xF2E4U, 0xF2E6U, kImmediate);
            pc = 0xF2E6U;
            break;

        case 0xF2E6U:  // STA $2C
            store(0xF2E6U, zp(0x2CU), registers.accumulator);
            append_checkpoint(0xF2E6U, 0xF2E8U, kZeroPage);
            pc = 0xF2E8U;
            break;

        // $0B is the camera's tile row, and the same three shifts turn it into
        // the VRAM word address the writer will program into the VDC.
        case 0xF2E8U:  // LDA $0B
            load_accumulator(read_ram(zp(0x0BU)));
            append_checkpoint(0xF2E8U, 0xF2EAU, kZeroPage);
            pc = 0xF2EAU;
            break;

        case 0xF2EAU:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF2EAU, 0xF2ECU, kImmediate);
            pc = 0xF2ECU;
            break;

        case 0xF2ECU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF2ECU, 0xF2EDU, kImplied);
            pc = 0xF2EDU;
            break;

        case 0xF2EDU:  // ADC #$F0
            add_with_carry(0xF0U);
            append_checkpoint(0xF2EDU, 0xF2EFU, kImmediate);
            pc = 0xF2EFU;
            break;

        case 0xF2EFU:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF2EFU, 0xF2F1U, kImmediate);
            pc = 0xF2F1U;
            break;

        case 0xF2F1U:  // STZ $2E
            store(0xF2F1U, zp(0x2EU), 0x00U);
            append_checkpoint(0xF2F1U, 0xF2F3U, kZeroPage);
            pc = 0xF2F3U;
            break;

        case 0xF2F3U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2F3U, 0xF2F4U, kImplied);
            pc = 0xF2F4U;
            break;

        case 0xF2F4U:  // ROL $2E
            rotate_memory_left(0xF2F4U, 0x2EU);
            append_checkpoint(0xF2F4U, 0xF2F6U, kZeroPageReadModifyWrite);
            pc = 0xF2F6U;
            break;

        case 0xF2F6U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2F6U, 0xF2F7U, kImplied);
            pc = 0xF2F7U;
            break;

        case 0xF2F7U:  // ROL $2E
            rotate_memory_left(0xF2F7U, 0x2EU);
            append_checkpoint(0xF2F7U, 0xF2F9U, kZeroPageReadModifyWrite);
            pc = 0xF2F9U;
            break;

        case 0xF2F9U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF2F9U, 0xF2FAU, kImplied);
            pc = 0xF2FAU;
            break;

        case 0xF2FAU:  // ROL $2E
            rotate_memory_left(0xF2FAU, 0x2EU);
            append_checkpoint(0xF2FAU, 0xF2FCU, kZeroPageReadModifyWrite);
            pc = 0xF2FCU;
            break;

        case 0xF2FCU:  // STA $2D
            store(0xF2FCU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF2FCU, 0xF2FEU, kZeroPage);
            pc = 0xF2FEU;
            break;

        // ---- 00:F301  a second row, when the movement spanned two ----------
        case 0xF301U:  // LDA $A7
            load_accumulator(read_ram(zp(0xA7U)));
            append_checkpoint(0xF301U, 0xF303U, kZeroPage);
            pc = 0xF303U;
            break;

        case 0xF303U:  // CMP #$08
            registers.status =
                compare(registers.status, registers.accumulator, 0x08U);
            append_checkpoint(0xF303U, 0xF305U, kImmediate);
            pc = 0xF305U;
            break;

        case 0xF305U:  // BCC $F330
            if ((registers.status & kCarry) == 0U) {
                append_checkpoint(0xF305U, 0xF330U, kBranchTaken);
                pc = 0xF330U;
                break;
            }
            append_checkpoint(0xF305U, 0xF307U, kBranchNotTaken);
            pc = 0xF307U;
            break;

        // Advancing all three pointers by one row. CLA leaves the carry alone,
        // which is what carries the addition into each high byte.
        case 0xF307U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF307U, 0xF308U, kImplied);
            pc = 0xF308U;
            break;

        case 0xF308U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF308U, 0xF30AU, kZeroPage);
            pc = 0xF30AU;
            break;

        case 0xF30AU:  // ADC #$40
            add_with_carry(0x40U);
            append_checkpoint(0xF30AU, 0xF30CU, kImmediate);
            pc = 0xF30CU;
            break;

        case 0xF30CU:  // STA $2F
            store(0xF30CU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF30CU, 0xF30EU, kZeroPage);
            pc = 0xF30EU;
            break;

        case 0xF30EU:  // CLA -- clears A and no flag
            registers.accumulator = 0x00U;
            append_checkpoint(0xF30EU, 0xF30FU, kImplied);
            pc = 0xF30FU;
            break;

        case 0xF30FU:  // ADC $30
            add_with_carry(read_ram(zp(0x30U)));
            append_checkpoint(0xF30FU, 0xF311U, kZeroPage);
            pc = 0xF311U;
            break;

        case 0xF311U:  // STA $30
            store(0xF311U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF311U, 0xF313U, kZeroPage);
            pc = 0xF313U;
            break;

        case 0xF313U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF313U, 0xF314U, kImplied);
            pc = 0xF314U;
            break;

        case 0xF314U:  // LDA $2B
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xF314U, 0xF316U, kZeroPage);
            pc = 0xF316U;
            break;

        case 0xF316U:  // ADC #$10
            add_with_carry(0x10U);
            append_checkpoint(0xF316U, 0xF318U, kImmediate);
            pc = 0xF318U;
            break;

        case 0xF318U:  // STA $2B
            store(0xF318U, zp(0x2BU), registers.accumulator);
            append_checkpoint(0xF318U, 0xF31AU, kZeroPage);
            pc = 0xF31AU;
            break;

        case 0xF31AU:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF31AU, 0xF31BU, kImplied);
            pc = 0xF31BU;
            break;

        case 0xF31BU:  // ADC $2C
            add_with_carry(read_ram(zp(0x2CU)));
            append_checkpoint(0xF31BU, 0xF31DU, kZeroPage);
            pc = 0xF31DU;
            break;

        case 0xF31DU:  // STA $2C
            store(0xF31DU, zp(0x2CU), registers.accumulator);
            append_checkpoint(0xF31DU, 0xF31FU, kZeroPage);
            pc = 0xF31FU;
            break;

        case 0xF31FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF31FU, 0xF320U, kImplied);
            pc = 0xF320U;
            break;

        case 0xF320U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF320U, 0xF322U, kZeroPage);
            pc = 0xF322U;
            break;

        case 0xF322U:  // ADC #$40
            add_with_carry(0x40U);
            append_checkpoint(0xF322U, 0xF324U, kImmediate);
            pc = 0xF324U;
            break;

        case 0xF324U:  // STA $2D
            store(0xF324U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF324U, 0xF326U, kZeroPage);
            pc = 0xF326U;
            break;

        case 0xF326U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF326U, 0xF327U, kImplied);
            pc = 0xF327U;
            break;

        case 0xF327U:  // ADC $2E
            add_with_carry(read_ram(zp(0x2EU)));
            append_checkpoint(0xF327U, 0xF329U, kZeroPage);
            pc = 0xF329U;
            break;

        // The tilemap wraps every eight rows, so the VRAM high byte is masked
        // rather than allowed to run past the table.
        case 0xF329U:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xF329U, 0xF32BU, kImmediate);
            pc = 0xF32BU;
            break;

        case 0xF32BU:  // STA $2E
            store(0xF32BU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xF32BU, 0xF32DU, kZeroPage);
            pc = 0xF32DU;
            break;

        // ---- 00:F330  the downward tail ------------------------------------
        case 0xF330U:  // JMP $F48D
            append_checkpoint(0xF330U, 0xF48DU, kJump);
            pc = 0xF48DU;
            break;

        // ---- 00:F333  a row came into view above the camera ----------------
        case 0xF333U:  // LDA $3580,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3580U + registers.x)));
            append_checkpoint(0xF333U, 0xF336U, kAbsolute);
            pc = 0xF336U;
            break;

        case 0xF336U:  // BMI $F33B
            if ((registers.status & kNegative) != 0U) {
                append_checkpoint(0xF336U, 0xF33BU, kBranchTaken);
                pc = 0xF33BU;
                break;
            }
            append_checkpoint(0xF336U, 0xF338U, kBranchNotTaken);
            pc = 0xF338U;
            break;

        case 0xF338U:  // JMP $F28B
            append_checkpoint(0xF338U, 0xF28BU, kJump);
            pc = 0xF28BU;
            break;

        case 0xF33BU:  // LDA $3500,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3500U + registers.x)));
            append_checkpoint(0xF33BU, 0xF33EU, kAbsolute);
            pc = 0xF33EU;
            break;

        case 0xF33EU:  // STA $A6
            store(0xF33EU, zp(0xA6U), registers.accumulator);
            append_checkpoint(0xF33EU, 0xF340U, kZeroPage);
            pc = 0xF340U;
            break;

        case 0xF340U:  // LDA $3580,X
            load_accumulator(read_ram(static_cast<std::uint16_t>(
                0x3580U + registers.x)));
            append_checkpoint(0xF340U, 0xF343U, kAbsolute);
            pc = 0xF343U;
            break;

        case 0xF343U:  // STA $A7
            store(0xF343U, zp(0xA7U), registers.accumulator);
            append_checkpoint(0xF343U, 0xF345U, kZeroPage);
            pc = 0xF345U;
            break;

        // $26 is how far above the camera the new row sits -- one tile, or two
        // when the movement was at least a whole tile.
        case 0xF345U:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xF345U, 0xF347U, kImmediate);
            pc = 0xF347U;
            break;

        case 0xF347U:  // STA $26
            store(0xF347U, zp(0x26U), registers.accumulator);
            append_checkpoint(0xF347U, 0xF349U, kZeroPage);
            pc = 0xF349U;
            break;

        case 0xF349U:  // LDA $A7
            load_accumulator(read_ram(zp(0xA7U)));
            append_checkpoint(0xF349U, 0xF34BU, kZeroPage);
            pc = 0xF34BU;
            break;

        case 0xF34BU:  // EOR #$FF
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator ^ 0xFFU));
            append_checkpoint(0xF34BU, 0xF34DU, kImmediate);
            pc = 0xF34DU;
            break;

        case 0xF34DU:  // INC A
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator + 1U));
            append_checkpoint(0xF34DU, 0xF34EU, kImplied);
            pc = 0xF34EU;
            break;

        case 0xF34EU:  // CMP #$08
            registers.status =
                compare(registers.status, registers.accumulator, 0x08U);
            append_checkpoint(0xF34EU, 0xF350U, kImmediate);
            pc = 0xF350U;
            break;

        case 0xF350U:  // BCC $F356
            if ((registers.status & kCarry) == 0U) {
                append_checkpoint(0xF350U, 0xF356U, kBranchTaken);
                pc = 0xF356U;
                break;
            }
            append_checkpoint(0xF350U, 0xF352U, kBranchNotTaken);
            pc = 0xF352U;
            break;

        case 0xF352U:  // LDA #$10
            load_accumulator(0x10U);
            append_checkpoint(0xF352U, 0xF354U, kImmediate);
            pc = 0xF354U;
            break;

        case 0xF354U:  // STA $26
            store(0xF354U, zp(0x26U), registers.accumulator);
            append_checkpoint(0xF354U, 0xF356U, kZeroPage);
            pc = 0xF356U;
            break;

        case 0xF356U:  // STZ $2F
            store(0xF356U, zp(0x2FU), 0x00U);
            append_checkpoint(0xF356U, 0xF358U, kZeroPage);
            pc = 0xF358U;
            break;

        case 0xF358U:  // LDA #$14
            load_accumulator(0x14U);
            append_checkpoint(0xF358U, 0xF35AU, kImmediate);
            pc = 0xF35AU;
            break;

        case 0xF35AU:  // STA $30
            store(0xF35AU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF35AU, 0xF35CU, kZeroPage);
            pc = 0xF35CU;
            break;

        // ---- 00:F35F  the upward VRAM and attribute addresses --------------
        // The same three address computations as 00:F2AB, subtracting $26
        // instead of adding a tile.
        case 0xF35FU:  // LDA $A9
            load_accumulator(read_ram(zp(0xA9U)));
            append_checkpoint(0xF35FU, 0xF361U, kZeroPage);
            pc = 0xF361U;
            break;

        case 0xF361U:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF361U, 0xF363U, kImmediate);
            pc = 0xF363U;
            break;

        case 0xF363U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF363U, 0xF364U, kImplied);
            pc = 0xF364U;
            break;

        case 0xF364U:  // SBC $26
            subtract_with_carry(read_ram(zp(0x26U)));
            append_checkpoint(0xF364U, 0xF366U, kZeroPage);
            pc = 0xF366U;
            break;

        case 0xF366U:  // STA $2D
            store(0xF366U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF366U, 0xF368U, kZeroPage);
            pc = 0xF368U;
            break;

        case 0xF368U:  // STA $2B
            store(0xF368U, zp(0x2BU), registers.accumulator);
            append_checkpoint(0xF368U, 0xF36AU, kZeroPage);
            pc = 0xF36AU;
            break;

        case 0xF36AU:  // LDA $AA
            load_accumulator(read_ram(zp(0xAAU)));
            append_checkpoint(0xF36AU, 0xF36CU, kZeroPage);
            pc = 0xF36CU;
            break;

        case 0xF36CU:  // SBC #$00
            subtract_with_carry(0x00U);
            append_checkpoint(0xF36CU, 0xF36EU, kImmediate);
            pc = 0xF36EU;
            break;

        case 0xF36EU:  // STA $2E
            store(0xF36EU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xF36EU, 0xF370U, kZeroPage);
            pc = 0xF370U;
            break;

        case 0xF370U:  // STA $2C
            store(0xF370U, zp(0x2CU), registers.accumulator);
            append_checkpoint(0xF370U, 0xF372U, kZeroPage);
            pc = 0xF372U;
            break;

        case 0xF372U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF372U, 0xF374U, kZeroPage);
            pc = 0xF374U;
            break;

        case 0xF374U:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF374U, 0xF376U, kImmediate);
            pc = 0xF376U;
            break;

        case 0xF376U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF376U, 0xF377U, kImplied);
            pc = 0xF377U;
            break;

        case 0xF377U:  // ROL $2E
            rotate_memory_left(0xF377U, 0x2EU);
            append_checkpoint(0xF377U, 0xF379U, kZeroPageReadModifyWrite);
            pc = 0xF379U;
            break;

        case 0xF379U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF379U, 0xF37AU, kImplied);
            pc = 0xF37AU;
            break;

        case 0xF37AU:  // ROL $2E
            rotate_memory_left(0xF37AU, 0x2EU);
            append_checkpoint(0xF37AU, 0xF37CU, kZeroPageReadModifyWrite);
            pc = 0xF37CU;
            break;

        case 0xF37CU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF37CU, 0xF37DU, kImplied);
            pc = 0xF37DU;
            break;

        case 0xF37DU:  // ROL $2E
            rotate_memory_left(0xF37DU, 0x2EU);
            append_checkpoint(0xF37DU, 0xF37FU, kZeroPageReadModifyWrite);
            pc = 0xF37FU;
            break;

        case 0xF37FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF37FU, 0xF380U, kImplied);
            pc = 0xF380U;
            break;

        case 0xF380U:  // ADC $2F
            add_with_carry(read_ram(zp(0x2FU)));
            append_checkpoint(0xF380U, 0xF382U, kZeroPage);
            pc = 0xF382U;
            break;

        case 0xF382U:  // STA $2F
            store(0xF382U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF382U, 0xF384U, kZeroPage);
            pc = 0xF384U;
            break;

        case 0xF384U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF384U, 0xF386U, kZeroPage);
            pc = 0xF386U;
            break;

        case 0xF386U:  // ADC $2E
            add_with_carry(read_ram(zp(0x2EU)));
            append_checkpoint(0xF386U, 0xF388U, kZeroPage);
            pc = 0xF388U;
            break;

        case 0xF388U:  // STA $30
            store(0xF388U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF388U, 0xF38AU, kZeroPage);
            pc = 0xF38AU;
            break;

        case 0xF38AU:  // LDA $2B
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xF38AU, 0xF38CU, kZeroPage);
            pc = 0xF38CU;
            break;

        case 0xF38CU:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF38CU, 0xF38EU, kImmediate);
            pc = 0xF38EU;
            break;

        case 0xF38EU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF38EU, 0xF38FU, kImplied);
            pc = 0xF38FU;
            break;

        case 0xF38FU:  // ROL $2C
            rotate_memory_left(0xF38FU, 0x2CU);
            append_checkpoint(0xF38FU, 0xF391U, kZeroPageReadModifyWrite);
            pc = 0xF391U;
            break;

        case 0xF391U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF391U, 0xF392U, kImplied);
            pc = 0xF392U;
            break;

        case 0xF392U:  // ADC #$59
            add_with_carry(0x59U);
            append_checkpoint(0xF392U, 0xF394U, kImmediate);
            pc = 0xF394U;
            break;

        case 0xF394U:  // STA $2B
            store(0xF394U, zp(0x2BU), registers.accumulator);
            append_checkpoint(0xF394U, 0xF396U, kZeroPage);
            pc = 0xF396U;
            break;

        case 0xF396U:  // LDA $2C
            load_accumulator(read_ram(zp(0x2CU)));
            append_checkpoint(0xF396U, 0xF398U, kZeroPage);
            pc = 0xF398U;
            break;

        case 0xF398U:  // ADC #$22
            add_with_carry(0x22U);
            append_checkpoint(0xF398U, 0xF39AU, kImmediate);
            pc = 0xF39AU;
            break;

        case 0xF39AU:  // STA $2C
            store(0xF39AU, zp(0x2CU), registers.accumulator);
            append_checkpoint(0xF39AU, 0xF39CU, kZeroPage);
            pc = 0xF39CU;
            break;

        case 0xF39CU:  // LDA $0B
            load_accumulator(read_ram(zp(0x0BU)));
            append_checkpoint(0xF39CU, 0xF39EU, kZeroPage);
            pc = 0xF39EU;
            break;

        case 0xF39EU:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF39EU, 0xF3A0U, kImmediate);
            pc = 0xF3A0U;
            break;

        case 0xF3A0U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF3A0U, 0xF3A1U, kImplied);
            pc = 0xF3A1U;
            break;

        case 0xF3A1U:  // SBC $26
            subtract_with_carry(read_ram(zp(0x26U)));
            append_checkpoint(0xF3A1U, 0xF3A3U, kZeroPage);
            pc = 0xF3A3U;
            break;

        case 0xF3A3U:  // AND #$F8
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF8U));
            append_checkpoint(0xF3A3U, 0xF3A5U, kImmediate);
            pc = 0xF3A5U;
            break;

        case 0xF3A5U:  // STZ $2E
            store(0xF3A5U, zp(0x2EU), 0x00U);
            append_checkpoint(0xF3A5U, 0xF3A7U, kZeroPage);
            pc = 0xF3A7U;
            break;

        case 0xF3A7U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF3A7U, 0xF3A8U, kImplied);
            pc = 0xF3A8U;
            break;

        case 0xF3A8U:  // ROL $2E
            rotate_memory_left(0xF3A8U, 0x2EU);
            append_checkpoint(0xF3A8U, 0xF3AAU, kZeroPageReadModifyWrite);
            pc = 0xF3AAU;
            break;

        case 0xF3AAU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF3AAU, 0xF3ABU, kImplied);
            pc = 0xF3ABU;
            break;

        case 0xF3ABU:  // ROL $2E
            rotate_memory_left(0xF3ABU, 0x2EU);
            append_checkpoint(0xF3ABU, 0xF3ADU, kZeroPageReadModifyWrite);
            pc = 0xF3ADU;
            break;

        case 0xF3ADU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF3ADU, 0xF3AEU, kImplied);
            pc = 0xF3AEU;
            break;

        case 0xF3AEU:  // ROL $2E
            rotate_memory_left(0xF3AEU, 0x2EU);
            append_checkpoint(0xF3AEU, 0xF3B0U, kZeroPageReadModifyWrite);
            pc = 0xF3B0U;
            break;

        case 0xF3B0U:  // STA $2D
            store(0xF3B0U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF3B0U, 0xF3B2U, kZeroPage);
            pc = 0xF3B2U;
            break;

        // ---- 00:F3B5  a second row above, when $26 spanned two -------------
        case 0xF3B5U:  // LDA $26
            load_accumulator(read_ram(zp(0x26U)));
            append_checkpoint(0xF3B5U, 0xF3B7U, kZeroPage);
            pc = 0xF3B7U;
            break;

        case 0xF3B7U:  // CMP #$08
            registers.status =
                compare(registers.status, registers.accumulator, 0x08U);
            append_checkpoint(0xF3B7U, 0xF3B9U, kImmediate);
            pc = 0xF3B9U;
            break;

        case 0xF3B9U:  // BEQ $F3E4
            if ((registers.status & kZero) != 0U) {
                append_checkpoint(0xF3B9U, 0xF3E4U, kBranchTaken);
                pc = 0xF3E4U;
                break;
            }
            append_checkpoint(0xF3B9U, 0xF3BBU, kBranchNotTaken);
            pc = 0xF3BBU;
            break;

        case 0xF3BBU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF3BBU, 0xF3BCU, kImplied);
            pc = 0xF3BCU;
            break;

        case 0xF3BCU:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF3BCU, 0xF3BEU, kZeroPage);
            pc = 0xF3BEU;
            break;

        case 0xF3BEU:  // ADC #$40
            add_with_carry(0x40U);
            append_checkpoint(0xF3BEU, 0xF3C0U, kImmediate);
            pc = 0xF3C0U;
            break;

        case 0xF3C0U:  // STA $2F
            store(0xF3C0U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF3C0U, 0xF3C2U, kZeroPage);
            pc = 0xF3C2U;
            break;

        case 0xF3C2U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF3C2U, 0xF3C3U, kImplied);
            pc = 0xF3C3U;
            break;

        case 0xF3C3U:  // ADC $30
            add_with_carry(read_ram(zp(0x30U)));
            append_checkpoint(0xF3C3U, 0xF3C5U, kZeroPage);
            pc = 0xF3C5U;
            break;

        case 0xF3C5U:  // STA $30
            store(0xF3C5U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF3C5U, 0xF3C7U, kZeroPage);
            pc = 0xF3C7U;
            break;

        case 0xF3C7U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF3C7U, 0xF3C8U, kImplied);
            pc = 0xF3C8U;
            break;

        case 0xF3C8U:  // LDA $2B
            load_accumulator(read_ram(zp(0x2BU)));
            append_checkpoint(0xF3C8U, 0xF3CAU, kZeroPage);
            pc = 0xF3CAU;
            break;

        case 0xF3CAU:  // ADC #$10
            add_with_carry(0x10U);
            append_checkpoint(0xF3CAU, 0xF3CCU, kImmediate);
            pc = 0xF3CCU;
            break;

        case 0xF3CCU:  // STA $2B
            store(0xF3CCU, zp(0x2BU), registers.accumulator);
            append_checkpoint(0xF3CCU, 0xF3CEU, kZeroPage);
            pc = 0xF3CEU;
            break;

        case 0xF3CEU:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF3CEU, 0xF3CFU, kImplied);
            pc = 0xF3CFU;
            break;

        case 0xF3CFU:  // ADC $2C
            add_with_carry(read_ram(zp(0x2CU)));
            append_checkpoint(0xF3CFU, 0xF3D1U, kZeroPage);
            pc = 0xF3D1U;
            break;

        case 0xF3D1U:  // STA $2C
            store(0xF3D1U, zp(0x2CU), registers.accumulator);
            append_checkpoint(0xF3D1U, 0xF3D3U, kZeroPage);
            pc = 0xF3D3U;
            break;

        case 0xF3D3U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF3D3U, 0xF3D4U, kImplied);
            pc = 0xF3D4U;
            break;

        case 0xF3D4U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF3D4U, 0xF3D6U, kZeroPage);
            pc = 0xF3D6U;
            break;

        case 0xF3D6U:  // ADC #$40
            add_with_carry(0x40U);
            append_checkpoint(0xF3D6U, 0xF3D8U, kImmediate);
            pc = 0xF3D8U;
            break;

        case 0xF3D8U:  // STA $2D
            store(0xF3D8U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF3D8U, 0xF3DAU, kZeroPage);
            pc = 0xF3DAU;
            break;

        case 0xF3DAU:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF3DAU, 0xF3DBU, kImplied);
            pc = 0xF3DBU;
            break;

        case 0xF3DBU:  // ADC $2E
            add_with_carry(read_ram(zp(0x2EU)));
            append_checkpoint(0xF3DBU, 0xF3DDU, kZeroPage);
            pc = 0xF3DDU;
            break;

        case 0xF3DDU:  // AND #$07
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x07U));
            append_checkpoint(0xF3DDU, 0xF3DFU, kImmediate);
            pc = 0xF3DFU;
            break;

        case 0xF3DFU:  // STA $2E
            store(0xF3DFU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xF3DFU, 0xF3E1U, kZeroPage);
            pc = 0xF3E1U;
            break;

        // ---- 00:F3E4  the upward tail --------------------------------------
        case 0xF3E4U:  // JMP $F43D
            append_checkpoint(0xF3E4U, 0xF43DU, kJump);
            pc = 0xF43DU;
            break;

        // ---- 00:F3E7  the row writer ---------------------------------------
        // $A4/$A5 caches the source the last upload used. The route never
        // repeats one, so the early return at 00:F3F3 is transcribed from the
        // disassembly and is not exercised by the census.
        case 0xF3E7U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF3E7U, 0xF3E9U, kZeroPage);
            pc = 0xF3E9U;
            break;

        case 0xF3E9U:  // CMP $A4
            registers.status = compare(
                registers.status, registers.accumulator, read_ram(zp(0xA4U)));
            append_checkpoint(0xF3E9U, 0xF3EBU, kZeroPage);
            pc = 0xF3EBU;
            break;

        case 0xF3EBU:  // BNE $F3F4
            if ((registers.status & kZero) == 0U) {
                append_checkpoint(0xF3EBU, 0xF3F4U, kBranchTaken);
                pc = 0xF3F4U;
                break;
            }
            append_checkpoint(0xF3EBU, 0xF3EDU, kBranchNotTaken);
            pc = 0xF3EDU;
            break;

        case 0xF3EDU:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF3EDU, 0xF3EFU, kZeroPage);
            pc = 0xF3EFU;
            break;

        case 0xF3EFU:  // CMP $A5
            registers.status = compare(
                registers.status, registers.accumulator, read_ram(zp(0xA5U)));
            append_checkpoint(0xF3EFU, 0xF3F1U, kZeroPage);
            pc = 0xF3F1U;
            break;

        case 0xF3F1U:  // BNE $F3F4
            if ((registers.status & kZero) == 0U) {
                append_checkpoint(0xF3F1U, 0xF3F4U, kBranchTaken);
                pc = 0xF3F4U;
                break;
            }
            append_checkpoint(0xF3F1U, 0xF3F3U, kBranchNotTaken);
            pc = 0xF3F3U;
            break;

        case 0xF3F4U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF3F4U, 0xF3F6U, kZeroPage);
            pc = 0xF3F6U;
            break;

        case 0xF3F6U:  // STA $A2
            store(0xF3F6U, zp(0xA2U), registers.accumulator);
            append_checkpoint(0xF3F6U, 0xF3F8U, kZeroPage);
            pc = 0xF3F8U;
            break;

        case 0xF3F8U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF3F8U, 0xF3FAU, kZeroPage);
            pc = 0xF3FAU;
            break;

        case 0xF3FAU:  // STA $A3
            store(0xF3FAU, zp(0xA3U), registers.accumulator);
            append_checkpoint(0xF3FAU, 0xF3FCU, kZeroPage);
            pc = 0xF3FCU;
            break;

        // VDC register $00 is the write-address latch and $02 the data port.
        // $06 shadows the register select the interrupt handler restores.
        case 0xF3FCU:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0xF3FCU, 0xF3FEU, kImmediate);
            pc = 0xF3FEU;
            break;

        case 0xF3FEU:  // STA $06
            store(0xF3FEU, zp(0x06U), registers.accumulator);
            append_checkpoint(0xF3FEU, 0xF400U, kZeroPage);
            pc = 0xF400U;
            break;

        case 0xF400U:  // STA $0000
            store_port(0xF400U, 0x0000U, registers.accumulator);
            append_checkpoint(
                0xF400U, 0xF403U, kPortStore, kPortSampleBack);
            pc = 0xF403U;
            break;

        case 0xF403U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF403U, 0xF405U, kZeroPage);
            pc = 0xF405U;
            break;

        case 0xF405U:  // STA $0002
            store_port(0xF405U, 0x0002U, registers.accumulator);
            append_checkpoint(
                0xF405U, 0xF408U, kPortStore, kPortSampleBack);
            pc = 0xF408U;
            break;

        case 0xF408U:  // LDA $2E
            load_accumulator(read_ram(zp(0x2EU)));
            append_checkpoint(0xF408U, 0xF40AU, kZeroPage);
            pc = 0xF40AU;
            break;

        case 0xF40AU:  // STA $0003
            store_port(0xF40AU, 0x0003U, registers.accumulator);
            append_checkpoint(
                0xF40AU, 0xF40DU, kPortStore, kPortSampleBack);
            pc = 0xF40DU;
            break;

        case 0xF40DU:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xF40DU, 0xF40FU, kImmediate);
            pc = 0xF40FU;
            break;

        case 0xF40FU:  // STA $06
            store(0xF40FU, zp(0x06U), registers.accumulator);
            append_checkpoint(0xF40FU, 0xF411U, kZeroPage);
            pc = 0xF411U;
            break;

        case 0xF411U:  // STA $0000
            store_port(0xF411U, 0x0000U, registers.accumulator);
            append_checkpoint(
                0xF411U, 0xF414U, kPortStore, kPortSampleBack);
            pc = 0xF414U;
            break;

        case 0xF414U:  // CLY -- clears Y and no flag
            registers.y = 0x00U;
            append_checkpoint(0xF414U, 0xF415U, kImplied);
            pc = 0xF415U;
            break;

        // Thirty-two words. The low byte of each is a tile index straight from
        // the source; the high byte is the next index with the tile's attribute
        // nibble folded into its top four bits.
        case 0xF415U:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xF415U, 0xF417U, kIndirectIndexed);
            pc = 0xF417U;
            break;

        case 0xF417U:  // STA $0002
            store_port(0xF417U, 0x0002U, registers.accumulator);
            append_checkpoint(
                0xF417U, 0xF41AU, kPortStore, kPortSampleBack);
            pc = 0xF41AU;
            break;

        case 0xF41AU:  // PHY
            push(0xF41AU, registers.y);
            append_checkpoint(0xF41AU, 0xF41BU, kPushY);
            pc = 0xF41BU;
            break;

        case 0xF41BU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0xF41BU, 0xF41CU, kImplied);
            pc = 0xF41CU;
            break;

        // Y halved is the word index, and halved again the attribute byte that
        // packs two of them. The odd bit decides which nibble.
        case 0xF41CU:  // LSR A
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if ((registers.accumulator & 0x01U) != 0U) {
                registers.status =
                    static_cast<std::uint8_t>(registers.status | kCarry);
            }
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator >> 1U));
            append_checkpoint(0xF41CU, 0xF41DU, kImplied);
            pc = 0xF41DU;
            break;

        case 0xF41DU:  // STA $27
            store(0xF41DU, zp(0x27U), registers.accumulator);
            append_checkpoint(0xF41DU, 0xF41FU, kZeroPage);
            pc = 0xF41FU;
            break;

        case 0xF41FU:  // LSR A
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if ((registers.accumulator & 0x01U) != 0U) {
                registers.status =
                    static_cast<std::uint8_t>(registers.status | kCarry);
            }
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator >> 1U));
            append_checkpoint(0xF41FU, 0xF420U, kImplied);
            pc = 0xF420U;
            break;

        case 0xF420U:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0xF420U, 0xF421U, kImplied);
            pc = 0xF421U;
            break;

        case 0xF421U:  // LDA ($2B),Y
            load_accumulator(read_mapped(indirect_y(0x2BU)));
            append_checkpoint(0xF421U, 0xF423U, kIndirectIndexed);
            pc = 0xF423U;
            break;

        // ---- 00:F426 / 00:F42A  the two arms of the attribute nibble -------
        // BBR0 at 00:F423 branches when the bit is clear, taking the even
        // word's nibble as it stands and shifting the odd word's down from the
        // high half. The core executes the BBR; see the note above.
        case 0xF426U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF426U, 0xF427U, kImplied);
            pc = 0xF427U;
            break;

        case 0xF427U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF427U, 0xF428U, kImplied);
            pc = 0xF428U;
            break;

        case 0xF428U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF428U, 0xF429U, kImplied);
            pc = 0xF429U;
            break;

        case 0xF429U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xF429U, 0xF42AU, kImplied);
            pc = 0xF42AU;
            break;

        case 0xF42AU:  // AND #$F0
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xF0U));
            append_checkpoint(0xF42AU, 0xF42CU, kImmediate);
            pc = 0xF42CU;
            break;

        case 0xF42CU:  // STA $27
            store(0xF42CU, zp(0x27U), registers.accumulator);
            append_checkpoint(0xF42CU, 0xF42EU, kZeroPage);
            pc = 0xF42EU;
            break;

        case 0xF42EU:  // PLY
            load_y(pop());
            append_checkpoint(0xF42EU, 0xF42FU, kPullY);
            pc = 0xF42FU;
            break;

        case 0xF42FU:  // INY
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xF42FU, 0xF430U, kImplied);
            pc = 0xF430U;
            break;

        case 0xF430U:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xF430U, 0xF432U, kIndirectIndexed);
            pc = 0xF432U;
            break;

        case 0xF432U:  // ORA $27
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0x27U))));
            append_checkpoint(0xF432U, 0xF434U, kZeroPage);
            pc = 0xF434U;
            break;

        case 0xF434U:  // STA $0003
            store_port(0xF434U, 0x0003U, registers.accumulator);
            append_checkpoint(
                0xF434U, 0xF437U, kPortStore, kPortSampleBack);
            pc = 0xF437U;
            break;

        case 0xF437U:  // INY
            load_y(static_cast<std::uint8_t>(registers.y + 1U));
            append_checkpoint(0xF437U, 0xF438U, kImplied);
            pc = 0xF438U;
            break;

        case 0xF438U:  // CPY #$40
            registers.status = compare(registers.status, registers.y, 0x40U);
            append_checkpoint(0xF438U, 0xF43AU, kImmediate);
            pc = 0xF43AU;
            break;

        case 0xF43AU:  // BNE $F415
            if ((registers.status & kZero) == 0U) {
                append_checkpoint(0xF43AU, 0xF415U, kBranchTaken);
                pc = 0xF415U;
                break;
            }
            append_checkpoint(0xF43AU, 0xF43CU, kBranchNotTaken);
            pc = 0xF43CU;
            break;

        // ---- 00:F43D  fold an upward movement back into the camera ---------
        case 0xF43DU:  // STZ $7A
            store(0xF43DU, zp(0x7AU), 0x00U);
            append_checkpoint(0xF43DU, 0xF43FU, kZeroPage);
            pc = 0xF43FU;
            break;

        case 0xF43FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF43FU, 0xF440U, kImplied);
            pc = 0xF440U;
            break;

        case 0xF440U:  // LDA $A8
            load_accumulator(read_ram(zp(0xA8U)));
            append_checkpoint(0xF440U, 0xF442U, kZeroPage);
            pc = 0xF442U;
            break;

        case 0xF442U:  // ADC $A6
            add_with_carry(read_ram(zp(0xA6U)));
            append_checkpoint(0xF442U, 0xF444U, kZeroPage);
            pc = 0xF444U;
            break;

        case 0xF444U:  // STA $2F
            store(0xF444U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF444U, 0xF446U, kZeroPage);
            pc = 0xF446U;
            break;

        case 0xF446U:  // LDA $A9
            load_accumulator(read_ram(zp(0xA9U)));
            append_checkpoint(0xF446U, 0xF448U, kZeroPage);
            pc = 0xF448U;
            break;

        case 0xF448U:  // ADC $A7
            add_with_carry(read_ram(zp(0xA7U)));
            append_checkpoint(0xF448U, 0xF44AU, kZeroPage);
            pc = 0xF44AU;
            break;

        case 0xF44AU:  // STA $30
            store(0xF44AU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF44AU, 0xF44CU, kZeroPage);
            pc = 0xF44CU;
            break;

        case 0xF44CU:  // LDA $AA
            load_accumulator(read_ram(zp(0xAAU)));
            append_checkpoint(0xF44CU, 0xF44EU, kZeroPage);
            pc = 0xF44EU;
            break;

        case 0xF44EU:  // ADC #$FF
            add_with_carry(0xFFU);
            append_checkpoint(0xF44EU, 0xF450U, kImmediate);
            pc = 0xF450U;
            break;

        case 0xF450U:  // STA $2D
            store(0xF450U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF450U, 0xF452U, kZeroPage);
            pc = 0xF452U;
            break;

        // The route never scrolls past the top of the table, so 00:F454-F463 --
        // the clamp that would run if it did -- is transcribed from the
        // disassembly and is not exercised by the census.
        case 0xF452U:  // BPL $F465
            if ((registers.status & kNegative) == 0U) {
                append_checkpoint(0xF452U, 0xF465U, kBranchTaken);
                pc = 0xF465U;
                break;
            }
            append_checkpoint(0xF452U, 0xF454U, kBranchNotTaken);
            pc = 0xF454U;
            break;

        case 0xF454U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF454U, 0xF455U, kImplied);
            pc = 0xF455U;
            break;

        case 0xF455U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF455U, 0xF456U, kImplied);
            pc = 0xF456U;
            break;

        case 0xF456U:  // SBC $A8
            subtract_with_carry(read_ram(zp(0xA8U)));
            append_checkpoint(0xF456U, 0xF458U, kZeroPage);
            pc = 0xF458U;
            break;

        case 0xF458U:  // STA $A6
            store(0xF458U, zp(0xA6U), registers.accumulator);
            append_checkpoint(0xF458U, 0xF45AU, kZeroPage);
            pc = 0xF45AU;
            break;

        case 0xF45AU:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF45AU, 0xF45BU, kImplied);
            pc = 0xF45BU;
            break;

        case 0xF45BU:  // SBC $A9
            subtract_with_carry(read_ram(zp(0xA9U)));
            append_checkpoint(0xF45BU, 0xF45DU, kZeroPage);
            pc = 0xF45DU;
            break;

        case 0xF45DU:  // STA $A7
            store(0xF45DU, zp(0xA7U), registers.accumulator);
            append_checkpoint(0xF45DU, 0xF45FU, kZeroPage);
            pc = 0xF45FU;
            break;

        case 0xF45FU:  // STZ $2F
            store(0xF45FU, zp(0x2FU), 0x00U);
            append_checkpoint(0xF45FU, 0xF461U, kZeroPage);
            pc = 0xF461U;
            break;

        case 0xF461U:  // STZ $30
            store(0xF461U, zp(0x30U), 0x00U);
            append_checkpoint(0xF461U, 0xF463U, kZeroPage);
            pc = 0xF463U;
            break;

        case 0xF463U:  // STZ $2D
            store(0xF463U, zp(0x2DU), 0x00U);
            append_checkpoint(0xF463U, 0xF465U, kZeroPage);
            pc = 0xF465U;
            break;

        case 0xF465U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF465U, 0xF467U, kZeroPage);
            pc = 0xF467U;
            break;

        case 0xF467U:  // STA $A8
            store(0xF467U, zp(0xA8U), registers.accumulator);
            append_checkpoint(0xF467U, 0xF469U, kZeroPage);
            pc = 0xF469U;
            break;

        case 0xF469U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF469U, 0xF46BU, kZeroPage);
            pc = 0xF46BU;
            break;

        case 0xF46BU:  // STA $A9
            store(0xF46BU, zp(0xA9U), registers.accumulator);
            append_checkpoint(0xF46BU, 0xF46DU, kZeroPage);
            pc = 0xF46DU;
            break;

        case 0xF46DU:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF46DU, 0xF46FU, kZeroPage);
            pc = 0xF46FU;
            break;

        case 0xF46FU:  // STA $AA
            store(0xF46FU, zp(0xAAU), registers.accumulator);
            append_checkpoint(0xF46FU, 0xF471U, kZeroPage);
            pc = 0xF471U;
            break;

        case 0xF471U:  // LDA $A6
            load_accumulator(read_ram(zp(0xA6U)));
            append_checkpoint(0xF471U, 0xF473U, kZeroPage);
            pc = 0xF473U;
            break;

        case 0xF473U:  // ORA $A7
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0xA7U))));
            append_checkpoint(0xF473U, 0xF475U, kZeroPage);
            pc = 0xF475U;
            break;

        case 0xF475U:  // BEQ $F48A
            if ((registers.status & kZero) != 0U) {
                append_checkpoint(0xF475U, 0xF48AU, kBranchTaken);
                pc = 0xF48AU;
                break;
            }
            append_checkpoint(0xF475U, 0xF477U, kBranchNotTaken);
            pc = 0xF477U;
            break;

        case 0xF477U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF477U, 0xF478U, kImplied);
            pc = 0xF478U;
            break;

        case 0xF478U:  // LDA $0A
            load_accumulator(read_ram(zp(0x0AU)));
            append_checkpoint(0xF478U, 0xF47AU, kZeroPage);
            pc = 0xF47AU;
            break;

        case 0xF47AU:  // ADC $A6
            add_with_carry(read_ram(zp(0xA6U)));
            append_checkpoint(0xF47AU, 0xF47CU, kZeroPage);
            pc = 0xF47CU;
            break;

        case 0xF47CU:  // STA $0A
            store(0xF47CU, zp(0x0AU), registers.accumulator);
            append_checkpoint(0xF47CU, 0xF47EU, kZeroPage);
            pc = 0xF47EU;
            break;

        case 0xF47EU:  // LDA $0B
            load_accumulator(read_ram(zp(0x0BU)));
            append_checkpoint(0xF47EU, 0xF480U, kZeroPage);
            pc = 0xF480U;
            break;

        case 0xF480U:  // ADC $A7
            add_with_carry(read_ram(zp(0xA7U)));
            append_checkpoint(0xF480U, 0xF482U, kZeroPage);
            pc = 0xF482U;
            break;

        case 0xF482U:  // STA $0B
            store(0xF482U, zp(0x0BU), registers.accumulator);
            append_checkpoint(0xF482U, 0xF484U, kZeroPage);
            pc = 0xF484U;
            break;

        case 0xF484U:  // LDA $0C
            load_accumulator(read_ram(zp(0x0CU)));
            append_checkpoint(0xF484U, 0xF486U, kZeroPage);
            pc = 0xF486U;
            break;

        case 0xF486U:  // ADC #$FF
            add_with_carry(0xFFU);
            append_checkpoint(0xF486U, 0xF488U, kImmediate);
            pc = 0xF488U;
            break;

        case 0xF488U:  // STA $0C
            store(0xF488U, zp(0x0CU), registers.accumulator);
            append_checkpoint(0xF488U, 0xF48AU, kZeroPage);
            pc = 0xF48AU;
            break;

        case 0xF48AU:  // JMP $F4DF
            append_checkpoint(0xF48AU, 0xF4DFU, kJump);
            pc = 0xF4DFU;
            break;

        // ---- 00:F48D  fold a downward movement back into the camera --------
        case 0xF48DU:  // STZ $7A
            store(0xF48DU, zp(0x7AU), 0x00U);
            append_checkpoint(0xF48DU, 0xF48FU, kZeroPage);
            pc = 0xF48FU;
            break;

        case 0xF48FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF48FU, 0xF490U, kImplied);
            pc = 0xF490U;
            break;

        case 0xF490U:  // LDA $0A
            load_accumulator(read_ram(zp(0x0AU)));
            append_checkpoint(0xF490U, 0xF492U, kZeroPage);
            pc = 0xF492U;
            break;

        case 0xF492U:  // ADC $A6
            add_with_carry(read_ram(zp(0xA6U)));
            append_checkpoint(0xF492U, 0xF494U, kZeroPage);
            pc = 0xF494U;
            break;

        case 0xF494U:  // STA $2F
            store(0xF494U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF494U, 0xF496U, kZeroPage);
            pc = 0xF496U;
            break;

        case 0xF496U:  // LDA $0B
            load_accumulator(read_ram(zp(0x0BU)));
            append_checkpoint(0xF496U, 0xF498U, kZeroPage);
            pc = 0xF498U;
            break;

        case 0xF498U:  // ADC $A7
            add_with_carry(read_ram(zp(0xA7U)));
            append_checkpoint(0xF498U, 0xF49AU, kZeroPage);
            pc = 0xF49AU;
            break;

        case 0xF49AU:  // STA $30
            store(0xF49AU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF49AU, 0xF49CU, kZeroPage);
            pc = 0xF49CU;
            break;

        case 0xF49CU:  // LDA $0C
            load_accumulator(read_ram(zp(0x0CU)));
            append_checkpoint(0xF49CU, 0xF49EU, kZeroPage);
            pc = 0xF49EU;
            break;

        case 0xF49EU:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xF49EU, 0xF4A0U, kImmediate);
            pc = 0xF4A0U;
            break;

        case 0xF4A0U:  // STA $2D
            store(0xF4A0U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF4A0U, 0xF4A2U, kZeroPage);
            pc = 0xF4A2U;
            break;

        // A negative or zero remainder means the camera has reached the end of
        // its scroll, and the movement is trimmed to whatever was left.
        case 0xF4A2U:  // BMI $F4BD
            if ((registers.status & kNegative) != 0U) {
                append_checkpoint(0xF4A2U, 0xF4BDU, kBranchTaken);
                pc = 0xF4BDU;
                break;
            }
            append_checkpoint(0xF4A2U, 0xF4A4U, kBranchNotTaken);
            pc = 0xF4A4U;
            break;

        case 0xF4A4U:  // ORA $2F
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0x2FU))));
            append_checkpoint(0xF4A4U, 0xF4A6U, kZeroPage);
            pc = 0xF4A6U;
            break;

        case 0xF4A6U:  // ORA $30
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0x30U))));
            append_checkpoint(0xF4A6U, 0xF4A8U, kZeroPage);
            pc = 0xF4A8U;
            break;

        case 0xF4A8U:  // BEQ $F4BD
            if ((registers.status & kZero) != 0U) {
                append_checkpoint(0xF4A8U, 0xF4BDU, kBranchTaken);
                pc = 0xF4BDU;
                break;
            }
            append_checkpoint(0xF4A8U, 0xF4AAU, kBranchNotTaken);
            pc = 0xF4AAU;
            break;

        case 0xF4AAU:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF4AAU, 0xF4ABU, kImplied);
            pc = 0xF4ABU;
            break;

        case 0xF4ABU:  // LDA $A6
            load_accumulator(read_ram(zp(0xA6U)));
            append_checkpoint(0xF4ABU, 0xF4ADU, kZeroPage);
            pc = 0xF4ADU;
            break;

        case 0xF4ADU:  // SBC $2F
            subtract_with_carry(read_ram(zp(0x2FU)));
            append_checkpoint(0xF4ADU, 0xF4AFU, kZeroPage);
            pc = 0xF4AFU;
            break;

        case 0xF4AFU:  // STA $A6
            store(0xF4AFU, zp(0xA6U), registers.accumulator);
            append_checkpoint(0xF4AFU, 0xF4B1U, kZeroPage);
            pc = 0xF4B1U;
            break;

        case 0xF4B1U:  // LDA $A7
            load_accumulator(read_ram(zp(0xA7U)));
            append_checkpoint(0xF4B1U, 0xF4B3U, kZeroPage);
            pc = 0xF4B3U;
            break;

        case 0xF4B3U:  // SBC $30
            subtract_with_carry(read_ram(zp(0x30U)));
            append_checkpoint(0xF4B3U, 0xF4B5U, kZeroPage);
            pc = 0xF4B5U;
            break;

        case 0xF4B5U:  // STA $A7
            store(0xF4B5U, zp(0xA7U), registers.accumulator);
            append_checkpoint(0xF4B5U, 0xF4B7U, kZeroPage);
            pc = 0xF4B7U;
            break;

        case 0xF4B7U:  // STZ $2F
            store(0xF4B7U, zp(0x2FU), 0x00U);
            append_checkpoint(0xF4B7U, 0xF4B9U, kZeroPage);
            pc = 0xF4B9U;
            break;

        case 0xF4B9U:  // STZ $30
            store(0xF4B9U, zp(0x30U), 0x00U);
            append_checkpoint(0xF4B9U, 0xF4BBU, kZeroPage);
            pc = 0xF4BBU;
            break;

        case 0xF4BBU:  // STZ $2D
            store(0xF4BBU, zp(0x2DU), 0x00U);
            append_checkpoint(0xF4BBU, 0xF4BDU, kZeroPage);
            pc = 0xF4BDU;
            break;

        case 0xF4BDU:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xF4BDU, 0xF4BFU, kZeroPage);
            pc = 0xF4BFU;
            break;

        case 0xF4BFU:  // STA $0A
            store(0xF4BFU, zp(0x0AU), registers.accumulator);
            append_checkpoint(0xF4BFU, 0xF4C1U, kZeroPage);
            pc = 0xF4C1U;
            break;

        case 0xF4C1U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xF4C1U, 0xF4C3U, kZeroPage);
            pc = 0xF4C3U;
            break;

        case 0xF4C3U:  // STA $0B
            store(0xF4C3U, zp(0x0BU), registers.accumulator);
            append_checkpoint(0xF4C3U, 0xF4C5U, kZeroPage);
            pc = 0xF4C5U;
            break;

        case 0xF4C5U:  // LDA $2D
            load_accumulator(read_ram(zp(0x2DU)));
            append_checkpoint(0xF4C5U, 0xF4C7U, kZeroPage);
            pc = 0xF4C7U;
            break;

        case 0xF4C7U:  // STA $0C
            store(0xF4C7U, zp(0x0CU), registers.accumulator);
            append_checkpoint(0xF4C7U, 0xF4C9U, kZeroPage);
            pc = 0xF4C9U;
            break;

        case 0xF4C9U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xF4C9U, 0xF4CAU, kImplied);
            pc = 0xF4CAU;
            break;

        case 0xF4CAU:  // LDA $A8
            load_accumulator(read_ram(zp(0xA8U)));
            append_checkpoint(0xF4CAU, 0xF4CCU, kZeroPage);
            pc = 0xF4CCU;
            break;

        case 0xF4CCU:  // ADC $A6
            add_with_carry(read_ram(zp(0xA6U)));
            append_checkpoint(0xF4CCU, 0xF4CEU, kZeroPage);
            pc = 0xF4CEU;
            break;

        case 0xF4CEU:  // STA $A8
            store(0xF4CEU, zp(0xA8U), registers.accumulator);
            append_checkpoint(0xF4CEU, 0xF4D0U, kZeroPage);
            pc = 0xF4D0U;
            break;

        case 0xF4D0U:  // LDA $A9
            load_accumulator(read_ram(zp(0xA9U)));
            append_checkpoint(0xF4D0U, 0xF4D2U, kZeroPage);
            pc = 0xF4D2U;
            break;

        case 0xF4D2U:  // ADC $A7
            add_with_carry(read_ram(zp(0xA7U)));
            append_checkpoint(0xF4D2U, 0xF4D4U, kZeroPage);
            pc = 0xF4D4U;
            break;

        case 0xF4D4U:  // STA $A9
            store(0xF4D4U, zp(0xA9U), registers.accumulator);
            append_checkpoint(0xF4D4U, 0xF4D6U, kZeroPage);
            pc = 0xF4D6U;
            break;

        case 0xF4D6U:  // LDA $AA
            load_accumulator(read_ram(zp(0xAAU)));
            append_checkpoint(0xF4D6U, 0xF4D8U, kZeroPage);
            pc = 0xF4D8U;
            break;

        case 0xF4D8U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xF4D8U, 0xF4DAU, kImmediate);
            pc = 0xF4DAU;
            break;

        case 0xF4DAU:  // STA $AA
            store(0xF4DAU, zp(0xAAU), registers.accumulator);
            append_checkpoint(0xF4DAU, 0xF4DCU, kZeroPage);
            pc = 0xF4DCU;
            break;

        case 0xF4DCU:  // JMP $F4DF
            append_checkpoint(0xF4DCU, 0xF4DFU, kJump);
            pc = 0xF4DFU;
            break;

        // ---- 00:F4DF  publish the negated movement for the sweep -----------
        // $35/$36 is what the camera-visibility sweep at 00:F4F2 reads, and $26
        // its sign extension. The walk hands 00:F4F2 back to the core, where
        // that routine's own replacement picks it up.
        case 0xF4DFU:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xF4DFU, 0xF4E0U, kImplied);
            pc = 0xF4E0U;
            break;

        case 0xF4E0U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF4E0U, 0xF4E1U, kImplied);
            pc = 0xF4E1U;
            break;

        case 0xF4E1U:  // SBC $A6
            subtract_with_carry(read_ram(zp(0xA6U)));
            append_checkpoint(0xF4E1U, 0xF4E3U, kZeroPage);
            pc = 0xF4E3U;
            break;

        case 0xF4E3U:  // STA $35
            store(0xF4E3U, zp(0x35U), registers.accumulator);
            append_checkpoint(0xF4E3U, 0xF4E5U, kZeroPage);
            pc = 0xF4E5U;
            break;

        case 0xF4E5U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0xF4E5U, 0xF4E6U, kImplied);
            pc = 0xF4E6U;
            break;

        case 0xF4E6U:  // SBC $A7
            subtract_with_carry(read_ram(zp(0xA7U)));
            append_checkpoint(0xF4E6U, 0xF4E8U, kZeroPage);
            pc = 0xF4E8U;
            break;

        case 0xF4E8U:  // STA $36
            store(0xF4E8U, zp(0x36U), registers.accumulator);
            append_checkpoint(0xF4E8U, 0xF4EAU, kZeroPage);
            pc = 0xF4EAU;
            break;

        case 0xF4EAU:  // STZ $26
            store(0xF4EAU, zp(0x26U), 0x00U);
            append_checkpoint(0xF4EAU, 0xF4ECU, kZeroPage);
            pc = 0xF4ECU;
            break;

        case 0xF4ECU:  // LDA $36
            load_accumulator(read_ram(zp(0x36U)));
            append_checkpoint(0xF4ECU, 0xF4EEU, kZeroPage);
            pc = 0xF4EEU;
            break;

        case 0xF4EEU:  // BPL $F4F2
            if ((registers.status & kNegative) == 0U) {
                append_checkpoint(0xF4EEU, 0xF4F2U, kBranchTaken);
                pc = 0xF4F2U;
                break;
            }
            append_checkpoint(0xF4EEU, 0xF4F0U, kBranchNotTaken);
            pc = 0xF4F0U;
            break;

        case 0xF4F0U: {  // DEC $26
            const auto value = static_cast<std::uint8_t>(
                read_ram(zp(0x26U)) - 1U);
            registers.status = set_nz(registers.status, value);
            store(0xF4F0U, zp(0x26U), value);
            append_checkpoint(0xF4F0U, 0xF4F2U, kZeroPageReadModifyWrite);
            pc = 0xF4F2U;
            break;
        }

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
