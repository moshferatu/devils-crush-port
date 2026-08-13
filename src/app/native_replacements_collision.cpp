// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/collision.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devils_crush::app::detail {

using emulation::ReplacementMemoryWrite;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_scan_run(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // The collision sweep's head. Each arrival either finds a slot eligible
    // for the vertical scan at CC1A or rejects it. The run walks rejected
    // slots, and an eligible slot no longer ends the transcript: the call to
    // the scanner is transcribed and the scanner's own transcript is spliced
    // in where the core would have run it, so the walk hands control back at
    // 01:CB61 with the scan already answered, or at the exhausted countdown.
    //
    // It is declared as an instruction-boundary transcript so it can cross
    // interrupt horizons rather than be bounded by them, which is what the
    // sibling walk at 01:C510 needed for the same reason.
    //
    //   CB48 LDY $64     12   CB4A LDA $3080,Y 15
    //   CB4D BPL $CB6B  6/12  CB4F BIT #$40     6
    //   CB51 BNE $CB6B  6/12  CB53 LDA $55     12
    //   CB55 BEQ $CB5E  6/12  CB5E JSR $CC1A   21 (composed)
    //   CC40 RTS         21 (composed)  CB61 BCC $CB6B (stays emulated)
    //   CB6B INC $64     18   CB6D DEC $60     18
    //   CB6F BNE $CB48  6/12  CB71 BBS5 $77   (stays emulated)
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCB48U,
    };
    constexpr std::uint16_t kSweepHeadPc = 0xCB48U;
    constexpr std::uint16_t kAttributesPc = 0xCB4AU;
    constexpr std::uint16_t kPositiveBranchPc = 0xCB4DU;
    constexpr std::uint16_t kMaskPc = 0xCB4FU;
    constexpr std::uint16_t kMaskBranchPc = 0xCB51U;
    constexpr std::uint16_t kGatePc = 0xCB53U;
    constexpr std::uint16_t kGateBranchPc = 0xCB55U;
    constexpr std::uint16_t kCandidatePc = 0xCB5EU;
    constexpr std::uint16_t kScannerPc = 0xCC1AU;
    constexpr std::uint16_t kScannerReturnPc = 0xCC40U;
    constexpr std::uint16_t kResultBranchPc = 0xCB61U;
    constexpr std::uint16_t kSlotIncrementPc = 0xCB6BU;
    constexpr std::uint16_t kCountdownPc = 0xCB6DU;
    constexpr std::uint16_t kSweepBranchPc = 0xCB6FU;
    constexpr std::uint16_t kExhaustedPc = 0xCB71U;
    constexpr std::uint16_t kSlotAddress = 0x2064U;
    constexpr std::uint16_t kCountdownAddress = 0x2060U;
    constexpr std::uint16_t kGateAddress = 0x2055U;
    constexpr std::uint16_t kSlotAttributesBase = 0x1080U;
    constexpr std::uint16_t kWorkRamBase = 0x2000U;
    constexpr std::uint16_t kStackBase = 0x2100U;

    constexpr std::uint32_t kImmediate = 6U;
    constexpr std::uint32_t kZeroPage = 12U;
    constexpr std::uint32_t kAbsoluteIndexed = 15U;
    constexpr std::uint32_t kReadModifyWrite = 18U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;
    // JSR and RTS cost the same, and this pair is the whole reason the walk
    // reaches past the call rather than stopping in front of it.
    constexpr std::uint32_t kSubroutineCall = 21U;
    constexpr std::uint32_t kSubroutineReturn = 21U;

    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is proven here.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto slot = std::to_integer<std::uint8_t>(
        work_ram[kSlotAddress - 0x2000U]);
    const auto remaining = std::to_integer<std::uint8_t>(
        work_ram[kCountdownAddress - 0x2000U]);
    const auto gate = std::to_integer<std::uint8_t>(
        work_ram[kGateAddress - 0x2000U]);

    dc_collision_scan_run run{};
    if (!dc_classify_collision_scan_run(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            slot,
            remaining,
            gate,
            static_cast<std::uint8_t>(DC_COLLISION_SCAN_MAX_SKIPS),
            &run)) {
        return std::nullopt;
    }
    // A countdown longer than the classifier's skip table would stop on the
    // budget rather than on a shape this transcript can end at.
    if (run.exit == DC_COLLISION_SCAN_BUDGET) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x82U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    // Eight boundaries and two writes per masked slot, plus a seven-
    // instruction candidate prefix: 1,031 checkpoints and 256 writes at the
    // very worst, inside the core's 2,048 and 512.
    replacement.checkpoints.reserve(
        static_cast<std::size_t>(run.skipped) * 8U + 7U);
    replacement.work_ram_writes.reserve(
        static_cast<std::size_t>(run.skipped) * 2U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                // Every instruction in this sweep samples the interrupt one
                // CPU cycle before its own last, which is where the core's
                // LastCycle sits. None of them touches a hardware port, so
                // none of them pays the VDC's stolen wait.
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };
    const auto step =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycles) {
            cycle += cycles;
            boundary(instruction_pc, resume_pc);
        };
    // LDY $64 followed by LDA $3080,Y.
    const auto read_slot = [&](const std::uint8_t index) {
        registers.y = index;
        registers.status = set_nz(registers.status, index);
        step(kSweepHeadPc, kAttributesPc, kZeroPage);
        const auto attributes = std::to_integer<std::uint8_t>(
            work_ram[static_cast<std::size_t>(kSlotAttributesBase) + index]);
        registers.accumulator = attributes;
        registers.status = set_nz(registers.status, attributes);
        step(kAttributesPc, kPositiveBranchPc, kAbsoluteIndexed);
        return attributes;
    };
    // BIT #$40. Beetle clears N and V and then takes them from the operand,
    // in immediate mode as well as the addressing modes where the 65C02
    // does, so this one always leaves N clear and V set whatever the slot
    // attributes hold; only Z reports the tested bit.
    const auto bit_mask = [&](const std::uint8_t attributes) {
        registers.status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~0xC2U));
        registers.status = static_cast<std::uint8_t>(
            registers.status | 0x40U);
        if ((attributes & 0x40U) == 0U) {
            registers.status = static_cast<std::uint8_t>(
                registers.status | 0x02U);
        }
        step(kMaskPc, kMaskBranchPc, kImmediate);
    };
    // INC $64 : DEC $60. Each leaves its own N and Z standing until the next
    // instruction reloads them, and an interrupt can arrive between them.
    const auto advance =
        [&](const std::uint8_t next_slot, const std::uint8_t left) {
            registers.status = set_nz(registers.status, next_slot);
            cycle += kReadModifyWrite;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, kSlotIncrementPc},
                .logical_address = kSlotAddress,
                .value = next_slot,
                .master_cycle_offset = cycle,
            });
            boundary(kSlotIncrementPc, kCountdownPc);
            registers.status = set_nz(registers.status, left);
            cycle += kReadModifyWrite;
            replacement.work_ram_writes.push_back({
                .instruction = {kRoutine.physical_bank, kCountdownPc},
                .logical_address = kCountdownAddress,
                .value = left,
                .master_cycle_offset = cycle,
            });
            boundary(kCountdownPc, kSweepBranchPc);
        };

    auto current = slot;
    auto left = remaining;
    for (std::uint8_t walked = 0U; walked < run.skipped; ++walked) {
        const auto attributes = read_slot(current);
        if ((attributes & 0x80U) == 0U) {
            step(kPositiveBranchPc, kSlotIncrementPc, kBranchTaken);
        } else {
            step(kPositiveBranchPc, kMaskPc, kBranchNotTaken);
            bit_mask(attributes);
            step(kMaskBranchPc, kSlotIncrementPc, kBranchTaken);
        }
        current = static_cast<std::uint8_t>(current + 1U);
        left = static_cast<std::uint8_t>(left - 1U);
        advance(current, left);
        if (left == 0U) {
            step(kSweepBranchPc, kExhaustedPc, kBranchNotTaken);
        } else {
            step(kSweepBranchPc, kSweepHeadPc, kBranchTaken);
        }
    }

    switch (run.exit) {
    case DC_COLLISION_SCAN_CANDIDATE: {
        const auto attributes = read_slot(current);
        step(kPositiveBranchPc, kMaskPc, kBranchNotTaken);
        bit_mask(attributes);
        // Bit six was clear, so the BNE fell through to the gate.
        step(kMaskBranchPc, kGatePc, kBranchNotTaken);
        registers.accumulator = gate;
        registers.status = set_nz(registers.status, gate);
        step(kGatePc, kGateBranchPc, kZeroPage);
        step(kGateBranchPc, kCandidatePc, kBranchTaken);
        break;
    }
    case DC_COLLISION_SCAN_EXHAUSTED:
        break;
    default:
        return std::nullopt;
    }

    // The transcript and the classifier must agree about where the sweep ends
    // up; a disagreement is a modelling error, not a state to run.
    const auto resume_pc = run.exit == DC_COLLISION_SCAN_EXHAUSTED
        ? kExhaustedPc
        : kCandidatePc;
    if (replacement.checkpoints.empty()
        || registers.accumulator != run.accumulator
        || current != run.slot
        || left != run.remaining
        || registers.program_counter != resume_pc) {
        return std::nullopt;
    }

    // 01:CB5E is `JSR $CC1A` and 01:CC40 is the `RTS` that answers it. Left to
    // the core they cost 21 master cycles each, which on this route is more
    // than the sweep body they bracket: the pair alone is 0.28% of the whole
    // run. Transcribing the call, splicing the scanner's own transcript in
    // where the core would have run it, and transcribing the return removes
    // both. The scanner declines some states and returns from the middle of
    // itself on a hit, so this is an attempt, not a requirement -- when it
    // does not compose the walk still ends in front of the call as before.
    if (run.exit == DC_COLLISION_SCAN_CANDIDATE) {
        const auto composed_checkpoints = replacement.checkpoints.size();
        const auto composed_writes = replacement.work_ram_writes.size();
        const auto composed_cycle = cycle;
        const auto composed_registers = registers;

        // The scanner reads work RAM, and the slots this walk has already
        // stepped over updated the cursor and the countdown, so it must see
        // the sweep's own writes rather than the snapshot it started from.
        std::vector<std::byte> staged(work_ram.begin(), work_ram.end());
        const auto stage = [&staged](const ReplacementMemoryWrite& write) {
            const auto offset =
                static_cast<std::size_t>(write.logical_address - kWorkRamBase);
            if (write.logical_address >= kWorkRamBase
                && offset < staged.size()) {
                staged[offset] = static_cast<std::byte>(write.value);
            }
        };
        for (const auto& write : replacement.work_ram_writes) {
            stage(write);
        }

        // JSR pushes the address of its own last byte, so the return lands on
        // the branch at CB61 that reads the scanner's carry.
        const auto push =
            [&](const std::uint8_t depth, const std::uint8_t value) {
                const ReplacementMemoryWrite write{
                    .instruction = {kRoutine.physical_bank, kCandidatePc},
                    .logical_address = static_cast<std::uint16_t>(
                        kStackBase
                        + static_cast<std::uint8_t>(
                            registers.stack_pointer - depth)),
                    .value = value,
                    .master_cycle_offset = cycle,
                };
                replacement.work_ram_writes.push_back(write);
                stage(write);
            };
        push(0U, static_cast<std::uint8_t>((kCandidatePc + 2U) >> 8U));
        push(1U, static_cast<std::uint8_t>((kCandidatePc + 2U) & 0xFFU));
        registers.stack_pointer =
            static_cast<std::uint8_t>(registers.stack_pointer - 2U);
        step(kCandidatePc, kScannerPc, kSubroutineCall);

        RoutineEntry scanner_entry{
            .routine = {
                .physical_bank = kRoutine.physical_bank,
                .logical_pc = kScannerPc,
            },
            .registers = registers,
        };
        scanner_entry.registers.program_counter = kScannerPc;
        const auto scanner =
            replace_collision_scan_vertical_reject(scanner_entry, staged, rom);

        // A hit leaves the scanner from the middle of itself, at CCD2 or
        // CCD4, and there is no return to transcribe from there.
        const bool composable = scanner.has_value()
            && !scanner->checkpoints.empty()
            && scanner->return_instruction
                == RoutineKey{
                    .physical_bank = kRoutine.physical_bank,
                    .logical_pc = kScannerReturnPc,
                }
            && scanner->output_registers.stack_pointer
                == registers.stack_pointer
            && replacement.checkpoints.size() + scanner->checkpoints.size()
                    + 1U
                <= 1600U
            && replacement.work_ram_writes.size()
                    + scanner->work_ram_writes.size()
                <= 448U;
        if (composable) {
            const auto scanner_offset = cycle;
            for (auto write : scanner->work_ram_writes) {
                write.master_cycle_offset += scanner_offset;
                replacement.work_ram_writes.push_back(write);
            }
            for (auto scanner_checkpoint : scanner->checkpoints) {
                scanner_checkpoint.interrupt_sample_master_cycle_offset +=
                    scanner_offset;
                scanner_checkpoint.master_cycle_offset += scanner_offset;
                replacement.checkpoints.push_back(scanner_checkpoint);
            }
            cycle += scanner->body_master_cycles;
            registers = scanner->output_registers;
            registers.stack_pointer =
                static_cast<std::uint8_t>(registers.stack_pointer + 2U);
            step(kScannerReturnPc, kResultBranchPc, kSubroutineReturn);
        } else {
            replacement.checkpoints.resize(composed_checkpoints);
            replacement.work_ram_writes.resize(composed_writes);
            cycle = composed_cycle;
            registers = composed_registers;
        }
    }

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = registers.program_counter,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_scan_candidate_branch(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // CB61 BCC $CB6B 6/12 — where the sweep resumes once the vertical scan at
    // CC1A returns. A clear carry means the slot missed and the sweep goes
    // straight to its tail; a set carry enters the hit path, whose first
    // instruction is the latch set. The helper call after it stays emulated,
    // so the transcript ends there.
    //
    //   CB61 BCC $CB6B 6/12   CB63 SMB2 $77 21
    //   CB65 JSR $CBCF        (stays emulated)
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCB61U,
    };
    constexpr std::uint16_t kMissBranchPc = 0xCB61U;
    constexpr std::uint16_t kLatchSetPc = 0xCB63U;
    constexpr std::uint16_t kHelperCallPc = 0xCB65U;
    constexpr std::uint16_t kSlotIncrementPc = 0xCB6BU;
    constexpr std::uint16_t kLatchAddress = 0x2077U;

    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;
    // RMB/SMB spend one cycle more than an ordinary zero-page read-modify-
    // write (huc6280.cpp RMW_ZP_B adds a cycle that RMW_ZP does not).
    constexpr std::uint32_t kBitReadModifyWrite = 21U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    auto registers = entry.registers;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(2U);

    // Neither instruction touches a flag: BCC only reads carry, and SMB sets a
    // bit without reporting on the result.
    if ((registers.status & 0x01U) == 0U) {
        registers.program_counter = kSlotIncrementPc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = kBranchTaken - 3U,
            .master_cycle_offset = kBranchTaken,
            .instruction = {kRoutine.physical_bank, kMissBranchPc},
            .registers = registers,
        });
        replacement.body_master_cycles = kBranchTaken;
    } else {
        registers.program_counter = kLatchSetPc;
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = kBranchNotTaken - 3U,
            .master_cycle_offset = kBranchNotTaken,
            .instruction = {kRoutine.physical_bank, kMissBranchPc},
            .registers = registers,
        });
        registers.program_counter = kHelperCallPc;
        replacement.body_master_cycles = kBranchNotTaken + kBitReadModifyWrite;
        replacement.work_ram_writes.push_back({
            .instruction = {kRoutine.physical_bank, kLatchSetPc},
            .logical_address = kLatchAddress,
            .value = static_cast<std::uint8_t>(
                std::to_integer<std::uint8_t>(
                    work_ram[kLatchAddress - 0x2000U])
                | 0x04U),
            .master_cycle_offset = replacement.body_master_cycles,
        });
        replacement.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset =
                replacement.body_master_cycles - 3U,
            .master_cycle_offset = replacement.body_master_cycles,
            .instruction = {kRoutine.physical_bank, kLatchSetPc},
            .registers = registers,
        });
    }

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = registers.program_counter,
    };
    replacement.output_registers = registers;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_scan_advance(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    // The collision sweep's own tail, and the counterpart of `01:C523` in the
    // actor loop. The `01:CB48` walk absorbs this sequence for every slot it
    // rejects, but a slot that reaches the vertical scan leaves the
    // transaction at CC1A, so the advance that follows is a separate arrival.
    // Both the miss branch at CB61 and the hit path's fall-through at CB68
    // land here.
    //
    //   CB6B INC $64 18   CB6D DEC $60 18
    //   CB6F BNE $CB48 6/12   CB71 BBS2 $77 (stays emulated)
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCB6BU,
    };
    constexpr std::uint16_t kSlotIncrementPc = 0xCB6BU;
    constexpr std::uint16_t kCountdownPc = 0xCB6DU;
    constexpr std::uint16_t kSweepBranchPc = 0xCB6FU;
    constexpr std::uint16_t kSweepHeadPc = 0xCB48U;
    constexpr std::uint16_t kExhaustedPc = 0xCB71U;
    constexpr std::uint16_t kSlotAddress = 0x2064U;
    constexpr std::uint16_t kCountdownAddress = 0x2060U;

    constexpr std::uint32_t kReadModifyWrite = 18U;
    constexpr std::uint32_t kBranchTaken = 12U;
    constexpr std::uint32_t kBranchNotTaken = 6U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Neither decimal mode nor the memory-operand T flag is proven here.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x82U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    const auto next_slot = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(work_ram[kSlotAddress - 0x2000U]) + 1U);
    const auto left = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(work_ram[kCountdownAddress - 0x2000U])
        - 1U);

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    RoutineReplacement replacement{};
    replacement.checkpoints.reserve(3U);
    replacement.work_ram_writes.reserve(2U);

    const auto boundary =
        [&](const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kRoutine.physical_bank, instruction_pc},
                .registers = registers,
            });
        };

    // INC $64 : DEC $60. Each leaves its own N and Z standing until the next
    // instruction reloads them, and an interrupt can arrive between them.
    registers.status = set_nz(registers.status, next_slot);
    cycle += kReadModifyWrite;
    replacement.work_ram_writes.push_back({
        .instruction = {kRoutine.physical_bank, kSlotIncrementPc},
        .logical_address = kSlotAddress,
        .value = next_slot,
        .master_cycle_offset = cycle,
    });
    boundary(kSlotIncrementPc, kCountdownPc);

    registers.status = set_nz(registers.status, left);
    cycle += kReadModifyWrite;
    replacement.work_ram_writes.push_back({
        .instruction = {kRoutine.physical_bank, kCountdownPc},
        .logical_address = kCountdownAddress,
        .value = left,
        .master_cycle_offset = cycle,
    });
    boundary(kCountdownPc, kSweepBranchPc);

    cycle += left == 0U ? kBranchNotTaken : kBranchTaken;
    boundary(kSweepBranchPc, left == 0U ? kExhaustedPc : kSweepHeadPc);

    replacement.return_instruction = {
        .physical_bank = kRoutine.physical_bank,
        .logical_pc = registers.program_counter,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_table_interpreter(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6966U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    // The transcript owns every path it can finish inside the transaction caps.
    // What follows is the older result-computing replacement, which still holds
    // the handful of calls whose record loop runs past the write cap.
    if (auto walk = replace_collision_table_walk(entry, work_ram, rom)) {
        return walk;
    }
    const auto selector_address = static_cast<std::uint16_t>(
        0x6A38U
        + (static_cast<std::uint16_t>(
               entry.registers.accumulator)
           << 1U));
    const auto selector_offset = static_cast<std::size_t>(
        0x03U * 0x2000U + (selector_address & 0x1FFFU));
    if (work_ram.size() < 0x2000U
        || selector_offset + 1U >= rom.size()) {
        return std::nullopt;
    }
    const std::uint16_t table_pointer = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(rom[selector_offset])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   rom[selector_offset + 1U]))
           << 8U));

    // The original ordinal-era proof matrix is kept temporarily as
    // migration archaeology, but is no longer compiled or consulted.
    // Semantic ROM/cursor resolution below owns these paths.
