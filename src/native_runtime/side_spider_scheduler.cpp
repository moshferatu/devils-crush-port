// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/side_spider_scheduler.hpp"

namespace devils_crush::native_runtime {

SideSpiderRuleScheduler::SideSpiderRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool SideSpiderRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }
    const auto side = side_spider_side_for_slot(contact.target.actor_slot);
    if (!side || queued_contacts_[*side] || !spiders_[*side].collision_ready) {
        return false;
    }
    queued_contacts_[*side] = true;
    return true;
}

std::optional<SideSpiderRulesUpdate>
SideSpiderRuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    const auto side = side_spider_side_for_slot(slot);
    if (!side || !queued_contacts_[*side]) {
        return std::nullopt;
    }

    queued_contacts_[*side] = false;
    const auto update = resolve_side_spider_contact(spiders_[*side], score_);
    spiders_[*side] = update.spider.state;
    score_ = update.score_award.state;
    return update;
}

bool SideSpiderRuleScheduler::complete_transition(
    const std::uint8_t slot) noexcept
{
    const auto side = side_spider_side_for_slot(slot);
    if (!side || spiders_[*side].collision_ready) {
        return false;
    }
    spiders_[*side] = complete_side_spider_transition(spiders_[*side]);
    return true;
}

bool SideSpiderRuleScheduler::seed(const SideSpiderState state) noexcept
{
    if (state.side >= spiders_.size()) {
        return false;
    }
    spiders_[state.side] = state;
    queued_contacts_[state.side] = false;
    return true;
}

const SideSpiderState* SideSpiderRuleScheduler::spider(
    const std::uint8_t side) const noexcept
{
    return side < spiders_.size() ? &spiders_[side] : nullptr;
}

} // namespace devils_crush::native_runtime
