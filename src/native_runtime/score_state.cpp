// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/score_state.hpp"

namespace devils_crush::native_runtime {

ScoreAwardStep apply_score_award(
    ScoreState state,
    const std::uint16_t awarded_units) noexcept
{
    const auto previous = state;
    if (state.stored_units >= ScoreState::kExclusiveLimit) {
        return {
            .previous = previous,
            .state = state,
            .disposition = ScoreAwardDisposition::reaches_limit,
            .awarded_units = awarded_units,
        };
    }
    const auto next = state.stored_units + awarded_units;
    if (next >= ScoreState::kExclusiveLimit) {
        return {
            .previous = previous,
            .state = state,
            .disposition = ScoreAwardDisposition::reaches_limit,
            .awarded_units = awarded_units,
        };
    }

    state.stored_units = next;
    state.display_dirty = state.display_dirty || awarded_units != 0U;
    return {
        .previous = previous,
        .state = state,
        .awarded_units = awarded_units,
    };
}

SmallSoldierScoreEvents small_soldier_score_events(
    const SmallSoldierContactStep& child_step,
    const std::optional<SmallSoldierGroupStep>& group_step) noexcept
{
    SmallSoldierScoreEvents events;
    const auto append = [&events](const std::uint16_t award) {
        if (award != 0U) {
            events.awards[events.count++] = award;
        }
    };
    append(child_step.hit_score_units);
    append(child_step.completion_score_units);
    if (group_step
        && group_step->disposition
            == SmallSoldierGroupDisposition::source_completed) {
        append(group_step->source_completion_score_units);
    }
    return events;
}

} // namespace devils_crush::native_runtime
