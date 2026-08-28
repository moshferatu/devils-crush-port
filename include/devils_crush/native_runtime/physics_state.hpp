// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/playfield_collision.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// State owned by native table physics. Contact history is kept with the ball
// because it is reset and consumed by the collision loop, not by an actor.
struct PhysicsState {
    BallState ball;
    ContactState contact;

    [[nodiscard]] bool operator==(const PhysicsState&) const = default;
};

// The no-contact frame path: every substep is integrated, then the original
// frame velocity is damped and accelerated. A later collision stage will run
// between the movement and damping portions and may replace that velocity.
struct PhysicsFrameStep {
    PhysicsState previous;
    PhysicsState state;
    BallSubstepAdvance movement;
    BallDampingStep damping;

    [[nodiscard]] bool operator==(const PhysicsFrameStep&) const = default;
};

// The cartridge's C6/C7 loop carries more than a ball and a collision map:
// `$56` counts consumed substeps, `$57` is the currently selected DC95 split,
// and `$59` limits all retries to sixteen trips through the loop.  Keeping
// those values named lets a contact response resume the same frame instead of
// pretending every solid tile ends it.
struct BallCollisionLoop {
    PhysicsState previous;
    PhysicsState state;
    BallMotionSubsteps plan;
    std::uint8_t completed_substeps = 0U;
    std::uint8_t remaining_attempts = 16U;

    [[nodiscard]] bool operator==(const BallCollisionLoop&) const = default;
};

// A frame evaluated against the native playfield. A solid tile deliberately
// stops before velocity response: that next stage needs the collision normal
// and table-rule result, neither of which is inferred by the map sampler.
// The loop snapshot retains the exact source resume point for that resolver.
// When `no_contact` is present, the frame completed and includes damping and
// gravity.
struct PhysicsPlayfieldFrame {
    PhysicsState previous;
    PlayfieldCollisionScan collision_scan;
    std::optional<PlayfieldContact> contact;
    std::optional<BallCollisionLoop> collision_loop;
    std::optional<PhysicsFrameStep> no_contact;

    [[nodiscard]] bool requires_collision_resolution() const noexcept
    {
        return contact.has_value();
    }

    [[nodiscard]] bool operator==(const PhysicsPlayfieldFrame&) const = default;
};

enum class BallCollisionLoopDisposition : std::uint8_t {
    substep_clear,
    playfield_contact,
    frame_complete,
    attempt_cap_exhausted,
};

// One source-visible transition through the C6/C7 substep loop.  A solid map
// probe leaves `state.ball` at the entry position and returns a contact; the
// caller must choose the source's separating retry or its CF33/DC95 response
// before the loop can continue.
struct BallCollisionLoopUpdate {
    BallCollisionLoop previous;
    BallCollisionLoop state;
    BallCollisionLoopDisposition disposition =
        BallCollisionLoopDisposition::substep_clear;
    std::optional<BallMotionStep> movement;
    std::optional<BallPlayfieldProbe> probe;
    std::optional<PlayfieldContact> contact;
    std::optional<BallDampingStep> damping;

    [[nodiscard]] bool operator==(const BallCollisionLoopUpdate&) const = default;
};

// Starts the adaptive C6/C7 collision loop from DC95's current motion split.
[[nodiscard]] BallCollisionLoop begin_ball_collision_loop(
    PhysicsState state) noexcept;

// Resumes C6/C7 from DC95's already-prepared cursor captured at a coroutine
// boundary. Unlike `begin_ball_collision_loop`, this deliberately does not
// derive another split from the current frame velocity: `$B9:$BC`, `$56`, and
// `$57` are the source authority for the increment and consumed prefix. The
// returned loop still begins with C6/C7's sixteen-trip attempt cap.
[[nodiscard]] std::optional<BallCollisionLoop>
begin_prepared_ball_collision_loop(
    PhysicsState state,
    BallSubstepProgress progress) noexcept;

// Advances exactly one loop trip. A completed split performs DD09's damping;
// a solid CD74 probe is retained for CF33/CDF8 instead. `nullopt` is reserved
// for invalid DD09 tuning on the completed-frame path.
[[nodiscard]] std::optional<BallCollisionLoopUpdate>
advance_ball_collision_loop(
    BallCollisionLoop loop,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept;

// CF8F clears the loop's consume bit. Its caller restores the cached entry
// position and retries the same substep, retaining the already-spent attempt.
[[nodiscard]] BallCollisionLoop retry_ball_collision_substep(
    BallCollisionLoop loop) noexcept;

// The normal CF33 branches set the consume bit and tail-call DC95. They retain
// the already-consumed substep count but replace the split with the response
// velocity's new adaptive plan, exactly as the source does before returning to
// C6/C7.
[[nodiscard]] BallCollisionLoop resume_ball_collision_after_response(
    BallCollisionLoop loop,
    BallState resolved_ball) noexcept;

// Returns no result when tuning asks a moving component to divide by zero.
[[nodiscard]] std::optional<PhysicsFrameStep> advance_physics_without_contact(
    PhysicsState state,
    const BallMotionTuning& tuning) noexcept;

// Runs the native adaptive movement loop and samples the playfield after each
// substep. A collision-free frame is completed with the regular damping pass.
// Returns no result only when that no-contact damping pass has invalid tuning.
[[nodiscard]] std::optional<PhysicsPlayfieldFrame>
advance_physics_with_playfield_scan(
    PhysicsState state,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept;

} // namespace devils_crush::native_runtime
