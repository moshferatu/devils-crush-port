// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording_session.hpp"

#include "census_writers.hpp"
#include "coverage_reports.hpp"
#include "deterministic_recording.hpp"
#include "high_score_autosave.hpp"
#include "native_registry_install.hpp"
#include "recording_plans.hpp"
#include "registry_reports.hpp"
#include "routine_replay.hpp"
#include "trace_writers.hpp"

namespace devils_crush::app::host {

// Runs the scripted recording the options name, or replays a captured
// session in its place, with whichever hooks, traces and censuses they ask
// for.
[[nodiscard]] int run_recording_session(
    PceRuntime& runtime,
    const CommandLine& options,
    const std::optional<InteractiveSessionReplay>& session_replay,
    const std::vector<std::byte>& replacement_rom)
{
    const auto runtime_info = runtime.info();
    const RecordingPlan session_replay_plan{
        .format = "devils-crush-interactive-session-replay-v1",
        .label = "Interactive session replay",
        .input_script = "captured processed pad stream",
        .frame_count = session_replay
            ? static_cast<int>(session_replay->inputs.size())
            : 0,
        .captured_inputs = session_replay
            ? std::span<const InputFrame>(session_replay->inputs)
            : std::span<const InputFrame>{},
        .expected_work_ram_sha256s = session_replay
                && options.playfield_collision_sources_path.empty()
            ? std::span<const std::string>(
                  session_replay->work_ram_sha256s)
            : std::span<const std::string>{},
        .restore_interactive_flipper_host = true,
        .restore_interactive_cabinet_host = options.cabinet
            || (session_replay && session_replay->cabinet_host),
    };
    const auto& recording_plan = !options.replay_session_input_path.empty()
        ? session_replay_plan
        : options.record_state_census
        ? kStateCensusRecordingPlan
        : options.record_gameplay_alternate
            ? kGameplayAlternateRecordingPlan
        : options.record_gameplay_sweep
            ? kGameplaySweepRecordingPlan
        : options.record_gameplay_crossfire
            ? kGameplayCrossfireRecordingPlan
        : options.record_gameplay_orbit
            ? kGameplayOrbitRecordingPlan
        : options.record_gameplay_lanes
            ? kGameplayLanesRecordingPlan
        : options.record_gameplay_stagger
            ? kGameplayStaggerRecordingPlan
        : options.record_gameplay_pulse
            ? kGameplayPulseRecordingPlan
        : options.record_gameplay_rally
            ? kGameplayRallyRecordingPlan
        : options.record_gameplay_slalom
            ? kGameplaySlalomRecordingPlan
        : options.record_gameplay_pendulum
            ? kGameplayPendulumRecordingPlan
        : options.record_gameplay_nudge
            ? kGameplayNudgeRecordingPlan
        : options.record_gameplay_switchback
            ? kGameplaySwitchbackRecordingPlan
        : options.record_gameplay_spiral
            ? kGameplaySpiralRecordingPlan
        : options.record_gameplay_ricochet
            ? kGameplayRicochetRecordingPlan
        : options.record_gameplay_volley
            ? kGameplayVolleyRecordingPlan
        : options.record_gameplay_whirlpool
            ? kGameplayWhirlpoolRecordingPlan
        : options.record_gameplay_cascade
            ? kGameplayCascadeRecordingPlan
        : options.record_gameplay_higharc
            ? kGameplayHigharcRecordingPlan
        : options.record_gameplay_twist
            ? kGameplayTwistRecordingPlan
        : options.record_gameplay_sidestep
            ? kGameplaySidestepRecordingPlan
        : options.record_gameplay_weave
            ? kGameplayWeaveRecordingPlan
        : options.record_gameplay_bankshot
            ? kGameplayBankshotRecordingPlan
        : options.record_gameplay_playfield
            ? kGameplayPlayfieldRecordingPlan
        : options.record_gameplay_corner
            ? kGameplayCornerRecordingPlan
        : options.record_gameplay_alley
            ? kGameplayAlleyRecordingPlan
        : options.record_gameplay_rebound
            ? kGameplayReboundRecordingPlan
        : options.record_gameplay_skew
            ? kGameplaySkewRecordingPlan
        : options.record_gameplay_helix
            ? kGameplayHelixRecordingPlan
        : options.record_gameplay_vector
            ? kGameplayVectorRecordingPlan
        : options.record_gameplay_radial
            ? kGameplayRadialRecordingPlan
        : options.record_gameplay
            ? kGameplayRecordingPlan
            : kCanonicalRecordingPlan;
    if (!options.playfield_collision_sources_path.empty()) {
        std::cout
            << "Playfield-collision source capture is diagnostic; "
               "stored replay hashes are not asserted.\n";
    }
    const auto recording_path = !options.replay_session_input_path.empty()
        ? options.replay_session_input_path.parent_path()
            / "replay.csv"
        : options.record_state_census
        ? options.state_census_path
        : options.record_gameplay_alternate
            ? options.gameplay_alternate_path
        : options.record_gameplay_sweep
            ? options.gameplay_sweep_path
        : options.record_gameplay_crossfire
            ? options.gameplay_crossfire_path
        : options.record_gameplay_orbit
            ? options.gameplay_orbit_path
        : options.record_gameplay_lanes
            ? options.gameplay_lanes_path
        : options.record_gameplay_stagger
            ? options.gameplay_stagger_path
        : options.record_gameplay_pulse
            ? options.gameplay_pulse_path
        : options.record_gameplay_rally
            ? options.gameplay_rally_path
        : options.record_gameplay_slalom
            ? options.gameplay_slalom_path
        : options.record_gameplay_pendulum
            ? options.gameplay_pendulum_path
        : options.record_gameplay_nudge
            ? options.gameplay_nudge_path
        : options.record_gameplay_switchback
            ? options.gameplay_switchback_path
        : options.record_gameplay_spiral
            ? options.gameplay_spiral_path
        : options.record_gameplay_ricochet
            ? options.gameplay_ricochet_path
        : options.record_gameplay_volley
            ? options.gameplay_volley_path
        : options.record_gameplay_whirlpool
            ? options.gameplay_whirlpool_path
        : options.record_gameplay_cascade
            ? options.gameplay_cascade_path
        : options.record_gameplay_higharc
            ? options.gameplay_higharc_path
        : options.record_gameplay_twist
            ? options.gameplay_twist_path
        : options.record_gameplay_sidestep
            ? options.gameplay_sidestep_path
        : options.record_gameplay_weave
            ? options.gameplay_weave_path
        : options.record_gameplay_bankshot
            ? options.gameplay_bankshot_path
        : options.record_gameplay_playfield
            ? options.gameplay_playfield_path
        : options.record_gameplay_corner
            ? options.gameplay_corner_path
        : options.record_gameplay_alley
            ? options.gameplay_alley_path
        : options.record_gameplay_rebound
            ? options.gameplay_rebound_path
        : options.record_gameplay_skew
            ? options.gameplay_skew_path
        : options.record_gameplay_helix
            ? options.gameplay_helix_path
        : options.record_gameplay_vector
            ? options.gameplay_vector_path
        : options.record_gameplay_radial
            ? options.gameplay_radial_path
        : options.record_gameplay
            ? options.gameplay_path
            : options.canonical_path;
    std::vector<std::uint64_t> pass_through_hits(
        options.pass_through_hooks.size());
    std::vector<std::uint64_t> replacement_hits(
        options.replace_hooks.size());
    std::vector<std::uint64_t> replacement_entries(
        options.replace_hooks.size());
    std::vector<RoutineEntry> observed_entries;
    std::vector<RoutineCall> observed_calls;
    std::vector<ObservedMemoryWrite> observed_work_ram_writes;
    std::vector<ObservedBusWrite> observed_bus_writes;
    std::vector<devils_crush::emulation::PlayfieldCollisionSource>
        playfield_collision_sources;
    std::vector<RoutineReplayCapture> routine_replays;
    routine_replays.reserve(options.routine_replays.size());
    for (const auto request : options.routine_replays) {
        routine_replays.push_back({
            .request = request,
        });
    }
    RoutineReplayCapture* active_replay = nullptr;
    bool suppress_recording_observers = false;
    for (std::size_t index = 0U;
         index < options.pass_through_hooks.size();
         ++index) {
        if (runtime.install_execution_hook({
            .routine = options.pass_through_hooks[index],
            .mode = HookMode::pass_through,
            .callback = [&pass_through_hits,
                         index](const RoutineEntry&) {
                ++pass_through_hits[index];
            },
        })) {
            continue;
        }
        std::cerr << "Could not install pass-through hook "
                  << index + 1U << ".\n";
        return EXIT_FAILURE;
    }
    for (std::size_t index = 0U;
         index < options.observe_hooks.size();
         ++index) {
        const auto replacement = std::find_if(
            options.replace_hooks.begin(),
            options.replace_hooks.end(),
            [&options, index](
                const RoutineReplayRequest& request) {
                return request.routine
                    == options.observe_hooks[index];
            });
        const bool replaces =
            replacement != options.replace_hooks.end();
        const auto replacement_index = static_cast<std::size_t>(
            std::distance(
                options.replace_hooks.begin(),
                replacement));
        devils_crush::emulation::ExecutionHook hook{
            .routine = options.observe_hooks[index],
            .mode = replaces
                ? HookMode::replace
                : HookMode::observe,
            .callback = [&runtime,
                         &observed_entries,
                         &routine_replays,
                         &playfield_collision_sources,
                         &active_replay,
                         &suppress_recording_observers](
                            const RoutineEntry& entry) {
                if (active_replay != nullptr
                    || suppress_recording_observers) {
                    return;
                }
                for (auto& replay : routine_replays) {
                    if (replay.request.routine != entry.routine) {
                        continue;
                    }
                    ++replay.natural_entry_count;
                    if (replay.natural_entry_count
                        == replay.request.ordinal) {
                        replay.checkpoint_entry = entry;
                        replay.checkpoint = runtime.serialize();
                        replay.pre_call_work_ram =
                            runtime.export_work_ram();
                    }
                }
                if (entry.routine
                        == RoutineKey{
                            .physical_bank = 0x01U,
                            .logical_pc = 0xCD74U,
                        }) {
                    const auto source =
                        devils_crush::emulation::
                            capture_playfield_collision_source(
                                entry.registers,
                                runtime.export_work_ram());
                    if (source
                        && std::find(
                               playfield_collision_sources.begin(),
                               playfield_collision_sources.end(),
                               *source)
                            == playfield_collision_sources.end()) {
                        playfield_collision_sources.push_back(*source);
                    }
                }
                observed_entries.push_back(entry);
            },
        };
        if (replaces) {
            hook.replacement_callback =
                [&replacement_hits,
                 &replacement_entries,
                 &replacement_rom,
                 &options,
                 replacement_index](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    const auto entry_number =
                        ++replacement_entries[
                            replacement_index];
                    if (!options.replace_hooks[
                             replacement_index]
                             .all_calls
                        && entry_number
                        != options.replace_hooks[
                               replacement_index]
                               .ordinal) {
                        return std::optional<
                            RoutineReplacement>{};
                    }
                    auto result = replace_native_gameplay_routine(
                        entry,
                        work_ram,
                        replacement_rom);
                    if (result
                        && options.replace_hooks[
                               replacement_index]
                               .all_calls
                        && result->checkpoints.empty()
                        && result->body_master_cycles
                            >= std::min(
                                entry.master_cycles_until_user_event,
                                entry
                                    .master_cycles_until_timer_interrupt)) {
                        result.reset();
                    }
                    if (result) {
                        ++replacement_hits[replacement_index];
                    }
                    return result;
                };
        }
        if (runtime.install_execution_hook(std::move(hook))) {
            continue;
        }
        std::cerr << "Could not install observe hook "
                  << index + 1U << ".\n";
        return EXIT_FAILURE;
    }
    for (std::size_t index = 0U;
         index < options.replace_hooks.size();
         ++index) {
        if (std::find(
                options.observe_hooks.begin(),
                options.observe_hooks.end(),
                options.replace_hooks[index].routine)
            != options.observe_hooks.end()) {
            continue;
        }
        if (runtime.install_execution_hook({
                .routine = options.replace_hooks[index].routine,
                .mode = HookMode::replace,
                .replacement_callback =
                    [&replacement_hits,
                     &replacement_entries,
                     &replacement_rom,
                     &options,
                     index](
                        const RoutineEntry& entry,
                        const std::span<const std::byte> work_ram) {
                        const auto entry_number =
                            ++replacement_entries[index];
                        if (!options.replace_hooks[index].all_calls
                            && entry_number
                            != options.replace_hooks[index]
                                   .ordinal) {
                            return std::optional<
                                RoutineReplacement>{};
                        }
                        auto result =
                            replace_native_gameplay_routine(
                                entry,
                                work_ram,
                                replacement_rom);
                        if (result
                            && options.replace_hooks[index].all_calls
                            && result->checkpoints.empty()
                            && result->body_master_cycles
                                >= std::min(
                                    entry
                                        .master_cycles_until_user_event,
                                    entry
                                        .master_cycles_until_timer_interrupt)) {
                            result.reset();
                        }
                        if (result) {
                            ++replacement_hits[index];
                        }
                        return result;
                    },
            })) {
            continue;
        }
        std::cerr << "Could not install replacement hook "
                  << index + 1U << ".\n";
        return EXIT_FAILURE;
    }
    NativeRegistryDeployment registry_deployment;
    if (options.replace_registry) {
        std::vector<RoutineKey> claimed_routines =
            options.observe_hooks;
        claimed_routines.reserve(
            claimed_routines.size() + options.replace_hooks.size());
        for (const auto& request : options.replace_hooks) {
            claimed_routines.push_back(request.routine);
        }
        if (recording_plan.restore_interactive_flipper_host) {
            claimed_routines.push_back(kShooterGateInstruction);
        }
        if (!install_native_registry(
                runtime,
                options,
                replacement_rom,
                claimed_routines,
                registry_deployment)) {
            return EXIT_FAILURE;
        }
    }
    // The checkpoint is intentionally taken before the ROM's boot
    // copy at 01:C107. Reinstall the same restore hook the live host
    // uses so frame one gets the player's table rather than ROM
    // defaults. New captures carry that table beside initial.state;
    // session v1 artifacts without it use the still-current host
    // table as the best available compatibility path.
    std::optional<HighScoreAutosave> session_replay_high_scores;
    if (session_replay) {
        const auto high_score_path =
            session_replay->initial_high_score_path.empty()
            ? kHighScorePath
            : session_replay->initial_high_score_path;
        session_replay_high_scores.emplace(
            open_high_score_autosave(high_score_path));
        if (!install_high_score_restore(
                runtime, *session_replay_high_scores)) {
            std::cerr << "Could not restore the interactive high-score "
                         "hook for session replay.\n";
            return EXIT_FAILURE;
        }
    }
    if (!options.routine_call_trace_path.empty()) {
        for (std::size_t index = 0U;
             index < options.observe_hooks.size();
             ++index) {
            if (runtime.install_routine_call_observer({
                .routine = options.observe_hooks[index],
                .callback = [&runtime,
                             &observed_calls,
                             &routine_replays,
                             &active_replay,
                             &suppress_recording_observers](
                                const RoutineCall& call) {
                    if (active_replay != nullptr) {
                        if (call.entry.routine
                                == active_replay->request.routine
                            && !active_replay->replayed_call) {
                            active_replay->replayed_call = call;
                            active_replay
                                ->replayed_post_call_work_ram =
                                runtime.export_work_ram();
                        }
                        return;
                    }
                    if (!suppress_recording_observers) {
                        observed_calls.push_back(call);
                    }
                    const auto replay = std::find_if(
                        routine_replays.begin(),
                        routine_replays.end(),
                        [&call](
                            const RoutineReplayCapture& capture) {
                            return !capture.checkpoint.core_state.empty()
                                && !capture.original_call
                                && capture.checkpoint_entry
                                        .absolute_master_cycle
                                    == call.entry
                                        .absolute_master_cycle
                                && capture.request.routine
                                    == call.entry.routine;
                        });
                    if (replay != routine_replays.end()) {
                        replay->original_call = call;
                        replay->original_post_call_work_ram =
                            runtime.export_work_ram();
                    }
                },
            })) {
                continue;
            }
            std::cerr
                << "Could not install routine-call observer "
                << index + 1U << ".\n";
            return EXIT_FAILURE;
        }
    }
    if (!options.work_ram_write_ranges.empty()
        && !runtime.set_work_ram_write_observer(
            [&observed_work_ram_writes,
              &ranges = options.work_ram_write_ranges,
              &suppress_recording_observers](
                const ObservedMemoryWrite& write) {
                if (suppress_recording_observers) {
                    return;
                }
                const bool selected = std::any_of(
                    ranges.begin(),
                    ranges.end(),
                    [&write](const LogicalAddressRange range) {
                        return write.logical_address >= range.first
                            && write.logical_address <= range.last;
                    });
                if (selected) {
                    observed_work_ram_writes.push_back(write);
                }
            })) {
        std::cerr
            << "Could not install work-RAM write observer.\n";
        return EXIT_FAILURE;
    }
    if (!options.bus_write_ranges.empty()
        && !runtime.set_bus_write_observer(
            [&observed_bus_writes,
              &ranges = options.bus_write_ranges,
              &suppress_recording_observers](
                const ObservedBusWrite& write) {
                if (suppress_recording_observers) {
                    return;
                }
                const bool selected = std::any_of(
                    ranges.begin(),
                    ranges.end(),
                    [&write](const LogicalAddressRange range) {
                        return write.logical_address >= range.first
                            && write.logical_address <= range.last;
                    });
                if (selected) {
                    observed_bus_writes.push_back(write);
                }
            })) {
        std::cerr << "Could not install bus write observer.\n";
        return EXIT_FAILURE;
    }

    if (!options.instruction_profile_path.empty()
        && !runtime.set_instruction_profiling(true)) {
        std::cerr << "Could not enable instruction profiling.\n";
        return EXIT_FAILURE;
    }

    const int result = run_deterministic_recording(
        runtime,
        recording_path,
        options.canonical_work_ram_path,
        options.canonical_framebuffer_path,
        options.canonical_audio_path,
        runtime_info.upstream_revision,
        recording_plan,
        options.work_ram_patches,
        options.table_vram_census_path,
        replacement_rom,
        session_replay ? session_replay->initial_state_path
                       : std::filesystem::path{},
        [&]() {
            // A frame failure used to discard the only evidence of
            // the routine handoffs immediately before it.  Preserve
            // explicitly requested traces so a captured session can
            // diagnose the failing frame as well as a completed one.
            if (!options.routine_trace_path.empty()) {
                static_cast<void>(write_routine_trace(
                    options.routine_trace_path,
                    options.observe_hooks,
                    observed_entries,
                    runtime_info.upstream_revision));
            }
            if (!options.routine_call_trace_path.empty()) {
                static_cast<void>(write_routine_call_trace(
                    options.routine_call_trace_path,
                    options.observe_hooks,
                    observed_calls,
                    runtime_info.upstream_revision));
            }
            if (options.replace_registry) {
                print_native_registry_summary(
                    registry_deployment,
                    runtime.replacement_nesting(),
                    runtime);
            }
        });
    if (result != EXIT_SUCCESS) {
        return result;
    }
    for (std::size_t index = 0U;
         index < pass_through_hits.size();
         ++index) {
        if (pass_through_hits[index] == 0U) {
            std::cerr << "Pass-through hook " << index + 1U
                      << " was never entered.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Pass-through hook " << index + 1U
                  << " entries: " << pass_through_hits[index]
                  << '\n';
    }
    for (std::size_t index = 0U;
         index < replacement_hits.size();
         ++index) {
        if (replacement_hits[index] == 0U) {
            std::cerr << "Native replacement "
                      << index + 1U
                      << " was never entered.\n";
            return EXIT_FAILURE;
        }
        const auto request = options.replace_hooks[index];
        std::cout << "Native replacement "
                  << std::uppercase << std::hex
                  << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(
                         request.routine.physical_bank)
                  << ':' << std::setw(4)
                  << request.routine.logical_pc << std::dec << '@';
        if (request.all_calls) {
            std::cout << "all";
        } else {
            std::cout << request.ordinal;
        }
        std::cout << std::setfill(' ') << " entries: "
                  << replacement_hits[index];
        if (request.all_calls) {
            std::cout << " of " << replacement_entries[index];
        }
        std::cout << '\n';
    }
    if (!options.instruction_profile_path.empty()
        && !write_instruction_profile(
            options.instruction_profile_path,
            runtime.export_instruction_profile(),
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    // The whole-ROM profile includes time inside accepted native
    // transcripts. Rank future ports from the actual interpreted
    // intervals, including guarded and core/host seams, instead.
    if (!options.instruction_profile_path.empty()
        && options.replace_registry) {
        auto residue_path = options.instruction_profile_path;
        residue_path += ".interpreter.csv";
        if (!write_instruction_profile(
                residue_path,
                runtime.export_interpreted_instruction_profile(),
                runtime_info.upstream_revision,
                "interpreter-residue")) {
            return EXIT_FAILURE;
        }
    }
    if (!options.abandonment_census_path.empty()
        && !write_abandonment_census(
            options.abandonment_census_path,
            runtime.export_abandonment_census(),
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    if (options.replace_registry) {
        print_native_registry_summary(
            registry_deployment,
            runtime.replacement_nesting(),
            runtime);
    }
    if (!options.native_cycle_census_path.empty()
        && !write_native_cycle_census(
            options.native_cycle_census_path,
            registry_deployment.rows,
            registry_deployment.span,
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    for (std::size_t index = 0U;
         index < options.observe_hooks.size();
         ++index) {
        const auto routine = options.observe_hooks[index];
        const auto hit_count = static_cast<std::size_t>(std::count_if(
            observed_entries.begin(),
            observed_entries.end(),
            [routine](const RoutineEntry& entry) {
                return entry.routine == routine;
            }));
        if (hit_count == 0U) {
            std::cerr << "Observe hook " << index + 1U
                      << " was never entered.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Observe hook " << index + 1U
                  << " entries: " << hit_count << '\n';
    }
    if (!options.routine_trace_path.empty()
        && !write_routine_trace(
            options.routine_trace_path,
            options.observe_hooks,
            observed_entries,
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    if (!options.routine_call_trace_path.empty()
        && !write_routine_call_trace(
            options.routine_call_trace_path,
            options.observe_hooks,
            observed_calls,
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    if (!options.playfield_collision_sources_path.empty()
        && !write_playfield_collision_sources(
            options.playfield_collision_sources_path,
            playfield_collision_sources,
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    if (!options.work_ram_write_trace_path.empty()
        && !write_work_ram_write_trace(
            options.work_ram_write_trace_path,
            options.work_ram_write_ranges,
            observed_work_ram_writes,
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    if (!options.bus_write_trace_path.empty()
        && !write_bus_write_trace(
            options.bus_write_trace_path,
            options.bus_write_ranges,
            observed_bus_writes,
            runtime_info.upstream_revision)) {
        return EXIT_FAILURE;
    }
    if (options.report_psg_coverage) {
        report_psg_coverage(runtime);
    }
    if (options.report_vdc_coverage) {
        report_vdc_coverage(runtime);
    }
    if (options.report_vce_coverage) {
        report_vce_coverage(runtime);
    }
    if (options.report_irq_coverage) {
        report_irq_coverage(runtime);
    }
    if (options.report_scheduler_coverage) {
        report_scheduler_coverage(runtime);
    }
    if (options.report_boundary_coverage) {
        report_boundary_coverage(runtime);
    }
    if (options.report_idle_coverage) {
        report_idle_coverage(runtime);
    }
    if (options.report_pad_coverage) {
        report_pad_coverage(runtime);
    }
    if (!routine_replays.empty()) {
        const int replay_result = verify_routine_replays(
            runtime,
            recording_plan,
            routine_replays,
            active_replay,
            suppress_recording_observers);
        if (replay_result != EXIT_SUCCESS) {
            return replay_result;
        }
        if (!options.routine_replay_corpus_output_directory.empty()
            && !write_routine_replay_corpora(
                options.routine_replay_corpus_output_directory,
                routine_replays,
                runtime_info.upstream_revision)) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
