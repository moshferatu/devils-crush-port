// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4_scheduler.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] std::optional<std::uint8_t> objective_index_for_slot(
    const std::uint8_t slot) noexcept
{
    for (std::uint8_t index = 0U; index < kBonusStage4ObjectiveSlots.size(); ++index) {
        if (kBonusStage4ObjectiveSlots[index] == slot) {
            return index;
        }
    }
    return std::nullopt;
}

} // namespace

BonusStage4RuleScheduler::BonusStage4RuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

void BonusStage4RuleScheduler::seed(const BonusStage4State state) noexcept
{
    stage_ = state;
    pending_objective_index_.reset();
}

const BonusStage4State* BonusStage4RuleScheduler::stage() const noexcept
{
    return stage_ ? &*stage_ : nullptr;
}

bool BonusStage4RuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (!stage_ || pending_objective_index_
        || contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }
    const auto objective_index = objective_index_for_slot(contact.target.actor_slot);
    if (!objective_index || !stage_->objectives[*objective_index].active) {
        return false;
    }
    pending_objective_index_ = *objective_index;
    return true;
}

std::optional<BonusStage4ObjectiveRulesUpdate>
BonusStage4RuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    if (!stage_ || !pending_objective_index_
        || kBonusStage4ObjectiveSlots[*pending_objective_index_] != slot) {
        return std::nullopt;
    }
    const auto objective_index = *pending_objective_index_;
    pending_objective_index_.reset();
    const auto update = resolve_bonus_stage_4_objective_contact(
        *stage_, score_, objective_index);
    if (update.objective.disposition == BonusStage4ObjectiveDisposition::ignored) {
        return std::nullopt;
    }
    *stage_ = update.objective.state;
    score_ = update.destruction_score_award.state;
    return update;
}

} // namespace devils_crush::native_runtime
