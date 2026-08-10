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

// The collision tail: the frame around the central overlap scanner, which has
// been native since the twenty-sixth replacement. Five routines in physical
// bank 01, none of which reads a device register:
//
//   01:CD74  the playfield-tile probe, and the largest single piece of
//            unported logic left in phase 1. It converts the ball position in
//            $65/$66/$67 into a tile coordinate -- $65 >> 3 across, the
//            vertical position less $20 and then >> 3 -- indexes the row
//            pointer table through $C8/$C9 and the attribute table through
//            $C6/$C7, reads a tile id and its attribute byte through $2D and
//            $2F, scales the attribute by eight into the mask table at
//            $90/$91, and ANDs the byte it finds there against the one-bit
//            column mask at $CF2B,Y. A non-zero result is a solid tile: it
//            publishes the tile id in $78 and returns carry set. It makes no
//            calls at all, so the whole 2.6M is one transcript per entry.
//
//   01:CA84  the ball/table gate. Six 16-bit range tests against the compare
//            helper at 00:E93C decide which of two dispatch bytes to write
//            into $3D00,X. Nothing but the six calls interrupts it.
//
//   01:CB2A  the sweep setup: zero the two latches, take the ball position
//            less $20 and the two scroll limits into $65/$66/$67, and prime
//            the loop counter at $60 and the slot cursor at $64. It falls
//            into the sweep at 01:CB48, which is already native.
//
//   01:CBAB  the single-slot probe, a one-shot version of the sweep used when
//            the caller already knows which slot it wants.
//
//   01:CBCF  the confirmed-contact dispatcher. It marks the target, masks the
//            actor type and tail-dispatches through the vector pair at
//            01:CBFA/01:CBFF -- two PHAs and an RTS. That is ordinary for a
//            transcript: the pushes are transcribed and the core executes the
//            RTS, which jumps exactly as it would have.
//
// Nineteen entry points. Eleven are the addresses the core arrives at after a
// call this family makes, because a transcript stops in front of one; four are
// routine heads; and four are branch arms of instructions handed back to the
// core -- see is_stop_instruction.
//
// Its zero page is shared with the rest of the game: $2D-$30 is the general
// pointer scratch, $26 a tile-id temporary, $64-$67 the sweep cursor and
// geometry, $78 the contact latch and $BD,X the per-ball last-contact byte.
// Measured on an assembly write trace against exact windows from observers on
// the three top-level heads rather than inferred from the writes: across all
// 6,862 calls, every write to an address these walks read comes from bank 01
// game code or from the arithmetic helpers at 00:E56E-00:E847. The split is
// clean rather than close -- those eighteen helper instructions land inside a
// window on 99.3% to 100% of their executions, which is what a callee looks
// like, and every one of the three hundred other instructions that writes the
// same addresses anywhere on the route lands inside a window on exactly 0.00%
// of them. Nothing in the timer driver, the music sequencer or the VDC
// interrupt handler writes any of it.
constexpr std::uint8_t kBank = 0x01U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the stack page is $2100-$21FF.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// Master cycles per instruction: three times the core's ADDCYC(n) plus the
// LastCycle it always charges.
constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kZeroPageIndexed = 12U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kAbsoluteIndexed = 15U;
constexpr std::uint32_t kZeroPageReadModifyWrite = 18U;
// RMB and SMB are the same RMW_ZP shape with one extra ADDCYC between the read
// and the write, which is why they cost a cycle more than LSR or ROL.
constexpr std::uint32_t kZeroPageBitSet = 21U;
constexpr std::uint32_t kIndirectIndexed = 21U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
constexpr std::uint32_t kSwapXY = 9U;
constexpr std::uint32_t kPushAccumulator = 9U;

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
    return logical_pc == 0xCA84U || logical_pc == 0xCAA1U
        || logical_pc == 0xCAB6U || logical_pc == 0xCAD3U
        || logical_pc == 0xCAE0U || logical_pc == 0xCAEDU
        || logical_pc == 0xCB04U || logical_pc == 0xCB11U
        || logical_pc == 0xCB2AU || logical_pc == 0xCB74U
        || logical_pc == 0xCB79U || logical_pc == 0xCB81U
        || logical_pc == 0xCB8AU || logical_pc == 0xCBABU
        || logical_pc == 0xCBB6U || logical_pc == 0xCBC2U
        || logical_pc == 0xCBCAU || logical_pc == 0xCBCFU
        || logical_pc == 0xCD74U;
}

// Where each walk stops. Nine are RTS instructions and eleven are the calls
// this family makes, which the core executes so that whichever replacement
// owns the target is entered normally.
//
// Two are neither. 01:CB46 is the STA $64 that ends the sweep setup: handing
// it back leaves the core at 01:CB48, where the sweep's own replacement picks
// the loop up. And 01:CB7E is a BBR. Every other instruction here charges its
// cycles in one ADDCYC and one LastCycle, which is the shape a checkpoint
// expresses; BBR is LD_ZP wrapping JR, so the machine crosses it in four clock
// steps where a checkpoint crosses it in two, and those steps are where the
// core tests next_event. The tilemap slice measured what a coarser step does:
// it moves the video EventHandler to a later timestamp and shifts an interrupt.
// So 01:CB7E goes back to the core and both its arms, 01:CB81 and 01:CB8A,
// become entry points. The same reasoning applies to 01:CB71 and 01:CB68,
// which are BBS instructions this family never walks into -- 01:CB74 is an
// entry precisely because the core resolves 01:CB71 itself.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return logical_pc == 0xCA89U || logical_pc == 0xCA9EU
        || logical_pc == 0xCAB3U || logical_pc == 0xCAD0U
        || logical_pc == 0xCADDU || logical_pc == 0xCAEAU
        || logical_pc == 0xCB01U || logical_pc == 0xCB0EU
        || logical_pc == 0xCB20U || logical_pc == 0xCB29U
        || logical_pc == 0xCB46U || logical_pc == 0xCB76U
        || logical_pc == 0xCB7BU || logical_pc == 0xCB7EU
        || logical_pc == 0xCB89U || logical_pc == 0xCB92U
        || logical_pc == 0xCBB2U || logical_pc == 0xCBB3U
        || logical_pc == 0xCBBFU || logical_pc == 0xCBC6U
        || logical_pc == 0xCBC7U || logical_pc == 0xCBCEU
        || logical_pc == 0xCBF9U || logical_pc == 0xCDF1U
        || logical_pc == 0xCDF7U;
}

} // namespace

