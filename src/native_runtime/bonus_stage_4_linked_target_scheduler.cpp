// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4_linked_target_scheduler.hpp"

#include "devils_crush/native_runtime/actor_state.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] bool is_valid_source_identity(
    const BonusStage4LinkedTargetState& target) noexcept
{
    return target.objective_index < kBonusStage4SourceSlots.size()
        && kBonusStage4SourceSlots[target.objective_index] == target.source_slot;
}

[[nodiscard]] std::optional<std::size_t> child_index_for_slot(
    const std::array<std::optional<BonusStage4OrbitingChildState>, 4U>& children,
    const std::uint8_t slot) noexcept
{
    for (std::size_t index = 0U; index < children.size(); ++index) {
        if (children[index] && children[index]->dynamic_slot == slot) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> vacant_child_index(
    const std::array<std::optional<BonusStage4OrbitingChildState>, 4U>& children)
    noexcept
{
    for (std::size_t index = 0U; index < children.size(); ++index) {
        if (!children[index]) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool is_valid_dynamic_slot(const std::uint8_t slot) noexcept
{
    return slot >= ActorState::kFirstDynamicSlot
        && slot < ActorState::kPastLastDynamicSlot;
}

[[nodiscard]] bool is_valid_target(const BonusStage4LinkedTargetState& target) noexcept
{
    return is_valid_source_identity(target) && target.ticks_until_motion_refresh != 0U
        && is_valid_dynamic_slot(target.dynamic_slot)
        && target.orbiting_children_pending <= 4U
        && !(target.hit_presentation_pending
            && target.orbiting_children_pending != 0U);
}

} // namespace

BonusStage4LinkedTargetRuleScheduler::BonusStage4LinkedTargetRuleScheduler(
    ScoreState& score) noexcept
    : score_(score)
{
}

bool BonusStage4LinkedTargetRuleScheduler::seed(
    const BonusStage4LinkedTargetState target) noexcept
{
    return seed(target, {});
}

bool BonusStage4LinkedTargetRuleScheduler::seed(
    const BonusStage4LinkedTargetState target,
    const std::array<std::optional<BonusStage4OrbitingChildState>, 4U> children) noexcept
{
    if (!is_valid_target(target)) {
        return false;
    }
    bool has_child = false;
    for (std::size_t index = 0U; index < children.size(); ++index) {
        const auto& child = children[index];
        if (!child) {
            continue;
        }
        has_child = true;
        if (!is_valid_dynamic_slot(child->dynamic_slot)
            || child->dynamic_slot == target.dynamic_slot
            || child->target_slot != target.dynamic_slot || child->orbit_phase >= 32U
            || child->position != bonus_stage_4_orbit_position(
                target.position, child->orbit_phase)) {
            return false;
        }
        for (std::size_t other = 0U; other < index; ++other) {
            if (children[other] && children[other]->dynamic_slot == child->dynamic_slot) {
                return false;
            }
        }
    }
    if (has_child && (target.hit_presentation_pending
        || target.orbiting_children_pending != 0U)) {
        return false;
    }
    target_ = target;
    contact_queued_ = false;
    orbiting_children_ = children;
    orbiting_child_contacts_queued_.fill(false);
    return true;
}

const BonusStage4OrbitingChildState*
BonusStage4LinkedTargetRuleScheduler::orbiting_child(
    const std::uint8_t slot) const noexcept
{
    const auto index = child_index_for_slot(orbiting_children_, slot);
    return index ? &*orbiting_children_[*index] : nullptr;
}

const BonusStage4LinkedTargetState*
BonusStage4LinkedTargetRuleScheduler::target() const noexcept
{
    return target_ ? &*target_ : nullptr;
}

bool BonusStage4LinkedTargetRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (!target_ || contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }
    const auto slot = contact.target.actor_slot;
    if (slot == target_->dynamic_slot) {
        if (contact_queued_ || target_->hit_presentation_pending
            || target_->orbiting_children_pending != 0U) {
            return false;
        }
        contact_queued_ = true;
        return true;
    }
    const auto child_index = child_index_for_slot(orbiting_children_, slot);
    if (!child_index || orbiting_child_contacts_queued_[*child_index]) {
        return false;
    }
    orbiting_child_contacts_queued_[*child_index] = true;
    return true;
}

std::optional<BonusStage4LinkedTargetRulesUpdate>
BonusStage4LinkedTargetRuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    if (!target_ || !contact_queued_ || slot != target_->dynamic_slot) {
        return std::nullopt;
    }
    contact_queued_ = false;
    const auto update = resolve_bonus_stage_4_linked_target_contact(*target_, score_);
    if (update.target.disposition == BonusStage4LinkedTargetContactDisposition::ignored) {
        return std::nullopt;
    }
    target_ = update.target.state;
    score_ = update.score_award.state;
    return update;
}

std::optional<BonusStage4LinkedTargetMotionStep>
BonusStage4LinkedTargetRuleScheduler::advance_motion_actor(
    const std::uint8_t slot) noexcept
{
    if (!target_ || contact_queued_ || slot != target_->dynamic_slot
        || target_->hit_presentation_pending
        || target_->orbiting_children_pending != 0U) {
        return std::nullopt;
    }
    const auto step = advance_bonus_stage_4_linked_target_motion(*target_);
    target_ = step.state;
    return step;
}

std::optional<BonusStage4OrbitingChildFrameUpdate>
BonusStage4LinkedTargetRuleScheduler::advance_orbiting_child_actor(
    const std::uint8_t slot) noexcept
{
    if (!target_) {
        return std::nullopt;
    }
    const auto child_index = child_index_for_slot(orbiting_children_, slot);
    if (!child_index || !orbiting_child_contacts_queued_[*child_index]) {
        return std::nullopt;
    }
    orbiting_child_contacts_queued_[*child_index] = false;
    const auto update = advance_bonus_stage_4_orbiting_child_frame(
        *orbiting_children_[*child_index], target_->position, true, true, score_);
    score_ = update.score_award.state;
    orbiting_children_[*child_index].reset();
    return update;
}

std::optional<BonusStage4OrbitingChildFrameUpdate>
BonusStage4LinkedTargetRuleScheduler::advance_orbiting_child_orbit_actor(
    const std::uint8_t slot) noexcept
{
    if (!target_) {
        return std::nullopt;
    }
    const auto child_index = child_index_for_slot(orbiting_children_, slot);
    if (!child_index || orbiting_child_contacts_queued_[*child_index]) {
        return std::nullopt;
    }
    const auto update = advance_bonus_stage_4_orbiting_child_frame(
        *orbiting_children_[*child_index], target_->position, true, false, score_);
    orbiting_children_[*child_index] = update.child.state;
    return update;
}

std::optional<BonusStage4LinkedTargetHitPresentationStep>
BonusStage4LinkedTargetRuleScheduler::complete_hit_presentation() noexcept
{
    if (!target_) {
        return std::nullopt;
    }
    const auto step = complete_bonus_stage_4_linked_target_hit_presentation(*target_);
    if (step.disposition
        == BonusStage4LinkedTargetHitPresentationDisposition::no_pending_presentation) {
        return std::nullopt;
    }
    target_ = step.state;
    return step;
}

std::optional<BonusStage4OrbitingChildAllocationStep>
BonusStage4LinkedTargetRuleScheduler::allocate_orbiting_child(
    const std::uint8_t dynamic_slot) noexcept
{
    if (!target_) {
        return std::nullopt;
    }
    const auto step = allocate_bonus_stage_4_orbiting_child(*target_, dynamic_slot);
    if (step.disposition != BonusStage4OrbitingChildAllocationDisposition::allocated) {
        return std::nullopt;
    }
    const auto child_index = vacant_child_index(orbiting_children_);
    if (!child_index || child_index_for_slot(orbiting_children_, dynamic_slot)) {
        return std::nullopt;
    }
    target_ = step.target;
    orbiting_children_[*child_index] = *step.child;
    orbiting_child_contacts_queued_[*child_index] = false;
    return step;
}

} // namespace devils_crush::native_runtime
