// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/ball_physics.hpp"

#include <algorithm>
#include <bit>

namespace devils_crush::native_runtime {
namespace {

constexpr std::int32_t kPositionMask = 0x00FFFFFF;
constexpr std::int32_t kPositionSignBit = 0x00800000;
constexpr std::int32_t kPositionWrap = 0x01000000;
constexpr std::uint8_t kFractionalOnlyMotion = 0x02U;

[[nodiscard]] constexpr std::int32_t wrap_fixed24_8(const std::int64_t raw)
{
    const auto bits = static_cast<std::int32_t>(raw & kPositionMask);
    return (bits & kPositionSignBit) != 0 ? bits - kPositionWrap : bits;
}

[[nodiscard]] constexpr bool changed_whole_pixels(
    const Fixed24_8 before,
    const Fixed24_8 after)
{
    return (before.raw & ~0xFF) != (after.raw & ~0xFF);
}

[[nodiscard]] constexpr std::int16_t damp_velocity(
    const std::int16_t velocity,
    const std::int8_t divisor) noexcept
{
    const auto value = static_cast<std::int32_t>(velocity);
    const auto drag = value / static_cast<std::int32_t>(divisor);
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value - drag));
}

[[nodiscard]] constexpr std::uint8_t whole_speed_magnitude(
    const std::int16_t velocity) noexcept
{
    const auto whole = static_cast<std::uint8_t>(
        std::bit_cast<std::uint16_t>(velocity) >> 8U);
    return (whole & 0x80U) == 0 ? whole
                                : static_cast<std::uint8_t>(0U - whole);
}

[[nodiscard]] constexpr std::int16_t arithmetic_shift_right(
    const std::int16_t velocity,
    const std::uint8_t shifts) noexcept
{
    auto bits = std::bit_cast<std::uint16_t>(velocity);
    for (std::uint8_t shift = 0; shift < shifts; ++shift) {
        bits = static_cast<std::uint16_t>(
            (bits >> 1U) | (bits & 0x8000U));
    }
    return std::bit_cast<std::int16_t>(bits);
}

} // namespace

BallMotionStep advance_ball_motion(BallState state) noexcept
{
    return advance_ball_motion_substep(
        state,
        {
            .horizontal = state.velocity_x,
            .vertical = state.velocity_y,
        });
}

BallMotionStep advance_ball_motion_substep(
    BallState state,
    const BallMotionIncrement increment) noexcept
{
    const auto previous = state;
    state.position_x.raw = wrap_fixed24_8(
        static_cast<std::int64_t>(state.position_x.raw) + increment.horizontal);
    state.position_y.raw = wrap_fixed24_8(
        static_cast<std::int64_t>(state.position_y.raw) + increment.vertical);

    const bool crossed_whole_pixel = changed_whole_pixels(
        previous.position_x, state.position_x)
        || changed_whole_pixels(previous.position_y, state.position_y);
    state.motion_flags = static_cast<std::uint8_t>(
        state.motion_flags & static_cast<std::uint8_t>(~kFractionalOnlyMotion));
    if (!crossed_whole_pixel) {
        state.motion_flags = static_cast<std::uint8_t>(
            state.motion_flags | kFractionalOnlyMotion);
    }

    return {
        .previous = previous,
        .state = state,
        .crossed_whole_pixel = crossed_whole_pixel,
    };
}

BallState rollback_ball_position(
    BallState state,
    const BallState& previous_position) noexcept
{
    state.position_x = previous_position.position_x;
    state.position_y = previous_position.position_y;
    return state;
}

BallMotionSubsteps split_ball_motion_substeps(const BallState& state) noexcept
{
    const auto maximum_whole_speed = std::max(
        whole_speed_magnitude(state.velocity_x),
        whole_speed_magnitude(state.velocity_y));
    const auto shifts = maximum_whole_speed >= 8U
        ? std::uint8_t{2U}
        : maximum_whole_speed >= 4U ? std::uint8_t{1U} : std::uint8_t{0U};

    return {
        .count = static_cast<std::uint8_t>(1U << shifts),
        .shifts = shifts,
        .maximum_whole_speed = maximum_whole_speed,
        .velocity_x_per_step = arithmetic_shift_right(state.velocity_x, shifts),
        .velocity_y_per_step = arithmetic_shift_right(state.velocity_y, shifts),
    };
}

std::optional<BallPreparedSubstepAdvance>
advance_prepared_ball_motion_substeps(
    BallState state,
    BallSubstepProgress progress) noexcept
{
    if ((progress.requested_steps != 1U && progress.requested_steps != 2U
            && progress.requested_steps != 4U)
        || progress.completed_steps > progress.requested_steps) {
        return std::nullopt;
    }

    BallPreparedSubstepAdvance advance{
        .previous = state,
        .state = state,
        .progress = progress,
    };
    while (!advance.progress.complete()) {
        const auto step = advance_ball_motion_substep(
            advance.state, advance.progress.increment);
        advance.steps[advance.completed_steps] = step;
        advance.state = step.state;
        ++advance.progress.completed_steps;
        ++advance.completed_steps;
    }
    return advance;
}

BallSubstepAdvance advance_ball_motion_substeps(BallState state) noexcept
{
    BallSubstepAdvance advance{
        .previous = state,
        .state = state,
        .plan = split_ball_motion_substeps(state),
    };

    for (std::uint8_t index = 0; index < advance.plan.count; ++index) {
        advance.steps[index] = advance_ball_motion_substep(
            advance.state,
            {
                .horizontal = advance.plan.velocity_x_per_step,
                .vertical = advance.plan.velocity_y_per_step,
            });
        advance.state = advance.steps[index].state;
        ++advance.completed_steps;
    }

    return advance;
}

std::optional<BallDampingStep> apply_ball_drag_and_gravity(
    BallState state,
    const BallMotionTuning& tuning) noexcept
{
    if ((state.velocity_x != 0 && tuning.horizontal_drag_divisor == 0)
        || (state.velocity_y != 0 && tuning.vertical_drag_divisor == 0)) {
        return std::nullopt;
    }

    const auto previous = state;
    if (state.velocity_x != 0) {
        state.velocity_x = damp_velocity(
            state.velocity_x, tuning.horizontal_drag_divisor);
    }
    if (state.velocity_y != 0) {
        state.velocity_y = damp_velocity(
            state.velocity_y, tuning.vertical_drag_divisor);
    }
    state.velocity_y = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(state.velocity_y)
            + tuning.vertical_gravity));

    return BallDampingStep{.previous = previous, .state = state};
}

} // namespace devils_crush::native_runtime
