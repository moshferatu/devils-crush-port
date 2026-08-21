// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// 09:7237-09:7268, the score panel's redraw poll: four gates in front of the
// panel body, and on this route it declines to redraw 2,616 times out of 2,649.
//
//   7237  LDA $2DBB / BEQ $723F     a redraw already in progress?
//   723C  JMP $726B                 ... then go straight to the body
//   723F  LDA $2D53 / BNE $725E     the session handshake, skip the countdown
//   7244  LDA $2D70 / BEQ $725E     the hold-off countdown, zero means ready
//   7249  ... DEC $2D70 ...         otherwise tick it down and leave
//   725E  LDA $2CC0 / BIT #$01      the panel dirty bit
//   7263  BNE $7266                 set: consume it and redraw
//   7265  RTS                       clear: nothing to do
//   7266  AND #$FE / STA $2CC0      consume the request
//   726B  ... the redraw itself
//
// **The addresses name it before the control flow does.** `$2CC0` is the flag
// `00:F55C`'s score accumulator sets, and bit 7 of it is already the ending
// request `01:C190` tests; bit 0 is the panel's. `ROUTINE_MAP.md` records the
// far side of this poll under the session-and-scoring closure route -- "`09:7266`,
// `09:72FD`: consumes the dirty bit and redraws the score and remaining-ball
// digits", with `01:C812` marking the panel dirty at frame 3,287. So this is
// the guard in front of a routine the port had already read, and what it mostly
// does is decide that the panel has not changed.
//
// **It is the largest thing left in bank 09 and the profile's grouping hid
// that.** `09:7237`-`09:7265` is 327,924 master cycles, forty-two per cent of
// the bank's remainder, in ten instructions. The candidate analyzer groups
// contiguous executed addresses, and this routine branches over the untaken
// countdown at 7249-725B, so it was ranked as two separate runs -- `09:71EA`-`7247`
// and `09:725E`-`72EA` -- neither of which is a routine. Control flow joins what
// address adjacency splits.
//
// Every gate is decided from work RAM, so the walk is state-generic and the
// three arms this route never takes are simply where it stops:
//
//   * `$2DBB` non-zero stops in front of the `JMP` at 723C.
//   * `$2D70` non-zero stops in front of the `DEC` at 7249.
//   * the dirty bit set walks 7266 and 7268 -- consuming the request is two
//     work RAM instructions -- and stops in front of the body at 726B. Thirty-
//     three calls on this route, and the body is the part that drives the VDC.
//
// The fourth arm, `$2D53` non-zero at 7242, needs nothing: it branches to 725E,
// which the walk models anyway for the 2,649 calls that arrive there.
//
// `BIT #$01` is the one instruction here worth checking against the core rather
// than the datasheet. A 65C02's immediate-mode `BIT` touches only `Z`; this
// core's `BIT` macro is shared across every addressing mode and clears `N` and
// `V` from the operand's top two bits, so `BIT #$01` clears both. Writing the
// documented behaviour instead would leave the transcript's flags disagreeing
// with the core's at the resume point, which is an abandonment rather than a
// wrong answer -- but an abandonment on 2,649 calls.
//
// The return is absorbed. The `RTS` at 7265 reads only the stack, and the
// caller is the bank-call wrapper at 00:E252 with MPR3 already holding this
// bank, which is what admission at `09:7237` states.
constexpr std::uint8_t kBank = 0x09U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// The four gates, in the order the routine reads them. All four are absolute
// addresses written into the instructions, so unlike a walk that builds a
// pointer there is nothing here that can leave work RAM.
constexpr std::uint16_t kRedrawInProgress = 0x2DBBU;
constexpr std::uint16_t kSessionHandshake = 0x2D53U;
constexpr std::uint16_t kHoldOffCountdown = 0x2D70U;
constexpr std::uint16_t kPanelFlags = 0x2CC0U;
constexpr std::uint8_t kPanelDirtyBit = 0x01U;

// Master cycles, three per CPU cycle, confirmed against the profile's own
// per-row totals: 2,649 executions of `LDA $2DBB` is 40,095, of the taken
// `BEQ` at 723A is 32,076, of the untaken `BNE` at 7242 is 15,990, and 2,616
// of `RTS` is 55,464.
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;

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

}  // namespace

