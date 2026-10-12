// SPDX-License-Identifier: GPL-3.0-or-later
#include "command_line.hpp"

#include "proven_replace_hooks.hpp"
#include "recording_plans.hpp"

namespace devils_crush::app::host {

[[nodiscard]] CommandLine parse_command_line(const int argc, char* argv[])
{
    CommandLine options;
    CommandLineOverrides overrides;
    if (!read_command_line_arguments(argc, argv, options, overrides)) {
        return options;
    }

    const int operation_count =
        static_cast<int>(options.smoke_test)
        + static_cast<int>(options.cabinet_smoke_test)
        + static_cast<int>(options.record_canonical)
        + static_cast<int>(options.record_gameplay)
        + static_cast<int>(options.record_gameplay_alternate)
        + static_cast<int>(options.record_gameplay_sweep)
        + static_cast<int>(options.record_gameplay_crossfire)
        + static_cast<int>(options.record_gameplay_orbit)
        + static_cast<int>(options.record_gameplay_lanes)
        + static_cast<int>(options.record_gameplay_stagger)
        + static_cast<int>(options.record_gameplay_pulse)
        + static_cast<int>(options.record_gameplay_rally)
        + static_cast<int>(options.record_gameplay_slalom)
        + static_cast<int>(options.record_gameplay_pendulum)
        + static_cast<int>(options.record_gameplay_nudge)
        + static_cast<int>(options.record_gameplay_switchback)
        + static_cast<int>(options.record_gameplay_spiral)
        + static_cast<int>(options.record_gameplay_ricochet)
        + static_cast<int>(options.record_gameplay_volley)
        + static_cast<int>(options.record_gameplay_whirlpool)
        + static_cast<int>(options.record_gameplay_cascade)
        + static_cast<int>(options.record_gameplay_higharc)
        + static_cast<int>(options.record_gameplay_twist)
        + static_cast<int>(options.record_gameplay_sidestep)
        + static_cast<int>(options.record_gameplay_weave)
        + static_cast<int>(options.record_gameplay_bankshot)
        + static_cast<int>(options.record_gameplay_playfield)
        + static_cast<int>(options.record_gameplay_corner)
        + static_cast<int>(options.record_gameplay_alley)
        + static_cast<int>(options.record_gameplay_rebound)
        + static_cast<int>(options.record_gameplay_skew)
        + static_cast<int>(options.record_gameplay_helix)
        + static_cast<int>(options.record_gameplay_vector)
        + static_cast<int>(options.record_gameplay_radial)
        + static_cast<int>(!options.replay_session_input_path.empty())
        + static_cast<int>(options.record_state_census)
        + static_cast<int>(!options.routine_replay_corpus_paths.empty())
        + static_cast<int>(options.verify_unsigned_square_root)
        + static_cast<int>(options.verify_unsigned_word_multiply_partial)
        + static_cast<int>(options.verify_signed_word_normalizer)
        + static_cast<int>(options.verify_signed_word_multiply)
        + static_cast<int>(options.verify_side_gate_deadline_check)
        + static_cast<int>(options.backup_ram_test);
    const bool recording_selected =
        options.record_canonical || options.record_gameplay
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
        || !options.replay_session_input_path.empty()
        || options.record_state_census;
    // Both interactive entry points run the same host loop, and both deploy
    // the admitted-routine registry unless told to emulate the original.
    const bool host_selected = !options.validate_only
        && (operation_count == 0 || options.cabinet_smoke_test);
    // Default enhancements belong to normal play. Keep diagnostic recordings
    // and reference-device comparisons reproducible unless explicitly enabled.
    const bool play_selected = host_selected && !options.cabinet_smoke_test;
    // Normal play also takes what the flags leave unsaid from devils-crush.ini,
    // where the launcher keeps it. A file that cannot be read is reported by
    // the host when it loads the file again, and plays with the defaults.
    devils_crush::cabinet::PlaySettings play;
    if (play_selected) {
        const auto loaded =
            devils_crush::cabinet::load_display_config(kCabinetConfigPath);
        if (loaded.loaded()) {
            play = loaded.config.play;
        }
        if (!overrides.rom && !play.rom.empty()) {
            options.rom_path = std::filesystem::path(
                std::u8string(play.rom.begin(), play.rom.end()));
        }
        if (!overrides.bonus_stage) {
            options.bonus_stage = play.bonus_stage;
        }
        if (!overrides.nudge_multiplier) {
            options.nudge_multiplier = play.nudge_multiplier;
        }
        // The mouse's hold on the ball is not in a capture's inputs.
        options.drag_ball = options.drag_ball
            || (play.drag_ball && options.capture_session_directory.empty());
    }
    options.cabinet = overrides.cabinet.value_or(play_selected && play.cabinet);
    options.music_voice_overlay = overrides.music_overlay.value_or(
        host_selected && !options.emulated_psg
        && (!play_selected || play.music_voice_overlay));
    // The drain and nudge probes deploy the registry too, so they can be
    // bisected with --registry-slice and --registry-exclude like the host can,
    // and so they measure the configuration people actually play.
    const bool host_registry =
        (host_selected || options.cabinet_drain_probe
         || options.cabinet_nudge_probe || options.cabinet_launch_check
         || options.cabinet_plunger_check
         || options.cabinet_ball_drag_check
         || options.cabinet_launch_flipper_probe)
        && !options.emulate_only;
    const int recording_frame_count = options.record_state_census
        ? kStateCensusRecordingFrames
        : options.record_gameplay_alternate
            ? kGameplayAlternateRecordingFrames
        : options.record_gameplay_sweep
            ? kGameplaySweepRecordingFrames
        : options.record_gameplay_crossfire
            ? kGameplayCrossfireRecordingFrames
        : options.record_gameplay_orbit
            ? kGameplayOrbitRecordingFrames
        : options.record_gameplay_lanes
            ? kGameplayLanesRecordingFrames
        : options.record_gameplay_stagger
            ? kGameplayStaggerRecordingFrames
        : options.record_gameplay_pulse
            ? kGameplayPulseRecordingFrames
        : options.record_gameplay_rally
            ? kGameplayRallyRecordingFrames
        : options.record_gameplay_slalom
            ? kGameplaySlalomRecordingFrames
        : options.record_gameplay_pendulum
            ? kGameplayPendulumRecordingFrames
        : options.record_gameplay_nudge
            ? kGameplayNudgeRecordingFrames
        : options.record_gameplay_switchback
            ? kGameplaySwitchbackRecordingFrames
        : options.record_gameplay_spiral
            ? kGameplaySpiralRecordingFrames
        : options.record_gameplay_ricochet
            ? kGameplayRicochetRecordingFrames
        : options.record_gameplay_volley
            ? kGameplayVolleyRecordingFrames
        : options.record_gameplay_whirlpool
            ? kGameplayWhirlpoolRecordingFrames
        : options.record_gameplay_cascade
            ? kGameplayCascadeRecordingFrames
        : options.record_gameplay_higharc
            ? kGameplayHigharcRecordingFrames
        : options.record_gameplay_twist
            ? kGameplayTwistRecordingFrames
        : options.record_gameplay_sidestep
            ? kGameplaySidestepRecordingFrames
        : options.record_gameplay_weave
            ? kGameplayWeaveRecordingFrames
        : options.record_gameplay_bankshot
            ? kGameplayBankshotRecordingFrames
        : options.record_gameplay_playfield
            ? kGameplayPlayfieldRecordingFrames
        : options.record_gameplay_corner
            ? kGameplayCornerRecordingFrames
        : options.record_gameplay_alley
            ? kGameplayAlleyRecordingFrames
        : options.record_gameplay_rebound
            ? kGameplayReboundRecordingFrames
        : options.record_gameplay_skew
            ? kGameplaySkewRecordingFrames
        : options.record_gameplay_helix
            ? kGameplayHelixRecordingFrames
        : options.record_gameplay_vector
            ? kGameplayVectorRecordingFrames
        : options.record_gameplay_radial
            ? kGameplayRadialRecordingFrames
        : options.record_gameplay
            ? kGameplayRecordingFrames
            : options.record_canonical
                ? kCanonicalRecordingFrames
                : 0;
    if (operation_count > 1) {
        options.error =
            "Only one test or recording operation may be selected.";
    } else if (options.cabinet
               && ((operation_count != 0
                    && options.replay_session_input_path.empty())
                   || options.validate_only)) {
        options.error =
            "--cabinet is available only for the interactive host or a "
            "captured-session replay.";
    } else if (!options.capture_session_directory.empty()
               && !host_selected) {
        options.error =
            "--capture-session requires the interactive host.";
    } else if (options.drag_ball
               && (!play_selected
                   || !options.capture_session_directory.empty())) {
        options.error =
            "--drag-ball is available only for the interactive host, and "
            "not with --capture-session.";
    } else if (options.bonus_stage != 0U && !host_selected) {
        options.error =
            "--bonus-stage is available only for the interactive host or "
            "--cabinet-smoke-test.";
    } else if (options.cabinet_overlay_stats && !options.cabinet
               && !options.cabinet_smoke_test) {
        options.error =
            "--cabinet-overlay-stats requires --cabinet or "
            "--cabinet-smoke-test.";
    } else if (options.cabinet_layers != CabinetLayers::all
               && !options.cabinet && !options.cabinet_smoke_test) {
        options.error =
            "--cabinet-layers requires --cabinet or --cabinet-smoke-test.";
    } else if ((!options.canonical_work_ram_path.empty()
                || !options.canonical_framebuffer_path.empty()
                || !options.canonical_audio_path.empty())
               && !recording_selected) {
        options.error =
            "--canonical-work-ram, --canonical-framebuffer, and "
            "--canonical-audio require a recording operation.";
    } else if ((!options.pass_through_hooks.empty()
                || !options.observe_hooks.empty()
                || !options.replace_hooks.empty())
               && !recording_selected
               && options.routine_replay_corpus_paths.empty()) {
        options.error =
            "Individual execution hooks currently require a recording "
            "or routine-replay corpus operation.";
    } else if (options.emulate_only && !host_selected) {
        options.error =
            "--emulate-only is available only for the interactive host.";
    } else if (options.native_gameplay_bootstrap_shadow && !host_selected) {
        options.error =
            "--native-gameplay-bootstrap-shadow is available only for the "
            "interactive host.";
    } else if (options.emulate_only && options.replace_registry) {
        options.error =
            "--emulate-only and --replace-registry are mutually exclusive.";
    } else if (
        options.replace_registry && !recording_selected && !host_registry
        && options.routine_replay_corpus_paths.empty()) {
        options.error =
            "--replace-registry requires a recording operation or the "
            "interactive host.";
    } else if (!options.routine_replay_corpus_output_directory.empty()
               && (!recording_selected || options.routine_replays.empty())) {
        options.error =
            "--routine-replay-corpus-out requires a recording and at least "
            "one --routine-replay.";
    } else if (!options.routine_replay_corpus_output_directory.empty()
               && (options.replace_registry
                   || std::any_of(
                       options.routine_replays.begin(),
                       options.routine_replays.end(),
                       [&options](const RoutineReplayRequest& replay) {
                           return std::find_if(
                                      options.replace_hooks.begin(),
                                      options.replace_hooks.end(),
                                      [&replay](
                                          const RoutineReplayRequest& replace) {
                                          return replace.routine
                                              == replay.routine;
                                      })
                               != options.replace_hooks.end();
                       }))) {
        options.error =
            "Routine-replay corpus exports must be captured from original "
            "assembly, without replacing the target or deploying the "
            "registry.";
    } else if (!options.routine_replay_corpus_paths.empty()
               && (recording_selected || !options.routine_replays.empty()
                   || !options.pass_through_hooks.empty()
                   || !options.observe_hooks.empty())) {
        options.error =
            "--routine-replay-corpus is a standalone operation; it cannot "
            "be combined with recording, transient replay, pass-through, "
            "or observe options.";
    } else if (!options.routine_replay_corpus_paths.empty()
               && options.replace_registry) {
        options.error =
            "Direct corpus replay currently accepts explicit semantic @all "
            "replacement hooks, not --replace-registry.";
    } else if (!options.routine_replay_corpus_paths.empty()
               && std::any_of(
                   options.replace_hooks.begin(),
                   options.replace_hooks.end(),
                   [](const RoutineReplayRequest& request) {
                       return !request.all_calls;
                   })) {
        options.error =
            "Direct corpus replay accepts only semantic @all replacement "
            "hooks.";
    } else if (!options.native_cycle_census_path.empty()
               && !options.replace_registry && !host_registry) {
        options.error =
            "--native-cycle-census requires a deployed registry.";
    } else if (options.registry_slice_set && !options.replace_registry
               && !host_registry) {
        options.error = "--registry-slice requires a deployed registry.";
    } else if (!options.registry_excludes.empty()
               && !options.replace_registry && !host_registry) {
        options.error = "--registry-exclude requires a deployed registry.";
    } else if (!options.instruction_profile_path.empty()
               && !recording_selected) {
        options.error =
            "--instruction-profile requires a recording operation.";
    } else if (!options.abandonment_census_path.empty()
               && !recording_selected) {
        options.error =
            "--abandonment-census requires a recording operation.";
    } else if (!options.abandonment_census_path.empty()
               && !options.replace_registry && !host_registry
               && options.replace_hooks.empty()) {
        // Only a transcript can be abandoned, so with nothing replaced the
        // census is empty by construction rather than by measurement.
        options.error =
            "--abandonment-census requires a replacement to be deployed.";
    } else if (!options.table_vram_census_path.empty()
               && !recording_selected) {
        options.error =
            "--table-vram-census requires a recording operation.";
    } else if (!std::all_of(
                   options.replace_hooks.begin(),
                   options.replace_hooks.end(),
                   is_proven_replace_hook)) {
        options.error = unproven_replace_hook_message();
    } else if (!options.work_ram_patches.empty() && !recording_selected) {
        options.error =
            "--work-ram-patch requires a recording operation.";
    } else if (std::any_of(
                   options.work_ram_patches.begin(),
                   options.work_ram_patches.end(),
                   [recording_frame_count](const WorkRamPatch& patch) {
                       return patch.frame > recording_frame_count;
                   })) {
        options.error =
            "Each work-RAM patch frame must be within the selected "
            "recording.";
    } else if (options.observe_hooks.empty()
               && (!options.routine_trace_path.empty()
                   || !options.routine_call_trace_path.empty())) {
        options.error =
            "Routine traces require at least one --observe-hook.";
    } else if (!options.observe_hooks.empty()
               && options.routine_trace_path.empty()
               && options.routine_call_trace_path.empty()) {
        options.error =
            "--observe-hook requires --routine-trace or "
            "--routine-call-trace.";
    } else if (!options.playfield_collision_sources_path.empty()
               && !recording_selected) {
        options.error =
            "--playfield-collision-sources requires a recording operation.";
    } else if (!options.playfield_collision_sources_path.empty()
               && std::find(
                      options.observe_hooks.begin(),
                      options.observe_hooks.end(),
                      RoutineKey{
                          .physical_bank = 0x01U,
                          .logical_pc = 0xCD74U,
                      }) == options.observe_hooks.end()) {
        options.error =
            "--playfield-collision-sources requires --observe-hook 01:CD74.";
    } else if (!options.routine_replays.empty()
               && (!recording_selected
                   || options.routine_call_trace_path.empty())) {
        options.error =
            "--routine-replay requires a recording operation and "
            "--routine-call-trace.";
    } else if (std::any_of(
                   options.routine_replays.begin(),
                   options.routine_replays.end(),
                   [&options](const RoutineReplayRequest& request) {
                       return std::find(
                                  options.observe_hooks.begin(),
                                  options.observe_hooks.end(),
                                  request.routine)
                           == options.observe_hooks.end();
                   })) {
        options.error =
            "Each --routine-replay target must also be an --observe-hook.";
    } else if ((!options.work_ram_write_ranges.empty()
                || !options.work_ram_write_trace_path.empty())
               && !recording_selected) {
        options.error =
            "Work-RAM write census options require a recording operation.";
    } else if (options.work_ram_write_ranges.empty()
               != options.work_ram_write_trace_path.empty()) {
        options.error =
            "--work-ram-write-range and --work-ram-write-trace "
            "must be used together.";
    } else if ((!options.bus_write_ranges.empty()
                || !options.bus_write_trace_path.empty())
               && !recording_selected) {
        options.error =
            "Bus write census options require a recording operation.";
    } else if (options.bus_write_ranges.empty()
               != options.bus_write_trace_path.empty()) {
        options.error =
            "--bus-write-range and --bus-write-trace "
            "must be used together.";
    } else if (options.music_voice_overlay && options.emulated_psg) {
        options.error =
            "--music-voice-overlay requires the native PSG; remove "
            "--emulated-psg.";
    }
    return options;
}

} // namespace devils_crush::app::host
