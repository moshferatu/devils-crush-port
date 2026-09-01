// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_scheduler.hpp"

#include <utility>

namespace devils_crush::native_runtime {
namespace {

ActorPresentationQueueStep file_presentation_eligible_actor(
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
            .requires_prequeue_presentation_refresh =
                requires_prequeue_presentation_refresh,
        };
    }
    if ((attributes & 0x20U) != 0U) {
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::hidden,
            .requires_prequeue_presentation_refresh =
                requires_prequeue_presentation_refresh,
        };
    }
    if ((attributes & 0x04U) != 0U) {
        if (state.auxiliary_count >= ActorPresentationQueueState::kQueueCapacity) {
            return {
                .previous = previous,
                .state = state,
                .slot = slot,
                .queue = ActorPresentationQueueKind::capacity_exhausted,
                .requires_prequeue_presentation_refresh =
                    requires_prequeue_presentation_refresh,
            };
        }
        state.auxiliary_slots[state.auxiliary_count++] = slot;
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::auxiliary,
            .requires_prequeue_presentation_refresh =
                requires_prequeue_presentation_refresh,
        };
    }
    if (state.primary_low_count + state.primary_high_count
        >= ActorPresentationQueueState::kQueueCapacity) {
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::capacity_exhausted,
            .requires_prequeue_presentation_refresh =
                requires_prequeue_presentation_refresh,
        };
    }
    if ((attributes & 0x08U) != 0U) {
        state.primary_low_slots[state.primary_low_count++] = slot;
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::primary_low,
            .requires_prequeue_presentation_refresh =
                requires_prequeue_presentation_refresh,
        };
    }
    state.primary_high_slots[state.primary_high_count++] = slot;
    return {
        .previous = previous,
        .state = state,
        .slot = slot,
        .queue = ActorPresentationQueueKind::primary_high,
        .requires_prequeue_presentation_refresh =
            requires_prequeue_presentation_refresh,
    };
}

} // namespace

std::optional<ActorPresentationQueueState>
capture_actor_presentation_queue_state(const GameStateView& state) noexcept
{
    const auto image = state.actor_presentation_queue_image();
    if (!image || image->auxiliary_count > ActorPresentationQueueState::kQueueCapacity
        || image->primary_low_count > ActorPresentationQueueState::kQueueCapacity
        || image->primary_high_next_index >= ActorPresentationQueueState::kQueueCapacity) {
        return std::nullopt;
    }

    const auto primary_high_count =
        ActorPresentationQueueState::kQueueCapacity - 1U
        - image->primary_high_next_index;
    if (static_cast<std::size_t>(image->primary_low_count) + primary_high_count
        > ActorPresentationQueueState::kQueueCapacity) {
        return std::nullopt;
    }

    ActorPresentationQueueState queues{
        .auxiliary_count = image->auxiliary_count,
        .primary_low_count = image->primary_low_count,
        .primary_high_count = primary_high_count,
    };
    for (std::size_t index = 0U; index < queues.auxiliary_count; ++index) {
        queues.auxiliary_slots[index] = image->auxiliary_slots[index];
    }
    for (std::size_t index = 0U; index < queues.primary_low_count; ++index) {
        queues.primary_low_slots[index] = image->primary_slots[index];
    }
    for (std::size_t index = 0U; index < queues.primary_high_count; ++index) {
        queues.primary_high_slots[index] = image->primary_slots[
            ActorPresentationQueueState::kQueueCapacity - 1U - index];
    }
    return queues;
}

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

MainActorPass::MainActorPass(
    const ActorPresentationQueueState presentation_queues) noexcept
    : presentation_queues_(presentation_queues)
{
}

const ActorPresentationQueueState& MainActorPass::presentation_queues() const noexcept
{
    return presentation_queues_;
}

std::optional<GameStateView::ActorSlot> MainActorPass::pending_actor() const noexcept
{
    return pending_actor_;
}

bool MainActorPass::terminal() const noexcept
{
    return terminal_;
}

bool MainActorPass::submit_scan_run(const MainActorScanRun& run) noexcept
{
    if (terminal_ || pending_actor_) {
        return false;
    }
    if (run.dispatch_actor) {
        pending_actor_ = *run.dispatch_actor;
    } else {
        terminal_ = true;
    }
    return true;
}

