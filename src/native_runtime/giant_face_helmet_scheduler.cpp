// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_face_helmet_scheduler.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] std::optional<std::size_t> channel_index(const std::uint8_t slot) noexcept
{
    if (slot == kGiantFaceLeftHelmetSlot) {
        return 0U;
    }
    if (slot == kGiantFaceRightHelmetSlot) {
        return 1U;
    }
    return std::nullopt;
}

} // namespace

GiantFaceHelmetRuleScheduler::GiantFaceHelmetRuleScheduler(ScoreState& score) noexcept
    : score_(score)
{
}

bool GiantFaceHelmetRuleScheduler::queue_confirmed_contact(
    const ContactDispatch& contact) noexcept
{
    const auto index = channel_index(contact.target.actor_slot);
    if (!helmet_ || !index || contact.disposition != ContactDisposition::dispatched
        || !contact.target.marked_for_response || contact_queued_[*index]
        || !helmet_->channels[*index].collision_ready) {
        return false;
    }
    contact_queued_[*index] = true;
    return true;
}

std::optional<GiantFaceHelmetRulesUpdate>
GiantFaceHelmetRuleScheduler::advance_actor(const std::uint8_t slot) noexcept
{
    const auto index = channel_index(slot);
    if (!helmet_ || !index) {
        return std::nullopt;
    }
    const auto update = resolve_giant_face_helmet_channel(
        *helmet_, slot, contact_queued_[*index], score_);
    contact_queued_[*index] = false;
    if (update.helmet.disposition == GiantFaceHelmetDisposition::invalid_state) {
        return std::nullopt;
    }
    helmet_ = update.helmet.state;
    score_ = update.score_award.state;
    return update;
}

std::optional<GiantFaceHelmetCompletionStep>
GiantFaceHelmetRuleScheduler::complete_activation_presentation(
    const std::uint8_t slot) noexcept
{
    if (!helmet_) {
        return std::nullopt;
    }
    const auto step = complete_giant_face_helmet_activation_presentation(*helmet_, slot);
    if (!step.completed) {
        return std::nullopt;
    }
    helmet_ = step.state;
    return step;
}

bool GiantFaceHelmetRuleScheduler::complete_ball_return(const std::uint8_t slot) noexcept
{
    return helmet_ && complete_giant_face_helmet_ball_return(*helmet_, slot);
}

void GiantFaceHelmetRuleScheduler::seed(const GiantFaceHelmetState state) noexcept
{
    helmet_ = state;
    contact_queued_ = {};
}

const GiantFaceHelmetState* GiantFaceHelmetRuleScheduler::helmet() const noexcept
{
    return helmet_ ? &*helmet_ : nullptr;
}

} // namespace devils_crush::native_runtime
