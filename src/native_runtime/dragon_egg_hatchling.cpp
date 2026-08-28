// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_egg_hatchling.hpp"

#include <algorithm>
#include <ranges>

namespace devils_crush::native_runtime {
namespace {

constexpr std::array<DragonEggHatchlingPosition, kDragonEggCount>
    kEggPositions{{
        {48, 566},
        {58, 561},
        {68, 556},
        {40, 558},
        {50, 553},
        {60, 548},
    }};

} // namespace

DragonEggHatchlingAllocationStep allocate_dragon_egg_hatchling(
    DragonEggClusterState cluster,
    const std::uint8_t source_egg_slot,
    const std::uint8_t dynamic_slot,
    const std::array<std::optional<DragonEggHatchlingState>, kDragonEggCount>&
        existing_hatchlings) noexcept
{
    const auto previous_cluster = cluster;
    const auto index = dragon_egg_index_for_slot(source_egg_slot);
    if (!index || !cluster.eggs[*index].hatchling_spawn_pending) {
        return {
            .previous_cluster = previous_cluster,
            .cluster = cluster,
            .source_egg_slot = source_egg_slot,
            .dynamic_slot = dynamic_slot,
        };
    }
    if (std::ranges::any_of(existing_hatchlings, [dynamic_slot](const auto& hatchling) {
            return hatchling && hatchling->dynamic_slot == dynamic_slot;
        })) {
        return {
            .previous_cluster = previous_cluster,
            .cluster = cluster,
            .source_egg_slot = source_egg_slot,
            .dynamic_slot = dynamic_slot,
            .disposition = DragonEggHatchlingAllocationDisposition::slot_already_in_use,
        };
    }

    cluster.eggs[*index].hatchling_spawn_pending = false;
    const DragonEggHatchlingState hatchling{
        .source_egg_slot = source_egg_slot,
        .dynamic_slot = dynamic_slot,
        .position = kEggPositions[*index],
    };
    return {
        .previous_cluster = previous_cluster,
        .cluster = cluster,
        .source_egg_slot = source_egg_slot,
        .dynamic_slot = dynamic_slot,
        .disposition = DragonEggHatchlingAllocationDisposition::allocated,
        .hatchling = hatchling,
    };
}

DragonEggHatchlingAdvanceStep advance_dragon_egg_hatchling(
    DragonEggHatchlingState state,
    const bool contact_confirmed) noexcept
{
    const auto previous = state;
    if (contact_confirmed) {
        return {
            .previous = previous,
            .state = state,
            .disposition = DragonEggHatchlingAdvanceDisposition::released_on_contact,
            .score_award_units = 200U,
            .audio_command = 0x34U,
            .releases_child = true,
        };
    }

    --state.animation_timer;
    return {.previous = previous, .state = state};
}

DragonEggHatchlingRulesUpdate resolve_dragon_egg_hatchling(
    const DragonEggHatchlingState state,
    const bool contact_confirmed,
    const ScoreState score) noexcept
{
    const auto hatchling = advance_dragon_egg_hatchling(state, contact_confirmed);
    return {
        .hatchling = hatchling,
        .score_award = apply_score_award(score, hatchling.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
