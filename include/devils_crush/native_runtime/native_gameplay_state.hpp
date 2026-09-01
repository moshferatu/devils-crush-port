// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/actor_collision_scan.hpp"
#include "devils_crush/native_runtime/ball_capture.hpp"
#include "devils_crush/native_runtime/ball_frame_schedule.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_scheduler.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_linked_target_scheduler.hpp"
#include "devils_crush/native_runtime/bumper_scheduler.hpp"
#include "devils_crush/native_runtime/collision_retry_placement.hpp"
#include "devils_crush/native_runtime/collision_retry_probe.hpp"
#include "devils_crush/native_runtime/collision_response.hpp"
#include "devils_crush/native_runtime/collision_response_asset.hpp"
#include "devils_crush/native_runtime/dragon_egg_scheduler.hpp"
#include "devils_crush/native_runtime/dragon_head_scheduler.hpp"
#include "devils_crush/native_runtime/front_spider_scheduler.hpp"
#include "devils_crush/native_runtime/giant_face_helmet_scheduler.hpp"
#include "devils_crush/native_runtime/giant_face_scheduler.hpp"
#include "devils_crush/native_runtime/giant_skeleton_scheduler.hpp"
#include "devils_crush/native_runtime/giant_skeleton_eyeball_scheduler.hpp"
#include "devils_crush/native_runtime/physics_state.hpp"
#include "devils_crush/native_runtime/side_spider_scheduler.hpp"
#include "devils_crush/native_runtime/skeleton_insect_scheduler.hpp"
#include "devils_crush/native_runtime/small_soldier_capture.hpp"
#include "devils_crush/native_runtime/small_soldier_scheduler.hpp"
#include "devils_crush/native_runtime/tower_guard.hpp"
#include "devils_crush/native_runtime/tower_insect_scheduler.hpp"
#include "devils_crush/native_runtime/tower_scheduler.hpp"

#include <cstddef>
#include <vector>

namespace devils_crush::native_runtime {

// Identifies the recovered gameplay rule that submitted a native audio
// command. Playback timing and PSG synthesis remain separate owners; this
// event stream preserves the semantic command and source ordering needed by
// that future scheduler.
enum class GameplayAudioSource {
    bonus_stage_4,
    bumper,
    dragon_egg,
    dragon_egg_hatchling,
    dragon_head,
    giant_face,
    giant_face_helmet,
    giant_skeleton,
    front_spider,
    side_spider,
    tower,
    tower_bonus_stage_selection,
    tower_insect,
    small_soldier,
};

struct GameplayAudioEvent {
    std::uint8_t slot = 0U;
    GameplayAudioSource source = GameplayAudioSource::bumper;
    std::uint8_t command = 0U;

    [[nodiscard]] bool operator==(const GameplayAudioEvent&) const = default;
};

// The native owner for the gameplay domains that now have recovered behavior.
// It is intentionally independent of the emulator transcript and is the seed
// of the eventual native table-frame runtime, rather than a second executable.
class NativeGameplayState {
public:
    NativeGameplayState() noexcept;
    NativeGameplayState(const NativeGameplayState&) = delete;
    NativeGameplayState& operator=(const NativeGameplayState&) = delete;
    NativeGameplayState(NativeGameplayState&&) = delete;
    NativeGameplayState& operator=(NativeGameplayState&&) = delete;

    [[nodiscard]] PhysicsState& physics() noexcept;
    [[nodiscard]] const PhysicsState& physics() const noexcept;
    [[nodiscard]] ActorState& actors() noexcept;
    [[nodiscard]] const ActorState& actors() const noexcept;
    [[nodiscard]] ScoreState& score() noexcept;
    [[nodiscard]] const ScoreState& score() const noexcept;
    [[nodiscard]] TableRuleLatches& table_rule_latches() noexcept;
    [[nodiscard]] const TableRuleLatches& table_rule_latches() const noexcept;
    void seed_table_rule_latches(TableRuleLatches latches) noexcept;

    // Moves all nonzero gameplay commands emitted since the prior drain into
    // the caller's native audio scheduler. This has no device-write side
    // effects and deliberately does not synthesize audio itself.
    [[nodiscard]] std::vector<GameplayAudioEvent> take_audio_events();

    // Establishes a freshly captured or newly served ball as native physics
    // state. Contact bookkeeping and any unresolved playfield impact belong
    // to the prior ball lifetime and are cleared as one transaction.
    void seed_ball(BallState ball) noexcept;

    // Establishes the active player's captured score as the native scoring
    // authority. The Small-Soldier scheduler retains a reference to this
    // score member, so reseeding updates the same owned state.
    void seed_score(ScoreState score) noexcept;

    // Restores one Bumper's recovered fixed-target state. This is an adapter
    // seam for capture startup; normal native contacts advance it in slot order.
    [[nodiscard]] bool seed_bumper(BumperState state) noexcept;
    [[nodiscard]] const BumperState* bumper(std::uint8_t side) const noexcept;