std::optional<ActorPresentationQueueStep> MainActorPass::complete_handler(
    const ActorPostHandlerPresentation actor) noexcept
{
    if (!pending_actor_ || actor.slot != pending_actor_->index) {
        return std::nullopt;
    }
    const auto step = file_completed_actor_for_presentation(
        presentation_queues_, actor);
    presentation_queues_ = step.state;
    pending_actor_.reset();
    return step;
}

std::optional<ActorPostHandlerPresentation>
capture_actor_post_handler_presentation(
    const GameStateView& before_handler,
    const GameStateView& after_handler,
    const std::uint8_t slot) noexcept
{
    const auto before = before_handler.actor(slot);
    const auto after = after_handler.actor(slot);
    if (!before || !after) {
        return std::nullopt;
    }
    return ActorPostHandlerPresentation{
        .slot = slot,
        .attributes_before_prequeue_presentation_refresh = before->attributes,
        .attributes_after_prequeue_presentation_refresh = after->attributes,
    };
}

std::optional<ActorBehaviorStateChanges>
capture_actor_behavior_state_changes(
    const GameStateView::ActorSlot& before,
    const GameStateView::ActorSlot& after) noexcept
{
    if (before.index != after.index) {
        return std::nullopt;
    }
    return ActorBehaviorStateChanges{
        .activity_flags = before.activity_flags != after.activity_flags,
        .attributes = before.attributes != after.attributes,
        .behavior_counter = before.behavior_counter != after.behavior_counter,
        .handler_address = before.handler_address != after.handler_address,
        .horizontal_motion = before.horizontal_motion != after.horizontal_motion,
        .vertical_motion = before.vertical_motion != after.vertical_motion,
        .collision_state = before.collision_state != after.collision_state
            || before.collision_table_program != after.collision_table_program
            || before.collision_shape_directory_index
                != after.collision_shape_directory_index
            || before.collision_shape_phase != after.collision_shape_phase
            || before.collision_table != after.collision_table
            || before.collision_reaction != after.collision_reaction,
    };
}

std::optional<CapturedScheduledActorHandlerBoundary>
capture_scheduled_actor_handler_boundary(
    const GameStateView& before_handler,
    const GameStateView& after_handler,
    const GameStateView::ActorSlot& scheduled_actor) noexcept
{
    const auto before = before_handler.actor(scheduled_actor.index);
    if (!before || *before != scheduled_actor) {
        return std::nullopt;
    }
    const auto presentation = capture_actor_post_handler_presentation(
        before_handler, after_handler, scheduled_actor.index);
    if (!presentation) {
        return std::nullopt;
    }
    return CapturedScheduledActorHandlerBoundary{
        .scheduled_actor = scheduled_actor,
        .presentation = *presentation,
    };
}

std::optional<NativeGameplayActorBehaviorCompletion>
join_native_gameplay_actor_behavior_presentation(
    const NativeGameplayActorBehaviorAdvance behavior,
    const ActorPostHandlerPresentation presentation) noexcept
{
    if (behavior.actor.index != behavior.update.slot
        || behavior.actor.index != presentation.slot) {
        return std::nullopt;
    }
    return NativeGameplayActorBehaviorCompletion{
        .behavior = behavior,
        .presentation = presentation,
    };
}

std::optional<NativeGameplayActorBehaviorCompletion>
join_native_gameplay_actor_behavior_boundary(
    const NativeGameplayActorBehaviorAdvance behavior,
    const CapturedScheduledActorHandlerBoundary& boundary) noexcept
{
    if (behavior.actor != boundary.scheduled_actor) {
        return std::nullopt;
    }
    return join_native_gameplay_actor_behavior_presentation(
        behavior, boundary.presentation);
}

NativeGameplayActorBehaviorDispatcher::NativeGameplayActorBehaviorDispatcher(
    NativeGameplayState& gameplay) noexcept
    : gameplay_(gameplay)
{
}

