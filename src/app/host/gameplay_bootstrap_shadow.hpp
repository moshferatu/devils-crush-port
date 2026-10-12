// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

// A read-only bridge from the live emulator to the human-readable gameplay
// runtime. It establishes that the native runtime can accept real, completed
// frame-boundary state and run one candidate frame without granting it frame
// ownership. Keeping this as a fresh bootstrap for each observation avoids
// carrying emulated state across an as-yet-unported gameplay transition.
struct NativeGameplayBootstrapShadow {
    struct DecodedPlayfieldMap {
        devils_crush::emulation::PlayfieldCollisionSource source;
        devils_crush::native_runtime::CollisionMapAsset map;
    };

    // A clear candidate is advanced entirely by readable native code. Compare
    // its completed ball state against the next emulator frame boundary before
    // considering any transfer of frame ownership. Contacts deliberately do
    // not install a prediction because their response owner is still split.
    struct NativeRuntimeFramePrediction {
        devils_crush::native_runtime::BallState ball;
        // The source scheduler's typed owner for the next boundary. Keep it
        // with a prediction so a frame that never reaches DC95 can be
        // attributed to a real owner rather than mistaken for failed physics.
        devils_crush::native_runtime::BallFrameSchedule schedule;
    };

    struct NativeEpochActorHandlerCount {
        std::uint16_t handler_address = 0U;
        std::uint64_t entries = 0U;
    };

    // This stays separate from the candidate runtime itself: C52C observes
    // source handler dispatches after the candidate's DC95 seed and before the
    // next comparison boundary. It identifies the live actor behaviors that
    // correlate with a persistent-state divergence without giving the shadow
    // any actor ownership.
    struct NativeEpochRuntimeChain {
        devils_crush::native_runtime::NativeGameplayRuntime runtime;
        std::array<NativeEpochActorHandlerCount, 8U> source_actor_handlers{};
        std::uint64_t unclassified_source_actor_handler_entries = 0U;
    };

    struct NativeEpochActorHandlerEvidence {
        std::uint16_t handler_address = 0U;
        std::uint64_t matching_entries = 0U;
        std::uint64_t mismatched_entries = 0U;
    };

    // Facts gathered from the emulator between two frame boundaries. They are
    // not native predictions: they classify why an otherwise-clear readable
    // candidate diverged before the next boundary.
    struct ActualFrameCollisionFacts {
        bool actor_contact = false;
        bool solid_playfield_contact = false;
        std::uint8_t motion_substep_split_entries = 0U;
        std::uint8_t position_integration_entries = 0U;
        std::uint8_t motion_damping_entries = 0U;
    };

    // C52C dispatches one dynamic actor handler, then C520 immediately calls
    // C606 to file that handler's final presentation attributes. This is a
    // narrower, regularly active boundary than C539: it can be verified on
    // the normal C510 walk without claiming the scheduler or a handler.
    struct PendingActorDispatch {
        std::uint64_t entry_absolute_master_cycle = 0U;
        std::uint8_t slot = 0U;
        // Raw C52C gate bytes. A native C510 scan may skip several slots
        // before it hands control back at this dispatch, so these describe
        // the dispatch target rather than necessarily the scan-run entry.
        std::uint8_t activity_flags = 0U;
        std::uint8_t attributes = 0U;
        // True only when this dispatch belongs to the C510 pass currently
        // bracketed by the shadow. Dynamic handlers remain emulator-owned;
        // this bit merely prevents an unrelated C606 call from being folded
        // into that pass's aggregate queue prediction.
        bool belongs_to_main_actor_pass = false;
        // This is the vector selected at C52C entry, before the dynamic
        // handler is allowed to retarget its own actor slot.
        std::uint16_t handler_address = 0U;
        // The named actor-domain state at the C52C boundary. Its eventual
        // same-slot transition is evidence for selecting a behavior family;
        // it does not give this observer permission to update that family.
        devils_crush::native_runtime::GameStateView::ActorSlot actor_before_handler;
    };

    struct PendingActorQueueFiling {
        std::uint8_t slot = 0U;
        std::uint8_t attributes_after_handler = 0U;
        std::uint16_t handler_address = 0U;
        bool belongs_to_main_actor_pass = false;
    };

    // C52C has returned to C520 and C606 is about to file the completed
    // actor. Retain the queue state from this precise entry boundary so the
    // original C606 call can be checked against the readable model after its
    // optional F605 presentation refresh. This is observation-only.
    struct PendingActorQueueFiler {
        PendingActorQueueFiling dispatch;
        devils_crush::native_runtime::ActorPresentationQueueState queues_before;
        std::uint8_t attributes_before_filing = 0U;
    };

    struct PendingMainActorPassDecision {
        std::uint8_t slot = 0U;
        std::uint8_t activity_flags = 0U;
        std::uint8_t attributes = 0U;
        std::optional<std::uint8_t> expected_dispatch_slot;
        std::size_t skipped_actor_count = 0U;
        // The work-RAM write observer records the last instruction to touch
        // either byte while this C510 scan-run entry is live. A C510-to-C52C
        // disagreement is actionable only when the recovered scan target
        // differs from C52C's eventual dispatch target.
        std::optional<RoutineKey> activity_gate_writer;
        std::optional<RoutineKey> attribute_gate_writer;
        bool dispatch_expected = false;
    };

    struct ActorGateWriteEvidence {
        RoutineKey instruction;
        std::uint64_t activity_changes = 0U;
        std::uint64_t attribute_changes = 0U;
    };

    // C510 is the re-entered head of the complete 128-slot main actor scan.
    // Capture its X=0 arrival after C4DE's preamble has established the
    // zeroed presentation queues, then sample every C510 scan-run entry. A
    // native run can skip several slots before it leaves at C51E; C52C's own
    // entry therefore supplies the authoritative dispatch-slot snapshot.
    // This remains observation-only.
    struct PendingMainActorPass {
        std::optional<PendingMainActorPassDecision> pending_decision;
        devils_crush::native_runtime::MainActorPass model;
        std::uint64_t matched_dispatch_decisions = 0U;
        std::uint64_t mismatched_dispatch_decisions = 0U;
        std::uint64_t dispatched_when_skip_expected = 0U;
        std::uint64_t dispatched_for_other_slot = 0U;
        std::uint64_t dispatched_after_inactive_activity_gate = 0U;
        std::uint64_t dispatched_after_deferred_attribute_gate = 0U;
        std::uint64_t dispatched_after_changed_gate_bytes = 0U;
        std::uint64_t dispatched_after_unchanged_gate_bytes = 0U;
        std::uint64_t completed_queue_filings = 0U;
        std::uint64_t queue_chain_mismatches = 0U;
    };

    struct ActorDispatchHandlerEvidence {
        std::uint16_t handler_address = 0U;
        std::uint64_t matching_handoffs = 0U;
        std::uint64_t mismatched_handoffs = 0U;
        std::uint64_t completed_handler_states = 0U;
        std::uint64_t changed_activity_flags = 0U;
        std::uint64_t changed_attributes = 0U;
        std::uint64_t changed_behavior_counter = 0U;
        std::uint64_t changed_handler_address = 0U;
        std::uint64_t changed_motion = 0U;
        std::uint64_t changed_collision_state = 0U;
    };

    void observe_playfield_collision_source(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram,
        const std::span<const std::byte> rom) noexcept
    {
        constexpr RoutineKey kPlayfieldProbe{
            .physical_bank = 0x01U,
            .logical_pc = 0xCD74U,
        };
        if (entry.routine != kPlayfieldProbe) {
            return;
        }

        if (active_collision_sweep) {
            active_collision_sweep->reached_playfield_probe = true;
            active_collision_sweep->playfield_probe_entry =
                entry.absolute_master_cycle;
        }

        ++playfield_probe_entries;
        try {
            const auto source = devils_crush::emulation::
                capture_playfield_collision_source(entry.registers, work_ram);
            if (!source) {
                ++unavailable_playfield_sources;
                return;
            }
            if (std::ranges::any_of(
                    playfield_maps,
                    [&source](const DecodedPlayfieldMap& existing) {
                        return existing.source == *source;
                    })) {
                ++reused_playfield_maps;
                return;
            }

            const devils_crush::native_runtime::MappedRomView mapped_rom(
                rom, source->mpr);
            const auto map = devils_crush::native_runtime::
                decode_main_playfield_collision_map(mapped_rom, source->tables);
            if (!map || !map->view().valid()) {
                ++rejected_playfield_maps;
                return;
            }
            playfield_maps.push_back({
                .source = *source,
                .map = std::move(*map),
            });
            ++decoded_playfield_maps;
        } catch (...) {
            // The shadow never changes a live session's correctness. Allocation
            // and asset-decoding failures stay visible in its shutdown report.
            ++playfield_capture_failures;
        }
    }

    // C539 is the composite actor scheduler entry. Capture only its decoded
    // pass input here: behavior execution and C606 presentation filing remain
    // emulator-owned until every handler boundary can be replayed strictly.
    void observe_short_actor_pass_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr RoutineKey kShortActorPass{
            .physical_bank = 0x01U,
            .logical_pc = 0xC539U,
        };
        if (entry.routine != kShortActorPass) {
            return;
        }

