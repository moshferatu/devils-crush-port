// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_guard_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kTowerGuardType = 0x82U;
constexpr std::uint16_t kSteadyTowerGuardHandler = 0x60A6U;

} // namespace

std::optional<TowerGuardCapture> capture_tower_guard_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    if (slot != kTowerGuardSlot) {
        return std::nullopt;
    }

    const auto actor = state.actor(slot);
    const auto tower = state.tower(kTowerSlot);
    if (!actor || !tower || actor->activity_flags != kTowerGuardType
        || actor->handler_address != kSteadyTowerGuardHandler) {
        return std::nullopt;
    }

    return TowerGuardCapture{
        .actor = *actor,
        .selector = select_tower_guard_selector({
            .remaining_armed_passes = tower->remaining_armed_passes,
        }),
    };
}

} // namespace devils_crush::native_runtime
