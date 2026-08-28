// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_egg.hpp"

#include <algorithm>
#include <ranges>

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] bool all_eggs_destroyed(
    const DragonEggClusterState& state) noexcept
{
    return std::ranges::none_of(
        state.eggs, [](const DragonEggState& egg) { return egg.active; });
}

[[nodiscard]] bool all_eggs_active(const DragonEggClusterState& state) noexcept
{
    return std::ranges::all_of(
        state.eggs, [](const DragonEggState& egg) { return egg.active; });
}

} // namespace

DragonEggHitStep consume_dragon_egg_contact(
    DragonEggClusterState state,
    const std::uint8_t slot) noexcept
{
    const auto previous = state;
    const auto index = dragon_egg_index_for_slot(slot);
    if (!index || state.completed || state.reset_pending || !state.eggs[*index].active) {
        return {.previous = previous, .state = state, .slot = slot};
    }

    auto& egg = state.eggs[*index];
    egg.active = false;
    egg.hatchling_spawn_pending = true;
    state.completion_pending = all_eggs_destroyed(state);
    return {
        .previous = previous,
        .state = state,
        .slot = slot,
        .disposition = DragonEggHitDisposition::destroyed,
        .score_award_units = 10U,
        .audio_command = 0x39U,
        .requests_hatchling_spawn = true,
    };
}

DragonEggCompletionStep advance_dragon_egg_controller(
    DragonEggClusterState state) noexcept
{
    const auto previous = state;
    if (state.completed) {
        return {
            .previous = previous,
            .state = state,
            .disposition = DragonEggCompletionDisposition::already_completed,
        };
    }
    if (!state.completion_pending) {
        return {.previous = previous, .state = state};
    }

    state.completion_pending = false;
    state.completed = true;
    return {
        .previous = previous,
        .state = state,
        .disposition = DragonEggCompletionDisposition::completion_requested,
        .score_award_units = 100U,
        .requests_group_reset = true,
    };
}

DragonEggGroupResetStep begin_dragon_egg_group_reset(
    DragonEggClusterState state) noexcept
{
    const auto previous = state;
    if (!state.completed || state.reset_pending) {
        return {.previous = previous, .state = state};
    }
    state.reset_pending = true;
    return {
        .previous = previous,
        .state = state,
        .disposition = DragonEggGroupResetDisposition::reset_armed,
    };
}

DragonEggFixedResetStep advance_dragon_egg_fixed_reset(
    DragonEggClusterState state,
    const std::uint8_t slot) noexcept
{
    const auto previous = state;
    const auto index = dragon_egg_index_for_slot(slot);
    if (!index || !state.reset_pending || state.eggs[*index].active) {
        return {.previous = previous, .state = state, .slot = slot};
    }

    state.eggs[*index] = {};
    auto disposition = DragonEggFixedResetDisposition::egg_reinitialized;
    if (all_eggs_active(state)) {
        state.completed = false;
        state.reset_pending = false;
        disposition = DragonEggFixedResetDisposition::cluster_reinitialized;
    }
    return {
        .previous = previous,
        .state = state,
        .slot = slot,
        .disposition = disposition,
    };
}

DragonEggRulesUpdate resolve_dragon_egg_contact(
    const DragonEggClusterState state,
    const std::uint8_t slot,
    const ScoreState score) noexcept
{
    const auto egg = consume_dragon_egg_contact(state, slot);
    return {
        .egg = egg,
        .score_award = apply_score_award(score, egg.score_award_units),
    };
}

DragonEggCompletionRulesUpdate resolve_dragon_egg_controller(
    const DragonEggClusterState state,
    const ScoreState score) noexcept
{
    const auto controller = advance_dragon_egg_controller(state);
    return {
        .controller = controller,
        .score_award = apply_score_award(score, controller.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
