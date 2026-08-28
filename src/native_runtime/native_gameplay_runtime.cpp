// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/native_gameplay_runtime.hpp"

#include <utility>

namespace devils_crush::native_runtime {

bool NativeGameplayRuntime::bootstrap(const NativeGameplayBootstrap& state) noexcept
{
    if (gameplay_) {
        return false;
    }

    gameplay_.emplace();
    if (!apply_native_gameplay_bootstrap(*gameplay_, state)) {
        gameplay_.reset();
        return false;
    }
    ball_frame_schedule_ = state.ball_frame_schedule;
    frame_number_ = 0U;
    return true;
}

bool NativeGameplayRuntime::ready() const noexcept
{
    return gameplay_.has_value();
}

std::uint64_t NativeGameplayRuntime::frame_number() const noexcept
{
    return frame_number_;
}

BallFrameSchedule NativeGameplayRuntime::ball_frame_schedule() const noexcept
{
    return ball_frame_schedule_;
}

const NativeGameplayState* NativeGameplayRuntime::gameplay() const noexcept
{
    return gameplay_ ? &*gameplay_ : nullptr;
}

NativeGameplayState* NativeGameplayRuntime::gameplay() noexcept
{
    return gameplay_ ? &*gameplay_ : nullptr;
}

std::optional<NativeGameplayFrame> NativeGameplayRuntime::advance_physics_epoch(
    const BallMotionTuning& tuning,
    const CollisionMap& map,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto physics = gameplay_->advance_physics(tuning, map);
    if (!physics) {
        return std::nullopt;
    }

    NativeGameplayFrame frame{
        .frame_number = ++frame_number_,
        .physics = *physics,
    };
    if (frame.physics.no_contact) {
        frame.actor_updates = gameplay_->advance_actor_phase(playfield);
        frame.audio_events = gameplay_->take_audio_events();
    }
    return frame;
}

std::optional<NativeGameplayFrame> NativeGameplayRuntime::advance_frame(
    const BallMotionTuning& tuning,
    const CollisionMap& map,
    const PlayfieldBounds playfield)
{
    return advance_physics_epoch(tuning, map, playfield);
}

std::optional<NativeInitialBallMotionUpdate>
NativeGameplayRuntime::advance_initial_ball_motion(
    const BallMotionTuning& tuning,
    const CollisionMap& map)
{
    if (!gameplay_
        || ball_frame_schedule_.owner
            != BallFrameOwner::initial_substep_transition
        || !ball_frame_schedule_.substep_progress) {
        return std::nullopt;
    }

    const auto motion = gameplay_->resume_initial_ball_motion(
        *ball_frame_schedule_.substep_progress, tuning, map);
    if (!motion) {
        return std::nullopt;
    }

    NativeInitialBallMotionUpdate update{
        .motion = *motion,
        .next_schedule = ball_frame_schedule_,
    };
    if (!motion->completes_initial_pass()) {
        return update;
    }

    const auto next_schedule = advance_initial_ball_frame_schedule(
        ball_frame_schedule_, *motion->transition,
        *motion->next_substep_progress);
    if (!next_schedule) {
        return std::nullopt;
    }
    ball_frame_schedule_ = *next_schedule;
    update.next_schedule = *next_schedule;
    update.returned_to_slot_zero_actor_loop =
        motion->transition->disposition
        == BallInitialTransitionDisposition::returns_to_caller;
    return update;
}

std::optional<NativeGameplayCollisionFrame>
NativeGameplayRuntime::complete_playfield_collision(
    const BallState resolved_ball,
    const PlayfieldBounds playfield)
{
    if (!gameplay_ || !gameplay_->pending_playfield_contact()) {
        return std::nullopt;
    }

    const auto contact = *gameplay_->pending_playfield_contact();
    if (!gameplay_->commit_playfield_collision(resolved_ball)) {
        return std::nullopt;
    }

    return NativeGameplayCollisionFrame{
        .frame_number = frame_number_,
        .contact = contact,
        .resolved_ball = resolved_ball,
        .actor_updates = gameplay_->advance_actor_phase(playfield),
        .audio_events = gameplay_->take_audio_events(),
    };
}

std::optional<NativeGameplayCf33PlayfieldUpdate>
NativeGameplayRuntime::resume_cf33_playfield_contact(
    const MappedRomView& rom,
    const GameStateView& state,
    const BallMotionTuning& tuning,
    const CollisionMap& map,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }
    const auto physics = gameplay_->resume_cf33_playfield_contact(
        rom, state, tuning, map);
    if (!physics) {
        return std::nullopt;
    }