#if 0
    const bool front_spider_reverse_full_record =
        entry.registers.accumulator == 0x2BU;
    const bool alternate_reverse_full_record =
        entry.registers.accumulator == 0x06U;
    const bool reverse_full_record =
        front_spider_reverse_full_record
        || alternate_reverse_full_record;
    const bool first_continuation_full_record =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA6U])
            == 0x02U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A6U])
            == 0x80U;
    const bool second_continuation_full_record =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA8U])
            == 0x02U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A8U])
            == 0x80U;
    const bool alternate_continuation_full_record =
        entry.registers.accumulator == 0x06U
        && entry.registers.x == 0x17U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1E97U])
            == 0x02U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1897U])
            == 0x80U;
    const bool horizontal_continuation_full_record =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA6U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A6U])
            == 0x80U;
    const bool reverse_horizontal_continuation_full_record =
        entry.registers.accumulator == 0x2BU
        && entry.registers.x == 0x27U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA7U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A7U])
            == 0x80U;
    const bool later_vertical_continuation_full_record =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA8U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A8U])
            == 0x80U;
    const bool second_later_vertical_continuation_full_record =
        entry.registers.accumulator == 0x2DU
        && entry.registers.x == 0x29U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA9U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A9U])
            == 0x80U;
    const bool continuation_full_record =
        first_continuation_full_record
        || second_continuation_full_record
        || alternate_continuation_full_record
        || horizontal_continuation_full_record
        || reverse_horizontal_continuation_full_record
        || later_vertical_continuation_full_record
        || second_later_vertical_continuation_full_record;
    const bool first_vertical_full_record =
        entry.registers.accumulator == 0x2CU;
    const bool second_vertical_full_record =
        entry.registers.accumulator == 0x2DU;
    const bool vertical_full_record =
        first_vertical_full_record || second_vertical_full_record;
    const std::uint8_t table_entry =
        alternate_reverse_full_record
            ? 0x0BU
            : (vertical_full_record
                   ? 0x05U
                   : (front_spider_reverse_full_record
                          ? 0x07U
                          : 0x03U));
    const std::uint16_t motion_pointer =
        alternate_reverse_full_record
            ? 0x6DE6U
            : (vertical_full_record
                   ? 0x6D7AU
                   : (front_spider_reverse_full_record
                          ? 0x6D9EU
                          : 0x6D56U));
    const std::uint8_t motion_header =
        continuation_full_record
            ? (alternate_continuation_full_record
                   ? 0x10U
                   : (horizontal_continuation_full_record
                          ? 0x20U
                          : (reverse_horizontal_continuation_full_record
                                 ? 0x40U
                                 : 0x08U)))
            : (alternate_reverse_full_record
            ? 0x8AU
            : (vertical_full_record
                   ? 0x84U
                   : (front_spider_reverse_full_record
                          ? 0x86U
                          : 0x82U)));
    const std::uint8_t motion_delta =
        continuation_full_record
            ? 0x00U
            : (vertical_full_record
            ? 0x08U
            : (reverse_full_record ? 0x40U : 0x20U));

