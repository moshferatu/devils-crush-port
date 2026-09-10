// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"

namespace devils_crush::app::detail {
[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_three_blobs_timer_branch(const emulation::RoutineEntry& entry)
{
    using emulation::RoutineKey;
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x66A6U};
    if (entry.routine != kRoutine || entry.registers.program_counter != 0x66A6U
        || entry.registers.mpr[3] != 0x03U
        || (entry.registers.status & 0x20U) != 0U) {
        return std::nullopt;
    }

    // Resume after the independent DEC fallback using its live flags. Do not reread
    // the timer or broaden the longer prefix's timer-horizon guard.
    const bool taken = (entry.registers.status & 0x02U) == 0U;
    const std::uint32_t cycles = taken ? 12U : 6U;
    auto output = entry.registers;
    output.program_counter = taken ? 0x66B5U : 0x66A8U;
    emulation::RoutineReplacement replacement{
        .return_instruction = {0x03U, output.program_counter},
        .output_registers = output,
        .body_master_cycles = cycles,
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = cycles - 3U,
        .master_cycle_offset = cycles,
        .instruction = kRoutine,
        .registers = output,
    });
    return replacement;
}
}
