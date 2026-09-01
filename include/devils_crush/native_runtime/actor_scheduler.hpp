// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/native_gameplay_state.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace devils_crush::native_runtime {

// C606 files an actor only after its scheduled handler has run. These names
// retain the three separate SAT queues without exposing their work-RAM
// addresses to the native presentation owner.
enum class ActorPresentationQueueKind {
    slot_zero_deferred,
    not_presentation_eligible,
    hidden,
    auxiliary,
    primary_low,
    primary_high,
    capacity_exhausted,
};

// The post-handler queue state consumed by the SAT presentation driver. The
// two primary groups share the original 128-entry capacity; the auxiliary
// group has its own 128-entry storage.
struct ActorPresentationQueueState {
    static constexpr std::size_t kQueueCapacity = 128U;

    std::array<std::uint8_t, kQueueCapacity> auxiliary_slots{};
    std::array<std::uint8_t, kQueueCapacity> primary_low_slots{};
    std::array<std::uint8_t, kQueueCapacity> primary_high_slots{};
    std::size_t auxiliary_count = 0U;
    std::size_t primary_low_count = 0U;
    std::size_t primary_high_count = 0U;

    [[nodiscard]] bool operator==(const ActorPresentationQueueState&) const
        = default;
};

// One uninterrupted execution of C510. The original loop skips ineligible
// records until it reaches the next dispatchable actor or exhausts the table;
// a handler can then change later records before the next scan run begins.
// Keeping that boundary explicit is essential for a future native owner.
struct MainActorScanRun {
    std::vector<GameStateView::ActorSlot> skipped_actors;
    std::optional<GameStateView::ActorSlot> dispatch_actor;

    [[nodiscard]] bool terminal() const noexcept
    {
        return !dispatch_actor.has_value();
    }

    [[nodiscard]] bool operator==(const MainActorScanRun&) const = default;
};

// Decodes C606's RAM-shaped queues into the canonical state used by the
// readable scheduler. The low primary cursor grows upward, while the high
// primary cursor starts at $7F and grows downward; the latter is normalized
// into registration order. Invalid or overlapping cursors remain outside the
// capture boundary rather than being silently repaired.
[[nodiscard]] std::optional<ActorPresentationQueueState>
capture_actor_presentation_queue_state(const GameStateView& state) noexcept;

// The short C539 actor-pass entry resets the sprite-emission queues and the
// completed ball-substep counter before the C510 dispatcher begins. Its $2D35
// gate only changes the shared actor-enable mask; individual slot selection
// and behavior execution remain later scheduler responsibilities.
struct ShortActorPassSeed {
    GameStateView::ActorFrameGate gate;
    ActorPresentationQueueState presentation_queues;
    std::uint8_t completed_ball_substeps = 0U;

    [[nodiscard]] bool operator==(const ShortActorPassSeed&) const = default;
};

// One validated C539 entry observation. Keeping the reset seed and decoded
// due-actor records together prevents a shadow consumer from combining a
// frame gate from one snapshot with a schedule from another.
struct ShortActorPassCapture {
    ShortActorPassSeed seed;
    std::vector<GameStateView::ActorSlot> scheduled_actors;

    [[nodiscard]] bool operator==(const ShortActorPassCapture&) const = default;
};

// One C606-equivalent filing decision. For a nonzero presentation-eligible
// slot, a bit-zero attribute asks the owning presentation-refresh helper to
// run before filing; the caller supplies that prior decision separately from
// the helper's resulting attributes, so this value records the dependency
// without guessing actor-family behavior. Slot zero returns before that test.
struct ActorPresentationQueueStep {
    ActorPresentationQueueState previous;
    ActorPresentationQueueState state;
    std::uint8_t slot = 0U;
    ActorPresentationQueueKind queue =
        ActorPresentationQueueKind::slot_zero_deferred;
    bool requires_prequeue_presentation_refresh = false;

    [[nodiscard]] bool operator==(const ActorPresentationQueueStep&) const
        = default;
};

// The completed handler's presentation-facing result. C606 first requires the
// entry attribute's camera bit (bit seven), samples bit zero before F605
// refreshes transformed presentation coordinates, then chooses the queue from
// the reread attribute value. Keeping both observations makes that sequencing
// a typed scheduler contract instead of asking a behavior family to
// reconstruct it.
struct ActorPostHandlerPresentation {
    std::uint8_t slot = 0U;
    std::uint8_t attributes_before_prequeue_presentation_refresh = 0U;
    std::uint8_t attributes_after_prequeue_presentation_refresh = 0U;

    [[nodiscard]] bool operator==(const ActorPostHandlerPresentation&) const
        = default;
};

