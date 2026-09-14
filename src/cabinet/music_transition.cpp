// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cabinet/music_transition.hpp"

namespace devils_crush::cabinet {
void MusicTransition::table_return(std::span<const std::byte> ram) noexcept
{
    table_resumed_ = ram.size() == 8192U && ram[0x55U] == std::byte{0}
        && ram[0xCCU] != std::byte{0};
}

std::optional<emulation::RoutineReplacement> MusicTransition::finish_star(
    const emulation::RoutineEntry& entry, std::span<const std::byte> ram)
{
    if (entry.routine != emulation::RoutineKey{3U, 0x7CF6U}) return std::nullopt;
    const bool redundant = table_resumed_ && ram.size() == 8192U
        && ram[0x55U] == std::byte{0} && ram[0xCCU] != std::byte{0};
    table_resumed_ = false;
    if (!redundant) return std::nullopt;
    // Keep the call's 21-cycle interval; the host omits only its audio restart.
    emulation::RoutineReplacement result;
    result.output_registers = entry.registers;
    result.output_registers.program_counter = 0x7CF9U;
    result.return_instruction = {3U, 0x7CF9U};
    result.body_master_cycles = 21U;
    result.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = entry.routine,
        .registers = result.output_registers,
    });
    return result;
}
}
