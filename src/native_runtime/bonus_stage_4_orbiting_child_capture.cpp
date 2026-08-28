// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4_orbiting_child_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kOrbitingChildType = 0xC2U;
constexpr std::uint16_t kOrbitingChildHandler = 0x6D27U;
constexpr std::uint16_t kPostAllocationTargetHandler = 0x6CA0U;
constexpr std::uint8_t kOrbitPhaseCount = 0x20U;

} // namespace

std::optional<BonusStage4OrbitingChildCapture>
capture_bonus_stage_4_orbiting_child(const GameStateView& state) noexcept
{
    std::optional<BonusStage4OrbitingChildCapture> capture;
    for (std::uint8_t slot = 0U; slot < GameStateView::kActorSlotCount; ++slot) {
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kOrbitingChildType
            || actor->handler_address != kOrbitingChildHandler
            || (actor->collision_state & 0x80U) != 0U) {
            continue;
        }

        const auto target_slot = actor->collision_table.record_cursor;
        const auto orbit_phase = actor->collision_table.collision_entry_cursor;
        const auto target = state.actor(target_slot);
        if (orbit_phase >= kOrbitPhaseCount || !target
            || target->activity_flags != kOrbitingChildType
            || target->handler_address != kPostAllocationTargetHandler) {
            return std::nullopt;
        }

        const BonusStage4LinkedTargetPosition target_position{
            .horizontal_pixels = target->collision_table.position_x,
            .vertical_pixels = target->collision_table.position_y,
        };
        const auto expected_position = bonus_stage_4_orbit_position(
            target_position, orbit_phase);
        if (actor->collision_table.position_x != expected_position.horizontal_pixels
            || actor->collision_table.position_y != expected_position.vertical_pixels) {
            return std::nullopt;
        }
        if (capture) {
            return std::nullopt;
        }
        capture = BonusStage4OrbitingChildCapture{
            .actor = *actor,
            .state = {
                .target_slot = target_slot,
                .dynamic_slot = slot,
                .orbit_phase = orbit_phase,
                .position = expected_position,
            },
        };
    }
    return capture;
}

} // namespace devils_crush::native_runtime
