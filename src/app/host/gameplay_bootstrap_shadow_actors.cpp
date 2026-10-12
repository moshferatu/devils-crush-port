// SPDX-License-Identifier: GPL-3.0-or-later
#include "gameplay_bootstrap_shadow.hpp"

namespace devils_crush::app::host {

void NativeGameplayBootstrapShadow::observe_main_actor_pass_entry(
    const RoutineEntry& entry,
    const devils_crush::native_runtime::GameStateView& state) noexcept
{
    // C510 is a loop head, not a one-shot subroutine entry. The native
    // registry can therefore surface it after every C523 tail that
    // reaches the next slot. X is loaded from $4D on those re-entries;
    // only C4DE's initial CLX/STX setup reaches C510 with X=0. Treating
    // every later arrival as another pass had discarded the pending
    // aggregate before its final C52B RTS could be observed.
    if (active_main_actor_pass
        && active_main_actor_pass->pending_decision) {
        // Reaching another C510 loop head proves that the prior native
        // run reached its terminal C523/C52B path rather than C52C.
        const auto pending = *active_main_actor_pass->pending_decision;
        active_main_actor_pass->pending_decision.reset();
        if (!pending.dispatch_expected) {
            ++active_main_actor_pass->matched_dispatch_decisions;
        } else {
            ++active_main_actor_pass->mismatched_dispatch_decisions;
        }
    }

    if (entry.registers.x == 0U) {
        ++main_actor_pass_entries;
        if (active_main_actor_pass) {
            // A new zero-slot arrival means the prior pass never
            // presented its terminal C52B return boundary. Do not join
            // them.
            ++superseded_main_actor_passes;
            active_main_actor_pass.reset();
        }

        const auto queues = devils_crush::native_runtime::
            capture_actor_presentation_queue_state(state);
        if (!queues) {
            ++unavailable_main_actor_pass_inputs;
            return;
        }

        try {
            active_main_actor_pass = PendingMainActorPass{
                .model = devils_crush::native_runtime::MainActorPass{
                    *queues},
            };
            ++captured_main_actor_passes;
        } catch (...) {
            ++main_actor_pass_capture_failures;
            return;
        }
    }

    if (!active_main_actor_pass) {
        return;
    }
    const devils_crush::native_runtime::ActorSchedulerView scheduler{
        state};
    const auto run = scheduler.scan_main_actor_run(entry.registers.x);
    if (!run) {
        ++unavailable_main_actor_pass_inputs;
        return;
    }
    const auto scanned_actor = !run->skipped_actors.empty()
        ? std::optional<devils_crush::native_runtime::GameStateView::ActorSlot>{
            run->skipped_actors.front()}
        : run->dispatch_actor;
    if (!scanned_actor
        || !active_main_actor_pass->model.submit_scan_run(*run)) {
        ++unavailable_main_actor_pass_inputs;
        return;
    }
    active_main_actor_pass->pending_decision = {
        .slot = entry.registers.x,
        .activity_flags = scanned_actor->activity_flags,
        .attributes = scanned_actor->attributes,
        .expected_dispatch_slot = run->dispatch_actor
            ? std::optional<std::uint8_t>{run->dispatch_actor->index}
            : std::nullopt,
        .skipped_actor_count = run->skipped_actors.size(),
        .dispatch_expected = run->dispatch_actor.has_value(),
    };
}

void NativeGameplayBootstrapShadow::observe_main_actor_pass_gate_write(
    const ObservedMemoryWrite& write) noexcept
{
    if (!active_main_actor_pass
        || !active_main_actor_pass->pending_decision) {
        return;
    }
    auto& decision = *active_main_actor_pass->pending_decision;
    const auto activity_address = static_cast<std::uint16_t>(
        0x3000U + decision.slot);
    const auto attribute_address = static_cast<std::uint16_t>(
        0x3080U + decision.slot);
    if (write.logical_address == activity_address) {
        decision.activity_gate_writer = write.instruction;
    } else if (write.logical_address == attribute_address) {
        decision.attribute_gate_writer = write.instruction;
    }
}

void NativeGameplayBootstrapShadow::observe_main_actor_pass_completion(
    const RoutineCall& call,
    const std::span<const std::byte> work_ram) noexcept
{
    // C510 is both the pass head and the loop-back target. Consequently
    // the routine-call observer records one active C510 call per loop
    // head, all with the same stack depth, and C52B closes every one of
    // those observer records together. Only the X=0 record began the
    // aggregate pass below; later loop-head records must not either close
    // it or be counted as unmatched completions.
    constexpr RoutineKey kMainActorPassReturn{
        .physical_bank = 0x01U,
        .logical_pc = 0xC52BU,
    };
    if (call.return_instruction != kMainActorPassReturn
        || call.entry.registers.x != 0U) {
        return;
    }
    if (!active_main_actor_pass) {
        ++unmatched_main_actor_pass_completions;
        return;
    }
    if (active_main_actor_pass->pending_decision) {
        // The terminal C52B return is reached from C523, so an unresolved
        // final decision necessarily took C510's skip path.
        const auto pending = *active_main_actor_pass->pending_decision;
        active_main_actor_pass->pending_decision.reset();
        if (!pending.dispatch_expected) {
            ++active_main_actor_pass->matched_dispatch_decisions;
        } else {
            ++active_main_actor_pass->mismatched_dispatch_decisions;
        }
    }
    const auto completed = std::move(*active_main_actor_pass);
    active_main_actor_pass.reset();
    ++completed_main_actor_passes;
    matched_main_actor_pass_dispatch_decisions
        += completed.matched_dispatch_decisions;
    mismatched_main_actor_pass_dispatch_decisions
        += completed.mismatched_dispatch_decisions;
    main_actor_pass_dispatched_when_skip_expected
        += completed.dispatched_when_skip_expected;
    main_actor_pass_dispatched_for_other_slot
        += completed.dispatched_for_other_slot;
    main_actor_pass_dispatched_after_inactive_activity_gate
        += completed.dispatched_after_inactive_activity_gate;
    main_actor_pass_dispatched_after_deferred_attribute_gate
        += completed.dispatched_after_deferred_attribute_gate;
    main_actor_pass_dispatched_after_changed_gate_bytes
        += completed.dispatched_after_changed_gate_bytes;
    main_actor_pass_dispatched_after_unchanged_gate_bytes
        += completed.dispatched_after_unchanged_gate_bytes;

    const devils_crush::native_runtime::GameStateView state{work_ram};
    const auto queues = devils_crush::native_runtime::
        capture_actor_presentation_queue_state(state);
    if (!queues) {
        ++unavailable_main_actor_pass_outputs;
        return;
    }
    ++main_actor_pass_queue_predictions;
    if (completed.model.terminal()
        && !completed.model.pending_actor()
        && completed.model.presentation_queues() == *queues) {
        ++matched_main_actor_pass_queue_predictions;
    } else {
        ++mismatched_main_actor_pass_queue_predictions;
    }
    main_actor_pass_completed_queue_filings
        += completed.completed_queue_filings;
    main_actor_pass_queue_chain_mismatches
        += completed.queue_chain_mismatches;
}

void NativeGameplayBootstrapShadow::record_main_actor_pass_gate_writer(
    const std::optional<RoutineKey> writer,
    const bool activity) noexcept
{
    if (!writer) {
        if (activity) {
            ++main_actor_pass_unobserved_activity_gate_writers;
        } else {
            ++main_actor_pass_unobserved_attribute_gate_writers;
        }
        return;
    }
    for (auto& evidence : main_actor_pass_gate_writer_evidence) {
        if (evidence.activity_changes == 0U
            && evidence.attribute_changes == 0U) {
            evidence.instruction = *writer;
            if (activity) {
                ++evidence.activity_changes;
            } else {
                ++evidence.attribute_changes;
            }
            return;
        }
        if (evidence.instruction != *writer) {
            continue;
        }
        if (activity) {
            ++evidence.activity_changes;
        } else {
            ++evidence.attribute_changes;
        }
        return;
    }
    if (activity) {
        ++unclassified_main_actor_pass_activity_gate_writers;
    } else {
        ++unclassified_main_actor_pass_attribute_gate_writers;
    }
}

void NativeGameplayBootstrapShadow::observe_actor_dispatch_entry(
    const RoutineEntry& entry,
    const devils_crush::native_runtime::GameStateView& state) noexcept
{
    ++actor_dispatch_entries;
    const bool belongs_to_main_actor_pass = active_main_actor_pass.has_value();
    if (pending_actor_queue_filing) {
        // C520 should enter C606 before another C52C dispatch. Do not
        // let an incomplete observation cross that proven boundary.
        ++unpaired_actor_dispatch_completions;
        pending_actor_queue_filing.reset();
    }
    if (pending_actor_queue_filer) {
        // A C606 call must complete before the next C52C dispatch. Its
        // pending oracle cannot be paired across that ordering boundary.
        ++superseded_actor_queue_filer_predictions;
        pending_actor_queue_filer.reset();
    }

    const auto actor = state.actor(entry.registers.x);
    if (!actor) {
        ++unavailable_actor_dispatch_entries;
        return;
    }
    try {
        if (pending_actor_dispatches.size()
            == kMaximumPendingActorDispatches) {
            pending_actor_dispatches.erase(
                pending_actor_dispatches.begin());
            ++dropped_actor_dispatch_entries;
        }
        pending_actor_dispatches.push_back({
            .entry_absolute_master_cycle = entry.absolute_master_cycle,
            .slot = actor->index,
            .activity_flags = actor->activity_flags,
            .attributes = actor->attributes,
            .belongs_to_main_actor_pass = belongs_to_main_actor_pass,
            .handler_address = actor->handler_address,
            .actor_before_handler = *actor,
        });
    } catch (...) {
        ++actor_dispatch_capture_failures;
    }
}

void NativeGameplayBootstrapShadow::observe_actor_dispatch_completion(
    const RoutineCall& call,
    const std::span<const std::byte> work_ram) noexcept
{
    const auto pending = std::find_if(
        pending_actor_dispatches.rbegin(),
        pending_actor_dispatches.rend(),
        [&call](const PendingActorDispatch& candidate) {
            return candidate.entry_absolute_master_cycle
                == call.entry.absolute_master_cycle;
        });
    if (pending == pending_actor_dispatches.rend()) {
        ++unmatched_actor_dispatch_completions;
        return;
    }
    const auto captured = *pending;
    pending_actor_dispatches.erase(std::next(pending).base());

    // `return_instruction` identifies the handler's terminating RTS.
    // Routine-call observers expose the post-RTS destination separately
    // in `return_pc`; the C52C BSR must land at C520 before that caller
    // can invoke C606. Comparing the RTS address here would reject every
    // dynamically dispatched handler as a false non-C520 return.
    constexpr std::uint16_t kActorQueueCallPc = 0xC520U;
    if (call.return_pc != kActorQueueCallPc) {
        ++nonqueue_actor_dispatch_returns;
        return;
    }

    // C510's BSR is the C52C invocation that returns at C520. A handler
    // may enter C52C recursively before this outer dispatch finishes, so
    // attributing a C52C *entry* to C510 confuses that legitimate nested
    // work with the pending slot decision. Resolve the decision only at
    // the C520 return boundary: it is after the dynamic handler has
    // unwound and immediately before the matching C606 filing call. The
    // C510's typed scan run predicts both the contiguous skipped slots
    // and C52C's next target. C52C is still resolved at its C520 return
    // so nested handlers cannot be mistaken for this outer dispatch.
    if (captured.belongs_to_main_actor_pass) {
        if (active_main_actor_pass
            && active_main_actor_pass->pending_decision) {
            auto decision =
                *active_main_actor_pass->pending_decision;
            active_main_actor_pass->pending_decision.reset();
            if (decision.skipped_actor_count != 0U) {
                ++native_main_actor_pass_scan_realignments;
                native_main_actor_pass_skipped_slots
                    += decision.skipped_actor_count;
            }
            if (!decision.dispatch_expected
                || !decision.expected_dispatch_slot
                || *decision.expected_dispatch_slot != captured.slot) {
                ++active_main_actor_pass->mismatched_dispatch_decisions;
                ++active_main_actor_pass->dispatched_when_skip_expected;
                if ((decision.activity_flags & 0x80U) == 0U) {
                    ++active_main_actor_pass
                           ->dispatched_after_inactive_activity_gate;
                } else {
                    ++active_main_actor_pass
                           ->dispatched_after_deferred_attribute_gate;
                }
                if (decision.activity_flags != captured.activity_flags
                    || decision.attributes != captured.attributes) {
                    ++active_main_actor_pass
                           ->dispatched_after_changed_gate_bytes;
                    if (decision.activity_flags
                        != captured.activity_flags) {
                        ++main_actor_pass_changed_activity_gate_bytes;
                        record_main_actor_pass_gate_writer(
                            decision.activity_gate_writer,
                            true);
                    }
                    if (decision.attributes != captured.attributes) {
                        ++main_actor_pass_changed_attribute_gate_bytes;
                        record_main_actor_pass_gate_writer(
                            decision.attribute_gate_writer,
                            false);
                    }
                } else {
                    ++active_main_actor_pass
                           ->dispatched_after_unchanged_gate_bytes;
                }
            } else {
                ++active_main_actor_pass->matched_dispatch_decisions;
            }
        } else {
            ++mismatched_main_actor_pass_dispatch_decisions;
            ++main_actor_pass_dispatches_without_pending_decision;
        }
    }
    if (call.output_registers.x != captured.slot) {
        ++actor_dispatch_output_slot_mismatches;
        return;
    }

    const devils_crush::native_runtime::GameStateView state{work_ram};
    const auto actor = state.actor(captured.slot);
    if (!actor) {
        ++unavailable_actor_dispatch_outputs;
        return;
    }
    const auto behavior_changes = devils_crush::native_runtime::
        capture_actor_behavior_state_changes(
            captured.actor_before_handler, *actor);
    if (!behavior_changes) {
        ++unavailable_actor_dispatch_state_transitions;
    } else {
        record_actor_dispatch_handler_state_transition(
            captured.handler_address, *behavior_changes);
    }

    if (pending_actor_queue_filing) {
        ++unpaired_actor_dispatch_completions;
    }
    pending_actor_queue_filing = {
        .slot = captured.slot,
        .attributes_after_handler = actor->attributes,
        .handler_address = captured.handler_address,
        .belongs_to_main_actor_pass = captured.belongs_to_main_actor_pass,
    };
    ++completed_actor_dispatches;
}

void NativeGameplayBootstrapShadow::observe_actor_queue_filer_entry(
    const RoutineEntry& entry,
    const devils_crush::native_runtime::GameStateView& state) noexcept
{
    ++actor_queue_filer_entries;
    if (!pending_actor_queue_filing) {
        ++unpaired_actor_queue_filer_entries;
        return;
    }

    const auto pending = *pending_actor_queue_filing;
    pending_actor_queue_filing.reset();
    const auto actor = state.actor(pending.slot);
    if (!actor) {
        ++unavailable_actor_queue_filer_inputs;
        return;
    }
    if (entry.registers.x != pending.slot
        || actor->attributes != pending.attributes_after_handler) {
        ++mismatched_actor_dispatch_queue_handoffs;
        record_actor_dispatch_handler_handoff(
            pending.handler_address, false);
        return;
    }
    ++matched_actor_dispatch_queue_handoffs;
    record_actor_dispatch_handler_handoff(pending.handler_address, true);

    const auto queues = devils_crush::native_runtime::
        capture_actor_presentation_queue_state(state);
    if (!queues) {
        ++unavailable_actor_queue_filer_queue_inputs;
        return;
    }
    pending_actor_queue_filer = {
        .dispatch = pending,
        .queues_before = *queues,
        .attributes_before_filing = actor->attributes,
    };
    ++actor_queue_filer_predictions;
}

void NativeGameplayBootstrapShadow::observe_actor_queue_filer_completion(
    const RoutineCall&,
    const std::span<const std::byte> work_ram) noexcept
{
    if (!pending_actor_queue_filer) {
        return;
    }
    const auto pending = *pending_actor_queue_filer;
    pending_actor_queue_filer.reset();

    const devils_crush::native_runtime::GameStateView state{work_ram};
    const auto actor = state.actor(pending.dispatch.slot);
    const auto queues = devils_crush::native_runtime::
        capture_actor_presentation_queue_state(state);
    if (!actor || !queues) {
        ++unavailable_actor_queue_filer_queue_outputs;
        return;
    }
    const devils_crush::native_runtime::ActorPostHandlerPresentation
        presentation{
            .slot = pending.dispatch.slot,
            .attributes_before_prequeue_presentation_refresh =
                pending.attributes_before_filing,
            .attributes_after_prequeue_presentation_refresh = actor->attributes,
        };
    const auto expected = devils_crush::native_runtime::
        file_completed_actor_for_presentation(pending.queues_before, presentation);
    if (pending.dispatch.belongs_to_main_actor_pass
        && active_main_actor_pass) {
        if (active_main_actor_pass->model.presentation_queues()
            != pending.queues_before) {
            ++active_main_actor_pass->queue_chain_mismatches;
        }
        const auto modeled = active_main_actor_pass->model.complete_handler(
            presentation);
        if (!modeled || modeled->state != expected.state) {
            ++active_main_actor_pass->queue_chain_mismatches;
        } else {
            ++active_main_actor_pass->completed_queue_filings;
        }
    }
    if (expected.state == *queues) {
        ++matched_actor_queue_filer_predictions;
    } else {
        ++mismatched_actor_queue_filer_predictions;
    }
}

void NativeGameplayBootstrapShadow::record_actor_dispatch_handler_handoff(
    const std::uint16_t handler_address,
    const bool matches) noexcept
{
    for (auto& evidence : actor_dispatch_handler_evidence) {
        if (actor_dispatch_handler_evidence_active(evidence)
            && evidence.handler_address == handler_address) {
            if (matches) {
                ++evidence.matching_handoffs;
            } else {
                ++evidence.mismatched_handoffs;
            }
            return;
        }
    }
    for (auto& evidence : actor_dispatch_handler_evidence) {
        if (!actor_dispatch_handler_evidence_active(evidence)) {
            evidence.handler_address = handler_address;
            if (matches) {
                ++evidence.matching_handoffs;
            } else {
                ++evidence.mismatched_handoffs;
            }
            return;
        }
    }
    ++unclassified_actor_dispatch_handler_handoffs;
}

[[nodiscard]] bool NativeGameplayBootstrapShadow::actor_dispatch_handler_evidence_active(
    const ActorDispatchHandlerEvidence& evidence) noexcept
{
    return evidence.matching_handoffs != 0U
        || evidence.mismatched_handoffs != 0U
        || evidence.completed_handler_states != 0U;
}

void NativeGameplayBootstrapShadow::record_actor_dispatch_handler_state_transition(
    const std::uint16_t handler_address,
    const devils_crush::native_runtime::ActorBehaviorStateChanges& changes) noexcept
{
    for (auto& evidence : actor_dispatch_handler_evidence) {
        if (actor_dispatch_handler_evidence_active(evidence)
            && evidence.handler_address == handler_address) {
            ++evidence.completed_handler_states;
            evidence.changed_activity_flags += changes.activity_flags ? 1U : 0U;
            evidence.changed_attributes += changes.attributes ? 1U : 0U;
            evidence.changed_behavior_counter += changes.behavior_counter ? 1U : 0U;
            evidence.changed_handler_address += changes.handler_address ? 1U : 0U;
            evidence.changed_motion +=
                changes.horizontal_motion || changes.vertical_motion ? 1U : 0U;
            evidence.changed_collision_state += changes.collision_state ? 1U : 0U;
            return;
        }
    }
    for (auto& evidence : actor_dispatch_handler_evidence) {
        if (!actor_dispatch_handler_evidence_active(evidence)) {
            evidence.handler_address = handler_address;
            evidence.completed_handler_states = 1U;
            evidence.changed_activity_flags = changes.activity_flags ? 1U : 0U;
            evidence.changed_attributes = changes.attributes ? 1U : 0U;
            evidence.changed_behavior_counter = changes.behavior_counter ? 1U : 0U;
            evidence.changed_handler_address = changes.handler_address ? 1U : 0U;
            evidence.changed_motion =
                changes.horizontal_motion || changes.vertical_motion ? 1U : 0U;
            evidence.changed_collision_state = changes.collision_state ? 1U : 0U;
            return;
        }
    }
    ++unclassified_actor_dispatch_state_transitions;
}

} // namespace devils_crush::app::host