#endif
    dc_collision_table_pass pass{};
    if (!dc_select_collision_table_pass(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            table_pointer,
            &pass)) {
        throw std::runtime_error(
            "Native 6966 replacement rejected the work-RAM snapshot.");
    }

    // Resolve the record path from ROM and the two per-slot cursors. The
    // semantic walker below owns control markers and headered records; the
    // one-step path that follows owns headerless records. Neither is tied to
    // a selector, actor slot, caller, or dynamic call ordinal.
    const auto read_bank_03 =
        [&rom](const std::uint16_t logical_address)
        -> std::optional<std::uint8_t> {
        if (logical_address < 0x6000U
            || logical_address > 0x7FFFU) {
            return std::nullopt;
        }
        const auto offset = static_cast<std::size_t>(
            0x03U * 0x2000U + (logical_address & 0x1FFFU));
        if (offset >= rom.size()) {
            return std::nullopt;
        }
        return std::to_integer<std::uint8_t>(rom[offset]);
    };

    // Follow control markers until the walker either terminates at $FF or
    // reaches a headered motion record. Ordinary one-step headerless records
    // continue through the simpler path below.
    const bool walker_active_pass =
        pass.collision_state == 0x80U
        && pass.collision_state_next == 0x81U
        && pass.pass_index == 1U
        && pass.returns_without_step == 0U
        && (entry.registers.status & 0x20U) == 0U;
    auto walker_collision_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E00U + entry.registers.x]);
    auto walker_motion_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E80U + entry.registers.x]);
    std::uint32_t walker_delay = 0U;
    std::uint8_t walker_table_entry = 0U;
    std::uint16_t walker_motion_pointer = 0U;
    std::uint8_t walker_motion_header = 0U;
    std::uint8_t walker_motion_delta = 0U;
    bool walker_valid = walker_active_pass;
    bool walker_record_ready = false;
    bool walker_terminates = false;
    std::vector<ReplacementMemoryWrite> walker_writes;
    walker_writes.reserve(24U);
    if (walker_active_pass) {
        constexpr std::size_t kMaximumWalkerIterations = 16U;
        for (std::size_t iteration = 0U;
             iteration < kMaximumWalkerIterations;
             ++iteration) {
            walker_writes.push_back({
                .instruction = {0x03U, 0x6995U},
                .logical_address = 0x2035U,
                .value = 0U,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        177U + walker_delay),
            });
            auto walker_entry = read_bank_03(
                static_cast<std::uint16_t>(
                    table_pointer + walker_collision_cursor));
            if (!walker_entry.has_value()) {
                walker_valid = false;
                break;
            }
            if (*walker_entry == 0xFFU) {
                walker_terminates = true;
                break;
            }
            if (*walker_entry == 0xFEU) {
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69A6U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E00U + entry.registers.x),
                    .value = 0U,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            258U + walker_delay),
                });
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69A9U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E80U + entry.registers.x),
                    .value = 0U,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            273U + walker_delay),
                });
                walker_collision_cursor = 0U;
                walker_motion_cursor = 0U;
                walker_delay =
                    static_cast<std::uint32_t>(walker_delay + 45U);
                walker_entry = read_bank_03(table_pointer);
                if (!walker_entry.has_value()
                    || *walker_entry >= 0xFEU) {
                    walker_valid = false;
                    break;
                }
            }
            // Reverse-oriented table entries execute SMB0 $35, a separate
            // read-modify-write timing class not present on this route.
            if ((*walker_entry & 0x80U) != 0U) {
                walker_valid = false;
                break;
            }
            walker_table_entry = *walker_entry;
            const auto motion_selector =
                static_cast<std::uint16_t>(
                    0x6C3AU
                    + (static_cast<std::uint16_t>(
                           walker_table_entry & 0x7FU)
                       << 1U));
            const auto motion_pointer_low =
                read_bank_03(motion_selector);
            const auto motion_pointer_high = read_bank_03(
                static_cast<std::uint16_t>(
                    motion_selector + 1U));
            if (!motion_pointer_low.has_value()
                || !motion_pointer_high.has_value()) {
                walker_valid = false;
                break;
            }
            walker_motion_pointer =
                static_cast<std::uint16_t>(
                    *motion_pointer_low
                    | (static_cast<std::uint16_t>(
                           *motion_pointer_high)
                       << 8U));
            walker_writes.push_back({
                .instruction = {0x03U, 0x69BBU},
                .logical_address = 0x202DU,
                .value = static_cast<std::uint8_t>(
                    walker_motion_pointer & 0x00FFU),
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        306U + walker_delay),
            });
            walker_writes.push_back({
                .instruction = {0x03U, 0x69C0U},
                .logical_address = 0x202EU,
                .value = static_cast<std::uint8_t>(
                    walker_motion_pointer >> 8U),
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        333U + walker_delay),
            });
            const auto walker_header = read_bank_03(
                static_cast<std::uint16_t>(
                    walker_motion_pointer + walker_motion_cursor));
            if (!walker_header.has_value()) {
                walker_valid = false;
                break;
            }
            if (*walker_header == 0xC0U) {
                walker_collision_cursor =
                    static_cast<std::uint8_t>(
                        walker_collision_cursor + 1U);
                walker_motion_cursor = 0U;
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69CBU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E00U + entry.registers.x),
                    .value = walker_collision_cursor,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            402U + walker_delay),
                });
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69CEU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E80U + entry.registers.x),
                    .value = 0U,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            417U + walker_delay),
                });
                walker_delay =
                    static_cast<std::uint32_t>(
                        walker_delay + 264U);
                continue;
            }
            if ((*walker_header & 0x80U) == 0U) {
                walker_valid = false;
                break;
            }
            walker_motion_header = *walker_header;
            const auto walker_delta = read_bank_03(
                static_cast<std::uint16_t>(
                    walker_motion_pointer
                    + walker_motion_cursor + 1U));
            if (!walker_delta.has_value()) {
                walker_valid = false;
                break;
            }
            walker_motion_delta = *walker_delta;
            walker_record_ready = true;
            break;
        }
        if (!walker_record_ready && !walker_terminates) {
            walker_valid = false;
        }
    }

    const auto walker_pushed_status =
        static_cast<std::uint8_t>(
            (entry.registers.status & 0x4DU) | 0x10U);
    const auto append_walker_entry_writes =
        [&entry, &pass, walker_pushed_status](
            std::vector<ReplacementMemoryWrite>& writes) {
        writes.push_back({
            .instruction = {0x03U, 0x696BU},
            .logical_address = 0x202FU,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer & 0x00FFU),
            .master_cycle_offset = 39U,
        });
        writes.push_back({
            .instruction = {0x03U, 0x6970U},
            .logical_address = 0x2030U,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer >> 8U),
            .master_cycle_offset = 66U,
        });
        writes.push_back({
            .instruction = {0x03U, 0x697CU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = walker_pushed_status,
            .master_cycle_offset = 114U,
        });
        writes.push_back({
            .instruction = {0x03U, 0x697FU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + pass.collision_work_ram_offset),
            .value = pass.collision_state_next,
            .master_cycle_offset = 135U,
        });
    };
    if (walker_valid && walker_terminates) {
        auto output = entry.registers;
        output.program_counter = 0x69A1U;
        output.accumulator = 0xFFU;
        output.y = walker_collision_cursor;
        output.status = static_cast<std::uint8_t>(
            (walker_pushed_status & 0x5CU) | 0x03U);

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x69A1U,
            },
            .output_registers = output,
            .body_master_cycles =
                static_cast<std::uint32_t>(
                    231U + walker_delay),
        };
        replacement.work_ram_writes.reserve(
            4U + walker_writes.size());
        append_walker_entry_writes(
            replacement.work_ram_writes);
        replacement.work_ram_writes.insert(
            replacement.work_ram_writes.end(),
            walker_writes.begin(),
            walker_writes.end());
        return replacement;
    }
    if (walker_valid && walker_record_ready) {
        dc_collision_table_record walker_record{};
        if (!dc_advance_collision_table_record(
                reinterpret_cast<const std::uint8_t*>(
                    work_ram.data()),
                work_ram.size(),
                entry.registers.x,
                table_pointer,
                walker_table_entry,
                walker_motion_pointer,
                walker_motion_header,
                walker_motion_delta,
                &walker_record)) {
            throw std::runtime_error(
                "Native 6966 walker rejected a resolved record.");
        }
        walker_record.cursor_after_header =
            static_cast<std::uint8_t>(
                walker_motion_cursor + 1U);
        walker_record.cursor_after_delta =
            static_cast<std::uint8_t>(
                walker_record.cursor_after_header + 1U);

        const auto horizontal_direction =
            static_cast<std::uint8_t>(
                walker_motion_delta & 0x60U);
        const auto vertical_direction =
            static_cast<std::uint8_t>(
                walker_motion_delta & 0x18U);
        std::uint32_t horizontal_delay = 0U;
        if (horizontal_direction == 0x20U) {
            horizontal_delay =
                51U
                + (walker_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (horizontal_direction == 0x40U) {
            horizontal_delay =
                60U
                + (walker_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        }
        std::uint32_t vertical_delay = 0U;
        if (vertical_direction == 0x08U) {
            vertical_delay =
                51U
                + (walker_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (vertical_direction == 0x10U) {
            vertical_delay =
                60U
                + (walker_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        }
        const auto record_delay =
            static_cast<std::uint32_t>(walker_delay + 123U);
        const auto cursor_write_cycle =
            static_cast<std::uint32_t>(
                483U + record_delay
                + horizontal_delay + vertical_delay);

        auto output = entry.registers;
        output.program_counter = 0x6A37U;
        output.accumulator =
            vertical_direction == 0x08U
                ? 0x08U
                : (vertical_direction == 0x10U
                       ? std::to_integer<std::uint8_t>(
                             work_ram[
                                 walker_record
                                     .vertical_low_work_ram_offset])
                       : 0U);
        output.y = walker_record.cursor_after_header;
        output.status = static_cast<std::uint8_t>(
            walker_pushed_status & 0x5CU);
        if (walker_record.cursor_after_delta == 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x02U);
        } else if (
            (walker_record.cursor_after_delta & 0x80U) != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x80U);
        }

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x6A37U,
            },
            .output_registers = output,
            .body_master_cycles = cursor_write_cycle + 6U,
        };
        replacement.work_ram_writes.reserve(
            16U + walker_writes.size());
        append_walker_entry_writes(
            replacement.work_ram_writes);
        replacement.work_ram_writes.insert(
            replacement.work_ram_writes.end(),
            walker_writes.begin(),
            walker_writes.end());
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x69D9U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + walker_record.event_work_ram_offset),
            .value = walker_record.event,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    420U + walker_delay),
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x69EEU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U
                + walker_record.orientation_work_ram_offset),
            .value = walker_record.orientation,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    480U + walker_delay),
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x69F1U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + walker_record.cursor_work_ram_offset),
            .value = walker_record.cursor_after_header,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    501U + walker_delay),
        });
        if (horizontal_direction == 0x20U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x69FFU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record
                          .horizontal_low_work_ram_offset),
                .value = walker_record.horizontal_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        450U + record_delay),
            });
            if (walker_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A04U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .horizontal_high_work_ram_offset),
                    .value = walker_record.horizontal_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            477U + record_delay),
                });
            }
        } else if (horizontal_direction == 0x40U) {
            if (walker_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A0EU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .horizontal_high_work_ram_offset),
                    .value = walker_record.horizontal_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            477U + record_delay),
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A11U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record
                          .horizontal_low_work_ram_offset),
                .value = walker_record.horizontal_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        (walker_record.horizontal_high_written != 0U
                             ? 498U
                             : 483U)
                        + record_delay),
            });
        }
        if (vertical_direction == 0x08U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A1EU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record.vertical_low_work_ram_offset),
                .value = walker_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        489U + record_delay
                        + horizontal_delay),
            });
            if (walker_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A23U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .vertical_high_work_ram_offset),
                    .value = walker_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + record_delay
                            + horizontal_delay),
                });
            }
        } else if (vertical_direction == 0x10U) {
            if (walker_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A2DU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .vertical_high_work_ram_offset),
                    .value = walker_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + record_delay
                            + horizontal_delay),
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A30U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record.vertical_low_work_ram_offset),
                .value = walker_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        (walker_record.vertical_high_written != 0U
                             ? 537U
                             : 522U)
                        + record_delay + horizontal_delay),
            });
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6A33U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + walker_record.cursor_work_ram_offset),
            .value = walker_record.cursor_after_delta,
            .master_cycle_offset = cursor_write_cycle,
        });
        return replacement;
    }

    const auto collision_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E00U + entry.registers.x]);
    const auto resolved_table_entry = read_bank_03(
        static_cast<std::uint16_t>(
            table_pointer + collision_cursor));
    std::optional<std::uint16_t> resolved_motion_pointer;
    std::optional<std::uint8_t> resolved_motion_byte;
    const auto motion_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E80U + entry.registers.x]);
    if (resolved_table_entry.has_value()
        && *resolved_table_entry < 0xFEU) {
        const auto motion_selector = static_cast<std::uint16_t>(
            0x6C3AU
            + (static_cast<std::uint16_t>(
                   *resolved_table_entry & 0x7FU)
               << 1U));
        const auto motion_pointer_low =
            read_bank_03(motion_selector);
        const auto motion_pointer_high = read_bank_03(
            static_cast<std::uint16_t>(motion_selector + 1U));
        if (motion_pointer_low.has_value()
            && motion_pointer_high.has_value()) {
            resolved_motion_pointer = static_cast<std::uint16_t>(
                *motion_pointer_low
                | (static_cast<std::uint16_t>(
                       *motion_pointer_high)
                   << 8U));
            resolved_motion_byte = read_bank_03(
                static_cast<std::uint16_t>(
                    *resolved_motion_pointer + motion_cursor));
        }
    }

    dc_collision_table_record semantic_record{};
    const bool semantic_headerless_step =
        pass.collision_state == 0x80U
        && pass.collision_state_next == 0x81U
        && pass.pass_index == 1U
        && pass.returns_without_step == 0U
        && (entry.registers.status & 0x20U) == 0U
        && resolved_table_entry.has_value()
        && resolved_motion_pointer.has_value()
        && resolved_motion_byte.has_value()
        && (*resolved_motion_byte & 0x80U) == 0U
        && *resolved_motion_byte != 0xC0U
        && dc_advance_collision_table_record(
               reinterpret_cast<const std::uint8_t*>(
                   work_ram.data()),
               work_ram.size(),
               entry.registers.x,
               table_pointer,
               *resolved_table_entry,
               *resolved_motion_pointer,
               *resolved_motion_byte,
               0U,
               &semantic_record) != 0;
    if (semantic_headerless_step) {
        const auto horizontal_direction =
            static_cast<std::uint8_t>(
                *resolved_motion_byte & 0x60U);
        const auto vertical_direction =
            static_cast<std::uint8_t>(
                *resolved_motion_byte & 0x18U);
        std::uint32_t horizontal_delay = 0U;
        if (horizontal_direction == 0x20U) {
            horizontal_delay =
                51U
                + (semantic_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (horizontal_direction == 0x40U) {
            horizontal_delay =
                60U
                + (semantic_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        }
        std::uint32_t vertical_delay = 0U;
        if (vertical_direction == 0x08U) {
            vertical_delay =
                51U
                + (semantic_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (vertical_direction == 0x10U) {
            vertical_delay =
                60U
                + (semantic_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        }
        const auto cursor_write_cycle = static_cast<std::uint32_t>(
            483U + horizontal_delay + vertical_delay);
        const auto pushed_status = static_cast<std::uint8_t>(
            (entry.registers.status & 0x4DU) | 0x10U);

        auto output = entry.registers;
        output.program_counter = 0x6A37U;
        output.accumulator =
            vertical_direction == 0x08U
                ? 0x08U
                : (vertical_direction == 0x10U
                       ? std::to_integer<std::uint8_t>(
                             work_ram[
                                 semantic_record
                                     .vertical_low_work_ram_offset])
                       : 0U);
        output.y = motion_cursor;
        output.status = static_cast<std::uint8_t>(
            pushed_status & 0x5CU);
        if (semantic_record.cursor_after_delta == 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x02U);
        } else if (
            (semantic_record.cursor_after_delta & 0x80U) != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x80U);
        }

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x6A37U,
            },
            .output_registers = output,
            .body_master_cycles = cursor_write_cycle + 6U,
        };
        replacement.work_ram_writes.reserve(12U);
        replacement.work_ram_writes = {
            {
                .instruction = {0x03U, 0x696BU},
                .logical_address = 0x202FU,
                .value = static_cast<std::uint8_t>(
                    pass.table_pointer & 0x00FFU),
                .master_cycle_offset = 39U,
            },
            {
                .instruction = {0x03U, 0x6970U},
                .logical_address = 0x2030U,
                .value = static_cast<std::uint8_t>(
                    pass.table_pointer >> 8U),
                .master_cycle_offset = 66U,
            },
            {
                .instruction = {0x03U, 0x697CU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + entry.registers.stack_pointer),
                .value = pushed_status,
                .master_cycle_offset = 114U,
            },
            {
                .instruction = {0x03U, 0x697FU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + pass.collision_work_ram_offset),
                .value = pass.collision_state_next,
                .master_cycle_offset = 135U,
            },
            {
                .instruction = {0x03U, 0x6995U},
                .logical_address = 0x2035U,
                .value = 0U,
                .master_cycle_offset = 177U,
            },
            {
                .instruction = {0x03U, 0x69BBU},
                .logical_address = 0x202DU,
                .value = static_cast<std::uint8_t>(
                    semantic_record.motion_pointer & 0x00FFU),
                .master_cycle_offset = 306U,
            },
            {
                .instruction = {0x03U, 0x69C0U},
                .logical_address = 0x202EU,
                .value = static_cast<std::uint8_t>(
                    semantic_record.motion_pointer >> 8U),
                .master_cycle_offset = 333U,
            },
        };
        if (horizontal_direction == 0x20U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x69FFU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record
                          .horizontal_low_work_ram_offset),
                .value = semantic_record.horizontal_low,
                .master_cycle_offset = 450U,
            });
            if (semantic_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A04U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .horizontal_high_work_ram_offset),
                    .value = semantic_record.horizontal_high,
                    .master_cycle_offset = 477U,
                });
            }
        } else if (horizontal_direction == 0x40U) {
            if (semantic_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A0EU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .horizontal_high_work_ram_offset),
                    .value = semantic_record.horizontal_high,
                    .master_cycle_offset = 477U,
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A11U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record
                          .horizontal_low_work_ram_offset),
                .value = semantic_record.horizontal_low,
                .master_cycle_offset =
                    semantic_record.horizontal_high_written != 0U
                        ? 498U
                        : 483U,
            });
        }
        if (vertical_direction == 0x08U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A1EU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record.vertical_low_work_ram_offset),
                .value = semantic_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        489U + horizontal_delay),
            });
            if (semantic_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A23U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .vertical_high_work_ram_offset),
                    .value = semantic_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + horizontal_delay),
                });
            }
        } else if (vertical_direction == 0x10U) {
            if (semantic_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A2DU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .vertical_high_work_ram_offset),
                    .value = semantic_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + horizontal_delay),
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A30U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record.vertical_low_work_ram_offset),
                .value = semantic_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        (semantic_record.vertical_high_written != 0U
                             ? 537U
                             : 522U)
                        + horizontal_delay),
            });
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6A33U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + semantic_record.cursor_work_ram_offset),
            .value = semantic_record.cursor_after_delta,
            .master_cycle_offset = cursor_write_cycle,
        });

        const bool checkpointable_bounded_axes =
            (*resolved_table_entry & 0x80U) == 0U
            && (horizontal_direction != 0U
                || vertical_direction != 0U)
            && (horizontal_direction == 0U
                || horizontal_direction == 0x20U
                || horizontal_direction == 0x40U)
            && (vertical_direction == 0U
                || vertical_direction == 0x08U
                || vertical_direction == 0x10U)
            && semantic_record.horizontal_high_written == 0U
            && semantic_record.vertical_high_written == 0U;
        if (checkpointable_bounded_axes) {
            const auto set_nz =
                [](std::uint8_t status, const std::uint8_t value) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0xA2U));
                    if (value == 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x02U);
                    } else if ((value & 0x80U) != 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x80U);
                    }
                    return status;
                };
            const auto compare_status =
                [&set_nz](
                    std::uint8_t status,
                    const std::uint8_t left,
                    const std::uint8_t right) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0x01U));
                    status = set_nz(
                        status,
                        static_cast<std::uint8_t>(
                            left - right));
                    if (left >= right) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x01U);
                    }
                    return status;
                };
            const auto bit_status =
                [](
                    std::uint8_t status,
                    const std::uint8_t accumulator,
                    const std::uint8_t operand) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0xE2U));
                    if ((accumulator & operand) == 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x02U);
                    }
                    status = static_cast<std::uint8_t>(
                        status | (operand & 0xC0U));
                    return status;
                };
            const auto shift_left_status =
                [&set_nz](
                    std::uint8_t status,
                    const std::uint8_t original,
                    const std::uint8_t result) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0x01U));
                    status = set_nz(status, result);
                    if ((original & 0x80U) != 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x01U);
                    }
                    return status;
                };
            const auto append_checkpoint =
                [&replacement](
                    const std::uint16_t instruction_pc,
                    const std::uint16_t resume_pc,
                    const std::uint32_t master_cycle,
                    auto registers) {
                    registers.program_counter = resume_pc;
                    replacement.checkpoints.push_back({
                        .interrupt_sample_master_cycle_offset =
                            master_cycle - 3U,
                        .master_cycle_offset = master_cycle,
                        .instruction = {0x03U, instruction_pc},
                        .registers = registers,
                    });
                };

            auto checkpoint = entry.registers;
            const auto doubled_selector =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator << 1U);
            checkpoint.status = shift_left_status(
                checkpoint.status,
                checkpoint.accumulator,
                doubled_selector);
            checkpoint.accumulator = doubled_selector;
            append_checkpoint(
                0x6966U,
                0x6967U,
                6U,
                checkpoint);

            checkpoint.y = checkpoint.accumulator;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x6967U,
                0x6968U,
                12U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    pass.table_pointer & 0x00FFU);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x6968U,
                0x696BU,
                27U,
                checkpoint);
            append_checkpoint(
                0x696BU,
                0x696DU,
                39U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    pass.table_pointer >> 8U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x696DU,
                0x6970U,
                54U,
                checkpoint);
            append_checkpoint(
                0x6970U,
                0x6972U,
                66U,
                checkpoint);

            checkpoint.y = pass.collision_state;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x6972U,
                0x6975U,
                81U,
                checkpoint);
            append_checkpoint(
                0x6975U,
                0x6977U,
                87U,
                checkpoint);

            checkpoint.accumulator = checkpoint.y;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x6977U,
                0x6978U,
                93U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x7FU);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x6978U,
                0x697AU,
                99U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator ^ 0x01U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x697AU,
                0x697CU,
                105U,
                checkpoint);

            checkpoint.stack_pointer =
                static_cast<std::uint8_t>(
                    checkpoint.stack_pointer - 1U);
            append_checkpoint(
                0x697CU,
                0x697DU,
                114U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator | 0x80U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x697DU,
                0x697FU,
                120U,
                checkpoint);
            append_checkpoint(
                0x697FU,
                0x6982U,
                135U,
                checkpoint);

            checkpoint.stack_pointer =
                static_cast<std::uint8_t>(
                    checkpoint.stack_pointer + 1U);
            checkpoint.status = pushed_status;
            append_checkpoint(
                0x6982U,
                0x6983U,
                147U,
                checkpoint);
            append_checkpoint(
                0x6983U,
                0x6985U,
                153U,
                checkpoint);
            append_checkpoint(
                0x6985U,
                0x6995U,
                165U,
                checkpoint);
            append_checkpoint(
                0x6995U,
                0x6997U,
                177U,
                checkpoint);

            checkpoint.y = collision_cursor;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x6997U,
                0x699AU,
                192U,
                checkpoint);

            checkpoint.accumulator = *resolved_table_entry;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x699AU,
                0x699CU,
                213U,
                checkpoint);

            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0xFFU);
            append_checkpoint(
                0x699CU,
                0x699EU,
                219U,
                checkpoint);
            append_checkpoint(
                0x699EU,
                0x69A2U,
                231U,
                checkpoint);

            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0xFEU);
            append_checkpoint(
                0x69A2U,
                0x69A4U,
                237U,
                checkpoint);
            append_checkpoint(
                0x69A4U,
                0x69AEU,
                249U,
                checkpoint);

            checkpoint.status = bit_status(
                checkpoint.status,
                checkpoint.accumulator,
                0x80U);
            append_checkpoint(
                0x69AEU,
                0x69B0U,
                255U,
                checkpoint);
            append_checkpoint(
                0x69B0U,
                0x69B6U,
                267U,
                checkpoint);

            const auto doubled_table_entry =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator << 1U);
            checkpoint.status = shift_left_status(
                checkpoint.status,
                checkpoint.accumulator,
                doubled_table_entry);
            checkpoint.accumulator = doubled_table_entry;
            append_checkpoint(
                0x69B6U,
                0x69B7U,
                273U,
                checkpoint);

            checkpoint.y = checkpoint.accumulator;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x69B7U,
                0x69B8U,
                279U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    semantic_record.motion_pointer & 0x00FFU);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69B8U,
                0x69BBU,
                294U,
                checkpoint);
            append_checkpoint(
                0x69BBU,
                0x69BDU,
                306U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    semantic_record.motion_pointer >> 8U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69BDU,
                0x69C0U,
                321U,
                checkpoint);
            append_checkpoint(
                0x69C0U,
                0x69C2U,
                333U,
                checkpoint);

            checkpoint.y = motion_cursor;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x69C2U,
                0x69C5U,
                348U,
                checkpoint);

            checkpoint.accumulator = *resolved_motion_byte;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69C5U,
                0x69C7U,
                369U,
                checkpoint);
            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0xC0U);
            append_checkpoint(
                0x69C7U,
                0x69C9U,
                375U,
                checkpoint);
            append_checkpoint(
                0x69C9U,
                0x69D3U,
                387U,
                checkpoint);
            checkpoint.status = bit_status(
                checkpoint.status,
                checkpoint.accumulator,
                0x80U);
            append_checkpoint(
                0x69D3U,
                0x69D5U,
                393U,
                checkpoint);
            append_checkpoint(
                0x69D5U,
                0x69F7U,
                405U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x60U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69F7U,
                0x69F9U,
                411U,
                checkpoint);

            std::uint32_t vertical_load_cycle = 444U;
            if (horizontal_direction != 0U) {
                append_checkpoint(
                    0x69F9U,
                    0x69FBU,
                    417U,
                    checkpoint);
                checkpoint.status = compare_status(
                    checkpoint.status,
                    checkpoint.accumulator,
                    0x20U);
                append_checkpoint(
                    0x69FBU,
                    0x69FDU,
                    423U,
                    checkpoint);

                if (horizontal_direction == 0x20U) {
                    append_checkpoint(
                        0x69FDU,
                        0x69FFU,
                        429U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.horizontal_low);
                    append_checkpoint(
                        0x69FFU,
                        0x6A02U,
                        450U,
                        checkpoint);
                    append_checkpoint(
                        0x6A02U,
                        0x6A07U,
                        462U,
                        checkpoint);
                    append_checkpoint(
                        0x6A07U,
                        0x6A14U,
                        474U,
                        checkpoint);
                } else {
                    append_checkpoint(
                        0x69FDU,
                        0x6A09U,
                        435U,
                        checkpoint);
                    checkpoint.accumulator =
                        std::to_integer<std::uint8_t>(
                            work_ram[
                                semantic_record
                                    .horizontal_low_work_ram_offset]);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    append_checkpoint(
                        0x6A09U,
                        0x6A0CU,
                        450U,
                        checkpoint);
                    append_checkpoint(
                        0x6A0CU,
                        0x6A11U,
                        462U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.horizontal_low);
                    append_checkpoint(
                        0x6A11U,
                        0x6A14U,
                        483U,
                        checkpoint);
                }
                vertical_load_cycle =
                    horizontal_direction == 0x20U
                        ? 495U
                        : 504U;
            } else {
                append_checkpoint(
                    0x69F9U,
                    0x6A14U,
                    423U,
                    checkpoint);
            }

            checkpoint.accumulator = *resolved_motion_byte;
            checkpoint.status = set_nz(
                checkpoint.status,
                checkpoint.accumulator);
            append_checkpoint(
                0x6A14U,
                0x6A16U,
                vertical_load_cycle,
                checkpoint);
            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x18U);
            checkpoint.status = set_nz(
                checkpoint.status,
                checkpoint.accumulator);
            append_checkpoint(
                0x6A16U,
                0x6A18U,
                vertical_load_cycle + 6U,
                checkpoint);

            if (vertical_direction == 0U) {
                append_checkpoint(
                    0x6A18U,
                    0x6A33U,
                    vertical_load_cycle + 18U,
                    checkpoint);
            } else {
                append_checkpoint(
                    0x6A18U,
                    0x6A1AU,
                    vertical_load_cycle + 12U,
                    checkpoint);
                checkpoint.status = compare_status(
                    checkpoint.status,
                    checkpoint.accumulator,
                    0x08U);
                append_checkpoint(
                    0x6A1AU,
                    0x6A1CU,
                    vertical_load_cycle + 18U,
                    checkpoint);

                if (vertical_direction == 0x08U) {
                    append_checkpoint(
                        0x6A1CU,
                        0x6A1EU,
                        vertical_load_cycle + 24U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.vertical_low);
                    append_checkpoint(
                        0x6A1EU,
                        0x6A21U,
                        vertical_load_cycle + 45U,
                        checkpoint);
                    append_checkpoint(
                        0x6A21U,
                        0x6A26U,
                        vertical_load_cycle + 57U,
                        checkpoint);
                    append_checkpoint(
                        0x6A26U,
                        0x6A33U,
                        vertical_load_cycle + 69U,
                        checkpoint);
                } else {
                    append_checkpoint(
                        0x6A1CU,
                        0x6A28U,
                        vertical_load_cycle + 30U,
                        checkpoint);
                    checkpoint.accumulator =
                        std::to_integer<std::uint8_t>(
                            work_ram[
                                semantic_record
                                    .vertical_low_work_ram_offset]);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    append_checkpoint(
                        0x6A28U,
                        0x6A2BU,
                        vertical_load_cycle + 45U,
                        checkpoint);
                    append_checkpoint(
                        0x6A2BU,
                        0x6A30U,
                        vertical_load_cycle + 57U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.vertical_low);
                    append_checkpoint(
                        0x6A30U,
                        0x6A33U,
                        vertical_load_cycle + 78U,
                        checkpoint);
                }
            }

            checkpoint.status = set_nz(
                checkpoint.status,
                semantic_record.cursor_after_delta);
            append_checkpoint(
                0x6A33U,
                0x6A36U,
                cursor_write_cycle,
                checkpoint);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x01U));
            append_checkpoint(
                0x6A36U,
                0x6A37U,
                cursor_write_cycle + 6U,
                output);
        }
        return replacement;
    }