    [[nodiscard]] bool seed_front_spider(FrontSpiderState state) noexcept;
    [[nodiscard]] const FrontSpiderState* front_spider(
        std::uint8_t side) const noexcept;
    [[nodiscard]] bool seed_side_spider(SideSpiderState state) noexcept;
    [[nodiscard]] const SideSpiderState* side_spider(
        std::uint8_t side) const noexcept;
    [[nodiscard]] bool seed_skeleton_insect(SkeletonInsectState state) noexcept;
    [[nodiscard]] const SkeletonInsectState* skeleton_insect(
        SkeletonInsectKind kind) const noexcept;
    void seed_dragon_head(DragonHeadState state) noexcept;
    [[nodiscard]] const DragonHeadState& dragon_head() const noexcept;
    void seed_giant_face(GiantFaceState state) noexcept;
    void seed_giant_face(
        GiantFaceState state,
        GiantFaceControllerState controller,
        GiantFaceAnimatorState animator) noexcept;
    [[nodiscard]] const GiantFaceState& giant_face() const noexcept;
    [[nodiscard]] const GiantFaceControllerState& giant_face_controller() const noexcept;
    [[nodiscard]] const GiantFaceAnimatorState& giant_face_animator() const noexcept;
    void seed_giant_face_helmet(GiantFaceHelmetState state) noexcept;
    [[nodiscard]] const GiantFaceHelmetState* giant_face_helmet() const noexcept;
    // The six fixed eggs and their controller are present only after fixed
    // table setup or capture establishes the cluster.
    void seed_dragon_egg_cluster(DragonEggClusterState state) noexcept;
    [[nodiscard]] const DragonEggClusterState* dragon_egg_cluster() const noexcept;
    // The Giant Skeleton changes its directional appearance every actor turn,
    // so it is explicitly present only after fixed-table setup or capture has
    // established its state.
    void seed_giant_skeleton(GiantSkeletonState state) noexcept;
    [[nodiscard]] const GiantSkeletonState* giant_skeleton() const noexcept;
    // The fixed Eyeball is a separate actor which observes the Dragon's-Egg
    // completion signal after the slot-$1F controller has run.
    void seed_giant_skeleton_eyeball(GiantSkeletonEyeballState state) noexcept;
    [[nodiscard]] const GiantSkeletonEyeballState* giant_skeleton_eyeball()
        const noexcept;
    void seed_tower(TowerState state) noexcept;
    [[nodiscard]] const TowerState& tower() const noexcept;
    // The Guard has no independent progression. A coherent fixed-record
    // capture only establishes that its selector belongs in actor order.
    void seed_tower_guard() noexcept;
    // The fixed Guard's every-update selector derives directly from the
    // Tower-owned gate. Its separate ball-response route remains downstream.
    [[nodiscard]] std::optional<TowerGuardSelectorStep>
    advance_tower_guard_actor(std::uint8_t slot) const noexcept;

    // Advances the recovered native ball prefix against the playfield. A clear
    // frame commits its damped state; a solid contact becomes a pending handoff
    // for the later normal/response and table-rule stages. A second physics
    // frame cannot skip that resolution.
    [[nodiscard]] std::optional<PhysicsPlayfieldFrame>
    advance_physics(const BallMotionTuning& tuning, const CollisionMap& map) noexcept;

    // Resumes an already-live C6/C7 loop from a captured DC95 cursor. This is
    // intentionally separate from `advance_physics`, which begins a fresh
    // DC95 split. A completed suffix owns DD09; a solid probe becomes the
    // usual pending CF33/CDF8 boundary.
    struct PreparedBallCollisionLoopResume {
        std::optional<PlayfieldContact> next_contact;
        std::optional<BallDampingStep> damping;
        bool attempt_cap_exhausted = false;

        [[nodiscard]] bool completes_frame() const noexcept
        {
            return damping.has_value();
        }

        [[nodiscard]] bool operator==(const PreparedBallCollisionLoopResume&) const
            = default;
    };

    [[nodiscard]] std::optional<PreparedBallCollisionLoopResume>
    resume_prepared_ball_collision_loop(
        BallSubstepProgress progress,
        const BallMotionTuning& tuning,
        const CollisionMap& map) noexcept;

    // Runs the readable C6C2 suffix after its banking-only prologue. On a
    // clear completion the source applies DD09, makes a fresh DC95 split, and
    // only then takes C708's horizontal gate. The result keeps that ordering
    // explicit; installing C721 or returning to the caller remains the actor
    // scheduler adapter's separate responsibility.
    struct InitialBallMotionHandoff {
        PreparedBallCollisionLoopResume physics;
        std::optional<BallInitialTransition> transition;
        std::optional<BallMotionSubsteps> next_substeps;
        std::optional<BallSubstepProgress> next_substep_progress;

        [[nodiscard]] bool completes_initial_pass() const noexcept
        {
            return physics.completes_frame() && transition.has_value()
                && next_substeps.has_value() && next_substep_progress.has_value();
        }

        [[nodiscard]] bool operator==(const InitialBallMotionHandoff&) const
            = default;
    };

