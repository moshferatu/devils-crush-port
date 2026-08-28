// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/side_spider_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kSideSpiderType = 0x83U;
constexpr std::uint16_t kSteadySideSpiderHandler = 0x651FU;

} // namespace

std::optional<SideSpiderCapture> capture_side_spider_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto expected_side = side_spider_side_for_slot(slot);
    const auto actor = state.actor(slot);
    if (!expected_side || !actor || actor->activity_flags != kSideSpiderType
        || actor->handler_address != kSteadySideSpiderHandler
        || actor->collision_reaction.phase != *expected_side) {
        return std::nullopt;
    }

    return SideSpiderCapture{
        .actor = *actor,
        .rule_state = {
            .side = *expected_side,
            .collision_ready = true,
        },
    };
}

} // namespace devils_crush::native_runtime