std::optional<RoutineReplacement> replace_score_animation_left_schedule(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // 09:70C1 is the left score-panel ornament's one-shot coroutine handoff.
    // Its JSR frame names 70C4, which C5C9 consumes and installs as the next
    // actor dispatch before returning directly to the actor-scan caller.
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x70C1U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    auto staged = std::vector<std::byte>(work_ram.begin(), work_ram.end());
    staged[0x0100U + stack_high] = static_cast<std::byte>(0x70U);
    staged[0x0100U + stack_low] = static_cast<std::byte>(0xC3U);

    auto scheduler_registers = entry.registers;
    scheduler_registers.stack_pointer = static_cast<std::uint8_t>(
        scheduler_registers.stack_pointer - 2U);
    scheduler_registers.program_counter = 0xC5C9U;
    RoutineEntry scheduler_entry{
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC5C9U},
        .registers = scheduler_registers,
    };
    auto scheduler = replace_coroutine_scheduler(scheduler_entry, staged);
    if (!scheduler || scheduler->checkpoints.empty()) {
        return std::nullopt;
    }

    constexpr std::uint32_t kSchedulerOffset = 21U;
    RoutineReplacement replacement{};
    replacement.work_ram_writes = {
        {
            .instruction = {kBank, 0x70C1U},
            .logical_address = static_cast<std::uint16_t>(
                kStackBase + stack_high),
            .value = 0x70U,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {kBank, 0x70C1U},
            .logical_address = static_cast<std::uint16_t>(
                kStackBase + stack_low),
            .value = 0xC3U,
            .master_cycle_offset = 0U,
        },
    };
    for (auto write : scheduler->work_ram_writes) {
        write.master_cycle_offset += kSchedulerOffset;
        replacement.work_ram_writes.push_back(write);
    }

    replacement.checkpoints.reserve(scheduler->checkpoints.size() + 1U);
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = kSchedulerOffset,
        .instruction = {kBank, 0x70C1U},
        .registers = scheduler_registers,
    });
    for (auto checkpoint : scheduler->checkpoints) {
        checkpoint.interrupt_sample_master_cycle_offset += kSchedulerOffset;
        checkpoint.master_cycle_offset += kSchedulerOffset;
        replacement.checkpoints.push_back(checkpoint);
    }
    replacement.return_instruction = scheduler->return_instruction;
    replacement.output_registers = scheduler->output_registers;
    replacement.body_master_cycles =
        kSchedulerOffset + scheduler->body_master_cycles;
    return replacement;
}

