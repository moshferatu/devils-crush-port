// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/gameplay_bootstrap.hpp"
#include "devils_crush/native_runtime/native_gameplay_state.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace devils_crush::native_runtime {

// One completed native actor phase. This is intentionally narrower than a
// display frame while the physics, presentation, and scene owners are still
// migrating. It gives the eventual NativePceRuntime one authoritative place
// to advance the already-native scheduler and collect its ordered audio work.
struct NativeGameplayActorFrame {
    std::uint64_t frame_number = 0U;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool operator==(const NativeGameplayActorFrame&) const = default;
};

// One candidate native gameplay physics epoch. This is deliberately not an
// emulator presentation frame: DC95 can start, resume, or complete a physics
// epoch on either side of an emulator frame boundary. A clear physics pass is
// followed by the recovered actor order and its resulting gameplay-audio
// handoff. A solid playfield contact deliberately stops at the existing
// collision-resolution boundary: its actor and audio vectors remain empty
// until a later native resolver owns the response.
struct NativeGameplayFrame {
    std::uint64_t frame_number = 0U;
    PhysicsPlayfieldFrame physics;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool requires_collision_resolution() const noexcept
    {
        return physics.requires_collision_resolution();
    }

    [[nodiscard]] bool operator==(const NativeGameplayFrame&) const = default;
};

// One native C6C2 initial-owner step. It is intentionally not a presentation
// frame: after the clear path it has updated the ball's next coroutine state.
// The C708 high-side exit returns to C520's post-handler filing call. Because
// C6C2 is slot zero's handler, C606 immediately takes its slot-zero exit and
// does not mutate a presentation queue; that recovered no-op is recorded
// explicitly below. The wider actor scan remains a later runtime boundary.
struct NativeInitialBallMotionUpdate {
    NativeGameplayState::InitialBallMotionHandoff motion;
    BallFrameSchedule next_schedule;
    bool returned_to_slot_zero_actor_loop = false;

    [[nodiscard]] bool operator==(const NativeInitialBallMotionUpdate&) const
        = default;
};

// A compatibility completion for a frame whose playfield contact was
// previously returned by `advance_frame`. It is suitable only for resolvers
// that have already completed the source loop; CF33 responses must instead
// use the retained collision-loop cursor so they can re-split or retry within
// the same physics frame before actor order and gameplay audio are advanced.
struct NativeGameplayCollisionFrame {
    std::uint64_t frame_number = 0U;
    PlayfieldContact contact;
    BallState resolved_ball;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool operator==(const NativeGameplayCollisionFrame&) const = default;
};

// The same-frame continuation after a typed CF33 playfield resolution. Actor
// work is present only after the cursor reaches DD09; another solid contact or
// the CDF8 handoff keeps this frame pending.
struct NativeGameplayCf33PlayfieldUpdate {
    std::uint64_t frame_number = 0U;
    NativeGameplayState::Cf33PlayfieldLoopResume physics;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool completes_frame() const noexcept
    {
        return physics.completes_frame();
    }

    [[nodiscard]] bool operator==(const NativeGameplayCf33PlayfieldUpdate&) const = default;
};

// The outcome of applying CEB5's already-native collision retry to a pending
// playfield contact. A clear retry has committed the recovered ball and owns
// the remaining actor/audio work in the original frame. A full-scan retry
// intentionally leaves those vectors empty while its geometry handoff remains
// pending for the native collision resolver.
struct NativeGameplayCollisionRetryUpdate {
    std::uint64_t frame_number = 0U;
    CollisionRetryProbeResult retry;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool completes_frame() const noexcept
    {
        return retry.disposition == CollisionRetryProbeDisposition::clear;
    }

    [[nodiscard]] bool operator==(const NativeGameplayCollisionRetryUpdate&) const = default;
};

