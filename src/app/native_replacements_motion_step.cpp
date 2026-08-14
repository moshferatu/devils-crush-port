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

// 00:F08E-00:F1B3, the shared motion step: advance one actor's position and
// velocity by whichever of the four axes its flags ask for, and report in the
// carry whether it left the playfield.
//
// `$3800,X` is the actor's motion-enable byte and the head is a four-way
// dispatcher on its bottom four bits, each guarding one call:
//
//   bit 0 -> 00:F0C0  integrate the vertical position
//   bit 1 -> 00:F104  integrate the horizontal position
//   bit 2 -> 00:F148  step the vertical velocity toward its target
//   bit 3 -> 00:F17E  step the horizontal velocity toward its target
//
// Position is a 24-bit fixed-point triple -- `$3100/$3180/$3200` horizontally
// and `$3280/$3300/$3380` vertically, sub-pixel first -- and velocity a signed
// 16-bit pair, `$3400/$3480` and `$3500/$3580`. The two integrators add the
// velocity into the position with the sign extended through `$2B`, then stage
// the resulting whole-pixel word into `$2F/$30` and a bound into `$2D/$2E` and
// call the registered 16-bit comparison at 00:E93C: $0140 across and $02D0
// down, the playfield's two dimensions. Out of bounds -- or a position that
// went negative, which 00:F0E7 and 00:F12B catch a compare earlier -- bumps
// `$26`, and the head turns `$26` into the carry it returns. The two velocity
// leaves are the same routine twice over: compare the whole-pixel position
// against the target in `$3700,X`/`$3780,X` and add or subtract the per-frame
// step in `$3600,X`/`$3680,X`, sixteen bits at a time.
//
// It is the most widely shared routine ported so far. 924 of its 1,226 calls
// arrive through the bank-01 trampoline at 01:C012, which sixteen sites across
// banks 02 to 07 reach it by, and the other 302 from eight `JSR`s in bank 09;
// the ball itself does not use it, having its own integrator at 00:F1B4.
//
// Eleven entry points. Four of them are this family's own leaves, which is new
// -- the head crosses into routines it owns rather than into somebody else's --
// and the rest are the addresses its six calls come back to:
//
//   00:F08E  the head, and the only entry reached from outside the family.
//   00:F09A  after `JSR $F0C0`, the vertical integrator.
//   00:F0A4  after `JSR $F104`, the horizontal integrator.
//   00:F0AE  after `JSR $F148`, the vertical velocity step.
//   00:F0B8  after `JSR $F17E`, the horizontal velocity step, and the tail.
//   00:F0C0  the vertical integrator, crossed into from 00:F097.
//   00:F0FE  after the vertical integrator's `JSR $E93C`.
//   00:F104  the horizontal integrator, crossed into from 00:F0A1.
//   00:F142  after the horizontal integrator's `JSR $E93C`.
//   00:F148  the vertical velocity step, crossed into from 00:F0AB.
//   00:F17E  the horizontal velocity step, crossed into from 00:F0B5.
//
// The four return addresses are entries even though a leaf hands the machine
// straight back to them: absorbing a return is conditional, and if the
// condition ever failed the core would emulate the `RTS` and arrive there
// itself. Owning them costs nothing and makes every arm recoverable.
//
// Nothing in the range is left interpreted. Every instruction the route
// executes is transcribed, the five returns it takes are absorbed, and the
// three arms it has never taken are handed back.
constexpr std::uint8_t kBank = 0x00U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
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
constexpr std::uint32_t kAbsoluteIndexed = 15U;
constexpr std::uint32_t kZeroPageReadModifyWrite = 18U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;
// BRA is a branch whose condition is a constant true, and it costs what any
// taken branch costs.
constexpr std::uint32_t kBranchAlways = 12U;
// JSR is seven CPU cycles: both return bytes pushed, then ADDCYC(6) and the
// LastCycle. Both writes carry the instruction's start offset, since the core
// pushes before it charges anything.
constexpr std::uint32_t kCall = 21U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status,
    const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    return static_cast<std::uint8_t>(
        result | static_cast<std::uint8_t>(value & kNegative));
}

[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0xF08EU || logical_pc == 0xF09AU
        || logical_pc == 0xF0A4U || logical_pc == 0xF0AEU
        || logical_pc == 0xF0B8U || logical_pc == 0xF0C0U
        || logical_pc == 0xF0FEU || logical_pc == 0xF104U
        || logical_pc == 0xF142U || logical_pc == 0xF148U
        || logical_pc == 0xF17EU;
}

