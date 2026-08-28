// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_skeleton_eyeball_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kGiantSkeletonEyeballType = 0x82U;
constexpr std::uint16_t kOpeningWaitHandler = 0x4DF7U;
constexpr std::uint8_t kMinimumOpeningTicks = 2U;
constexpr std::uint8_t kMaximumOpeningTicks = 8U;
constexpr std::uint8_t kOpeningState = 2U;
constexpr std::uint16_t kFinalAnimationWaitHandler = 0x4F3DU;
constexpr std::uint8_t kMinimumFinalAppearanceTicks = 2U;
constexpr std::uint8_t kMaximumFinalAppearanceTicks = 8U;

} // namespace

std::optional<GiantSkeletonEyeballCapture>
capture_giant_skeleton_eyeball_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    if (slot != kGiantSkeletonEyeballSlot || !actor
        || actor->activity_flags != kGiantSkeletonEyeballType
        || actor->handler_address != kOpeningWaitHandler
        || actor->behavior_counter != kOpeningState
        || actor->collision_reaction.ticks_until_next_phase < kMinimumOpeningTicks
        || actor->collision_reaction.ticks_until_next_phase > kMaximumOpeningTicks) {
        return std::nullopt;
    }

    return GiantSkeletonEyeballCapture{
        .actor = *actor,
        .rule_state = {
            .opening_countdown_active = true,
            .opening_ticks_remaining =
                actor->collision_reaction.ticks_until_next_phase,
        },
    };
}

std::optional<GiantSkeletonEyeballCapture>
capture_giant_skeleton_eyeball_final_animation_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    const auto final_animation = state.giant_skeleton_eyeball_final_animation(slot);
    if (slot != kGiantSkeletonEyeballSlot || !actor || !final_animation
        || actor->activity_flags != kGiantSkeletonEyeballType
        || actor->handler_address != kFinalAnimationWaitHandler
        || final_animation->appearance_ticks_remaining
            < kMinimumFinalAppearanceTicks
        || final_animation->appearance_ticks_remaining
            > kMaximumFinalAppearanceTicks
        || final_animation->ticks_remaining == 0U
        || (final_animation->ticks_remaining & 0x00FFU) <= 1U
        || final_animation->appearance > 1U) {
        return std::nullopt;
    }

    return GiantSkeletonEyeballCapture{
        .actor = *actor,
        .rule_state = {
            .final_animation_active = true,
            .final_ticks_remaining = final_animation->ticks_remaining,
            .final_appearance_ticks_remaining =
                final_animation->appearance_ticks_remaining,
            .final_appearance_phase = final_animation->appearance,
            .final_appearance = final_animation->appearance,
        },
    };
}

} // namespace devils_crush::native_runtime