        ++short_actor_pass_entries;
        try {
            const devils_crush::native_runtime::GameStateView state{work_ram};
            const devils_crush::native_runtime::ActorSchedulerView scheduler{
                state};
            const auto capture = scheduler.capture_short_actor_pass();
            if (!capture) {
                ++unavailable_short_actor_pass_captures;
                return;
            }
            ++captured_short_actor_passes;
            scheduled_short_actor_pass_actors +=
                capture->scheduled_actors.size();
            if (capture->seed.gate.frozen) {
                ++frozen_short_actor_passes;
            }
            if (capture->scheduled_actors.empty()) {
                ++empty_short_actor_passes;
            }
        } catch (...) {
            // The observer must remain incapable of changing a live frame.
            ++short_actor_pass_capture_failures;
        }
    }

    // The short pass is reached by two direct JSR sites. Keep their entries
    // separate from the C539 snapshot so an otherwise quiet report can say
    // whether the session simply never selected this scheduler variant.
    void observe_short_actor_pass_call_site(const RoutineEntry& entry) noexcept
    {
        constexpr RoutineKey kMainLoopCaller{
            .physical_bank = 0x01U,
            .logical_pc = 0xC1D3U,
        };
        constexpr RoutineKey kTimerCaller{
            .physical_bank = 0x00U,
            .logical_pc = 0xEEC1U,
        };
        if (entry.routine == kMainLoopCaller) {
            ++short_actor_pass_main_loop_call_sites;
        } else if (entry.routine == kTimerCaller) {
            ++short_actor_pass_timer_call_sites;
        }
    }

    void observe_motion_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram,
        const std::span<const std::byte> rom) noexcept
    {
        constexpr RoutineKey kMotionSubstepSplit{
            .physical_bank = 0x01U,
            .logical_pc = 0xDC95U,
        };
        constexpr RoutineKey kPositionIntegration{
            .physical_bank = 0x00U,
            .logical_pc = 0xF1B4U,
        };
        constexpr RoutineKey kMotionDamping{
            .physical_bank = 0x01U,
            .logical_pc = 0xDD09U,
        };
        constexpr RoutineKey kCollisionSweep{
            .physical_bank = 0x01U,
            .logical_pc = 0xCB2AU,
        };
        constexpr RoutineKey kContactResponse{
            .physical_bank = 0x01U,
            .logical_pc = 0xCF33U,
        };
        constexpr RoutineKey kPositionRetry{
            .physical_bank = 0x01U,
            .logical_pc = 0xCDF8U,
        };
        constexpr RoutineKey kActorDispatch{
            .physical_bank = 0x01U,
            .logical_pc = 0xC52CU,
        };
        constexpr RoutineKey kActorQueueFiler{
            .physical_bank = 0x01U,
            .logical_pc = 0xC606U,
        };
        constexpr RoutineKey kMainActorPass{
            .physical_bank = 0x01U,
            .logical_pc = 0xC510U,
        };
        constexpr RoutineKey kSingleActorProbe{
            .physical_bank = 0x01U,
            .logical_pc = 0xCBABU,
        };
        constexpr RoutineKey kActorCollisionScanner{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC1AU,
        };
        constexpr RoutineKey kCollisionMaskLookup{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC98U,
        };
        constexpr RoutineKey kSingleActorProbeResult{
            .physical_bank = 0x01U,
            .logical_pc = 0xCBC2U,
        };
        constexpr RoutineKey kConfirmedActorContact{
            .physical_bank = 0x01U,
            .logical_pc = 0xCBCFU,
        };
        if ((entry.routine != kMotionSubstepSplit
             && entry.routine != kPositionIntegration
             && entry.routine != kMotionDamping
             && entry.routine != kCollisionSweep
             && entry.routine != kContactResponse
             && entry.routine != kPositionRetry
             && entry.routine != kActorDispatch
             && entry.routine != kActorQueueFiler
             && entry.routine != kMainActorPass
             && entry.routine != kSingleActorProbe
             && entry.routine != kActorCollisionScanner
             && entry.routine != kCollisionMaskLookup
             && entry.routine != kSingleActorProbeResult
             && entry.routine != kConfirmedActorContact)
            || (entry.routine != kActorDispatch
                && entry.routine != kActorQueueFiler
                && entry.routine != kMainActorPass
                && entry.routine != kSingleActorProbe
                && entry.routine != kActorCollisionScanner
                && entry.routine != kCollisionMaskLookup
                && entry.routine != kSingleActorProbeResult
                && entry.routine != kConfirmedActorContact
                && entry.registers.x != 0U)) {
            return;
        }

        const devils_crush::native_runtime::GameStateView state{work_ram};
        if (entry.routine == kMainActorPass) {
            observe_main_actor_pass_entry(entry, state);
            return;
        }
        if (entry.routine == kActorDispatch) {
            observe_native_epoch_actor_handler_dispatch(state, entry.registers.x);
            observe_actor_dispatch_entry(entry, state);
            return;
        }
        if (entry.routine == kActorQueueFiler) {
            observe_actor_queue_filer_entry(entry, state);
            return;
        }
        if (entry.routine == kSingleActorProbe) {
            begin_single_slot_actor_collision_probe(entry);
            return;
        }
        if (entry.routine == kActorCollisionScanner) {
            observe_single_slot_actor_collision_scan(entry, state, rom);
            return;
        }
        if (entry.routine == kCollisionMaskLookup) {
            observe_single_slot_actor_collision_mask_stage(entry, state, rom);
            return;
        }
        if (entry.routine == kSingleActorProbeResult) {
            observe_single_slot_actor_collision_result(entry);
            return;
        }
        if (entry.routine == kConfirmedActorContact) {
            observe_single_slot_confirmed_actor_contact();
            return;
        }

        if (entry.routine == kMotionSubstepSplit) {
            ++actual_frame_collision_facts.motion_substep_split_entries;
        }

        const auto ball = state.valid()
            ? devils_crush::native_runtime::capture_active_ball(state)
            : std::nullopt;
        if (!ball) {
            ++unavailable_motion_routine_seeds;
            return;
        }

        if (entry.routine == kMotionSubstepSplit) {
            observe_native_epoch_candidate_entry(state, ball->state);
            observe_collision_loop_split_entry(entry, ball->state);
            return;
        }
        if (entry.routine == kCollisionSweep) {
            observe_collision_sweep_entry(entry, state, rom);
            return;
        }
        if (entry.routine == kContactResponse) {
            observe_playfield_contact_response_entry(state);
            return;
        }
        if (entry.routine == kPositionRetry) {
            observe_collision_retry_entry(entry, state, work_ram);
            return;
        }

        std::optional<devils_crush::native_runtime::BallMotionTuning> tuning;
        std::optional<devils_crush::native_runtime::BallMotionIncrement>
            substep_motion;
        if (entry.routine == kPositionIntegration) {
            ++actual_frame_collision_facts.position_integration_entries;
            substep_motion = state.ball_substep_motion();
            if (!substep_motion) {
                ++unavailable_motion_routine_seeds;
                return;
            }
            observe_collision_loop_position_entry(
                entry, ball->state, *substep_motion);
        } else {
            ++actual_frame_collision_facts.motion_damping_entries;
            tuning = state.ball_motion_tuning();
            if (!tuning) {
                ++unavailable_motion_routine_seeds;
                return;
            }
            observe_collision_loop_damping_entry(
                entry, ball->state, *tuning);
        }

        try {
            if (motion_routine_seeds.size() == kMaximumMotionRoutineSeeds) {
                motion_routine_seeds.erase(motion_routine_seeds.begin());
                ++dropped_motion_routine_seeds;
            }
            motion_routine_seeds.push_back({
                .routine = entry.routine,
                .entry_absolute_master_cycle = entry.absolute_master_cycle,
                .ball = ball->state,
                .tuning = tuning,
                .substep_motion = substep_motion,
            });
        } catch (...) {
            ++motion_routine_capture_failures;
        }
    }

    void observe_motion_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept
    {
        const auto seed = std::find_if(
            motion_routine_seeds.rbegin(),
            motion_routine_seeds.rend(),
            [&call](const MotionRoutineSeed& candidate) {
                return candidate.routine == call.entry.routine
                    && candidate.entry_absolute_master_cycle
                        == call.entry.absolute_master_cycle;
            });
        if (seed == motion_routine_seeds.rend()) {
            ++unmatched_motion_routine_completions;
            return;
        }
        const auto captured = *seed;
        motion_routine_seeds.erase(std::next(seed).base());

        const devils_crush::native_runtime::GameStateView state{work_ram};
        const auto observed_ball = state.valid()
            ? devils_crush::native_runtime::capture_active_ball(state)
            : std::nullopt;
        if (!observed_ball) {
            ++unavailable_motion_routine_outputs;
            return;
        }

        constexpr RoutineKey kPositionIntegration{
            .physical_bank = 0x00U,
            .logical_pc = 0xF1B4U,
        };
        if (captured.routine == kPositionIntegration) {
            if (!captured.substep_motion) {
                ++rejected_position_integration_predictions;
                return;
            }
            ++position_integration_predictions;
            if (devils_crush::native_runtime::advance_ball_motion_substep(
                    captured.ball, *captured.substep_motion).state
                == observed_ball->state) {
                ++position_integration_matches;
            } else {
                ++position_integration_mismatches;
            }
            observe_collision_loop_position_completion(
                call, observed_ball->state);
            return;
        }

        if (!captured.tuning) {
            ++rejected_motion_damping_predictions;
            return;
        }
        const auto prediction = devils_crush::native_runtime::
            apply_ball_drag_and_gravity(captured.ball, *captured.tuning);
        if (!prediction) {
            ++rejected_motion_damping_predictions;
            return;
        }
        ++motion_damping_predictions;
        if (prediction->state == observed_ball->state) {
            ++motion_damping_matches;
        } else {
            ++motion_damping_mismatches;
        }
        observe_collision_loop_damping_completion(call, observed_ball->state);
    }

    void observe_main_actor_pass_entry(
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

    void observe_main_actor_pass_gate_write(
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

    void observe_main_actor_pass_completion(
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

    void record_main_actor_pass_gate_writer(
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

    void observe_actor_dispatch_entry(
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

    void observe_actor_dispatch_completion(
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

    void observe_actor_queue_filer_entry(
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

    void observe_actor_queue_filer_completion(
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

    void record_actor_dispatch_handler_handoff(
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

    [[nodiscard]] static bool actor_dispatch_handler_evidence_active(
        const ActorDispatchHandlerEvidence& evidence) noexcept
    {
        return evidence.matching_handoffs != 0U
            || evidence.mismatched_handoffs != 0U
            || evidence.completed_handler_states != 0U;
    }

    void record_actor_dispatch_handler_state_transition(
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

    void observe_gameplay_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr RoutineKey kActorDispatch{
            .physical_bank = 0x01U,
            .logical_pc = 0xC52CU,
        };
        constexpr RoutineKey kActorQueueFiler{
            .physical_bank = 0x01U,
            .logical_pc = 0xC606U,
        };
        constexpr RoutineKey kMainActorPass{
            .physical_bank = 0x01U,
            .logical_pc = 0xC510U,
        };
        constexpr RoutineKey kCachedPositionRestore{
            .physical_bank = 0x00U,
            .logical_pc = 0xF241U,
        };
        constexpr RoutineKey kPlayfieldProbe{
            .physical_bank = 0x01U,
            .logical_pc = 0xCD74U,
        };
        if (call.entry.routine == kCachedPositionRestore) {
            observe_marked_playfield_retry_restore(call, work_ram);
            return;
        }
        if (call.entry.routine == kActorDispatch) {
            observe_actor_dispatch_completion(call, work_ram);
            return;
        }
        if (call.entry.routine == kActorQueueFiler) {
            observe_actor_queue_filer_completion(call, work_ram);
            return;
        }
        if (call.entry.routine == kMainActorPass) {
            observe_main_actor_pass_completion(call, work_ram);
            return;
        }
        if (call.entry.routine == kPlayfieldProbe) {
            observe_playfield_collision_completion(call);
            return;
        }
        observe_motion_completion(call, work_ram);
    }

    void observe(
        const std::span<const std::byte> work_ram,
        const InputFrame& input)
    {
        ++frame_boundaries;
        observe_pending_native_frame_prediction(work_ram, input);
        actual_frame_collision_facts = {};

        const devils_crush::native_runtime::GameStateView state{work_ram};
        if (!state.valid()) {
            ++invalid_work_ram;
            return;
        }
        const auto bootstrap =
            devils_crush::native_runtime::capture_native_gameplay_bootstrap(
                state);
        if (!bootstrap) {
            ++unavailable_bootstraps;
            return;
        }

        devils_crush::native_runtime::NativeGameplayRuntime native_runtime;
        if (!native_runtime.bootstrap(*bootstrap)) {
            ++rejected_bootstraps;
            return;
        }
        const auto* const gameplay = native_runtime.gameplay();
        if (gameplay == nullptr
            || gameplay->physics().ball != bootstrap->ball.state
            || native_runtime.ball_frame_schedule()
                != bootstrap->ball_frame_schedule
            || gameplay->score() != bootstrap->score.state
            || gameplay->table_rule_latches()
                != bootstrap->table_rule_latches) {
            ++seed_mismatches;
            return;
        }
        ++verified_bootstraps;

        const auto tuning = state.ball_motion_tuning();
        if (!tuning) {
            ++unavailable_motion_tuning;
            return;
        }

        if (playfield_maps.empty()) {
            ++waiting_for_playfield_map;
            return;
        }

        if (bootstrap->ball_frame_schedule.owner
            == devils_crush::native_runtime::BallFrameOwner::
                initial_substep_transition) {
            ++native_initial_motion_candidates;
            try {
                const auto initial = native_runtime.advance_initial_ball_motion(
                    *tuning, playfield_maps.back().map.view());
                if (!initial) {
                    ++unavailable_native_initial_motion;
                    return;
                }
                if (!initial->motion.completes_initial_pass()) {
                    if (initial->motion.physics.next_contact) {
                        ++native_initial_motion_contacts;
                    } else if (initial->motion.physics.attempt_cap_exhausted) {
                        ++native_initial_motion_attempt_caps;
                    } else {
                        ++unavailable_native_initial_motion;
                    }
                    return;
                }
                if (initial->next_schedule.owner
                    == devils_crush::native_runtime::BallFrameOwner::
                        normal_motion) {
                    ++native_initial_motion_schedules_normal;
                } else if (initial->returned_to_slot_zero_actor_loop) {
                    ++native_initial_motion_slot_zero_returns;
                } else {
                    ++unavailable_native_initial_motion;
                }
            } catch (...) {
                // The shadow candidate remains isolated from live emulation.
                ++native_initial_motion_failures;
            }
            return;
        }

        if (!bootstrap->ball_frame_schedule.runs_normal_motion()) {
            ++native_runtime_deferred_schedule_frames;
            record_deferred_frame_schedule(bootstrap->ball_frame_schedule.owner);
            if (bootstrap->ball_frame_schedule.owner
                == devils_crush::native_runtime::BallFrameOwner::unrecognized) {
                record_unrecognized_schedule_handler(
                    bootstrap->ball_frame_schedule.source_handler_address);
            }
            return;
        }

        ++verified_clear_physics_inputs;
        try {
            const auto frame = native_runtime.advance_physics_epoch(
                *tuning, playfield_maps.back().map.view());
            if (!frame) {
                ++unavailable_native_runtime_frames;
                return;
            }
            ++native_runtime_frames;
            native_runtime_actor_updates += frame->actor_updates.size();
            native_runtime_audio_events += frame->audio_events.size();
            if (frame->requires_collision_resolution()) {
                ++native_runtime_contact_frames;
            } else {
                ++native_runtime_clear_frames;
                const auto* const advanced_gameplay = native_runtime.gameplay();
                if (advanced_gameplay == nullptr) {
                    ++unavailable_native_runtime_frame_predictions;
                } else {
                    pending_native_runtime_frame_prediction = {
                        .ball = advanced_gameplay->physics().ball,
                        .schedule = bootstrap->ball_frame_schedule,
                    };
                    ++native_runtime_frame_predictions;
                }
            }
        } catch (...) {
            // This candidate stays entirely private to the observer. A
            // transient allocation failure must never affect live emulation.
            ++native_runtime_frame_failures;
        }
    }

    // E970's palette-fade driver calls the native E9B6 interpolator, then
    // advances two packed-colour pointers before it either loops through E976
    // or falls into E994.  This is an observation-only model of that caller
    // tail: the core retains the JSR/RTS and E9B6 retains the interpolation.
    struct PaletteFadeTailPrediction {
        devils_crush::emulation::CpuRegisters registers;
        std::array<std::uint8_t, 4U> pointers{};
        bool loops = false;
    };

    [[nodiscard]] static std::optional<std::uint16_t>
    palette_fade_caller_pc(
        const devils_crush::emulation::RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept
    {
        // On entry the JSR frame is at SP+1/SP+2 in ordinary work RAM.
        if (work_ram.size() < 0x200U) {
            return std::nullopt;
        }
        const auto low_index = static_cast<std::size_t>(
            0x100U
            + static_cast<std::uint8_t>(entry.registers.stack_pointer + 1U));
        const auto high_index = static_cast<std::size_t>(
            0x100U
            + static_cast<std::uint8_t>(entry.registers.stack_pointer + 2U));
        const auto stacked_return = static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(work_ram[low_index])
            | (static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(work_ram[high_index]))
               << 8U));
        return static_cast<std::uint16_t>(stacked_return - 2U);
    }

    [[nodiscard]] static std::uint8_t palette_fade_set_nz(
        const std::uint8_t status,
        const std::uint8_t value) noexcept
    {
        auto result = static_cast<std::uint8_t>(status & 0x7DU);
        if (value == 0U) {
            result = static_cast<std::uint8_t>(result | 0x02U);
        }
        return static_cast<std::uint8_t>(result | (value & 0x80U));
    }

    static void palette_fade_adc(
        devils_crush::emulation::CpuRegisters& registers,
        const std::uint8_t operand) noexcept
    {
        const auto accumulator = registers.accumulator;
        const auto sum = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(accumulator)
            + static_cast<std::uint16_t>(operand)
            + static_cast<std::uint16_t>(registers.status & 0x01U));
        auto status = static_cast<std::uint8_t>(registers.status & 0x3CU);
        if ((~(accumulator ^ operand) & (accumulator ^ sum) & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x40U);
        }
        if ((sum & 0x100U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        registers.accumulator = static_cast<std::uint8_t>(sum);
        registers.status = palette_fade_set_nz(status, registers.accumulator);
    }

    void observe_palette_fade_block_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr RoutineKey kBlockInterpolator{0x00U, 0xE9B6U};
        if (entry.routine != kBlockInterpolator
            || palette_fade_caller_pc(entry, work_ram) != 0xE976U) {
            return;
        }
        ++palette_fade_block_entries;
        if (!pending_palette_fade_tail) {
            return;
        }
        if (!pending_palette_fade_tail->loops) {
            ++missed_palette_fade_terminal_tails;
            pending_palette_fade_tail.reset();
            return;
        }
        const auto pointers = std::array<std::uint8_t, 4U>{
            std::to_integer<std::uint8_t>(work_ram[0x2DU]),
            std::to_integer<std::uint8_t>(work_ram[0x2EU]),
            std::to_integer<std::uint8_t>(work_ram[0x2FU]),
            std::to_integer<std::uint8_t>(work_ram[0x30U]),
        };
        if (entry.registers == pending_palette_fade_tail->registers
            && pointers == pending_palette_fade_tail->pointers) {
            ++matched_palette_fade_loop_tails;
        } else {
            ++mismatched_palette_fade_loop_tails;
        }
        pending_palette_fade_tail.reset();
    }

    void observe_palette_fade_block_completion(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr RoutineKey kBlockInterpolator{0x00U, 0xE9B6U};
        constexpr RoutineKey kReturn{0x00U, 0xEA76U};
        if (call.entry.routine != kBlockInterpolator
            || call.caller_pc != 0xE976U) {
            return;
        }
        ++palette_fade_block_completions;
        if (call.return_instruction != kReturn || call.return_pc != 0xE979U
            || work_ram.size() < 0x31U
            || (call.output_registers.status & 0x28U) != 0U) {
            ++unavailable_palette_fade_tail_predictions;
            return;
        }
        if (pending_palette_fade_tail) {
            ++superseded_palette_fade_tail_predictions;
        }

        auto predicted = call.output_registers;
        predicted.program_counter = 0xE979U;
        predicted.stack_pointer = static_cast<std::uint8_t>(
            predicted.stack_pointer + 2U);
        auto pointers = std::array<std::uint8_t, 4U>{
            std::to_integer<std::uint8_t>(work_ram[0x2DU]),
            std::to_integer<std::uint8_t>(work_ram[0x2EU]),
            std::to_integer<std::uint8_t>(work_ram[0x2FU]),
            std::to_integer<std::uint8_t>(work_ram[0x30U]),
        };
        // CLC; LDA $2D; ADC #$20; STA $2D; CLA; ADC $2E; STA $2E.
        predicted.status = static_cast<std::uint8_t>(predicted.status & ~0x01U);
        predicted.accumulator = pointers[0U];
        predicted.status = palette_fade_set_nz(
            predicted.status, predicted.accumulator);
        palette_fade_adc(predicted, 0x20U);
        pointers[0U] = predicted.accumulator;
        predicted.accumulator = 0U; // CLA preserves the carry from ADC.
        palette_fade_adc(predicted, pointers[1U]);
        pointers[1U] = predicted.accumulator;
        // CLC; LDA $2F; ADC #$20; STA $2F; CLA; ADC $30; STA $30.
        predicted.status = static_cast<std::uint8_t>(predicted.status & ~0x01U);
        predicted.accumulator = pointers[2U];
        predicted.status = palette_fade_set_nz(
            predicted.status, predicted.accumulator);
        palette_fade_adc(predicted, 0x20U);
        pointers[2U] = predicted.accumulator;
        predicted.accumulator = 0U;
        palette_fade_adc(predicted, pointers[3U]);
        pointers[3U] = predicted.accumulator;
        // DEX; BNE E976.
        predicted.x = static_cast<std::uint8_t>(predicted.x - 1U);
        predicted.status = palette_fade_set_nz(predicted.status, predicted.x);
        const auto loops = predicted.x != 0U;
        if (loops) {
            predicted.program_counter = 0xE9B6U;
            predicted.stack_pointer = static_cast<std::uint8_t>(
                predicted.stack_pointer - 2U);
            ++palette_fade_loop_tail_predictions;
        } else {
            predicted.program_counter = 0xE994U;
            ++palette_fade_terminal_tail_predictions;
        }
        pending_palette_fade_tail = PaletteFadeTailPrediction{
            .registers = predicted,
            .pointers = pointers,
            .loops = loops,
        };
    }

    void observe_palette_fade_terminal_entry(
        const RoutineEntry& entry,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr RoutineKey kTerminal{0x00U, 0xE994U};
        if (entry.routine != kTerminal || !pending_palette_fade_tail) {
            return;
        }
        if (pending_palette_fade_tail->loops) {
            ++unexpected_palette_fade_terminal_entries;
            pending_palette_fade_tail.reset();
            return;
        }
        const auto pointers = std::array<std::uint8_t, 4U>{
            std::to_integer<std::uint8_t>(work_ram[0x2DU]),
            std::to_integer<std::uint8_t>(work_ram[0x2EU]),
            std::to_integer<std::uint8_t>(work_ram[0x2FU]),
            std::to_integer<std::uint8_t>(work_ram[0x30U]),
        };
        if (entry.registers == pending_palette_fade_tail->registers
            && pointers == pending_palette_fade_tail->pointers) {
            ++matched_palette_fade_terminal_tails;
        } else {
            ++mismatched_palette_fade_terminal_tails;
        }
        pending_palette_fade_tail.reset();
    }

    void report() const
    {
        std::cout << "Native gameplay bootstrap shadow: "
                  << verified_bootstraps << " verified seed(s) across "
                  << frame_boundaries << " frame boundary/boundaries";
        if (unavailable_bootstraps != 0U) {
            std::cout << ", " << unavailable_bootstraps
                      << " unavailable";
        }
        if (invalid_work_ram != 0U) {
            std::cout << ", " << invalid_work_ram << " invalid RAM image(s)";
        }
        if (rejected_bootstraps != 0U) {
            std::cout << ", " << rejected_bootstraps
                      << " rejected bootstrap(s)";
        }
        if (seed_mismatches != 0U) {
            std::cout << ", " << seed_mismatches << " seed mismatch(es)";
        }
        if (verified_clear_physics_inputs != 0U) {
            std::cout << "; " << verified_clear_physics_inputs
                      << " frame boundary/boundaries had typed motion tuning "
                         "and a decoded playfield map";
        }
        if (native_runtime_frames != 0U
            || unavailable_native_runtime_frames != 0U
            || native_runtime_frame_failures != 0U) {
            std::cout << "; native gameplay host-boundary epoch shadow: "
                      << native_runtime_clear_frames << " clear completion(s), "
                      << native_runtime_contact_frames
                      << " contact boundary/boundaries from "
                      << native_runtime_frames << " candidate frame(s)";
            if (native_runtime_actor_updates != 0U
                || native_runtime_audio_events != 0U) {
                std::cout << ", " << native_runtime_actor_updates
                          << " actor update(s), " << native_runtime_audio_events
                          << " gameplay audio event(s)";
            }
            if (unavailable_native_runtime_frames != 0U) {
                std::cout << ", " << unavailable_native_runtime_frames
                          << " unavailable candidate frame(s)";
            }
            if (native_runtime_frame_failures != 0U) {
                std::cout << ", " << native_runtime_frame_failures
                          << " candidate failure(s)";
            }
        }
        if (native_epoch_runtime_candidates != 0U
            || native_epoch_runtime_unavailable != 0U) {
            std::cout << "; native gameplay DC95 chain shadow: "
                      << matched_native_epoch_runtime_boundaries << "/"
                      << observed_native_epoch_runtime_boundaries
                      << " completed epoch boundary/boundaries match from "
                      << native_epoch_runtime_candidates
                      << " candidate epoch(s)";
            if (native_epoch_runtime_clear_candidates != 0U
                || native_epoch_runtime_contact_candidates != 0U) {
                std::cout << " (" << native_epoch_runtime_clear_candidates
                          << " clear, "
                          << native_epoch_runtime_contact_candidates
                          << " contact handoff(s))";
            }
            if (native_epoch_runtime_seeds != 0U) {
                std::cout << ", " << native_epoch_runtime_seeds
                          << " seed(s)";
            }
            if (mismatched_native_epoch_runtime_boundaries != 0U) {
                std::cout << ", "
                          << mismatched_native_epoch_runtime_boundaries
                          << " divergent rebootstrap(s)";
                if (native_epoch_actor_handler_evidence[0U].mismatched_entries
                    != 0U) {
                    std::cout << " (source actor handlers";
                    for (const auto& handler : native_epoch_actor_handler_evidence) {
                        if (handler.mismatched_entries == 0U) {
                            continue;
                        }
                        std::cout << ' ' << std::format(
                            "{:04X}={}/{}",
                            handler.handler_address,
                            handler.mismatched_entries,
                            handler.mismatched_entries
                                + handler.matching_entries);
                    }
                    if (unclassified_native_epoch_actor_handler_entries != 0U) {
                        std::cout << " other="
                                  << unclassified_native_epoch_actor_handler_entries;
                    }
                    std::cout << ')';
                }
            }
            if (native_epoch_runtime_unavailable != 0U) {
                std::cout << ", " << native_epoch_runtime_unavailable
                          << " unavailable epoch(s)";
            }
        }
        if (native_runtime_frame_predictions != 0U
            || observed_native_runtime_frame_predictions != 0U
            || unavailable_native_runtime_frame_predictions != 0U) {
            std::cout << "; native gameplay host-boundary diagnostic: "
                      << matched_native_runtime_frame_predictions << "/"
                      << observed_native_runtime_frame_predictions
                      << " clear-frame ball state match(es) from "
                      << native_runtime_frame_predictions
                      << " prediction(s)";
            if (observed_native_runtime_frame_predictions != 0U) {
                std::cout << ", scheduler owner "
                          << matched_native_runtime_frame_prediction_schedule_owners
                          << "/"
                          << observed_native_runtime_frame_predictions
                          << " match(es)";
                if (mismatched_native_runtime_frame_prediction_schedule_owners
                    != 0U) {
                    std::cout << " (departures";
                    for (const auto& owner : prediction_schedule_owners) {
                        if (owner.mismatches == 0U) {
                            continue;
                        }
                        std::cout << ' ' << devils_crush::native_runtime::
                            describe_ball_frame_owner(owner.owner)
                                  << '=' << owner.mismatches;
                    }
                    if (unclassified_prediction_schedule_owners != 0U) {
                        std::cout << " other="
                                  << unclassified_prediction_schedule_owners;
                    }
                    std::cout << ')';
                }
            }
            if (mismatched_native_runtime_frame_predictions != 0U) {
                std::cout << ", "
                          << mismatched_native_runtime_frame_predictions
                          << " mismatch(es) (x="
                          << native_runtime_frame_prediction_position_x_mismatches
                          << ", y="
                          << native_runtime_frame_prediction_position_y_mismatches
                          << ", vx="
                          << native_runtime_frame_prediction_velocity_x_mismatches
                          << ", vy="
                          << native_runtime_frame_prediction_velocity_y_mismatches
                          << ", motion="
                          << native_runtime_frame_prediction_motion_flag_mismatches
                          << ", collision="
                          << native_runtime_frame_prediction_collision_state_mismatches
                          << "; routes actor="
                          << native_runtime_frame_prediction_actor_contact_mismatches
                          << ", playfield="
                          << native_runtime_frame_prediction_playfield_contact_mismatches
                          << ", both="
                          << native_runtime_frame_prediction_both_contact_mismatches
                          << ", no-contact="
                          << native_runtime_frame_prediction_no_contact_mismatches
                          << "; input active="
                          << native_runtime_frame_prediction_input_active_mismatches
                          << ", neutral="
                          << native_runtime_frame_prediction_input_neutral_mismatches
                          << "; motion DC95="
                          << native_runtime_frame_prediction_split_mismatches
                          << " (one="
                          << native_runtime_frame_prediction_single_split_mismatches
                          << ", many="
                          << native_runtime_frame_prediction_multiple_split_mismatches
                          << ')'
                          << ", F1B4="
                          << native_runtime_frame_prediction_position_mismatches
                          << ", DD09="
                          << native_runtime_frame_prediction_damping_mismatches
                          << " (zero="
                          << native_runtime_frame_prediction_no_damping_mismatches
                          << ", one="
                          << native_runtime_frame_prediction_single_damping_mismatches
                          << ", many="
                          << native_runtime_frame_prediction_multiple_damping_mismatches
                          << ')'
                          << ", no DC95="
                          << native_runtime_frame_prediction_no_split_mismatches
                          << ')';
            }
            if (unavailable_native_runtime_frame_predictions != 0U) {
                std::cout << ", "
                          << unavailable_native_runtime_frame_predictions
                          << " unavailable output(s)";
            }
            if (pending_native_runtime_frame_prediction) {
                std::cout << ", 1 awaiting frame boundary";
            }
        }
        if (native_runtime_deferred_schedule_frames != 0U) {
            std::cout << "; native gameplay deferred schedule frame(s): "
                      << native_runtime_deferred_schedule_frames;
            for (const auto& owner : deferred_frame_schedule_owners) {
                if (owner.mismatches == 0U) {
                    continue;
                }
                std::cout << ' ' << devils_crush::native_runtime::
                    describe_ball_frame_owner(owner.owner)
                          << '=' << owner.mismatches;
            }
            if (unclassified_deferred_frame_schedule_owners != 0U) {
                std::cout << " other="
                          << unclassified_deferred_frame_schedule_owners;
            }
        }
        if (native_initial_motion_candidates != 0U) {
            std::cout << "; native initial C6C2 shadow: "
                      << native_initial_motion_schedules_normal
                      << " tails into C721, "
                      << native_initial_motion_slot_zero_returns
                      << " returns through slot-zero actor filing, "
                      << native_initial_motion_contacts
                      << " contact(s), " << native_initial_motion_attempt_caps
                      << " attempt-cap(s), " << unavailable_native_initial_motion
                      << " unavailable";
            if (native_initial_motion_failures != 0U) {
                std::cout << ", " << native_initial_motion_failures
                          << " failure(s)";
            }
        }
        if (std::ranges::any_of(
                deferred_frame_schedule_owners,
                [](const FrameScheduleOwnerCount& owner) {
                    return owner.owner
                            == devils_crush::native_runtime::BallFrameOwner::unrecognized
                        && owner.mismatches != 0U;
                })) {
            std::cout << "; unrecognized schedule handler(s)";
            for (const auto& handler : unrecognized_schedule_handlers) {
                if (handler.frames == 0U) {
                    continue;
                }
                std::cout << ' ' << std::format(
                    "{:04X}={}", handler.handler_address, handler.frames);
            }
            if (unclassified_schedule_handlers != 0U) {
                std::cout << " other=" << unclassified_schedule_handlers;
            }
        }
        report_motion_routine_shadow();
        report_collision_loop_shadow();
        if (palette_fade_block_entries != 0U
            || palette_fade_block_completions != 0U) {
            std::cout << "; palette-fade caller shadow: "
                      << matched_palette_fade_loop_tails << "/"
                      << palette_fade_loop_tail_predictions
                      << " loop-tail reentry/reentries match, "
                      << matched_palette_fade_terminal_tails << "/"
                      << palette_fade_terminal_tail_predictions
                      << " terminal tail(s) match from "
                      << palette_fade_block_completions
                      << " E9B6 completion(s)";
            if (mismatched_palette_fade_loop_tails != 0U
                || mismatched_palette_fade_terminal_tails != 0U
                || unavailable_palette_fade_tail_predictions != 0U
                || superseded_palette_fade_tail_predictions != 0U
                || missed_palette_fade_terminal_tails != 0U
                || unexpected_palette_fade_terminal_entries != 0U) {
                std::cout << " (";
                bool separated = false;
                const auto append = [&separated](
                                        const std::uint64_t count,
                                        const std::string_view label) {
                    if (count == 0U) {
                        return;
                    }
                    if (separated) {
                        std::cout << ", ";
                    }
                    std::cout << count << ' ' << label;
                    separated = true;
                };
                append(mismatched_palette_fade_loop_tails,
                       "loop mismatch(es)");
                append(mismatched_palette_fade_terminal_tails,
                       "terminal mismatch(es)");
                append(unavailable_palette_fade_tail_predictions,
                       "unavailable tail(s)");
                append(superseded_palette_fade_tail_predictions,
                       "superseded tail(s)");
                append(missed_palette_fade_terminal_tails,
                       "missed terminal tail(s)");
                append(unexpected_palette_fade_terminal_entries,
                       "unexpected terminal entry/entries");
                std::cout << ')';
            }
        }
        if (main_actor_pass_entries != 0U) {
            std::cout << "; main actor-pass shadow: "
                      << completed_main_actor_passes << "/"
                      << captured_main_actor_passes
                      << " C510 completion(s), "
                      << matched_main_actor_pass_dispatch_decisions << "/"
                      << (matched_main_actor_pass_dispatch_decisions
                          + mismatched_main_actor_pass_dispatch_decisions)
                      << " C510 scan target(s) matched, "
                      << matched_main_actor_pass_queue_predictions << "/"
                      << main_actor_pass_queue_predictions
                      << " model final queue-state match(es)"
                      << " (" << main_actor_pass_completed_queue_filings
                      << " paired C606 filing(s))";
            if (mismatched_main_actor_pass_dispatch_decisions != 0U
                || mismatched_main_actor_pass_queue_predictions != 0U
                || main_actor_pass_queue_chain_mismatches != 0U
                || unavailable_main_actor_pass_inputs != 0U
                || unavailable_main_actor_pass_outputs != 0U
                || superseded_main_actor_passes != 0U
                || unmatched_main_actor_pass_completions != 0U
                || main_actor_pass_capture_failures != 0U
                || native_main_actor_pass_scan_realignments != 0U) {
                std::cout << " (";
                bool separated = false;
                const auto append = [&separated](
                                        const std::uint64_t count,
                                        const std::string_view label) {
                    if (count == 0U) {
                        return;
                    }
                    if (separated) {
                        std::cout << ", ";
                    }
                    std::cout << count << ' ' << label;
                    separated = true;
                };
                append(mismatched_main_actor_pass_dispatch_decisions,
                       "live dispatch mismatch(es)");
                append(main_actor_pass_dispatched_when_skip_expected,
                       "dispatch(es) after an expected skip");
                append(main_actor_pass_dispatched_for_other_slot,
                       "dispatch(es) for another slot");
                append(main_actor_pass_dispatched_after_inactive_activity_gate,
                       "dispatch(es) after an inactive C510 gate");
                append(main_actor_pass_dispatched_after_deferred_attribute_gate,
                       "dispatch(es) after a deferred C510 attribute gate");
                append(main_actor_pass_dispatched_after_changed_gate_bytes,
                       "skip-dispatch(es) with changed gate bytes");
                append(main_actor_pass_dispatched_after_unchanged_gate_bytes,
                       "skip-dispatch(es) with unchanged gate bytes");
                append(native_main_actor_pass_scan_realignments,
                       "native scan run(s) with skipped slot(s)");
                append(native_main_actor_pass_skipped_slots,
                       "native skipped slot(s)");
                append(main_actor_pass_dispatches_without_pending_decision,
                       "dispatch(es) without a pending decision");
                append(mismatched_main_actor_pass_queue_predictions,
                       "final queue mismatch(es)");
                append(main_actor_pass_queue_chain_mismatches,
                       "intermediate queue-chain mismatch(es)");
                append(unavailable_main_actor_pass_inputs,
                       "unavailable input(s)");
                append(unavailable_main_actor_pass_outputs,
                       "unavailable output(s)");
                append(superseded_main_actor_passes, "superseded pass(es)");
                append(unmatched_main_actor_pass_completions,
                       "unmatched completion(s)");
                append(main_actor_pass_capture_failures,
                       "capture failure(s)");
                std::cout << ')';
            }
            if (main_actor_pass_changed_activity_gate_bytes != 0U
                || main_actor_pass_changed_attribute_gate_bytes != 0U) {
                std::cout << "; main actor-pass gate-write shadow: activity="
                          << main_actor_pass_changed_activity_gate_bytes
                          << ", attributes="
                          << main_actor_pass_changed_attribute_gate_bytes;
                for (const auto& evidence
                     : main_actor_pass_gate_writer_evidence) {
                    if (evidence.activity_changes == 0U
                        && evidence.attribute_changes == 0U) {
                        continue;
                    }
                    std::cout << ' ' << format_routine_key(evidence.instruction)
                              << "=activity:" << evidence.activity_changes
                              << "/attribute:"
                              << evidence.attribute_changes;
                }
                if (main_actor_pass_unobserved_activity_gate_writers != 0U
                    || main_actor_pass_unobserved_attribute_gate_writers
                        != 0U) {
                    std::cout << " unobserved=activity:"
                              << main_actor_pass_unobserved_activity_gate_writers
                              << "/attribute:"
                              << main_actor_pass_unobserved_attribute_gate_writers;
                }
                if (unclassified_main_actor_pass_activity_gate_writers != 0U
                    || unclassified_main_actor_pass_attribute_gate_writers
                        != 0U) {
                    std::cout << " other=activity:"
                              << unclassified_main_actor_pass_activity_gate_writers
                              << "/attribute:"
                              << unclassified_main_actor_pass_attribute_gate_writers;
                }
            }
        }
        if (actor_dispatch_entries != 0U || actor_queue_filer_entries != 0U) {
            std::cout << "; actor-dispatch handoff shadow: "
                      << completed_actor_dispatches
                      << " C52C handler completion(s), "
                      << matched_actor_dispatch_queue_handoffs << "/"
                      << (matched_actor_dispatch_queue_handoffs
                          + mismatched_actor_dispatch_queue_handoffs)
                      << " paired C606 input(s) matched";
            if (mismatched_actor_dispatch_queue_handoffs != 0U) {
                std::cout << ", "
                          << mismatched_actor_dispatch_queue_handoffs
                          << " slot/attribute mismatch(es)";
            }
            if (unpaired_actor_dispatch_completions != 0U
                || unpaired_actor_queue_filer_entries != 0U) {
                std::cout << " (unpaired C52C="
                          << unpaired_actor_dispatch_completions
                          << ", other C606="
                          << unpaired_actor_queue_filer_entries
                          << ')';
            }
            if (nonqueue_actor_dispatch_returns != 0U
                || actor_dispatch_output_slot_mismatches != 0U
                || unavailable_actor_dispatch_entries != 0U
                || unavailable_actor_dispatch_outputs != 0U
                || unavailable_actor_dispatch_state_transitions != 0U
                || unavailable_actor_queue_filer_inputs != 0U
                || unmatched_actor_dispatch_completions != 0U
                || dropped_actor_dispatch_entries != 0U
                || actor_dispatch_capture_failures != 0U) {
                std::cout << " (";
                bool separated = false;
                const auto append = [&separated](
                                        const std::uint64_t count,
                                        const std::string_view label) {
                    if (count == 0U) {
                        return;
                    }
                    if (separated) {
                        std::cout << ", ";
                    }
                    std::cout << count << ' ' << label;
                    separated = true;
                };
                append(nonqueue_actor_dispatch_returns, "non-C520 return(s)");
                append(actor_dispatch_output_slot_mismatches,
                       "C52C output-slot mismatch(es)");
                append(unavailable_actor_dispatch_entries,
                       "unavailable C52C input(s)");
                append(unavailable_actor_dispatch_outputs,
                       "unavailable C52C output(s)");
                append(unavailable_actor_dispatch_state_transitions,
                       "unavailable C52C state transition(s)");
                append(unavailable_actor_queue_filer_inputs,
                       "unavailable C606 input(s)");
                append(unmatched_actor_dispatch_completions,
                       "unmatched C52C completion(s)");
                append(dropped_actor_dispatch_entries,
                       "dropped C52C entry/entries");
                append(actor_dispatch_capture_failures,
                       "C52C capture failure(s)");
                std::cout << ')';
            }
            if (matched_actor_dispatch_queue_handoffs != 0U
                || mismatched_actor_dispatch_queue_handoffs != 0U) {
                std::cout << "; actor-dispatch top handler coverage:";
                std::array<bool, kMaximumActorDispatchHandlerEvidence> reported{};
                for (std::size_t rank = 0U;
                     rank < kReportedActorDispatchHandlerCoverage;
                     ++rank) {
                    std::optional<std::size_t> next;
                    for (std::size_t index = 0U;
                         index < actor_dispatch_handler_evidence.size();
                         ++index) {
                        const auto& evidence = actor_dispatch_handler_evidence[index];
                        const auto count = evidence.matching_handoffs
                            + evidence.mismatched_handoffs;
                        if (reported[index] || count == 0U
                            || (next && count
                                <= actor_dispatch_handler_evidence[*next]
                                       .matching_handoffs
                                    + actor_dispatch_handler_evidence[*next]
                                          .mismatched_handoffs)) {
                            continue;
                        }
                        next = index;
                    }
                    if (!next) {
                        break;
                    }
                    reported[*next] = true;
                    const auto& evidence = actor_dispatch_handler_evidence[*next];
                    std::cout << ' ' << std::format(
                        "{:04X}={}/{}",
                        evidence.handler_address,
                        evidence.matching_handoffs,
                        evidence.matching_handoffs
                            + evidence.mismatched_handoffs);
                }
                if (unclassified_actor_dispatch_handler_handoffs != 0U) {
                    std::cout << " other="
                              << unclassified_actor_dispatch_handler_handoffs;
                }
            }
            if (std::ranges::any_of(
                    actor_dispatch_handler_evidence,
                    [](const ActorDispatchHandlerEvidence& evidence) {
                        return evidence.completed_handler_states != 0U;
                    })) {
                std::cout << "; actor-dispatch state effects:";
                std::array<bool, kMaximumActorDispatchHandlerEvidence> reported{};
                for (std::size_t rank = 0U;
                     rank < kReportedActorDispatchHandlerEffects;
                     ++rank) {
                    std::optional<std::size_t> next;
                    for (std::size_t index = 0U;
                         index < actor_dispatch_handler_evidence.size();
                         ++index) {
                        const auto& evidence = actor_dispatch_handler_evidence[index];
                        if (reported[index]
                            || evidence.completed_handler_states == 0U
                            || (next && evidence.completed_handler_states
                                <= actor_dispatch_handler_evidence[*next]
                                       .completed_handler_states)) {
                            continue;
                        }
                        next = index;
                    }
                    if (!next) {
                        break;
                    }
                    reported[*next] = true;
                    const auto& evidence = actor_dispatch_handler_evidence[*next];
                    std::cout << ' ' << std::format(
                        "{:04X}={}",
                        evidence.handler_address,
                        evidence.completed_handler_states);
                    if (evidence.changed_activity_flags != 0U
                        || evidence.changed_attributes != 0U
                        || evidence.changed_behavior_counter != 0U
                        || evidence.changed_handler_address != 0U
                        || evidence.changed_motion != 0U
                        || evidence.changed_collision_state != 0U) {
                        std::cout << "(activity=" << evidence.changed_activity_flags
                                  << ", attributes=" << evidence.changed_attributes
                                  << ", counter=" << evidence.changed_behavior_counter
                                  << ", vector=" << evidence.changed_handler_address
                                  << ", motion=" << evidence.changed_motion
                                  << ", collision="
                                  << evidence.changed_collision_state << ')';
                    }
                }
                if (unclassified_actor_dispatch_state_transitions != 0U) {
                    std::cout << " other="
                              << unclassified_actor_dispatch_state_transitions;
                }
            }
            if (actor_queue_filer_predictions != 0U
                || unavailable_actor_queue_filer_queue_inputs != 0U) {
                std::cout << "; actor-queue filing shadow: "
                          << matched_actor_queue_filer_predictions << "/"
                          << actor_queue_filer_predictions
                          << " C606 queue-state match(es)";
                if (mismatched_actor_queue_filer_predictions != 0U) {
                    std::cout << ", "
                              << mismatched_actor_queue_filer_predictions
                              << " mismatch(es)";
                }
                if (unavailable_actor_queue_filer_queue_inputs != 0U
                    || unavailable_actor_queue_filer_queue_outputs != 0U
                    || superseded_actor_queue_filer_predictions != 0U) {
                    std::cout << " (";
                    bool separated = false;
                    const auto append = [&separated](
                                            const std::string_view label,
                                            const std::uint64_t count) {
                        if (count == 0U) {
                            return;
                        }
                        if (separated) {
                            std::cout << ", ";
                        }
                        std::cout << count << ' ' << label;
                        separated = true;
                    };
                    append("unavailable input queue(s)",
                        unavailable_actor_queue_filer_queue_inputs);
                    append("unavailable output queue(s)",
                        unavailable_actor_queue_filer_queue_outputs);
                    append("superseded prediction(s)",
                        superseded_actor_queue_filer_predictions);
                    std::cout << ')';
                }
            }
        }
        if (collision_retry_entries != 0U) {
            std::cout << "; collision-retry shadow: "
                      << collision_retry_consuming_entries
                      << " CDF8 entry/entries with CB7E's consume bit set, "
                      << collision_retry_rearming_entries << " with it clear"
                      << " ("
                      << collision_retry_after_marked_playfield_cf33_entries
                      << " after marked playfield CF33, "
                      << collision_retry_after_unmarked_playfield_cf33_entries
                      << " after unmarked signed playfield CF33, "
                      << collision_retry_outside_signed_playfield_cf33_entries
                      << " other)";
        }
        if (marked_playfield_retry_restore_candidates != 0U
            || unavailable_marked_playfield_retry_restore_caches != 0U) {
            std::cout << "; marked CDF8 rollback shadow: "
                      << verified_marked_playfield_retry_restores << "/"
                      << observed_marked_playfield_retry_restores
                      << " F241 cache restore(s) from "
                      << marked_playfield_retry_restore_candidates
                      << " marked clear candidate(s)";
            if (mismatched_marked_playfield_retry_restores != 0U) {
                std::cout << ", "
                          << mismatched_marked_playfield_retry_restores
                          << " mismatch(es)";
            }
            if (unavailable_marked_playfield_retry_restore_caches != 0U) {
                std::cout << ", "
                          << unavailable_marked_playfield_retry_restore_caches
                          << " unavailable cache(s)";
            }
            if (superseded_marked_playfield_retry_restores != 0U) {
                std::cout << ", "
                          << superseded_marked_playfield_retry_restores
                          << " superseded before F241";
            }
            if (pending_marked_playfield_retry_restore) {
                std::cout << ", 1 awaiting F241";
            }
        }
        if (playfield_probe_entries != 0U) {
            std::cout << "; playfield map shadow: " << decoded_playfield_maps
                      << " decoded distinct map(s) from "
                      << playfield_probe_entries << " CD74 probe(s)";
            if (reused_playfield_maps != 0U) {
                std::cout << ", " << reused_playfield_maps << " reused";
            }
            if (unavailable_playfield_sources != 0U) {
                std::cout << ", " << unavailable_playfield_sources
                          << " unavailable source(s)";
            }
            if (rejected_playfield_maps != 0U) {
                std::cout << ", " << rejected_playfield_maps
                          << " rejected map(s)";
            }
            if (playfield_capture_failures != 0U) {
                std::cout << ", " << playfield_capture_failures
                          << " capture failure(s)";
            }
            if (solid_playfield_probes != 0U || clear_playfield_probes != 0U
                || unmatched_playfield_probe_completions != 0U) {
                std::cout << "; playfield-probe shadow: "
                          << solid_playfield_probes << " solid, "
                          << clear_playfield_probes << " clear";
                if (unmatched_playfield_probe_completions != 0U) {
                    std::cout << ", "
                              << unmatched_playfield_probe_completions
                              << " unmatched completion(s)";
                }
            }
        }
        {
            std::cout << "; C539 actor-pass capture: "
                      << captured_short_actor_passes << "/"
                      << short_actor_pass_entries << " coherent input(s), "
                      << scheduled_short_actor_pass_actors
                      << " scheduled actor(s), caller probes C1D3="
                      << short_actor_pass_main_loop_call_sites << " EEC1="
                      << short_actor_pass_timer_call_sites;
            if (frozen_short_actor_passes != 0U) {
                std::cout << ", " << frozen_short_actor_passes
                          << " frozen";
            }
            if (empty_short_actor_passes != 0U) {
                std::cout << ", " << empty_short_actor_passes << " empty";
            }
            if (unavailable_short_actor_pass_captures != 0U) {
                std::cout << ", " << unavailable_short_actor_pass_captures
                          << " unavailable";
            }
            if (short_actor_pass_capture_failures != 0U) {
                std::cout << ", " << short_actor_pass_capture_failures
                          << " capture failure(s)";
            }
        }
        if (waiting_for_playfield_map != 0U) {
            std::cout << "; " << waiting_for_playfield_map
                      << " bootstrap(s) preceded the first playfield map";
        }
        if (unavailable_motion_tuning != 0U) {
            std::cout << "; " << unavailable_motion_tuning
                      << " unavailable motion-tuning snapshot(s)";
        }
        std::cout << ".\n";
    }

    std::uint64_t frame_boundaries = 0U;
    std::uint64_t invalid_work_ram = 0U;
    std::uint64_t unavailable_bootstraps = 0U;
    std::uint64_t rejected_bootstraps = 0U;
    std::uint64_t seed_mismatches = 0U;
    std::uint64_t verified_bootstraps = 0U;
    std::uint64_t unavailable_motion_tuning = 0U;
    std::uint64_t waiting_for_playfield_map = 0U;
    std::optional<PaletteFadeTailPrediction> pending_palette_fade_tail;
    std::uint64_t palette_fade_block_entries = 0U;
    std::uint64_t palette_fade_block_completions = 0U;
    std::uint64_t palette_fade_loop_tail_predictions = 0U;
    std::uint64_t matched_palette_fade_loop_tails = 0U;
    std::uint64_t mismatched_palette_fade_loop_tails = 0U;
    std::uint64_t palette_fade_terminal_tail_predictions = 0U;
    std::uint64_t matched_palette_fade_terminal_tails = 0U;
    std::uint64_t mismatched_palette_fade_terminal_tails = 0U;
    std::uint64_t unavailable_palette_fade_tail_predictions = 0U;
    std::uint64_t superseded_palette_fade_tail_predictions = 0U;
    std::uint64_t missed_palette_fade_terminal_tails = 0U;
    std::uint64_t unexpected_palette_fade_terminal_entries = 0U;
    std::uint64_t verified_clear_physics_inputs = 0U;
    std::uint64_t native_runtime_frames = 0U;
    std::uint64_t native_runtime_clear_frames = 0U;
    std::uint64_t native_runtime_contact_frames = 0U;
    std::uint64_t native_runtime_actor_updates = 0U;
    std::uint64_t native_runtime_audio_events = 0U;
    std::uint64_t native_runtime_deferred_schedule_frames = 0U;
    std::uint64_t native_epoch_runtime_candidates = 0U;
    std::uint64_t native_epoch_runtime_clear_candidates = 0U;
    std::uint64_t native_epoch_runtime_contact_candidates = 0U;
    std::uint64_t native_epoch_runtime_seeds = 0U;
    std::uint64_t observed_native_epoch_runtime_boundaries = 0U;
    std::uint64_t matched_native_epoch_runtime_boundaries = 0U;
    std::uint64_t mismatched_native_epoch_runtime_boundaries = 0U;
    std::uint64_t native_epoch_runtime_unavailable = 0U;
    std::array<NativeEpochActorHandlerEvidence, 16U>
        native_epoch_actor_handler_evidence{};
    std::uint64_t unclassified_native_epoch_actor_handler_entries = 0U;
    ActualFrameCollisionFacts actual_frame_collision_facts;
    std::optional<NativeRuntimeFramePrediction>
        pending_native_runtime_frame_prediction;
    std::uint64_t native_runtime_frame_predictions = 0U;
    std::uint64_t observed_native_runtime_frame_predictions = 0U;
    std::uint64_t matched_native_runtime_frame_predictions = 0U;
    std::uint64_t mismatched_native_runtime_frame_predictions = 0U;
    std::uint64_t matched_native_runtime_frame_prediction_schedule_owners = 0U;
    std::uint64_t mismatched_native_runtime_frame_prediction_schedule_owners = 0U;
    std::uint64_t native_runtime_frame_prediction_position_x_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_position_y_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_velocity_x_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_velocity_y_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_motion_flag_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_collision_state_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_actor_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_playfield_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_both_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_no_contact_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_input_active_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_input_neutral_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_position_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_damping_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_no_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_single_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_multiple_split_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_no_damping_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_single_damping_mismatches = 0U;
    std::uint64_t native_runtime_frame_prediction_multiple_damping_mismatches = 0U;
    struct FrameScheduleOwnerCount {
        devils_crush::native_runtime::BallFrameOwner owner =
            devils_crush::native_runtime::BallFrameOwner::unrecognized;
        std::uint64_t mismatches = 0U;
    };
    std::array<FrameScheduleOwnerCount, 4U> deferred_frame_schedule_owners{};
    std::uint64_t unclassified_deferred_frame_schedule_owners = 0U;
    std::array<FrameScheduleOwnerCount, 4U> prediction_schedule_owners{};
    std::uint64_t unclassified_prediction_schedule_owners = 0U;
    std::uint64_t native_initial_motion_candidates = 0U;
    std::uint64_t native_initial_motion_schedules_normal = 0U;
    std::uint64_t native_initial_motion_slot_zero_returns = 0U;
    std::uint64_t native_initial_motion_contacts = 0U;
    std::uint64_t native_initial_motion_attempt_caps = 0U;
    std::uint64_t unavailable_native_initial_motion = 0U;
    std::uint64_t native_initial_motion_failures = 0U;
    struct UnrecognizedScheduleHandlerCount {
        std::uint16_t handler_address = 0U;
        std::uint64_t frames = 0U;
    };
    std::array<UnrecognizedScheduleHandlerCount, 8U>
        unrecognized_schedule_handlers{};
    std::uint64_t unclassified_schedule_handlers = 0U;
    std::uint64_t unavailable_native_runtime_frame_predictions = 0U;
    std::uint64_t unavailable_native_runtime_frames = 0U;
    std::uint64_t native_runtime_frame_failures = 0U;
    std::uint64_t playfield_probe_entries = 0U;
    std::uint64_t unavailable_playfield_sources = 0U;
    std::uint64_t rejected_playfield_maps = 0U;
    std::uint64_t playfield_capture_failures = 0U;
    std::uint64_t reused_playfield_maps = 0U;
    std::uint64_t decoded_playfield_maps = 0U;
    std::uint64_t short_actor_pass_entries = 0U;
    std::uint64_t short_actor_pass_main_loop_call_sites = 0U;
    std::uint64_t short_actor_pass_timer_call_sites = 0U;
    std::uint64_t captured_short_actor_passes = 0U;
    std::uint64_t unavailable_short_actor_pass_captures = 0U;
    std::uint64_t short_actor_pass_capture_failures = 0U;
    std::uint64_t scheduled_short_actor_pass_actors = 0U;
    std::uint64_t frozen_short_actor_passes = 0U;
    std::uint64_t empty_short_actor_passes = 0U;
    std::uint64_t main_actor_pass_entries = 0U;
    std::uint64_t captured_main_actor_passes = 0U;
    std::uint64_t completed_main_actor_passes = 0U;
    std::uint64_t matched_main_actor_pass_dispatch_decisions = 0U;
    std::uint64_t mismatched_main_actor_pass_dispatch_decisions = 0U;
    std::uint64_t main_actor_pass_dispatched_when_skip_expected = 0U;
    std::uint64_t main_actor_pass_dispatched_for_other_slot = 0U;
    std::uint64_t main_actor_pass_dispatched_after_inactive_activity_gate = 0U;
    std::uint64_t main_actor_pass_dispatched_after_deferred_attribute_gate = 0U;
    std::uint64_t main_actor_pass_dispatched_after_changed_gate_bytes = 0U;
    std::uint64_t main_actor_pass_dispatched_after_unchanged_gate_bytes = 0U;
    std::uint64_t native_main_actor_pass_scan_realignments = 0U;
    std::uint64_t native_main_actor_pass_skipped_slots = 0U;
    std::uint64_t main_actor_pass_changed_activity_gate_bytes = 0U;
    std::uint64_t main_actor_pass_changed_attribute_gate_bytes = 0U;
    std::uint64_t main_actor_pass_unobserved_activity_gate_writers = 0U;
    std::uint64_t main_actor_pass_unobserved_attribute_gate_writers = 0U;
    std::uint64_t unclassified_main_actor_pass_activity_gate_writers = 0U;
    std::uint64_t unclassified_main_actor_pass_attribute_gate_writers = 0U;
    std::uint64_t main_actor_pass_dispatches_without_pending_decision = 0U;
    std::uint64_t main_actor_pass_queue_predictions = 0U;
    std::uint64_t matched_main_actor_pass_queue_predictions = 0U;
    std::uint64_t mismatched_main_actor_pass_queue_predictions = 0U;
    std::uint64_t main_actor_pass_completed_queue_filings = 0U;
    std::uint64_t main_actor_pass_queue_chain_mismatches = 0U;
    std::uint64_t unavailable_main_actor_pass_inputs = 0U;
    std::uint64_t unavailable_main_actor_pass_outputs = 0U;
    std::uint64_t superseded_main_actor_passes = 0U;
    std::uint64_t unmatched_main_actor_pass_completions = 0U;
    std::uint64_t main_actor_pass_capture_failures = 0U;
    std::uint64_t actor_dispatch_entries = 0U;
    std::uint64_t completed_actor_dispatches = 0U;
    std::uint64_t actor_queue_filer_entries = 0U;
    std::uint64_t matched_actor_dispatch_queue_handoffs = 0U;
    std::uint64_t mismatched_actor_dispatch_queue_handoffs = 0U;
    std::uint64_t unpaired_actor_dispatch_completions = 0U;
    std::uint64_t unpaired_actor_queue_filer_entries = 0U;
    std::uint64_t unavailable_actor_dispatch_entries = 0U;
    std::uint64_t unavailable_actor_dispatch_outputs = 0U;
    std::uint64_t unavailable_actor_queue_filer_inputs = 0U;
    std::uint64_t actor_queue_filer_predictions = 0U;
    std::uint64_t matched_actor_queue_filer_predictions = 0U;
    std::uint64_t mismatched_actor_queue_filer_predictions = 0U;
    std::uint64_t unavailable_actor_queue_filer_queue_inputs = 0U;
    std::uint64_t unavailable_actor_queue_filer_queue_outputs = 0U;
    std::uint64_t superseded_actor_queue_filer_predictions = 0U;
    std::uint64_t unmatched_actor_dispatch_completions = 0U;
    std::uint64_t nonqueue_actor_dispatch_returns = 0U;
    std::uint64_t actor_dispatch_output_slot_mismatches = 0U;
    std::uint64_t dropped_actor_dispatch_entries = 0U;
    std::uint64_t actor_dispatch_capture_failures = 0U;
    static constexpr std::size_t kMaximumActorDispatchHandlerEvidence = 256U;
    static constexpr std::size_t kReportedActorDispatchHandlerCoverage = 16U;
    static constexpr std::size_t kReportedActorDispatchHandlerEffects = 8U;
    std::array<ActorDispatchHandlerEvidence, kMaximumActorDispatchHandlerEvidence>
        actor_dispatch_handler_evidence{};
    std::uint64_t unclassified_actor_dispatch_handler_handoffs = 0U;
    std::uint64_t unavailable_actor_dispatch_state_transitions = 0U;
    std::uint64_t unclassified_actor_dispatch_state_transitions = 0U;
    std::uint64_t solid_playfield_probes = 0U;
    std::uint64_t clear_playfield_probes = 0U;
    std::uint64_t unmatched_playfield_probe_completions = 0U;
    std::uint64_t collision_retry_entries = 0U;
    std::uint64_t collision_retry_consuming_entries = 0U;
    std::uint64_t collision_retry_rearming_entries = 0U;
    std::uint64_t collision_retry_after_marked_playfield_cf33_entries = 0U;
    std::uint64_t collision_retry_after_unmarked_playfield_cf33_entries = 0U;
    std::uint64_t collision_retry_outside_signed_playfield_cf33_entries = 0U;
    std::vector<DecodedPlayfieldMap> playfield_maps;
    std::optional<NativeEpochRuntimeChain> native_epoch_runtime;

private:
    static constexpr std::size_t kMaximumPendingActorDispatches = 16U;
    static constexpr std::size_t kMaximumMainActorPassGateWriters = 16U;
    std::array<ActorGateWriteEvidence, kMaximumMainActorPassGateWriters>
        main_actor_pass_gate_writer_evidence{};
    std::vector<PendingActorDispatch> pending_actor_dispatches;
    std::optional<PendingActorQueueFiling> pending_actor_queue_filing;
    std::optional<PendingActorQueueFiler> pending_actor_queue_filer;
    std::optional<PendingMainActorPass> active_main_actor_pass;

    struct MotionRoutineSeed {
        RoutineKey routine;
        std::uint64_t entry_absolute_master_cycle = 0U;
        devils_crush::native_runtime::BallState ball;
        std::optional<devils_crush::native_runtime::BallMotionTuning> tuning;
        std::optional<devils_crush::native_runtime::BallMotionIncrement>
            substep_motion;
    };

    // DC95 gives the loop one immutable increment and F1B4 consumes it until
    // DD09 closes the clear path. The collision sweep can take a different
    // route at any point, so this observer certifies only a fully composed
    // sequence whose live state remains exactly on that no-contact path.
    struct CollisionLoopSeed {
        std::uint64_t split_entry_absolute_master_cycle = 0U;
        devils_crush::native_runtime::BallMotionSubsteps plan;
        devils_crush::native_runtime::BallState expected_ball;
        std::uint8_t completed_steps = 0U;
        std::optional<std::uint64_t> pending_position_entry;
        std::optional<std::uint64_t> pending_damping_entry;
        std::optional<devils_crush::native_runtime::BallMotionTuning>
            pending_damping_tuning;
        bool diverged_from_clear_path = false;
    };

    // CB2A exhausts actor candidates before it can fall through to CD74. That
    // makes the presence of a CD74 entry an unambiguous table-probe boundary;
    // its absence means the sweep was retained by an actor-contact path.
    struct CollisionSweepSeed {
        std::uint64_t entry_absolute_master_cycle = 0U;
        // The readable normal CB48/CC1A scan is actor-first. Preserve only
        // its carry-like boundary here: detailed target behavior remains with
        // the later native contact owner.
        std::optional<bool> native_actor_scan_found_solid;
        bool reached_playfield_probe = false;
        std::optional<std::uint64_t> playfield_probe_entry;
        std::optional<bool> playfield_probe_is_solid;
        std::optional<bool> cf33_selector_is_signed;
        std::optional<bool> cf33_signed_collision_marker;
    };

    // CBAB is a caller-selected one-slot actor query. It can occur inside the
    // wider CB2A sweep, so it owns a separate source/typed correlation rather
    // than being attributed to the outer scan's CD74 continuation.
    struct SingleSlotActorCollisionProbeSeed {
        std::uint64_t entry_absolute_master_cycle = 0U;
        std::optional<std::uint8_t> scanned_slot;
        std::optional<std::uint16_t> scanned_handler_address;
        std::optional<devils_crush::native_runtime::ActorCollisionScanResult>
            native_scan;
        std::optional<devils_crush::native_runtime::ActorCollisionScanDisposition>
            native_disposition;
        // CBC2 receives CC1A's carry immediately before CBAB either returns
        // clear or enters CBCF. A later CBCF can belong to CB48's broad scan,
        // so only this direct carry handoff may retain the one-slot seed for
        // confirmed-contact comparison.
        bool returned_contact = false;
    };

    struct SingleSlotActorCollisionHandlerEvidence {
        std::uint16_t handler_address = 0U;
        std::uint64_t matching_contacts = 0U;
        std::uint64_t mismatched_contacts = 0U;
    };

    // A carry-confirmed CBAB query that the readable CC1A model calls empty
    // is a true disagreement. Keep its finite mask input rather than guessing
    // a flipper-specific rule from the handler name.
    struct SingleSlotActorCollisionMaskMismatchEvidence {
        std::uint16_t handler_address = 0U;
        std::uint8_t slot = 0U;
        std::uint8_t phase = 0U;
        std::uint8_t probe_horizontal = 0U;
        std::uint16_t probe_vertical = 0U;
        std::uint8_t relative_horizontal = 0U;
        std::uint16_t relative_vertical = 0U;
        std::uint8_t mask_index = 0U;
        std::uint8_t selected_row = 0U;
        std::uint8_t column_bit = 0U;
        std::uint64_t contacts = 0U;
    };

    // The host-frame shadow above deliberately creates a fresh candidate at
    // every presentation boundary. DC95 is the narrower, recovered physics
    // boundary, so retain a separate candidate here and compare its completed
    // native state at the next live DC95 entry. A collision handoff resets the
    // chain because CF33/CDF8 still belong to the source resolver.
    void observe_native_epoch_candidate_entry(
        const devils_crush::native_runtime::GameStateView& state,
        const devils_crush::native_runtime::BallState observed_ball) noexcept
    {
        if (native_epoch_runtime) {
            const auto* const gameplay = native_epoch_runtime->runtime.gameplay();
            if (gameplay == nullptr) {
                ++native_epoch_runtime_unavailable;
                native_epoch_runtime.reset();
            } else {
                ++observed_native_epoch_runtime_boundaries;
                if (gameplay->physics().ball == observed_ball) {
                    ++matched_native_epoch_runtime_boundaries;
                    record_native_epoch_actor_handler_evidence(
                        *native_epoch_runtime, true);
                } else {
                    ++mismatched_native_epoch_runtime_boundaries;
                    record_native_epoch_actor_handler_evidence(
                        *native_epoch_runtime, false);
                    native_epoch_runtime.reset();
                }
            }
        }

        const auto tuning = state.ball_motion_tuning();
        if (!tuning || playfield_maps.empty()) {
            ++native_epoch_runtime_unavailable;
            native_epoch_runtime.reset();
            return;
        }

        try {
            if (!native_epoch_runtime) {
                const auto bootstrap = devils_crush::native_runtime::
                    capture_native_gameplay_bootstrap(state);
                if (!bootstrap) {
                    ++native_epoch_runtime_unavailable;
                    return;
                }
                native_epoch_runtime.emplace();
                if (!native_epoch_runtime->runtime.bootstrap(*bootstrap)) {
                    ++native_epoch_runtime_unavailable;
                    native_epoch_runtime.reset();
                    return;
                }
                ++native_epoch_runtime_seeds;
            }

            const auto frame = native_epoch_runtime->runtime.advance_physics_epoch(
                *tuning, playfield_maps.back().map.view());
            if (!frame) {
                ++native_epoch_runtime_unavailable;
                native_epoch_runtime.reset();
                return;
            }
            ++native_epoch_runtime_candidates;
            if (frame->requires_collision_resolution()) {
                ++native_epoch_runtime_contact_candidates;
                native_epoch_runtime.reset();
            } else {
                ++native_epoch_runtime_clear_candidates;
            }
        } catch (...) {
            // This chain is diagnostic only. Never let an allocation or
            // decoding failure alter live execution at a registry hook.
            ++native_epoch_runtime_unavailable;
            native_epoch_runtime.reset();
        }
    }

    void observe_native_epoch_actor_handler_dispatch(
        const devils_crush::native_runtime::GameStateView& state,
        const std::uint8_t slot) noexcept
    {
        if (!native_epoch_runtime) {
            return;
        }
        const auto actor = state.valid() ? state.actor(slot) : std::nullopt;
        if (!actor) {
            ++native_epoch_runtime->unclassified_source_actor_handler_entries;
            return;
        }
        for (auto& handler : native_epoch_runtime->source_actor_handlers) {
            if (handler.entries != 0U
                && handler.handler_address == actor->handler_address) {
                ++handler.entries;
                return;
            }
        }
        for (auto& handler : native_epoch_runtime->source_actor_handlers) {
            if (handler.entries == 0U) {
                handler = {
                    .handler_address = actor->handler_address,
                    .entries = 1U,
                };
                return;
            }
        }
        ++native_epoch_runtime->unclassified_source_actor_handler_entries;
    }

    void record_native_epoch_actor_handler_evidence(
        const NativeEpochRuntimeChain& chain,
        const bool matches) noexcept
    {
        for (const auto& source_handler : chain.source_actor_handlers) {
            if (source_handler.entries == 0U) {
                continue;
            }
            bool recorded_handler = false;
            for (auto& recorded : native_epoch_actor_handler_evidence) {
                if (recorded.matching_entries + recorded.mismatched_entries != 0U
                    && recorded.handler_address == source_handler.handler_address) {
                    if (matches) {
                        recorded.matching_entries += source_handler.entries;
                    } else {
                        recorded.mismatched_entries += source_handler.entries;
                    }
                    recorded_handler = true;
                    break;
                }
            }
            if (!recorded_handler) {
                for (auto& recorded : native_epoch_actor_handler_evidence) {
                    if (recorded.matching_entries + recorded.mismatched_entries == 0U) {
                        recorded.handler_address = source_handler.handler_address;
                        if (matches) {
                            recorded.matching_entries = source_handler.entries;
                        } else {
                            recorded.mismatched_entries = source_handler.entries;
                        }
                        recorded_handler = true;
                        break;
                    }
                }
            }
            if (!recorded_handler) {
                unclassified_native_epoch_actor_handler_entries
                    += source_handler.entries;
            }
        }
        unclassified_native_epoch_actor_handler_entries
            += chain.unclassified_source_actor_handler_entries;
    }

    void record_deferred_frame_schedule(
        const devils_crush::native_runtime::BallFrameOwner owner) noexcept
    {
        for (auto& recorded : deferred_frame_schedule_owners) {
            if (recorded.mismatches != 0U && recorded.owner == owner) {
                ++recorded.mismatches;
                return;
            }
        }
        for (auto& recorded : deferred_frame_schedule_owners) {
            if (recorded.mismatches == 0U) {
                recorded = {
                    .owner = owner,
                    .mismatches = 1U,
                };
                return;
            }
        }
        ++unclassified_deferred_frame_schedule_owners;
    }

    void record_prediction_schedule_owner(
        const devils_crush::native_runtime::BallFrameOwner owner) noexcept
    {
        for (auto& recorded : prediction_schedule_owners) {
            if (recorded.mismatches != 0U && recorded.owner == owner) {
                ++recorded.mismatches;
                return;
            }
        }
        for (auto& recorded : prediction_schedule_owners) {
            if (recorded.mismatches == 0U) {
                recorded = {
                    .owner = owner,
                    .mismatches = 1U,
                };
                return;
            }
        }
        ++unclassified_prediction_schedule_owners;
    }

    void record_unrecognized_schedule_handler(
        const std::uint16_t handler_address) noexcept
    {
        for (auto& recorded : unrecognized_schedule_handlers) {
            if (recorded.frames != 0U
                && recorded.handler_address == handler_address) {
                ++recorded.frames;
                return;
            }
        }
        for (auto& recorded : unrecognized_schedule_handlers) {
            if (recorded.frames == 0U) {
                recorded = {
                    .handler_address = handler_address,
                    .frames = 1U,
                };
                return;
            }
        }
        ++unclassified_schedule_handlers;
    }

    void observe_pending_native_frame_prediction(
        const std::span<const std::byte> work_ram,
        const InputFrame& input) noexcept
    {
        if (!pending_native_runtime_frame_prediction) {
            return;
        }
        const auto expected = *pending_native_runtime_frame_prediction;
        pending_native_runtime_frame_prediction.reset();
        const devils_crush::native_runtime::GameStateView state{work_ram};
        const auto ball = state.valid()
            ? devils_crush::native_runtime::capture_active_ball(state)
            : std::nullopt;
        if (!ball) {
            ++unavailable_native_runtime_frame_predictions;
            return;
        }
        const auto actual_schedule =
            devils_crush::native_runtime::capture_ball_frame_schedule(state);
        ++observed_native_runtime_frame_predictions;
        if (actual_schedule.owner == expected.schedule.owner) {
            ++matched_native_runtime_frame_prediction_schedule_owners;
        } else {
            ++mismatched_native_runtime_frame_prediction_schedule_owners;
            record_prediction_schedule_owner(actual_schedule.owner);
        }
        if (ball->state == expected.ball) {
            ++matched_native_runtime_frame_predictions;
        } else {
            ++mismatched_native_runtime_frame_predictions;
            if (input.player_one == 0U) {
                ++native_runtime_frame_prediction_input_neutral_mismatches;
            } else {
                ++native_runtime_frame_prediction_input_active_mismatches;
            }
            if (actual_frame_collision_facts.motion_substep_split_entries != 0U) {
                ++native_runtime_frame_prediction_split_mismatches;
                if (actual_frame_collision_facts.motion_substep_split_entries == 1U) {
                    ++native_runtime_frame_prediction_single_split_mismatches;
                } else {
                    ++native_runtime_frame_prediction_multiple_split_mismatches;
                }
            } else {
                ++native_runtime_frame_prediction_no_split_mismatches;
            }
            if (actual_frame_collision_facts.position_integration_entries != 0U) {
                ++native_runtime_frame_prediction_position_mismatches;
            }
            if (actual_frame_collision_facts.motion_damping_entries != 0U) {
                ++native_runtime_frame_prediction_damping_mismatches;
                if (actual_frame_collision_facts.motion_damping_entries == 1U) {
                    ++native_runtime_frame_prediction_single_damping_mismatches;
                } else {
                    ++native_runtime_frame_prediction_multiple_damping_mismatches;
                }
            } else {
                ++native_runtime_frame_prediction_no_damping_mismatches;
            }
            if (actual_frame_collision_facts.actor_contact
                && actual_frame_collision_facts.solid_playfield_contact) {
                ++native_runtime_frame_prediction_both_contact_mismatches;
            } else if (actual_frame_collision_facts.actor_contact) {
                ++native_runtime_frame_prediction_actor_contact_mismatches;
            } else if (actual_frame_collision_facts.solid_playfield_contact) {
                ++native_runtime_frame_prediction_playfield_contact_mismatches;
            } else {
                ++native_runtime_frame_prediction_no_contact_mismatches;
            }
            if (ball->state.position_x != expected.ball.position_x) {
                ++native_runtime_frame_prediction_position_x_mismatches;
            }
            if (ball->state.position_y != expected.ball.position_y) {
                ++native_runtime_frame_prediction_position_y_mismatches;
            }
            if (ball->state.velocity_x != expected.ball.velocity_x) {
                ++native_runtime_frame_prediction_velocity_x_mismatches;
            }
            if (ball->state.velocity_y != expected.ball.velocity_y) {
                ++native_runtime_frame_prediction_velocity_y_mismatches;
            }
            if (ball->state.motion_flags != expected.ball.motion_flags) {
                ++native_runtime_frame_prediction_motion_flag_mismatches;
            }
            if (ball->state.collision_state != expected.ball.collision_state) {
                ++native_runtime_frame_prediction_collision_state_mismatches;
            }
        }
    }

    // A marked CF33 contact enters CDF8 with CB7E's retry bit clear. Its first
    // child is F241, which restores F1B4's cache before CDF8 tries any
    // whole-pixel candidate. The later DC95 boundary may include additional
    // retry-loop motion, so this direct child-return boundary is the only
    // sound place to validate the readable rollback state.
    struct MarkedPlayfieldRetryRestoreSeed {
        std::uint64_t cdf8_entry_absolute_master_cycle = 0U;
        devils_crush::native_runtime::GameStateView::
            BallCollisionPositionCache cached_position;
    };

    enum class CollisionSweepExit {
        next_substep,
        damping,
        rearmed,
    };

    void observe_collision_sweep_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> rom) noexcept
    {
        finish_collision_sweep(CollisionSweepExit::next_substep);
        active_collision_sweep = CollisionSweepSeed{
            .entry_absolute_master_cycle = entry.absolute_master_cycle,
        };
        ++collision_sweep_entries;

        try {
            const auto walk = state.normal_actor_collision_scan_walk();
            const auto probe = state.normal_actor_collision_setup_probe();
            if (!walk || !probe) {
                ++unavailable_normal_actor_collision_scans;
                return;
            }
            const devils_crush::native_runtime::MappedRomView mapped_rom(
                rom, entry.registers.mpr);
            const auto scan = devils_crush::native_runtime::
                scan_normal_actor_collision_sweep(
                    mapped_rom,
                    state,
                    *walk,
                    *probe);
            if (!scan) {
                ++unavailable_normal_actor_collision_scans;
                return;
            }
            active_collision_sweep->native_actor_scan_found_solid =
                scan->first_solid_actor_slot.has_value();
            ++normal_actor_collision_scans;
        } catch (...) {
            // This remains a read-only bridge while the contact response is
            // still source-owned.  A bad mapped asset must not affect play.
            ++unavailable_normal_actor_collision_scans;
        }
    }

    void begin_single_slot_actor_collision_probe(
        const RoutineEntry& entry) noexcept
    {
        active_single_slot_actor_collision_probe = {
            .entry_absolute_master_cycle = entry.absolute_master_cycle,
        };
        ++single_slot_actor_collision_probe_entries;
    }

    void observe_single_slot_actor_collision_scan(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> rom) noexcept
    {
        if (!active_single_slot_actor_collision_probe
            || active_single_slot_actor_collision_probe->native_disposition) {
            return;
        }
        try {
            const auto actor = state.actor(entry.registers.y);
            const auto context = devils_crush::native_runtime::
                make_actor_collision_scan_context(state);
            const auto probe = state.normal_actor_collision_probe();
            if (!actor || !context || !probe) {
                ++unavailable_single_slot_actor_collision_scans;
                return;
            }
            const devils_crush::native_runtime::MappedRomView mapped_rom(
                rom, entry.registers.mpr);
            const auto scan = devils_crush::native_runtime::scan_actor_collision(
                mapped_rom, *context, *actor, *probe);
            active_single_slot_actor_collision_probe->scanned_slot = actor->index;
            active_single_slot_actor_collision_probe->scanned_handler_address =
                actor->handler_address;
            active_single_slot_actor_collision_probe->native_scan = scan;
            active_single_slot_actor_collision_probe->native_disposition =
                scan.disposition;
            ++single_slot_actor_collision_scans;
        } catch (...) {
            ++unavailable_single_slot_actor_collision_scans;
        }
    }

    // The source reaches CC98 only after it has converted the local mask
    // coordinates into row-major scratch state. Sampling this boundary lets
    // the readable pre-scan distinguish an incorrect geometry model from a
    // later mapped-ROM lookup without changing live play.
    void observe_single_slot_actor_collision_mask_stage(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> rom) noexcept
    {
        if (!active_single_slot_actor_collision_probe
            || !active_single_slot_actor_collision_probe->native_scan
            || !active_single_slot_actor_collision_probe->native_scan->mask) {
            return;
        }
        try {
            const auto stage = state.actor_collision_mask_lookup_stage();
            const auto pointers = state.actor_collision_mask_table_pointers();
            if (!stage || !pointers) {
                ++unavailable_single_slot_collision_mask_stages;
                return;
            }

            const devils_crush::native_runtime::ActorCollisionMaskCoordinates
                staged_coordinates{
                    .input = {
                        .bounds = {.disposition = devils_crush::native_runtime::
                                       CollisionBoundsDisposition::intersecting},
                    },
                    .cell = {
                        .column = stage->mask_column,
                        .row = stage->mask_row,
                    },
                    .horizontal_cell = stage->horizontal_cell,
                    .vertical_cell = stage->vertical_offset,
                    // `$2F` already contains CC95's multiplied row offset.
                    .rounded_horizontal_span = 1U,
                };
            const devils_crush::native_runtime::MappedRomView mapped_rom(
                rom, entry.registers.mpr);
            const auto staged = devils_crush::native_runtime::
                resolve_actor_collision_mask(
                    mapped_rom, *pointers, staged_coordinates);
            if (!staged) {
                ++unavailable_single_slot_collision_mask_stages;
                return;
            }

            ++single_slot_collision_mask_stage_entries;
            const auto& predicted = *active_single_slot_actor_collision_probe
                                        ->native_scan->mask;
            if (predicted.table_index == staged->table_index) {
                ++matched_single_slot_collision_mask_stage_indices;
            }
            if (predicted.evaluation.cell == staged->evaluation.cell) {
                ++matched_single_slot_collision_mask_stage_cells;
            }
            if (predicted.mask_index == staged->mask_index
                && predicted.evaluation.selected_row
                    == staged->evaluation.selected_row
                && predicted.evaluation.column_bit
                    == staged->evaluation.column_bit) {
                ++matched_single_slot_collision_mask_stage_lookups;
            }
            if (predicted.evaluation.disposition
                == staged->evaluation.disposition) {
                ++matched_single_slot_collision_mask_stage_decisions;
            }
        } catch (...) {
            ++unavailable_single_slot_collision_mask_stages;
        }
    }

    void observe_single_slot_confirmed_actor_contact() noexcept
    {
        if (!active_single_slot_actor_collision_probe
            || !active_single_slot_actor_collision_probe->returned_contact) {
            return;
        }
        ++single_slot_confirmed_actor_contacts;
        const auto disposition =
            active_single_slot_actor_collision_probe->native_disposition;
        if (disposition
            == devils_crush::native_runtime::ActorCollisionScanDisposition::
                solid_mask) {
            ++matched_single_slot_actor_collision_contacts;
            record_single_slot_actor_collision_handler(
                active_single_slot_actor_collision_probe->scanned_handler_address,
                true);
        } else {
            ++mismatched_single_slot_actor_collision_contacts;
            record_single_slot_actor_collision_disposition(disposition);
            record_single_slot_actor_collision_mask_mismatch(
                active_single_slot_actor_collision_probe->native_scan);
            record_single_slot_actor_collision_handler(
                active_single_slot_actor_collision_probe->scanned_handler_address,
                false);
        }
        active_single_slot_actor_collision_probe.reset();
    }

    void observe_single_slot_actor_collision_result(
        const RoutineEntry& entry) noexcept
    {
        if (!active_single_slot_actor_collision_probe) {
            return;
        }

        // CBC2 follows the JSR CC1A inside CBAB. Its incoming carry is the
        // scanner's contact result; a clear return cannot subsequently reach
        // CBCF through this one-slot query, so discard it now. This avoids
        // attributing a later CB48 contact to a stale flipper probe.
        if ((entry.registers.status & 0x01U) == 0U) {
            active_single_slot_actor_collision_probe.reset();
            return;
        }
        active_single_slot_actor_collision_probe->returned_contact = true;
        ++single_slot_actor_collision_contact_returns;
    }

    void record_single_slot_actor_collision_disposition(
        const std::optional<devils_crush::native_runtime::
            ActorCollisionScanDisposition> disposition) noexcept
    {
        using Disposition = devils_crush::native_runtime::
            ActorCollisionScanDisposition;
        if (!disposition) {
            ++unavailable_single_slot_confirmed_contact_scans;
            return;
        }
        switch (*disposition) {
        case Disposition::outside_vertical_proximity:
            ++single_slot_confirmed_contact_vertical_misses;
            return;
        case Disposition::unresolved_shape_asset:
            ++single_slot_confirmed_contact_shape_asset_misses;
            return;
        case Disposition::outside_horizontal_world:
            ++single_slot_confirmed_contact_horizontal_world_misses;
            return;
        case Disposition::outside_bounds:
            ++single_slot_confirmed_contact_bounds_misses;
            return;
        case Disposition::unresolved_mask_asset:
            ++single_slot_confirmed_contact_mask_asset_misses;
            return;
        case Disposition::empty_mask:
            ++single_slot_confirmed_contact_empty_mask_misses;
            return;
        case Disposition::solid_mask:
            return;
        }
    }

    void record_single_slot_actor_collision_handler(
        const std::optional<std::uint16_t> handler_address,
        const bool matches) noexcept
    {
        if (!handler_address) {
            ++unclassified_single_slot_actor_collision_contacts;
            return;
        }
        for (auto& handler : single_slot_actor_collision_handlers) {
            if (handler.matching_contacts + handler.mismatched_contacts != 0U
                && handler.handler_address == *handler_address) {
                if (matches) {
                    ++handler.matching_contacts;
                } else {
                    ++handler.mismatched_contacts;
                }
                return;
            }
        }
        for (auto& handler : single_slot_actor_collision_handlers) {
            if (handler.matching_contacts + handler.mismatched_contacts == 0U) {
                handler.handler_address = *handler_address;
                if (matches) {
                    handler.matching_contacts = 1U;
                } else {
                    handler.mismatched_contacts = 1U;
                }
                return;
            }
        }
        ++unclassified_single_slot_actor_collision_contacts;
    }

    void record_single_slot_actor_collision_mask_mismatch(
        const std::optional<devils_crush::native_runtime::
            ActorCollisionScanResult>& scan) noexcept
    {
        using Disposition = devils_crush::native_runtime::
            ActorCollisionScanDisposition;
        if (!scan || scan->disposition != Disposition::empty_mask
            || !scan->bounds || !scan->mask) {
            return;
        }

        const auto& mask = *scan->mask;
        const SingleSlotActorCollisionMaskMismatchEvidence candidate{
            .handler_address = scan->actor.handler_address,
            .slot = scan->actor.index,
            .phase = scan->actor.collision_shape_phase,
            .probe_horizontal = scan->probe.horizontal,
            .probe_vertical = scan->probe.vertical,
            .relative_horizontal = scan->bounds->relative_horizontal,
            .relative_vertical = scan->bounds->relative_vertical,
            .mask_index = mask.mask_index,
            .selected_row = mask.evaluation.selected_row,
            .column_bit = mask.evaluation.column_bit,
            .contacts = 1U,
        };
        for (auto& evidence : single_slot_actor_collision_mask_mismatches) {
            if (evidence.contacts != 0U
                && evidence.handler_address == candidate.handler_address
                && evidence.slot == candidate.slot
                && evidence.phase == candidate.phase
                && evidence.probe_horizontal == candidate.probe_horizontal
                && evidence.probe_vertical == candidate.probe_vertical
                && evidence.relative_horizontal == candidate.relative_horizontal
                && evidence.relative_vertical == candidate.relative_vertical
                && evidence.mask_index == candidate.mask_index
                && evidence.selected_row == candidate.selected_row
                && evidence.column_bit == candidate.column_bit) {
                ++evidence.contacts;
                return;
            }
        }
        for (auto& evidence : single_slot_actor_collision_mask_mismatches) {
            if (evidence.contacts == 0U) {
                evidence = candidate;
                return;
            }
        }
        ++unclassified_single_slot_actor_collision_mask_mismatches;
    }

    void observe_playfield_collision_completion(const RoutineCall& call) noexcept
    {
        if (!active_collision_sweep
            || !active_collision_sweep->playfield_probe_entry
            || *active_collision_sweep->playfield_probe_entry
                != call.entry.absolute_master_cycle) {
            ++unmatched_playfield_probe_completions;
            return;
        }
        const bool solid = (call.output_registers.status & 0x01U) != 0U;
        active_collision_sweep->playfield_probe_is_solid = solid;
        if (solid) {
            ++solid_playfield_probes;
            actual_frame_collision_facts.solid_playfield_contact = true;
        } else {
            ++clear_playfield_probes;
        }
    }

    // CF33 follows a solid CD74 table probe. Its high selector bit chooses a
    // signed leg or clear-selector CF5B; within the signed leg, the ball's
    // collision marker selects CDF8's placement retry versus CF3F's ordinary
    // response. Retain those exact facts on the active sweep and classify its
    // later observed exit only.
    void observe_playfield_contact_response_entry(
        const devils_crush::native_runtime::GameStateView& state) noexcept
    {
        if (!active_collision_sweep
            || !active_collision_sweep->playfield_probe_is_solid
            || !*active_collision_sweep->playfield_probe_is_solid) {
            return;
        }
        const auto input = state.collision_response_input();
        if (!input) {
            ++unavailable_playfield_contact_responses;
            return;
        }
        const auto selector_is_signed = input->selects_signed_cf33_response();
        active_collision_sweep->cf33_selector_is_signed = selector_is_signed;
        if (selector_is_signed) {
            ++signed_playfield_contact_responses;
            const auto ball = state.ball();
            if (!ball) {
                ++unavailable_playfield_contact_markers;
            } else if ((ball->collision_state & 0x80U) != 0U) {
                active_collision_sweep->cf33_signed_collision_marker = true;
                ++signed_playfield_contact_marked_retries;
            } else {
                active_collision_sweep->cf33_signed_collision_marker = false;
                ++signed_playfield_contact_unmarked_responses;
            }
        } else {
            ++clear_playfield_contact_responses;
        }
    }

    // CDF8 returns to CB7E, whose BBR3 $77 controls whether the parent C6/C7
    // loop consumes the resolved substep. Capture the bit before CDF8's own
    // placement work, without treating either arm as a native completion.
    void observe_collision_retry_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::GameStateView& state,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr std::size_t kCollisionSweepFlags = 0x77U;
        if (work_ram.size() <= kCollisionSweepFlags) {
            return;
        }
        ++collision_retry_entries;
        const auto flags = std::to_integer<std::uint8_t>(
            work_ram[kCollisionSweepFlags]);
        if ((flags & 0x08U) != 0U) {
            ++collision_retry_consuming_entries;
        } else {
            ++collision_retry_rearming_entries;
        }
        const bool follows_signed_playfield_response = active_collision_sweep
            && active_collision_sweep->cf33_selector_is_signed
            && *active_collision_sweep->cf33_selector_is_signed
            && active_collision_sweep->cf33_signed_collision_marker.has_value();
        if (!follows_signed_playfield_response) {
            ++collision_retry_outside_signed_playfield_cf33_entries;
            return;
        }
        if (*active_collision_sweep->cf33_signed_collision_marker) {
            ++collision_retry_after_marked_playfield_cf33_entries;
        } else {
            // CF33's unmarked signed leg branches to CF3F, not CDF8. Keep any
            // such transcript visible rather than silently attributing it to
            // the pending playfield retry.
            ++collision_retry_after_unmarked_playfield_cf33_entries;
            return;
        }

        // Only CB7E's clear arm is the restored-cache retry that the readable
        // state model owns. A consuming arm is retained as telemetry until its
        // parent context is independently classified.
        if ((flags & 0x08U) != 0U) {
            return;
        }
        const auto cache = state.ball_collision_position_cache();
        if (!cache) {
            ++unavailable_marked_playfield_retry_restore_caches;
            return;
        }
        if (pending_marked_playfield_retry_restore) {
            ++superseded_marked_playfield_retry_restores;
        }
        pending_marked_playfield_retry_restore =
            MarkedPlayfieldRetryRestoreSeed{
                .cdf8_entry_absolute_master_cycle =
                    entry.absolute_master_cycle,
                .cached_position = *cache,
            };
        ++marked_playfield_retry_restore_candidates;
    }

    void observe_marked_playfield_retry_restore(
        const RoutineCall& call,
        const std::span<const std::byte> work_ram) noexcept
    {
        constexpr std::uint16_t kPositionRetryRestoreCaller = 0xCE1FU;
        constexpr std::uint16_t kPositionRetryRestoreReturn = 0xCE22U;
        if (call.caller_pc != kPositionRetryRestoreCaller
            || call.return_pc != kPositionRetryRestoreReturn
            || !pending_marked_playfield_retry_restore) {
            return;
        }
        const auto expected = *pending_marked_playfield_retry_restore;
        pending_marked_playfield_retry_restore.reset();
        ++observed_marked_playfield_retry_restores;
        const devils_crush::native_runtime::GameStateView state{work_ram};
        const auto restored = state.ball();
        if (!restored) {
            ++unavailable_marked_playfield_retry_restore_caches;
            return;
        }
        if (restored->position_x == expected.cached_position.position_x
            && restored->position_y == expected.cached_position.position_y) {
            ++verified_marked_playfield_retry_restores;
        } else {
            ++mismatched_marked_playfield_retry_restores;
        }
    }

    void finish_collision_sweep(const CollisionSweepExit exit) noexcept
    {
        if (!active_collision_sweep) {
            return;
        }
        const auto reached_playfield =
            active_collision_sweep->reached_playfield_probe;
        if (active_collision_sweep->native_actor_scan_found_solid
            && reached_playfield) {
            // A later CBAB single-slot query can run inside this broader
            // sweep.  Only an actual CD74 continuation belongs unambiguously
            // to CB48's actor-first result; a no-CD74 exit is therefore not
            // evidence that the outer scan missed an actor.
            if (!*active_collision_sweep->native_actor_scan_found_solid) {
                ++matched_normal_actor_collision_boundaries;
            } else {
                ++mismatched_normal_actor_collision_boundaries;
            }
        } else if (active_collision_sweep->native_actor_scan_found_solid) {
            ++unresolved_normal_actor_collision_boundaries;
        }
        switch (exit) {
        case CollisionSweepExit::next_substep:
            ++clear_collision_sweeps;
            if (reached_playfield
                && active_collision_sweep->cf33_selector_is_signed) {
                record_playfield_contact_response_exit(exit);
            }
            break;
        case CollisionSweepExit::damping:
            if (reached_playfield) {
                ++playfield_contact_sweeps_to_damping;
                if (active_collision_sweep->playfield_probe_is_solid
                    && *active_collision_sweep->playfield_probe_is_solid) {
                    record_playfield_contact_response_exit(exit);
                }
            } else {
                ++actor_contact_sweeps_to_damping;
                actual_frame_collision_facts.actor_contact = true;
            }
            break;
        case CollisionSweepExit::rearmed:
            if (reached_playfield) {
                ++playfield_contact_sweeps_rearmed;
                if (active_collision_sweep->playfield_probe_is_solid
                    && *active_collision_sweep->playfield_probe_is_solid) {
                    record_playfield_contact_response_exit(exit);
                }
            } else {
                ++actor_contact_sweeps_rearmed;
                actual_frame_collision_facts.actor_contact = true;
            }
            break;
        }
        active_collision_sweep.reset();
    }

    void record_playfield_contact_response_exit(
        const CollisionSweepExit exit) noexcept
    {
        if (!active_collision_sweep->cf33_selector_is_signed) {
            ++playfield_contacts_without_cf33_response;
            return;
        }
        if (*active_collision_sweep->cf33_selector_is_signed) {
            if (exit == CollisionSweepExit::rearmed) {
                ++signed_playfield_contact_rearms;
            } else if (exit == CollisionSweepExit::damping) {
                ++signed_playfield_contact_damping_paths;
            } else {
                ++signed_playfield_contact_clear_paths;
            }
            return;
        }
        if (exit == CollisionSweepExit::rearmed) {
            ++clear_playfield_contact_rearms;
        } else if (exit == CollisionSweepExit::damping) {
            ++clear_playfield_contact_damping_paths;
        } else {
            ++clear_playfield_contact_clear_paths;
        }
    }

    void observe_collision_loop_split_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::BallState ball) noexcept
    {
        finish_collision_sweep(CollisionSweepExit::rearmed);
        if (active_collision_loop) {
            ++collision_loop_rearmed_before_completion;
        }
        active_collision_loop = CollisionLoopSeed{
            .split_entry_absolute_master_cycle = entry.absolute_master_cycle,
            .plan = devils_crush::native_runtime::split_ball_motion_substeps(ball),
            .expected_ball = ball,
        };
        ++collision_loop_seeds;
    }

    void observe_collision_loop_position_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::BallState ball,
        const devils_crush::native_runtime::BallMotionIncrement increment) noexcept
    {
        finish_collision_sweep(CollisionSweepExit::next_substep);
        if (!active_collision_loop) {
            return;
        }
        auto& loop = *active_collision_loop;
        const devils_crush::native_runtime::BallMotionIncrement expected_increment{
            .horizontal = loop.plan.velocity_x_per_step,
            .vertical = loop.plan.velocity_y_per_step,
        };
        if (loop.completed_steps >= loop.plan.count || increment != expected_increment
            || ball != loop.expected_ball) {
            loop.diverged_from_clear_path = true;
            return;
        }
        loop.pending_position_entry = entry.absolute_master_cycle;
    }

    void observe_collision_loop_position_completion(
        const RoutineCall& call,
        const devils_crush::native_runtime::BallState ball) noexcept
    {
        if (!active_collision_loop) {
            return;
        }
        auto& loop = *active_collision_loop;
        if (!loop.pending_position_entry
            || *loop.pending_position_entry != call.entry.absolute_master_cycle) {
            return;
        }
        loop.pending_position_entry.reset();
        const devils_crush::native_runtime::BallMotionIncrement increment{
            .horizontal = loop.plan.velocity_x_per_step,
            .vertical = loop.plan.velocity_y_per_step,
        };
        const auto expected = devils_crush::native_runtime::advance_ball_motion_substep(
            loop.expected_ball, increment);
        if (expected.state != ball) {
            loop.diverged_from_clear_path = true;
            return;
        }
        loop.expected_ball = expected.state;
        ++loop.completed_steps;
    }

    void observe_collision_loop_damping_entry(
        const RoutineEntry& entry,
        const devils_crush::native_runtime::BallState ball,
        const devils_crush::native_runtime::BallMotionTuning& tuning) noexcept
    {
        const bool completed_clear_path = active_collision_loop
            && !active_collision_loop->diverged_from_clear_path
            && active_collision_loop->completed_steps
                == active_collision_loop->plan.count
            && ball == active_collision_loop->expected_ball;
        finish_collision_sweep(
            completed_clear_path ? CollisionSweepExit::next_substep
                                 : CollisionSweepExit::damping);
        if (!active_collision_loop) {
            return;
        }
        auto& loop = *active_collision_loop;
        if (loop.diverged_from_clear_path || loop.completed_steps != loop.plan.count
            || ball != loop.expected_ball) {
            ++collision_or_control_routed_loops;
            active_collision_loop.reset();
            return;
        }
        loop.pending_damping_entry = entry.absolute_master_cycle;
        loop.pending_damping_tuning = tuning;
    }

    void observe_collision_loop_damping_completion(
        const RoutineCall& call,
        const devils_crush::native_runtime::BallState ball) noexcept
    {
        if (!active_collision_loop) {
            return;
        }
        const auto& loop = *active_collision_loop;
        if (!loop.pending_damping_entry || !loop.pending_damping_tuning
            || *loop.pending_damping_entry != call.entry.absolute_master_cycle) {
            return;
        }
        const auto expected = devils_crush::native_runtime::apply_ball_drag_and_gravity(
            loop.expected_ball, *loop.pending_damping_tuning);
        if (expected && expected->state == ball) {
            ++composed_clear_collision_loops;
        } else {
            ++unexpected_composed_clear_loop_results;
        }
        active_collision_loop.reset();
    }

    void report_collision_loop_shadow() const
    {
        if (collision_loop_seeds == 0U) {
            return;
        }
        std::cout << "; collision-loop shadow: "
                  << composed_clear_collision_loops << "/"
                  << collision_loop_seeds
                  << " complete clear epoch(s) composed";
        if (collision_or_control_routed_loops != 0U) {
            std::cout << ", " << collision_or_control_routed_loops
                      << " collision/control-routed";
        }
        if (collision_loop_rearmed_before_completion != 0U) {
            std::cout << ", " << collision_loop_rearmed_before_completion
                      << " rearmed before completion";
        }
        if (unexpected_composed_clear_loop_results != 0U) {
            std::cout << ", " << unexpected_composed_clear_loop_results
                      << " unexpected clear result(s)";
        }
        if (collision_sweep_entries != 0U) {
            std::cout << "; collision-sweep shadow: "
                      << clear_collision_sweeps
                      << " clear sweep(s), "
                      << actor_contact_sweeps_rearmed
                      << " actor-contact rearm(s), "
                      << playfield_contact_sweeps_rearmed
                      << " playfield-probed rearm(s), "
                      << actor_contact_sweeps_to_damping
                      << " actor-contact damping path(s), "
                          << playfield_contact_sweeps_to_damping
                          << " playfield-probed damping path(s)";
            if (normal_actor_collision_scans != 0U
                || unavailable_normal_actor_collision_scans != 0U) {
                std::cout << "; normal actor-collision shadow: "
                          << matched_normal_actor_collision_boundaries
                          << " playfield-continuation match(es)";
                if (mismatched_normal_actor_collision_boundaries != 0U) {
                    std::cout << ", "
                              << mismatched_normal_actor_collision_boundaries
                              << " mismatch(es)";
                }
                if (unavailable_normal_actor_collision_scans != 0U) {
                    std::cout << ", "
                              << unavailable_normal_actor_collision_scans
                              << " unavailable scan(s)";
                }
                if (unresolved_normal_actor_collision_boundaries != 0U) {
                    std::cout << ", "
                              << unresolved_normal_actor_collision_boundaries
                              << " nested route(s) retained for the "
                                 "single-slot owner";
                }
            }
            if (signed_playfield_contact_responses != 0U
                || clear_playfield_contact_responses != 0U
                || unavailable_playfield_contact_responses != 0U) {
                std::cout << "; playfield-response shadow: "
                          << signed_playfield_contact_responses
                          << " signed CF33 selector(s), "
                          << clear_playfield_contact_responses
                          << " clear CF5B selector(s), "
                          << signed_playfield_contact_rearms
                          << " signed rearm(s), "
                          << clear_playfield_contact_rearms
                          << " clear rearm(s), "
                          << signed_playfield_contact_damping_paths
                          << " signed damping path(s), "
                          << clear_playfield_contact_damping_paths
                          << " clear damping path(s), "
                          << signed_playfield_contact_clear_paths
                          << " signed next-substep path(s), "
                          << clear_playfield_contact_clear_paths
                          << " clear next-substep path(s)";
                if (signed_playfield_contact_marked_retries != 0U
                    || signed_playfield_contact_unmarked_responses != 0U
                    || unavailable_playfield_contact_markers != 0U) {
                    std::cout << ", "
                              << signed_playfield_contact_marked_retries
                              << " signed marked CDF8 handoff(s), "
                              << signed_playfield_contact_unmarked_responses
                              << " signed unmarked CF3F branch(es)";
                    if (unavailable_playfield_contact_markers != 0U) {
                        std::cout << ", "
                                  << unavailable_playfield_contact_markers
                                  << " unavailable signed marker(s)";
                    }
                }
                if (playfield_contacts_without_cf33_response != 0U) {
                    std::cout << ", "
                              << playfield_contacts_without_cf33_response
                              << " contact(s) without a CF33 entry";
                }
                if (unavailable_playfield_contact_responses != 0U) {
                    std::cout << ", "
                              << unavailable_playfield_contact_responses
                              << " unavailable response input(s)";
                }
            }
        }
        if (single_slot_actor_collision_probe_entries != 0U) {
            std::cout << "; single-slot actor-collision shadow: "
                      << matched_single_slot_actor_collision_contacts << "/"
                      << single_slot_confirmed_actor_contacts
                      << " confirmed-contact match(es) from "
                      << single_slot_actor_collision_scans
                      << " typed scan(s)";
            if (mismatched_single_slot_actor_collision_contacts != 0U) {
                std::cout << ", "
                          << mismatched_single_slot_actor_collision_contacts
                          << " mismatch(es)";
                std::cout << " (vertical="
                          << single_slot_confirmed_contact_vertical_misses
                          << ", shape="
                          << single_slot_confirmed_contact_shape_asset_misses
                          << ", horizontal="
                          << single_slot_confirmed_contact_horizontal_world_misses
                          << ", bounds="
                          << single_slot_confirmed_contact_bounds_misses
                          << ", mask="
                          << single_slot_confirmed_contact_mask_asset_misses
                          << ", empty="
                          << single_slot_confirmed_contact_empty_mask_misses
                          << ", unavailable="
                          << unavailable_single_slot_confirmed_contact_scans
                          << ')';
            }
            if (unavailable_single_slot_actor_collision_scans != 0U) {
                std::cout << ", "
                          << unavailable_single_slot_actor_collision_scans
                          << " unavailable scan(s)";
            }
            if (single_slot_actor_collision_handlers[0U].matching_contacts
                    + single_slot_actor_collision_handlers[0U].mismatched_contacts
                != 0U) {
                std::cout << "; single-slot target handler(s)";
                for (const auto& handler : single_slot_actor_collision_handlers) {
                    if (handler.matching_contacts + handler.mismatched_contacts
                        == 0U) {
                        continue;
                    }
                    std::cout << ' ' << std::uppercase << std::hex
                              << std::setw(4) << std::setfill('0')
                              << handler.handler_address << std::dec << '='
                              << handler.matching_contacts << '/'
                              << handler.mismatched_contacts;
                }
                if (unclassified_single_slot_actor_collision_contacts != 0U) {
                    std::cout << " other="
                              << unclassified_single_slot_actor_collision_contacts;
                }
            }
            if (single_slot_actor_collision_mask_mismatches[0U].contacts != 0U) {
                std::cout << "; single-slot empty-mask evidence";
                for (const auto& evidence : single_slot_actor_collision_mask_mismatches) {
                    if (evidence.contacts == 0U) {
                        continue;
                    }
                    std::cout << " " << std::uppercase << std::hex
                              << std::setw(4) << std::setfill('0')
                              << evidence.handler_address << ":slot="
                              << std::setw(2)
                              << static_cast<unsigned>(evidence.slot)
                              << ",phase=" << std::setw(2)
                              << static_cast<unsigned>(evidence.phase)
                              << ",probe=" << std::setw(2)
                              << static_cast<unsigned>(evidence.probe_horizontal)
                              << ':' << std::setw(4) << evidence.probe_vertical
                              << ",delta=" << std::setw(2)
                              << static_cast<unsigned>(evidence.relative_horizontal)
                              << ':' << std::setw(4) << evidence.relative_vertical
                              << ",mask=" << std::setw(2)
                              << static_cast<unsigned>(evidence.mask_index)
                              << ':' << std::setw(2)
                              << static_cast<unsigned>(evidence.selected_row)
                              << "/" << std::setw(2)
                              << static_cast<unsigned>(evidence.column_bit)
                              << std::dec << "x" << evidence.contacts;
                }
                if (unclassified_single_slot_actor_collision_mask_mismatches != 0U) {
                    std::cout << " other="
                              << unclassified_single_slot_actor_collision_mask_mismatches;
                }
            }
            if (single_slot_collision_mask_stage_entries != 0U
                || unavailable_single_slot_collision_mask_stages != 0U) {
                std::cout << "; single-slot CC98 stage shadow: "
                          << single_slot_collision_mask_stage_entries
                          << " sampled (index="
                          << matched_single_slot_collision_mask_stage_indices
                          << ", cell="
                          << matched_single_slot_collision_mask_stage_cells
                          << ", lookup="
                          << matched_single_slot_collision_mask_stage_lookups
                          << ", decision="
                          << matched_single_slot_collision_mask_stage_decisions
                          << ')';
                if (unavailable_single_slot_collision_mask_stages != 0U) {
                    std::cout << ", "
                              << unavailable_single_slot_collision_mask_stages
                              << " unavailable";
                }
            }
        }
    }

    void report_motion_routine_shadow() const
    {
        if (position_integration_predictions == 0U
            && motion_damping_predictions == 0U
            && rejected_position_integration_predictions == 0U
            && rejected_motion_damping_predictions == 0U) {
            return;
        }
        std::cout << "; motion-routine shadow: F1B4 "
                  << position_integration_matches << "/"
                  << position_integration_predictions << " match(es), DD09 "
                  << motion_damping_matches << "/"
                  << motion_damping_predictions << " match(es)";
        if (position_integration_mismatches != 0U) {
            std::cout << ", " << position_integration_mismatches
                      << " F1B4 mismatch(es)";
        }
        if (motion_damping_mismatches != 0U) {
            std::cout << ", " << motion_damping_mismatches
                      << " DD09 mismatch(es)";
        }
        if (rejected_position_integration_predictions != 0U) {
            std::cout << ", " << rejected_position_integration_predictions
                      << " rejected F1B4 prediction(s)";
        }
        if (rejected_motion_damping_predictions != 0U) {
            std::cout << ", " << rejected_motion_damping_predictions
                      << " rejected DD09 prediction(s)";
        }
        if (unavailable_motion_routine_seeds != 0U
            || unavailable_motion_routine_outputs != 0U
            || unmatched_motion_routine_completions != 0U
            || dropped_motion_routine_seeds != 0U
            || motion_routine_capture_failures != 0U) {
            std::cout << " (";
            bool separated = false;
            const auto append = [&separated](
                                    const std::uint64_t count,
                                    const std::string_view label) {
                if (count == 0U) {
                    return;
                }
                if (separated) {
                    std::cout << ", ";
                }
                std::cout << count << ' ' << label;
                separated = true;
            };
            append(unavailable_motion_routine_seeds, "unavailable seed(s)");
            append(unavailable_motion_routine_outputs, "unavailable output(s)");
            append(unmatched_motion_routine_completions, "unmatched completion(s)");
            append(dropped_motion_routine_seeds, "dropped seed(s)");
            append(motion_routine_capture_failures, "capture failure(s)");
            std::cout << ')';
        }
    }

    static constexpr std::size_t kMaximumMotionRoutineSeeds = 16U;
    std::vector<MotionRoutineSeed> motion_routine_seeds;
    std::uint64_t unavailable_motion_routine_seeds = 0U;
    std::uint64_t unavailable_motion_routine_outputs = 0U;
    std::uint64_t unmatched_motion_routine_completions = 0U;
    std::uint64_t dropped_motion_routine_seeds = 0U;
    std::uint64_t motion_routine_capture_failures = 0U;
    std::uint64_t position_integration_predictions = 0U;
    std::uint64_t position_integration_matches = 0U;
    std::uint64_t position_integration_mismatches = 0U;
    std::uint64_t rejected_position_integration_predictions = 0U;
    std::uint64_t motion_damping_predictions = 0U;
    std::uint64_t motion_damping_matches = 0U;
    std::uint64_t motion_damping_mismatches = 0U;
    std::uint64_t rejected_motion_damping_predictions = 0U;
    std::optional<CollisionLoopSeed> active_collision_loop;
    std::uint64_t collision_loop_seeds = 0U;
    std::uint64_t composed_clear_collision_loops = 0U;
    std::uint64_t collision_or_control_routed_loops = 0U;
    std::uint64_t collision_loop_rearmed_before_completion = 0U;
    std::uint64_t unexpected_composed_clear_loop_results = 0U;
    std::optional<CollisionSweepSeed> active_collision_sweep;
    std::uint64_t collision_sweep_entries = 0U;
    std::uint64_t clear_collision_sweeps = 0U;
    std::uint64_t actor_contact_sweeps_rearmed = 0U;
    std::uint64_t playfield_contact_sweeps_rearmed = 0U;
    std::uint64_t actor_contact_sweeps_to_damping = 0U;
    std::uint64_t playfield_contact_sweeps_to_damping = 0U;
    std::uint64_t normal_actor_collision_scans = 0U;
    std::uint64_t unavailable_normal_actor_collision_scans = 0U;
    std::uint64_t matched_normal_actor_collision_boundaries = 0U;
    std::uint64_t mismatched_normal_actor_collision_boundaries = 0U;
    std::uint64_t unresolved_normal_actor_collision_boundaries = 0U;
    std::optional<SingleSlotActorCollisionProbeSeed>
        active_single_slot_actor_collision_probe;
    std::uint64_t single_slot_actor_collision_probe_entries = 0U;
    std::uint64_t single_slot_actor_collision_scans = 0U;
    std::uint64_t unavailable_single_slot_actor_collision_scans = 0U;
    std::uint64_t single_slot_actor_collision_contact_returns = 0U;
    std::uint64_t single_slot_confirmed_actor_contacts = 0U;
    std::uint64_t matched_single_slot_actor_collision_contacts = 0U;
    std::uint64_t mismatched_single_slot_actor_collision_contacts = 0U;
    std::uint64_t single_slot_confirmed_contact_vertical_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_shape_asset_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_horizontal_world_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_bounds_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_mask_asset_misses = 0U;
    std::uint64_t single_slot_confirmed_contact_empty_mask_misses = 0U;
    std::uint64_t unavailable_single_slot_confirmed_contact_scans = 0U;
    std::array<SingleSlotActorCollisionHandlerEvidence, 8U>
        single_slot_actor_collision_handlers{};
    std::uint64_t unclassified_single_slot_actor_collision_contacts = 0U;
    std::array<SingleSlotActorCollisionMaskMismatchEvidence, 8U>
        single_slot_actor_collision_mask_mismatches{};
    std::uint64_t unclassified_single_slot_actor_collision_mask_mismatches = 0U;
    std::uint64_t single_slot_collision_mask_stage_entries = 0U;
    std::uint64_t unavailable_single_slot_collision_mask_stages = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_indices = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_cells = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_lookups = 0U;
    std::uint64_t matched_single_slot_collision_mask_stage_decisions = 0U;
    std::uint64_t unavailable_playfield_contact_responses = 0U;
    std::uint64_t unavailable_playfield_contact_markers = 0U;
    std::uint64_t signed_playfield_contact_responses = 0U;
    std::uint64_t signed_playfield_contact_marked_retries = 0U;
    std::uint64_t signed_playfield_contact_unmarked_responses = 0U;
    std::uint64_t clear_playfield_contact_responses = 0U;
    std::uint64_t playfield_contacts_without_cf33_response = 0U;
    std::uint64_t signed_playfield_contact_rearms = 0U;
    std::uint64_t clear_playfield_contact_rearms = 0U;
    std::uint64_t signed_playfield_contact_damping_paths = 0U;
    std::uint64_t clear_playfield_contact_damping_paths = 0U;
    std::uint64_t signed_playfield_contact_clear_paths = 0U;
    std::uint64_t clear_playfield_contact_clear_paths = 0U;
    std::optional<MarkedPlayfieldRetryRestoreSeed>
        pending_marked_playfield_retry_restore;
    std::uint64_t marked_playfield_retry_restore_candidates = 0U;
    std::uint64_t observed_marked_playfield_retry_restores = 0U;
    std::uint64_t verified_marked_playfield_retry_restores = 0U;
    std::uint64_t mismatched_marked_playfield_retry_restores = 0U;
    std::uint64_t unavailable_marked_playfield_retry_restore_caches = 0U;
    std::uint64_t superseded_marked_playfield_retry_restores = 0U;
};

} // namespace devils_crush::app::host
