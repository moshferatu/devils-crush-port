// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_scheduler.hpp"

namespace devils_crush::native_runtime {

TowerRuleScheduler::TowerRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool TowerRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response || contact.target.actor_slot != kTowerSlot
        || contact_queued_ || !tower_.collision_ready) {
        return false;
    }
    contact_queued_ = true;
    return true;
}

std::optional<TowerRulesUpdate> TowerRuleScheduler::advance_actor(
    const std::uint8_t slot,
    const bool center_insect_source_already_set) noexcept
{
    if (slot != kTowerSlot || !contact_queued_) {
        return std::nullopt;
    }

    contact_queued_ = false;
    const auto update = resolve_tower_contact(
        tower_, center_insect_source_already_set, score_);
    tower_ = update.tower.state;
    score_ = update.score_award.state;
    return update;
}

std::optional<TowerPresentationStep>
TowerRuleScheduler::advance_presentation(const std::uint8_t slot) noexcept
{
    if (slot != kTowerSlot || tower_.presentation_updates_remaining == 0U) {
        return std::nullopt;
    }
    const auto update = advance_tower_presentation(tower_);
    tower_ = update.state;
    return update;
}

std::optional<TowerBonusStageSelectionStep>
TowerRuleScheduler::advance_bonus_stage_selection(
    const std::uint8_t slot,
    const bool negative_collision_latched) noexcept
{
    if (slot != kTowerSlot || (!tower_.bonus_stage_selection_pending
        && !tower_.bonus_stage_transition_requested)) {
        return std::nullopt;
    }

    const auto update = consume_tower_bonus_stage_selection(
        tower_, negative_collision_latched);
    tower_ = update.state;
    return update;
}

void TowerRuleScheduler::seed(const TowerState state) noexcept
{
    tower_ = state;
    contact_queued_ = false;
}

const TowerState& TowerRuleScheduler::tower() const noexcept
{
    return tower_;
}

} // namespace devils_crush::native_runtime