std::optional<NativeGameplayActorBehaviorAdvance>
NativeGameplayActorBehaviorDispatcher::advance_actor(
    const GameStateView::ActorSlot& actor)
{
    const auto update = gameplay_.advance_actor_phase_slot(actor.index);
    if (!update || update->slot != actor.index) {
        return std::nullopt;
    }
    return NativeGameplayActorBehaviorAdvance{
        .actor = actor,
        .update = *update,
    };
}

ShortActorPass::ShortActorPass(
    const ShortActorPassSeed seed,
    std::vector<GameStateView::ActorSlot> scheduled_actors) noexcept
    : presentation_(seed)
    , scheduled_actors_(std::move(scheduled_actors))
{
}

const GameStateView::ActorFrameGate& ShortActorPass::gate() const noexcept
{
    return presentation_.gate();
}

std::uint8_t ShortActorPass::completed_ball_substeps() const noexcept
{
    return presentation_.completed_ball_substeps();
}

const ActorPresentationQueueState& ShortActorPass::presentation_queues() const noexcept
{
    return presentation_.presentation_queues();
}

const std::vector<GameStateView::ActorSlot>& ShortActorPass::scheduled_actors()
    const noexcept
{
    return scheduled_actors_;
}

std::optional<GameStateView::ActorSlot> ShortActorPass::next_actor() const noexcept
{
    if (next_actor_index_ >= scheduled_actors_.size()) {
        return std::nullopt;
    }
    return scheduled_actors_[next_actor_index_];
}

bool ShortActorPass::complete() const noexcept
{
    return next_actor_index_ == scheduled_actors_.size();
}

std::optional<ActorPresentationQueueStep> ShortActorPass::complete_handler(
    const ActorPostHandlerPresentation actor) noexcept
{
    const auto expected = next_actor();
    if (!expected || actor.slot != expected->index) {
        return std::nullopt;
    }

    ++next_actor_index_;
    return presentation_.complete_handler(actor);
}

std::optional<ActorPresentationQueueStep>
ShortActorPass::complete_native_gameplay_behavior(
    const NativeGameplayActorBehaviorCompletion& completion) noexcept
{
    const auto expected = next_actor();
    if (!expected || completion.behavior.actor != *expected
        || completion.behavior.actor.index != expected->index
        || completion.behavior.update.slot != expected->index
        || completion.presentation.slot != expected->index) {
        return std::nullopt;
    }
    return complete_handler(completion.presentation);
}

ShortActorPassCompletionReplay
ShortActorPass::replay_native_gameplay_behavior_completions(
    const std::vector<NativeGameplayActorBehaviorCompletion>& completions) noexcept
{
    ShortActorPassCompletionReplay replay;
    while (const auto actor = next_actor()) {
        if (replay.consumed_completions >= completions.size()) {
            replay.blocked_actor = *actor;
            replay.block_reason =
                ShortActorPassCompletionReplayBlockReason::awaiting_completion;
            return replay;
        }
        const auto step = complete_native_gameplay_behavior(
            completions[replay.consumed_completions]);
        if (!step) {
            replay.blocked_actor = *actor;
            replay.block_reason =
                ShortActorPassCompletionReplayBlockReason::rejected_completion;
            replay.rejected_completion = completions[replay.consumed_completions];
            replay.unconsumed_completions = completions.size()
                - replay.consumed_completions;
            return replay;
        }
        replay.completed_handlers.push_back(*step);
        ++replay.consumed_completions;
    }
    replay.pass_complete = true;
    replay.unconsumed_completions = completions.size() - replay.consumed_completions;
    if (replay.unconsumed_completions != 0U) {
        replay.first_surplus_completion = completions[replay.consumed_completions];
    }
    return replay;
}

bool ShortActorPassCompletionReplay::coherent() const noexcept
{
    if (pass_complete) {
        return !blocked_actor && !block_reason && !rejected_completion
            && (unconsumed_completions == 0U) == !first_surplus_completion;
    }
    if (!blocked_actor || !block_reason || first_surplus_completion) {
        return false;
    }
    switch (*block_reason) {
    case ShortActorPassCompletionReplayBlockReason::awaiting_completion:
        return !rejected_completion && unconsumed_completions == 0U;
    case ShortActorPassCompletionReplayBlockReason::rejected_completion:
        return rejected_completion && unconsumed_completions != 0U;
    }
    return false;
}

