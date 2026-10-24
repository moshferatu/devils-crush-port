// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cabinet/music_transition.hpp"

namespace devils_crush::cabinet {
namespace {
constexpr emulation::RoutineKey kPerfectResult{4U, 0x46A2U};
constexpr emulation::RoutineKey kTableReturn{1U, 0xC204U};
constexpr emulation::RoutineKey kStarJingle{3U, 0x7CC1U};      // JSR $C087
constexpr emulation::RoutineKey kOtherStarJingle{5U, 0xA984U}; // JSR $C087
constexpr emulation::RoutineKey kStarResume{3U, 0x7CF6U};      // JSR $C090

bool star_pending(std::span<const std::byte> ram) noexcept
{
    return ram.size() == 8192U && ram[0x55U] == std::byte{0}
        && ram[0xCCU] != std::byte{0};
}

// Skips one 3-byte JSR. The call's own 21-cycle interval is kept; only the
// audio routine it would have run is left out.
emulation::RoutineReplacement skip_call(const emulation::RoutineEntry& entry)
{
    emulation::RoutineReplacement result;
    result.output_registers = entry.registers;
    result.output_registers.program_counter =
        static_cast<std::uint16_t>(entry.routine.logical_pc + 3U);
    result.return_instruction = {entry.routine.physical_bank,
        result.output_registers.program_counter};
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

void MusicTransition::table_return(std::span<const std::byte> ram) noexcept
{
    table_resumed_ = star_pending(ram);
}

std::optional<emulation::RoutineReplacement> MusicTransition::star_jingle(
    const emulation::RoutineEntry& entry, std::span<const std::byte> ram)
{
    if (entry.routine != kStarJingle && entry.routine != kOtherStarJingle) return std::nullopt;
    const bool skip = skip_perfect_jingle_ && perfect_pending_ && star_pending(ram);
    perfect_pending_ = false;
    if (!skip) return std::nullopt;
    return skip_call(entry);
}

std::optional<emulation::RoutineReplacement> MusicTransition::finish_star(
    const emulation::RoutineEntry& entry, std::span<const std::byte> ram)
{
    if (entry.routine != kStarResume) return std::nullopt;
    const bool redundant = table_resumed_ && star_pending(ram);
    table_resumed_ = false;
    perfect_pending_ = false;
    if (!redundant) return std::nullopt;
    return skip_call(entry);
}

bool MusicTransition::install(emulation::PceRuntime& runtime)
{
    using emulation::HookMode;
    const auto replace = [&](emulation::RoutineKey key, auto member) {
        return runtime.install_execution_hook({
            .routine = key, .mode = HookMode::replace,
            .replacement_callback = [this, member](const emulation::RoutineEntry& entry,
                std::span<const std::byte> ram) { return (this->*member)(entry, ram); },
        });
    };
    return runtime.install_execution_hook({
               .routine = kPerfectResult, .mode = HookMode::observe,
               .callback = [this](const emulation::RoutineEntry&) { perfect_result(); },
           })
        && runtime.install_execution_hook({
               .routine = kTableReturn, .mode = HookMode::observe,
               .callback = [this, &runtime](const emulation::RoutineEntry&) {
                   table_return(runtime.export_work_ram());
               },
           })
        && replace(kStarJingle, &MusicTransition::star_jingle)
        && replace(kOtherStarJingle, &MusicTransition::star_jingle)
        && replace(kStarResume, &MusicTransition::finish_star);
}
}