#if 0
    dc_collision_table_record record{};
    const bool has_full_record = dc_advance_collision_table_record(
        reinterpret_cast<const std::uint8_t*>(work_ram.data()),
        work_ram.size(),
        entry.registers.x,
        table_pointer,
        table_entry,
        motion_pointer,
        motion_header,
        motion_delta,
        &record) != 0;
    const bool proven_forward_full_record_path =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.y == 0U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x02U
        && has_full_record
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(work_ram[0x1E26U]) == 0U
        && record.pass.collision_work_ram_offset == 0x18A6U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2AU
        && record.motion_pointer == 0x6D56U
        && record.event_work_ram_offset == 0x1AA6U
        && record.orientation_work_ram_offset == 0x1D26U
        && record.cursor_work_ram_offset == 0x1EA6U
        && record.horizontal_low_work_ram_offset == 0x11A6U
        && record.table_entry == 0x03U
        && record.motion_header == 0x82U
        && record.motion_delta == 0x20U
        && record.event == 0x02U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.horizontal_low == 0x41U
        && record.horizontal_high == 0U
        && record.event_written != 0U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_reverse_full_record_path =
        entry.registers.accumulator == 0x2BU
        && entry.registers.x == 0x27U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x10U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A7U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2DU
        && record.motion_pointer == 0x6D9EU
        && record.event_work_ram_offset == 0x1AA7U
        && record.orientation_work_ram_offset == 0x1D27U
        && record.cursor_work_ram_offset == 0x1EA7U
        && record.horizontal_low_work_ram_offset == 0x11A7U
        && record.table_entry == 0x07U
        && record.motion_header == 0x86U
        && record.motion_delta == 0x40U
        && record.event == 0x06U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.horizontal_low == 0xD7U
        && record.event_written != 0U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_vertical_full_record_path =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.y == 0U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x12U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A8U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C30U
        && record.motion_pointer == 0x6D7AU
        && record.event_work_ram_offset == 0x1AA8U
        && record.orientation_work_ram_offset == 0x1D28U
        && record.cursor_work_ram_offset == 0x1EA8U
        && record.vertical_low_work_ram_offset == 0x1328U
        && record.table_entry == 0x05U
        && record.motion_header == 0x84U
        && record.motion_delta == 0x08U
        && record.event == 0x04U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.vertical_low == 0x83U
        && record.event_written != 0U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_second_vertical_full_record_path =
        entry.registers.accumulator == 0x2DU
        && entry.registers.x == 0x29U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x10U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A9U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C35U
        && record.motion_pointer == 0x6D7AU
        && record.event_work_ram_offset == 0x1AA9U
        && record.orientation_work_ram_offset == 0x1D29U
        && record.cursor_work_ram_offset == 0x1EA9U
        && record.vertical_low_work_ram_offset == 0x1329U
        && record.table_entry == 0x05U
        && record.motion_header == 0x84U
        && record.motion_delta == 0x08U
        && record.event == 0x04U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.vertical_low == 0x83U
        && record.event_written != 0U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_alternate_reverse_full_record_path =
        entry.registers.accumulator == 0x06U
        && entry.registers.x == 0x17U
        && entry.registers.y == 0x75U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x1897U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6B0AU
        && record.motion_pointer == 0x6DE6U
        && record.event_work_ram_offset == 0x1A97U
        && record.orientation_work_ram_offset == 0x1D17U
        && record.cursor_work_ram_offset == 0x1E97U
        && record.horizontal_low_work_ram_offset == 0x1197U
        && record.table_entry == 0x0BU
        && record.motion_header == 0x8AU
        && record.motion_delta == 0x40U
        && record.event == 0x0AU
        && record.orientation == 0x8DU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.horizontal_low == 0xF5U
        && record.event_written != 0U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_continuation_full_record_path =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x02U
        && continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A6U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2AU
        && record.motion_pointer == 0x6D56U
        && record.cursor_work_ram_offset == 0x1EA6U
        && record.vertical_low_work_ram_offset == 0x1326U
        && record.table_entry == 0x03U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 2U
        && record.cursor_after_delta == 3U
        && record.vertical_low == 0x7DU
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_second_continuation_full_record_path =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x02U
        && second_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A8U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C30U
        && record.motion_pointer == 0x6D7AU
        && record.cursor_work_ram_offset == 0x1EA8U
        && record.vertical_low_work_ram_offset == 0x1328U
        && record.table_entry == 0x05U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 2U
        && record.cursor_after_delta == 3U
        && record.vertical_low == 0x84U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_alternate_continuation_full_record_path =
        entry.registers.accumulator == 0x06U
        && entry.registers.x == 0x17U
        && entry.registers.y == 0x75U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && alternate_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x1897U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6B0AU
        && record.motion_pointer == 0x6DE6U
        && record.cursor_work_ram_offset == 0x1E97U
        && record.vertical_low_work_ram_offset == 0x1317U
        && record.table_entry == 0x0BU
        && record.motion_header == 0x10U
        && record.motion_delta == 0x10U
        && record.event_written == 0U
        && record.cursor_after_header == 2U
        && record.cursor_after_delta == 3U
        && record.vertical_low == 0xC3U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_horizontal_continuation_full_record_path =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x02U
        && horizontal_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A6U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2AU
        && record.motion_pointer == 0x6D56U
        && record.cursor_work_ram_offset == 0x1EA6U
        && record.horizontal_low_work_ram_offset == 0x11A6U
        && record.table_entry == 0x03U
        && record.motion_header == 0x20U
        && record.motion_delta == 0x20U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.horizontal_low == 0x42U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_reverse_horizontal_continuation_full_record_path =
        entry.registers.accumulator == 0x2BU
        && entry.registers.x == 0x27U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && reverse_horizontal_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A7U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2DU
        && record.motion_pointer == 0x6D9EU
        && record.cursor_work_ram_offset == 0x1EA7U
        && record.horizontal_low_work_ram_offset == 0x11A7U
        && record.table_entry == 0x07U
        && record.motion_header == 0x40U
        && record.motion_delta == 0x40U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.horizontal_low == 0xD6U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_later_vertical_continuation_full_record_path =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x12U
        && later_vertical_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A8U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C30U
        && record.motion_pointer == 0x6D7AU
        && record.cursor_work_ram_offset == 0x1EA8U
        && record.vertical_low_work_ram_offset == 0x1328U
        && record.table_entry == 0x05U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.vertical_low == 0x85U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_second_later_vertical_continuation_full_record_path =
        entry.registers.accumulator == 0x2DU
        && entry.registers.x == 0x29U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && second_later_vertical_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A9U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C35U
        && record.motion_pointer == 0x6D7AU
        && record.cursor_work_ram_offset == 0x1EA9U
        && record.vertical_low_work_ram_offset == 0x1329U
        && record.table_entry == 0x05U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.vertical_low == 0x85U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_any_later_vertical_continuation_full_record_path =
        proven_later_vertical_continuation_full_record_path
        || proven_second_later_vertical_continuation_full_record_path;
    const bool proven_any_horizontal_continuation_full_record_path =
        proven_horizontal_continuation_full_record_path
        || proven_reverse_horizontal_continuation_full_record_path;
    const bool proven_any_continuation_full_record_path =
        proven_continuation_full_record_path
        || proven_second_continuation_full_record_path
        || proven_alternate_continuation_full_record_path
        || proven_any_horizontal_continuation_full_record_path
        || proven_any_later_vertical_continuation_full_record_path;
    const bool proven_any_reverse_full_record_path =
        proven_reverse_full_record_path
        || proven_alternate_reverse_full_record_path;
    const bool proven_any_vertical_full_record_path =
        proven_vertical_full_record_path
        || proven_second_vertical_full_record_path;
    const bool proven_full_record_path =
        proven_forward_full_record_path
        || proven_any_reverse_full_record_path
        || proven_any_vertical_full_record_path
        || proven_any_continuation_full_record_path;
    if (proven_full_record_path) {
        auto output = entry.registers;
        output.program_counter = 0x6A37U;
        output.accumulator =
            proven_alternate_continuation_full_record_path
                ? 0xC4U
                : ((proven_any_vertical_full_record_path
                    || proven_continuation_full_record_path
                    || proven_second_continuation_full_record_path
                    || proven_any_later_vertical_continuation_full_record_path)
                       ? 0x08U
                       : 0U);
        output.y =
            (proven_any_horizontal_continuation_full_record_path
             || proven_any_later_vertical_continuation_full_record_path)
                ? 0x03U
                : (proven_any_continuation_full_record_path
                       ? 0x02U
                       : 0x01U);
        output.status = 0x10U;

        if (proven_any_continuation_full_record_path) {
            RoutineReplacement replacement{
                .return_instruction = {
                    .physical_bank = 0x03U,
                    .logical_pc = 0x6A37U,
                },
                .output_registers = output,
                .body_master_cycles =
                    (proven_alternate_continuation_full_record_path
                     || proven_reverse_horizontal_continuation_full_record_path)
                        ? 549U
                        : 540U,
            };
            replacement.work_ram_writes = {
                {
                    .instruction = {0x03U, 0x696BU},
                    .logical_address = 0x202FU,
                    .value = static_cast<std::uint8_t>(
                        record.pass.table_pointer & 0x00FFU),
                    .master_cycle_offset = 39U,
                },
                {
                    .instruction = {0x03U, 0x6970U},
                    .logical_address = 0x2030U,
                    .value = static_cast<std::uint8_t>(
                        record.pass.table_pointer >> 8U),
                    .master_cycle_offset = 66U,
                },
                {
                    .instruction = {0x03U, 0x697CU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2100U + entry.registers.stack_pointer),
                    .value = 0x10U,
                    .master_cycle_offset = 114U,
                },
                {
                    .instruction = {0x03U, 0x697FU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + record.pass.collision_work_ram_offset),
                    .value = record.pass.collision_state_next,
                    .master_cycle_offset = 135U,
                },
                {
                    .instruction = {0x03U, 0x6995U},
                    .logical_address = 0x2035U,
                    .value = 0U,
                    .master_cycle_offset = 177U,
                },
                {
                    .instruction = {0x03U, 0x69BBU},
                    .logical_address = 0x202DU,
                    .value = static_cast<std::uint8_t>(
                        record.motion_pointer & 0x00FFU),
                    .master_cycle_offset = 306U,
                },
                {
                    .instruction = {0x03U, 0x69C0U},
                    .logical_address = 0x202EU,
                    .value = static_cast<std::uint8_t>(
                        record.motion_pointer >> 8U),
                    .master_cycle_offset = 333U,
                },
                {
                    .instruction = {
                        0x03U,
                        proven_alternate_continuation_full_record_path
                            ? 0x6A30U
                            : (proven_reverse_horizontal_continuation_full_record_path
                                   ? 0x6A11U
                                   : (proven_horizontal_continuation_full_record_path
                                          ? 0x69FFU
                                          : 0x6A1EU)),
                    },
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + (proven_any_horizontal_continuation_full_record_path
                               ? record.horizontal_low_work_ram_offset
                               : record.vertical_low_work_ram_offset)),
                    .value =
                        proven_any_horizontal_continuation_full_record_path
                            ? record.horizontal_low
                            : record.vertical_low,
                    .master_cycle_offset =
                        proven_alternate_continuation_full_record_path
                            ? 522U
                            : (proven_reverse_horizontal_continuation_full_record_path
                                   ? 483U
                                   : (proven_horizontal_continuation_full_record_path
                                          ? 450U
                                          : 489U)),
                },
                {
                    .instruction = {0x03U, 0x6A33U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U + record.cursor_work_ram_offset),
                    .value = record.cursor_after_delta,
                    .master_cycle_offset =
                        (proven_alternate_continuation_full_record_path
                         || proven_reverse_horizontal_continuation_full_record_path)
                            ? 543U
                            : 534U,
                },
            };
            return replacement;
        }

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x6A37U,
            },
            .output_registers = output,
            .body_master_cycles =
                proven_any_reverse_full_record_path ? 672U : 663U,
        };
        replacement.work_ram_writes = {
            {
                .instruction = {0x03U, 0x696BU},
                .logical_address = 0x202FU,
                .value = static_cast<std::uint8_t>(
                    record.pass.table_pointer & 0x00FFU),
                .master_cycle_offset = 39U,
            },
            {
                .instruction = {0x03U, 0x6970U},
                .logical_address = 0x2030U,
                .value = static_cast<std::uint8_t>(
                    record.pass.table_pointer >> 8U),
                .master_cycle_offset = 66U,
            },
            {
                .instruction = {0x03U, 0x697CU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + entry.registers.stack_pointer),
                .value = 0x10U,
                .master_cycle_offset = 114U,
            },
            {
                .instruction = {0x03U, 0x697FU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + record.pass.collision_work_ram_offset),
                .value = record.pass.collision_state_next,
                .master_cycle_offset = 135U,
            },
            {
                .instruction = {0x03U, 0x6995U},
                .logical_address = 0x2035U,
                .value = 0U,
                .master_cycle_offset = 177U,
            },
            {
                .instruction = {0x03U, 0x69BBU},
                .logical_address = 0x202DU,
                .value = static_cast<std::uint8_t>(
                    record.motion_pointer & 0x00FFU),
                .master_cycle_offset = 306U,
            },
            {
                .instruction = {0x03U, 0x69C0U},
                .logical_address = 0x202EU,
                .value = static_cast<std::uint8_t>(
                    record.motion_pointer >> 8U),
                .master_cycle_offset = 333U,
            },
            {
                .instruction = {0x03U, 0x69D9U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.event_work_ram_offset),
                .value = record.event,
                .master_cycle_offset = 420U,
            },
            {
                .instruction = {0x03U, 0x69EEU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.orientation_work_ram_offset),
                .value = record.orientation,
                .master_cycle_offset = 480U,
            },
            {
                .instruction = {0x03U, 0x69F1U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.cursor_work_ram_offset),
                .value = record.cursor_after_header,
                .master_cycle_offset = 501U,
            },
            {
                .instruction = {
                    0x03U,
                    static_cast<std::uint16_t>(
                        proven_any_reverse_full_record_path
                            ? 0x6A11U
                            : (proven_any_vertical_full_record_path
                                   ? 0x6A1EU
                                   : 0x69FFU)),
                },
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + (proven_any_vertical_full_record_path
                           ? record.vertical_low_work_ram_offset
                           : record.horizontal_low_work_ram_offset)),
                .value = proven_any_vertical_full_record_path
                    ? record.vertical_low
                    : record.horizontal_low,
                .master_cycle_offset =
                    proven_any_reverse_full_record_path
                        ? 606U
                        : (proven_any_vertical_full_record_path
                               ? 612U
                               : 573U),
            },
            {
                .instruction = {0x03U, 0x6A33U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.cursor_work_ram_offset),
                .value = record.cursor_after_delta,
                .master_cycle_offset =
                    proven_any_reverse_full_record_path ? 666U : 657U,
            },
        };
        return replacement;
    }