std::optional<RoutineReplacement> replace_collision_tail(
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
    // zero page for every AND, ORA, ADC and SBC below.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
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

    // Reads the byte a logical address currently maps to. The tile probe
    // reaches its row and attribute tables through pointers the caller staged,
    // and the two vector tables it dispatches from live in this bank's own
    // ROM page, so both are resolved from the live MPRs rather than assumed.
    // The hardware page fails the bank test, which is what makes a device read
    // decline rather than fabricate a value.
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
    // offset and the instruction of the checkpoint that follows it.
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
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
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

    // PUSH writes $2100 + S and then decrements S. Deriving it from the live
    // stack pointer is what keeps a transcript valid at a depth other than the
    // one it was recorded at.
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

    // Zero page lives at $2000, and the core wraps a zero-page index inside the
    // page before adding that base.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };
    const auto zp_x = [&registers](const std::uint8_t address) {
        return static_cast<std::uint16_t>(
            kWorkRamBase
            + static_cast<std::uint8_t>(address + registers.x));
    };
    // A zero-page pointer pair indexed by Y. The high half is fetched from the
    // next zero-page byte, wrapping inside the page exactly as GetIY does.
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
    const auto load_x = [&registers](const std::uint8_t value) {
        registers.x = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto and_accumulator = [&registers, &load_accumulator](
                                     const std::uint8_t operand) {
        load_accumulator(
            static_cast<std::uint8_t>(registers.accumulator & operand));
    };
    const auto or_accumulator = [&registers, &load_accumulator](
                                    const std::uint8_t operand) {
        load_accumulator(
            static_cast<std::uint8_t>(registers.accumulator | operand));
    };

    // ADC in binary mode, with the overflow bit the core computes. Nothing here
    // tests V, but a checkpoint that publishes the wrong one is abandoned at
    // the core's register comparison, so it has to be exact.
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

    // SBC in binary mode. The carry runs the other way: a borrow going in, and
    // set on the way out when the subtraction did not borrow.
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

    // CMP sets the same three flags a subtraction would but keeps the
    // accumulator, and leaves V alone.
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
    const auto shift_memory_right =
        [&registers, &read_ram, &store, &zp](
            const std::uint16_t instruction_pc,
            const std::uint8_t address) {
            const auto value = read_ram(zp(address));
            const auto result = static_cast<std::uint8_t>(value >> 1U);
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if ((value & kCarry) != 0U) {
                registers.status =
                    static_cast<std::uint8_t>(registers.status | kCarry);
            }
            registers.status = set_nz(registers.status, result);
            store(instruction_pc, zp(address), result);
        };
    // RMB clears one bit of a zero-page byte and touches no flag at all.
    const auto reset_memory_bit =
        [&read_ram, &store, &zp](
            const std::uint16_t instruction_pc,
            const std::uint8_t address,
            const std::uint8_t bit) {
            const auto value = read_ram(zp(address));
            store(
                instruction_pc,
                zp(address),
                static_cast<std::uint8_t>(
                    value & static_cast<std::uint8_t>(~(1U << bit))));
        };

    // A relative branch costs a cycle more when it is taken, and the address
    // it resumes at is the only thing that distinguishes the two arms.
    const auto branch =
        [&append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t taken_pc,
            const std::uint16_t fallthrough_pc,
            const bool condition) {
            const auto next = condition ? taken_pc : fallthrough_pc;
            append_checkpoint(
                instruction_pc,
                next,
                condition ? kBranchTaken : kBranchNotTaken);
            return next;
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
            return replacement;
        }

        switch (pc) {
        // ---- 01:CA84  the ball/table gate ---------------------------------
        // $3C80 is the table-active byte; anything non-zero means the gate is
        // shut and the routine returns immediately.
        case 0xCA84U:  // LDA $3C80
            load_accumulator(read_ram(0x3C80U));
            append_checkpoint(0xCA84U, 0xCA87U, kAbsolute);
            pc = 0xCA87U;
            break;

        case 0xCA87U:  // BEQ $CA8A
            pc = branch(
                0xCA87U, 0xCA8AU, 0xCA89U,
                (registers.status & kZero) != 0U);
            break;

        case 0xCA8AU:  // LDX #$00
            load_x(0x00U);
            append_checkpoint(0xCA8AU, 0xCA8CU, kImmediate);
            pc = 0xCA8CU;
            break;

        // $3180/$3200 is the ball position and $3300/$3380 the pair of scroll
        // limits; each is staged into $2F/$30 and compared against a constant
        // in $2D/$2E by the helper at 00:E93C.
        case 0xCA8CU:  // LDA $3180,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3180U + registers.x)));
            append_checkpoint(0xCA8CU, 0xCA8FU, kAbsoluteIndexed);
            pc = 0xCA8FU;
            break;

        case 0xCA8FU:  // STA $2F
            store(0xCA8FU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCA8FU, 0xCA91U, kZeroPage);
            pc = 0xCA91U;
            break;

        case 0xCA91U:  // LDA $3200,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3200U + registers.x)));
            append_checkpoint(0xCA91U, 0xCA94U, kAbsoluteIndexed);
            pc = 0xCA94U;
            break;

        case 0xCA94U:  // STA $30
            store(0xCA94U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCA94U, 0xCA96U, kZeroPage);
            pc = 0xCA96U;
            break;

        case 0xCA96U:  // LDA #$08
            load_accumulator(0x08U);
            append_checkpoint(0xCA96U, 0xCA98U, kImmediate);
            pc = 0xCA98U;
            break;

        case 0xCA98U:  // STA $2D
            store(0xCA98U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCA98U, 0xCA9AU, kZeroPage);
            pc = 0xCA9AU;
            break;

        case 0xCA9AU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xCA9AU, 0xCA9CU, kImmediate);
            pc = 0xCA9CU;
            break;

        case 0xCA9CU:  // STA $2E
            store(0xCA9CU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCA9CU, 0xCA9EU, kZeroPage);
            pc = 0xCA9EU;
            break;

        case 0xCAA1U:  // BCC $CAF1
            pc = branch(
                0xCAA1U, 0xCAF1U, 0xCAA3U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCAA3U:  // LDA $3300,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3300U + registers.x)));
            append_checkpoint(0xCAA3U, 0xCAA6U, kAbsoluteIndexed);
            pc = 0xCAA6U;
            break;

        case 0xCAA6U:  // STA $2F
            store(0xCAA6U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCAA6U, 0xCAA8U, kZeroPage);
            pc = 0xCAA8U;
            break;

        case 0xCAA8U:  // LDA $3380,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3380U + registers.x)));
            append_checkpoint(0xCAA8U, 0xCAABU, kAbsoluteIndexed);
            pc = 0xCAABU;
            break;

        case 0xCAABU:  // STA $30
            store(0xCAABU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCAABU, 0xCAADU, kZeroPage);
            pc = 0xCAADU;
            break;

        case 0xCAADU:  // STZ $2D
            store(0xCAADU, zp(0x2DU), 0x00U);
            append_checkpoint(0xCAADU, 0xCAAFU, kZeroPage);
            pc = 0xCAAFU;
            break;

        case 0xCAAFU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xCAAFU, 0xCAB1U, kImmediate);
            pc = 0xCAB1U;
            break;

        case 0xCAB1U:  // STA $2E
            store(0xCAB1U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCAB1U, 0xCAB3U, kZeroPage);
            pc = 0xCAB3U;
            break;

        case 0xCAB6U:  // BCC $CABE
            pc = branch(
                0xCAB6U, 0xCABEU, 0xCAB8U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCAB8U:  // LDA $53
            load_accumulator(read_ram(zp(0x53U)));
            append_checkpoint(0xCAB8U, 0xCABAU, kZeroPage);
            pc = 0xCABAU;
            break;

        case 0xCABAU:  // ORA #$08
            or_accumulator(0x08U);
            append_checkpoint(0xCABAU, 0xCABCU, kImmediate);
            pc = 0xCABCU;
            break;

        case 0xCABCU:  // STA $53
            store(0xCABCU, zp(0x53U), registers.accumulator);
            append_checkpoint(0xCABCU, 0xCABEU, kZeroPage);
            pc = 0xCABEU;
            break;

        case 0xCABEU:  // LDA $3300,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3300U + registers.x)));
            append_checkpoint(0xCABEU, 0xCAC1U, kAbsoluteIndexed);
            pc = 0xCAC1U;
            break;

        case 0xCAC1U:  // STA $2F
            store(0xCAC1U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCAC1U, 0xCAC3U, kZeroPage);
            pc = 0xCAC3U;
            break;

        case 0xCAC3U:  // LDA $3380,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3380U + registers.x)));
            append_checkpoint(0xCAC3U, 0xCAC6U, kAbsoluteIndexed);
            pc = 0xCAC6U;
            break;

        case 0xCAC6U:  // STA $30
            store(0xCAC6U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCAC6U, 0xCAC8U, kZeroPage);
            pc = 0xCAC8U;
            break;

        case 0xCAC8U:  // LDA #$68
            load_accumulator(0x68U);
            append_checkpoint(0xCAC8U, 0xCACAU, kImmediate);
            pc = 0xCACAU;
            break;

        case 0xCACAU:  // STA $2D
            store(0xCACAU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCACAU, 0xCACCU, kZeroPage);
            pc = 0xCACCU;
            break;

        case 0xCACCU:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xCACCU, 0xCACEU, kImmediate);
            pc = 0xCACEU;
            break;

        case 0xCACEU:  // STA $2E
            store(0xCACEU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCACEU, 0xCAD0U, kZeroPage);
            pc = 0xCAD0U;
            break;

        case 0xCAD3U:  // BCS $CB21
            pc = branch(
                0xCAD3U, 0xCB21U, 0xCAD5U,
                (registers.status & kCarry) != 0U);
            break;

        case 0xCAD5U:  // LDA #$30
            load_accumulator(0x30U);
            append_checkpoint(0xCAD5U, 0xCAD7U, kImmediate);
            pc = 0xCAD7U;
            break;

        case 0xCAD7U:  // STA $2D
            store(0xCAD7U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCAD7U, 0xCAD9U, kZeroPage);
            pc = 0xCAD9U;
            break;

        case 0xCAD9U:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xCAD9U, 0xCADBU, kImmediate);
            pc = 0xCADBU;
            break;

        case 0xCADBU:  // STA $2E
            store(0xCADBU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCADBU, 0xCADDU, kZeroPage);
            pc = 0xCADDU;
            break;

        case 0xCAE0U:  // BCS $CB16
            pc = branch(
                0xCAE0U, 0xCB16U, 0xCAE2U,
                (registers.status & kCarry) != 0U);
            break;

        case 0xCAE2U:  // LDA #$E8
            load_accumulator(0xE8U);
            append_checkpoint(0xCAE2U, 0xCAE4U, kImmediate);
            pc = 0xCAE4U;
            break;

        case 0xCAE4U:  // STA $2D
            store(0xCAE4U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCAE4U, 0xCAE6U, kZeroPage);
            pc = 0xCAE6U;
            break;

        case 0xCAE6U:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xCAE6U, 0xCAE8U, kImmediate);
            pc = 0xCAE8U;
            break;

        case 0xCAE8U:  // STA $2E
            store(0xCAE8U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCAE8U, 0xCAEAU, kZeroPage);
            pc = 0xCAEAU;
            break;

        case 0xCAEDU:  // BCS $CB21
            pc = branch(
                0xCAEDU, 0xCB21U, 0xCAEFU,
                (registers.status & kCarry) != 0U);
            break;

        case 0xCAEFU:  // BCC $CB16
            pc = branch(
                0xCAEFU, 0xCB16U, 0xCAF1U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCAF1U:  // LDA $3300,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3300U + registers.x)));
            append_checkpoint(0xCAF1U, 0xCAF4U, kAbsoluteIndexed);
            pc = 0xCAF4U;
            break;

        case 0xCAF4U:  // STA $2F
            store(0xCAF4U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCAF4U, 0xCAF6U, kZeroPage);
            pc = 0xCAF6U;
            break;

        case 0xCAF6U:  // LDA $3380,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3380U + registers.x)));
            append_checkpoint(0xCAF6U, 0xCAF9U, kAbsoluteIndexed);
            pc = 0xCAF9U;
            break;

        case 0xCAF9U:  // STA $30
            store(0xCAF9U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCAF9U, 0xCAFBU, kZeroPage);
            pc = 0xCAFBU;
            break;

        case 0xCAFBU:  // LDA #$40
            load_accumulator(0x40U);
            append_checkpoint(0xCAFBU, 0xCAFDU, kImmediate);
            pc = 0xCAFDU;
            break;

        case 0xCAFDU:  // STA $2D
            store(0xCAFDU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCAFDU, 0xCAFFU, kZeroPage);
            pc = 0xCAFFU;
            break;

        case 0xCAFFU:  // STZ $2E
            store(0xCAFFU, zp(0x2EU), 0x00U);
            append_checkpoint(0xCAFFU, 0xCB01U, kZeroPage);
            pc = 0xCB01U;
            break;

        case 0xCB04U:  // BCC $CB21
            pc = branch(
                0xCB04U, 0xCB21U, 0xCB06U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCB06U:  // LDA #$10
            load_accumulator(0x10U);
            append_checkpoint(0xCB06U, 0xCB08U, kImmediate);
            pc = 0xCB08U;
            break;

        case 0xCB08U:  // STA $2D
            store(0xCB08U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCB08U, 0xCB0AU, kZeroPage);
            pc = 0xCB0AU;
            break;

        case 0xCB0AU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xCB0AU, 0xCB0CU, kImmediate);
            pc = 0xCB0CU;
            break;

        case 0xCB0CU:  // STA $2E
            store(0xCB0CU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCB0CU, 0xCB0EU, kZeroPage);
            pc = 0xCB0EU;
            break;

        case 0xCB11U:  // BCC $CB16
            pc = branch(
                0xCB11U, 0xCB16U, 0xCB13U,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCB13U:  // STZ $2CFD
            store(0xCB13U, 0x2CFDU, 0x00U);
            append_checkpoint(0xCB13U, 0xCB16U, kAbsolute);
            pc = 0xCB16U;
            break;

        // The two exits write the same byte with bit 7 set or clear: in range
        // for the ball, or out of it.
        case 0xCB16U:  // LDA $3D00,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3D00U + registers.x)));
            append_checkpoint(0xCB16U, 0xCB19U, kAbsoluteIndexed);
            pc = 0xCB19U;
            break;

        case 0xCB19U:  // AND #$0F
            and_accumulator(0x0FU);
            append_checkpoint(0xCB19U, 0xCB1BU, kImmediate);
            pc = 0xCB1BU;
            break;

        case 0xCB1BU:  // ORA #$80
            or_accumulator(0x80U);
            append_checkpoint(0xCB1BU, 0xCB1DU, kImmediate);
            pc = 0xCB1DU;
            break;

        case 0xCB1DU:  // STA $3D00,X
            store(
                0xCB1DU,
                static_cast<std::uint16_t>(0x3D00U + registers.x),
                registers.accumulator);
            append_checkpoint(0xCB1DU, 0xCB20U, kAbsoluteIndexed);
            pc = 0xCB20U;
            break;

        case 0xCB21U:  // LDA $3D00,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3D00U + registers.x)));
            append_checkpoint(0xCB21U, 0xCB24U, kAbsoluteIndexed);
            pc = 0xCB24U;
            break;

        case 0xCB24U:  // AND #$0F
            and_accumulator(0x0FU);
            append_checkpoint(0xCB24U, 0xCB26U, kImmediate);
            pc = 0xCB26U;
            break;

        case 0xCB26U:  // STA $3D00,X
            store(
                0xCB26U,
                static_cast<std::uint16_t>(0x3D00U + registers.x),
                registers.accumulator);
            append_checkpoint(0xCB26U, 0xCB29U, kAbsoluteIndexed);
            pc = 0xCB29U;
            break;

        // ---- 01:CB2A  the sweep setup -------------------------------------
        // $B7 collects the group flags the sweep raises and $77 its result
        // latch; $65/$66/$67 is the ball geometry every candidate is tested
        // against; $60 counts the 124 slots and $64 is the cursor.
        case 0xCB2AU:  // STZ $B7
            store(0xCB2AU, zp(0xB7U), 0x00U);
            append_checkpoint(0xCB2AU, 0xCB2CU, kZeroPage);
            pc = 0xCB2CU;
            break;

        case 0xCB2CU:  // STZ $77
            store(0xCB2CU, zp(0x77U), 0x00U);
            append_checkpoint(0xCB2CU, 0xCB2EU, kZeroPage);
            pc = 0xCB2EU;
            break;

        case 0xCB2EU:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xCB2EU, 0xCB2FU, kImplied);
            pc = 0xCB2FU;
            break;

        case 0xCB2FU:  // LDA $3180,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3180U + registers.x)));
            append_checkpoint(0xCB2FU, 0xCB32U, kAbsoluteIndexed);
            pc = 0xCB32U;
            break;

        case 0xCB32U:  // SBC #$20
            subtract_with_carry(0x20U);
            append_checkpoint(0xCB32U, 0xCB34U, kImmediate);
            pc = 0xCB34U;
            break;

        case 0xCB34U:  // STA $65
            store(0xCB34U, zp(0x65U), registers.accumulator);
            append_checkpoint(0xCB34U, 0xCB36U, kZeroPage);
            pc = 0xCB36U;
            break;

        case 0xCB36U:  // LDA $3300,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3300U + registers.x)));
            append_checkpoint(0xCB36U, 0xCB39U, kAbsoluteIndexed);
            pc = 0xCB39U;
            break;

        case 0xCB39U:  // STA $66
            store(0xCB39U, zp(0x66U), registers.accumulator);
            append_checkpoint(0xCB39U, 0xCB3BU, kZeroPage);
            pc = 0xCB3BU;
            break;

        case 0xCB3BU:  // LDA $3380,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3380U + registers.x)));
            append_checkpoint(0xCB3BU, 0xCB3EU, kAbsoluteIndexed);
            pc = 0xCB3EU;
            break;

        case 0xCB3EU:  // STA $67
            store(0xCB3EU, zp(0x67U), registers.accumulator);
            append_checkpoint(0xCB3EU, 0xCB40U, kZeroPage);
            pc = 0xCB40U;
            break;

        case 0xCB40U:  // LDA #$7C
            load_accumulator(0x7CU);
            append_checkpoint(0xCB40U, 0xCB42U, kImmediate);
            pc = 0xCB42U;
            break;

        case 0xCB42U:  // STA $60
            store(0xCB42U, zp(0x60U), registers.accumulator);
            append_checkpoint(0xCB42U, 0xCB44U, kZeroPage);
            pc = 0xCB44U;
            break;

        case 0xCB44U:  // LDA #$04
            load_accumulator(0x04U);
            append_checkpoint(0xCB44U, 0xCB46U, kImmediate);
            pc = 0xCB46U;
            break;

        // ---- 01:CB71  the sweep tail --------------------------------------
        // 01:CB74 is where the core lands when the BBS2 at 01:CB71 falls
        // through: no candidate was accepted this pass, so the last-contact
        // byte is cleared before the playfield probe runs.
        case 0xCB74U:  // STZ $BD,X
            store(0xCB74U, zp_x(0xBDU), 0x00U);
            append_checkpoint(0xCB74U, 0xCB76U, kZeroPageIndexed);
            pc = 0xCB76U;
            break;

        case 0xCB79U:  // BCC $CB7E
            pc = branch(
                0xCB79U, 0xCB7EU, 0xCB7BU,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCB81U:  // LDA $3800,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3800U + registers.x)));
            append_checkpoint(0xCB81U, 0xCB84U, kAbsoluteIndexed);
            pc = 0xCB84U;
            break;

        case 0xCB84U:  // ORA #$80
            or_accumulator(0x80U);
            append_checkpoint(0xCB84U, 0xCB86U, kImmediate);
            pc = 0xCB86U;
            break;

        case 0xCB86U:  // STA $3800,X
            store(
                0xCB86U,
                static_cast<std::uint16_t>(0x3800U + registers.x),
                registers.accumulator);
            append_checkpoint(0xCB86U, 0xCB89U, kAbsoluteIndexed);
            pc = 0xCB89U;
            break;

        case 0xCB8AU:  // LDA $3800,X
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3800U + registers.x)));
            append_checkpoint(0xCB8AU, 0xCB8DU, kAbsoluteIndexed);
            pc = 0xCB8DU;
            break;

        case 0xCB8DU:  // AND #$7F
            and_accumulator(0x7FU);
            append_checkpoint(0xCB8DU, 0xCB8FU, kImmediate);
            pc = 0xCB8FU;
            break;

        case 0xCB8FU:  // STA $3800,X
            store(
                0xCB8FU,
                static_cast<std::uint16_t>(0x3800U + registers.x),
                registers.accumulator);
            append_checkpoint(0xCB8FU, 0xCB92U, kAbsoluteIndexed);
            pc = 0xCB92U;
            break;

        // ---- 01:CBAB  the single-slot probe -------------------------------
        case 0xCBABU:  // LDA $3C80
            load_accumulator(read_ram(0x3C80U));
            append_checkpoint(0xCBABU, 0xCBAEU, kAbsolute);
            pc = 0xCBAEU;
            break;

        case 0xCBAEU:  // AND #$01
            and_accumulator(0x01U);
            append_checkpoint(0xCBAEU, 0xCBB0U, kImmediate);
            pc = 0xCBB0U;
            break;

        case 0xCBB0U:  // BEQ $CBB3
            pc = branch(
                0xCBB0U, 0xCBB3U, 0xCBB2U,
                (registers.status & kZero) != 0U);
            break;

        // $77 starts at $80 so the sweep's own flags read as "single slot",
        // and the slot index is parked in $64 while X and Y trade places.
        case 0xCBB6U:  // LDA #$80
            load_accumulator(0x80U);
            append_checkpoint(0xCBB6U, 0xCBB8U, kImmediate);
            pc = 0xCBB8U;
            break;

        case 0xCBB8U:  // STA $77
            store(0xCBB8U, zp(0x77U), registers.accumulator);
            append_checkpoint(0xCBB8U, 0xCBBAU, kZeroPage);
            pc = 0xCBBAU;
            break;

        case 0xCBBAU:  // STX $64
            store(0xCBBAU, zp(0x64U), registers.x);
            append_checkpoint(0xCBBAU, 0xCBBCU, kZeroPage);
            pc = 0xCBBCU;
            break;

        case 0xCBBCU:  // LDY #$00
            load_y(0x00U);
            append_checkpoint(0xCBBCU, 0xCBBEU, kImmediate);
            pc = 0xCBBEU;
            break;

        case 0xCBBEU: {  // SXY
            const auto swapped = registers.x;
            registers.x = registers.y;
            registers.y = swapped;
            append_checkpoint(0xCBBEU, 0xCBBFU, kSwapXY);
            pc = 0xCBBFU;
            break;
        }

        case 0xCBC2U:  // BCS $CBC7
            pc = branch(
                0xCBC2U, 0xCBC7U, 0xCBC4U,
                (registers.status & kCarry) != 0U);
            break;

        case 0xCBC4U:  // LDX $64
            load_x(read_ram(zp(0x64U)));
            append_checkpoint(0xCBC4U, 0xCBC6U, kZeroPage);
            pc = 0xCBC6U;
            break;

        case 0xCBCAU:  // STZ $BD,X
            store(0xCBCAU, zp_x(0xBDU), 0x00U);
            append_checkpoint(0xCBCAU, 0xCBCCU, kZeroPageIndexed);
            pc = 0xCBCCU;
            break;

        case 0xCBCCU:  // LDX $64
            load_x(read_ram(zp(0x64U)));
            append_checkpoint(0xCBCCU, 0xCBCEU, kZeroPage);
            pc = 0xCBCEU;
            break;

        // ---- 01:CBCF  the confirmed-contact dispatcher ---------------------
        // A contact against a slot the ball did not touch last time marks the
        // target and records it, unless $78 already says the contact is
        // handled.
        case 0xCBCFU:  // LDY $64
            load_y(read_ram(zp(0x64U)));
            append_checkpoint(0xCBCFU, 0xCBD1U, kZeroPage);
            pc = 0xCBD1U;
            break;

        case 0xCBD1U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0xCBD1U, 0xCBD2U, kImplied);
            pc = 0xCBD2U;
            break;

        case 0xCBD2U:  // CMP $BD,X
            registers.status = compare(
                registers.status,
                registers.accumulator,
                read_ram(zp_x(0xBDU)));
            append_checkpoint(0xCBD2U, 0xCBD4U, kZeroPageIndexed);
            pc = 0xCBD4U;
            break;

        case 0xCBD4U:  // BEQ $CBE5
            pc = branch(
                0xCBD4U, 0xCBE5U, 0xCBD6U,
                (registers.status & kZero) != 0U);
            break;

        case 0xCBD6U:  // LDA $78
            load_accumulator(read_ram(zp(0x78U)));
            append_checkpoint(0xCBD6U, 0xCBD8U, kZeroPage);
            pc = 0xCBD8U;
            break;

        case 0xCBD8U:  // BMI $CBE5
            pc = branch(
                0xCBD8U, 0xCBE5U, 0xCBDAU,
                (registers.status & kNegative) != 0U);
            break;

        case 0xCBDAU:  // LDA $3800,Y
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3800U + registers.y)));
            append_checkpoint(0xCBDAU, 0xCBDDU, kAbsoluteIndexed);
            pc = 0xCBDDU;
            break;

        case 0xCBDDU:  // ORA #$80
            or_accumulator(0x80U);
            append_checkpoint(0xCBDDU, 0xCBDFU, kImmediate);
            pc = 0xCBDFU;
            break;

        case 0xCBDFU:  // STA $3800,Y
            store(
                0xCBDFU,
                static_cast<std::uint16_t>(0x3800U + registers.y),
                registers.accumulator);
            append_checkpoint(0xCBDFU, 0xCBE2U, kAbsoluteIndexed);
            pc = 0xCBE2U;
            break;

        case 0xCBE2U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0xCBE2U, 0xCBE3U, kImplied);
            pc = 0xCBE3U;
            break;

        case 0xCBE3U:  // STA $BD,X
            store(0xCBE3U, zp_x(0xBDU), registers.accumulator);
            append_checkpoint(0xCBE3U, 0xCBE5U, kZeroPageIndexed);
            pc = 0xCBE5U;
            break;

        // The low six bits of the slot state select the response vector. Types
        // below 2 leave bit 7 of $78 alone; everything else clears it, which
        // is what lets a later contact in the same frame be handled.
        case 0xCBE5U:  // LDA $3000,Y
            load_accumulator(read_ram(
                static_cast<std::uint16_t>(0x3000U + registers.y)));
            append_checkpoint(0xCBE5U, 0xCBE8U, kAbsoluteIndexed);
            pc = 0xCBE8U;
            break;

        case 0xCBE8U:  // AND #$3F
            and_accumulator(0x3FU);
            append_checkpoint(0xCBE8U, 0xCBEAU, kImmediate);
            pc = 0xCBEAU;
            break;

        case 0xCBEAU:  // TAY
            load_y(registers.accumulator);
            append_checkpoint(0xCBEAU, 0xCBEBU, kImplied);
            pc = 0xCBEBU;
            break;

        case 0xCBEBU:  // CMP #$02
            registers.status =
                compare(registers.status, registers.accumulator, 0x02U);
            append_checkpoint(0xCBEBU, 0xCBEDU, kImmediate);
            pc = 0xCBEDU;
            break;

        case 0xCBEDU:  // BCC $CBF1
            pc = branch(
                0xCBEDU, 0xCBF1U, 0xCBEFU,
                (registers.status & kCarry) == 0U);
            break;

        case 0xCBEFU:  // RMB7 $78
            reset_memory_bit(0xCBEFU, 0x78U, 7U);
            append_checkpoint(0xCBEFU, 0xCBF1U, kZeroPageBitSet);
            pc = 0xCBF1U;
            break;

        // The tail dispatch: push the vector's high byte then its low byte and
        // let the RTS jump there. Both tables are in this bank's ROM page.
        case 0xCBF1U:  // LDA $CBFF,Y
            load_accumulator(read_mapped(
                static_cast<std::uint16_t>(0xCBFFU + registers.y)));
            append_checkpoint(0xCBF1U, 0xCBF4U, kAbsoluteIndexed);
            pc = 0xCBF4U;
            break;

        case 0xCBF4U:  // PHA
            push(0xCBF4U, registers.accumulator);
            append_checkpoint(0xCBF4U, 0xCBF5U, kPushAccumulator);
            pc = 0xCBF5U;
            break;

        case 0xCBF5U:  // LDA $CBFA,Y
            load_accumulator(read_mapped(
                static_cast<std::uint16_t>(0xCBFAU + registers.y)));
            append_checkpoint(0xCBF5U, 0xCBF8U, kAbsoluteIndexed);
            pc = 0xCBF8U;
            break;

        case 0xCBF8U:  // PHA
            push(0xCBF8U, registers.accumulator);
            append_checkpoint(0xCBF8U, 0xCBF9U, kPushAccumulator);
            pc = 0xCBF9U;
            break;

        // ---- 01:CD74  the playfield-tile probe ----------------------------
        // $65 is the horizontal position: its low three bits pick the column
        // within a tile and the rest, shifted down, the tile column.
        case 0xCD74U:  // LDA $65
            load_accumulator(read_ram(zp(0x65U)));
            append_checkpoint(0xCD74U, 0xCD76U, kZeroPage);
            pc = 0xCD76U;
            break;

        case 0xCD76U:  // STA $2D
            store(0xCD76U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCD76U, 0xCD78U, kZeroPage);
            pc = 0xCD78U;
            break;

        case 0xCD78U:  // AND #$07
            and_accumulator(0x07U);
            append_checkpoint(0xCD78U, 0xCD7AU, kImmediate);
            pc = 0xCD7AU;
            break;

        case 0xCD7AU:  // STA $5E
            store(0xCD7AU, zp(0x5EU), registers.accumulator);
            append_checkpoint(0xCD7AU, 0xCD7CU, kZeroPage);
            pc = 0xCD7CU;
            break;

        case 0xCD7CU:  // LSR $2D
            shift_memory_right(0xCD7CU, 0x2DU);
            append_checkpoint(0xCD7CU, 0xCD7EU, kZeroPageReadModifyWrite);
            pc = 0xCD7EU;
            break;

        case 0xCD7EU:  // LSR $2D
            shift_memory_right(0xCD7EU, 0x2DU);
            append_checkpoint(0xCD7EU, 0xCD80U, kZeroPageReadModifyWrite);
            pc = 0xCD80U;
            break;

        case 0xCD80U:  // LSR $2D
            shift_memory_right(0xCD80U, 0x2DU);
            append_checkpoint(0xCD80U, 0xCD82U, kZeroPageReadModifyWrite);
            pc = 0xCD82U;
            break;

        // $66/$67 is the vertical position; the playfield starts $20 above it.
        case 0xCD82U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xCD82U, 0xCD83U, kImplied);
            pc = 0xCD83U;
            break;

        case 0xCD83U:  // LDA $66
            load_accumulator(read_ram(zp(0x66U)));
            append_checkpoint(0xCD83U, 0xCD85U, kZeroPage);
            pc = 0xCD85U;
            break;

        case 0xCD85U:  // SBC #$20
            subtract_with_carry(0x20U);
            append_checkpoint(0xCD85U, 0xCD87U, kImmediate);
            pc = 0xCD87U;
            break;

        case 0xCD87U:  // STA $2F
            store(0xCD87U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCD87U, 0xCD89U, kZeroPage);
            pc = 0xCD89U;
            break;

        case 0xCD89U:  // LDA $67
            load_accumulator(read_ram(zp(0x67U)));
            append_checkpoint(0xCD89U, 0xCD8BU, kZeroPage);
            pc = 0xCD8BU;
            break;

        case 0xCD8BU:  // SBC #$00
            subtract_with_carry(0x00U);
            append_checkpoint(0xCD8BU, 0xCD8DU, kImmediate);
            pc = 0xCD8DU;
            break;

        case 0xCD8DU:  // STA $30
            store(0xCD8DU, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCD8DU, 0xCD8FU, kZeroPage);
            pc = 0xCD8FU;
            break;

        case 0xCD8FU:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xCD8FU, 0xCD91U, kZeroPage);
            pc = 0xCD91U;
            break;

        case 0xCD91U:  // AND #$07
            and_accumulator(0x07U);
            append_checkpoint(0xCD91U, 0xCD93U, kImmediate);
            pc = 0xCD93U;
            break;

        case 0xCD93U:  // STA $5F
            store(0xCD93U, zp(0x5FU), registers.accumulator);
            append_checkpoint(0xCD93U, 0xCD95U, kZeroPage);
            pc = 0xCD95U;
            break;

        // The row number times 32 -- two shifts of the masked byte carrying
        // into $30 -- plus the tile column is the offset of this tile in the
        // map, which is then biased by the two table pointers.
        case 0xCD95U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xCD95U, 0xCD97U, kZeroPage);
            pc = 0xCD97U;
            break;

        case 0xCD97U:  // AND #$F8
            and_accumulator(0xF8U);
            append_checkpoint(0xCD97U, 0xCD99U, kImmediate);
            pc = 0xCD99U;
            break;

        case 0xCD99U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xCD99U, 0xCD9AU, kImplied);
            pc = 0xCD9AU;
            break;

        case 0xCD9AU:  // ROL $30
            rotate_memory_left(0xCD9AU, 0x30U);
            append_checkpoint(0xCD9AU, 0xCD9CU, kZeroPageReadModifyWrite);
            pc = 0xCD9CU;
            break;

        case 0xCD9CU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xCD9CU, 0xCD9DU, kImplied);
            pc = 0xCD9DU;
            break;

        case 0xCD9DU:  // ROL $30
            rotate_memory_left(0xCD9DU, 0x30U);
            append_checkpoint(0xCD9DU, 0xCD9FU, kZeroPageReadModifyWrite);
            pc = 0xCD9FU;
            break;

        case 0xCD9FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xCD9FU, 0xCDA0U, kImplied);
            pc = 0xCDA0U;
            break;

        case 0xCDA0U:  // ADC $2D
            add_with_carry(read_ram(zp(0x2DU)));
            append_checkpoint(0xCDA0U, 0xCDA2U, kZeroPage);
            pc = 0xCDA2U;
            break;

        case 0xCDA2U:  // STA $2F
            store(0xCDA2U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCDA2U, 0xCDA4U, kZeroPage);
            pc = 0xCDA4U;
            break;

        case 0xCDA4U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xCDA4U, 0xCDA6U, kZeroPage);
            pc = 0xCDA6U;
            break;

        case 0xCDA6U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xCDA6U, 0xCDA8U, kImmediate);
            pc = 0xCDA8U;
            break;

        case 0xCDA8U:  // STA $30
            store(0xCDA8U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCDA8U, 0xCDAAU, kZeroPage);
            pc = 0xCDAAU;
            break;

        case 0xCDAAU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xCDAAU, 0xCDABU, kImplied);
            pc = 0xCDABU;
            break;

        case 0xCDABU:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xCDABU, 0xCDADU, kZeroPage);
            pc = 0xCDADU;
            break;

        case 0xCDADU:  // ADC $C8
            add_with_carry(read_ram(zp(0xC8U)));
            append_checkpoint(0xCDADU, 0xCDAFU, kZeroPage);
            pc = 0xCDAFU;
            break;

        case 0xCDAFU:  // STA $2D
            store(0xCDAFU, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xCDAFU, 0xCDB1U, kZeroPage);
            pc = 0xCDB1U;
            break;

        case 0xCDB1U:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xCDB1U, 0xCDB3U, kZeroPage);
            pc = 0xCDB3U;
            break;

        case 0xCDB3U:  // ADC $C9
            add_with_carry(read_ram(zp(0xC9U)));
            append_checkpoint(0xCDB3U, 0xCDB5U, kZeroPage);
            pc = 0xCDB5U;
            break;

        case 0xCDB5U:  // STA $2E
            store(0xCDB5U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xCDB5U, 0xCDB7U, kZeroPage);
            pc = 0xCDB7U;
            break;

        case 0xCDB7U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xCDB7U, 0xCDB8U, kImplied);
            pc = 0xCDB8U;
            break;

        case 0xCDB8U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xCDB8U, 0xCDBAU, kZeroPage);
            pc = 0xCDBAU;
            break;

        case 0xCDBAU:  // ADC $C6
            add_with_carry(read_ram(zp(0xC6U)));
            append_checkpoint(0xCDBAU, 0xCDBCU, kZeroPage);
            pc = 0xCDBCU;
            break;

        case 0xCDBCU:  // STA $2F
            store(0xCDBCU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCDBCU, 0xCDBEU, kZeroPage);
            pc = 0xCDBEU;
            break;

        case 0xCDBEU:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xCDBEU, 0xCDC0U, kZeroPage);
            pc = 0xCDC0U;
            break;

        case 0xCDC0U:  // ADC $C7
            add_with_carry(read_ram(zp(0xC7U)));
            append_checkpoint(0xCDC0U, 0xCDC2U, kZeroPage);
            pc = 0xCDC2U;
            break;

        case 0xCDC2U:  // STA $30
            store(0xCDC2U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCDC2U, 0xCDC4U, kZeroPage);
            pc = 0xCDC4U;
            break;

        case 0xCDC4U:  // CLY -- clears Y and no flag
            registers.y = 0x00U;
            append_checkpoint(0xCDC4U, 0xCDC5U, kImplied);
            pc = 0xCDC5U;
            break;

        // Two parallel maps: ($2D) holds the tile id and ($2F) its attribute.
        case 0xCDC5U:  // LDA ($2D),Y
            load_accumulator(read_mapped(indirect_y(0x2DU)));
            append_checkpoint(0xCDC5U, 0xCDC7U, kIndirectIndexed);
            pc = 0xCDC7U;
            break;

        case 0xCDC7U:  // STA $26
            store(0xCDC7U, zp(0x26U), registers.accumulator);
            append_checkpoint(0xCDC7U, 0xCDC9U, kZeroPage);
            pc = 0xCDC9U;
            break;

        case 0xCDC9U:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xCDC9U, 0xCDCBU, kIndirectIndexed);
            pc = 0xCDCBU;
            break;

        // The attribute times eight is the row offset of this tile's eight-byte
        // solidity mask, biased by the mask table pointer in $90/$91.
        case 0xCDCBU:  // STZ $30
            store(0xCDCBU, zp(0x30U), 0x00U);
            append_checkpoint(0xCDCBU, 0xCDCDU, kZeroPage);
            pc = 0xCDCDU;
            break;

        case 0xCDCDU:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xCDCDU, 0xCDCEU, kImplied);
            pc = 0xCDCEU;
            break;

        case 0xCDCEU:  // ROL $30
            rotate_memory_left(0xCDCEU, 0x30U);
            append_checkpoint(0xCDCEU, 0xCDD0U, kZeroPageReadModifyWrite);
            pc = 0xCDD0U;
            break;

        case 0xCDD0U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xCDD0U, 0xCDD1U, kImplied);
            pc = 0xCDD1U;
            break;

        case 0xCDD1U:  // ROL $30
            rotate_memory_left(0xCDD1U, 0x30U);
            append_checkpoint(0xCDD1U, 0xCDD3U, kZeroPageReadModifyWrite);
            pc = 0xCDD3U;
            break;

        case 0xCDD3U:  // ASL A
            shift_accumulator_left();
            append_checkpoint(0xCDD3U, 0xCDD4U, kImplied);
            pc = 0xCDD4U;
            break;

        case 0xCDD4U:  // ROL $30
            rotate_memory_left(0xCDD4U, 0x30U);
            append_checkpoint(0xCDD4U, 0xCDD6U, kZeroPageReadModifyWrite);
            pc = 0xCDD6U;
            break;

        case 0xCDD6U:  // STA $2F
            store(0xCDD6U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCDD6U, 0xCDD8U, kZeroPage);
            pc = 0xCDD8U;
            break;

        case 0xCDD8U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xCDD8U, 0xCDD9U, kImplied);
            pc = 0xCDD9U;
            break;

        case 0xCDD9U:  // LDA $2F
            load_accumulator(read_ram(zp(0x2FU)));
            append_checkpoint(0xCDD9U, 0xCDDBU, kZeroPage);
            pc = 0xCDDBU;
            break;

        case 0xCDDBU:  // ADC $90
            add_with_carry(read_ram(zp(0x90U)));
            append_checkpoint(0xCDDBU, 0xCDDDU, kZeroPage);
            pc = 0xCDDDU;
            break;

        case 0xCDDDU:  // STA $2F
            store(0xCDDDU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xCDDDU, 0xCDDFU, kZeroPage);
            pc = 0xCDDFU;
            break;

        case 0xCDDFU:  // LDA $30
            load_accumulator(read_ram(zp(0x30U)));
            append_checkpoint(0xCDDFU, 0xCDE1U, kZeroPage);
            pc = 0xCDE1U;
            break;

        case 0xCDE1U:  // ADC $91
            add_with_carry(read_ram(zp(0x91U)));
            append_checkpoint(0xCDE1U, 0xCDE3U, kZeroPage);
            pc = 0xCDE3U;
            break;

        case 0xCDE3U:  // STA $30
            store(0xCDE3U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xCDE3U, 0xCDE5U, kZeroPage);
            pc = 0xCDE5U;
            break;

        // Row $5F of the mask, column $5E of the row: a set bit is solid.
        case 0xCDE5U:  // LDY $5F
            load_y(read_ram(zp(0x5FU)));
            append_checkpoint(0xCDE5U, 0xCDE7U, kZeroPage);
            pc = 0xCDE7U;
            break;

        case 0xCDE7U:  // LDA ($2F),Y
            load_accumulator(read_mapped(indirect_y(0x2FU)));
            append_checkpoint(0xCDE7U, 0xCDE9U, kIndirectIndexed);
            pc = 0xCDE9U;
            break;

        case 0xCDE9U:  // LDY $5E
            load_y(read_ram(zp(0x5EU)));
            append_checkpoint(0xCDE9U, 0xCDEBU, kZeroPage);
            pc = 0xCDEBU;
            break;

        case 0xCDEBU:  // AND $CF2B,Y
            and_accumulator(read_mapped(
                static_cast<std::uint16_t>(0xCF2BU + registers.y)));
            append_checkpoint(0xCDEBU, 0xCDEEU, kAbsoluteIndexed);
            pc = 0xCDEEU;
            break;

        case 0xCDEEU:  // BNE $CDF2
            pc = branch(
                0xCDEEU, 0xCDF2U, 0xCDF0U,
                (registers.status & kZero) == 0U);
            break;

        case 0xCDF0U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0xCDF0U, 0xCDF1U, kImplied);
            pc = 0xCDF1U;
            break;

        case 0xCDF2U:  // LDA $26
            load_accumulator(read_ram(zp(0x26U)));
            append_checkpoint(0xCDF2U, 0xCDF4U, kZeroPage);
            pc = 0xCDF4U;
            break;

        case 0xCDF4U:  // STA $78
            store(0xCDF4U, zp(0x78U), registers.accumulator);
            append_checkpoint(0xCDF4U, 0xCDF6U, kZeroPage);
            pc = 0xCDF6U;
            break;

        case 0xCDF6U:  // SEC
            registers.status =
                static_cast<std::uint8_t>(registers.status | kCarry);
            append_checkpoint(0xCDF6U, 0xCDF7U, kImplied);
            pc = 0xCDF7U;
            break;

        default:
            return std::nullopt;
        }
    }
}

} // namespace devils_crush::app::detail
