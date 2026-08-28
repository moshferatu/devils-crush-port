// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/side_spider.hpp"

namespace devils_crush::native_runtime {

SideSpiderContactStep consume_side_spider_contact(SideSpiderState state) noexcept
{
    const auto previous = state;
    if (state.side > 1U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SideSpiderContactDisposition::invalid_state,
        };
    }
    if (!state.collision_ready) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SideSpiderContactDisposition::transition_pending,
        };
    }

    state.collision_ready = false;
    return {
        .previous = previous,
        .state = state,
        .disposition = SideSpiderContactDisposition::hit,
        .score_award_units = 100U,
        .audio_command = 0x34U,
        .sets_shared_transition_latch = true,
    };
}

SideSpiderState complete_side_spider_transition(SideSpiderState state) noexcept
{
    if (state.side <= 1U) {
        state.collision_ready = true;
    }
    return state;
}

SideSpiderRulesUpdate resolve_side_spider_contact(
    const SideSpiderState state,
    const ScoreState score) noexcept
{
    const auto spider = consume_side_spider_contact(state);
    return {
        .spider = spider,
        .score_award = apply_score_award(score, spider.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