#endif
    // This branch depends only on the ROM selector and the indexed pass
    // state. It is therefore admitted semantically for every selector/slot,
    // rather than enumerating individual dynamic call ordinals.
    if (pass.collision_state != 0x81U
        || pass.collision_state_next != 0x80U
        || pass.pass_index != 0U
        || pass.returns_without_step == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x6988U;
    output.accumulator = pass.collision_state_next;
    output.y = pass.collision_state;
    output.status = 0x12U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6988U,
        },
        .output_registers = output,
        .body_master_cycles = 165U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x696BU},
            .logical_address = 0x202FU,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer & 0x00FFU),
            .master_cycle_offset = 39U,
        },
        {
            .instruction = {0x03U, 0x6970U},
            .logical_address = 0x2030U,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer >> 8U),
            .master_cycle_offset = 66U,
        },
        {
            .instruction = {0x03U, 0x697CU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0x12U,
            .master_cycle_offset = 114U,
        },
        {
            .instruction = {0x03U, 0x697FU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + pass.collision_work_ram_offset),
            .value = pass.collision_state_next,
            .master_cycle_offset = 135U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    const auto doubled_selector = static_cast<std::uint8_t>(
        checkpoint.accumulator << 1U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    if ((checkpoint.accumulator & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    checkpoint.accumulator = doubled_selector;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6966U, 0x6967U, 6U, checkpoint);

    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x6967U, 0x6968U, 12U, checkpoint);

    checkpoint.accumulator = static_cast<std::uint8_t>(
        pass.table_pointer & 0x00FFU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6968U, 0x696BU, 27U, checkpoint);
    append_checkpoint(0x696BU, 0x696DU, 39U, checkpoint);

    checkpoint.accumulator = static_cast<std::uint8_t>(
        pass.table_pointer >> 8U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x696DU, 0x6970U, 54U, checkpoint);
    append_checkpoint(0x6970U, 0x6972U, 66U, checkpoint);

    checkpoint.y = pass.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x6972U, 0x6975U, 81U, checkpoint);
    append_checkpoint(0x6975U, 0x6977U, 87U, checkpoint);

    checkpoint.accumulator = checkpoint.y;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6977U, 0x6978U, 93U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x7FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6978U, 0x697AU, 99U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator ^ 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x697AU, 0x697CU, 105U, checkpoint);

    const auto pushed_status = static_cast<std::uint8_t>(
        checkpoint.status | 0x10U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0x697CU, 0x697DU, 114U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator | 0x80U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x697DU, 0x697FU, 120U, checkpoint);
    append_checkpoint(0x697FU, 0x6982U, 135U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.status = pushed_status;
    append_checkpoint(0x6982U, 0x6983U, 147U, checkpoint);
    append_checkpoint(0x6983U, 0x6987U, 159U, checkpoint);
    append_checkpoint(0x6987U, 0x6988U, 165U, output);
    return replacement;
}

// The four `RTS` instructions the interpreter can end on. Both the transcript
// and the older result-computing replacement report one of these as the
// boundary they stopped in front of, so this covers every path through the
// routine and not only the transcribed ones.
[[nodiscard]] constexpr bool is_collision_table_return(
    const std::uint16_t logical_pc)
{
    return logical_pc == 0x6988U || logical_pc == 0x6994U
        || logical_pc == 0x69A1U || logical_pc == 0x6A37U;
}

// The interpreter as the core enters it, which is not how the three sites that
// run it in line enter it.
//
// `03:6A37` and `03:6988` are 442,578 emulated master cycles between them, the
// largest single pool left anywhere on the route, and they are exactly the
// returns the interpreter stops in front of. It cannot absorb them itself. The
// same function serves this entry and three composition sites -- the Sorcerer
// tick's two calls, `03:7F57` and `03:73AC` -- and every one of those reads
// `return_instruction` to learn which `RTS` the child stopped on so that it can
// transcribe that return into its own transcript. A routine that ran past its
// own return would hand all three an address in the caller and a body twenty-one
// cycles longer than the one they are splicing, and they would transcribe the
// return a second time on top of it.
//
// So the absorption belongs to the entry rather than to the routine. This is
// the only thing the registry dispatches at `03:6966`; the composition sites go
// on calling the interpreter directly and see exactly what they saw before.
[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_table_entry(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    auto replacement =
        replace_collision_table_interpreter(entry, work_ram, rom);
    if (!replacement || work_ram.size() < 0x2000U) {
        return replacement;
    }
    // The frame the return reads is the one the caller's `JSR` left, which is
    // where the walk thinks it is only if every push the routine made has been
    // popped. The interpreter's own `JSR $6995` loop balances, and its walk
    // refuses a transcript whose depth does not come back to zero, but an entry
    // that leaves on a different stack pointer than it arrived on declines the
    // absorption rather than declaring a boundary the core will never reach.
    if (replacement->return_instruction.physical_bank != 0x03U
        || !is_collision_table_return(
            replacement->return_instruction.logical_pc)
        || replacement->output_registers.stack_pointer
            != entry.registers.stack_pointer) {
        return replacement;
    }
    // The result-computing replacement answers atomically on every path the
    // transcript refuses, and an atomic body that reaches an interrupt horizon
    // is dropped whole rather than shortened, so it has to be asked first.
    if (replacement->checkpoints.empty()
        && !absorbed_return_fits_atomically(
            entry, replacement->body_master_cycles)) {
        return replacement;
    }

    const auto return_instruction = replacement->return_instruction;
    const auto return_cycle = replacement->body_master_cycles;
    const auto at_return = replacement->output_registers;
    absorb_return(
        *replacement, work_ram, entry.registers.stack_pointer,
        return_instruction, return_cycle, at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_response(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7FB1U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_collision_response response{};
    if (!dc_advance_collision_response(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &response)) {
        throw std::runtime_error(
            "Native 7FB1 replacement rejected the work-RAM snapshot.");
    }
    auto output = entry.registers;
    output.program_counter = 0x7FD1U;
    output.accumulator = response.countdown_reloaded != 0U
        ? response.countdown
        : response.response;
    // The response ADC cannot carry or overflow: response_base is a
    // multiple of four and phase contributes only its low two bits. DEC
    // supplies the final N/Z flags on the ordinary path; INC does so on
    // the reload path. All other status bits survive the routine.
    output.status =
        static_cast<std::uint8_t>(entry.registers.status & 0x3CU);
    const auto final_flag_value = response.countdown_reloaded != 0U
        ? response.phase
        : response.countdown;
    if (final_flag_value == 0U) {
        output.status =
            static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((final_flag_value & 0x80U) != 0U) {
        output.status =
            static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7FD1U,
        },
        .output_registers = output,
        .body_master_cycles =
            response.countdown_reloaded != 0U ? 162U : 126U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x7FB7U},
            .logical_address = 0x2026U,
            .value = response.response_base,
            .master_cycle_offset = 39U,
        },
        {
            .instruction = {0x03U, 0x7FC1U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.response_work_ram_offset),
            .value = response.response,
            .master_cycle_offset = 93U,
        },
        {
            .instruction = {0x03U, 0x7FC4U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.countdown_work_ram_offset),
            .value = response.countdown_reloaded != 0U
                ? 0U
                : response.countdown,
            .master_cycle_offset = 114U,
        },
    };
    if (response.countdown_reloaded != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7FCBU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.countdown_work_ram_offset),
            .value = response.countdown,
            .master_cycle_offset = 141U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7FCEU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.phase_work_ram_offset),
            .value = response.phase,
            .master_cycle_offset = 162U,
        });
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    const auto original_phase = std::to_integer<std::uint8_t>(
        work_ram[response.phase_work_ram_offset]);
    const auto masked_event =
        static_cast<std::uint8_t>(response.event & 0xFEU);
    const auto phase_bits =
        static_cast<std::uint8_t>(original_phase & 0x03U);
    const auto decremented_countdown =
        response.countdown_reloaded != 0U ? 0U : response.countdown;

    auto checkpoint = entry.registers;
    checkpoint.accumulator = response.event;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FB1U, 0x7FB4U, 15U, checkpoint);

    checkpoint.accumulator = masked_event;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FB4U, 0x7FB6U, 21U, checkpoint);

    checkpoint.accumulator = response.response_base;
    checkpoint.status = set_nz(
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U),
        checkpoint.accumulator);
    if ((masked_event & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    append_checkpoint(0x7FB6U, 0x7FB7U, 27U, checkpoint);
    append_checkpoint(0x7FB7U, 0x7FB9U, 39U, checkpoint);

    checkpoint.accumulator = original_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FB9U, 0x7FBCU, 54U, checkpoint);

    checkpoint.accumulator = phase_bits;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FBCU, 0x7FBEU, 60U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0x7FBEU, 0x7FBFU, 66U, checkpoint);

    checkpoint.accumulator = response.response;
    checkpoint.status = set_nz(
        static_cast<std::uint8_t>(checkpoint.status & ~0x41U),
        checkpoint.accumulator);
    append_checkpoint(0x7FBFU, 0x7FC1U, 78U, checkpoint);
    append_checkpoint(0x7FC1U, 0x7FC4U, 93U, checkpoint);

    checkpoint.status = set_nz(
        checkpoint.status,
        static_cast<std::uint8_t>(decremented_countdown));
    append_checkpoint(0x7FC4U, 0x7FC7U, 114U, checkpoint);

    if (response.countdown_reloaded == 0U) {
        append_checkpoint(0x7FC7U, 0x7FD1U, 126U, output);
    } else {
        append_checkpoint(0x7FC7U, 0x7FC9U, 120U, checkpoint);

        checkpoint.accumulator = response.countdown;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x7FC9U, 0x7FCBU, 126U, checkpoint);
        append_checkpoint(0x7FCBU, 0x7FCEU, 141U, checkpoint);
        append_checkpoint(0x7FCEU, 0x7FD1U, 162U, output);
    }
    return replacement;
}

