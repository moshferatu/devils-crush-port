// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

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

// One C606-equivalent filing decision. A bit-zero attribute asks the owning
// presentation-refresh helper to run before filing; the caller supplies that
// prior decision separately from the helper's resulting attributes, so this
// value records the dependency without guessing actor-family behavior.
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

// The completed handler's presentation-facing result. C606 samples bit zero
// before F605 refreshes transformed presentation coordinates, then chooses the
// queue from the reread attribute value. Keeping both observations makes that
// sequencing a typed scheduler contract instead of asking a behavior family
// to reconstruct it.
struct ActorPostHandlerPresentation {
    std::uint8_t slot = 0U;
    std::uint8_t attributes_before_prequeue_presentation_refresh = 0U;
    std::uint8_t attributes_after_prequeue_presentation_refresh = 0U;

    [[nodiscard]] bool operator==(const ActorPostHandlerPresentation&) const
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
// performs its second read. Slot zero is emitted by the fixed middle SAT
// stage, while all other slots enter one of the three dynamic queues or are
// explicitly hidden.
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

    // Returns records in their table order, matching the game's observable
    // update order. An invalid state image produces no scheduled actors.
    [[nodiscard]] std::vector<GameStateView::ActorSlot>
    actors_due_for_update() const;

    // Decodes C539's frame-entry state without treating its composite
    // dispatcher and dynamic handlers as one replaceable routine. The caller
    // owns the subsequent named behavior updates and C606 filings.
    [[nodiscard]] std::optional<ShortActorPassSeed> begin_short_actor_pass()
        const noexcept;

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