namespace {

std::optional<RoutineReplacement> replace_score_animation_step_impl(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom,
    const RoutineKey routine,
    const std::uint16_t table,
    const std::uint16_t return_pc)
{
    if (entry.routine != routine || work_ram.size() < 0x2000U
        || rom.size() != 0x60000U || entry.registers.mpr[3] != kBank) {
        return std::nullopt;
    }

    constexpr std::uint32_t kImmediateCycles = 6U;
    constexpr std::uint32_t kAbsoluteCycles = 15U;
    constexpr std::uint32_t kAbsoluteIndexedCycles = 15U;
    constexpr std::uint32_t kReadModifyWriteIndexedCycles = 21U;
    constexpr std::uint32_t kStepBranchTaken = 12U;
    constexpr std::uint32_t kStepBranchNotTaken = 6U;

    const auto countdown_address = static_cast<std::uint16_t>(
        0x3A00U + entry.registers.x);
    const auto phase_address = static_cast<std::uint16_t>(
        0x3A80U + entry.registers.x);
    const auto appearance_address = static_cast<std::uint16_t>(
        0x3980U + entry.registers.x);

    const auto read_ram = [work_ram](const std::uint16_t address) {
        return std::to_integer<std::uint8_t>(
            work_ram[static_cast<std::size_t>(address - kWorkRamBase)]);
    };
    const auto read_rom = [rom](const std::uint16_t address) {
        constexpr std::size_t kBankOffset =
            static_cast<std::size_t>(kBank) * 0x2000U;
        return std::to_integer<std::uint8_t>(
            rom[kBankOffset + static_cast<std::size_t>(address & 0x1FFFU)]);
    };

    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(5U);
    replacement.checkpoints.reserve(13U);
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    std::size_t committed_writes = 0U;

    const auto store =
        [&replacement](
            const std::uint16_t instruction,
            const std::uint16_t address,
            const std::uint8_t value) {
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };
    const auto append_checkpoint =
        [&replacement, &registers, &cycle, &commit_pending_writes](
            const std::uint16_t instruction,
            const std::uint16_t resume,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            registers.program_counter = resume;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction},
                .registers = registers,
            });
        };
    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto compare_accumulator = [&registers](const std::uint8_t value) {
        const auto difference = static_cast<std::uint8_t>(
            registers.accumulator - value);
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~kCarry));
        if (registers.accumulator >= value) {
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
        }
        registers.status = set_nz(registers.status, difference);
    };

    // DEC $3A00,X / BNE return: seven of every eight calls stop here.
    const auto countdown = static_cast<std::uint8_t>(
        read_ram(countdown_address) - 1U);
    registers.status = set_nz(registers.status, countdown);
    store(routine.logical_pc, countdown_address, countdown);
    append_checkpoint(
        routine.logical_pc,
        static_cast<std::uint16_t>(routine.logical_pc + 3U),
        kReadModifyWriteIndexedCycles);
    if (countdown != 0U) {
        append_checkpoint(
            static_cast<std::uint16_t>(routine.logical_pc + 3U),
            return_pc,
            kStepBranchTaken);
        auto at_return = registers;
        at_return.program_counter = return_pc;
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            {kBank, return_pc},
            cycle,
            at_return);
        return replacement;
    }
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 3U),
        static_cast<std::uint16_t>(routine.logical_pc + 5U),
        kStepBranchNotTaken);

    load_accumulator(0x08U);
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 5U),
        static_cast<std::uint16_t>(routine.logical_pc + 7U),
        kImmediateCycles);
    store(
        static_cast<std::uint16_t>(routine.logical_pc + 7U),
        countdown_address,
        registers.accumulator);
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 7U),
        static_cast<std::uint16_t>(routine.logical_pc + 10U),
        kAbsoluteIndexedCycles);

    const auto phase = static_cast<std::uint8_t>(read_ram(phase_address) + 1U);
    registers.status = set_nz(registers.status, phase);
    store(
        static_cast<std::uint16_t>(routine.logical_pc + 10U),
        phase_address,
        phase);
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 10U),
        static_cast<std::uint16_t>(routine.logical_pc + 13U),
        kReadModifyWriteIndexedCycles);
    load_y(phase);
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 13U),
        static_cast<std::uint16_t>(routine.logical_pc + 16U),
        kAbsoluteIndexedCycles);
    load_accumulator(read_rom(static_cast<std::uint16_t>(table + registers.y)));
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 16U),
        static_cast<std::uint16_t>(routine.logical_pc + 19U),
        kAbsoluteIndexedCycles);
    compare_accumulator(0xFFU);
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 19U),
        static_cast<std::uint16_t>(routine.logical_pc + 21U),
        kImmediateCycles);

    if (registers.accumulator == 0xFFU) {
        append_checkpoint(
            static_cast<std::uint16_t>(routine.logical_pc + 21U),
            static_cast<std::uint16_t>(routine.logical_pc + 23U),
            kStepBranchNotTaken);
        store(
            static_cast<std::uint16_t>(routine.logical_pc + 23U),
            phase_address,
            0U);
        append_checkpoint(
            static_cast<std::uint16_t>(routine.logical_pc + 23U),
            static_cast<std::uint16_t>(routine.logical_pc + 26U),
            kAbsoluteIndexedCycles);
        load_accumulator(read_rom(table));
        append_checkpoint(
            static_cast<std::uint16_t>(routine.logical_pc + 26U),
            static_cast<std::uint16_t>(routine.logical_pc + 29U),
            kAbsoluteCycles);
    } else {
        append_checkpoint(
            static_cast<std::uint16_t>(routine.logical_pc + 21U),
            static_cast<std::uint16_t>(routine.logical_pc + 29U),
            kStepBranchTaken);
    }

    store(
        static_cast<std::uint16_t>(routine.logical_pc + 29U),
        appearance_address,
        registers.accumulator);
    append_checkpoint(
        static_cast<std::uint16_t>(routine.logical_pc + 29U),
        return_pc,
        kAbsoluteIndexedCycles);

    auto at_return = registers;
    at_return.program_counter = return_pc;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kBank, return_pc},
        cycle,
        at_return);
    return replacement;
}

}  // namespace