// A handler's typed mutation of its own actor record. This deliberately
// describes domains rather than RAM locations, so scheduler evidence can
// select an owning behavior family without preserving an instruction trace.
struct ActorBehaviorStateChanges {
    bool activity_flags = false;
    bool attributes = false;
    bool behavior_counter = false;
    bool handler_address = false;
    bool horizontal_motion = false;
    bool vertical_motion = false;
    bool collision_state = false;

    [[nodiscard]] bool any() const noexcept
    {
        return activity_flags || attributes || behavior_counter
            || handler_address || horizontal_motion || vertical_motion
            || collision_state;
    }

    [[nodiscard]] bool operator==(const ActorBehaviorStateChanges&) const
        = default;
};

// Captures only a same-slot handler transition. The collision group retains
// the coupled collision selectors, table state, and response state as one
// behavior-owned domain; a future family adapter can split it further after
// its own semantics are named.
[[nodiscard]] std::optional<ActorBehaviorStateChanges>
capture_actor_behavior_state_changes(
    const GameStateView::ActorSlot& before,
    const GameStateView::ActorSlot& after) noexcept;

// Owns C510's sequential scan and C606 queue state for one complete main
// actor pass. Unlike the C539 capture model, each scan run is submitted only
// when it is reached: a dispatched handler is allowed to change later actor
// records before the next run is decoded. The owner therefore never treats a
// pass-start snapshot as a fixed schedule.
class MainActorPass {
public:
    explicit MainActorPass(ActorPresentationQueueState presentation_queues) noexcept;

    [[nodiscard]] const ActorPresentationQueueState& presentation_queues()
        const noexcept;
    [[nodiscard]] std::optional<GameStateView::ActorSlot> pending_actor()
        const noexcept;
    [[nodiscard]] bool terminal() const noexcept;

    // Accepts exactly one C510 scan run when no handler is pending. A terminal
    // run marks the pass complete; a dispatch run waits for its matching
    // post-handler C606 filing before another scan may begin.
    [[nodiscard]] bool submit_scan_run(const MainActorScanRun& run) noexcept;

    // Files a handler only when it belongs to the current run's dispatch
    // target. Rejected or duplicate observations leave the pass unchanged.
    [[nodiscard]] std::optional<ActorPresentationQueueStep> complete_handler(
        ActorPostHandlerPresentation actor) noexcept;

private:
    ActorPresentationQueueState presentation_queues_;
    std::optional<GameStateView::ActorSlot> pending_actor_;
    bool terminal_ = false;
};

// Captures the two attribute reads that surround C606's optional F605
// presentation refresh from verified handler-boundary snapshots. This is a
// pure observation adapter: it does not infer an actor family's behavior or
// permit a native C539 pass to advance without an observed post-handler
// boundary.
[[nodiscard]] std::optional<ActorPostHandlerPresentation>
capture_actor_post_handler_presentation(
    const GameStateView& before_handler,
    const GameStateView& after_handler,
    std::uint8_t slot) noexcept;

// A handler-boundary observation that is bound to C539's decoded scheduled
// actor record. The complete pre-handler record is retained so a host capture
// cannot accidentally pair a post-handler attribute change with a reused slot
// from another scheduler pass.
struct CapturedScheduledActorHandlerBoundary {
    GameStateView::ActorSlot scheduled_actor;
    ActorPostHandlerPresentation presentation;

    [[nodiscard]] bool operator==(const CapturedScheduledActorHandlerBoundary&) const
        = default;
};

// Captures a C606-facing handler boundary only when the before snapshot still
// exactly matches the actor record decoded by C539. The after snapshot may
// differ because the handler owns that mutation. This is capture validation
// only; it neither invokes a behavior nor changes a live scheduler pass.
[[nodiscard]] std::optional<CapturedScheduledActorHandlerBoundary>
capture_scheduled_actor_handler_boundary(
    const GameStateView& before_handler,
    const GameStateView& after_handler,
    const GameStateView::ActorSlot& scheduled_actor) noexcept;

// The future native behavior owner receives a decoded actor record, never a
// ROM handler address or CPU continuation. Returning no result leaves the
// short pass at the same actor so an unported family can remain emulator-owned
// during shadow validation.
class ActorPassBehaviorDispatcher {
public:
    virtual ~ActorPassBehaviorDispatcher() = default;

    [[nodiscard]] virtual std::optional<ActorPostHandlerPresentation>
    advance_actor(const GameStateView::ActorSlot& actor) = 0;
};