// One transition through the playfield-owned CDF8 placement loop. A clear
// candidate becomes the ball position for CB7E's rearmed C6/C7 substep, but
// does not itself release actor/audio work. A scan is present only when this
// update ran the native CED2/CD74 asset boundary itself.
struct NativeGameplayCollisionRetryPlacementUpdate {
    std::uint64_t frame_number = 0U;
    std::optional<CollisionRetryFullScanResult> scan;
    CollisionRetryPlacementUpdate placement;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool completes_frame() const noexcept
    {
        return false;
    }

    [[nodiscard]] bool operator==(const NativeGameplayCollisionRetryPlacementUpdate&) const
        = default;
};

// The same-frame result after CDF8's clear arm has rearmed C6/C7 at the
// accepted placement. Actor/audio work appears only when DD09 completes.
struct NativeGameplayCollisionRetryPlacementResume {
    std::uint64_t frame_number = 0U;
    NativeGameplayState::CollisionRetryPlacementLoopResume physics;
    std::vector<NativeGameplayState::ActorPhaseUpdate> actor_updates;
    std::vector<GameplayAudioEvent> audio_events;

    [[nodiscard]] bool completes_frame() const noexcept
    {
        return physics.completes_frame();
    }

    [[nodiscard]] bool operator==(const NativeGameplayCollisionRetryPlacementResume&) const
        = default;
};

