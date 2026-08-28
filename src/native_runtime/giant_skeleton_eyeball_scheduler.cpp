// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_skeleton_eyeball_scheduler.hpp"

namespace devils_crush::native_runtime {

std::optional<GiantSkeletonEyeballStep>
GiantSkeletonEyeballScheduler::advance_actor(
    const std::uint8_t slot,
    const bool dragon_egg_completion_signaled) noexcept
{
    if (slot != kGiantSkeletonEyeballSlot || !eyeball_) {
        return std::nullopt;
    }
    const auto step = advance_giant_skeleton_eyeball(
        *eyeball_, dragon_egg_completion_signaled);
    *eyeball_ = step.state;
    return step;
}

std::optional<GiantSkeletonEyeballOpeningStartStep>
GiantSkeletonEyeballScheduler::begin_opening_countdown() noexcept
{
    if (!eyeball_) {
        return std::nullopt;
    }
    const auto step = begin_giant_skeleton_eyeball_opening_countdown(*eyeball_);
    if (step.disposition
        != GiantSkeletonEyeballOpeningStartDisposition::countdown_started) {
        return std::nullopt;
    }
    *eyeball_ = step.state;
    return step;
}

std::optional<GiantSkeletonEyeballFinalStartStep>
GiantSkeletonEyeballScheduler::begin_final_animation(
    const bool extended_wait) noexcept
{
    if (!eyeball_) {
        return std::nullopt;
    }
    const auto step = begin_giant_skeleton_eyeball_final_animation(
        *eyeball_, extended_wait);
    if (step.disposition
        != GiantSkeletonEyeballFinalStartDisposition::final_animation_started) {
        return std::nullopt;
    }
    *eyeball_ = step.state;
    return step;
}

std::optional<GiantSkeletonEyeballFinalResetStep>
GiantSkeletonEyeballScheduler::complete_final_reset() noexcept
{
    if (!eyeball_) {
        return std::nullopt;
    }
    const auto step = complete_giant_skeleton_eyeball_final_reset(*eyeball_);
    if (step.disposition
        != GiantSkeletonEyeballFinalResetDisposition::dormant_reinitialized) {
        return std::nullopt;
    }
    *eyeball_ = step.state;
    return step;
}

void GiantSkeletonEyeballScheduler::seed(
    const GiantSkeletonEyeballState state) noexcept
{
    eyeball_ = state;
}

const GiantSkeletonEyeballState*
GiantSkeletonEyeballScheduler::eyeball() const noexcept
{
    return eyeball_ ? &*eyeball_ : nullptr;
}

} // namespace devils_crush::native_runtime
