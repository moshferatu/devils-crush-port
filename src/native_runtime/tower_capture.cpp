// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kTowerType = 0x83U;
constexpr std::uint16_t kSteadyTowerHandler = 0x60C3U;

} // namespace

std::optional<TowerCapture> capture_tower_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    const auto tower = state.tower(slot);
    if (!actor || !tower || actor->activity_flags != kTowerType
        || actor->handler_address != kSteadyTowerHandler
        || actor->collision_state != 0U) {
        return std::nullopt;
    }

    return TowerCapture{
        .actor = *actor,
        .rule_state = {
            .hit_progress = tower->hit_progress,
            .remaining_armed_passes = tower->remaining_armed_passes,
        },
    };
}

} // namespace devils_crush::native_runtime