// A typed C539 behavior observation produced by the recovered native gameplay
// domain. It deliberately carries no presentation result: C606 must still see
// captured post-handler attributes before a future pass can submit a filing.
struct NativeGameplayActorBehaviorAdvance {
    GameStateView::ActorSlot actor;
    NativeGameplayState::ActorPhaseUpdate update;

    [[nodiscard]] bool operator==(const NativeGameplayActorBehaviorAdvance&) const
        = default;
};

// A native behavior update paired with the captured C606-facing state from
// the same handler boundary. It is not a dispatch result yet: the strict join
// only establishes that both shadow domains observed the same C539 slot.
struct NativeGameplayActorBehaviorCompletion {
    NativeGameplayActorBehaviorAdvance behavior;
    ActorPostHandlerPresentation presentation;

    [[nodiscard]] bool operator==(const NativeGameplayActorBehaviorCompletion&) const
        = default;
};

// Rejects a presentation observation from a different actor before either
// owner can submit it to a future native C539 pass.
[[nodiscard]] std::optional<NativeGameplayActorBehaviorCompletion>
join_native_gameplay_actor_behavior_presentation(
    NativeGameplayActorBehaviorAdvance behavior,
    ActorPostHandlerPresentation presentation) noexcept;

// The C539 capture-preserving form additionally requires the complete decoded
// actor record to agree. It prevents a same-slot record whose activity,
// attributes, or dynamic-handler identity changed between the schedule and the
// native behavior observation from being accepted as the same completion.
[[nodiscard]] std::optional<NativeGameplayActorBehaviorCompletion>
join_native_gameplay_actor_behavior_boundary(
    NativeGameplayActorBehaviorAdvance behavior,
    const CapturedScheduledActorHandlerBoundary& boundary) noexcept;

// Invokes one named recovered behavior for a C539 actor record. The ROM
// handler address stays identification-only; dispatch is solely the selected
// native actor slot and the state that has been seeded for it. This owner does
// not implement ActorPassBehaviorDispatcher because it cannot truthfully
// fabricate C606's post-handler presentation attributes.
class NativeGameplayActorBehaviorDispatcher {
public:
    explicit NativeGameplayActorBehaviorDispatcher(
        NativeGameplayState& gameplay) noexcept;

    [[nodiscard]] std::optional<NativeGameplayActorBehaviorAdvance>
    advance_actor(const GameStateView::ActorSlot& actor);

private:
    NativeGameplayState& gameplay_;
};

// One shadow dispatch attempt over a contiguous C539 prefix. A blocked actor
// is deliberately retained as the next actor in the pass: this result is a
// coverage observation, not permission to skip an unported behavior family.
struct ShortActorPassDispatchRun {
    std::vector<ActorPresentationQueueStep> completed_handlers;
    std::optional<GameStateView::ActorSlot> blocked_actor;
    bool complete = false;

    [[nodiscard]] bool operator==(const ShortActorPassDispatchRun&) const
        = default;
};

// Explains why a capture-backed C539 completion replay stopped before the
// current pass was complete. The absence of a reason means the pass completed.
enum class ShortActorPassCompletionReplayBlockReason {
    awaiting_completion,
    rejected_completion,
};

// Result of replaying capture-backed native behavior completions through a
// short C539 pass. A mismatched observation is left unconsumed at the pass
// cursor, preserving the exact point where shadow evidence ceased to agree.
struct ShortActorPassCompletionReplay {
    std::vector<ActorPresentationQueueStep> completed_handlers;
    std::optional<GameStateView::ActorSlot> blocked_actor;
    std::optional<ShortActorPassCompletionReplayBlockReason> block_reason;
    // Present only when an observed completion was available but failed the
    // current pass cursor's strict identity checks. This preserves the exact
    // evidence that stopped the shadow replay.
    std::optional<NativeGameplayActorBehaviorCompletion> rejected_completion;
    std::size_t consumed_completions = 0U;
    std::size_t unconsumed_completions = 0U;
    // Present only after a complete pass when capture supplied an additional
    // completion. Keeping the first surplus record makes cross-pass capture
    // attribution explicit without treating it as a failure of this pass.
    std::optional<NativeGameplayActorBehaviorCompletion> first_surplus_completion;
    bool pass_complete = false;

    // Validates the mutually exclusive terminal forms emitted by the replay
    // helper. Capture adapters can use this before reporting a result without
    // granting malformed observation data any scheduler authority.
    [[nodiscard]] bool coherent() const noexcept;

    [[nodiscard]] bool operator==(const ShortActorPassCompletionReplay&) const
        = default;
};

