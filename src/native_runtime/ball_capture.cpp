// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/ball_capture.hpp"

namespace devils_crush::native_runtime {

std::uint8_t CollisionRetryBallCapture::parent_vertical_high_byte() const noexcept
{
    return static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(impact_ball.state.position_y.raw) >> 16U);
}

std::optional<ActiveBallCapture> capture_active_ball(
    const GameStateView& game_state) noexcept
{
    const auto source = game_state.ball();
    if (!source || (source->activity_flags & 0x80U) == 0U) {
        return std::nullopt;
    }

    return ActiveBallCapture{
        .source = *source,
        .state = {
            .position_x = {.raw = source->position_x.raw},
            .position_y = {.raw = source->position_y.raw},
            .velocity_x = source->horizontal_motion.raw,
            .velocity_y = source->vertical_motion.raw,
            .motion_flags = source->motion_flags,
            .collision_state = source->collision_state,
        },
    };
}

std::optional<CollisionRetryBallCapture> capture_collision_retry_ball(
    const GameStateView& game_state) noexcept
{
    const auto impact_ball = capture_active_ball(game_state);
    const auto cached_position = game_state.ball_collision_position_cache();
    if (!impact_ball || !cached_position) {
        return std::nullopt;
    }
    return CollisionRetryBallCapture{
        .impact_ball = *impact_ball,
        .previous_position = {
            .position_x = {.raw = cached_position->position_x.raw},
            .position_y = {.raw = cached_position->position_y.raw},
            .velocity_x = impact_ball->state.velocity_x,
            .velocity_y = impact_ball->state.velocity_y,
            .motion_flags = impact_ball->state.motion_flags,
            .collision_state = impact_ball->state.collision_state,
        },
    };
}

} // namespace devils_crush::native_runtime
