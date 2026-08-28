// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_motion.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::int32_t kPositionMask = 0x00FFFFFF;
constexpr std::int32_t kPositionSignBit = 0x00800000;
constexpr std::int32_t kPositionWrap = 0x01000000;

[[nodiscard]] constexpr std::int32_t wrap_fixed24_8(const std::int64_t raw)
{
    const auto bits = static_cast<std::int32_t>(raw & kPositionMask);
    return (bits & kPositionSignBit) != 0 ? bits - kPositionWrap : bits;
}

[[nodiscard]] constexpr std::int16_t adjust_velocity(
    const std::int16_t velocity,
    const std::int32_t position,
    const std::int32_t target,
    const std::uint8_t adjustment)
{
    if (position == target) {
        return velocity;
    }

    const auto delta = position < target ? adjustment : -adjustment;
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(velocity)
        + static_cast<std::uint16_t>(delta));
}

[[nodiscard]] constexpr bool outside(
    const std::int32_t coordinate,
    const std::int32_t limit)
{
    return coordinate < 0 || coordinate >= limit;
}

} // namespace

ActorMotionStep advance_actor_motion(
    ActorMotionState state,
    const PlayfieldBounds playfield) noexcept
{
    bool left_playfield = false;

    if (state.integrate_y) {
        state.position_y.raw = wrap_fixed24_8(
            static_cast<std::int64_t>(state.position_y.raw) + state.velocity_y);
        left_playfield = outside(state.position_y.whole_pixels(), playfield.height);
    }
    if (state.integrate_x) {
        state.position_x.raw = wrap_fixed24_8(
            static_cast<std::int64_t>(state.position_x.raw) + state.velocity_x);
        left_playfield = left_playfield
            || outside(state.position_x.whole_pixels(), playfield.width);
    }
    if (state.steer_y) {
        state.velocity_y = adjust_velocity(
            state.velocity_y, state.position_y.whole_pixels(), state.target_y,
            state.velocity_adjustment_y);
    }
    if (state.steer_x) {
        state.velocity_x = adjust_velocity(
            state.velocity_x, state.position_x.whole_pixels(), state.target_x,
            state.velocity_adjustment_x);
    }

    return {.state = state, .left_playfield = left_playfield};
}

} // namespace devils_crush::native_runtime