// Owns the C539-to-C606 presentation state for one short actor pass. A named
// behavior handler submits its final presentation observations here exactly
// once after it has run; the pass then carries the resulting queues forward to
// the next slot. It deliberately does not decide which handler to execute.
class ShortActorPassPresentation {
public:
    explicit ShortActorPassPresentation(ShortActorPassSeed seed) noexcept;

    [[nodiscard]] const GameStateView::ActorFrameGate& gate() const noexcept;
    [[nodiscard]] std::uint8_t completed_ball_substeps() const noexcept;
    [[nodiscard]] const ActorPresentationQueueState& presentation_queues()
        const noexcept;

    // Applies C606 after one native handler has completed. The returned step
    // retains the old and new queue states for an oracle, while this owner
    // keeps the new state for the remaining slots in the pass.
    [[nodiscard]] ActorPresentationQueueStep complete_handler(
        ActorPostHandlerPresentation actor) noexcept;

private:
    GameStateView::ActorFrameGate gate_;
    ActorPresentationQueueState presentation_queues_;
    std::uint8_t completed_ball_substeps_ = 0U;
};

// Owns one fully named C539 short pass after its frame seed has been decoded.
// Behavior families still own their individual updates, but they must submit
// their final presentation result in the original slot order. This keeps the
// scheduler's ownership boundary explicit without turning a dynamic handler
// dispatch into a collection of instruction-shaped replacements.
class ShortActorPass {
public:
    ShortActorPass(
        ShortActorPassSeed seed,
        std::vector<GameStateView::ActorSlot> scheduled_actors) noexcept;

    [[nodiscard]] const GameStateView::ActorFrameGate& gate() const noexcept;
    [[nodiscard]] std::uint8_t completed_ball_substeps() const noexcept;
    [[nodiscard]] const ActorPresentationQueueState& presentation_queues()
        const noexcept;
    [[nodiscard]] const std::vector<GameStateView::ActorSlot>&
    scheduled_actors() const noexcept;
    [[nodiscard]] std::optional<GameStateView::ActorSlot> next_actor()
        const noexcept;
    [[nodiscard]] bool complete() const noexcept;

    // Rejects out-of-order or duplicate completions without mutating the
    // presentation state. A successful submission advances exactly one slot
    // and performs the C606-equivalent filing for that actor.
    [[nodiscard]] std::optional<ActorPresentationQueueStep> complete_handler(
        ActorPostHandlerPresentation actor) noexcept;

    // Files a behavior only after its decoded actor record, native update,
    // captured presentation, and pass cursor all agree. This accepts a
    // completed shadow observation; it does not execute behavior or invent a
    // post-handler attribute value.
    [[nodiscard]] std::optional<ActorPresentationQueueStep>
    complete_native_gameplay_behavior(
        const NativeGameplayActorBehaviorCompletion& completion) noexcept;

    // Replays an ordered captured completion sequence. It stops before the
    // first unavailable or mismatched observation, and reports surplus data
    // rather than silently associating it with a later pass.
    [[nodiscard]] ShortActorPassCompletionReplay
    replay_native_gameplay_behavior_completions(
        const std::vector<NativeGameplayActorBehaviorCompletion>& completions) noexcept;

    // Invokes a named native behavior only for the next scheduled actor.
    // An unavailable behavior leaves the cursor and presentation queues
    // untouched, preserving a clean emulator fallback boundary.
    [[nodiscard]] std::optional<ActorPresentationQueueStep> advance_next_actor(
        ActorPassBehaviorDispatcher& dispatcher) noexcept;

    // Advances the contiguous prefix that the supplied native dispatcher can
    // complete. The first unavailable or invalid result remains pending so a
    // shadow run never changes the original actor ordering.
    [[nodiscard]] ShortActorPassDispatchRun advance_available_prefix(
        ActorPassBehaviorDispatcher& dispatcher) noexcept;

private:
    ShortActorPassPresentation presentation_;
    std::vector<GameStateView::ActorSlot> scheduled_actors_;
    std::size_t next_actor_index_ = 0U;
};

// Files one completed native handler result through the recovered C606
// boundary. A future behavior handler owns its own update and any F605-like
// presentation refresh it needs; this function owns only the proven handoff
// to presentation.
[[nodiscard]] ActorPresentationQueueStep file_completed_actor_for_presentation(
    ActorPresentationQueueState state,
    ActorPostHandlerPresentation actor) noexcept;

