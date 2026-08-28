// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bumper_capture.hpp"

#include <algorithm>

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kBumperType = 0x82U;
constexpr std::uint16_t kSteadyBumperHandler = 0x78C7U;

} // namespace

std::optional<BumperCapture> capture_bumper_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto expected_side = bumper_side_for_slot(slot);
    const auto actor = state.actor(slot);
    const auto bumper = state.bumper(slot);
    if (!expected_side || !actor || !bumper || actor->activity_flags != kBumperType
        || actor->handler_address != kSteadyBumperHandler
        || bumper->side != *expected_side || bumper->hit_phase > 8U) {
        return std::nullopt;
    }

    const auto expected_primary = std::min<std::uint8_t>(bumper->hit_phase, 6U);
    const auto expected_secondary = bumper->hit_phase > 6U
        ? static_cast<std::uint8_t>(bumper->hit_phase - 6U)
        : 0U;
    if (bumper->primary_appearance != expected_primary
        || bumper->secondary_damage != expected_secondary) {
        return std::nullopt;
    }

    return BumperCapture{
        .actor = *actor,
        .rule_state = {
            .side = *expected_side,
            .hit_phase = bumper->hit_phase,
            .primary_appearance = bumper->primary_appearance,
            .secondary_damage = bumper->secondary_damage,
            .active = true,
        },
    };
}

} // namespace devils_crush::native_runtime
