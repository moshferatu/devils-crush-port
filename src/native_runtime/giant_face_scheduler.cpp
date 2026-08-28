// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_face_scheduler.hpp"

namespace devils_crush::native_runtime {

GiantFaceRuleScheduler::GiantFaceRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool GiantFaceRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    if (contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response) {
        return false;
    }
    if (contact.target.actor_slot == kGiantFaceCrownSlot
        && !crown_contact_queued_ && giant_face_.crown_collision_ready) {
        crown_contact_queued_ = true;
        return true;
    }
    if (contact.target.actor_slot == kGiantFaceMouthSlot && !mouth_contact_queued_
        && giant_face_.mouth_collision_ready && !giant_face_.bonus_stage_4_requested) {
        mouth_contact_queued_ = true;
        return true;
    }
    return false;
}

std::optional<GiantFaceRulesUpdate> GiantFaceRuleScheduler::advance_actor(
    const std::uint8_t slot) noexcept
{
    GiantFaceRulesUpdate update{};
    if (slot == kGiantFaceCrownSlot && crown_contact_queued_) {
        crown_contact_queued_ = false;
        update = resolve_giant_face_crown_contact(giant_face_, score_);
    } else if (slot == kGiantFaceMouthSlot && mouth_contact_queued_) {
        mouth_contact_queued_ = false;
        update = resolve_giant_face_mouth_contact(giant_face_, score_);
    } else {
        return std::nullopt;
    }
    if (update.giant_face.disposition == GiantFaceContactDisposition::invalid_state) {
        return std::nullopt;
    }
    giant_face_ = update.giant_face.state;
    score_ = update.score_award.state;
    return update;
}

std::optional<GiantFaceControllerStep>
GiantFaceRuleScheduler::advance_controller_actor(const std::uint8_t slot) noexcept
{
    if (slot != kGiantFaceControllerSlot) {
        return std::nullopt;
    }
    const auto step = advance_giant_face_controller(controller_, giant_face_);
    if (step.disposition == GiantFaceControllerDisposition::dormant) {
        return std::nullopt;
    }
    controller_ = step.state;
    giant_face_ = step.face_state;
    if (step.starts_lower_form_animation) {
        static_cast<void>(begin_giant_face_animator(
            animator_, GiantFaceAnimatorMode::lower_forms));
    }
    if (step.starts_upper_form_animation) {
        static_cast<void>(begin_giant_face_animator(
            animator_, GiantFaceAnimatorMode::upper_forms));
    }
    if (step.stops_form_animation) {
        animator_ = {};
    }
    return step;
}

std::optional<GiantFaceAnimatorStep> GiantFaceRuleScheduler::advance_animator_actor(
    const std::uint8_t slot) noexcept
{
    if (slot != kGiantFaceAnimatorSlot) {
        return std::nullopt;
    }
    const auto step = advance_giant_face_animator(animator_);
    if (step.disposition == GiantFaceAnimatorDisposition::idle) {
        return std::nullopt;
    }
    animator_ = step.state;
    return step;
}

bool GiantFaceRuleScheduler::complete_impact_presentation(
    const std::uint8_t slot) noexcept
{
    return complete_giant_face_impact_presentation(giant_face_, slot);
}

bool GiantFaceRuleScheduler::complete_mouth_opening() noexcept
{
    return complete_giant_face_mouth_opening(giant_face_);
}

bool GiantFaceRuleScheduler::request_wake() noexcept
{
    return request_giant_face_wake(giant_face_);
}

bool GiantFaceRuleScheduler::request_transformation() noexcept
{
    return request_giant_face_transformation(giant_face_);
}

bool GiantFaceRuleScheduler::begin_animator(const GiantFaceAnimatorMode mode) noexcept
{
    return begin_giant_face_animator(animator_, mode);
}

void GiantFaceRuleScheduler::seed(const GiantFaceState state) noexcept
{
    seed(state, {}, {});
}

void GiantFaceRuleScheduler::seed(
    const GiantFaceState state,
    const GiantFaceControllerState controller,
    const GiantFaceAnimatorState animator) noexcept
{
    giant_face_ = state;
    controller_ = controller;
    animator_ = animator;
    crown_contact_queued_ = false;
    mouth_contact_queued_ = false;
}

const GiantFaceState& GiantFaceRuleScheduler::giant_face() const noexcept
{
    return giant_face_;
}

const GiantFaceControllerState& GiantFaceRuleScheduler::controller() const noexcept
{
    return controller_;
}

const GiantFaceAnimatorState& GiantFaceRuleScheduler::animator() const noexcept
{
    return animator_;
}

} // namespace devils_crush::native_runtime
