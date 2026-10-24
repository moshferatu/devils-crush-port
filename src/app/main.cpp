// SPDX-License-Identifier: GPL-3.0-or-later
#include "host/host_common.hpp"
#include "host/checks_cabinet.hpp"
#include "host/checks_flippers.hpp"
#include "host/checks_launch.hpp"
#include "host/checks_oracle.hpp"
#include "host/command_line.hpp"
#include "host/deterministic_recording.hpp"
#include "host/host_files.hpp"
#include "host/host_input.hpp"
#include "host/interactive_session.hpp"
#include "host/native_registry_install.hpp"
#include "host/play_assists.hpp"
#include "host/recording_session.hpp"
#include "host/routine_replay.hpp"
#include "host/semantic_fixtures.hpp"
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
            return run_recording_session(
                *runtime, options, session_replay, *replacement_rom);
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
            options.drag_ball,
            options.skip_perfect_jingle);
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
