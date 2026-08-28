// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/physics_state.hpp"

#include <limits>

namespace devils_crush::native_runtime {

BallCollisionLoop begin_ball_collision_loop(PhysicsState state) noexcept
{
    return {
        .previous = state,
        .state = state,
        .plan = split_ball_motion_substeps(state.ball),
    };
}

std::optional<BallCollisionLoop> begin_prepared_ball_collision_loop(
    PhysicsState state,
    const BallSubstepProgress progress) noexcept
{
    std::uint8_t shifts = 0U;
    switch (progress.requested_steps) {
    case 1U:
        break;
    case 2U:
        shifts = 1U;
        break;
    case 4U:
        shifts = 2U;
        break;
    default:
        return std::nullopt;
    }
    if (progress.completed_steps > progress.requested_steps) {
        return std::nullopt;
    }

    // `maximum_whole_speed` is diagnostic data from a fresh DC95 split. A
    // continuation captures the selected count and exact increment instead,
    // and must not infer a possibly different maximum from its later state.
    return BallCollisionLoop{
        .previous = state,
        .state = state,
        .plan = {
            .count = progress.requested_steps,
            .shifts = shifts,
            .velocity_x_per_step = progress.increment.horizontal,
            .velocity_y_per_step = progress.increment.vertical,
        },
        .completed_substeps = progress.completed_steps,
    };
}

std::optional<BallCollisionLoopUpdate> advance_ball_collision_loop(
    BallCollisionLoop loop,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    const auto previous = loop;
    if (loop.completed_substeps >= loop.plan.count) {
        const auto damping = apply_ball_drag_and_gravity(loop.state.ball, tuning);
        if (!damping) {
            return std::nullopt;
        }
        loop.state.ball = damping->state;
        return BallCollisionLoopUpdate{
            .previous = previous,
            .state = loop,
            .disposition = BallCollisionLoopDisposition::frame_complete,
            .damping = *damping,
        };
    }

    if (loop.remaining_attempts == 0U) {
        return BallCollisionLoopUpdate{
            .previous = previous,
            .state = loop,
            .disposition = BallCollisionLoopDisposition::attempt_cap_exhausted,
        };
    }

    --loop.remaining_attempts;
    const auto movement = advance_ball_motion_substep(
        loop.state.ball,
        {
            .horizontal = loop.plan.velocity_x_per_step,
            .vertical = loop.plan.velocity_y_per_step,
        });
    BallPlayfieldProbe probe{
        .substep_index = loop.completed_substeps,
    };
    const auto horizontal = movement.state.position_x.whole_pixels();
    const auto vertical = movement.state.position_y.whole_pixels();
    if (horizontal >= 0
        && horizontal <= std::numeric_limits<std::uint16_t>::max()
        && vertical >= 0
        && vertical <= std::numeric_limits<std::uint16_t>::max()) {
        probe.position_in_world = true;
        probe.collision = evaluate_playfield_collision(
            map,
            PlayfieldCollisionProbe{
                .horizontal_pixels = static_cast<std::uint16_t>(horizontal),
                .vertical_pixels = static_cast<std::uint16_t>(vertical),
            });
    }

    if (probe.collision.disposition == PlayfieldCollisionDisposition::solid) {
        return BallCollisionLoopUpdate{
            .previous = previous,
            .state = loop,
            .disposition = BallCollisionLoopDisposition::playfield_contact,
            .movement = movement,
            .probe = probe,
            .contact = PlayfieldContact{
                .substep_index = loop.completed_substeps,
                .entry_ball = movement.previous,
                .impact_ball = movement.state,
                .impact = probe.collision,
            },
        };
    }

    loop.state.ball = movement.state;
    ++loop.completed_substeps;
    return BallCollisionLoopUpdate{
        .previous = previous,
        .state = loop,
        .disposition = BallCollisionLoopDisposition::substep_clear,
        .movement = movement,
        .probe = probe,
    };
}

BallCollisionLoop retry_ball_collision_substep(BallCollisionLoop loop) noexcept
{
    return loop;
}

BallCollisionLoop resume_ball_collision_after_response(
    BallCollisionLoop loop,
    const BallState resolved_ball) noexcept
{
    loop.state.ball = resolved_ball;
    loop.plan = split_ball_motion_substeps(resolved_ball);
    ++loop.completed_substeps;
    return loop;
}

std::optional<PhysicsFrameStep> advance_physics_without_contact(
    PhysicsState state,
    const BallMotionTuning& tuning) noexcept
{
    const auto previous = state;
    const auto movement = advance_ball_motion_substeps(state.ball);
    const auto damping = apply_ball_drag_and_gravity(movement.state, tuning);
    if (!damping) {
        return std::nullopt;
    }

    state.ball = damping->state;
    return PhysicsFrameStep{
        .previous = previous,
        .state = state,
        .movement = movement,
        .damping = *damping,
    };
}

std::optional<PhysicsPlayfieldFrame> advance_physics_with_playfield_scan(
    PhysicsState state,
    const BallMotionTuning& tuning,
    const CollisionMap& map) noexcept
{
    const auto previous = state;
    auto loop = begin_ball_collision_loop(state);
    PlayfieldCollisionScan collision_scan{
        .movement = {
            .previous = state.ball,
            .state = state.ball,
            .plan = loop.plan,
        },
    };
    for (;;) {
        const auto update = advance_ball_collision_loop(loop, tuning, map);
        if (!update) {
            return std::nullopt;
        }
        loop = update->state;
        if (update->movement && update->probe) {
            const auto index = update->probe->substep_index;
            collision_scan.movement.steps[index] = *update->movement;
            collision_scan.movement.state = update->movement->state;
            collision_scan.probes[index] = *update->probe;
            ++collision_scan.completed_probes;
        }

        if (update->disposition
            == BallCollisionLoopDisposition::playfield_contact) {
            collision_scan.first_solid_substep = update->contact->substep_index;
            return PhysicsPlayfieldFrame{
                .previous = previous,
                .collision_scan = collision_scan,
                .contact = update->contact,
                .collision_loop = loop,
            };
        }
        if (update->disposition == BallCollisionLoopDisposition::substep_clear) {
            collision_scan.movement.completed_steps = loop.completed_substeps;
            continue;
        }
        if (update->disposition
            == BallCollisionLoopDisposition::attempt_cap_exhausted) {
            return std::nullopt;
        }

        state = loop.state;
        const PhysicsFrameStep no_contact{
            .previous = previous,
            .state = state,
            .movement = collision_scan.movement,
            .damping = *update->damping,
        };
        return PhysicsPlayfieldFrame{
            .previous = previous,
            .collision_scan = collision_scan,
            .no_contact = no_contact,
        };
    }
}

} // namespace devils_crush::native_runtime