std::optional<ActorPresentationQueueStep> ShortActorPass::advance_next_actor(
    ActorPassBehaviorDispatcher& dispatcher) noexcept
{
    const auto actor = next_actor();
    if (!actor) {
        return std::nullopt;
    }

    const auto presentation = dispatcher.advance_actor(*actor);
    if (!presentation) {
        return std::nullopt;
    }
    return complete_handler(*presentation);
}

ShortActorPassDispatchRun ShortActorPass::advance_available_prefix(
    ActorPassBehaviorDispatcher& dispatcher) noexcept
{
    ShortActorPassDispatchRun run;
    while (const auto actor = next_actor()) {
        const auto step = advance_next_actor(dispatcher);
        if (!step) {
            run.blocked_actor = *actor;
            return run;
        }
        run.completed_handlers.push_back(*step);
    }
    run.complete = true;
    return run;
}

ActorPresentationQueueStep file_completed_actor_for_presentation(
    ActorPresentationQueueState state,
    const ActorPostHandlerPresentation actor) noexcept
{
    if (actor.slot == 0U) {
        return {
            .previous = state,
            .state = state,
            .slot = actor.slot,
            .queue = ActorPresentationQueueKind::slot_zero_deferred,
            .requires_prequeue_presentation_refresh = false,
        };
    }
    if ((actor.attributes_before_prequeue_presentation_refresh & 0x80U) == 0U) {
        return {
            .previous = state,
            .state = state,
            .slot = actor.slot,
            .queue = ActorPresentationQueueKind::not_presentation_eligible,
            .requires_prequeue_presentation_refresh = false,
        };
    }
    return file_presentation_eligible_actor(
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
            .requires_prequeue_presentation_refresh = false,
        };
    }
    if ((attributes & 0x80U) == 0U) {
        return {
            .previous = previous,
            .state = state,
            .slot = slot,
            .queue = ActorPresentationQueueKind::not_presentation_eligible,
            .requires_prequeue_presentation_refresh = false,
        };
    }
    return file_presentation_eligible_actor(
        state, slot, attributes, requires_prequeue_presentation_refresh);
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

bool ActorSchedulerView::actor_is_due_for_update(
    const GameStateView::ActorSlot& actor) noexcept
{
    if ((actor.activity_flags & 0x80U) == 0U) {
        return false;
    }

    // C510 dispatches negative attributes immediately. Otherwise bit one
    // selects its defer path.
    return (actor.attributes & 0x80U) != 0U
        || (actor.attributes & 0x02U) == 0U;
}

std::optional<MainActorScanRun>
ActorSchedulerView::scan_main_actor_run(const std::uint8_t first_slot) const
{
    if (!state_.valid()) {
        return std::nullopt;
    }

    MainActorScanRun run;
    run.skipped_actors.reserve(GameStateView::kActorSlotCount - first_slot);
    for (std::uint16_t raw_index = first_slot;
         raw_index < GameStateView::kActorSlotCount;
         ++raw_index) {
        const auto actor = state_.actor(static_cast<std::uint8_t>(raw_index));
        if (!actor) {
            return std::nullopt;
        }
        if (actor_is_due_for_update(*actor)) {
            run.dispatch_actor = *actor;
            return run;
        }
        run.skipped_actors.push_back(*actor);
    }
    return run;
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
        if (!actor || !actor_is_due_for_update(*actor)) {
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

std::optional<ShortActorPassCapture>
ActorSchedulerView::capture_short_actor_pass() const
{
    const auto seed = begin_short_actor_pass();
    if (!seed) {
        return std::nullopt;
    }
    return ShortActorPassCapture{
        .seed = *seed,
        .scheduled_actors = actors_due_for_update(),
    };
}

std::optional<ShortActorPass>
ActorSchedulerView::begin_short_actor_pass_session() const
{
    const auto capture = capture_short_actor_pass();
    if (!capture) {
        return std::nullopt;
    }
    return ShortActorPass(capture->seed, capture->scheduled_actors);
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