    [[nodiscard]] std::optional<InitialBallMotionHandoff>
    resume_initial_ball_motion(
        BallSubstepProgress progress,
        const BallMotionTuning& tuning,
        const CollisionMap& map) noexcept;

    // The capture-side scheduler may resume the already-live C721
    // normal-motion owner or the readable C6C2 suffix. C6C2 returns its
    // post-loop scheduler intent instead of mutating a scheduler adapter;
    // every other non-physics owner remains deferred. This is deliberately an
    // explicit dispatch seam rather than a bootstrap side effect: a host
    // frame can cut through this coroutine.
    enum class ScheduledBallMotionDisposition : std::uint8_t {
        resumed,
        deferred,
        unavailable,
    };

    struct ScheduledBallMotionResume {
        ScheduledBallMotionDisposition disposition =
            ScheduledBallMotionDisposition::unavailable;
        std::optional<PreparedBallCollisionLoopResume> physics;
        std::optional<InitialBallMotionHandoff> initial;

        [[nodiscard]] bool operator==(const ScheduledBallMotionResume&) const
            = default;
    };

    [[nodiscard]] ScheduledBallMotionResume resume_scheduled_ball_motion(
        const BallFrameSchedule& schedule,
        const BallMotionTuning& tuning,
        const CollisionMap& map) noexcept;

    [[nodiscard]] const std::optional<PlayfieldContact>&
    pending_playfield_contact() const noexcept;
    // The retained C6/C7 cursor belongs to the same pending contact. It is
    // exposed only as a typed resume boundary for the CF33/CDF8 owner; generic
    // contact completion deliberately clears it instead of fabricating a loop
    // continuation.
    [[nodiscard]] const std::optional<BallCollisionLoop>&
    pending_playfield_collision_loop() const noexcept;

    // Commits the ball state chosen by a recovered collision resolver. The
    // resolver owns normal selection, rewinding, and response mode; this owner
    // enforces that the resulting state answers one pending solid contact.
    [[nodiscard]] bool commit_playfield_collision(BallState resolved_ball) noexcept;

    // Applies the recovered CDF8 retry only after an owning collision resolver
    // has selected that branch for the pending contact. A proven clear probe
    // becomes the new ball state. A full-scan probe intentionally leaves the
    // contact pending, so the still-unrecovered geometry stage cannot be
    // skipped or substituted with a generic response.
    [[nodiscard]] std::optional<CollisionRetryProbeResult>
    retry_pending_playfield_collision(
        const CollisionRetryOffsets& offsets) noexcept;

    // A retry that reaches the recovered full-scan handoff remains pending
    // until its actor/playfield resolver supplies a collision result through
    // `commit_playfield_collision`. Returning the stored probe prevents a
    // second caller from reapplying the retry offset while geometry is pending.
    [[nodiscard]] const std::optional<CollisionRetryProbeResult>&
    pending_collision_retry_full_scan() const noexcept;

    // Begins the playfield-owned CDF8 placement loop after CF33's marked
    // signed handoff. It rejects a pending contact whose collision marker is
    // clear. Its current candidate is evaluated through the existing
    // CEB5 short probe; a full scan remains pending until the native CED2/CD74
    // boundary supplies its typed outcome. A clear candidate restores the
    // cached pre-impact ball and rearms the unconsumed C6/C7 substep rather
    // than committing the candidate as a new ball position.
    [[nodiscard]] std::optional<CollisionRetryPlacementUpdate>
    begin_pending_collision_retry(
        const CollisionRetryOffsetAttempts& offsets) noexcept;

    // Resolves the current full-scan retry candidate. A solid outcome advances
    // only the native placement loop, while clear accepts its candidate and an
    // unresolved outcome remains pending. No target-contact response is
    // selected at this boundary.
    [[nodiscard]] std::optional<CollisionRetryPlacementUpdate>
    resolve_pending_collision_retry_full_scan(
        CollisionRetryFullScanOutcome outcome) noexcept;

    // Runs the recovered CED2 actor-first and CD74 playfield scan for the
    // pending candidate, then advances CDF8 from that scan's carry-like
    // result. The mapped assets and captured table state remain an explicit
    // adapter boundary; the returned scan retains target identity for the
    // later contact-response owner instead of collapsing it into a boolean.
    struct CollisionRetryFullScanUpdate {
        std::optional<CollisionRetryFullScanResult> scan;
        CollisionRetryPlacementUpdate placement;

        [[nodiscard]] bool operator==(
            const CollisionRetryFullScanUpdate&) const = default;
    };

    [[nodiscard]] std::optional<CollisionRetryFullScanUpdate>
    resolve_pending_collision_retry_full_scan(
        const MappedRomView& rom,
        const GameStateView& state,
        std::uint8_t parent_vertical_high_byte);

    // Supplies the caller-owned RNG byte when the recovered CDF8 loop has
    // exhausted one direction's three candidates. A duplicate direction keeps
    // the placement state pending for another RNG byte.
    [[nodiscard]] std::optional<CollisionRetryPlacementUpdate>
    select_pending_collision_retry_direction(std::uint8_t random_byte) noexcept;

    [[nodiscard]] const std::optional<CollisionRetryPlacementState>&
    pending_collision_retry_placement() const noexcept;

