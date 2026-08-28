// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/front_spider_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kFrontSpiderType = 0x83U;
constexpr std::uint16_t kSteadyFrontSpiderHandler = 0x6490U;

} // namespace

std::optional<FrontSpiderCapture> capture_front_spider_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto expected_side = front_spider_side_for_slot(slot);
    const auto actor = state.actor(slot);
    if (!expected_side || !actor || actor->activity_flags != kFrontSpiderType
        || actor->handler_address != kSteadyFrontSpiderHandler
        || actor->collision_reaction.phase != *expected_side) {
        return std::nullopt;
    }

    return FrontSpiderCapture{
        .actor = *actor,
        .rule_state = {
            .side = *expected_side,
            .collision_ready = true,
        },
    };
}

} // namespace devils_crush::native_runtime
