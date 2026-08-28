// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_head_scheduler.hpp"

namespace devils_crush::native_runtime {

DragonHeadRuleScheduler::DragonHeadRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool DragonHeadRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response
        || contact.target.actor_slot != kDragonHeadSlot || contact_queued_
        || !dragon_.collision_ready) {
        return false;
    }
    contact_queued_ = true;
    return true;
}

std::optional<DragonHeadRulesUpdate>
DragonHeadRuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    if (slot != kDragonHeadSlot || !contact_queued_) {
        return std::nullopt;
    }
    contact_queued_ = false;
    const auto update = resolve_dragon_head_contact(dragon_, score_);
    if (update.dragon.disposition == DragonHeadContactDisposition::invalid_state) {
        return std::nullopt;
    }
    dragon_ = update.dragon.state;
    score_ = update.terminal_score_award.state;
    return update;
}

bool DragonHeadRuleScheduler::complete_hit_presentation(
    const std::uint8_t slot) noexcept
{
    return slot == kDragonHeadSlot
        && complete_dragon_head_hit_presentation(dragon_);
}

void DragonHeadRuleScheduler::seed(const DragonHeadState state) noexcept
{
    dragon_ = state;
    contact_queued_ = false;
}

const DragonHeadState& DragonHeadRuleScheduler::dragon() const noexcept
{
    return dragon_;
}

} // namespace devils_crush::native_runtime
