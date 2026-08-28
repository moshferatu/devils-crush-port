// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/front_spider_scheduler.hpp"

namespace devils_crush::native_runtime {

FrontSpiderRuleScheduler::FrontSpiderRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool FrontSpiderRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }
    const auto side = front_spider_side_for_slot(contact.target.actor_slot);
    if (!side || queued_contacts_[*side] || !spiders_[*side].collision_ready) {
        return false;
    }
    queued_contacts_[*side] = true;
    return true;
}

std::optional<FrontSpiderRulesUpdate>
FrontSpiderRuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    const auto side = front_spider_side_for_slot(slot);
    if (!side || !queued_contacts_[*side]) {
        return std::nullopt;
    }

    queued_contacts_[*side] = false;
    const auto update = resolve_front_spider_contact(spiders_[*side], score_);
    spiders_[*side] = update.spider.state;
    score_ = update.score_award.state;
    return update;
}

bool FrontSpiderRuleScheduler::complete_transition(
    const std::uint8_t slot) noexcept
{
    const auto side = front_spider_side_for_slot(slot);
    if (!side || spiders_[*side].collision_ready) {
        return false;
    }
    spiders_[*side] = complete_front_spider_transition(spiders_[*side]);
    return true;
}

bool FrontSpiderRuleScheduler::seed(const FrontSpiderState state) noexcept
{
    if (state.side >= spiders_.size()) {
        return false;
    }
    spiders_[state.side] = state;
    queued_contacts_[state.side] = false;
    return true;
}

const FrontSpiderState* FrontSpiderRuleScheduler::spider(
    const std::uint8_t side) const noexcept
{
    return side < spiders_.size() ? &spiders_[side] : nullptr;
}

bool FrontSpiderRuleScheduler::contact_queued(
    const std::uint8_t slot) const noexcept
{
    const auto side = front_spider_side_for_slot(slot);
    return side && queued_contacts_[*side];
}

} // namespace devils_crush::native_runtime