std::optional<RoutineReplacement> replace_score_animation_left_step(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x70C4U,
    };
    return replace_score_animation_step_impl(
        entry, work_ram, rom, kRoutine, 0x70E5U, 0x70E4U);
}

std::optional<RoutineReplacement> replace_score_animation_right_step(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x71EAU,
    };
    return replace_score_animation_step_impl(
        entry, work_ram, rom, kRoutine, 0x720BU, 0x720AU);
}

std::optional<RoutineReplacement> replace_score_panel_poll(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x7237U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // The memory-operand T flag would take the left-hand side of the `AND` at
    // 7266 out of zero page instead of the accumulator. Decimal reaches nothing
    // here -- there is no arithmetic in the routine at all.
    if ((entry.registers.status & kMemory) != 0U) {
        return std::nullopt;
    }

    RoutineReplacement replacement;
    replacement.work_ram_writes.reserve(1U);
    replacement.checkpoints.reserve(11U);

    CpuRegisters registers = entry.registers;
    std::uint32_t cycle = 0U;

    const auto read_ram = [work_ram](const std::uint16_t address) {
        return std::to_integer<std::uint8_t>(
            work_ram[static_cast<std::size_t>(address - kWorkRamBase)]);
    };

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
    // which is where the core's LastCycle sits -- and a branch calls it on
    // both arms, so a taken branch samples at 9 and an untaken one at 3.
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

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };

    // A transcript that ends in front of an instruction rather than past a
    // return: the core resumes inside this bank, on the arm the gates chose.
    const auto stop_before =
        [&replacement, &registers, &cycle](const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.return_instruction = {kBank, resume_pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            return replacement;
        };

    // ---- 7237  a redraw already in progress? -----------------------------
    load_accumulator(read_ram(kRedrawInProgress));
    append_checkpoint(0x7237U, 0x723AU, kAbsolute);

    if ((registers.status & kZero) == 0U) {
        // The body is already running somewhere above this call; 723C jumps
        // straight into it. Never taken on the census route, so the walk stops
        // in front of the `JMP` rather than modelling an arm nothing exercises.
        append_checkpoint(0x723AU, 0x723CU, kBranchNotTaken);
        return stop_before(0x723CU);
    }
    append_checkpoint(0x723AU, 0x723FU, kBranchTaken);

    // ---- 723F  the session handshake -------------------------------------
    // Non-zero skips the hold-off countdown entirely and goes to the dirty-bit
    // test, which is where this walk is going anyway.
    load_accumulator(read_ram(kSessionHandshake));
    append_checkpoint(0x723FU, 0x7242U, kAbsolute);

    if ((registers.status & kZero) == 0U) {
        append_checkpoint(0x7242U, 0x725EU, kBranchTaken);
    } else {
        append_checkpoint(0x7242U, 0x7244U, kBranchNotTaken);

        // ---- 7244  the hold-off countdown --------------------------------
        load_accumulator(read_ram(kHoldOffCountdown));
        append_checkpoint(0x7244U, 0x7247U, kAbsolute);

        if ((registers.status & kZero) == 0U) {
            // Still counting down. 7249 decrements it and takes a different
            // exit; the census route never gets here.
            append_checkpoint(0x7247U, 0x7249U, kBranchNotTaken);
            return stop_before(0x7249U);
        }
        append_checkpoint(0x7247U, 0x725EU, kBranchTaken);
    }

    // ---- 725E  the panel dirty bit ---------------------------------------
    load_accumulator(read_ram(kPanelFlags));
    append_checkpoint(0x725EU, 0x7261U, kAbsolute);

    // `BIT #$01` as this core implements it: Z from the masked accumulator, and
    // N and V taken from the operand, which for $01 clears both.
    registers.status = static_cast<std::uint8_t>(
        registers.status
        & static_cast<std::uint8_t>(~(kZero | kOverflow | kNegative)));
    if ((registers.accumulator & kPanelDirtyBit) == 0U) {
        registers.status = static_cast<std::uint8_t>(registers.status | kZero);
    }
    append_checkpoint(0x7261U, 0x7263U, kImmediate);

    if ((registers.status & kZero) == 0U) {
        // ---- 7266  consume the request -----------------------------------
        // Two work RAM instructions, and then the body, which converts the
        // score and drives the VDC through 09:72FD. The walk clears the bit
        // and hands the redraw itself back to the interpreter.
        append_checkpoint(0x7263U, 0x7266U, kBranchTaken);

        const auto cleared = static_cast<std::uint8_t>(
            registers.accumulator
            & static_cast<std::uint8_t>(~kPanelDirtyBit));
        load_accumulator(cleared);
        append_checkpoint(0x7266U, 0x7268U, kImmediate);

        replacement.work_ram_writes.push_back({
            .instruction = {kBank, 0x7268U},
            .logical_address = kPanelFlags,
            .value = registers.accumulator,
            .master_cycle_offset = 0U,
        });
        append_checkpoint(0x7268U, 0x726BU, kAbsolute);

        return stop_before(0x726BU);
    }
    append_checkpoint(0x7263U, 0x7265U, kBranchNotTaken);

    // ---- 7265  the return, absorbed --------------------------------------
    // The common answer, 2,616 of 2,649 calls: nothing about the panel has
    // changed. The routine pushes nothing, so the stack pointer is still the
    // one the call was entered on.
    auto at_return = registers;
    at_return.program_counter = 0x7265U;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kBank, 0x7265U},
        cycle,
        at_return);
    return replacement;
}

