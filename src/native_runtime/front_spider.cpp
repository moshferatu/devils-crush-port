// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/front_spider.hpp"

namespace devils_crush::native_runtime {

FrontSpiderContactStep consume_front_spider_contact(
    FrontSpiderState state) noexcept
{
    const auto previous = state;
    if (state.side > 1U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = FrontSpiderContactDisposition::invalid_state,
        };
    }
    if (!state.collision_ready) {
        return {
            .previous = previous,
            .state = state,
            .disposition = FrontSpiderContactDisposition::transition_pending,
        };
    }

    state.collision_ready = false;
    return {
        .previous = previous,
        .state = state,
        .disposition = FrontSpiderContactDisposition::hit,
        .score_award_units = 40U,
        .audio_command = 0x34U,
        .sets_side_safety = true,
        .requested_transition_stages = 4U,
    };
}

FrontSpiderState complete_front_spider_transition(
    FrontSpiderState state) noexcept
{
    if (state.side <= 1U) {
        state.collision_ready = true;
    }
    return state;
}

FrontSpiderRulesUpdate resolve_front_spider_contact(
    const FrontSpiderState state,
    const ScoreState score) noexcept
{
    const auto spider = consume_front_spider_contact(state);
    return {
        .spider = spider,
        .score_award = apply_score_award(score, spider.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