    // Starts CDF8 from a confirmed actor contact rather than a playfield
    // impact. Its independent pending state is seeded from CF33's typed
    // impact/cache pair and never aliases `pending_playfield_contact()`.
    [[nodiscard]] std::optional<CollisionRetryPlacementUpdate>
    begin_actor_contact_collision_retry(
        const CollisionRetryBallCapture& capture,
        const CollisionRetryOffsetAttempts& offsets) noexcept;
    [[nodiscard]] std::optional<CollisionRetryPlacementUpdate>
    resolve_pending_actor_contact_collision_retry_full_scan(
        CollisionRetryFullScanOutcome outcome) noexcept;
    [[nodiscard]] std::optional<CollisionRetryFullScanUpdate>
    resolve_pending_actor_contact_collision_retry_full_scan(
        const MappedRomView& rom,
        const GameStateView& state);
    [[nodiscard]] std::optional<CollisionRetryPlacementUpdate>
    select_pending_actor_contact_collision_retry_direction(
        std::uint8_t random_byte) noexcept;
    [[nodiscard]] const std::optional<CollisionRetryProbeResult>&
    pending_actor_contact_collision_retry_full_scan() const noexcept;
    [[nodiscard]] const std::optional<CollisionRetryPlacementState>&
    pending_actor_contact_collision_retry_placement() const noexcept;

    // Commits native ball-contact bookkeeping before selecting any recovered
    // target rule. Dispatched Bumper and Small-Soldier targets are marked for
    // their later actor-order phase; other behaviors retain their handoff.
    struct ActorContactUpdate {
        ContactDispatch contact;
        bool bumper_queued = false;
        bool bonus_stage_4_queued = false;
        bool bonus_stage_4_linked_target_queued = false;
        bool bonus_stage_4_orbiting_child_queued = false;
        bool dragon_egg_queued = false;
        bool dragon_head_queued = false;
        bool giant_face_queued = false;
        bool giant_face_helmet_queued = false;
        bool giant_skeleton_queued = false;
        bool front_spider_queued = false;
        bool side_spider_queued = false;
        bool tower_queued = false;
        bool tower_insect_queued = false;
        bool small_soldier_queued = false;

        [[nodiscard]] bool operator==(const ActorContactUpdate&) const = default;
    };

    // A target geometry owner uses this entry after it has selected CF33's
    // contact basis and response family. It applies the recovered ball-vector
    // response only for a newly dispatched contact, then queues the ordinary
    // target rule in the same transaction. Callers that do not own such a
    // geometry decision continue using confirm_actor_contact unchanged.
    struct OrientedActorContactUpdate {
        ActorContactUpdate contact;
        std::optional<OrientedCollisionResponse> response;

        [[nodiscard]] bool operator==(
            const OrientedActorContactUpdate&) const = default;
    };

    // CF33 can either apply a response, return from a separating clear leg,
    // or transfer a signed marked contact into CDF8's position-retry loop.
    // The retry handoff deliberately carries its own captured ball pair: it
    // must not be mistaken for the playfield-only pending-contact state.
    enum class Cf33ActorContactDisposition : std::uint8_t {
        rejected,
        unavailable,
        separating,
        contact_not_dispatched,
        response_applied,
        position_retry_ready,
    };

    struct Cf33ActorContactResolution {
        Cf33ActorContactDisposition disposition =
            Cf33ActorContactDisposition::rejected;
        std::optional<OrientedActorContactUpdate> contact;
        std::optional<CollisionRetryBallCapture> retry_ball;

        [[nodiscard]] bool operator==(
            const Cf33ActorContactResolution&) const = default;
    };

    // The playfield reaches CF33 only after CD74 has returned a solid tile.
    // Its clear selector either applies CF5B's oriented response or takes the
    // separating return. A signed selector reaches CDF8 only while the
    // captured ball collision marker is set; otherwise it follows CF3F's
    // ordinary reflected response. This evaluates that exact decision against
    // the pending impact without committing a ball or advancing its frame.
    enum class Cf33PlayfieldContactDisposition : std::uint8_t {
        rejected,
        unavailable,
        signed_retry_required,
        separating,
        response_ready,
    };

    struct Cf33PlayfieldContactResolution {
        Cf33PlayfieldContactDisposition disposition =
            Cf33PlayfieldContactDisposition::rejected;
        BallState resolved_ball;
        std::optional<OrientedCollisionResponse> response;

        [[nodiscard]] bool operator==(
            const Cf33PlayfieldContactResolution&) const = default;
    };

    // The resolved CF33 result joined to the retained C6/C7 cursor. A new
    // contact is deliberately returned as another boundary; only DD09's
    // completed-frame result may release actor/audio work.
    struct Cf33PlayfieldLoopResume {
        Cf33PlayfieldContactResolution response;
        std::optional<PlayfieldContact> next_contact;
        std::optional<BallDampingStep> damping;
        bool attempt_cap_exhausted = false;

        [[nodiscard]] bool completes_frame() const noexcept
        {
            return damping.has_value();
        }

