// SPDX-License-Identifier: GPL-3.0-or-later
#include "gameplay_bootstrap_shadow.hpp"

namespace devils_crush::app::host {

void NativeGameplayBootstrapShadow::report() const
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

void NativeGameplayBootstrapShadow::report_collision_loop_shadow() const
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

void NativeGameplayBootstrapShadow::report_motion_routine_shadow() const
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

} // namespace devils_crush::app::host