// The five returns this family takes, each of them the last instruction of a
// routine the walk entered at its first. A walk reaches one of these with the
// stack exactly as it found it -- the only pushes here belong to a crossed
// `JSR`, and crossing ends the walk -- so the frame the return reads is the one
// its caller wrote.
[[nodiscard]] constexpr bool is_absorbable_return(const std::uint16_t logical_pc)
{
    return logical_pc == 0xF0BFU || logical_pc == 0xF103U
        || logical_pc == 0xF147U || logical_pc == 0xF17DU
        || logical_pc == 0xF1B3U;
}

// Where each walk stops: the five returns above, and three arms this route has
// never taken.
//
// 00:F0BC is the head reporting a departure, and 00:F100 and 00:F144 are the
// two integrators declaring one. They are the same fact told twice, and in
// 3,600 frames no actor left the playfield through this routine: `$26` was zero
// at 00:F0B8 on all 1,226 calls, 00:F0E7 and 00:F12B never found a negative
// position, and 00:F0FE and 00:F142 took the in-bounds arm every time. Handing
// them back costs nothing measurable and keeps the walk from claiming a path it
// has never seen. It also puts 00:F0BD, 00:F102 and 00:F146 out of reach, which
// is why those three returns are not in the absorbable set.
[[nodiscard]] constexpr bool is_stop_instruction(const std::uint16_t logical_pc)
{
    return is_absorbable_return(logical_pc)
        || logical_pc == 0xF0BCU || logical_pc == 0xF100U
        || logical_pc == 0xF144U;
}

} // namespace

