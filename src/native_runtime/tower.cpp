// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower.hpp"

namespace devils_crush::native_runtime {

TowerContactStep consume_tower_contact(
    TowerState state,
    const bool center_insect_source_already_set) noexcept
{
    const auto previous = state;
    if (!state.collision_ready) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerContactDisposition::transition_pending,
        };
    }
    if (!center_insect_source_already_set && state.hit_progress == 0U) {
        state.collision_ready = false;
        ++state.hit_progress;
        state.presentation_updates_remaining = 4U;
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerContactDisposition::first_source_hit,
            .score_award_units = 50U,
            .audio_command = 0x33U,
            .sets_center_insect_source = true,
        };
    }
    if (state.bonus_stage_selection_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerContactDisposition::bonus_stage_selection_pending,
        };
    }
    if (state.remaining_armed_passes == 0U) {
        state.collision_ready = false;
        state.bonus_stage_selection_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerContactDisposition::bonus_stage_selection_armed,
        };
    }

    --state.remaining_armed_passes;
    if (state.remaining_armed_passes == 0U) {
        state.collision_ready = false;
        state.bonus_stage_selection_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerContactDisposition::bonus_stage_selection_armed,
        };
    }
    return {
        .previous = previous,
        .state = state,
        .disposition = TowerContactDisposition::armed_pass_decremented,
    };
}

TowerPresentationStep advance_tower_presentation(TowerState state) noexcept
{
    const auto previous = state;
    if (state.presentation_updates_remaining == 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerPresentationDisposition::not_pending,
        };
    }

    --state.presentation_updates_remaining;
    if (state.presentation_updates_remaining != 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerPresentationDisposition::countdown,
        };
    }
    state.collision_ready = true;
    return {
        .previous = previous,
        .state = state,
        .disposition = TowerPresentationDisposition::steady_dispatch_installed,
    };
}

TowerBonusStageSelectionStep consume_tower_bonus_stage_selection(
    TowerState state,
    const bool negative_collision_latched) noexcept
{
    const auto previous = state;
    if (state.bonus_stage_transition_requested) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerBonusStageSelectionDisposition::transition_pending,
        };
    }
    if (!state.bonus_stage_selection_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerBonusStageSelectionDisposition::not_pending,
        };
    }
    if (!negative_collision_latched) {
        return {
            .previous = previous,
            .state = state,
            .disposition = TowerBonusStageSelectionDisposition::waiting_for_collision,
        };
    }

    state.bonus_stage_selection_pending = false;
    state.bonus_stage_transition_requested = true;
    state.collision_ready = false;
    state.remaining_armed_passes = 5U;
    return {
        .previous = previous,
        .state = state,
        .disposition = TowerBonusStageSelectionDisposition::bonus_stage_selected,
        .bonus_stage = 5U,
        .audio_command = 0x41U,
        .sets_tower_stage_flag = true,
        .requests_session_transition = true,
        .starts_next_tower_insect_cycle = true,
    };
}

TowerRulesUpdate resolve_tower_contact(
    const TowerState state,
    const bool center_insect_source_already_set,
    const ScoreState score) noexcept
{
    const auto tower = consume_tower_contact(state, center_insect_source_already_set);
    return {
        .tower = tower,
        .score_award = apply_score_award(score, tower.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