        [[nodiscard]] bool operator==(const Cf33PlayfieldLoopResume&) const
            = default;
    };

    // The same C6/C7 continuation after CDF8 has accepted a clear placement.
    // CDF8 restores its cache internally before applying the selected offset;
    // CB7E then clears the marker and resumes the unconsumed substep from the
    // accepted position.
    struct CollisionRetryPlacementLoopResume {
        std::optional<PlayfieldContact> next_contact;
        std::optional<BallDampingStep> damping;
        bool attempt_cap_exhausted = false;

        [[nodiscard]] bool completes_frame() const noexcept
        {
            return damping.has_value();
        }

        [[nodiscard]] bool operator==(const CollisionRetryPlacementLoopResume&) const
            = default;
    };

    [[nodiscard]] ActorContactUpdate confirm_actor_contact(
        ContactTarget target) noexcept;
    [[nodiscard]] OrientedActorContactUpdate confirm_oriented_actor_contact(
        ContactTarget target,
        CollisionResponseBasis basis,
        const CollisionResponseConfig& config,
        CollisionNormalReflection reflection =
            CollisionNormalReflection::before_response) noexcept;
    // Asset-bound form of CF33's signed-selector response leg. It consumes the
    // shared response input captured at the contact boundary, decodes the
    // selected D081 basis and CFD8 configuration, then applies the same
    // one-shot contact transaction. The separate clear-selector CF5B leg,
    // unmapped ROM, or incomplete captured state leaves gameplay unchanged.
    [[nodiscard]] std::optional<OrientedActorContactUpdate>
    confirm_oriented_actor_contact(
        ContactTarget target,
        const MappedRomView& rom,
        const GameStateView& state) noexcept;
    // CF33's typed entrypoint accepts the captured overlapping actor instead
    // of a caller-supplied response kind. It admits CBCF type `$82`, derives
    // the target from that active actor, and selects the recovered signed or
    // clear CF33 response leg. Retry/clear-return cases remain unchanged for
    // their respective source owners.
    [[nodiscard]] std::optional<OrientedActorContactUpdate>
    confirm_cf33_actor_contact(
        const GameStateView::ActorSlot& actor,
        const MappedRomView& rom,
        const GameStateView& state) noexcept;
    // Complete typed decision for CF33's captured actor contact. Unlike the
    // compatibility response-only entrypoint above, this retains CDF8's
    // signed-marked branch as an explicit retry handoff with F1B4's cached
    // pre-integration position. It performs no retry placement itself.
    [[nodiscard]] Cf33ActorContactResolution resolve_cf33_actor_contact(
        const GameStateView::ActorSlot& actor,
        const MappedRomView& rom,
        const GameStateView& state) noexcept;
    [[nodiscard]] Cf33PlayfieldContactResolution
    resolve_cf33_playfield_contact(
        const MappedRomView& rom,
        const GameStateView& state) const noexcept;
    // Applies CF33's proven clear outcomes to the retained collision loop.
    // CF8F retries the current substep; CF5B's non-separating response
    // consumes it, re-splits through DC95, and continues until either another
    // contact or DD09. A signed selector with the collision marker set remains
    // pending for CDF8.
    [[nodiscard]] std::optional<Cf33PlayfieldLoopResume>
    resume_cf33_playfield_contact(
        const MappedRomView& rom,
        const GameStateView& state,
        const BallMotionTuning& tuning,
        const CollisionMap& map) noexcept;

    // Runs CDF8's rearmed clear arm through the retained C6/C7 cursor. It
    // preserves the already-numbered frame and may produce another playfield
    // contact before DD09 completes the frame.
    [[nodiscard]] std::optional<CollisionRetryPlacementLoopResume>
    resume_rearmed_collision_retry(
        const BallMotionTuning& tuning,
        const CollisionMap& map) noexcept;

