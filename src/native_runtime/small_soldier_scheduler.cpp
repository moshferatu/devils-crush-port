// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/small_soldier_scheduler.hpp"

namespace devils_crush::native_runtime {

SmallSoldierRuleScheduler::SmallSoldierRuleScheduler(
    ActorState& actors,
    ScoreState& score) noexcept
    : actors_(actors)
    , score_(score)
{
}

bool SmallSoldierRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }

    const auto slot = contact.target.actor_slot;
    const auto* actor = actors_.actor(slot);
    if (actor == nullptr || actor->behavior != ActorBehavior::small_soldier
        || !actor->small_soldier || queued_contacts_[slot]) {
        return false;
    }

    queued_contacts_[slot] = true;
    return true;
}

std::optional<SmallSoldierRulesUpdate>
SmallSoldierRuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    if (slot >= ActorState::kSlotCount || !queued_contacts_[slot]) {
        return std::nullopt;
    }

    // A dynamic slot can be released and reused between phases. Clearing
    // before resolving guarantees that a stale mark cannot damage its new
    // occupant when the rule state no longer matches.
    queued_contacts_[slot] = false;
    return resolve_small_soldier_contact(actors_, score_, slot);
}

std::optional<SmallSoldierRulesUpdate>
SmallSoldierRuleScheduler::complete_animation(const std::uint8_t slot) noexcept
{
    if (slot >= ActorState::kSlotCount) {
        return std::nullopt;
    }
    return resolve_small_soldier_completion_animation(actors_, score_, slot);
}

bool SmallSoldierRuleScheduler::contact_queued(
    const std::uint8_t slot) const noexcept
{
    return slot < ActorState::kSlotCount && queued_contacts_[slot];
}

} // namespace devils_crush::native_runtime
