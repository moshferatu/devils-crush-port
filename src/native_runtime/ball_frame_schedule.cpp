// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/ball_frame_schedule.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kBallSlot = 0U;
constexpr std::uint8_t kOffTableFlag = 0x01U;

constexpr std::uint16_t kInitialSubstepLoop = 0xC6C2U;
constexpr std::uint16_t kResumedSubstepLoop = 0xC721U;
constexpr std::uint16_t kTableControlWait = 0xC79BU;
constexpr std::uint16_t kTableControlWaitContinuation = 0xC79EU;
constexpr std::uint16_t kTableControlGate = 0xC7CFU;
constexpr std::uint16_t kOffTableContinuation = 0xC8DAU;
constexpr std::uint16_t kOffTableDampingContinuation = 0xC8E7U;
constexpr std::uint16_t kResumedSubstepLoopReturn = 0xC8F0U;
constexpr std::uint16_t kInitialMotionGateHorizontalPixels = 0x00F0U;

[[nodiscard]] bool is_fresh_motion_cursor(
    const BallSubstepProgress progress) noexcept
{
    return progress.completed_steps == 0U
        && (progress.requested_steps == 1U || progress.requested_steps == 2U
            || progress.requested_steps == 4U);
}

} // namespace

BallInitialTransition classify_ball_initial_transition(const BallState& ball) noexcept
{
    // C6F5/C6FA stage $3180:$3200, which is the ball's unsigned X word,
    // before C705's E93C comparison against $00F0.  Convert through an
    // unsigned representation so negative fixed-point values retain the
    // same low 16 source bits rather than becoming a signed-domain decision.
    const auto horizontal_pixels = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(ball.position_x.raw) >> 8U);
    return {
        .horizontal_pixels = horizontal_pixels,
        .disposition = horizontal_pixels < kInitialMotionGateHorizontalPixels
            ? BallInitialTransitionDisposition::schedules_normal_motion
            : BallInitialTransitionDisposition::returns_to_caller,
    };
}

BallFrameSchedule capture_ball_frame_schedule(const GameStateView& state) noexcept
{
    const auto ball = state.ball();
    const auto actor = state.actor(kBallSlot);
    if (!ball || !actor || (ball->activity_flags & 0x80U) == 0U) {
        return {};
    }
    const BallCollisionShapeSelection collision_shape{
        .directory_index = actor->collision_shape_directory_index,
        .phase = actor->collision_shape_phase,
    };

    switch (actor->handler_address) {
    case kTableControlWait:
    case kTableControlWaitContinuation:
    case kTableControlGate:
        return {
            .owner = BallFrameOwner::table_control_wait,
            .source_handler_address = actor->handler_address,
            .collision_shape = collision_shape,
        };
    case kOffTableContinuation:
    case kOffTableDampingContinuation:
        return {
            .owner = BallFrameOwner::off_table_transition,
            .source_handler_address = actor->handler_address,
            .collision_shape = collision_shape,
        };
    case kResumedSubstepLoop:
    case kResumedSubstepLoopReturn:
        return {
            .owner = (ball->motion_flags & kOffTableFlag) == 0U
                ? BallFrameOwner::normal_motion
                : BallFrameOwner::off_table_transition,
            .source_handler_address = actor->handler_address,
            .substep_progress = state.ball_substep_progress(),
            .collision_shape = collision_shape,
        };
    case kInitialSubstepLoop:
        // C708 returns to the caller while the ball remains at or beyond the
        // unsigned X=$00F0 gate.  Otherwise it installs C721 before its DC95
        // tail handoff.  It is a named initial transition, not an unknown
        // scheduler state.
        {
            const auto progress = state.ball_substep_progress();
            return {
                .owner = BallFrameOwner::initial_substep_transition,
                .source_handler_address = actor->handler_address,
                .substep_progress = progress,
                .initial_transition = progress && progress->complete()
                    ? std::optional{classify_ball_initial_transition({
                        .position_x = {.raw = ball->position_x.raw},
                        .position_y = {.raw = ball->position_y.raw},
                        .velocity_x = ball->horizontal_motion.raw,
                        .velocity_y = ball->vertical_motion.raw,
                        .motion_flags = ball->motion_flags,
                        .collision_state = ball->collision_state,
                    })}
                    : std::nullopt,
                .collision_shape = collision_shape,
            };
        }
    default:
        return {
            .source_handler_address = actor->handler_address,
            .collision_shape = collision_shape,
        };
    }
}

std::optional<BallFrameSchedule> advance_initial_ball_frame_schedule(
    BallFrameSchedule schedule,
    const BallInitialTransition transition,
    const BallSubstepProgress fresh_progress) noexcept
{
    if (schedule.owner != BallFrameOwner::initial_substep_transition
        || !schedule.collision_shape || !is_fresh_motion_cursor(fresh_progress)) {
        return std::nullopt;
    }

    schedule.source_handler_address = 0U;
    schedule.substep_progress = fresh_progress;
    schedule.initial_transition.reset();
    switch (transition.disposition) {
    case BallInitialTransitionDisposition::schedules_normal_motion:
        schedule.owner = BallFrameOwner::normal_motion;
        schedule.collision_shape = BallCollisionShapeSelection{};
        return schedule;
    case BallInitialTransitionDisposition::returns_to_caller:
        return schedule;
    }
    return std::nullopt;
}

std::string_view describe_ball_frame_owner(const BallFrameOwner owner) noexcept
{
    switch (owner) {
    case BallFrameOwner::normal_motion:
        return "normal motion";
    case BallFrameOwner::initial_substep_transition:
        return "initial substep transition";
    case BallFrameOwner::table_control_wait:
        return "table-control wait";
    case BallFrameOwner::off_table_transition:
        return "off-table transition";
    case BallFrameOwner::unrecognized:
        return "unrecognized";
    }
    return "unrecognized";
}

} // namespace devils_crush::native_runtime
