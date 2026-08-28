// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_egg_scheduler.hpp"

#include <algorithm>

namespace devils_crush::native_runtime {

DragonEggRuleScheduler::DragonEggRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool DragonEggRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    const auto index = dragon_egg_index_for_slot(contact.target.actor_slot);
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response || !cluster_) {
        return false;
    }
    if (index && !contact_slot_ && !cluster_->reset_pending
        && cluster_->eggs[*index].active) {
        contact_slot_ = contact.target.actor_slot;
        return true;
    }
    const auto* existing_hatchling = this->hatchling(contact.target.actor_slot);
    if (existing_hatchling == nullptr || hatchling_contact_slot_) {
        return false;
    }
    hatchling_contact_slot_ = existing_hatchling->dynamic_slot;
    return true;
}

std::optional<DragonEggRulesUpdate>
DragonEggRuleScheduler::advance_egg_actor(const std::uint8_t slot) noexcept
{
    if (!cluster_ || contact_slot_ != slot) {
        return std::nullopt;
    }
    contact_slot_.reset();
    const auto update = resolve_dragon_egg_contact(*cluster_, slot, score_);
    if (update.egg.disposition != DragonEggHitDisposition::destroyed) {
        return std::nullopt;
    }
    *cluster_ = update.egg.state;
    score_ = update.score_award.state;
    return update;
}

std::optional<DragonEggCompletionRulesUpdate>
DragonEggRuleScheduler::advance_controller_actor(const std::uint8_t slot) noexcept
{
    if (slot != kDragonEggControllerSlot || !cluster_) {
        return std::nullopt;
    }
    const auto update = resolve_dragon_egg_controller(*cluster_, score_);
    *cluster_ = update.controller.state;
    score_ = update.score_award.state;
    return update;
}

std::optional<DragonEggGroupResetStep>
DragonEggRuleScheduler::begin_group_reset() noexcept
{
    if (!cluster_) {
        return std::nullopt;
    }
    const auto step = begin_dragon_egg_group_reset(*cluster_);
    if (step.disposition != DragonEggGroupResetDisposition::reset_armed) {
        return std::nullopt;
    }
    *cluster_ = step.state;
    contact_slot_.reset();
    return step;
}

std::optional<DragonEggFixedResetStep>
DragonEggRuleScheduler::advance_fixed_reset_actor(const std::uint8_t slot) noexcept
{
    if (!cluster_) {
        return std::nullopt;
    }
    const auto step = advance_dragon_egg_fixed_reset(*cluster_, slot);
    if (step.disposition == DragonEggFixedResetDisposition::ignored) {
        return std::nullopt;
    }
    *cluster_ = step.state;
    return step;
}

std::optional<DragonEggHatchlingAllocationStep>
DragonEggRuleScheduler::allocate_hatchling(
    const std::uint8_t source_egg_slot,
    const std::uint8_t dynamic_slot) noexcept
{
    if (!cluster_ || cluster_->reset_pending) {
        return std::nullopt;
    }
    const auto step = allocate_dragon_egg_hatchling(
        *cluster_, source_egg_slot, dynamic_slot, hatchlings_);
    if (step.disposition != DragonEggHatchlingAllocationDisposition::allocated
        || !step.hatchling) {
        return std::nullopt;
    }
    const auto index = dragon_egg_index_for_slot(source_egg_slot);
    *cluster_ = step.cluster;
    hatchlings_[*index] = *step.hatchling;
    return step;
}

std::optional<DragonEggHatchlingRulesUpdate>
DragonEggRuleScheduler::advance_hatchling_actor(
    const std::uint8_t dynamic_slot) noexcept
{
    const auto iterator = std::find_if(
        hatchlings_.begin(), hatchlings_.end(), [dynamic_slot](const auto& hatchling) {
            return hatchling && hatchling->dynamic_slot == dynamic_slot;
        });
    if (iterator == hatchlings_.end() || !*iterator) {
        return std::nullopt;
    }

    const bool contact_confirmed = hatchling_contact_slot_ == dynamic_slot;
    if (contact_confirmed) {
        hatchling_contact_slot_.reset();
    }
    const auto update = resolve_dragon_egg_hatchling(
        **iterator, contact_confirmed, score_);
    score_ = update.score_award.state;
    if (update.hatchling.releases_child) {
        iterator->reset();
    } else {
        *iterator = update.hatchling.state;
    }
    return update;
}

bool DragonEggRuleScheduler::seed_hatchling(
    const DragonEggHatchlingState hatchling) noexcept
{
    const auto index = dragon_egg_index_for_slot(hatchling.source_egg_slot);
    if (!cluster_ || !index || cluster_->eggs[*index].active
        || cluster_->eggs[*index].hatchling_spawn_pending
        || this->hatchling(hatchling.dynamic_slot) != nullptr
        || hatchlings_[*index]) {
        return false;
    }
    hatchlings_[*index] = hatchling;
    return true;
}

void DragonEggRuleScheduler::seed(const DragonEggClusterState state) noexcept
{
    cluster_ = state;
    contact_slot_.reset();
    hatchling_contact_slot_.reset();
    hatchlings_.fill(std::nullopt);
}

const DragonEggClusterState* DragonEggRuleScheduler::cluster() const noexcept
{
    return cluster_ ? &*cluster_ : nullptr;
}

const DragonEggHatchlingState* DragonEggRuleScheduler::hatchling(
    const std::uint8_t dynamic_slot) const noexcept
{
    const auto iterator = std::find_if(
        hatchlings_.begin(), hatchlings_.end(), [dynamic_slot](const auto& hatchling) {
            return hatchling && hatchling->dynamic_slot == dynamic_slot;
        });
    return iterator == hatchlings_.end() || !*iterator ? nullptr : &**iterator;
}

} // namespace devils_crush::native_runtime
