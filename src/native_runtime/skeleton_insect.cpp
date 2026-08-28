// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/skeleton_insect.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] bool source_is_set(
    const SkeletonInsectKind kind,
    const TableRuleLatches& latches) noexcept
{
    switch (kind) {
    case SkeletonInsectKind::left:
        return latches.left_front_spider_safety;
    case SkeletonInsectKind::center:
        return latches.tower_completed;
    case SkeletonInsectKind::right:
        return latches.right_front_spider_safety;
    }
    return false;
}

[[nodiscard]] std::uint16_t score_for(const SkeletonInsectKind kind) noexcept
{
    return kind == SkeletonInsectKind::center ? 100U : 40U;
}

[[nodiscard]] std::uint8_t startup_rounds_for(
    const SkeletonInsectKind kind) noexcept
{
    switch (kind) {
    case SkeletonInsectKind::left:
        return 3U;
    case SkeletonInsectKind::center:
        return 16U;
    case SkeletonInsectKind::right:
        return 2U;
    }
    return 0U;
}

} // namespace

SkeletonInsectTriggerStep consume_skeleton_insect_source(
    SkeletonInsectState state,
    const TableRuleLatches& latches) noexcept
{
    const auto previous = state;
    if (state.lifecycle == SkeletonInsectLifecycle::transition_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SkeletonInsectTriggerDisposition::transition_pending,
        };
    }
    if (state.lifecycle == SkeletonInsectLifecycle::post_trigger) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SkeletonInsectTriggerDisposition::post_trigger,
        };
    }
    if (!source_is_set(state.kind, latches)) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SkeletonInsectTriggerDisposition::waiting_for_source,
        };
    }

    state.lifecycle = SkeletonInsectLifecycle::transition_pending;
    return {
        .previous = previous,
        .state = state,
        .disposition = SkeletonInsectTriggerDisposition::triggered,
        .score_award_units = score_for(state.kind),
        .requested_startup_rounds = startup_rounds_for(state.kind),
    };
}

SkeletonInsectState complete_skeleton_insect_transition(
    SkeletonInsectState state) noexcept
{
    if (state.lifecycle == SkeletonInsectLifecycle::transition_pending) {
        state.lifecycle = SkeletonInsectLifecycle::post_trigger;
    }
    return state;
}

SkeletonInsectRulesUpdate resolve_skeleton_insect_source(
    const SkeletonInsectState state,
    const TableRuleLatches& latches,
    const ScoreState score) noexcept
{
    const auto insect = consume_skeleton_insect_source(state, latches);
    return {
        .insect = insect,
        .score_award = apply_score_award(score, insect.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
