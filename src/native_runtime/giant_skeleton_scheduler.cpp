// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_skeleton_scheduler.hpp"

namespace devils_crush::native_runtime {

GiantSkeletonRuleScheduler::GiantSkeletonRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool GiantSkeletonRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response
        || contact.target.actor_slot != kGiantSkeletonSlot || contact_queued_
        || !giant_skeleton_) {
        return false;
    }
    contact_queued_ = true;
    return true;
}

std::optional<GiantSkeletonRulesUpdate>
GiantSkeletonRuleScheduler::advance_actor(
    const std::uint8_t slot,
    const GiantSkeletonTargetPosition target) noexcept
{
    if (slot != kGiantSkeletonSlot || !giant_skeleton_) {
        return std::nullopt;
    }
    const auto update = resolve_giant_skeleton_update(
        *giant_skeleton_, target, contact_queued_, score_);
    contact_queued_ = false;
    *giant_skeleton_ = update.giant_skeleton.state;
    score_ = update.hit_score_award.state;
    return update;
}

void GiantSkeletonRuleScheduler::seed(const GiantSkeletonState state) noexcept
{
    giant_skeleton_ = state;
    contact_queued_ = false;
}

const GiantSkeletonState*
GiantSkeletonRuleScheduler::giant_skeleton() const noexcept
{
    return giant_skeleton_ ? &*giant_skeleton_ : nullptr;
}

} // namespace devils_crush::native_runtime