// The collision response as the core enters it, for the same reason and in the
// same shape as the interpreter above: `03:7FB1` is both a registered entry and
// a call `03:7F57` splices in line, and the splice reads `return_instruction`
// to find the `RTS` it has to transcribe. `03:7FD1` was 110,856 emulated master
// cycles and this takes 90,237 of them, one per top-level entry; the rest are
// calls whose transcript an interrupt cut short before it reached the end.
//
// The routine has one return and every path emits checkpoints, so neither the
// address test nor the atomic question the interpreter has to ask can fail
// here; both are written out anyway, because what makes them unnecessary is a
// property of the transcript above rather than of this entry.
[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_response_entry(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    auto replacement = replace_collision_response(entry, work_ram);
    if (!replacement || work_ram.size() < 0x2000U) {
        return replacement;
    }
    if (replacement->return_instruction
            != RoutineKey{.physical_bank = 0x03U, .logical_pc = 0x7FD1U}
        || replacement->output_registers.stack_pointer
            != entry.registers.stack_pointer) {
        return replacement;
    }
    if (replacement->checkpoints.empty()
        && !absorbed_return_fits_atomically(
            entry, replacement->body_master_cycles)) {
        return replacement;
    }

    const auto return_instruction = replacement->return_instruction;
    const auto return_cycle = replacement->body_master_cycles;
    const auto at_return = replacement->output_registers;
    absorb_return(
        *replacement, work_ram, entry.registers.stack_pointer,
        return_instruction, return_cycle, at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_consumer_no_hit(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7F57U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    const auto slot = entry.registers.x;
    const auto collision = std::to_integer<std::uint8_t>(
        work_ram[0x1800U + slot]);
    if ((collision & 0x80U) != 0U) {
        return std::nullopt;
    }

    auto table_entry = entry;
    table_entry.routine = {
        .physical_bank = 0x03U,
        .logical_pc = 0x6966U,
    };
    table_entry.registers.program_counter = 0x6966U;
    table_entry.registers.accumulator =
        std::to_integer<std::uint8_t>(work_ram[0x1780U + slot]);
    table_entry.registers.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    table_entry.registers.status = static_cast<std::uint8_t>(
        entry.registers.status & static_cast<std::uint8_t>(~0x82U));
    if (table_entry.registers.accumulator == 0U) {
        table_entry.registers.status = static_cast<std::uint8_t>(
            table_entry.registers.status | 0x02U);
    } else if ((table_entry.registers.accumulator & 0x80U) != 0U) {
        table_entry.registers.status = static_cast<std::uint8_t>(
            table_entry.registers.status | 0x80U);
    }

    auto table = replace_collision_table_interpreter(
        table_entry,
        work_ram,
        rom);
    // The interpreter has four `RTS` instructions -- $6988, $6994, $69A1 and
    // $6A37 -- and this route only ever reaches the two named here. Lifting the
    // restriction changes nothing measurable: admission stays at 20,862 of
    // 20,867 and all three artifacts hash identically, because no call returns
    // through the other two. It stays because the other two are unexercised,
    // not because the code behind it is specific to these -- both post-`RTS`
    // boundaries below take the address from the child. That is what `03:73A6`
    // failed to do, and the gate that hid it there was a body-length gate,
    // which this is not.
    if (!table
        || table->return_instruction.physical_bank != 0x03U
        || (table->return_instruction.logical_pc != 0x6988U
            && table->return_instruction.logical_pc != 0x6A37U)) {
        return std::nullopt;
    }

    // The response helper observes the collision-table helper's committed
    // state. Compose against a private snapshot so full-record transactions
    // can feed their new event byte into 7FB1 without mutating live RAM.
    std::vector<std::byte> response_work_ram(
        work_ram.begin(),
        work_ram.end());
    for (const auto& write : table->work_ram_writes) {
        if (write.logical_address < 0x2000U) {
            return std::nullopt;
        }
        const auto offset = static_cast<std::size_t>(
            write.logical_address - 0x2000U);
        if (offset >= response_work_ram.size()) {
            return std::nullopt;
        }
        response_work_ram[offset] =
            static_cast<std::byte>(write.value);
    }

    auto response_entry = entry;
    response_entry.routine = {
        .physical_bank = 0x03U,
        .logical_pc = 0x7FB1U,
    };
    response_entry.registers = table->output_registers;
    response_entry.registers.program_counter = 0x7FB1U;
    response_entry.registers.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    auto response = replace_collision_response(
        response_entry,
        response_work_ram);
    if (!response
        || response->return_instruction.physical_bank != 0x03U
        || response->return_instruction.logical_pc != 0x7FD1U) {
        return std::nullopt;
    }

    // The state on the `RTS` at $7F80's own boundary. This used to be the
    // transcript's output; it is now the instant before the return rather than
    // the end of the walk, because the walk goes past it.
    auto at_return = entry.registers;
    at_return.program_counter = 0x7F80U;
    at_return.accumulator = collision;
    at_return.y = response->output_registers.y;
    at_return.status = static_cast<std::uint8_t>(
        response->output_registers.status
        & static_cast<std::uint8_t>(~0x82U));
    if (collision == 0U) {
        at_return.status =
            static_cast<std::uint8_t>(at_return.status | 0x02U);
    } else if ((collision & 0x80U) != 0U) {
        at_return.status =
            static_cast<std::uint8_t>(at_return.status | 0x80U);
    }

    constexpr std::uint32_t kFirstChildOffset = 36U;
    const auto second_jsr_offset = static_cast<std::uint32_t>(
        kFirstChildOffset + table->body_master_cycles + 21U);
    const auto response_child_offset = static_cast<std::uint32_t>(
        second_jsr_offset + 21U);
    const auto response_end = static_cast<std::uint32_t>(
        response_child_offset + response->body_master_cycles);
    // Where the `RTS` at $7F80 begins. The `BPL` at $7F63 lands on it, so this
    // is the last boundary of the no-hit arm and what the whole transcript used
    // to be worth.
    const auto return_offset = static_cast<std::uint32_t>(response_end + 48U);
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7F80U,
        },
        .output_registers = at_return,
        .body_master_cycles = return_offset,
    };
    replacement.work_ram_writes.reserve(
        4U
        + table->work_ram_writes.size()
        + response->work_ram_writes.size());
    const auto outer_stack_high = entry.registers.stack_pointer;
    const auto outer_stack_low = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 1U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5AU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_high),
        .value = 0x7FU,
        .master_cycle_offset = 15U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5AU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_low),
        .value = 0x5CU,
        .master_cycle_offset = 15U,
    });
    for (auto write : table->work_ram_writes) {
        write.master_cycle_offset += kFirstChildOffset;
        replacement.work_ram_writes.push_back(write);
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5DU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_high),
        .value = 0x7FU,
        .master_cycle_offset = second_jsr_offset,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5DU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_low),
        .value = 0x5FU,
        .master_cycle_offset = second_jsr_offset,
    });
    for (auto write : response->work_ram_writes) {
        write.master_cycle_offset += response_child_offset;
        replacement.work_ram_writes.push_back(write);
    }

    if (!table->checkpoints.empty()
        && !response->checkpoints.empty()) {
        const auto set_nz =
            [](std::uint8_t status, const std::uint8_t value) {
                status = static_cast<std::uint8_t>(
                    status & static_cast<std::uint8_t>(~0xA2U));
                if (value == 0U) {
                    status =
                        static_cast<std::uint8_t>(status | 0x02U);
                } else if ((value & 0x80U) != 0U) {
                    status =
                        static_cast<std::uint8_t>(status | 0x80U);
                }
                return status;
            };
        const auto append_checkpoint =
            [&replacement](
                const std::uint16_t instruction_pc,
                const std::uint16_t resume_pc,
                const std::uint32_t interrupt_sample_master_cycle,
                const std::uint32_t master_cycle,
                auto registers) {
                registers.program_counter = resume_pc;
                replacement.checkpoints.push_back({
                    .interrupt_sample_master_cycle_offset =
                        interrupt_sample_master_cycle,
                    .master_cycle_offset = master_cycle,
                    .instruction = {0x03U, instruction_pc},
                    .registers = registers,
                });
            };

        auto checkpoint = entry.registers;
        checkpoint.accumulator =
            std::to_integer<std::uint8_t>(
                work_ram[0x1780U + slot]);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            0x7F57U,
            0x7F5AU,
            12U,
            15U,
            checkpoint);

        append_checkpoint(
            0x7F5AU,
            0x6966U,
            33U,
            kFirstChildOffset,
            table_entry.registers);

        for (auto child_checkpoint : table->checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset +=
                kFirstChildOffset;
            child_checkpoint.master_cycle_offset +=
                kFirstChildOffset;
            replacement.checkpoints.push_back(child_checkpoint);
        }

        auto after_table_rts = table->output_registers;
        after_table_rts.stack_pointer = entry.registers.stack_pointer;
        append_checkpoint(
            table->return_instruction.logical_pc,
            0x7F5DU,
            second_jsr_offset - 3U,
            second_jsr_offset,
            after_table_rts);

        append_checkpoint(
            0x7F5DU,
            0x7FB1U,
            response_child_offset - 3U,
            response_child_offset,
            response_entry.registers);

        for (auto child_checkpoint : response->checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset +=
                response_child_offset;
            child_checkpoint.master_cycle_offset +=
                response_child_offset;
            replacement.checkpoints.push_back(child_checkpoint);
        }

        auto after_response_rts = response->output_registers;
        after_response_rts.stack_pointer =
            entry.registers.stack_pointer;
        append_checkpoint(
            response->return_instruction.logical_pc,
            0x7F60U,
            response_end + 18U,
            response_end + 21U,
            after_response_rts);

        checkpoint = after_response_rts;
        checkpoint.accumulator = collision;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            0x7F60U,
            0x7F63U,
            response_end + 33U,
            response_end + 36U,
            checkpoint);
        append_checkpoint(
            0x7F63U,
            0x7F80U,
            response_end + 45U,
            response_end + 48U,
            at_return);
    }

    // Absorb the return.
    //
    // $7F80 was the single largest emulated `RTS` left on the route -- 21,010
    // executions and 441,213 master cycles -- and it opened the bank 03 rollout
    // because nothing composes this routine: it is entered from the actor
    // dispatcher and returns straight to it, so the absorbed address is a
    // caller rather than a parent walk's next instruction. What is left at
    // $7F80 afterwards is the two-hit arm, which falls through to the same
    // return and is not transcribed here.
    //
    // The staged read the helper does earns its keep here in particular: this
    // walk writes into the stack page itself, staging both children's `JSR`
    // frames at S and S-1. Those sit below the return address at S+1/S+2 and so
    // cannot reach it, but that is a fact about this routine rather than about
    // the mechanism. It goes last because the helper closes the transcript --
    // the resume, the output registers, the length and the final checkpoint are
    // all its answer, and the writes have to be staged before it reads them.
    absorb_return(
        replacement,
        work_ram,
        entry.registers.stack_pointer,
        {0x03U, 0x7F80U},
        return_offset,
        at_return);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_mask_lookup(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCC98U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    const auto read_logical =
        [&entry, work_ram, rom](const std::uint16_t address)
            -> std::optional<std::uint8_t> {
            const auto segment = static_cast<std::uint8_t>(address >> 13U);
            const auto bank = entry.registers.mpr[segment];
            const auto bank_offset =
                static_cast<std::size_t>(address & 0x1FFFU);
            if (bank == 0xF8U) {
                return std::to_integer<std::uint8_t>(
                    work_ram[bank_offset]);
            }
            std::size_t mapped_bank = bank;
            if (rom.size() == 0x60000U && bank < 0x80U) {
                mapped_bank = bank < 0x40U
                    ? static_cast<std::size_t>(bank & 0x1FU)
                    : static_cast<std::size_t>(
                        (bank & 0x0FU) + 0x20U);
            }
            const auto rom_offset =
                mapped_bank * 0x2000U + bank_offset;
            if (rom_offset >= rom.size()) {
                return std::nullopt;
            }
            return std::to_integer<std::uint8_t>(rom[rom_offset]);
        };
    const auto byte_at = [work_ram](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset]);
    };
    const auto pointer_at = [&byte_at](const std::size_t offset) {
        return static_cast<std::uint16_t>(
            byte_at(offset)
            | (static_cast<std::uint16_t>(byte_at(offset + 1U)) << 8U));
    };

    const auto table_index = static_cast<std::uint8_t>(
        byte_at(0x002DU) + byte_at(0x002FU));
    const auto first_value = read_logical(static_cast<std::uint16_t>(
        pointer_at(0x0074U) + table_index));
    const auto second_value = read_logical(static_cast<std::uint16_t>(
        pointer_at(0x0072U)
        + static_cast<std::uint8_t>(table_index + 4U)));
    if (!first_value || !second_value) {
        return std::nullopt;
    }
    const auto mask_pointer = static_cast<std::uint16_t>(
        pointer_at(0x008EU)
        + (static_cast<std::uint16_t>(*second_value) << 3U));
    const auto mask_value = read_logical(static_cast<std::uint16_t>(
        mask_pointer + byte_at(0x005FU)));
    const auto column_bit = read_logical(static_cast<std::uint16_t>(
        0xCF2BU + byte_at(0x005EU)));
    if (!mask_value || !column_bit) {
        return std::nullopt;
    }

    dc_collision_mask_lookup lookup{};
    if (!dc_resolve_collision_mask_lookup(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            *first_value,
            *second_value,
            *mask_value,
            *column_bit,
            &lookup)) {
        throw std::runtime_error(
            "Native CC98 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = lookup.collision != 0U ? 0xCCD2U : 0xCCD4U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(9U);
    replacement.checkpoints.reserve(40U);

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t pc,
            const std::uint16_t resume_pc,
            const std::uint32_t cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {0x01U, pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement](
            const std::uint16_t pc,
            const std::uint16_t address,
            const std::uint8_t value,
            const std::uint32_t cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = cycle,
            });
        };
    const auto adc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto shift_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 6U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x21U));
    append_checkpoint(0xCC98U, 0xCC99U, cycle, checkpoint);
    cycle += 12U;
    checkpoint.accumulator = byte_at(0x002DU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCC99U, 0xCC9BU, cycle, checkpoint);
    cycle += 12U;
    const auto index_left = checkpoint.accumulator;
    checkpoint.accumulator = lookup.table_index;
    checkpoint.status = adc(
        checkpoint.status,
        index_left,
        byte_at(0x002FU),
        false,
        checkpoint.accumulator);
    append_checkpoint(0xCC9BU, 0xCC9DU, cycle, checkpoint);
    cycle += 12U;
    append_write(0xCC9DU, 0x202BU, lookup.table_index, cycle);
    append_checkpoint(0xCC9DU, 0xCC9FU, cycle, checkpoint);
    cycle += 6U;
    checkpoint.y = lookup.table_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCC9FU, 0xCCA0U, cycle, checkpoint);
    cycle += 21U;
    checkpoint.accumulator = lookup.first_table_value;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCCA0U, 0xCCA2U, cycle, checkpoint);
    cycle += 12U;
    append_write(0xCCA2U, 0x2078U, lookup.first_table_value, cycle);
    append_checkpoint(0xCCA2U, 0xCCA4U, cycle, checkpoint);
    cycle += 12U;
    checkpoint.accumulator = lookup.table_index;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCCA4U, 0xCCA6U, cycle, checkpoint);
    cycle += 6U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x21U));
    append_checkpoint(0xCCA6U, 0xCCA7U, cycle, checkpoint);
    cycle += 6U;
    checkpoint.accumulator = static_cast<std::uint8_t>(
        lookup.table_index + 4U);
    checkpoint.status = adc(
        checkpoint.status,
        lookup.table_index,
        4U,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xCCA7U, 0xCCA9U, cycle, checkpoint);
    cycle += 6U;
    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCCA9U, 0xCCAAU, cycle, checkpoint);
    cycle += 21U;
    checkpoint.accumulator = lookup.second_table_value;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCCAAU, 0xCCACU, cycle, checkpoint);
    cycle += 12U;
    append_write(0xCCACU, 0x202EU, 0U, cycle);
    append_checkpoint(0xCCACU, 0xCCAEU, cycle, checkpoint);

    std::uint8_t pointer_low = lookup.second_table_value;
    std::uint8_t pointer_high = 0U;
    for (std::size_t index = 0U; index < 3U; ++index) {
        const auto low_before = pointer_low;
        cycle += 6U;
        pointer_low = static_cast<std::uint8_t>(pointer_low << 1U);
        checkpoint.accumulator = pointer_low;
        checkpoint.status = shift_status(
            checkpoint.status, low_before, pointer_low);
        const auto asl_pc = static_cast<std::uint16_t>(
            0xCCAEU + index * 3U);
        append_checkpoint(
            asl_pc,
            static_cast<std::uint16_t>(asl_pc + 1U),
            cycle,
            checkpoint);
        const auto high_before = pointer_high;
        const bool carry_in = (checkpoint.status & 0x01U) != 0U;
        cycle += 18U;
        pointer_high = static_cast<std::uint8_t>(
            (pointer_high << 1U) | static_cast<std::uint8_t>(carry_in));
        checkpoint.status = shift_status(
            checkpoint.status, high_before, pointer_high);
        const auto rol_pc = static_cast<std::uint16_t>(asl_pc + 1U);
        append_write(rol_pc, 0x202EU, pointer_high, cycle);
        append_checkpoint(
            rol_pc,
            static_cast<std::uint16_t>(rol_pc + 2U),
            cycle,
            checkpoint);
    }
    cycle += 12U;
    append_write(0xCCB7U, 0x202DU, pointer_low, cycle);
    append_checkpoint(0xCCB7U, 0xCCB9U, cycle, checkpoint);
    cycle += 6U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x21U));
    append_checkpoint(0xCCB9U, 0xCCBAU, cycle, checkpoint);

    const auto add_pointer_byte =
        [&checkpoint,
         &cycle,
         &set_nz,
         &adc,
         &append_checkpoint,
         &append_write](
            const std::uint16_t load_pc,
            const std::uint16_t add_pc,
            const std::uint16_t store_pc,
            const std::uint16_t address,
            const std::uint8_t left,
            const std::uint8_t right) {
            cycle += 12U;
            checkpoint.accumulator = left;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(load_pc, add_pc, cycle, checkpoint);
            const bool carry_in = (checkpoint.status & 0x01U) != 0U;
            cycle += 12U;
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            checkpoint.accumulator = result;
            checkpoint.status = adc(
                checkpoint.status,
                left,
                right,
                carry_in,
                result);
            cycle += 12U;
            append_checkpoint(add_pc, store_pc, cycle - 12U, checkpoint);
            append_write(store_pc, address, result, cycle);
            append_checkpoint(
                store_pc,
                static_cast<std::uint16_t>(store_pc + 2U),
                cycle,
                checkpoint);
        };
    add_pointer_byte(
        0xCCBAU, 0xCCBCU, 0xCCBEU, 0x202DU,
        pointer_low, byte_at(0x008EU));
    add_pointer_byte(
        0xCCC0U, 0xCCC2U, 0xCCC4U, 0x202EU,
        pointer_high, byte_at(0x008FU));

    cycle += 12U;
    checkpoint.y = lookup.mask_row;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCCC6U, 0xCCC8U, cycle, checkpoint);
    cycle += 21U;
    checkpoint.accumulator = lookup.mask_value;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCCC8U, 0xCCCAU, cycle, checkpoint);
    cycle += 12U;
    checkpoint.y = lookup.mask_column;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCCCAU, 0xCCCCU, cycle, checkpoint);
    cycle += 15U;
    checkpoint.accumulator = static_cast<std::uint8_t>(
        lookup.mask_value & lookup.column_bit);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCCCCU, 0xCCCFU, cycle, checkpoint);
    if (lookup.collision != 0U) {
        cycle += 6U;
        append_checkpoint(0xCCCFU, 0xCCD1U, cycle, checkpoint);
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(0xCCD1U, 0xCCD2U, cycle, checkpoint);
    } else {
        cycle += 12U;
        append_checkpoint(0xCCCFU, 0xCCD3U, cycle, checkpoint);
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x21U));
        append_checkpoint(0xCCD3U, 0xCCD4U, cycle, checkpoint);
    }

    const auto expected_cycles =
        lookup.collision != 0U ? 378U : 384U;
    if (cycle != expected_cycles
        || replacement.work_ram_writes.size() != 9U) {
        throw std::runtime_error(
            "Native CC98 transcript diverged from its state model.");
    }
    replacement.body_master_cycles = cycle;
    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_geometry(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCCD5U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.y >= 0x80U
        || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }
    const auto read_logical =
        [&entry, work_ram, rom](const std::uint16_t address)
            -> std::optional<std::uint8_t> {
            const auto segment = static_cast<std::uint8_t>(address >> 13U);
            const auto bank = entry.registers.mpr[segment];
            const auto bank_offset =
                static_cast<std::size_t>(address & 0x1FFFU);
            if (bank == 0xF8U) {
                return std::to_integer<std::uint8_t>(
                    work_ram[bank_offset]);
            }
            std::size_t mapped_bank = bank;
            if (rom.size() == 0x60000U && bank < 0x80U) {
                mapped_bank = bank < 0x40U
                    ? static_cast<std::size_t>(bank & 0x1FU)
                    : static_cast<std::size_t>(
                        (bank & 0x0FU) + 0x20U);
            }
            const auto rom_offset =
                mapped_bank * 0x2000U + bank_offset;
            if (rom_offset >= rom.size()) {
                return std::nullopt;
            }
            return std::to_integer<std::uint8_t>(rom[rom_offset]);
        };
    const auto pointer_at =
        [work_ram](const std::size_t low_offset) {
            return static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(work_ram[low_offset])
                | (static_cast<std::uint16_t>(
                       std::to_integer<std::uint8_t>(
                           work_ram[low_offset + 1U]))
                   << 8U));
        };
    const auto indexed_address =
        [](const std::uint16_t base, const std::uint8_t index) {
            return static_cast<std::uint16_t>(base + index);
        };
    const auto target = entry.registers.y;
    const auto directory_index = std::to_integer<std::uint8_t>(
        work_ram[0x1900U + target]);
    const auto phase_index = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(
            work_ram[0x1980U + target])
        << 1U);
    const std::array<std::uint16_t, 4> directory_bases{
        pointer_at(0x0096U),
        pointer_at(0x0098U),
        pointer_at(0x009AU),
        pointer_at(0x009CU),
    };
    std::array<std::uint8_t, 4> directory_bytes{};
    for (std::size_t index = 0; index < directory_bases.size(); ++index) {
        const auto value = read_logical(indexed_address(
            directory_bases[index], directory_index));
        if (!value) {
            return std::nullopt;
        }
        directory_bytes[index] = *value;
    }

    const std::array<std::uint16_t, 2> geometry_bases{
        static_cast<std::uint16_t>(
            directory_bytes[0]
            | (static_cast<std::uint16_t>(directory_bytes[1]) << 8U)),
        static_cast<std::uint16_t>(
            directory_bytes[2]
            | (static_cast<std::uint16_t>(directory_bytes[3]) << 8U)),
    };
    std::array<std::uint8_t, 4> geometry_bytes{};
    for (std::size_t base_index = 0;
         base_index < geometry_bases.size();
         ++base_index) {
        for (std::size_t byte_index = 0; byte_index < 2U; ++byte_index) {
            const auto index = static_cast<std::uint8_t>(
                phase_index + byte_index);
            const auto value = read_logical(indexed_address(
                geometry_bases[base_index], index));
            if (!value) {
                return std::nullopt;
            }
            const auto output_index = base_index + byte_index * 2U;
            geometry_bytes[output_index] = *value;
        }
    }

    const auto descriptor_base = static_cast<std::uint16_t>(
        geometry_bytes[0]
        | (static_cast<std::uint16_t>(geometry_bytes[2]) << 8U));
    std::array<std::uint8_t, 4> descriptor{};
    for (std::size_t index = 0; index < descriptor.size(); ++index) {
        const auto value = read_logical(indexed_address(
            descriptor_base, static_cast<std::uint8_t>(index)));
        if (!value) {
            return std::nullopt;
        }
        descriptor[index] = *value;
    }

    const dc_collision_geometry_tables tables{
        .first_directory_low = directory_bytes[0],
        .first_directory_high = directory_bytes[1],
        .second_directory_low = directory_bytes[2],
        .second_directory_high = directory_bytes[3],
        .first_geometry_low = geometry_bytes[0],
        .first_geometry_high = geometry_bytes[2],
        .second_geometry_low = geometry_bytes[1],
        .second_geometry_high = geometry_bytes[3],
        .horizontal_offset = descriptor[0],
        .vertical_offset = descriptor[1],
        .horizontal_limit = descriptor[2],
        .vertical_limit = descriptor[3],
    };
    dc_collision_geometry geometry{};
    if (!dc_prepare_collision_geometry(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            target,
            &tables,
            &geometry)
        || geometry.vertical.branch
            != DC_COLLISION_SCAN_VERTICAL_DEFER
        || geometry.position_rejected != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC3U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };

    const auto target_low = std::to_integer<std::uint8_t>(
        work_ram[0x1300U + target]);
    const auto target_high = std::to_integer<std::uint8_t>(
        work_ram[0x1380U + target]);
    const auto low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(target_low)
        + tables.vertical_offset);
    const bool low_carry = low_sum > 0xFFU;
    auto output = entry.registers;
    output.program_counter = 0xCD70U;
    output.accumulator = tables.vertical_limit;
    output.y = target;
    if (geometry.vertical_offset_negative != 0U) {
        output.status = adc_status(
            output.status,
            target_high,
            0xFFU,
            low_carry,
            geometry.collision_high);
    } else {
        output.status = adc_status(
            output.status,
            0U,
            target_high,
            low_carry,
            geometry.collision_high);
    }
    output.status = set_nz(output.status, entry.registers.x);
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x01U));

    const bool vertical_negative =
        geometry.vertical.difference_high != 0U;
    std::uint32_t cycle = vertical_negative ? 222U : 120U;
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCD70U,
        },
        .output_registers = output,
    };
    replacement.work_ram_writes.reserve(19U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCCDBU},
        .logical_address = 0x2042U,
        .value = geometry.vertical.difference_low,
        .master_cycle_offset = 45U,
    });
    if (vertical_negative) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF0U},
            .logical_address = 0x2043U,
            .value = geometry.vertical.difference_high,
            .master_cycle_offset = 102U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF6U},
            .logical_address = 0x2042U,
            .value = geometry.vertical.magnitude_low,
            .master_cycle_offset = 138U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCFBU},
            .logical_address = 0x2043U,
            .value = geometry.vertical.magnitude_high,
            .master_cycle_offset = 168U,
        });
    }
    const auto add_write =
        [&replacement](
            const std::uint16_t pc,
            const std::uint16_t address,
            const std::uint8_t value,
            const std::uint32_t write_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = write_cycle,
            });
        };

    cycle += 9U;
    add_write(
        0xCD01U,
        static_cast<std::uint16_t>(
            0x2100U + entry.registers.stack_pointer),
        entry.registers.x,
        cycle);
    cycle += 6U + 15U;
    for (std::size_t index = 0; index < 4U; ++index) {
        cycle += index == 0U ? 24U : 21U;
        cycle += 12U;
        add_write(
            static_cast<std::uint16_t>(0xCD08U + index * 4U),
            static_cast<std::uint16_t>(0x202DU + index),
            directory_bytes[index],
            cycle);
    }
    cycle += 15U + 6U + 6U;
    for (std::size_t index = 0; index < 4U; ++index) {
        if (index == 2U) {
            cycle += 6U;
        }
        cycle += 21U;
        cycle += 12U;
        static constexpr std::array<std::uint16_t, 4> kGeometryPcs{
            0xCD1DU, 0xCD21U, 0xCD26U, 0xCD2AU,
        };
        static constexpr std::array<std::uint16_t, 4> kGeometryAddresses{
            0x2072U, 0x2074U, 0x2073U, 0x2075U,
        };
        add_write(
            kGeometryPcs[index],
            kGeometryAddresses[index],
            geometry_bytes[index],
            cycle);
    }
    cycle += 6U + 15U + 12U + 12U;
    add_write(0xCD33U, 0x2068U, geometry.position_low, cycle);
    cycle += 15U + 12U + 6U + 6U + 12U;
    cycle += 15U;
    cycle += 12U;
    add_write(
        0xCD41U, 0x2068U, geometry.adjusted_position_low, cycle);

    cycle += 6U;
    cycle += 21U;
    if (geometry.vertical_offset_negative != 0U) {
        cycle += 12U + 6U + 15U + 12U;
        add_write(0xCD5AU, 0x2069U, geometry.collision_low, cycle);
        cycle += 15U + 6U + 12U;
        add_write(0xCD61U, 0x206AU, geometry.collision_high, cycle);
    } else {
        cycle += 6U + 6U + 15U + 12U;
        add_write(0xCD4CU, 0x2069U, geometry.collision_low, cycle);
        cycle += 6U + 15U + 12U;
        add_write(0xCD52U, 0x206AU, geometry.collision_high, cycle);
        cycle += 12U;
    }
    cycle += 6U;
    cycle += 21U;
    cycle += 12U;
    add_write(0xCD66U, 0x206BU, tables.horizontal_limit, cycle);
    cycle += 6U;
    cycle += 21U;
    cycle += 12U;
    add_write(0xCD6BU, 0x206CU, tables.vertical_limit, cycle);
    cycle += 6U + 15U + 6U;
    replacement.body_master_cycles = cycle;

    replacement.checkpoints.reserve(64U);
    auto checkpoint = entry.registers;
    std::uint32_t checkpoint_cycle = 0U;
    const auto append_checkpoint =
        [&replacement, &checkpoint, &checkpoint_cycle](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            checkpoint_cycle += instruction_cycles;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x20U));
            checkpoint.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    checkpoint_cycle - 3U,
                .master_cycle_offset = checkpoint_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = checkpoint,
            });
        };
    const auto load_accumulator =
        [&checkpoint, &set_nz, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles,
            const std::uint8_t value) {
            checkpoint.accumulator = value;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                instruction_cycles);
        };
    const auto load_y =
        [&checkpoint, &set_nz, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles,
            const std::uint8_t value) {
            checkpoint.y = value;
            checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                instruction_cycles);
        };
    const auto subtract =
        [&checkpoint, &set_nz](
            const std::uint8_t value) {
            const auto left = checkpoint.accumulator;
            const bool carry_in =
                (checkpoint.status & 0x01U) != 0U;
            const auto difference = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(value)
                - static_cast<std::uint16_t>(!carry_in));
            const auto result =
                static_cast<std::uint8_t>(difference);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0xC3U));
            checkpoint.status = set_nz(checkpoint.status, result);
            if (difference < 0x100U) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x01U);
            }
            if (((left ^ result) & (left ^ value) & 0x80U) != 0U) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x40U);
            }
            checkpoint.accumulator = result;
        };
    const auto add =
        [&checkpoint, &set_nz](
            const std::uint8_t value) {
            const auto left = checkpoint.accumulator;
            const bool carry_in =
                (checkpoint.status & 0x01U) != 0U;
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(value)
                + static_cast<std::uint16_t>(carry_in));
            const auto result = static_cast<std::uint8_t>(sum);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0xC3U));
            checkpoint.status = set_nz(checkpoint.status, result);
            if (sum > 0xFFU) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x01U);
            }
            if (((~(left ^ value) & (left ^ result)) & 0x80U) != 0U) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x40U);
            }
            checkpoint.accumulator = result;
        };
    const auto compare_accumulator =
        [&checkpoint, &set_nz](const std::uint8_t value) {
            const auto result = static_cast<std::uint8_t>(
                checkpoint.accumulator - value);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x01U));
            checkpoint.status = set_nz(checkpoint.status, result);
            if (checkpoint.accumulator >= value) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x01U);
            }
        };
    const auto append_store =
        [&append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            append_checkpoint(instruction_pc, resume_pc, 12U);
        };

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xCCD5U, 0xCCD6U, 6U);
    load_accumulator(
        0xCCD6U,
        0xCCD8U,
        12U,
        std::to_integer<std::uint8_t>(work_ram[0x0066U]));
    subtract(target_low);
    append_checkpoint(0xCCD8U, 0xCCDBU, 15U);
    append_store(0xCCDBU, 0xCCDDU);
    load_accumulator(
        0xCCDDU,
        0xCCDFU,
        12U,
        std::to_integer<std::uint8_t>(work_ram[0x0067U]));
    subtract(target_high);
    append_checkpoint(0xCCDFU, 0xCCE2U, 15U);

    if (vertical_negative) {
        append_checkpoint(0xCCE2U, 0xCCE4U, 6U);
        append_checkpoint(0xCCE4U, 0xCCF0U, 12U);
        append_store(0xCCF0U, 0xCCF2U);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        append_checkpoint(0xCCF2U, 0xCCF3U, 6U);
        checkpoint.accumulator = 0U;
        append_checkpoint(0xCCF3U, 0xCCF4U, 6U);
        subtract(geometry.vertical.difference_low);
        append_checkpoint(0xCCF4U, 0xCCF6U, 12U);
        append_store(0xCCF6U, 0xCCF8U);
        checkpoint.accumulator = 0U;
        append_checkpoint(0xCCF8U, 0xCCF9U, 6U);
        subtract(geometry.vertical.difference_high);
        append_checkpoint(0xCCF9U, 0xCCFBU, 12U);
        append_store(0xCCFBU, 0xCCFDU);
        append_checkpoint(0xCCFDU, 0xCCFFU, 6U);
        append_checkpoint(0xCCFFU, 0xCCE8U, 12U);
    } else {
        append_checkpoint(0xCCE2U, 0xCCE8U, 12U);
    }
    load_accumulator(
        0xCCE8U,
        0xCCEAU,
        12U,
        vertical_negative
            ? geometry.vertical.magnitude_low
            : geometry.vertical.difference_low);
    compare_accumulator(0x80U);
    append_checkpoint(0xCCEAU, 0xCCECU, 6U);
    append_checkpoint(0xCCECU, 0xCCEEU, 6U);
    append_checkpoint(0xCCEEU, 0xCD01U, 12U);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U);
    append_checkpoint(0xCD01U, 0xCD02U, 9U);
    std::swap(checkpoint.x, checkpoint.y);
    append_checkpoint(0xCD02U, 0xCD03U, 9U);
    load_y(0xCD03U, 0xCD06U, 15U, directory_index);
    load_accumulator(0xCD06U, 0xCD08U, 21U, directory_bytes[0]);
    append_store(0xCD08U, 0xCD0AU);
    load_accumulator(0xCD0AU, 0xCD0CU, 21U, directory_bytes[1]);
    append_store(0xCD0CU, 0xCD0EU);
    load_accumulator(0xCD0EU, 0xCD10U, 21U, directory_bytes[2]);
    append_store(0xCD10U, 0xCD12U);
    load_accumulator(0xCD12U, 0xCD14U, 21U, directory_bytes[3]);
    append_store(0xCD14U, 0xCD16U);
    load_accumulator(
        0xCD16U,
        0xCD19U,
        15U,
        std::to_integer<std::uint8_t>(
            work_ram[0x1980U + target]));
    const auto phase = checkpoint.accumulator;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status
        & static_cast<std::uint8_t>(~0x01U));
    if ((phase & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    checkpoint.accumulator =
        static_cast<std::uint8_t>(phase << 1U);
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xCD19U, 0xCD1AU, 6U);
    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCD1AU, 0xCD1BU, 6U);
    load_accumulator(0xCD1BU, 0xCD1DU, 21U, geometry_bytes[0]);
    append_store(0xCD1DU, 0xCD1FU);
    load_accumulator(0xCD1FU, 0xCD21U, 21U, geometry_bytes[1]);
    append_store(0xCD21U, 0xCD23U);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCD23U, 0xCD24U, 6U);
    load_accumulator(0xCD24U, 0xCD26U, 21U, geometry_bytes[2]);
    append_store(0xCD26U, 0xCD28U);
    load_accumulator(0xCD28U, 0xCD2AU, 21U, geometry_bytes[3]);
    append_store(0xCD2AU, 0xCD2CU);
    checkpoint.y = 0U;
    append_checkpoint(0xCD2CU, 0xCD2DU, 6U);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xCD2DU, 0xCD2EU, 6U);
    load_accumulator(
        0xCD2EU,
        0xCD31U,
        15U,
        std::to_integer<std::uint8_t>(
            work_ram[0x1180U + target]));
    subtract(0x20U);
    append_checkpoint(0xCD31U, 0xCD33U, 6U);
    append_store(0xCD33U, 0xCD35U);
    load_accumulator(
        0xCD35U,
        0xCD38U,
        15U,
        std::to_integer<std::uint8_t>(
            work_ram[0x1200U + target]));
    subtract(0U);
    append_checkpoint(0xCD38U, 0xCD3AU, 6U);
    append_checkpoint(0xCD3AU, 0xCD3CU, 6U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status
        & static_cast<std::uint8_t>(~0x01U));
    append_checkpoint(0xCD3CU, 0xCD3DU, 6U);
    load_accumulator(
        0xCD3DU,
        0xCD3FU,
        12U,
        geometry.position_low);
    add(tables.horizontal_offset);
    append_checkpoint(0xCD3FU, 0xCD41U, 21U);
    append_store(0xCD41U, 0xCD43U);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCD43U, 0xCD44U, 6U);
    load_accumulator(
        0xCD44U,
        0xCD46U,
        21U,
        tables.vertical_offset);

    if (geometry.vertical_offset_negative != 0U) {
        append_checkpoint(0xCD46U, 0xCD56U, 12U);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status
            & static_cast<std::uint8_t>(~0x01U));
        append_checkpoint(0xCD56U, 0xCD57U, 6U);
        add(target_low);
        append_checkpoint(0xCD57U, 0xCD5AU, 15U);
        append_store(0xCD5AU, 0xCD5CU);
        load_accumulator(
            0xCD5CU,
            0xCD5FU,
            15U,
            target_high);
        add(0xFFU);
        append_checkpoint(0xCD5FU, 0xCD61U, 6U);
        append_store(0xCD61U, 0xCD63U);
    } else {
        append_checkpoint(0xCD46U, 0xCD48U, 6U);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status
            & static_cast<std::uint8_t>(~0x01U));
        append_checkpoint(0xCD48U, 0xCD49U, 6U);
        add(target_low);
        append_checkpoint(0xCD49U, 0xCD4CU, 15U);
        append_store(0xCD4CU, 0xCD4EU);
        checkpoint.accumulator = 0U;
        append_checkpoint(0xCD4EU, 0xCD4FU, 6U);
        add(target_high);
        append_checkpoint(0xCD4FU, 0xCD52U, 15U);
        append_store(0xCD52U, 0xCD54U);
        append_checkpoint(0xCD54U, 0xCD63U, 12U);
    }

    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCD63U, 0xCD64U, 6U);
    load_accumulator(
        0xCD64U,
        0xCD66U,
        21U,
        tables.horizontal_limit);
    append_store(0xCD66U, 0xCD68U);
    checkpoint.y = static_cast<std::uint8_t>(checkpoint.y + 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xCD68U, 0xCD69U, 6U);
    load_accumulator(
        0xCD69U,
        0xCD6BU,
        21U,
        tables.vertical_limit);
    append_store(0xCD6BU, 0xCD6DU);
    std::swap(checkpoint.x, checkpoint.y);
    append_checkpoint(0xCD6DU, 0xCD6EU, 9U);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.x = entry.registers.x;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xCD6EU, 0xCD6FU, 12U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status
        & static_cast<std::uint8_t>(~0x01U));
    append_checkpoint(0xCD6FU, 0xCD70U, 6U);

    if (checkpoint_cycle != replacement.body_master_cycles
        || checkpoint != replacement.output_registers) {
        throw std::runtime_error(
            "Native CCD5 checkpoint transcript diverged from its "
            "atomic transaction.");
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_scan_post_geometry_reject(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCC1AU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.y >= 0x80U
        || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x82U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC3U));
            status = set_nz(status, result);
            if (static_cast<std::uint16_t>(left) >= subtrahend) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((left ^ result) & (left ^ right) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return status;
        };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result = static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x83U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [](
            RoutineReplacement& replacement,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCC40U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(24U);
    replacement.checkpoints.reserve(112U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCC1AU},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        .value = 0xCCU,
        .master_cycle_offset = cycle,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCC1AU},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        .value = 0x1CU,
        .master_cycle_offset = cycle,
    });
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(
        replacement,
        0xCC1AU,
        0xCCD5U,
        cycle,
        checkpoint);

    RoutineEntry child_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCCD5U,
        },
        .registers = checkpoint,
    };
    child_entry.registers.program_counter = 0xCCD5U;
    auto child = replace_collision_geometry(
        child_entry,
        staged,
        rom);
    if (!child || child->checkpoints.empty()
        || child->return_instruction
            != RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCD70U,
            }) {
        return std::nullopt;
    }

    const auto child_offset = cycle;
    for (auto write : child->work_ram_writes) {
        write.master_cycle_offset += child_offset;
        replacement.work_ram_writes.push_back(write);
        staged[write.logical_address - 0x2000U] =
            static_cast<std::byte>(write.value);
    }
    for (auto child_checkpoint : child->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            child_offset;
        child_checkpoint.master_cycle_offset += child_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += child->body_master_cycles;
    checkpoint = child->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(
        replacement,
        child->return_instruction.logical_pc,
        0xCC1DU,
        cycle,
        checkpoint);

    dc_collision_scan_bounds_reject reject{};
    if (!dc_classify_collision_scan_bounds_reject(
            reinterpret_cast<const std::uint8_t*>(staged.data()),
            staged.size(),
            &reject)) {
        throw std::runtime_error(
            "Native CC1A post-geometry classifier rejected staged RAM.");
    }
    const bool in_range =
        reject.branch == DC_COLLISION_SCAN_BOUNDS_DEFER;

    cycle += 6U;
    append_checkpoint(
        replacement, 0xCC1DU, 0xCC1FU, cycle, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status | 0x01U);
    cycle += 6U;
    append_checkpoint(
        replacement, 0xCC1FU, 0xCC20U, cycle, checkpoint);
    checkpoint.accumulator = reject.horizontal_source;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC20U, 0xCC22U, cycle, checkpoint);
    checkpoint.status = sbc_status(
        checkpoint.status,
        reject.horizontal_source,
        reject.horizontal_target,
        true,
        reject.horizontal_difference);
    checkpoint.accumulator = reject.horizontal_difference;
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC22U, 0xCC24U, cycle, checkpoint);
    cycle += 12U;
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCC24U},
        .logical_address = 0x202DU,
        .value = reject.horizontal_difference,
        .master_cycle_offset = cycle,
    });
    staged[0x002DU] =
        static_cast<std::byte>(reject.horizontal_difference);
    append_checkpoint(
        replacement, 0xCC24U, 0xCC26U, cycle, checkpoint);
    checkpoint.status = compare_status(
        checkpoint.status,
        checkpoint.accumulator,
        reject.horizontal_limit);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC26U, 0xCC28U, cycle, checkpoint);

    if (reject.branch == DC_COLLISION_SCAN_BOUNDS_HORIZONTAL) {
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC28U, 0xCC3FU, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(
            replacement, 0xCC28U, 0xCC2AU, cycle, checkpoint);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status | 0x01U);
        cycle += 6U;
        append_checkpoint(
            replacement, 0xCC2AU, 0xCC2BU, cycle, checkpoint);
        checkpoint.accumulator = reject.vertical_source_low;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC2BU, 0xCC2DU, cycle, checkpoint);
        checkpoint.status = sbc_status(
            checkpoint.status,
            reject.vertical_source_low,
            reject.vertical_target_low,
            true,
            reject.vertical_difference_low);
        checkpoint.accumulator = reject.vertical_difference_low;
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC2DU, 0xCC2FU, cycle, checkpoint);
        cycle += 12U;
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCC2FU},
            .logical_address = 0x202FU,
            .value = reject.vertical_difference_low,
            .master_cycle_offset = cycle,
        });
        staged[0x002FU] =
            static_cast<std::byte>(reject.vertical_difference_low);
        append_checkpoint(
            replacement, 0xCC2FU, 0xCC31U, cycle, checkpoint);
        checkpoint.accumulator = reject.vertical_source_high;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC31U, 0xCC33U, cycle, checkpoint);
        checkpoint.status = sbc_status(
            checkpoint.status,
            reject.vertical_source_high,
            reject.vertical_target_high,
            reject.vertical_low_subtract_carry != 0U,
            reject.vertical_difference_high);
        checkpoint.accumulator = reject.vertical_difference_high;
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC33U, 0xCC35U, cycle, checkpoint);

        if (reject.branch
            == DC_COLLISION_SCAN_BOUNDS_VERTICAL_NEGATIVE) {
            cycle += 12U;
            append_checkpoint(
                replacement, 0xCC35U, 0xCC3FU, cycle, checkpoint);
        } else {
            cycle += 6U;
            append_checkpoint(
                replacement, 0xCC35U, 0xCC37U, cycle, checkpoint);
            if (reject.branch
                == DC_COLLISION_SCAN_BOUNDS_VERTICAL_PAGE) {
                cycle += 12U;
                append_checkpoint(
                    replacement,
                    0xCC37U,
                    0xCC3FU,
                    cycle,
                    checkpoint);
            } else {
                cycle += 6U;
                append_checkpoint(
                    replacement,
                    0xCC37U,
                    0xCC39U,
                    cycle,
                    checkpoint);
                checkpoint.accumulator =
                    reject.vertical_difference_low;
                checkpoint.status = set_nz(
                    checkpoint.status,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(
                    replacement,
                    0xCC39U,
                    0xCC3BU,
                    cycle,
                    checkpoint);
                checkpoint.status = compare_status(
                    checkpoint.status,
                    checkpoint.accumulator,
                    reject.vertical_limit);
                cycle += 12U;
                append_checkpoint(
                    replacement,
                    0xCC3BU,
                    0xCC3DU,
                    cycle,
                    checkpoint);
                cycle += in_range ? 12U : 6U;
                append_checkpoint(
                    replacement,
                    0xCC3DU,
                    in_range ? 0xCC41U : 0xCC3FU,
                    cycle,
                    checkpoint);
            }
        }
    }
    if (!in_range) {
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x01U));
        cycle += 6U;
        append_checkpoint(
            replacement, 0xCC3FU, 0xCC40U, cycle, checkpoint);

        checkpoint.program_counter = 0xCC40U;
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = cycle;
        const auto expected_parent_cycles =
            reject.branch == DC_COLLISION_SCAN_BOUNDS_HORIZONTAL
            ? 120U
            : reject.branch == DC_COLLISION_SCAN_BOUNDS_VERTICAL_NEGATIVE
            ? 192U
            : reject.branch == DC_COLLISION_SCAN_BOUNDS_VERTICAL_PAGE
            ? 198U
            : 222U;
        if (cycle != child->body_master_cycles + expected_parent_cycles
            || replacement.work_ram_writes.size() > 24U
            || replacement.checkpoints.size() > 112U) {
            throw std::runtime_error(
                "Native CC1A post-geometry composition exceeded its "
                "bounds.");
        }
        return replacement;
    }

    dc_collision_mask_coordinates coordinates{};
    if (!dc_prepare_collision_mask_coordinates(
            reinterpret_cast<const std::uint8_t*>(staged.data()),
            staged.size(),
            entry.registers.y,
            &coordinates)) {
        throw std::runtime_error(
            "Native CC1A mask-coordinate model rejected staged RAM.");
    }

    const auto lsr_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x01U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto stage_write =
        [&replacement, &staged, &append_checkpoint, &checkpoint, &cycle](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = cycle,
            });
            staged[logical_address - 0x2000U] =
                static_cast<std::byte>(value);
            append_checkpoint(
                replacement,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    const auto session_mode =
        std::to_integer<std::uint8_t>(staged[0x0254U]);
    cycle += 15U;
    checkpoint.accumulator = session_mode;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xCC41U, 0xCC44U, cycle, checkpoint);
    if (session_mode == 0U) {
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC44U, 0xCC64U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(
            replacement, 0xCC44U, 0xCC46U, cycle, checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.y,
            0x0AU);
        cycle += 6U;
        append_checkpoint(
            replacement, 0xCC46U, 0xCC48U, cycle, checkpoint);
        if (checkpoint.y >= 0x0AU) {
            cycle += 12U;
            append_checkpoint(
                replacement, 0xCC48U, 0xCC64U, cycle, checkpoint);
        } else {
            cycle += 6U;
            append_checkpoint(
                replacement, 0xCC48U, 0xCC4AU, cycle, checkpoint);
            const auto target_state =
                std::to_integer<std::uint8_t>(
                    staged[0x1980U + checkpoint.y]);
            checkpoint.accumulator = target_state;
            checkpoint.status = set_nz(
                checkpoint.status,
                checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                replacement, 0xCC4AU, 0xCC4DU, cycle, checkpoint);
            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0x03U);
            cycle += 6U;
            append_checkpoint(
                replacement, 0xCC4DU, 0xCC4FU, cycle, checkpoint);
            if (target_state == 0x03U) {
                cycle += 12U;
                append_checkpoint(
                    replacement,
                    0xCC4FU,
                    0xCC64U,
                    cycle,
                    checkpoint);
            } else {
                cycle += 6U;
                append_checkpoint(
                    replacement,
                    0xCC4FU,
                    0xCC51U,
                    cycle,
                    checkpoint);
                checkpoint.accumulator = checkpoint.y;
                checkpoint.status = set_nz(
                    checkpoint.status,
                    checkpoint.accumulator);
                cycle += 6U;
                append_checkpoint(
                    replacement,
                    0xCC51U,
                    0xCC52U,
                    cycle,
                    checkpoint);
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x01U);
                checkpoint.status = set_nz(
                    checkpoint.status,
                    checkpoint.accumulator);
                cycle += 6U;
                append_checkpoint(
                    replacement,
                    0xCC52U,
                    0xCC54U,
                    cycle,
                    checkpoint);
                if (checkpoint.accumulator == 0U) {
                    cycle += 12U;
                    append_checkpoint(
                        replacement,
                        0xCC54U,
                        0xCC5EU,
                        cycle,
                        checkpoint);
                    checkpoint.accumulator =
                        std::to_integer<std::uint8_t>(
                            staged[0x00B7U]);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    cycle += 12U;
                    append_checkpoint(
                        replacement,
                        0xCC5EU,
                        0xCC60U,
                        cycle,
                        checkpoint);
                    checkpoint.accumulator =
                        coordinates.side_flags;
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    cycle += 6U;
                    append_checkpoint(
                        replacement,
                        0xCC60U,
                        0xCC62U,
                        cycle,
                        checkpoint);
                    stage_write(
                        0xCC62U,
                        0xCC64U,
                        0x20B7U,
                        coordinates.side_flags,
                        12U);
                } else {
                    cycle += 6U;
                    append_checkpoint(
                        replacement,
                        0xCC54U,
                        0xCC56U,
                        cycle,
                        checkpoint);
                    checkpoint.accumulator =
                        std::to_integer<std::uint8_t>(
                            staged[0x00B7U]);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    cycle += 12U;
                    append_checkpoint(
                        replacement,
                        0xCC56U,
                        0xCC58U,
                        cycle,
                        checkpoint);
                    checkpoint.accumulator =
                        coordinates.side_flags;
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    cycle += 6U;
                    append_checkpoint(
                        replacement,
                        0xCC58U,
                        0xCC5AU,
                        cycle,
                        checkpoint);
                    stage_write(
                        0xCC5AU,
                        0xCC5CU,
                        0x20B7U,
                        coordinates.side_flags,
                        12U);
                    cycle += 12U;
                    append_checkpoint(
                        replacement,
                        0xCC5CU,
                        0xCC64U,
                        cycle,
                        checkpoint);
                }
            }
        }
    }

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[0x002DU]);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC64U, 0xCC66U, cycle, checkpoint);
    checkpoint.accumulator = coordinates.mask_column;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 6U;
    append_checkpoint(
        replacement, 0xCC66U, 0xCC68U, cycle, checkpoint);
    stage_write(
        0xCC68U,
        0xCC6AU,
        0x205EU,
        coordinates.mask_column,
        12U);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[0x002DU]);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC6AU, 0xCC6CU, cycle, checkpoint);
    for (std::uint16_t pc = 0xCC6CU; pc <= 0xCC6EU; ++pc) {
        const auto before = checkpoint.accumulator;
        checkpoint.accumulator =
            static_cast<std::uint8_t>(before >> 1U);
        checkpoint.status = lsr_status(
            checkpoint.status,
            before,
            checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            replacement,
            pc,
            static_cast<std::uint16_t>(pc + 1U),
            cycle,
            checkpoint);
    }
    stage_write(
        0xCC6FU,
        0xCC71U,
        0x202DU,
        coordinates.horizontal_cell,
        12U);

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[0x006BU]);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC71U, 0xCC73U, cycle, checkpoint);
    stage_write(
        0xCC73U,
        0xCC75U,
        0x202BU,
        checkpoint.accumulator,
        12U);
    cycle += 6U;
    checkpoint.accumulator = 0U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x20U));
    append_checkpoint(
        replacement, 0xCC75U, 0xCC76U, cycle, checkpoint);

    auto span_value =
        std::to_integer<std::uint8_t>(staged[0x002BU]);
    for (std::size_t index = 0U; index < 3U; ++index) {
        const auto lsr_pc = static_cast<std::uint16_t>(
            0xCC76U + index * 3U);
        const auto before_span = span_value;
        span_value = static_cast<std::uint8_t>(span_value >> 1U);
        checkpoint.status = lsr_status(
            checkpoint.status,
            before_span,
            span_value);
        stage_write(
            lsr_pc,
            static_cast<std::uint16_t>(lsr_pc + 2U),
            0x202BU,
            span_value,
            18U);

        const auto before_accumulator = checkpoint.accumulator;
        const bool carry_in =
            (checkpoint.status & 0x01U) != 0U;
        checkpoint.accumulator = static_cast<std::uint8_t>(
            (before_accumulator >> 1U)
            | (carry_in ? 0x80U : 0U));
        checkpoint.status = lsr_status(
            checkpoint.status,
            before_accumulator,
            checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            replacement,
            static_cast<std::uint16_t>(lsr_pc + 2U),
            static_cast<std::uint16_t>(lsr_pc + 3U),
            cycle,
            checkpoint);
    }
    if (checkpoint.accumulator == 0U) {
        cycle += 12U;
        append_checkpoint(
            replacement, 0xCC7FU, 0xCC83U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(
            replacement, 0xCC7FU, 0xCC81U, cycle, checkpoint);
        span_value = coordinates.rounded_horizontal_span;
        checkpoint.status = set_nz(checkpoint.status, span_value);
        stage_write(
            0xCC81U,
            0xCC83U,
            0x202BU,
            span_value,
            18U);
    }

    checkpoint.accumulator =
        std::to_integer<std::uint8_t>(staged[0x002FU]);
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC83U, 0xCC85U, cycle, checkpoint);
    checkpoint.accumulator = coordinates.mask_row;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 6U;
    append_checkpoint(
        replacement, 0xCC85U, 0xCC87U, cycle, checkpoint);
    stage_write(
        0xCC87U,
        0xCC89U,
        0x205FU,
        coordinates.mask_row,
        12U);

    auto vertical_cell =
        std::to_integer<std::uint8_t>(staged[0x002FU]);
    for (std::size_t index = 0U; index < 3U; ++index) {
        const auto before = vertical_cell;
        vertical_cell = static_cast<std::uint8_t>(
            vertical_cell >> 1U);
        checkpoint.status = lsr_status(
            checkpoint.status,
            before,
            vertical_cell);
        const auto pc = static_cast<std::uint16_t>(
            0xCC89U + index * 2U);
        stage_write(
            pc,
            static_cast<std::uint16_t>(pc + 2U),
            0x202FU,
            vertical_cell,
            18U);
    }

    checkpoint.accumulator = span_value;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC8FU, 0xCC91U, cycle, checkpoint);
    stage_write(
        0xCC91U,
        0xCC93U,
        0x2030U,
        coordinates.rounded_horizontal_span,
        12U);
    checkpoint.accumulator = coordinates.horizontal_cell;
    checkpoint.status = set_nz(
        checkpoint.status,
        checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(
        replacement, 0xCC93U, 0xCC95U, cycle, checkpoint);

    const auto multiply_stack_high = checkpoint.stack_pointer;
    const auto multiply_stack_low = static_cast<std::uint8_t>(
        multiply_stack_high - 1U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCC95U},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + multiply_stack_high),
        .value = 0xCCU,
        .master_cycle_offset = cycle,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCC95U},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + multiply_stack_low),
        .value = 0x97U,
        .master_cycle_offset = cycle,
    });
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(
        replacement, 0xCC95U, 0xE569U, cycle, checkpoint);

    RoutineEntry multiply_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE569U,
        },
        .registers = checkpoint,
    };
    multiply_entry.registers.program_counter = 0xE569U;
    auto multiply = replace_unsigned_byte_multiply(
        multiply_entry,
        staged);
    if (!multiply || multiply->checkpoints.empty()
        || multiply->return_instruction
            != RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE57FU,
            }) {
        return std::nullopt;
    }
    const auto multiply_offset = cycle;
    for (auto write : multiply->work_ram_writes) {
        write.master_cycle_offset += multiply_offset;
        replacement.work_ram_writes.push_back(write);
        if (write.logical_address >= 0x2000U
            && write.logical_address < 0x4000U) {
            staged[write.logical_address - 0x2000U] =
                static_cast<std::byte>(write.value);
        }
    }
    for (auto child_checkpoint : multiply->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            multiply_offset;
        child_checkpoint.master_cycle_offset += multiply_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += multiply->body_master_cycles;
    checkpoint = multiply->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    checkpoint.program_counter = 0xCC98U;
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = cycle - 3U,
        .master_cycle_offset = cycle,
        .instruction = multiply->return_instruction,
        .registers = checkpoint,
    });

    RoutineEntry mask_entry{
        .routine = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCC98U,
        },
        .registers = checkpoint,
    };
    mask_entry.registers.program_counter = 0xCC98U;
    auto mask = replace_collision_mask_lookup(
        mask_entry,
        staged,
        rom);
    if (!mask || mask->checkpoints.empty()
        || mask->return_instruction.physical_bank != 0x01U
        || (mask->return_instruction.logical_pc != 0xCCD2U
            && mask->return_instruction.logical_pc != 0xCCD4U)) {
        return std::nullopt;
    }
    const auto mask_offset = cycle;
    for (auto write : mask->work_ram_writes) {
        write.master_cycle_offset += mask_offset;
        replacement.work_ram_writes.push_back(write);
    }
    for (auto child_checkpoint : mask->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            mask_offset;
        child_checkpoint.master_cycle_offset += mask_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += mask->body_master_cycles;
    replacement.return_instruction = mask->return_instruction;
    replacement.output_registers = mask->output_registers;
    replacement.body_master_cycles = cycle;
    if (replacement.work_ram_writes.size() > 128U
        || replacement.checkpoints.size() > 256U) {
        throw std::runtime_error(
            "Native CC1A in-range composition exceeded its bounds.");
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_scan_vertical_reject(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCC1AU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_collision_scan_vertical_reject reject{};
    if (!dc_classify_collision_scan_vertical_reject(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.y,
            &reject)) {
        throw std::runtime_error(
            "Native CC1A replacement rejected the work-RAM snapshot.");
    }
    if (reject.branch == DC_COLLISION_SCAN_VERTICAL_DEFER) {
        return replace_collision_scan_post_geometry_reject(
            entry,
            work_ram,
            rom);
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x82U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC3U));
            status = set_nz(status, result);
            if (static_cast<std::uint16_t>(left) >= subtrahend) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((left ^ result) & (left ^ right) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result = static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x83U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [](
            RoutineReplacement& replacement,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    const bool negative =
        reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE
        || reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW;
    const bool compares_low =
        reject.branch == DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW
        || reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW;
    const std::uint32_t body_master_cycles =
        reject.branch == DC_COLLISION_SCAN_VERTICAL_POSITIVE_PAGE
        ? 150U
        : reject.branch == DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW
        ? 180U
        : reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE
        ? 246U
        : 282U;

    auto output = entry.registers;
    output.program_counter = 0xCC40U;
    output.accumulator = negative
        ? (compares_low ? reject.magnitude_low : reject.magnitude_high)
        : (compares_low ? reject.difference_low : reject.difference_high);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCC40U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xCC1AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = 0xCCU,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x01U, 0xCC1AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = 0x1CU,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x01U, 0xCCDBU},
            .logical_address = 0x2042U,
            .value = reject.difference_low,
            .master_cycle_offset = 66U,
        },
    };
    if (negative) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF0U},
            .logical_address = 0x2043U,
            .value = reject.difference_high,
            .master_cycle_offset = 123U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF6U},
            .logical_address = 0x2042U,
            .value = reject.magnitude_low,
            .master_cycle_offset = 159U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCFBU},
            .logical_address = 0x2043U,
            .value = reject.magnitude_high,
            .master_cycle_offset = 189U,
        });
    }

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0xCC1AU, 0xCCD5U, 21U, checkpoint);

    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status | 0x01U);
    append_checkpoint(replacement, 0xCCD5U, 0xCCD6U, 27U, checkpoint);

    checkpoint.accumulator = reject.source_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xCCD6U, 0xCCD8U, 39U, checkpoint);

    checkpoint.status = sbc_status(
        checkpoint.status,
        reject.source_low,
        reject.target_low,
        true,
        reject.difference_low);
    checkpoint.accumulator = reject.difference_low;
    append_checkpoint(replacement, 0xCCD8U, 0xCCDBU, 54U, checkpoint);
    append_checkpoint(replacement, 0xCCDBU, 0xCCDDU, 66U, checkpoint);

    checkpoint.accumulator = reject.source_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xCCDDU, 0xCCDFU, 78U, checkpoint);

    checkpoint.status = sbc_status(
        checkpoint.status,
        reject.source_high,
        reject.target_high,
        reject.low_subtract_carry != 0U,
        reject.difference_high);
    checkpoint.accumulator = reject.difference_high;
    append_checkpoint(replacement, 0xCCDFU, 0xCCE2U, 93U, checkpoint);

    std::uint32_t cycle = 93U;
    if (reject.difference_high == 0U) {
        cycle += 12U;
        append_checkpoint(replacement, 0xCCE2U, 0xCCE8U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(replacement, 0xCCE2U, 0xCCE4U, cycle, checkpoint);
        cycle += negative ? 12U : 6U;
        append_checkpoint(
            replacement,
            0xCCE4U,
            negative ? 0xCCF0U : 0xCCE6U,
            cycle,
            checkpoint);
    }

    if (negative) {
        append_checkpoint(
            replacement, 0xCCF0U, 0xCCF2U, cycle += 12U, checkpoint);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status | 0x01U);
        append_checkpoint(
            replacement, 0xCCF2U, 0xCCF3U, cycle += 6U, checkpoint);
        checkpoint.accumulator = 0U;
        append_checkpoint(
            replacement, 0xCCF3U, 0xCCF4U, cycle += 6U, checkpoint);
        checkpoint.status = sbc_status(
            checkpoint.status,
            0U,
            reject.difference_low,
            true,
            reject.magnitude_low);
        checkpoint.accumulator = reject.magnitude_low;
        append_checkpoint(
            replacement, 0xCCF4U, 0xCCF6U, cycle += 12U, checkpoint);
        append_checkpoint(
            replacement, 0xCCF6U, 0xCCF8U, cycle += 12U, checkpoint);
        checkpoint.accumulator = 0U;
        append_checkpoint(
            replacement, 0xCCF8U, 0xCCF9U, cycle += 6U, checkpoint);
        checkpoint.status = sbc_status(
            checkpoint.status,
            0U,
            reject.difference_high,
            (reject.magnitude_low == 0U),
            reject.magnitude_high);
        checkpoint.accumulator = reject.magnitude_high;
        append_checkpoint(
            replacement, 0xCCF9U, 0xCCFBU, cycle += 12U, checkpoint);
        append_checkpoint(
            replacement, 0xCCFBU, 0xCCFDU, cycle += 12U, checkpoint);
        if (reject.magnitude_high != 0U) {
            append_checkpoint(
                replacement, 0xCCFDU, 0xCCE6U, cycle += 12U, checkpoint);
        } else {
            append_checkpoint(
                replacement, 0xCCFDU, 0xCCFFU, cycle += 6U, checkpoint);
            append_checkpoint(
                replacement, 0xCCFFU, 0xCCE8U, cycle += 12U, checkpoint);
        }
    }

    if (compares_low) {
        checkpoint.accumulator = negative
            ? reject.magnitude_low
            : reject.difference_low;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            replacement, 0xCCE8U, 0xCCEAU, cycle += 12U, checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status, checkpoint.accumulator, 0x80U);
        append_checkpoint(
            replacement, 0xCCEAU, 0xCCECU, cycle += 6U, checkpoint);
        append_checkpoint(
            replacement, 0xCCECU, 0xCCE6U, cycle += 12U, checkpoint);
    }

    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status | 0x01U);
    append_checkpoint(
        replacement, 0xCCE6U, 0xCCE7U, cycle += 6U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(
        replacement, 0xCCE7U, 0xCC1DU, cycle += 21U, checkpoint);
    append_checkpoint(
        replacement, 0xCC1DU, 0xCC3FU, cycle += 12U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    append_checkpoint(
        replacement, 0xCC3FU, 0xCC40U, cycle += 6U, checkpoint);

    checkpoint.program_counter = 0xCC40U;
    replacement.output_registers = checkpoint;
    if (cycle != body_master_cycles) {
        throw std::runtime_error(
            "Native CC1A replacement composition diverged from its "
            "state model.");
    }
    return replacement;
}
} // namespace devils_crush::app::detail
