// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bumper_scheduler.hpp"

namespace devils_crush::native_runtime {

BumperRuleScheduler::BumperRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool BumperRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }
    const auto side = bumper_side_for_slot(contact.target.actor_slot);
    if (!side || queued_contacts_[*side] || !bumpers_[*side].active) {
        return false;
    }
    queued_contacts_[*side] = true;
    return true;
}

std::optional<BumperRulesUpdate> BumperRuleScheduler::advance_actor(
    const std::uint8_t slot) noexcept
{
    const auto side = bumper_side_for_slot(slot);
    if (!side || !queued_contacts_[*side]) {
        return std::nullopt;
    }

    queued_contacts_[*side] = false;
    const auto update = resolve_bumper_contact(bumpers_[*side], score_);
    bumpers_[*side] = update.bumper.state;
    score_ = update.score_award.state;
    return update;
}

bool BumperRuleScheduler::seed(const BumperState state) noexcept
{
    if (state.side >= bumpers_.size()) {
        return false;
    }
    bumpers_[state.side] = state;
    queued_contacts_[state.side] = false;
    return true;
}

const BumperState* BumperRuleScheduler::bumper(
    const std::uint8_t side) const noexcept
{
    return side < bumpers_.size() ? &bumpers_[side] : nullptr;
}

bool BumperRuleScheduler::contact_queued(const std::uint8_t slot) const noexcept
{
    const auto side = bumper_side_for_slot(slot);
    return side && queued_contacts_[*side];
}

} // namespace devils_crush::native_runtime
