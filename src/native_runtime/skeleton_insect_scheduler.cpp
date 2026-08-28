// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/skeleton_insect_scheduler.hpp"

namespace devils_crush::native_runtime {

SkeletonInsectRuleScheduler::SkeletonInsectRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

std::optional<SkeletonInsectRulesUpdate>
SkeletonInsectRuleScheduler::advance_actor(
    const std::uint8_t slot,
    const TableRuleLatches& latches) noexcept
{
    const auto kind = skeleton_insect_kind_for_slot(slot);
    if (!kind) {
        return std::nullopt;
    }
    const auto index = index_for(*kind);
    if (!index) {
        return std::nullopt;
    }
    auto& insect = insects_[*index];
    const auto update = resolve_skeleton_insect_source(insect, latches, score_);
    if (update.insect.disposition != SkeletonInsectTriggerDisposition::triggered) {
        return std::nullopt;
    }
    insect = update.insect.state;
    score_ = update.score_award.state;
    return update;
}

bool SkeletonInsectRuleScheduler::complete_transition(
    const std::uint8_t slot) noexcept
{
    const auto kind = skeleton_insect_kind_for_slot(slot);
    if (!kind) {
        return false;
    }
    const auto index = index_for(*kind);
    if (!index) {
        return false;
    }
    auto& insect = insects_[*index];
    if (insect.lifecycle != SkeletonInsectLifecycle::transition_pending) {
        return false;
    }
    insect = complete_skeleton_insect_transition(insect);
    return true;
}

bool SkeletonInsectRuleScheduler::seed(const SkeletonInsectState state) noexcept
{
    const auto index = index_for(state.kind);
    if (!index) {
        return false;
    }
    insects_[*index] = state;
    return true;
}

const SkeletonInsectState* SkeletonInsectRuleScheduler::insect(
    const SkeletonInsectKind kind) const noexcept
{
    const auto index = index_for(kind);
    return index ? &insects_[*index] : nullptr;
}

std::optional<std::size_t> SkeletonInsectRuleScheduler::index_for(
    const SkeletonInsectKind kind) noexcept
{
    switch (kind) {
    case SkeletonInsectKind::left:
        return 0U;
    case SkeletonInsectKind::center:
        return 1U;
    case SkeletonInsectKind::right:
        return 2U;
    }
    return std::nullopt;
}

} // namespace devils_crush::native_runtime