// Files one dispatched actor using its final post-handler attributes. The
// caller carries `requires_prequeue_presentation_refresh` from the pre-helper
// attributes, because the helper may change those attributes before C606
// performs its second read. This lower-level form treats `attributes` as the
// already-eligible queue-selection value; callers that have both C606 reads
// should use `file_completed_actor_for_presentation` so its initial bit-seven
// eligibility gate remains explicit.
[[nodiscard]] ActorPresentationQueueStep file_actor_for_presentation(
    ActorPresentationQueueState state,
    std::uint8_t slot,
    std::uint8_t attributes,
    bool requires_prequeue_presentation_refresh = false) noexcept;

// Returns the dynamic slots in the SAT driver's drain order: auxiliary,
// primary-low, then primary-high. The fixed slot-zero stage is intentionally
// absent and belongs between the latter two stages in the presentation owner.
[[nodiscard]] std::vector<std::uint8_t> actor_presentation_drain_order(
    const ActorPresentationQueueState& state);

// The table owns a fixed pool of actor records. This is the semantic boundary
// for the old per-frame slot walk: callers receive actors due for an update,
// never a CPU continuation.
class ActorSchedulerView {
public:
    struct DynamicActorReservation {
        std::uint8_t slot = 0;
        GameStateView::DynamicActorReservationKind kind =
            GameStateView::DynamicActorReservationKind::standard;

        [[nodiscard]] bool operator==(const DynamicActorReservation&) const = default;
    };

    // One actor that passed the CED2 scanner's cheap vertical gate. It remains
    // a geometry candidate: descriptor selection, mask lookup, and contact
    // resolution deliberately have not happened yet.
    struct CollisionRetryVerticalCandidate {
        GameStateView::ActorSlot actor;
        ActorVerticalProximity vertical_proximity;

        [[nodiscard]] bool operator==(const CollisionRetryVerticalCandidate&) const
            = default;
    };

    explicit ActorSchedulerView(const GameStateView& state) noexcept;

    // Models C510's decision for one live actor-table slot. The caller may
    // sample it at each loop head when an earlier handler can have changed a
    // later slot during the same scan.
    [[nodiscard]] static bool actor_is_due_for_update(
        const GameStateView::ActorSlot& actor) noexcept;

    // Models precisely one native C510 transcript starting at `first_slot`.
    // It samples only the current state image, so callers resume from the
    // dispatch actor's successor after that actor's handler completes.
    // Invalid images and an out-of-range start slot have no recovered run.
    [[nodiscard]] std::optional<MainActorScanRun> scan_main_actor_run(
        std::uint8_t first_slot) const;

    // Returns records in their table order, matching the game's observable
    // update order. An invalid state image produces no scheduled actors.
    [[nodiscard]] std::vector<GameStateView::ActorSlot>
    actors_due_for_update() const;

    // Decodes C539's frame-entry state without treating its composite
    // dispatcher and dynamic handlers as one replaceable routine. The caller
    // owns the subsequent named behavior updates and C606 filings.
    [[nodiscard]] std::optional<ShortActorPassSeed> begin_short_actor_pass()
        const noexcept;

    // Captures C539's reset seed and its decoded table-order due-actor list
    // from the same state view. This is read-only shadow input; behavior and
    // C606 presentation ownership remain separate.
    [[nodiscard]] std::optional<ShortActorPassCapture>
    capture_short_actor_pass() const;

    // Builds a pass from one capture. This is the owner-level handoff for a
    // future native behavior dispatcher; no live replacement is installed
    // until its behavior families are proven.
    [[nodiscard]] std::optional<ShortActorPass> begin_short_actor_pass_session()
        const;

    // The collision-retry full scanner traverses dynamic slots 4 through 127
    // in order. Its eligibility bit lives in the attribute record and is
    // distinct from the general actor-update activity gate: a negative
    // attribute participates unless its collision-suppression bit is set.
    // Geometry remains with the later actor-collision resolver.
    [[nodiscard]] std::vector<GameStateView::ActorSlot>
    actors_for_collision_retry_full_scan() const;

    // Applies the recovered CC1A first gate to the CED2 candidate order. The
    // caller supplies the scanner's 16-bit vertical source explicitly: the
    // retry entry proves its low byte, but a natural CED2 trace is still
    // required to certify the high-byte source. Results preserve slot order
    // and are only inputs to the later descriptor/mask resolver.
    [[nodiscard]] std::vector<CollisionRetryVerticalCandidate>
    actors_for_collision_retry_vertical_scan(std::uint16_t scan_vertical) const;

    // Chooses the first unoccupied dynamic slot. Claiming it is the native
    // actor state's responsibility; this read-only adapter never writes the
    // emulator image during migration.
    [[nodiscard]] std::optional<DynamicActorReservation>
    next_dynamic_actor_reservation() const noexcept;

private:
    const GameStateView& state_;
};

} // namespace devils_crush::native_runtime
