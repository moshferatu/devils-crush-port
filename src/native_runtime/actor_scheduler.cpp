// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_scheduler.hpp"

namespace devils_crush::native_runtime {

ShortActorPassPresentation::ShortActorPassPresentation(
    const ShortActorPassSeed seed) noexcept
    : gate_(seed.gate)
    , presentation_queues_(seed.presentation_queues)
    , completed_ball_substeps_(seed.completed_ball_substeps)
{
}

const GameStateView::ActorFrameGate& ShortActorPassPresentation::gate() const noexcept
{
    return gate_;
}

std::uint8_t ShortActorPassPresentation::completed_ball_substeps() const noexcept
{
    return completed_ball_substeps_;
}

const ActorPresentationQueueState&
ShortActorPassPresentation::presentation_queues() const noexcept
{
    return presentation_queues_;
}

ActorPresentationQueueStep ShortActorPassPresentation::complete_handler(
    const ActorPostHandlerPresentation actor) noexcept
{
    const auto step = file_completed_actor_for_presentation(
        presentation_queues_, actor);
    presentation_queues_ = step.state;
    return step;
}

ActorPresentationQueueStep file_completed_actor_for_presentation(
    ActorPresentationQueueState state,
    const ActorPostHandlerPresentation actor) noexcept
{
    return file_actor_for_presentation(
        state,
        actor.slot,
        actor.attributes_after_prequeue_presentation_refresh,
        (actor.attributes_before_prequeue_presentation_refresh & 0x01U) != 0U);
}

ActorPresentationQueueStep file_actor_for_presentation(
    ActorPresentationQueueState state,
    const std::uint8_t slot,
    const std::uint8_t attributes,
    const bool requires_prequeue_presentation_refresh) noexcept
{
    const auto previous = state;
    if (slot == 0U) {
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::slot_zero_deferred,
            .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
        };
    }
    if ((attributes & 0x20U) != 0U) {
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::hidden,
            .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
        };
    }
    if ((attributes & 0x04U) != 0U) {
        if (state.auxiliary_count >= ActorPresentationQueueState::kQueueCapacity) {
            return {
                .previous = previous,
                .state = state,
                .slot = slot,
                .queue = ActorPresentationQueueKind::capacity_exhausted,
                .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
            };
        }
        state.auxiliary_slots[state.auxiliary_count++] = slot;
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::auxiliary,
            .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
        };
    }
    if (state.primary_low_count + state.primary_high_count
        >= ActorPresentationQueueState::kQueueCapacity) {
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::capacity_exhausted,
            .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
        };
    }
    if ((attributes & 0x08U) != 0U) {
        state.primary_low_slots[state.primary_low_count++] = slot;
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::primary_low,
            .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
        };
    }
    state.primary_high_slots[state.primary_high_count++] = slot;
    return {
        .previous = previous,
        .state = state,
        .slot = slot,
        .queue = ActorPresentationQueueKind::primary_high,
        .requires_prequeue_presentation_refresh = requires_prequeue_presentation_refresh,
    };
}

std::vector<std::uint8_t> actor_presentation_drain_order(
    const ActorPresentationQueueState& state)
{
    std::vector<std::uint8_t> slots;
    slots.reserve(
        state.auxiliary_count + state.primary_low_count + state.primary_high_count);
    for (std::size_t index = state.auxiliary_count; index > 0U; --index) {
        slots.push_back(state.auxiliary_slots[index - 1U]);
    }
    for (std::size_t index = state.primary_low_count; index > 0U; --index) {
        slots.push_back(state.primary_low_slots[index - 1U]);
    }
    for (std::size_t index = 0U; index < state.primary_high_count; ++index) {
        slots.push_back(state.primary_high_slots[index]);
    }
    return slots;
}

ActorSchedulerView::ActorSchedulerView(const GameStateView& state) noexcept
    : state_(state)
{
}

std::vector<GameStateView::ActorSlot>
ActorSchedulerView::actors_due_for_update() const
{
    std::vector<GameStateView::ActorSlot> actors;
    if (!state_.valid()) {
        return actors;
    }

    actors.reserve(GameStateView::kActorSlotCount);
    for (std::uint8_t index = 0U;
         index < GameStateView::kActorSlotCount;
         ++index) {
        const auto actor = state_.actor(index);
        if (!actor || (actor->activity_flags & 0x80U) == 0U) {
            continue;
        }

        // The active actor's attribute record may explicitly defer its turn.
        // Negative attributes always run; otherwise bit 1 selects the defer
        // path. This is a domain policy, not an emulated branch sequence.
        if ((actor->attributes & 0x80U) == 0U
            && (actor->attributes & 0x02U) != 0U) {
            continue;
        }
        actors.push_back(*actor);
    }
    return actors;
}

std::optional<ShortActorPassSeed>
ActorSchedulerView::begin_short_actor_pass() const noexcept
{
    const auto gate = state_.short_actor_frame_gate();
    if (!gate) {
        return std::nullopt;
    }

    return ShortActorPassSeed{
        .gate = *gate,
    };
}

std::vector<GameStateView::ActorSlot>
ActorSchedulerView::actors_for_collision_retry_full_scan() const
{
    std::vector<GameStateView::ActorSlot> actors;
    if (!state_.valid()) {
        return actors;
    }

    actors.reserve(GameStateView::kActorSlotCount - 4U);
    for (std::uint8_t index = 0x04U;
         index < GameStateView::kActorSlotCount;
         ++index) {
        const auto actor = state_.actor(index);
        if (!actor || (actor->attributes & 0x80U) == 0U
            || (actor->attributes & 0x40U) != 0U) {
            continue;
        }
        actors.push_back(*actor);
    }
    return actors;
}

std::vector<ActorSchedulerView::CollisionRetryVerticalCandidate>
ActorSchedulerView::actors_for_collision_retry_vertical_scan(
    const std::uint16_t scan_vertical) const
{
    std::vector<CollisionRetryVerticalCandidate> candidates;
    const auto actors = actors_for_collision_retry_full_scan();
    candidates.reserve(actors.size());
    for (const auto& actor : actors) {
        const auto vertical_proximity = evaluate_actor_vertical_proximity(
            scan_vertical, actor.collision_table.position_y);
        if (vertical_proximity.intersecting) {
            candidates.push_back({
                .actor = actor,
                .vertical_proximity = vertical_proximity,
            });
        }
    }
    return candidates;
}

std::optional<ActorSchedulerView::DynamicActorReservation>
ActorSchedulerView::next_dynamic_actor_reservation() const noexcept
{
    constexpr std::uint8_t kFirstDynamicSlot = 0x04U;
    constexpr std::uint8_t kPastLastDynamicSlot = 0x7CU;

    const auto kind = state_.dynamic_actor_reservation_kind();
    if (!kind) {
        return std::nullopt;
    }
    for (std::uint8_t slot = kFirstDynamicSlot;
         slot < kPastLastDynamicSlot;
         ++slot) {
        const auto actor = state_.actor(slot);
        if (actor && actor->activity_flags == 0U) {
            return DynamicActorReservation{.slot = slot, .kind = *kind};
        }
    }
    return std::nullopt;
}

} // namespace devils_crush::native_runtime
