// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_skeleton_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kGiantSkeletonType = 0x83U;
constexpr std::uint16_t kSteadyGiantSkeletonHandler = 0x5EECU;
constexpr std::uint8_t kMinimumLiveAppearance = 2U;
constexpr std::uint8_t kMaximumLiveAppearance = 6U;

} // namespace

std::optional<GiantSkeletonCapture> capture_giant_skeleton_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    if (slot != kGiantSkeletonSlot || !actor
        || actor->activity_flags != kGiantSkeletonType
        || actor->handler_address != kSteadyGiantSkeletonHandler
        || (actor->collision_state & 0x80U) != 0U
        || actor->collision_shape_phase < kMinimumLiveAppearance
        || actor->collision_shape_phase > kMaximumLiveAppearance) {
        return std::nullopt;
    }

    return GiantSkeletonCapture{
        .actor = *actor,
        .rule_state = {.appearance = actor->collision_shape_phase},
    };
}

} // namespace devils_crush::native_runtime