    NativeGameplayCf33PlayfieldUpdate update{
        .frame_number = frame_number_,
        .physics = *physics,
    };
    if (update.completes_frame()) {
        update.actor_updates = gameplay_->advance_actor_phase(playfield);
        update.audio_events = gameplay_->take_audio_events();
    }
    return update;
}

std::optional<NativeGameplayCollisionRetryUpdate>
NativeGameplayRuntime::retry_pending_playfield_collision(
    const CollisionRetryOffsets& offsets,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto retry = gameplay_->retry_pending_playfield_collision(offsets);
    if (!retry) {
        return std::nullopt;
    }

    NativeGameplayCollisionRetryUpdate update{
        .frame_number = frame_number_,
        .retry = *retry,
    };
    if (update.completes_frame()) {
        update.actor_updates = gameplay_->advance_actor_phase(playfield);
        update.audio_events = gameplay_->take_audio_events();
    }
    return update;
}

NativeGameplayCollisionRetryPlacementUpdate
NativeGameplayRuntime::finish_collision_retry_placement(
    CollisionRetryPlacementUpdate placement,
    std::optional<CollisionRetryFullScanResult> scan,
    const PlayfieldBounds playfield)
{
    NativeGameplayCollisionRetryPlacementUpdate update{
        .frame_number = frame_number_,
        .scan = std::move(scan),
        .placement = std::move(placement),
    };
    if (update.completes_frame()) {
        update.actor_updates = gameplay_->advance_actor_phase(playfield);
        update.audio_events = gameplay_->take_audio_events();
    }
    return update;
}

std::optional<NativeGameplayCollisionRetryPlacementUpdate>
NativeGameplayRuntime::begin_pending_collision_retry(
    const CollisionRetryOffsetAttempts& offsets,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto placement = gameplay_->begin_pending_collision_retry(offsets);
    if (!placement) {
        return std::nullopt;
    }
    return finish_collision_retry_placement(*placement, std::nullopt, playfield);
}

std::optional<NativeGameplayCollisionRetryPlacementUpdate>
NativeGameplayRuntime::resolve_pending_collision_retry_full_scan(
    const CollisionRetryFullScanOutcome outcome,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto placement = gameplay_->resolve_pending_collision_retry_full_scan(outcome);
    if (!placement) {
        return std::nullopt;
    }
    return finish_collision_retry_placement(*placement, std::nullopt, playfield);
}

std::optional<NativeGameplayCollisionRetryPlacementUpdate>
NativeGameplayRuntime::resolve_pending_collision_retry_full_scan(
    const MappedRomView& rom,
    const GameStateView& state,
    const std::uint8_t parent_vertical_high_byte,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto resolution = gameplay_->resolve_pending_collision_retry_full_scan(
        rom, state, parent_vertical_high_byte);
    if (!resolution) {
        return std::nullopt;
    }
    return finish_collision_retry_placement(
        resolution->placement, resolution->scan, playfield);
}

std::optional<NativeGameplayCollisionRetryPlacementUpdate>
NativeGameplayRuntime::select_pending_collision_retry_direction(
    const std::uint8_t random_byte,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto placement = gameplay_->select_pending_collision_retry_direction(
        random_byte);
    if (!placement) {
        return std::nullopt;
    }
    return finish_collision_retry_placement(*placement, std::nullopt, playfield);
}

std::optional<NativeGameplayCollisionRetryPlacementResume>
NativeGameplayRuntime::resume_rearmed_collision_retry(
    const BallMotionTuning& tuning,
    const CollisionMap& map,
    const PlayfieldBounds playfield)
{
    if (!gameplay_) {
        return std::nullopt;
    }

    const auto physics = gameplay_->resume_rearmed_collision_retry(tuning, map);
    if (!physics) {
        return std::nullopt;
    }

    NativeGameplayCollisionRetryPlacementResume update{
        .frame_number = frame_number_,
        .physics = *physics,
    };
    if (update.completes_frame()) {
        update.actor_updates = gameplay_->advance_actor_phase(playfield);
        update.audio_events = gameplay_->take_audio_events();
    }
    return update;
}

std::optional<NativeGameplayActorFrame>
NativeGameplayRuntime::advance_actor_frame(const PlayfieldBounds playfield)
{
    if (!gameplay_ || gameplay_->pending_playfield_contact()) {
        return std::nullopt;
    }

    return NativeGameplayActorFrame{
        .frame_number = ++frame_number_,
        .actor_updates = gameplay_->advance_actor_phase(playfield),
        .audio_events = gameplay_->take_audio_events(),
    };
}

} // namespace devils_crush::native_runtime
