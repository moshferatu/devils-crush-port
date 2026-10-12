// SPDX-License-Identifier: GPL-3.0-or-later
#include "gameplay_bootstrap_shadow.hpp"

namespace devils_crush::app::host {

void NativeGameplayBootstrapShadow::observe_collision_sweep_entry(
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

void NativeGameplayBootstrapShadow::begin_single_slot_actor_collision_probe(
    const RoutineEntry& entry) noexcept
{
    active_single_slot_actor_collision_probe = {
        .entry_absolute_master_cycle = entry.absolute_master_cycle,
    };
    ++single_slot_actor_collision_probe_entries;
}

void NativeGameplayBootstrapShadow::observe_single_slot_actor_collision_scan(
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
void NativeGameplayBootstrapShadow::observe_single_slot_actor_collision_mask_stage(
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

void NativeGameplayBootstrapShadow::observe_single_slot_confirmed_actor_contact() noexcept
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

void NativeGameplayBootstrapShadow::observe_single_slot_actor_collision_result(
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

void NativeGameplayBootstrapShadow::record_single_slot_actor_collision_disposition(
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

void NativeGameplayBootstrapShadow::record_single_slot_actor_collision_handler(
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

void NativeGameplayBootstrapShadow::record_single_slot_actor_collision_mask_mismatch(
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

void NativeGameplayBootstrapShadow::observe_playfield_collision_completion(const RoutineCall& call) noexcept
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
void NativeGameplayBootstrapShadow::observe_playfield_contact_response_entry(
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
void NativeGameplayBootstrapShadow::observe_collision_retry_entry(
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

void NativeGameplayBootstrapShadow::observe_marked_playfield_retry_restore(
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

void NativeGameplayBootstrapShadow::finish_collision_sweep(const CollisionSweepExit exit) noexcept
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

void NativeGameplayBootstrapShadow::record_playfield_contact_response_exit(
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

void NativeGameplayBootstrapShadow::observe_collision_loop_split_entry(
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

void NativeGameplayBootstrapShadow::observe_collision_loop_position_entry(
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

void NativeGameplayBootstrapShadow::observe_collision_loop_position_completion(
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

void NativeGameplayBootstrapShadow::observe_collision_loop_damping_entry(
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

void NativeGameplayBootstrapShadow::observe_collision_loop_damping_completion(
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

} // namespace devils_crush::app::host
