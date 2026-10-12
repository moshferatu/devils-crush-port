// SPDX-License-Identifier: GPL-3.0-or-later
#include "host/host_common.hpp"
#include "host/census_writers.hpp"
#include "host/checks_cabinet.hpp"
#include "host/checks_flippers.hpp"
#include "host/checks_launch.hpp"
#include "host/checks_oracle.hpp"
#include "host/command_line.hpp"
#include "host/coverage_reports.hpp"
#include "host/deterministic_recording.hpp"
#include "host/high_score_autosave.hpp"
#include "host/host_files.hpp"
#include "host/host_input.hpp"
#include "host/interactive_session.hpp"
#include "host/native_registry_install.hpp"
#include "host/play_assists.hpp"
#include "host/recording_plans.hpp"
#include "host/registry_reports.hpp"
#include "host/routine_replay.hpp"
#include "host/semantic_fixtures.hpp"
#include "host/trace_writers.hpp"
#include "host/windows_host.hpp"

#include <SDL3/SDL_main.h>

using namespace devils_crush::app::host;

int main(const int argc, char* argv[])
{
    // Before anything is read: the options take normal play's settings from
    // devils-crush.ini, which an earlier build kept in a subfolder.
    const auto migration =
        devils_crush::core::migrate_legacy_data_folder({});
    for (const auto& moved : migration.moved) {
        std::cout << "Moved " << moved << '\n';
    }
    for (const auto& problem : migration.problems) {
        std::cerr << "Could not move " << problem << '\n';
    }
    auto options = parse_command_line(argc, argv);
    if (!options.error.empty()) {
        std::cerr << options.error << "\n\n";
        print_usage();
        return EXIT_FAILURE;
    }
    if (options.show_help) {
        print_usage();
        return EXIT_SUCCESS;
    }
    // Before the ROM is even looked at, because this one is about the cabinet's
    // hardware rather than the game's: it is what somebody runs when the
    // plunger does nothing, and asking them for a ROM first would be beside
    // the point.
    if (options.cabinet_plunger_probe) {
        return run_plunger_probe();
    }
    if (options.verify_unsigned_square_root) {
        return run_unsigned_square_root_semantic_fixture();
    }
    if (options.verify_unsigned_word_multiply_partial) {
        return run_unsigned_word_multiply_partial_semantic_fixture();
    }
    if (options.verify_signed_word_normalizer) {
        return run_signed_word_normalizer_semantic_fixture();
    }
    if (options.verify_signed_word_multiply) {
        return run_signed_word_multiply_semantic_fixture();
    }
    if (options.verify_side_gate_deadline_check) {
        return run_side_gate_deadline_check_semantic_fixture();
    }

    const auto validation =
        devils_crush::rom::validate_file(options.rom_path);
    print_validation(validation);
    if (!validation.supported()) {
        return EXIT_FAILURE;
    }
    if (options.validate_only) {
        return EXIT_SUCCESS;
    }

    std::optional<InteractiveSessionReplay> session_replay;
    if (!options.replay_session_input_path.empty()) {
        session_replay = read_interactive_session_replay(
            options.replay_session_input_path);
        if (!session_replay) {
            return EXIT_FAILURE;
        }
    }

    try {
        const auto runtime = devils_crush::emulation::create_pce_runtime();
        const auto runtime_info = runtime->info();
        std::cout << "Runtime:  " << runtime_info.name << '\n'
                  << "Revision: " << runtime_info.upstream_revision << '\n';

        // Installed before the ROM loads so the emulator hands the model its
        // output buffers as it builds them.
        if (!options.emulated_psg && !runtime->set_native_psg(true)) {
            std::cerr << "Could not install the native sound generator.\n";
            return EXIT_FAILURE;
        }

        if (options.music_voice_overlay) {
            const auto overlay_rom = read_rom_bytes(options.rom_path);
            if (!overlay_rom
                || !runtime->set_music_voice_overlay(true, *overlay_rom)) {
                std::cerr << "Could not install the experimental music "
                             "voice overlay.\n";
                return EXIT_FAILURE;
            }
            std::cout << "Music voice overlay: enabled (experimental)\n";
        }

        // Installed before the ROM for a different reason: the model takes
        // its whole state from the machine's power-on, and the machine
        // powers on once, while loading.
        if (!options.emulated_vdc && !runtime->set_native_vdc(true)) {
            std::cerr << "Could not install the native video display "
                         "controller.\n";
            return EXIT_FAILURE;
        }

        // And before the ROM for the same reason. Installed after the
        // controller only for reading order: the boundary decides which
        // controller the encoder clocks on every call, so either order lands
        // in the same place.
        if (!options.emulated_vce && !runtime->set_native_vce(true)) {
            std::cerr << "Could not install the native colour encoder.\n";
            return EXIT_FAILURE;
        }

        // Before the ROM for a sharper version of the same reason: the
        // processor mirrors three of this device's values, and the mirror is
        // only refreshed by a dispatch. Installing after the machine had
        // powered on would leave the emulator holding a divider and a
        // request register the model never wrote.
        if (!options.emulated_irq && !runtime->set_native_irq(true)) {
            std::cerr << "Could not install the native timer and interrupt "
                         "controller.\n";
            return EXIT_FAILURE;
        }

        // Before the ROM for a sharper reason still. Installing this one moves
        // where the machine's clock lives, so a machine already running would
        // find its clock replaced underneath it rather than merely one of its
        // devices handed over.
        if (!options.emulated_scheduler
            && !runtime->set_native_scheduler(true)) {
            std::cerr << "Could not install the native clock and event "
                         "scheduler.\n";
            return EXIT_FAILURE;
        }

        // Before the ROM for the narrowest reason of the six. This model
        // hands over no storage and leaves no mirror, so nothing would be
        // stranded by a late install -- except the one thing that matters:
        // the reset vector a machine takes at power-on is the only event
        // that ever puts a reset into the latch.
        if (!options.emulated_boundary
            && !runtime->set_native_boundary(true)) {
            std::cerr << "Could not install the native instruction "
                         "boundary.\n";
            return EXIT_FAILURE;
        }

        // And this one has no such reason, which is what makes it the odd
        // entry of the seven: it hands over no work, so there is nothing for
        // a machine to find missing whenever it is installed. `00:E323` is
        // the frame-sync wait -- the two instructions the profile measures at
        // 45.2% of the census route -- and the model verifies the bytes are
        // there before it claims anything about them.
        //
        // Installable at any point, alone among the seven, because it hands
        // over nothing: it is put here only so that the reporting on exit
        // reads in the same order as the installs above.
        if (!options.emulated_idle
            && !runtime->set_native_idle_skip(
                devils_crush::cpu::HuC6280IdleSkip::Loop{
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE323U,
                })) {
            std::cerr << "Could not install the frame-sync fast-forward.\n";
            return EXIT_FAILURE;
        }

        // Before the ROM, and for a reason that is the machine's rather than
        // the model's: the emulator reports what is plugged into each port
        // exactly once, when it builds its devices at load. The boundary
        // remembers those reports and replays them into whichever model is
        // current, so a later install is not a model that missed them -- but
        // installing here means it never has to be replayed.
        if (!options.emulated_pad && !runtime->set_native_pad(true)) {
            std::cerr << "Could not install the native controller port.\n";
            return EXIT_FAILURE;
        }

        const auto load_result = runtime->load_rom(options.rom_path);
        if (!load_result.loaded) {
            std::cerr << load_result.message << '\n';
            return EXIT_FAILURE;
        }
        std::cout << load_result.message << '\n';

        const double nudge_multiplier = session_replay
            ? session_replay->nudge_multiplier : options.nudge_multiplier;
        if (session_replay && options.nudge_multiplier != 1.0
            && options.nudge_multiplier != nudge_multiplier) {
            std::cerr << "Nudge multiplier differs from the captured session.\n";
            return EXIT_FAILURE;
        }
        options.nudge_multiplier = nudge_multiplier;
        if (!install_nudge_multiplier(*runtime, nudge_multiplier)) {
            std::cerr << "Could not install nudge multiplier.\n";
            return EXIT_FAILURE;
        }
        if (nudge_multiplier != 1.0) {
            std::cout << "Nudge force multiplier: " << nudge_multiplier << "x\n";
        }

        const auto replacement_rom =
            options.replace_hooks.empty() && !options.replace_registry
            && options.table_vram_census_path.empty()
            ? std::optional<std::vector<std::byte>>{
                  std::vector<std::byte>{}}
            : read_rom_bytes(options.rom_path);
        if (!replacement_rom) {
            return EXIT_FAILURE;
        }

        if (!options.routine_replay_corpus_paths.empty()) {
            return run_routine_replay_corpus(
                *runtime,
                options,
                *replacement_rom,
                runtime_info.upstream_revision);
        }

        if (!options.cabinet_state_dump_directory.empty()) {
            return run_state_dump(
                *runtime, options.rom_path, kQuickStatePath,
                options.cabinet_state_dump_directory);
        }
        if (options.cabinet_attribute_check) {
            return run_attribute_check(*runtime);
        }
        if (options.cabinet_palette_check) {
            return run_palette_check(*runtime);
        }
        if (options.cabinet_seed_check) {
            // Both ways: if the seeded and learned composes already disagreed
            // before the cull was lifted, lifting it did not cause this.
            if (run_seed_check(*runtime, options.rom_path, false)
                != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            return run_seed_check(*runtime, options.rom_path, true);
        }
        if (options.cabinet_drain_probe) {
            return run_drain_probe(
                *runtime,
                options.rom_path,
                kQuickStatePath,
                true,
                options.emulate_only ? nullptr : &options);
        }
        if (options.cabinet_nudge_probe) {
            const auto* const registry =
                options.emulate_only ? nullptr : &options;
            NudgePassResult without;
            NudgePassResult with;
            if (run_nudge_probe(
                    *runtime, options.rom_path, kQuickStatePath, registry,
                    true, false, without)
                != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            runtime->reset();
            if (run_nudge_probe(
                    *runtime, options.rom_path, kQuickStatePath, registry,
                    true, true, with)
                != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            report_nudge_effect(without, with);
            return EXIT_SUCCESS;
        }
        if (options.cabinet_plunger_check) {
            return run_plunger_check(*runtime);
        }
        if (options.cabinet_ball_drag_check) {
            const auto drag_rom = read_rom_bytes(options.rom_path);
            return drag_rom
                ? run_ball_drag_check(
                    *runtime, *drag_rom,
                    options.emulate_only ? nullptr : &options)
                : EXIT_FAILURE;
        }
        if (options.cabinet_launch_flipper_probe) {
            const auto probe_rom = read_rom_bytes(options.rom_path);
            if (!probe_rom) {
                return EXIT_FAILURE;
            }
            LaunchFlipperPass pass;
            for (const auto& [uncull, restore] :
                 std::array<std::pair<bool, bool>, 4>{
                     std::pair{false, false},
                     std::pair{true, false},
                     std::pair{false, true},
                     std::pair{true, true},
                 }) {
                if (run_launch_flipper_probe(
                        *runtime,
                        options.emulate_only ? nullptr : &options,
                        *probe_rom,
                        uncull,
                        restore,
                        pass)
                    != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
            }
            return EXIT_SUCCESS;
        }
        if (options.high_score_check) {
            return run_high_score_check(*runtime);
        }
        if (!options.cabinet_view_probe_directory.empty()) {
            return run_cabinet_view_probe(
                *runtime,
                options.rom_path,
                options.cabinet_view_probe_directory);
        }
        if (!options.cabinet_motion_check_directory.empty()) {
            return run_cabinet_motion_check(*runtime, options.rom_path,
                options.cabinet_motion_check_directory, options.cabinet_motion_gpu_check);
        }
        if (options.cabinet_launch_check) {
            std::cout << "=== launch keys ===\n";
            if (run_launch_check(*runtime, Button::right, "Right")
                != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            return run_launch_check(*runtime, Button::run, "Return");
        }
        if (options.cabinet_flipper_trace) {
            if (run_flipper_trace(*runtime, kQuickStatePath, false)
                != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            return run_flipper_trace(*runtime, kQuickStatePath, true);
        }
        if (options.cabinet_freeze_census) {
            return run_freeze_census(
                *runtime,
                options.rom_path,
                options.cabinet_freeze_census_state);
        }
        if (options.cabinet_oracle) {
            return run_cabinet_oracle(*runtime, options.rom_path);
        }
        if (!options.cabinet_dump_route_directory.empty()) {
            return dump_cabinet_route(
                *runtime,
                options.rom_path,
                options.cabinet_dump_route_directory);
        }

        if (!options.replay_session_input_path.empty()
            || options.record_canonical || options.record_gameplay
            || options.record_gameplay_alternate
            || options.record_gameplay_sweep
            || options.record_gameplay_crossfire
            || options.record_gameplay_orbit
            || options.record_gameplay_lanes
            || options.record_gameplay_stagger
            || options.record_gameplay_pulse
            || options.record_gameplay_rally
            || options.record_gameplay_slalom
            || options.record_gameplay_pendulum
            || options.record_gameplay_nudge
            || options.record_gameplay_switchback
            || options.record_gameplay_spiral
            || options.record_gameplay_ricochet
            || options.record_gameplay_volley
            || options.record_gameplay_whirlpool
            || options.record_gameplay_cascade
            || options.record_gameplay_higharc
            || options.record_gameplay_twist
            || options.record_gameplay_sidestep
            || options.record_gameplay_weave
            || options.record_gameplay_bankshot
            || options.record_gameplay_playfield
            || options.record_gameplay_corner
            || options.record_gameplay_alley
            || options.record_gameplay_rebound
            || options.record_gameplay_skew
            || options.record_gameplay_helix
            || options.record_gameplay_vector
            || options.record_gameplay_radial
            || options.record_state_census) {
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
                if (runtime->install_execution_hook({
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
                                replay.checkpoint = runtime->serialize();
                                replay.pre_call_work_ram =
                                    runtime->export_work_ram();
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
                                        runtime->export_work_ram());
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
                                *replacement_rom);
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
                if (runtime->install_execution_hook(std::move(hook))) {
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
                if (runtime->install_execution_hook({
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
                                        *replacement_rom);
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
                        *runtime,
                        options,
                        *replacement_rom,
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
                        *runtime, *session_replay_high_scores)) {
                    std::cerr << "Could not restore the interactive high-score "
                                 "hook for session replay.\n";
                    return EXIT_FAILURE;
                }
            }
            if (!options.routine_call_trace_path.empty()) {
                for (std::size_t index = 0U;
                     index < options.observe_hooks.size();
                     ++index) {
                    if (runtime->install_routine_call_observer({
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
                                        runtime->export_work_ram();
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
                                    runtime->export_work_ram();
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
                && !runtime->set_work_ram_write_observer(
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
                && !runtime->set_bus_write_observer(
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
                && !runtime->set_instruction_profiling(true)) {
                std::cerr << "Could not enable instruction profiling.\n";
                return EXIT_FAILURE;
            }

            const int result = run_deterministic_recording(
                *runtime,
                recording_path,
                options.canonical_work_ram_path,
                options.canonical_framebuffer_path,
                options.canonical_audio_path,
                runtime_info.upstream_revision,
                recording_plan,
                options.work_ram_patches,
                options.table_vram_census_path,
                *replacement_rom,
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
                            runtime->replacement_nesting(),
                            *runtime);
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
                    runtime->export_instruction_profile(),
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
                        runtime->export_interpreted_instruction_profile(),
                        runtime_info.upstream_revision,
                        "interpreter-residue")) {
                    return EXIT_FAILURE;
                }
            }
            if (!options.abandonment_census_path.empty()
                && !write_abandonment_census(
                    options.abandonment_census_path,
                    runtime->export_abandonment_census(),
                    runtime_info.upstream_revision)) {
                return EXIT_FAILURE;
            }
            if (options.replace_registry) {
                print_native_registry_summary(
                    registry_deployment,
                    runtime->replacement_nesting(),
                    *runtime);
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
                report_psg_coverage(*runtime);
            }
            if (options.report_vdc_coverage) {
                report_vdc_coverage(*runtime);
            }
            if (options.report_vce_coverage) {
                report_vce_coverage(*runtime);
            }
            if (options.report_irq_coverage) {
                report_irq_coverage(*runtime);
            }
            if (options.report_scheduler_coverage) {
                report_scheduler_coverage(*runtime);
            }
            if (options.report_boundary_coverage) {
                report_boundary_coverage(*runtime);
            }
            if (options.report_idle_coverage) {
                report_idle_coverage(*runtime);
            }
            if (options.report_pad_coverage) {
                report_pad_coverage(*runtime);
            }
            if (!routine_replays.empty()) {
                const int replay_result = verify_routine_replays(
                    *runtime,
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
        if (options.backup_ram_test) {
            return run_backup_ram_reload_test(
                *runtime,
                options.rom_path,
                options.backup_ram_test_path);
        }
        if (options.smoke_test) {
            return run_smoke_test(*runtime);
        }
        // The registry is the interactive default: a session on the cabinet
        // runs every routine the regressions have proven, and --emulate-only
        // falls back to the untouched emulated original.
        const CommandLine* const host_registry =
            options.emulate_only ? nullptr : &options;
        if (options.cabinet_smoke_test) {
            return run_windows_host(
                *runtime,
                options.rom_path,
                true,
                options.bonus_stage,
                options.cabinet_presentation_check ? 1920 : options.bonus_stage != 0U
                    ? kBonusStageSmokeFrames
                    : kSmokeTestFrames,
                host_registry,
                options.cabinet_layers,
                options.cabinet_overlay_stats,
                options.capture_session_directory,
                options.native_gameplay_bootstrap_shadow,
                  options.nudge_multiplier,
                  options.cabinet_presentation_check);
        }

        (void)load_backup_ram(*runtime, kBackupRamPath);
        const int host_result = run_windows_host(
            *runtime,
            options.rom_path,
            options.cabinet,
            options.bonus_stage,
            0,
            host_registry,
            options.cabinet_layers,
            options.cabinet_overlay_stats,
            options.capture_session_directory,
            options.native_gameplay_bootstrap_shadow,
            options.nudge_multiplier,
            false,
            options.drag_ball);
        const bool save_succeeded =
            save_backup_ram(*runtime, kBackupRamPath);
        return host_result == EXIT_SUCCESS && save_succeeded
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    } catch (const std::exception& exception) {
        std::cerr << "Could not start the PCE runtime: " << exception.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}
