// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_head_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kDragonHeadType = 0x82U;
constexpr std::uint16_t kSteadyDragonHeadHandler = 0x5A4BU;
constexpr std::uint8_t kMaximumDragonHeadHits = 8U;

} // namespace

std::optional<DragonHeadCapture> capture_dragon_head_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    if (slot != kDragonHeadSlot || !actor
        || actor->activity_flags != kDragonHeadType
        || actor->handler_address != kSteadyDragonHeadHandler
        || (actor->collision_state & 0x80U) != 0U
        || actor->behavior_counter == 0U
        || actor->behavior_counter > kMaximumDragonHeadHits) {
        return std::nullopt;
    }

    return DragonHeadCapture{
        .actor = *actor,
        .rule_state = {
            .hits_remaining = actor->behavior_counter,
            .collision_ready = true,
        },
    };
}

} // namespace devils_crush::native_runtime