std::optional<RoutineReplacement> replace_score_panel_glyph(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // 09:72FD-09:737B writes one two-by-two score-panel glyph. The caller
    // supplies a digit in A and a right-to-left VRAM cursor in $35:$36. The
    // routine indexes four tile bytes at $7390 by A * 4, writes the top and
    // bottom rows through the VDC, and leaves the cursor two columns left of
    // where it started.
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x72FDU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || rom.size() != 0x60000U) {
        return std::nullopt;
    }
    // Zero page and the stack are work RAM, the glyph table is in the bank
    // executing in segment 3, and the VDC ports are visible only through the
    // hardware page in segment 0. Decimal mode changes both cursor arithmetic
    // operations; T redirects accumulator arithmetic through zero page.
    if (entry.registers.mpr[0] != 0xFFU
        || entry.registers.mpr[1] != 0xF8U
        || entry.registers.mpr[3] != kBank
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    std::array<std::uint8_t, 0x2000U> ram{};
    for (std::size_t index = 0U; index < ram.size(); ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement;
    replacement.work_ram_writes.reserve(25U);
    replacement.checkpoints.reserve(56U);

    CpuRegisters registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool overflow = false;

    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };
    const auto read_ram = [&ram, &overflow](const std::uint16_t address) {
        if (address < kWorkRamBase || address >= kWorkRamEnd) {
            overflow = true;
            return std::uint8_t{0U};
        }
        return ram[address - kWorkRamBase];
    };
    const auto read_rom =
        [&registers, rom, &overflow](const std::uint16_t address) {
            const auto bank = registers.mpr[address >> 13U];
            if (bank >= 0x80U) {
                overflow = true;
                return std::uint8_t{0U};
            }
            const auto physical_bank = bank < 0x40U
                ? static_cast<std::size_t>(bank & 0x1FU)
                : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
            const auto offset = physical_bank * 0x2000U
                + static_cast<std::size_t>(address & 0x1FFFU);
            if (offset >= rom.size()) {
                overflow = true;
                return std::uint8_t{0U};
            }
            return std::to_integer<std::uint8_t>(rom[offset]);
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

    std::size_t committed_writes = 0U;
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };
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

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto shift_left_accumulator = [&registers]() {
        const auto carry = static_cast<std::uint8_t>(
            (registers.accumulator >> 7U) & kCarry);
        registers.accumulator = static_cast<std::uint8_t>(
            registers.accumulator << 1U);
        registers.status = set_nz(
            static_cast<std::uint8_t>(
                (registers.status & static_cast<std::uint8_t>(~kCarry))
                | carry),
            registers.accumulator);
    };
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
    const auto push_y = [&registers, &store]() {
        store(
            0x72FDU,
            static_cast<std::uint16_t>(kStackBase + registers.stack_pointer),
            registers.y);
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer - 1U);
    };
    const auto pop = [&registers, &read_ram]() {
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer + 1U);
        return read_ram(
            static_cast<std::uint16_t>(kStackBase + registers.stack_pointer));
    };

    constexpr std::uint32_t kImplied = 6U;
    constexpr std::uint32_t kPush = 9U;
    constexpr std::uint32_t kPull = 12U;
    constexpr std::uint32_t kImmediateCycles = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsoluteCycles = 15U;
    constexpr std::uint32_t kPortStore = 18U;
    constexpr std::uint32_t kPortSampleBack = 6U;

    // The VDC's high-data write may stall longer than the declared eighteen
    // master cycles while VRAM is busy. The core applies that wait through the
    // hardware bus and reconciles it at the checkpoint; the transcript keeps
    // the instruction's nominal cycle count and samples two CPU cycles back.
    push_y();
    append_checkpoint(0x72FDU, 0x72FEU, kPush);
    shift_left_accumulator();
    append_checkpoint(0x72FEU, 0x72FFU, kImplied);
    shift_left_accumulator();
    append_checkpoint(0x72FFU, 0x7300U, kImplied);
    load_y(registers.accumulator);
    append_checkpoint(0x7300U, 0x7301U, kImplied);

    load_accumulator(0x00U);
    append_checkpoint(0x7301U, 0x7303U, kImmediateCycles);
    store(0x7303U, zp(0x06U), registers.accumulator);
    append_checkpoint(0x7303U, 0x7305U, kZeroPage);
    store_port(0x7305U, 0x0000U, registers.accumulator);
    append_checkpoint(0x7305U, 0x7308U, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(zp(0x35U)));
    append_checkpoint(0x7308U, 0x730AU, kZeroPage);
    store_port(0x730AU, 0x0002U, registers.accumulator);
    append_checkpoint(0x730AU, 0x730DU, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(zp(0x36U)));
    append_checkpoint(0x730DU, 0x730FU, kZeroPage);
    store_port(0x730FU, 0x0003U, registers.accumulator);
    append_checkpoint(0x730FU, 0x7312U, kPortStore, kPortSampleBack);

    load_accumulator(0x02U);
    append_checkpoint(0x7312U, 0x7314U, kImmediateCycles);
    store(0x7314U, zp(0x06U), registers.accumulator);
    append_checkpoint(0x7314U, 0x7316U, kZeroPage);
    store_port(0x7316U, 0x0000U, registers.accumulator);
    append_checkpoint(0x7316U, 0x7319U, kPortStore, kPortSampleBack);
    load_accumulator(read_rom(static_cast<std::uint16_t>(0x7390U + registers.y)));
    append_checkpoint(0x7319U, 0x731CU, kAbsoluteCycles);
    store_port(0x731CU, 0x0002U, registers.accumulator);
    append_checkpoint(0x731CU, 0x731FU, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(0x2D6FU));
    append_checkpoint(0x731FU, 0x7322U, kAbsoluteCycles);
    store_port(0x7322U, 0x0003U, registers.accumulator);
    append_checkpoint(0x7322U, 0x7325U, kPortStore, kPortSampleBack);
    load_accumulator(read_rom(static_cast<std::uint16_t>(0x7391U + registers.y)));
    append_checkpoint(0x7325U, 0x7328U, kAbsoluteCycles);
    store_port(0x7328U, 0x0002U, registers.accumulator);
    append_checkpoint(0x7328U, 0x732BU, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(0x2D6FU));
    append_checkpoint(0x732BU, 0x732EU, kAbsoluteCycles);
    store_port(0x732EU, 0x0003U, registers.accumulator);
    append_checkpoint(0x732EU, 0x7331U, kPortStore, kPortSampleBack);

    registers.status =
        static_cast<std::uint8_t>(registers.status & ~kCarry);
    append_checkpoint(0x7331U, 0x7332U, kImplied);
    load_accumulator(read_ram(zp(0x35U)));
    append_checkpoint(0x7332U, 0x7334U, kZeroPage);
    add_with_carry(0x40U);
    append_checkpoint(0x7334U, 0x7336U, kImmediateCycles);
    store(0x7336U, zp(0x35U), registers.accumulator);
    append_checkpoint(0x7336U, 0x7338U, kZeroPage);
    registers.accumulator = 0U; // CLA does not change flags on this core.
    append_checkpoint(0x7338U, 0x7339U, kImplied);
    add_with_carry(read_ram(zp(0x36U)));
    append_checkpoint(0x7339U, 0x733BU, kZeroPage);
    store(0x733BU, zp(0x36U), registers.accumulator);
    append_checkpoint(0x733BU, 0x733DU, kZeroPage);

    load_accumulator(0x00U);
    append_checkpoint(0x733DU, 0x733FU, kImmediateCycles);
    store(0x733FU, zp(0x06U), registers.accumulator);
    append_checkpoint(0x733FU, 0x7341U, kZeroPage);
    store_port(0x7341U, 0x0000U, registers.accumulator);
    append_checkpoint(0x7341U, 0x7344U, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(zp(0x35U)));
    append_checkpoint(0x7344U, 0x7346U, kZeroPage);
    store_port(0x7346U, 0x0002U, registers.accumulator);
    append_checkpoint(0x7346U, 0x7349U, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(zp(0x36U)));
    append_checkpoint(0x7349U, 0x734BU, kZeroPage);
    store_port(0x734BU, 0x0003U, registers.accumulator);
    append_checkpoint(0x734BU, 0x734EU, kPortStore, kPortSampleBack);

    load_accumulator(0x02U);
    append_checkpoint(0x734EU, 0x7350U, kImmediateCycles);
    store(0x7350U, zp(0x06U), registers.accumulator);
    append_checkpoint(0x7350U, 0x7352U, kZeroPage);
    store_port(0x7352U, 0x0000U, registers.accumulator);
    append_checkpoint(0x7352U, 0x7355U, kPortStore, kPortSampleBack);
    load_accumulator(read_rom(static_cast<std::uint16_t>(0x7392U + registers.y)));
    append_checkpoint(0x7355U, 0x7358U, kAbsoluteCycles);
    store_port(0x7358U, 0x0002U, registers.accumulator);
    append_checkpoint(0x7358U, 0x735BU, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(0x2D6FU));
    append_checkpoint(0x735BU, 0x735EU, kAbsoluteCycles);
    store_port(0x735EU, 0x0003U, registers.accumulator);
    append_checkpoint(0x735EU, 0x7361U, kPortStore, kPortSampleBack);
    load_accumulator(read_rom(static_cast<std::uint16_t>(0x7393U + registers.y)));
    append_checkpoint(0x7361U, 0x7364U, kAbsoluteCycles);
    store_port(0x7364U, 0x0002U, registers.accumulator);
    append_checkpoint(0x7364U, 0x7367U, kPortStore, kPortSampleBack);
    load_accumulator(read_ram(0x2D6FU));
    append_checkpoint(0x7367U, 0x736AU, kAbsoluteCycles);
    store_port(0x736AU, 0x0003U, registers.accumulator);
    append_checkpoint(0x736AU, 0x736DU, kPortStore, kPortSampleBack);

    registers.status =
        static_cast<std::uint8_t>(registers.status | kCarry);
    append_checkpoint(0x736DU, 0x736EU, kImplied);
    load_accumulator(read_ram(zp(0x35U)));
    append_checkpoint(0x736EU, 0x7370U, kZeroPage);
    subtract_with_carry(0x42U);
    append_checkpoint(0x7370U, 0x7372U, kImmediateCycles);
    store(0x7372U, zp(0x35U), registers.accumulator);
    append_checkpoint(0x7372U, 0x7374U, kZeroPage);
    load_accumulator(read_ram(zp(0x36U)));
    append_checkpoint(0x7374U, 0x7376U, kZeroPage);
    subtract_with_carry(0x00U);
    append_checkpoint(0x7376U, 0x7378U, kImmediateCycles);
    store(0x7378U, zp(0x36U), registers.accumulator);
    append_checkpoint(0x7378U, 0x737AU, kZeroPage);
    load_y(pop());
    append_checkpoint(0x737AU, 0x737BU, kPull);

    if (overflow || cycle != 699U || registers.stack_pointer
            != entry.registers.stack_pointer) {
        return std::nullopt;
    }
    auto at_return = registers;
    at_return.program_counter = 0x737BU;
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {kBank, 0x737BU},
        cycle,
        at_return);
    return replacement;
}

}  // namespace devils_crush::app::detail