// Owns the native gameplay state after a single validated bootstrap. The
// runner advances recovered clear physics and actor phases together. A solid
// contact remains with the existing collision resolver; presentation, audio
// synthesis, and scene flow also remain with their respective owners until
// those domains become native frame participants. Rejecting a second bootstrap
// prevents a live frame loop from silently replacing state ownership.
class NativeGameplayRuntime {
public:
    [[nodiscard]] bool bootstrap(const NativeGameplayBootstrap& state) noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] std::uint64_t frame_number() const noexcept;
    [[nodiscard]] BallFrameSchedule ball_frame_schedule() const noexcept;

    [[nodiscard]] const NativeGameplayState* gameplay() const noexcept;
    [[nodiscard]] NativeGameplayState* gameplay() noexcept;

    // Advances one native physics epoch. On a clear physics result this
    // immediately advances the actor scheduler and drains the ordered audio
    // events. On a solid contact it returns that pending handoff without
    // advancing actors, so a future collision resolver cannot be bypassed.
    // The caller must establish an actual DC95 epoch boundary; an emulator
    // frame boundary alone is not sufficient.
    [[nodiscard]] std::optional<NativeGameplayFrame> advance_physics_epoch(
        const BallMotionTuning& tuning,
        const CollisionMap& map,
        PlayfieldBounds playfield = {});

    // Compatibility spelling retained while callers migrate to the explicit
    // physics-epoch contract above.
    [[nodiscard]] std::optional<NativeGameplayFrame> advance_frame(
        const BallMotionTuning& tuning,
        const CollisionMap& map,
        PlayfieldBounds playfield = {});

    // Runs only a captured C6C2 initial owner. A clear completion installs
    // the typed C721 schedule or retains the initial owner according to its
    // post-DD09 gate; a pending contact leaves the current schedule intact.
    // The high-side return completes C520/C606's slot-zero no-op filing, but
    // this does not yet advance the wider actor scan or presentation frame.
    [[nodiscard]] std::optional<NativeInitialBallMotionUpdate>
    advance_initial_ball_motion(
        const BallMotionTuning& tuning,
        const CollisionMap& map);

    // Compatibility completion for a pending contact whose owner has already
    // completed the C6/C7 loop. This does not increment `frame_number()`.
    // CF33's separating and response-ready outcomes must not call this path:
    // the retained native collision-loop cursor owns their same-frame resume.
    [[nodiscard]] std::optional<NativeGameplayCollisionFrame>
    complete_playfield_collision(
        BallState resolved_ball,
        PlayfieldBounds playfield = {});

    // Runs the typed CF33 clear outcome through the retained C6/C7 cursor.
    // It preserves the current frame number, keeps CDF8 signed selectors
    // pending, and advances actor/audio work only after DD09 completes.
    [[nodiscard]] std::optional<NativeGameplayCf33PlayfieldUpdate>
    resume_cf33_playfield_contact(
        const MappedRomView& rom,
        const GameStateView& state,
        const BallMotionTuning& tuning,
        const CollisionMap& map,
        PlayfieldBounds playfield = {});

    // Applies the recovered CEB5 retry to a pending contact. A clear retry
    // completes the existing frame with native actor order and audio; a
    // full-scan retry remains pending, without inventing a response for its
    // still-unrecovered geometry branch. Like direct contact completion, this
    // never increments `frame_number()`.
    [[nodiscard]] std::optional<NativeGameplayCollisionRetryUpdate>
    retry_pending_playfield_collision(
        const CollisionRetryOffsets& offsets,
        PlayfieldBounds playfield = {});

    // Begins the recovered playfield CDF8 placement loop for a pending marked
    // contact. The candidate may require a later CED2/CD74 scan or a
    // caller-supplied RNG byte. A clear candidate rearms the original C6/C7
    // substep; it does not complete the already-numbered frame.
    [[nodiscard]] std::optional<NativeGameplayCollisionRetryPlacementUpdate>
    begin_pending_collision_retry(
        const CollisionRetryOffsetAttempts& offsets,
        PlayfieldBounds playfield = {});

    // Applies a typed CED2/CD74 outcome to CDF8's current candidate. This is
    // useful while the full collision-map owner is being joined to the runtime;
    // unresolved and solid outcomes stay pending in the same frame.
    [[nodiscard]] std::optional<NativeGameplayCollisionRetryPlacementUpdate>
    resolve_pending_collision_retry_full_scan(
        CollisionRetryFullScanOutcome outcome,
        PlayfieldBounds playfield = {});

    // Runs the recovered CED2 actor-first/CD74 playfield scan against native
    // ROM and work-RAM views, retaining its typed scan result in the update.
    [[nodiscard]] std::optional<NativeGameplayCollisionRetryPlacementUpdate>
    resolve_pending_collision_retry_full_scan(
        const MappedRomView& rom,
        const GameStateView& state,
        std::uint8_t parent_vertical_high_byte,
        PlayfieldBounds playfield = {});

    // Supplies CDF8's shared RNG byte when its current direction is exhausted.
    // Repeated directions and new probes remain pending; a clear candidate
    // finishes actor/audio work in the original frame.
    [[nodiscard]] std::optional<NativeGameplayCollisionRetryPlacementUpdate>
    select_pending_collision_retry_direction(
        std::uint8_t random_byte,
        PlayfieldBounds playfield = {});

    // Continues a clear CDF8 probe through the original C6/C7 cursor. This
    // does not increment the candidate frame number.
    [[nodiscard]] std::optional<NativeGameplayCollisionRetryPlacementResume>
    resume_rearmed_collision_retry(
        const BallMotionTuning& tuning,
        const CollisionMap& map,
        PlayfieldBounds playfield = {});

    // Narrow actor-only seam retained for testing an individual recovered
    // actor phase while the complete native frame owner is still growing. It
    // rejects a pending playfield contact so this temporary entry point cannot
    // bypass the collision-resolution boundary.
    [[nodiscard]] std::optional<NativeGameplayActorFrame> advance_actor_frame(
        PlayfieldBounds playfield = {});

private:
    [[nodiscard]] NativeGameplayCollisionRetryPlacementUpdate
    finish_collision_retry_placement(
        CollisionRetryPlacementUpdate placement,
        std::optional<CollisionRetryFullScanResult> scan,
        PlayfieldBounds playfield);

    std::optional<NativeGameplayState> gameplay_;
    BallFrameSchedule ball_frame_schedule_;
    std::uint64_t frame_number_ = 0U;
};

} // namespace devils_crush::native_runtime
