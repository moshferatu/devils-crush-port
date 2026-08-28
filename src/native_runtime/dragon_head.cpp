// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_head.hpp"

namespace devils_crush::native_runtime {

DragonHeadContactStep consume_dragon_head_contact(DragonHeadState state) noexcept
{
    const auto previous = state;
    if (!state.collision_ready || state.hits_remaining == 0U
        || state.mouth_opening_transition_pending) {
        return {.previous = previous, .state = state};
    }

    state.collision_ready = false;
    --state.hits_remaining;
    if (state.hits_remaining != 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = DragonHeadContactDisposition::survived_hit,
            .hit_score_award_units = 50U,
            .audio_command = 0x2EU,
            .requests_impact_effect = true,
            .requested_appearance = 3U,
        };
    }

    state.mouth_opening_transition_pending = true;
    return {
        .previous = previous,
        .state = state,
        .disposition = DragonHeadContactDisposition::mouth_opening_requested,
        .hit_score_award_units = 50U,
        .terminal_score_award_units = 50U,
        .audio_command = 0x2EU,
        .requests_impact_effect = true,
        .requested_appearance = 3U,
    };
}

bool complete_dragon_head_hit_presentation(DragonHeadState& state) noexcept
{
    if (state.collision_ready || state.mouth_opening_transition_pending
        || state.hits_remaining == 0U) {
        return false;
    }
    state.collision_ready = true;
    return true;
}

DragonHeadRulesUpdate resolve_dragon_head_contact(
    const DragonHeadState state,
    ScoreState score) noexcept
{
    const auto step = consume_dragon_head_contact(state);
    const auto hit_score_award = apply_score_award(score, step.hit_score_award_units);
    score = hit_score_award.state;
    const auto terminal_score_award = apply_score_award(
        score, step.terminal_score_award_units);
    return {
        .dragon = step,
        .hit_score_award = hit_score_award,
        .terminal_score_award = terminal_score_award,
    };
}

} // namespace devils_crush::native_runtime
