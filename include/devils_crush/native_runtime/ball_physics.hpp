// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_motion.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// One active ball. Devil's Crush has one ball in normal play, so this is
// direct game state rather than a generic actor dispatch case.
struct BallState {
    Fixed24_8 position_x;
    Fixed24_8 position_y;
    std::int16_t velocity_x = 0;
    std::int16_t velocity_y = 0;
    // Other physics stages own the remaining bits. Bit 1 records an update
    // that changed only the fractional position.
    std::uint8_t motion_flags = 0;
    // The dedicated ball still uses the generic actor collision latch. CF33
    // reads its high bit after a signed selector to choose CDF8's position
    // retry instead of CF3F's normal collision response.
    std::uint8_t collision_state = 0;

    [[nodiscard]] bool operator==(const BallState&) const = default;
};

struct BallMotionStep {
    BallState previous;
    BallState state;
    bool crossed_whole_pixel = false;

    [[nodiscard]] bool operator==(const BallMotionStep&) const = default;
};

// The collision loop carries a separate signed 8.8 increment while it works
// through a split high-speed frame. It is deliberately not the authoritative
// frame velocity retained by BallState for later damping.
struct BallMotionIncrement {
    std::int16_t horizontal = 0;
    std::int16_t vertical = 0;

    [[nodiscard]] bool operator==(const BallMotionIncrement&) const = default;
};

// Integrates the ball's signed 8.8 velocity into its signed 16.8 position.
// Coordinates retain the original 24-bit wrap, and bit 1 of motion_flags is
// recomputed from whether either whole-pixel coordinate changed.
[[nodiscard]] BallMotionStep advance_ball_motion(BallState state) noexcept;

// Performs exactly one F1B4 position integration using the collision loop's
// current split increment. The frame velocity stored in `state` is preserved.
[[nodiscard]] BallMotionStep advance_ball_motion_substep(
    BallState state,
    BallMotionIncrement increment) noexcept;

// Restores only the ball position that immediately preceded a rejected
// collision step. The cartridge's rollback helper deliberately leaves the
// caller-selected velocity and motion flags intact, so response code can
// transform velocity before requesting this position-only operation.
[[nodiscard]] BallState rollback_ball_position(
    BallState state,
    const BallState& previous_position) noexcept;

// The collision loop advances fast balls in equal signed-8.8 increments.
// The largest whole-pixel component selects one, two, or four checks; each
// per-check velocity is an arithmetic (not rounding) division by that count.
struct BallMotionSubsteps {
    std::uint8_t count = 1;
    std::uint8_t shifts = 0;
    std::uint8_t maximum_whole_speed = 0;
    std::int16_t velocity_x_per_step = 0;
    std::int16_t velocity_y_per_step = 0;

    [[nodiscard]] bool operator==(const BallMotionSubsteps&) const = default;
};

[[nodiscard]] BallMotionSubsteps split_ball_motion_substeps(
    const BallState& state) noexcept;

// The C6/C7 coroutine may be entered after DC95 has already split a frame's
// velocity.  This is that live cursor: it retains the prepared signed
// increment and exactly how much of the split the loop has consumed.  It is
// deliberately separate from BallMotionSubsteps, which describes a new split
// derived from a complete frame velocity.
struct BallSubstepProgress {
    BallMotionIncrement increment;
    std::uint8_t completed_steps = 0;
    std::uint8_t requested_steps = 0;

    [[nodiscard]] bool complete() const noexcept
    {
        return completed_steps == requested_steps;
    }

    [[nodiscard]] bool operator==(const BallSubstepProgress&) const = default;
};

// Completes the clear suffix of a DC95-prepared collision loop.  Only the
// one-, two-, and four-substep plans that DC95 can create are accepted.  A
// contact must instead stay with the collision-loop owner, so this helper
// intentionally performs no collision detection or response.
struct BallPreparedSubstepAdvance {
    BallState previous;
    BallState state;
    BallSubstepProgress progress;
    std::array<BallMotionStep, 4> steps{};
    std::uint8_t completed_steps = 0;

    [[nodiscard]] bool operator==(const BallPreparedSubstepAdvance&) const
        = default;
};

[[nodiscard]] std::optional<BallPreparedSubstepAdvance>
advance_prepared_ball_motion_substeps(
    BallState state,
    BallSubstepProgress progress) noexcept;

// The no-contact path through the collision loop. The ball moves once per
// planned collision check, while its frame velocity remains authoritative for
// later physics stages. `steps` contains the completed prefix only.
struct BallSubstepAdvance {
    BallState previous;
    BallState state;
    BallMotionSubsteps plan;
    std::array<BallMotionStep, 4> steps{};
    std::uint8_t completed_steps = 0;

    [[nodiscard]] bool operator==(const BallSubstepAdvance&) const = default;
};

[[nodiscard]] BallSubstepAdvance advance_ball_motion_substeps(
    BallState state) noexcept;

// Per-frame table physics applied after a ball update. Each nonzero signed
// velocity is reduced by its signed drag divisor; the vertical component then
// receives the table's unsigned gravity increment.
struct BallMotionTuning {
    std::int8_t horizontal_drag_divisor = 1;
    std::int8_t vertical_drag_divisor = 1;
    std::uint8_t vertical_gravity = 0;

    [[nodiscard]] bool operator==(const BallMotionTuning&) const = default;
};

struct BallDampingStep {
    BallState previous;
    BallState state;

    [[nodiscard]] bool operator==(const BallDampingStep&) const = default;
};

// Returns no result only when a nonzero velocity would be divided by zero.
// A stationary axis may have a zero divisor because its drag calculation is
// skipped by the original behavior.
[[nodiscard]] std::optional<BallDampingStep> apply_ball_drag_and_gravity(
    BallState state,
    const BallMotionTuning& tuning) noexcept;

} // namespace devils_crush::native_runtime