std::optional<RoutineReplacement> replace_actor_motion_step(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Decimal matters here and is refused rather than modelled: both
    // integrators and both velocity steps are `ADC`/`SBC` chains, and a decimal
    // add of a fixed-point position is not something this routine is written
    // to survive either. The memory-operand T flag would take the left-hand
    // side of the four `AND`s out of zero page instead of the accumulator, and
    // is refused with it.
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

    // Zero page is the first 256 bytes of work RAM, because MPR1 holds F8 on
    // every entry this family is reached from.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };

    // The actor index. Every array here is a page of one byte per actor.
    const auto indexed = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(base + registers.x);
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

    // A relative branch costs a cycle more when it is taken, and the address it
    // resumes at is the only thing that distinguishes the two arms.
    const auto branch =
        [&append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t taken_pc,
            const std::uint16_t fallthrough_pc,
            const bool condition) {
            const auto next = condition ? taken_pc : fallthrough_pc;
            append_checkpoint(
                instruction_pc, next,
                condition ? kBranchTaken : kBranchNotTaken);
            return next;
        };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto and_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator =
            static_cast<std::uint8_t>(registers.accumulator & value);
        registers.status = set_nz(registers.status, registers.accumulator);
    };
    // CMP sets the same three flags a subtraction would but keeps the
    // accumulator, and leaves the overflow bit alone.
    const auto compare_accumulator = [&registers](const std::uint8_t value) {
        const auto result =
            static_cast<std::uint8_t>(registers.accumulator - value);
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if (registers.accumulator >= value) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        registers.status = set_nz(status, result);
    };
    const auto set_carry = [&registers](const bool value) {
        registers.status = value
            ? static_cast<std::uint8_t>(registers.status | kCarry)
            : static_cast<std::uint8_t>(
                  registers.status & static_cast<std::uint8_t>(~kCarry));
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

    // DEC on memory reports on the result and leaves the carry alone.
    const auto decrement_memory =
        [&registers, &read_ram, &store](
            const std::uint16_t instruction_pc,
            const std::uint16_t address) {
            const auto value =
                static_cast<std::uint8_t>(read_ram(address) - 1U);
            store(instruction_pc, address, value);
            registers.status = set_nz(registers.status, value);
        };

    // A call the walk makes rather than hands back. The two return bytes are
    // pushed before the instruction charges a cycle, so both writes carry its
    // start offset, and the transcript ends on the callee's first instruction
    // with whatever runs there picking the body up. Four of the six land on an
    // entry of this family and the other two on the registered helper at
    // 00:E93C, so every one of them hands over to a transcript rather than to
    // the interpreter.
    std::optional<RoutineKey> crossed;
    const auto call =
        [&registers, &cycle, &push, &commit_pending_writes, &append_checkpoint,
         &crossed](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc) {
            const auto pushed = static_cast<std::uint16_t>(instruction_pc + 2U);
            push(instruction_pc, static_cast<std::uint8_t>(pushed >> 8U));
            push(instruction_pc, static_cast<std::uint8_t>(pushed & 0x00FFU));
            commit_pending_writes(cycle);
            append_checkpoint(instruction_pc, target_pc, kCall);
            crossed = RoutineKey{
                .physical_bank = registers.mpr[target_pc >> 13U],
                .logical_pc = target_pc,
            };
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        // The core's channels are 512 writes and 2,048 checkpoints. The longest
        // walk here is the vertical integrator at twenty-five instructions and
        // seven writes, so exceeding either means the walk has gone somewhere
        // it was not written for and the assembly should run.
        if (overflow
            || replacement.checkpoints.size() > 64U
            || replacement.work_ram_writes.size() > 32U) {
            return std::nullopt;
        }

        if (is_stop_instruction(pc)) {
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;

            // The stack pointer is tested rather than assumed. No walk here
            // reaches a return having pushed -- the only pushes belong to a
            // crossed `JSR`, which ends the walk two instructions earlier --
            // but a return read from the wrong end of a frame is the one
            // mistake here that would still look correct.
            //
            // The atomic question cannot fail: no stop is an entry, so the
            // walk has crossed at least one instruction and emitted at least
            // one checkpoint. It is asked anyway, because what makes it
            // unnecessary is a property of is_owned_entry rather than of this
            // loop.
            if (is_absorbable_return(pc)
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
        // ---- 00:F08E  the dispatcher ---------------------------------------
        // `$26` is the departure flag the leaves raise and the tail reports.
        case 0xF08EU:  // STZ $26
            store(0xF08EU, zp(0x26U), 0x00U);
            append_checkpoint(0xF08EU, 0xF090U, kZeroPage);
            pc = 0xF090U;
            break;

        case 0xF090U:  // LDA $3800,X
            load_accumulator(read_ram(indexed(0x3800U)));
            append_checkpoint(0xF090U, 0xF093U, kAbsoluteIndexed);
            pc = 0xF093U;
            break;

        case 0xF093U:  // AND #$01 -- integrate the vertical position?
            and_accumulator(0x01U);
            append_checkpoint(0xF093U, 0xF095U, kImmediate);
            pc = 0xF095U;
            break;

        case 0xF095U:  // BEQ $F09A
            pc = branch(
                0xF095U, 0xF09AU, 0xF097U,
                (registers.status & kZero) != 0U);
            break;

        case 0xF097U:  // JSR $F0C0 -- into this family's vertical integrator
            call(0xF097U, 0xF0C0U);
            break;

        // ---- 00:F09A  back from the vertical integrator --------------------
        case 0xF09AU:  // LDA $3800,X
            load_accumulator(read_ram(indexed(0x3800U)));
            append_checkpoint(0xF09AU, 0xF09DU, kAbsoluteIndexed);
            pc = 0xF09DU;
            break;

        case 0xF09DU:  // AND #$02 -- integrate the horizontal position?
            and_accumulator(0x02U);
            append_checkpoint(0xF09DU, 0xF09FU, kImmediate);
            pc = 0xF09FU;
            break;

        case 0xF09FU:  // BEQ $F0A4
            pc = branch(
                0xF09FU, 0xF0A4U, 0xF0A1U,
                (registers.status & kZero) != 0U);
            break;

        case 0xF0A1U:  // JSR $F104 -- into the horizontal integrator
            call(0xF0A1U, 0xF104U);
            break;

        // ---- 00:F0A4  back from the horizontal integrator ------------------
        case 0xF0A4U:  // LDA $3800,X
            load_accumulator(read_ram(indexed(0x3800U)));
            append_checkpoint(0xF0A4U, 0xF0A7U, kAbsoluteIndexed);
            pc = 0xF0A7U;
            break;

        case 0xF0A7U:  // AND #$04 -- step the vertical velocity?
            and_accumulator(0x04U);
            append_checkpoint(0xF0A7U, 0xF0A9U, kImmediate);
            pc = 0xF0A9U;
            break;

        case 0xF0A9U:  // BEQ $F0AE
            pc = branch(
                0xF0A9U, 0xF0AEU, 0xF0ABU,
                (registers.status & kZero) != 0U);
            break;

        case 0xF0ABU:  // JSR $F148 -- into the vertical velocity step
            call(0xF0ABU, 0xF148U);
            break;

        // ---- 00:F0AE  back from the vertical velocity step -----------------
        case 0xF0AEU:  // LDA $3800,X
            load_accumulator(read_ram(indexed(0x3800U)));
            append_checkpoint(0xF0AEU, 0xF0B1U, kAbsoluteIndexed);
            pc = 0xF0B1U;
            break;

        case 0xF0B1U:  // AND #$08 -- step the horizontal velocity?
            and_accumulator(0x08U);
            append_checkpoint(0xF0B1U, 0xF0B3U, kImmediate);
            pc = 0xF0B3U;
            break;

        case 0xF0B3U:  // BEQ $F0B8
            pc = branch(
                0xF0B3U, 0xF0B8U, 0xF0B5U,
                (registers.status & kZero) != 0U);
            break;

        case 0xF0B5U:  // JSR $F17E -- into the horizontal velocity step
            call(0xF0B5U, 0xF17EU);
            break;

        // ---- 00:F0B8  the tail: report a departure in the carry ------------
        case 0xF0B8U:  // LDA $26
            load_accumulator(read_ram(zp(0x26U)));
            append_checkpoint(0xF0B8U, 0xF0BAU, kZeroPage);
            pc = 0xF0BAU;
            break;

        case 0xF0BAU:  // BEQ $F0BE -- clear on all 1,226 calls
            pc = branch(
                0xF0BAU, 0xF0BEU, 0xF0BCU,
                (registers.status & kZero) != 0U);
            break;

        case 0xF0BEU:  // CLC -- still on the playfield
            set_carry(false);
            append_checkpoint(0xF0BEU, 0xF0BFU, kImplied);
            pc = 0xF0BFU;
            break;

        // ---- 00:F0C0  the vertical integrator ------------------------------
        // `$2B` is the sign extension of the 16-bit velocity into the third
        // byte of the position: zero for a fall, $FF for a rise.
        case 0xF0C0U:  // STZ $2B
            store(0xF0C0U, zp(0x2BU), 0x00U);
            append_checkpoint(0xF0C0U, 0xF0C2U, kZeroPage);
            pc = 0xF0C2U;
            break;

        case 0xF0C2U:  // LDA $3580,X -- the velocity's high byte
            load_accumulator(read_ram(indexed(0x3580U)));
            append_checkpoint(0xF0C2U, 0xF0C5U, kAbsoluteIndexed);
            pc = 0xF0C5U;
            break;

        case 0xF0C5U:  // BPL $F0C9
            pc = branch(
                0xF0C5U, 0xF0C9U, 0xF0C7U,
                (registers.status & kNegative) == 0U);
            break;

        case 0xF0C7U:  // DEC $2B -- extend the sign
            decrement_memory(0xF0C7U, zp(0x2BU));
            append_checkpoint(0xF0C7U, 0xF0C9U, kZeroPageReadModifyWrite);
            pc = 0xF0C9U;
            break;

        case 0xF0C9U:  // CLC
            set_carry(false);
            append_checkpoint(0xF0C9U, 0xF0CAU, kImplied);
            pc = 0xF0CAU;
            break;

        case 0xF0CAU:  // LDA $3280,X -- position, sub-pixel byte
            load_accumulator(read_ram(indexed(0x3280U)));
            append_checkpoint(0xF0CAU, 0xF0CDU, kAbsoluteIndexed);
            pc = 0xF0CDU;
            break;

        case 0xF0CDU:  // ADC $3500,X
            add_with_carry(read_ram(indexed(0x3500U)));
            append_checkpoint(0xF0CDU, 0xF0D0U, kAbsoluteIndexed);
            pc = 0xF0D0U;
            break;

        case 0xF0D0U:  // STA $3280,X
            store(0xF0D0U, indexed(0x3280U), registers.accumulator);
            append_checkpoint(0xF0D0U, 0xF0D3U, kAbsoluteIndexed);
            pc = 0xF0D3U;
            break;

        case 0xF0D3U:  // LDA $3300,X -- position, low whole-pixel byte
            load_accumulator(read_ram(indexed(0x3300U)));
            append_checkpoint(0xF0D3U, 0xF0D6U, kAbsoluteIndexed);
            pc = 0xF0D6U;
            break;

        case 0xF0D6U:  // ADC $3580,X
            add_with_carry(read_ram(indexed(0x3580U)));
            append_checkpoint(0xF0D6U, 0xF0D9U, kAbsoluteIndexed);
            pc = 0xF0D9U;
            break;

        case 0xF0D9U:  // STA $3300,X
            store(0xF0D9U, indexed(0x3300U), registers.accumulator);
            append_checkpoint(0xF0D9U, 0xF0DCU, kAbsoluteIndexed);
            pc = 0xF0DCU;
            break;

        case 0xF0DCU:  // LDA $3380,X -- position, high whole-pixel byte
            load_accumulator(read_ram(indexed(0x3380U)));
            append_checkpoint(0xF0DCU, 0xF0DFU, kAbsoluteIndexed);
            pc = 0xF0DFU;
            break;

        case 0xF0DFU:  // ADC $2B
            add_with_carry(read_ram(zp(0x2BU)));
            append_checkpoint(0xF0DFU, 0xF0E1U, kZeroPage);
            pc = 0xF0E1U;
            break;

        case 0xF0E1U:  // STA $3380,X
            store(0xF0E1U, indexed(0x3380U), registers.accumulator);
            append_checkpoint(0xF0E1U, 0xF0E4U, kAbsoluteIndexed);
            pc = 0xF0E4U;
            break;

        case 0xF0E4U:  // LDA $3380,X
            load_accumulator(read_ram(indexed(0x3380U)));
            append_checkpoint(0xF0E4U, 0xF0E7U, kAbsoluteIndexed);
            pc = 0xF0E7U;
            break;

        case 0xF0E7U:  // BMI $F100 -- above the top of the playfield
            pc = branch(
                0xF0E7U, 0xF100U, 0xF0E9U,
                (registers.status & kNegative) != 0U);
            break;

        // The whole-pixel position and the bound $02D0 are staged for the
        // registered 16-bit comparison at 00:E93C, which reports out of bounds
        // in the carry.
        case 0xF0E9U:  // LDA $3300,X
            load_accumulator(read_ram(indexed(0x3300U)));
            append_checkpoint(0xF0E9U, 0xF0ECU, kAbsoluteIndexed);
            pc = 0xF0ECU;
            break;

        case 0xF0ECU:  // STA $2F
            store(0xF0ECU, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF0ECU, 0xF0EEU, kZeroPage);
            pc = 0xF0EEU;
            break;

        case 0xF0EEU:  // LDA $3380,X
            load_accumulator(read_ram(indexed(0x3380U)));
            append_checkpoint(0xF0EEU, 0xF0F1U, kAbsoluteIndexed);
            pc = 0xF0F1U;
            break;

        case 0xF0F1U:  // STA $30
            store(0xF0F1U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF0F1U, 0xF0F3U, kZeroPage);
            pc = 0xF0F3U;
            break;

        case 0xF0F3U:  // LDA #$D0
            load_accumulator(0xD0U);
            append_checkpoint(0xF0F3U, 0xF0F5U, kImmediate);
            pc = 0xF0F5U;
            break;

        case 0xF0F5U:  // STA $2D
            store(0xF0F5U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF0F5U, 0xF0F7U, kZeroPage);
            pc = 0xF0F7U;
            break;

        case 0xF0F7U:  // LDA #$02
            load_accumulator(0x02U);
            append_checkpoint(0xF0F7U, 0xF0F9U, kImmediate);
            pc = 0xF0F9U;
            break;

        case 0xF0F9U:  // STA $2E
            store(0xF0F9U, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xF0F9U, 0xF0FBU, kZeroPage);
            pc = 0xF0FBU;
            break;

        case 0xF0FBU:  // JSR $E93C -- crossed into the comparison helper
            call(0xF0FBU, 0xE93CU);
            break;

        // ---- 00:F0FE  what the comparison decided --------------------------
        case 0xF0FEU:  // BCC $F103 -- in bounds on all 506 calls
            pc = branch(
                0xF0FEU, 0xF103U, 0xF100U,
                (registers.status & kCarry) == 0U);
            break;

        // ---- 00:F104  the horizontal integrator ----------------------------
        case 0xF104U:  // STZ $2B
            store(0xF104U, zp(0x2BU), 0x00U);
            append_checkpoint(0xF104U, 0xF106U, kZeroPage);
            pc = 0xF106U;
            break;

        case 0xF106U:  // LDA $3480,X
            load_accumulator(read_ram(indexed(0x3480U)));
            append_checkpoint(0xF106U, 0xF109U, kAbsoluteIndexed);
            pc = 0xF109U;
            break;

        case 0xF109U:  // BPL $F10D
            pc = branch(
                0xF109U, 0xF10DU, 0xF10BU,
                (registers.status & kNegative) == 0U);
            break;

        case 0xF10BU:  // DEC $2B
            decrement_memory(0xF10BU, zp(0x2BU));
            append_checkpoint(0xF10BU, 0xF10DU, kZeroPageReadModifyWrite);
            pc = 0xF10DU;
            break;

        case 0xF10DU:  // CLC
            set_carry(false);
            append_checkpoint(0xF10DU, 0xF10EU, kImplied);
            pc = 0xF10EU;
            break;

        case 0xF10EU:  // LDA $3100,X
            load_accumulator(read_ram(indexed(0x3100U)));
            append_checkpoint(0xF10EU, 0xF111U, kAbsoluteIndexed);
            pc = 0xF111U;
            break;

        case 0xF111U:  // ADC $3400,X
            add_with_carry(read_ram(indexed(0x3400U)));
            append_checkpoint(0xF111U, 0xF114U, kAbsoluteIndexed);
            pc = 0xF114U;
            break;

        case 0xF114U:  // STA $3100,X
            store(0xF114U, indexed(0x3100U), registers.accumulator);
            append_checkpoint(0xF114U, 0xF117U, kAbsoluteIndexed);
            pc = 0xF117U;
            break;

        case 0xF117U:  // LDA $3180,X
            load_accumulator(read_ram(indexed(0x3180U)));
            append_checkpoint(0xF117U, 0xF11AU, kAbsoluteIndexed);
            pc = 0xF11AU;
            break;

        case 0xF11AU:  // ADC $3480,X
            add_with_carry(read_ram(indexed(0x3480U)));
            append_checkpoint(0xF11AU, 0xF11DU, kAbsoluteIndexed);
            pc = 0xF11DU;
            break;

        case 0xF11DU:  // STA $3180,X
            store(0xF11DU, indexed(0x3180U), registers.accumulator);
            append_checkpoint(0xF11DU, 0xF120U, kAbsoluteIndexed);
            pc = 0xF120U;
            break;

        case 0xF120U:  // LDA $3200,X
            load_accumulator(read_ram(indexed(0x3200U)));
            append_checkpoint(0xF120U, 0xF123U, kAbsoluteIndexed);
            pc = 0xF123U;
            break;

        case 0xF123U:  // ADC $2B
            add_with_carry(read_ram(zp(0x2BU)));
            append_checkpoint(0xF123U, 0xF125U, kZeroPage);
            pc = 0xF125U;
            break;

        case 0xF125U:  // STA $3200,X
            store(0xF125U, indexed(0x3200U), registers.accumulator);
            append_checkpoint(0xF125U, 0xF128U, kAbsoluteIndexed);
            pc = 0xF128U;
            break;

        case 0xF128U:  // LDA $3200,X
            load_accumulator(read_ram(indexed(0x3200U)));
            append_checkpoint(0xF128U, 0xF12BU, kAbsoluteIndexed);
            pc = 0xF12BU;
            break;

        case 0xF12BU:  // BMI $F144 -- left of the playfield
            pc = branch(
                0xF12BU, 0xF144U, 0xF12DU,
                (registers.status & kNegative) != 0U);
            break;

        case 0xF12DU:  // LDA $3180,X
            load_accumulator(read_ram(indexed(0x3180U)));
            append_checkpoint(0xF12DU, 0xF130U, kAbsoluteIndexed);
            pc = 0xF130U;
            break;

        case 0xF130U:  // STA $2F
            store(0xF130U, zp(0x2FU), registers.accumulator);
            append_checkpoint(0xF130U, 0xF132U, kZeroPage);
            pc = 0xF132U;
            break;

        case 0xF132U:  // LDA $3200,X
            load_accumulator(read_ram(indexed(0x3200U)));
            append_checkpoint(0xF132U, 0xF135U, kAbsoluteIndexed);
            pc = 0xF135U;
            break;

        case 0xF135U:  // STA $30
            store(0xF135U, zp(0x30U), registers.accumulator);
            append_checkpoint(0xF135U, 0xF137U, kZeroPage);
            pc = 0xF137U;
            break;

        case 0xF137U:  // LDA #$40
            load_accumulator(0x40U);
            append_checkpoint(0xF137U, 0xF139U, kImmediate);
            pc = 0xF139U;
            break;

        case 0xF139U:  // STA $2D
            store(0xF139U, zp(0x2DU), registers.accumulator);
            append_checkpoint(0xF139U, 0xF13BU, kZeroPage);
            pc = 0xF13BU;
            break;

        case 0xF13BU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0xF13BU, 0xF13DU, kImmediate);
            pc = 0xF13DU;
            break;

        case 0xF13DU:  // STA $2E
            store(0xF13DU, zp(0x2EU), registers.accumulator);
            append_checkpoint(0xF13DU, 0xF13FU, kZeroPage);
            pc = 0xF13FU;
            break;

        case 0xF13FU:  // JSR $E93C -- crossed into the comparison helper
            call(0xF13FU, 0xE93CU);
            break;

        // ---- 00:F142  what the comparison decided --------------------------
        case 0xF142U:  // BCC $F147 -- in bounds on all 1,022 calls
            pc = branch(
                0xF142U, 0xF147U, 0xF144U,
                (registers.status & kCarry) == 0U);
            break;

        // ---- 00:F148  the vertical velocity step ---------------------------
        // While the whole-pixel position is below the target in `$3780,X` the
        // velocity gains the step in `$3680,X`; at or above it, it loses it.
        // The high byte being non-zero means the position is past $0100 and so
        // certainly above any target, which is what the first test is for.
        case 0xF148U:  // LDA $3380,X
            load_accumulator(read_ram(indexed(0x3380U)));
            append_checkpoint(0xF148U, 0xF14BU, kAbsoluteIndexed);
            pc = 0xF14BU;
            break;

        case 0xF14BU:  // BNE $F16B
            pc = branch(
                0xF14BU, 0xF16BU, 0xF14DU,
                (registers.status & kZero) == 0U);
            break;

        case 0xF14DU:  // LDA $3300,X
            load_accumulator(read_ram(indexed(0x3300U)));
            append_checkpoint(0xF14DU, 0xF150U, kAbsoluteIndexed);
            pc = 0xF150U;
            break;

        case 0xF150U:  // CMP $3780,X
            compare_accumulator(read_ram(indexed(0x3780U)));
            append_checkpoint(0xF150U, 0xF153U, kAbsoluteIndexed);
            pc = 0xF153U;
            break;

        case 0xF153U:  // BEQ $F17D -- already on target, leave the velocity
            pc = branch(
                0xF153U, 0xF17DU, 0xF155U,
                (registers.status & kZero) != 0U);
            break;

        case 0xF155U:  // BCS $F16B
            pc = branch(
                0xF155U, 0xF16BU, 0xF157U,
                (registers.status & kCarry) != 0U);
            break;

        case 0xF157U:  // CLC
            set_carry(false);
            append_checkpoint(0xF157U, 0xF158U, kImplied);
            pc = 0xF158U;
            break;

        case 0xF158U:  // LDA $3500,X
            load_accumulator(read_ram(indexed(0x3500U)));
            append_checkpoint(0xF158U, 0xF15BU, kAbsoluteIndexed);
            pc = 0xF15BU;
            break;

        case 0xF15BU:  // ADC $3680,X
            add_with_carry(read_ram(indexed(0x3680U)));
            append_checkpoint(0xF15BU, 0xF15EU, kAbsoluteIndexed);
            pc = 0xF15EU;
            break;

        case 0xF15EU:  // STA $3500,X
            store(0xF15EU, indexed(0x3500U), registers.accumulator);
            append_checkpoint(0xF15EU, 0xF161U, kAbsoluteIndexed);
            pc = 0xF161U;
            break;

        case 0xF161U:  // LDA $3580,X
            load_accumulator(read_ram(indexed(0x3580U)));
            append_checkpoint(0xF161U, 0xF164U, kAbsoluteIndexed);
            pc = 0xF164U;
            break;

        case 0xF164U:  // ADC #$00 -- carry the addition into the high byte
            add_with_carry(0x00U);
            append_checkpoint(0xF164U, 0xF166U, kImmediate);
            pc = 0xF166U;
            break;

        case 0xF166U:  // STA $3580,X
            store(0xF166U, indexed(0x3580U), registers.accumulator);
            append_checkpoint(0xF166U, 0xF169U, kAbsoluteIndexed);
            pc = 0xF169U;
            break;

        case 0xF169U:  // BRA $F17D
            append_checkpoint(0xF169U, 0xF17DU, kBranchAlways);
            pc = 0xF17DU;
            break;

        case 0xF16BU:  // SEC
            set_carry(true);
            append_checkpoint(0xF16BU, 0xF16CU, kImplied);
            pc = 0xF16CU;
            break;

        case 0xF16CU:  // LDA $3500,X
            load_accumulator(read_ram(indexed(0x3500U)));
            append_checkpoint(0xF16CU, 0xF16FU, kAbsoluteIndexed);
            pc = 0xF16FU;
            break;

        case 0xF16FU:  // SBC $3680,X
            subtract_with_carry(read_ram(indexed(0x3680U)));
            append_checkpoint(0xF16FU, 0xF172U, kAbsoluteIndexed);
            pc = 0xF172U;
            break;

        case 0xF172U:  // STA $3500,X
            store(0xF172U, indexed(0x3500U), registers.accumulator);
            append_checkpoint(0xF172U, 0xF175U, kAbsoluteIndexed);
            pc = 0xF175U;
            break;

        case 0xF175U:  // LDA $3580,X
            load_accumulator(read_ram(indexed(0x3580U)));
            append_checkpoint(0xF175U, 0xF178U, kAbsoluteIndexed);
            pc = 0xF178U;
            break;

        case 0xF178U:  // SBC #$00 -- borrow out of the high byte
            subtract_with_carry(0x00U);
            append_checkpoint(0xF178U, 0xF17AU, kImmediate);
            pc = 0xF17AU;
            break;

        case 0xF17AU:  // STA $3580,X
            store(0xF17AU, indexed(0x3580U), registers.accumulator);
            append_checkpoint(0xF17AU, 0xF17DU, kAbsoluteIndexed);
            pc = 0xF17DU;
            break;

        // ---- 00:F17E  the horizontal velocity step -------------------------
        case 0xF17EU:  // LDA $3200,X
            load_accumulator(read_ram(indexed(0x3200U)));
            append_checkpoint(0xF17EU, 0xF181U, kAbsoluteIndexed);
            pc = 0xF181U;
            break;

        case 0xF181U:  // BNE $F1A1
            pc = branch(
                0xF181U, 0xF1A1U, 0xF183U,
                (registers.status & kZero) == 0U);
            break;

        case 0xF183U:  // LDA $3180,X
            load_accumulator(read_ram(indexed(0x3180U)));
            append_checkpoint(0xF183U, 0xF186U, kAbsoluteIndexed);
            pc = 0xF186U;
            break;

        case 0xF186U:  // CMP $3700,X
            compare_accumulator(read_ram(indexed(0x3700U)));
            append_checkpoint(0xF186U, 0xF189U, kAbsoluteIndexed);
            pc = 0xF189U;
            break;

        case 0xF189U:  // BEQ $F1B3
            pc = branch(
                0xF189U, 0xF1B3U, 0xF18BU,
                (registers.status & kZero) != 0U);
            break;

        case 0xF18BU:  // BCS $F1A1
            pc = branch(
                0xF18BU, 0xF1A1U, 0xF18DU,
                (registers.status & kCarry) != 0U);
            break;

        case 0xF18DU:  // CLC
            set_carry(false);
            append_checkpoint(0xF18DU, 0xF18EU, kImplied);
            pc = 0xF18EU;
            break;

        case 0xF18EU:  // LDA $3400,X
            load_accumulator(read_ram(indexed(0x3400U)));
            append_checkpoint(0xF18EU, 0xF191U, kAbsoluteIndexed);
            pc = 0xF191U;
            break;

        case 0xF191U:  // ADC $3600,X
            add_with_carry(read_ram(indexed(0x3600U)));
            append_checkpoint(0xF191U, 0xF194U, kAbsoluteIndexed);
            pc = 0xF194U;
            break;

        case 0xF194U:  // STA $3400,X
            store(0xF194U, indexed(0x3400U), registers.accumulator);
            append_checkpoint(0xF194U, 0xF197U, kAbsoluteIndexed);
            pc = 0xF197U;
            break;

        case 0xF197U:  // LDA $3480,X
            load_accumulator(read_ram(indexed(0x3480U)));
            append_checkpoint(0xF197U, 0xF19AU, kAbsoluteIndexed);
            pc = 0xF19AU;
            break;

        case 0xF19AU:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0xF19AU, 0xF19CU, kImmediate);
            pc = 0xF19CU;
            break;

        case 0xF19CU:  // STA $3480,X
            store(0xF19CU, indexed(0x3480U), registers.accumulator);
            append_checkpoint(0xF19CU, 0xF19FU, kAbsoluteIndexed);
            pc = 0xF19FU;
            break;

        case 0xF19FU:  // BRA $F1B3
            append_checkpoint(0xF19FU, 0xF1B3U, kBranchAlways);
            pc = 0xF1B3U;
            break;

        case 0xF1A1U:  // SEC
            set_carry(true);
            append_checkpoint(0xF1A1U, 0xF1A2U, kImplied);
            pc = 0xF1A2U;
            break;

        case 0xF1A2U:  // LDA $3400,X
            load_accumulator(read_ram(indexed(0x3400U)));
            append_checkpoint(0xF1A2U, 0xF1A5U, kAbsoluteIndexed);
            pc = 0xF1A5U;
            break;

        case 0xF1A5U:  // SBC $3600,X
            subtract_with_carry(read_ram(indexed(0x3600U)));
            append_checkpoint(0xF1A5U, 0xF1A8U, kAbsoluteIndexed);
            pc = 0xF1A8U;
            break;

        case 0xF1A8U:  // STA $3400,X
            store(0xF1A8U, indexed(0x3400U), registers.accumulator);
            append_checkpoint(0xF1A8U, 0xF1ABU, kAbsoluteIndexed);
            pc = 0xF1ABU;
            break;

        case 0xF1ABU:  // LDA $3480,X
            load_accumulator(read_ram(indexed(0x3480U)));
            append_checkpoint(0xF1ABU, 0xF1AEU, kAbsoluteIndexed);
            pc = 0xF1AEU;
            break;

        case 0xF1AEU:  // SBC #$00
            subtract_with_carry(0x00U);
            append_checkpoint(0xF1AEU, 0xF1B0U, kImmediate);
            pc = 0xF1B0U;
            break;

        case 0xF1B0U:  // STA $3480,X
            store(0xF1B0U, indexed(0x3480U), registers.accumulator);
            append_checkpoint(0xF1B0U, 0xF1B3U, kAbsoluteIndexed);
            pc = 0xF1B3U;
            break;

        default:
            return std::nullopt;
        }

        // A call ends the walk, but it is made rather than left standing: the
        // transcript hands the machine the callee's first instruction, and
        // whatever owns that address picks the body up from there. Nothing is
        // absorbed here -- the return the walk would take belongs to the callee
        // and has not been executed yet.
        if (crossed.has_value()) {
            if (overflow) {
                return std::nullopt;
            }
            replacement.return_instruction = *crossed;
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        }
    }
}

} // namespace devils_crush::app::detail
