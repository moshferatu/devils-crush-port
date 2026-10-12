// SPDX-License-Identifier: GPL-3.0-or-later
#include "gameplay_bootstrap_shadow.hpp"

namespace devils_crush::app::host {

void NativeGameplayBootstrapShadow::observe_playfield_collision_source(
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
void NativeGameplayBootstrapShadow::observe_short_actor_pass_entry(
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
void NativeGameplayBootstrapShadow::observe_short_actor_pass_call_site(const RoutineEntry& entry) noexcept
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

void NativeGameplayBootstrapShadow::observe_motion_entry(
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

void NativeGameplayBootstrapShadow::observe_motion_completion(
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

void NativeGameplayBootstrapShadow::observe_gameplay_completion(
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

void NativeGameplayBootstrapShadow::observe(
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

[[nodiscard]] std::optional<std::uint16_t>
NativeGameplayBootstrapShadow::palette_fade_caller_pc(
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

[[nodiscard]] std::uint8_t NativeGameplayBootstrapShadow::palette_fade_set_nz(
    const std::uint8_t status,
    const std::uint8_t value) noexcept
{
    auto result = static_cast<std::uint8_t>(status & 0x7DU);
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | 0x02U);
    }
    return static_cast<std::uint8_t>(result | (value & 0x80U));
}

void NativeGameplayBootstrapShadow::palette_fade_adc(
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

void NativeGameplayBootstrapShadow::observe_palette_fade_block_entry(
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

void NativeGameplayBootstrapShadow::observe_palette_fade_block_completion(
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

void NativeGameplayBootstrapShadow::observe_palette_fade_terminal_entry(
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

} // namespace devils_crush::app::host