    // These are explicit actor-order and animation phases, matching the
    // recovered Small-Soldier timing rather than resolving a rule in the
    // collision dispatcher.
    [[nodiscard]] std::optional<BumperRulesUpdate>
    advance_bumper_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<DragonHeadRulesUpdate>
    advance_dragon_head_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_dragon_head_hit_presentation(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceRulesUpdate>
    advance_giant_face_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_giant_face_impact_presentation(
        std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_giant_face_mouth_opening() noexcept;
    [[nodiscard]] bool request_giant_face_transformation() noexcept;
    [[nodiscard]] bool begin_giant_face_animator(GiantFaceAnimatorMode mode) noexcept;
    [[nodiscard]] std::optional<GiantFaceControllerStep>
    advance_giant_face_controller_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceAnimatorStep>
    advance_giant_face_animator_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceHelmetRulesUpdate>
    advance_giant_face_helmet_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceHelmetCompletionStep>
    complete_giant_face_helmet_activation_presentation(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_giant_face_helmet_ball_return(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<DragonEggRulesUpdate>
    advance_dragon_egg_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<DragonEggCompletionRulesUpdate>
    advance_dragon_egg_controller_actor(std::uint8_t slot) noexcept;
    // The table/presentation reset owner supplies its completed timing signal
    // here; the six fixed eggs then reset on their own later actor turns.
    [[nodiscard]] std::optional<DragonEggGroupResetStep>
    begin_dragon_egg_group_reset() noexcept;
    [[nodiscard]] std::optional<DragonEggFixedResetStep>
    advance_dragon_egg_fixed_reset_actor(std::uint8_t slot) noexcept;
    // Claims the allocator-selected dynamic record and commits one fixed
    // egg's already-emitted hatchling request. Its future flight behavior is
    // kept separate from the recovered spawn and contact lifecycle.
    [[nodiscard]] std::optional<DragonEggHatchlingAllocationStep>
    allocate_dragon_egg_hatchling(
        std::uint8_t source_egg_slot,
        std::uint8_t dynamic_slot) noexcept;
    // Claims a captured dynamic record without replaying the fixed egg's
    // allocation event.
    [[nodiscard]] bool seed_dragon_egg_hatchling(
        DragonEggHatchlingState hatchling) noexcept;
    [[nodiscard]] const DragonEggHatchlingState* dragon_egg_hatchling(
        std::uint8_t dynamic_slot) const noexcept;
    [[nodiscard]] std::optional<DragonEggHatchlingRulesUpdate>
    advance_dragon_egg_hatchling_actor(std::uint8_t dynamic_slot) noexcept;
    [[nodiscard]] std::optional<GiantSkeletonRulesUpdate>
    advance_giant_skeleton_actor(
        std::uint8_t slot,
        GiantSkeletonTargetPosition target) noexcept;
    [[nodiscard]] std::optional<GiantSkeletonEyeballStep>
    advance_giant_skeleton_eyeball_actor(std::uint8_t slot) noexcept;
    // Called after the opening presentation/setup owner finishes the source's
    // immediate work and hands its verified eight-tick wait back to gameplay.
    [[nodiscard]] std::optional<GiantSkeletonEyeballOpeningStartStep>
    begin_giant_skeleton_eyeball_opening_countdown() noexcept;
    // The scene/presentation owner supplies the source's secondary-mode
    // selection after the opening expiry. The ensuing final mouth wait is
    // native and advances at the fixed Eyeball actor turn.
    [[nodiscard]] std::optional<GiantSkeletonEyeballFinalStartStep>
    begin_giant_skeleton_eyeball_final_animation(bool extended_wait) noexcept;
    [[nodiscard]] std::optional<GiantSkeletonEyeballFinalResetStep>
    complete_giant_skeleton_eyeball_final_reset() noexcept;
    [[nodiscard]] std::optional<FrontSpiderRulesUpdate>
    advance_front_spider_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_front_spider_transition(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<SideSpiderRulesUpdate>
    advance_side_spider_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_side_spider_transition(
        std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_skeleton_insect_transition(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<TowerPresentationStep>
    advance_tower_presentation(std::uint8_t slot) noexcept;
    // Called by the collision owner while 03:6136 is installed. A false
    // latch is the routine's scheduled no-hit update; true is a real
    // geometry-delivered sign-bit collision.
    [[nodiscard]] std::optional<TowerBonusStageSelectionStep>
    advance_tower_bonus_stage_selection(
        std::uint8_t slot,
        bool negative_collision_latched) noexcept;
    // The source's recovered coroutine determines when these explicit spawn
    // requests occur. The native rule owns the four-child budget thereafter.
    [[nodiscard]] std::optional<TowerInsectSpawnStep>
    spawn_tower_insect_child(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<TowerInsectSourceStep>
    begin_tower_insect_generation(bool tower_insect_cycle_requested) noexcept;
    [[nodiscard]] bool seed_tower_insect_generation(
        TowerInsectGenerationState generation) noexcept;
    [[nodiscard]] std::optional<TowerInsectRulesUpdate>
    advance_tower_insect_actor(std::uint8_t slot) noexcept;
    struct TowerInsectContactWithEffectUpdate {
        TowerInsectRulesUpdate rules;
        // Present whenever the rule requested 03:7BF6. An allocator rejection
        // remains an observed result rather than suppressing the contact.
        std::optional<ActorCollisionEffectAllocation> collision_effect;

        [[nodiscard]] bool operator==(
            const TowerInsectContactWithEffectUpdate&) const = default;
    };
    // Integrates a rule-emitted shared-effect request with a caller-selected
    // allocator destination. It claims the effect before releasing a terminal
    // Tower insect, matching the source routine's call ordering.
    [[nodiscard]] std::optional<TowerInsectContactWithEffectUpdate>
    advance_tower_insect_actor_with_collision_effect(
        std::uint8_t slot,
        std::uint8_t effect_slot) noexcept;
    [[nodiscard]] std::optional<TowerInsectSourceStep>
    advance_tower_insect_source(std::uint8_t slot) noexcept;
    // The caller supplies the Tower-owned cycle latch after its scene/timer
    // transition has completed; false preserves the completed generation.
    [[nodiscard]] std::optional<TowerInsectSourceStep>
    restart_tower_insect_generation(bool tower_insect_cycle_requested) noexcept;
    [[nodiscard]] const TowerInsectSourceState& tower_insect_source() const noexcept;
    [[nodiscard]] const TowerInsectState* tower_insect(
        std::uint8_t slot) const noexcept;
    // The shared effect allocator runs after a rule emits its named request
    // and before a terminal source slot is released. The caller supplies the
    // allocator-selected child slot; rendering remains a separate owner.
    [[nodiscard]] ActorCollisionEffectAllocation claim_collision_effect(
        std::uint8_t source_slot,
        std::uint8_t effect_slot) noexcept;
    [[nodiscard]] bool seed_collision_effect(
        std::uint8_t effect_slot,
        CollisionEffectState state) noexcept;
    // Restores one collision-ready dynamic Small Soldier without inventing its
    // Side-Star source generation. The captured actor owns the native contact
    // rule; generation bookkeeping remains with its current owner.
    [[nodiscard]] bool seed_captured_small_soldier(
        const SmallSoldierActorCapture& capture) noexcept;
    [[nodiscard]] bool seed_captured_small_soldier_from_source(
        const SmallSoldierActorCapture& capture) noexcept;
    [[nodiscard]] bool seed_small_soldier_source(
        SmallSoldierSourceState state) noexcept;
    [[nodiscard]] std::optional<SmallSoldierSourceStep>
    advance_captured_small_soldier_source(SmallSoldierGroupSide side) noexcept;
    [[nodiscard]] std::optional<SmallSoldierRulesUpdate>
    advance_small_soldier_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<SmallSoldierRulesUpdate>
    complete_small_soldier_animation(std::uint8_t slot) noexcept;

    // BS-4 state is deliberately opt-in because its fixed slot numbers are
    // reused by normal-table actors. Its contacts join the existing native
    // contact transaction and resolve on their captured actor-order turns.
    void seed_bonus_stage_4(BonusStage4State state) noexcept;
    [[nodiscard]] const BonusStage4State* bonus_stage_4() const noexcept;
    [[nodiscard]] std::optional<BonusStage4ObjectiveRulesUpdate>
    advance_bonus_stage_4_actor(std::uint8_t slot) noexcept;

    // A captured linked target is a dynamic BS-4 actor, independent of the
    // three fixed objectives. Its presentation and child-allocation seams
    // remain explicit because their timing is scheduler-owned.
    [[nodiscard]] bool seed_bonus_stage_4_linked_target(
        BonusStage4LinkedTargetState target) noexcept;
    [[nodiscard]] bool seed_bonus_stage_4_linked_target(
        BonusStage4LinkedTargetState target,
        std::array<std::optional<BonusStage4OrbitingChildState>, 4U> children) noexcept;
    [[nodiscard]] const BonusStage4LinkedTargetState* bonus_stage_4_linked_target()
        const noexcept;
    [[nodiscard]] const BonusStage4OrbitingChildState* bonus_stage_4_orbiting_child(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] std::optional<BonusStage4LinkedTargetRulesUpdate>
    advance_bonus_stage_4_linked_target_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<BonusStage4LinkedTargetMotionStep>
    advance_bonus_stage_4_linked_target_motion_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<BonusStage4OrbitingChildFrameUpdate>
    advance_bonus_stage_4_orbiting_child_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<BonusStage4OrbitingChildFrameUpdate>
    advance_bonus_stage_4_orbiting_child_orbit_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<BonusStage4LinkedTargetHitPresentationStep>
    complete_bonus_stage_4_linked_target_hit_presentation() noexcept;
    [[nodiscard]] std::optional<BonusStage4OrbitingChildAllocationStep>
    allocate_bonus_stage_4_orbiting_child(std::uint8_t dynamic_slot) noexcept;

    enum class ActorPhaseUpdateKind {
        shared_motion,
        bonus_stage_4_objective,
        bonus_stage_4_linked_target,
        bonus_stage_4_linked_target_motion,
        bonus_stage_4_orbiting_child,
        bonus_stage_4_orbiting_child_orbit,
        bumper_contact,
        dragon_egg_controller,
        dragon_egg_fixed_reset,
        dragon_egg_contact,
        dragon_egg_hatchling,
        dragon_head_contact,
        giant_face_helmet,
        giant_face_controller,
        giant_face_animator,
        giant_face_contact,
        giant_skeleton_update,
        giant_skeleton_eyeball,
        front_spider_contact,
        side_spider_contact,
        tower_contact,
        tower_guard_selector,
        tower_insect_source,
        tower_insect_contact,
        collision_effect,
        skeleton_insect_trigger,
        small_soldier_contact,
    };

    // One observed actor-order update. The optional payload is selected by
    // `kind`, keeping recovered behavior results in their real slot order.
    struct ActorPhaseUpdate {
        std::uint8_t slot = 0;
        ActorPhaseUpdateKind kind = ActorPhaseUpdateKind::shared_motion;
        std::optional<ActorMotionUpdate> motion;
        std::optional<BonusStage4ObjectiveRulesUpdate> bonus_stage_4;
        std::optional<BonusStage4LinkedTargetRulesUpdate> bonus_stage_4_linked_target;
        std::optional<BonusStage4LinkedTargetMotionStep> bonus_stage_4_linked_target_motion;
        std::optional<BonusStage4OrbitingChildFrameUpdate> bonus_stage_4_orbiting_child;
        std::optional<BumperRulesUpdate> bumper;
        std::optional<DragonEggCompletionRulesUpdate> dragon_egg_controller;
        std::optional<DragonEggFixedResetStep> dragon_egg_fixed_reset;
        std::optional<DragonEggRulesUpdate> dragon_egg;
        std::optional<DragonEggHatchlingRulesUpdate> dragon_egg_hatchling;
        std::optional<DragonHeadRulesUpdate> dragon_head;
        std::optional<GiantFaceHelmetRulesUpdate> giant_face_helmet;
        std::optional<GiantFaceControllerStep> giant_face_controller;
        std::optional<GiantFaceAnimatorStep> giant_face_animator;
        std::optional<GiantFaceRulesUpdate> giant_face;
        std::optional<GiantSkeletonRulesUpdate> giant_skeleton;
        std::optional<GiantSkeletonEyeballStep> giant_skeleton_eyeball;
        std::optional<FrontSpiderRulesUpdate> front_spider;
        std::optional<SideSpiderRulesUpdate> side_spider;
        std::optional<TowerRulesUpdate> tower;
        std::optional<TowerGuardSelectorStep> tower_guard;
        std::optional<TowerInsectSourceStep> tower_insect_source;
        std::optional<TowerInsectRulesUpdate> tower_insect;
        std::optional<ActorCollisionEffectUpdate> collision_effect;
        std::optional<SkeletonInsectRulesUpdate> skeleton_insect;
        std::optional<SmallSoldierRulesUpdate> small_soldier;

        [[nodiscard]] bool operator==(const ActorPhaseUpdate&) const = default;
    };

    // Advances one recovered actor behavior without touching any other slot.
    // This is the typed unit used by the C539 scheduler shadow; it is not a
    // live registry hook.
    [[nodiscard]] std::optional<ActorPhaseUpdate> advance_actor_phase_slot(
        std::uint8_t slot,
        PlayfieldBounds playfield = {});

    // Walks due native actors in slot order. Shared motion advances every due
    // frame; a Small Soldier emits a rule update only when a confirmed contact
    // was queued by the collision phase.
    [[nodiscard]] std::vector<ActorPhaseUpdate> advance_actor_phase(
        PlayfieldBounds playfield = {});

private:
    [[nodiscard]] std::vector<ActorPhaseUpdate> advance_actor_phase_slots(
        std::size_t first_slot,
        std::size_t slot_count,
        PlayfieldBounds playfield);

    PhysicsState physics_;
    ActorState actors_;
    ScoreState score_;
    TableRuleLatches table_rule_latches_;
    BumperRuleScheduler bumper_scheduler_;
    BonusStage4RuleScheduler bonus_stage_4_scheduler_;
    BonusStage4LinkedTargetRuleScheduler bonus_stage_4_linked_target_scheduler_;
    DragonEggRuleScheduler dragon_egg_scheduler_;
    DragonHeadRuleScheduler dragon_head_scheduler_;
    GiantFaceRuleScheduler giant_face_scheduler_;
    GiantFaceHelmetRuleScheduler giant_face_helmet_scheduler_;
    GiantSkeletonRuleScheduler giant_skeleton_scheduler_;
    GiantSkeletonEyeballScheduler giant_skeleton_eyeball_scheduler_;
    FrontSpiderRuleScheduler front_spider_scheduler_;
    SideSpiderRuleScheduler side_spider_scheduler_;
    TowerRuleScheduler tower_scheduler_;
    bool tower_guard_seeded_ = false;
    TowerInsectRuleScheduler tower_insect_scheduler_;
    SkeletonInsectRuleScheduler skeleton_insect_scheduler_;
    SmallSoldierRuleScheduler small_soldier_scheduler_;
    std::optional<PlayfieldContact> pending_playfield_contact_;
    std::optional<BallCollisionLoop> pending_playfield_collision_loop_;
    std::optional<CollisionRetryProbeResult> pending_collision_retry_full_scan_;
    std::optional<CollisionRetryPlacementState> pending_collision_retry_placement_;
    bool pending_collision_retry_rearm_ = false;
    std::optional<CollisionRetryProbeResult>
        pending_actor_contact_collision_retry_full_scan_;
    std::optional<CollisionRetryPlacementState>
        pending_actor_contact_collision_retry_placement_;
    std::vector<GameplayAudioEvent> pending_audio_events_;

    [[nodiscard]] CollisionRetryPlacementUpdate apply_collision_retry_placement(
        CollisionRetryPlacementUpdate update) noexcept;
    [[nodiscard]] CollisionRetryPlacementUpdate
    apply_actor_contact_collision_retry_placement(
        CollisionRetryPlacementUpdate update) noexcept;
    void record_audio_event(
        std::uint8_t slot,
        GameplayAudioSource source,
        std::uint8_t command);
};

} // namespace devils_crush::native_runtime
