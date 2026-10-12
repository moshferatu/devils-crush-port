// SPDX-License-Identifier: GPL-3.0-or-later
#include "command_line.hpp"

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
    } else if (std::any_of(
                   options.replace_hooks.begin(),
                   options.replace_hooks.end(),
                   [](const RoutineReplayRequest& request) {
                       const bool cached_position_restore =
                           request.routine
                           == RoutineKey{
                               .physical_bank = 0x00U,
                               .logical_pc = 0xF241U,
                           };
                       const bool proven_position_integration =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF1B4U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 10U
                               || request.ordinal == 17U);
                       const bool proven_motion_damping =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xDD09U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 2U
                               || request.ordinal == 181U);
                       const bool proven_motion_rotation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD1A2U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 23U);
                       const bool proven_flipper_projection =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD09EU,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_radius_projection =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD612U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_angle_projection =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD62EU,
                                   }
                           && request.all_calls;
                       const bool proven_collision_motion_adjustment =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCFD8U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_response =
                           request.routine
                               == RoutineKey{
                                   .physical_bank = 0x03U,
                                   .logical_pc = 0x7FB1U,
                               }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 3U
                               || request.ordinal == 7U);
                       const bool proven_collision_consumer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7F57U,
                                   }
                           && request.all_calls;
                       const bool proven_initial_collision_consumer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7F1AU,
                                   }
                           && request.all_calls;
                       const bool proven_bank7_mode_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x07U,
                                       .logical_pc = 0x4003U,
                                   }
                           && request.all_calls;
                       const bool proven_score_accumulator =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF55CU,
                                   }
                           && (request.all_calls || request.ordinal == 1U);
                       const bool proven_score_repeat_wrapper =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF54FU,
                                   }
                           && request.all_calls;
                       const bool proven_rng_update =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE8B3U,
                                   }
                           && (request.all_calls || request.ordinal == 1U);
                       const bool proven_signed_word_negation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE837U,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_square_root =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE85DU,
                                   }
                           && request.all_calls;
                       const bool proven_signed_division =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE905U,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_fixed_division =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE728U,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_24_division =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE7D7U,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_byte_division =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE6DCU,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_multiply_round =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE624U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 2U);
                       const bool proven_unsigned_byte_multiply =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE569U,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_word_multiply_partial =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE5BCU,
                                   }
                           && request.all_calls;
                       const bool proven_signed_word_multiply =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE5ECU,
                                   }
                           && request.all_calls;
                       const bool proven_side_gate_deadline_check =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4FC4U,
                                   }
                           && request.all_calls;
                       const bool proven_signed_fixed_multiply =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE65CU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_camera_activation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF4FCU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_slot_reset =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC574U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_input_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD466U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_release_arm =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD4B3U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_active_update =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xD458U
                               || request.routine.logical_pc == 0xD48EU
                               || request.routine.logical_pc == 0xD491U
                               || request.routine.logical_pc == 0xD494U
                               || request.routine.logical_pc == 0xD4C6U
                               || request.routine.logical_pc == 0xD4C9U
                               || request.routine.logical_pc == 0xD4CCU)
                           && request.all_calls;
                       const bool proven_collision_scan_vertical_reject =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCC1AU,
                                   }
                           && (request.all_calls
                               || request.ordinal == 19176U);
                       const bool proven_collision_mask_lookup =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCC98U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_geometry =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCCD5U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_phase_step =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD500U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_collision_response_continuation =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xD525U
                               || request.routine.logical_pc == 0xD52CU
                               || request.routine.logical_pc == 0xD53BU
                               || request.routine.logical_pc == 0xD54AU
                               || request.routine.logical_pc == 0xD54DU
                               || request.routine.logical_pc == 0xD550U
                               || request.routine.logical_pc == 0xD553U
                               || request.routine.logical_pc == 0xD556U
                               || request.routine.logical_pc == 0xD589U
                               || request.routine.logical_pc == 0xD594U
                               || request.routine.logical_pc == 0xD599U
                               || request.routine.logical_pc == 0xD5A9U
                               || request.routine.logical_pc == 0xD5B8U
                               || request.routine.logical_pc == 0xD5BBU
                               || request.routine.logical_pc == 0xD5BEU
                               || request.routine.logical_pc == 0xD5C1U
                               || request.routine.logical_pc == 0xD5C4U
                               || request.routine.logical_pc == 0xD5D8U)
                           && request.all_calls;
                       const bool proven_flipper_geometry =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD081U,
                                   }
                           && request.all_calls;
                       const bool proven_coroutine_scheduler =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC5C9U,
                                   }
                           && request.all_calls;
                       const bool proven_nudge_input_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC3CBU,
                                   }
                           && request.all_calls;
                       const bool proven_collision_vector_rts =
                           request.routine == RoutineKey{0x01U, 0xCBF9U}
                           && request.all_calls;
                       const bool proven_session_input_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC49EU,
                                   }
                           && request.all_calls;
                       const bool proven_ball_drain_threshold =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xDD6EU,
                                   }
                           && request.all_calls;
                       const bool proven_motion_substep_split =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xDC95U,
                                   }
                           && request.all_calls;
                       const bool proven_ball_substep_loop =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xC6C2U
                               || request.routine.logical_pc == 0xC6C5U
                               || request.routine.logical_pc == 0xC6D9U
                               || request.routine.logical_pc == 0xC6E6U
                               || request.routine.logical_pc == 0xC6E9U
                               || request.routine.logical_pc == 0xC6EBU
                               || request.routine.logical_pc == 0xC6F2U
                               || request.routine.logical_pc == 0xC6F5U
                               || request.routine.logical_pc == 0xC708U
                               || request.routine.logical_pc == 0xC721U
                               || request.routine.logical_pc == 0xC72EU
                               || request.routine.logical_pc == 0xC742U
                               || request.routine.logical_pc == 0xC74CU
                               || request.routine.logical_pc == 0xC759U
                               || request.routine.logical_pc == 0xC75CU
                               || request.routine.logical_pc == 0xC75EU
                               || request.routine.logical_pc == 0xC761U
                               || request.routine.logical_pc == 0xC768U
                               || request.routine.logical_pc == 0xC76BU
                               || request.routine.logical_pc == 0xC76EU)
                           && request.all_calls;
                       const bool proven_actor_motion_step =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xF08EU
                               || request.routine.logical_pc == 0xF09AU
                               || request.routine.logical_pc == 0xF0A4U
                               || request.routine.logical_pc == 0xF0AEU
                               || request.routine.logical_pc == 0xF0B8U
                               || request.routine.logical_pc == 0xF0C0U
                               || request.routine.logical_pc == 0xF0FEU
                               || request.routine.logical_pc == 0xF104U
                               || request.routine.logical_pc == 0xF142U
                               || request.routine.logical_pc == 0xF148U
                               || request.routine.logical_pc == 0xF17EU)
                           && request.all_calls;
                       const bool proven_remaining_ball_presentation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC80AU,
                                   }
                           && request.all_calls;
                       const bool proven_remaining_ball_transition_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7EAU,
                                   }
                           && request.all_calls;
                       const bool proven_remaining_ball_decision =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7E5U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4DD9U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4DC8U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_opening_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4DF7U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_steady_idle =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4E24U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_final_wait =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4F3DU,
                                   }
                            || request.routine == RoutineKey{0x02U, 0x4F6CU})
                           && request.all_calls;
                       const bool proven_gray_blob_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6876U,
                                   }
                           && request.all_calls;
                       const bool proven_gray_blob_first_live_prefix =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x68A1U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_live_controller_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x57ADU,
                                   }
                           && request.all_calls;
                       const bool proven_mercy_star_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4F9FU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x50EEU,
                           }
                           && request.all_calls;
                       const bool proven_little_skull_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x44E4U,
                           }
                           && request.all_calls;
                       const bool proven_little_skull_gray_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x458AU,
                                   }
                           && request.all_calls;
                       const bool proven_little_skull_gray_release =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x45A4U,
                                   }
                           && request.all_calls;
                       const bool proven_little_skull_live_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4515U,
                                   }
                           && request.all_calls;
                       const bool proven_little_skull_pair_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4485U,
                           }
                           && request.all_calls;
                       const bool proven_little_skull_hidden_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4455U,
                           }
                           && request.all_calls;
                       const bool proven_big_star_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x565CU,
                                    }
                            && request.all_calls;
                       const bool proven_zero_ball_scheduler =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7F4U,
                                   }
                           && request.all_calls;
                       const bool proven_alternate_ball_scheduler =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7EFU,
                                   }
                           && request.all_calls;
                       const bool proven_zero_ball_countdown_tick =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7F9U,
                                   }
                           && request.all_calls;
                       const bool proven_zero_ball_expiry_scheduler =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7FCU,
                                   }
                           && request.all_calls;
                       const bool proven_zero_ball_presentation_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC804U,
                                   }
                           && request.all_calls;
                       const bool proven_zero_ball_state_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC812U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_deployment_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5808U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_staging_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x57F8U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x433FU,
                                    }
                            && request.all_calls;
                       const bool proven_undead_head_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x61A0U,
                                   }
                           && request.all_calls;
                       const bool proven_undead_head_selector_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x61ABU,
                                   }
                           && request.all_calls;
                       const bool proven_undead_head_waiting_parent =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x61A3U,
                                   }
                           && request.all_calls;
                       const bool proven_side_worm_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5C4BU,
                                    }
                               || request.routine
                                      == RoutineKey{
                                          .physical_bank = 0x02U,
                                          .logical_pc = 0x5C7CU,
                                      })
                           && request.all_calls;
                       const bool proven_side_worm_selector_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5CCDU,
                                   }
                           && request.all_calls;
                       const bool proven_side_worm_waiting_parent =
                           request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x5CC5U}
                           && request.all_calls;
                       const bool proven_side_worm_active_timer_initializer =
                           request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x5D4AU}
                           && request.all_calls;
                       const bool proven_dragon_head_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5A31U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5EE4U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5EECU,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x664AU,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_controller_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x65BCU,
                                   }
                           && request.all_calls;
                       const bool proven_walled_star_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6319U,
                           }
                           && request.all_calls;
                       const bool proven_walled_star_access_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4A44U,
                           }
                           && request.all_calls;
                       const bool proven_side_star_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A54U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A4CU,
                                    })
                           && request.all_calls;
                       const bool proven_knight_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x734EU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7353U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x735AU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7361U,
                                    })
                           && request.all_calls;
                       const bool proven_side_spider_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x64FAU,
                                   }
                           && request.all_calls;
                       const bool proven_front_spider_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x646BU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4C78U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_outer_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A5CU,
                                    }
                            || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A64U,
                                    })
                           && request.all_calls;
                       const bool proven_mercy_star_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4FC1U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x42F6U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4345U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4357U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_post_allocation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4371U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_reset_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4387U,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_feature_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x445BU,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_pending_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x570AU,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_pending_helper =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5722U,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_pending_right_helper =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5738U,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_rotation_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5674U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_dormant =
                           (request.routine
                                == RoutineKey{
                                    .physical_bank = 0x02U,
                                    .logical_pc = 0x53DDU,
                                }
                           || request.routine
                                == RoutineKey{
                                    .physical_bank = 0x02U,
                                    .logical_pc = 0x5456U,
                                })
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_position =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5397U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_appearance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5408U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_active_timer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x545DU,
                                   }
                           && request.all_calls;
                       const bool
                           proven_devil_shooter_companion_left_active_timer =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x53E4U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_left_active_controller =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x53C8U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_right_active_controller =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5441U,
                                       }
                               && request.all_calls;
                       const bool proven_dragon_head_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5A4BU,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_head_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5A6EU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5116U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_release_delay_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5159U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_after_audio =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x518DU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_tail =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5197U,
                                   }
                           && request.all_calls;
                       const bool
                           proven_devil_shooter_appearance_submission_prefix =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5475U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_appearance_submission_tail =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5481U,
                                       }
                               && request.all_calls;
                       const bool proven_devil_shooter_post_launch_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x525FU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_launch_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x520BU,
                                   }
                           && request.all_calls;
                       const bool
                           proven_devil_shooter_charge_retraction_calls =
                               (request.routine
                                        == RoutineKey{
                                            .physical_bank = 0x02U,
                                            .logical_pc = 0x5208U,
                                        }
                                   || request.routine
                                        == RoutineKey{
                                            .physical_bank = 0x02U,
                                            .logical_pc = 0x52C3U,
                                        })
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_charge_entry_controller =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5223U,
                                       }
                               && request.all_calls;
                       const bool proven_upper_parent_animation = request.all_calls
                           && request.routine.physical_bank == 0x09U
                           && (request.routine.logical_pc == 0x705CU
                               || request.routine.logical_pc == 0x705FU
                               || request.routine.logical_pc == 0x7084U
                               || request.routine.logical_pc == 0x7087U
                               || request.routine.logical_pc == 0x7094U
                               || request.routine.logical_pc == 0x709EU
                               || request.routine.logical_pc == 0x70A7U
                               || request.routine.logical_pc == 0x7194U
                               || request.routine.logical_pc == 0x7197U
                               || request.routine.logical_pc == 0x71BCU
                               || request.routine.logical_pc == 0x71BFU
                               || request.routine.logical_pc == 0x71CDU);
                       const bool proven_scene_layout_record_loop =
                           (request.routine == RoutineKey{0x02U, 0x40A8U}
                               || request.routine == RoutineKey{0x02U, 0x40BDU}
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4090U,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4099U,
                                    })
                           && request.all_calls;
                       const bool proven_scene_layout_record_loader =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x40F6U,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x40FEU,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4096U,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x40FBU,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x40A7U,
                                    })
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5249U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_controller_tail =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x524CU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_position =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x52E3U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_return =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x533AU,
                                   }
                           && request.all_calls;
                       const bool
                           proven_devil_shooter_catch_continuation =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x52B8U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_catch_retraction_tail =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x52C6U,
                                       }
                               && request.all_calls;
                       const bool proven_skeleton_insect_left_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x54B9U,
                                   }
                           && request.all_calls;
                       const bool proven_skeleton_insect_startup_countdown =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x54D0U,
                                   }
                           && request.all_calls;
                       const bool
                           proven_skeleton_insect_left_post_trigger =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5500U,
                                       }
                               && request.all_calls;
                       const bool proven_skeleton_insect_right_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5551U,
                                   }
                           && request.all_calls;
                       const bool
                           proven_skeleton_insect_right_startup_countdown =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5568U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_skeleton_insect_right_post_trigger =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5590U,
                                       }
                               && request.all_calls;
                       const bool proven_skeleton_insect_center_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x55F9U,
                                   }
                           && request.all_calls;
                       const bool
                           proven_skeleton_insect_center_continuation =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5604U,
                                       }
                               && request.all_calls;
                       const bool proven_skeleton_insect_center_toggle =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x561AU,
                                   }
                           && request.all_calls;
                       const bool
                           proven_skeleton_insect_center_post_delay_handoff =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5625U,
                                       }
                               && request.all_calls;
                       const bool proven_skeleton_insect_center_opening_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5630U,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x63F1U,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x63E1U,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_opening =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x640AU,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_closing =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6445U,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x65C2U,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_appearance_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x67B3U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_door_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x62B8U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_door_hit_continuation =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x03U,
                                           .logical_pc = 0x62CBU,
                                       }
                               && request.all_calls;
                       const bool proven_giant_door_completion_score_prefix =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x03U,
                                           .logical_pc = 0x62D4U,
                                       }
                               && request.all_calls;
                       const bool proven_giant_door_completion_scheduler_prefix =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x03U,
                                           .logical_pc = 0x62E2U,
                                       }
                               && request.all_calls;
                       const bool proven_giant_door_completion_request_prefix =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x03U,
                                           .logical_pc = 0x62F0U,
                                       }
                               && request.all_calls;
                       const bool proven_giant_door_completion_wait_gate =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x03U,
                                           .logical_pc = 0x62FEU,
                                       }
                               && request.all_calls;
                       const bool proven_giant_door_completion_channel_advance =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x03U,
                                           .logical_pc = 0x6308U,
                                       }
                               && request.all_calls;
                       const bool proven_giant_door_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6294U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_animator_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x767EU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_animator_active_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x76C5U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_request_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x74D8U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_crown_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x76FAU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_mouth_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7726U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4C91U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_shared_return =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4CF1U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4CFBU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_followup_timer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4D0BU,
                                   }
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4A93U,
                                   }
                           && request.all_calls;
                       const bool proven_walled_star_access_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4B3FU,
                                   }
                           && request.all_calls;
                       const bool proven_side_star_access_controller =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4BD5U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4C0AU,
                                    })
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_post_audio_score =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4AD2U,
                                   }
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_post_appearance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4AE5U,
                                   }
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_animation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4AEBU,
                                   }
                           && request.all_calls;
                       const bool proven_side_star_bonus_entrance_animation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4C33U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5972U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_countdown =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5991U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_opening =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x59A5U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_opening_followup =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x59C7U,
                                   }
                           && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_opening_expiry =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x59E0U,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_post_opening_schedule =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5A08U,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_post_opening_state =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5A0BU,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_post_opening_countdown =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5A1BU,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_live_setup =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5975U,
                                   }
                           && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_live_suffix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x5986U}
                            && request.all_calls;
                       const bool proven_walled_star_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6339U,
                                   }
                           && request.all_calls;
                       const bool proven_walled_star_post_destruction_idle =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x638FU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60C3U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_bonus_terminal =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60F9U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_bonus_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6136U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60B3U,
                                   }
                           && request.all_calls;
                       const bool proven_knight_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x73D8U,
                                   }
                           && request.all_calls;
                       const bool proven_knight_terminal_score_prefix =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x73FEU,
                                   }
                           && request.all_calls;
                        const bool proven_knight_terminal_release_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7418U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_acknowledgement_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x740AU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_scheduler =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7420U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x41A0U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_setup_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x41A8U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_table_setup_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x41ADU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_call_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x41C2U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_pointer_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x41F5U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_call_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x420BU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_mode_branch_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x420EU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_pointer_low_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4210U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_pointer_low_store_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4212U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_pointer_high_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4215U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_pointer_high_store_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4217U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_scratch_clear_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x421AU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_mode_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x421CU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_mode_signed_branch_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x421EU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_carry_clear_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4222U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_base_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4223U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_mode_add_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4226U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_store_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4228U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x422BU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_add_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x422EU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4230U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4233U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_mode_load_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4235U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4237U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x423BU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_base_load_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x423CU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x423FU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_position_high_actor_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4241U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_load_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4244U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4247U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4249U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_clear_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x424CU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x424DU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4250U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_attribute_load_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4253U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4256U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4258U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_attribute_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x425AU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x425DU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x425FU}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix =
                            request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4262U}
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_post_return_actor_field_second_constant_store_prefix = request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4264U} && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_jump_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC024U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_scan_initializer_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5ABU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_scan_read_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5ADU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5B0U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_scan_increment_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5B2U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_scan_range_compare_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5B3U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_shared_scan_loop_branch_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5B5U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_allocator_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5B9U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_allocator_store_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5BBU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_allocator_mode_load_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5BEU,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_allocator_mode_branch_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5C0U,
                                    }
                            && request.all_calls;
                        const bool proven_knight_terminal_release_helper_allocator_return_carry_clear_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x01U,
                                        .logical_pc = 0xC5C7U,
                                    }
                            && request.all_calls;
                       const bool proven_knight_waiting_parent =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7378U,
                                   }
                           && request.all_calls;
                       const bool proven_knight_movement_parent =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x73A6U,
                                   }
                          && (request.all_calls || request.ordinal == 2U || request.ordinal == 4U
                              || request.ordinal == 16U || request.ordinal == 58U
                              || request.ordinal == 60U);
                       const bool proven_spider_appearance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x65A4U,
                           }
                           && request.all_calls;
                       const bool proven_spider_controller_parents =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x6490U,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x651FU,
                                    })
                           && request.all_calls;
                       const bool proven_front_spider_scheduler_call_seams =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x64D2U,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x64DDU,
                                    }
                               || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x64EDU,
                                    })
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_retraction =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x533BU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_animation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6054U,
                                   }
                           && request.all_calls;
                       const bool proven_impact_burst_lifetime =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x42B0U,
                                   }
                           && request.all_calls;
                       const bool proven_impact_burst_audio_command_call =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x42A3U,
                                   }
                           && request.all_calls;
                       const bool proven_impact_burst_allocator_success_return =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x427CU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_initial_controller =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5FDBU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5FE6U,
                                   })
                           && request.all_calls;
                       const bool proven_tower_insect_controller =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6016U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5FFBU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x601CU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x601EU,
                           })
                           && request.all_calls;
                       const bool proven_small_soldier_source_gates =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7D85U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7E23U,
                           })
                           && request.all_calls;
                       const bool proven_small_soldier_source_initializers =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7D4DU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7DE9U,
                           })
                           && request.all_calls;
                       const bool proven_small_soldier_delayed_spawn_gates =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7D6FU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7E0DU,
                                   })
                           && request.all_calls;
                       const bool proven_small_soldier_child_allocation_seams =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7E87U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7E8AU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7EEEU,
                                   })
                           && request.all_calls;
                       const bool proven_collision_effect_allocator =
                           request.routine.physical_bank == 0x03U
                           && (request.routine.logical_pc == 0x7BF6U
                               || request.routine.logical_pc == 0x7C02U
                               || request.routine.logical_pc == 0x7C05U)
                           && request.all_calls;
                       const bool proven_small_soldier_initial_controller =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7EFAU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7F05U,
                                   })
                           && request.all_calls;
                       const bool proven_giant_face_controller_initializers =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7428U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x742BU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x742EU,
                           })
                           && request.all_calls;
                       const bool proven_giant_face_animator_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x766CU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_crown_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x76F7U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_mouth_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7718U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_source_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F5FU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_source_countdown_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F4AU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_source_countdown_return =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F5BU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_child_spawner_seams =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F78U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F7BU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F7EU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5FD6U,
                                   })
                           && request.all_calls;
                       const bool proven_tower_insect_source_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F22U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_color_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x57E9U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60E0U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_post_dispatch =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x611CU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_guard_selector =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60A6U,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x78C7U,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_initializers =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x789AU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x78A4U,
                                   })
                           && request.all_calls;
                       const bool proven_bumper_child_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x79DEU,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_primary_progress =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x78F5U,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_secondary_progress =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7912U,
                                   }
                           && request.all_calls;
                       const bool proven_fixed_response_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7CAFU,
                                   }
                           && request.all_calls;
                        const bool proven_fixed_scheduler_gate =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B27U,
                                    }
                            && request.all_calls;
                        const bool proven_fixed_scheduler_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B1FU,
                                    }
                            && request.all_calls;
                        const bool proven_fixed_marker_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B6DU,
                                    }
                            && request.all_calls;
                       const bool proven_fixed_response_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7CA7U,
                                    }
                            && request.all_calls;
                        const bool proven_dragon_egg_controller_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x42EBU,
                                    }
                            && request.all_calls;
                        const bool proven_ranked_control_seam = request.all_calls && (request.routine == RoutineKey{0x09U, 0x62E0U}
                            || request.routine == RoutineKey{0x09U, 0x62E6U}
                            || request.routine == RoutineKey{0x09U, 0x62E8U}
                            || request.routine == RoutineKey{0x09U, 0x62E2U}
                            || request.routine == RoutineKey{0x09U, 0x60BAU}
                            || request.routine == RoutineKey{0x09U, 0x62EEU}
                            || request.routine == RoutineKey{0x09U, 0x60BFU}
                            || request.routine == RoutineKey{0x09U, 0x608DU}
                            || request.routine == RoutineKey{0x09U, 0x6090U}
                            || request.routine == RoutineKey{0x09U, 0x6095U}
                            || request.routine == RoutineKey{0x09U, 0x6099U}
                            || request.routine == RoutineKey{0x02U, 0x4D30U}
                            || request.routine == RoutineKey{0x03U, 0x7AE0U}
                            || request.routine == RoutineKey{0x03U, 0x7AEBU}
                            || request.routine == RoutineKey{0x03U, 0x7AEFU}
                            || request.routine == RoutineKey{0x45U, 0x4148U}
                            || request.routine == RoutineKey{0x01U, 0xD7B3U}
                            || request.routine == RoutineKey{0x03U, 0x66A8U}
                            || request.routine == RoutineKey{0x03U, 0x66B0U}
                            || request.routine == RoutineKey{0x03U, 0x7ACAU}
                            || request.routine == RoutineKey{0x03U, 0x7AD5U}
                            || request.routine == RoutineKey{0x02U, 0x5A05U}
                            || request.routine == RoutineKey{0x03U, 0x61A9U}
                            || request.routine == RoutineKey{0x01U, 0xC9BBU}
                            || request.routine == RoutineKey{0x02U, 0x5A00U}
                            || request.routine == RoutineKey{0x02U, 0x4D32U}
                            || request.routine == RoutineKey{0x02U, 0x56A5U}
                            || request.routine == RoutineKey{0x02U, 0x56A9U}
                            || request.routine == RoutineKey{0x02U, 0x5682U}
                            || request.routine == RoutineKey{0x01U, 0xD902U}
                            || request.routine == RoutineKey{0x01U, 0xD915U}
                            || request.routine == RoutineKey{0x01U, 0xD92FU}
                            || request.routine == RoutineKey{0x01U, 0xD907U}
                            || request.routine == RoutineKey{1U, 0xC9B3U}
                            || request.routine == RoutineKey{1U, 0xC9CBU}
                            || request.routine == RoutineKey{1U, 0xC9D1U}
                            || request.routine == RoutineKey{1U, 0xC9AFU}
                            || request.routine == RoutineKey{1U, 0xC9C7U}
                            || request.routine == RoutineKey{1U, 0xC9C9U}
                            || request.routine == RoutineKey{1U, 0xC9CDU}
                            || request.routine == RoutineKey{1U, 0xC9B1U}
                            || request.routine == RoutineKey{1U, 0xC9B5U}
                            || request.routine == RoutineKey{1U, 0xC9B9U}
                            || request.routine == RoutineKey{1U, 0xC9B7U}
                            || request.routine == RoutineKey{1U, 0xC9BFU}
                            || request.routine == RoutineKey{0U, 0xEB1AU}
                            || request.routine == RoutineKey{0U, 0xE32CU}
                            || request.routine == RoutineKey{1U, 0xD62DU}
                            || request.routine == RoutineKey{1U, 0xD68DU}
                            || request.routine == RoutineKey{0U, 0xE32FU}
                            || request.routine == RoutineKey{0U, 0xF55BU}
                            || request.routine == RoutineKey{3U, 0x7D6FU}
                            || request.routine == RoutineKey{0x45U, 0x4B36U}
                            || request.routine == RoutineKey{3U, 0x657CU}
                            || request.routine == RoutineKey{9U, 0x7040U}
                            || request.routine == RoutineKey{9U, 0x7178U}
                            || request.routine == RoutineKey{9U, 0x7022U}
                            || request.routine == RoutineKey{9U, 0x715AU}
                            || request.routine == RoutineKey{2U, 0x5004U}
                            || request.routine == RoutineKey{2U, 0x5007U}
                            || request.routine == RoutineKey{2U, 0x5009U}
                            || request.routine == RoutineKey{2U, 0x500CU}
                            || request.routine == RoutineKey{2U, 0x500EU}
                            || request.routine == RoutineKey{2U, 0x5013U}
                            || request.routine == RoutineKey{3U, 0x7AE2U}
                            || request.routine == RoutineKey{3U, 0x7AE5U}
                            || request.routine == RoutineKey{3U, 0x7AE8U}
                            || request.routine == RoutineKey{3U, 0x7AEDU}
                            || request.routine == RoutineKey{3U, 0x7957U}
                            || request.routine == RoutineKey{0x02U, 0x4D40U}
                            || request.routine == RoutineKey{0x03U, 0x788EU}
                            || request.routine == RoutineKey{0x03U, 0x76C8U}
                            || request.routine == RoutineKey{0x03U, 0x76D3U}
                            || request.routine == RoutineKey{0x03U, 0x76CDU}
                            || request.routine == RoutineKey{0x03U, 0x76D6U}
                            || request.routine == RoutineKey{0x03U, 0x76ECU}
                            || request.routine == RoutineKey{0x01U, 0xC8DDU}
                            || request.routine == RoutineKey{0x03U, 0x7DCCU}
                            || request.routine == RoutineKey{0x01U, 0xC8DFU}
                            || request.routine == RoutineKey{0x03U, 0x7DD5U}
                            || request.routine == RoutineKey{0x03U, 0x7DD8U}
                            || request.routine == RoutineKey{0x00U, 0xE5BAU}
                            || request.routine == RoutineKey{0x00U, 0xE59FU}
                            || request.routine == RoutineKey{0x02U, 0x4D34U}
                            || request.routine == RoutineKey{0x02U, 0x5677U}
                            || request.routine == RoutineKey{0x03U, 0x76EFU}
                            || request.routine == RoutineKey{0x02U, 0x4D3BU}
                            || request.routine == RoutineKey{0x03U, 0x76D0U}
                            || request.routine == RoutineKey{0x02U, 0x567AU}
                            || request.routine == RoutineKey{0x01U, 0xC9C1U}
                            || request.routine == RoutineKey{0x01U, 0xC9BDU}
                            || request.routine == RoutineKey{0x01U, 0xC9CFU}
                            || request.routine == RoutineKey{0x03U, 0x76E9U}
                            || request.routine == RoutineKey{2U, 0x4D53U}
                            || request.routine == RoutineKey{2U, 0x4D2DU}
                            || request.routine == RoutineKey{1U, 0xC8DAU}
                            || request.routine == RoutineKey{1U, 0xDA09U}
                            || request.routine == RoutineKey{3U, 0x61A6U}
                            || request.routine == RoutineKey{0U, 0xE5A4U}
                            || request.routine == RoutineKey{0U, 0xE5A6U}
                            || request.routine == RoutineKey{0U, 0xE5ABU}
                            || request.routine == RoutineKey{0U, 0xE5F4U}
                            || request.routine == RoutineKey{0U, 0xE5F6U}
                            || request.routine == RoutineKey{0U, 0xE5FDU});
                        const bool proven_math_transfer_seam = request.all_calls && (request.routine == RoutineKey{1U, 0xC9C3U}
                            || request.routine == RoutineKey{1U, 0xC9D3U}
                            || request.routine == RoutineKey{0U, 0xE8A3U}
                            || request.routine == RoutineKey{0U, 0xE5ADU}
                            || request.routine == RoutineKey{0U, 0xE5B8U}
                            || request.routine == RoutineKey{0U, 0xE5B2U}
                            || request.routine == RoutineKey{0U, 0xE5B4U}
                            || request.routine == RoutineKey{0U, 0xE5EFU}
                            || request.routine == RoutineKey{0U, 0xE5B6U}
                            || request.routine == RoutineKey{0U, 0xE5FBU}
                            || request.routine == RoutineKey{0U, 0xE5A0U}
                            || request.routine == RoutineKey{0U, 0xE5A2U});
                        const bool proven_ranked_timer_seam = request.all_calls && (request.routine == RoutineKey{3U, 0x7E7EU}
                            || request.routine == RoutineKey{3U, 0x66A3U}
                            || request.routine == RoutineKey{3U, 0x7643U}
                            || request.routine == RoutineKey{3U, 0x7E6AU}
                            || request.routine == RoutineKey{0x45U, 0x485DU}
                            || request.routine == RoutineKey{3U, 0x7E76U}
                            || request.routine == RoutineKey{3U, 0x7E73U}
                            || request.routine == RoutineKey{2U, 0x567FU}
                            || request.routine == RoutineKey{3U, 0x66AAU}
                            || request.routine == RoutineKey{3U, 0x66B2U}
                            || request.routine == RoutineKey{0U, 0xE438U}
                            || request.routine == RoutineKey{3U, 0x66ADU});
                        const bool proven_eyeball_countdown_seam = request.all_calls && (
                            request.routine == RoutineKey{2U, 0x4E32U}
                            || request.routine == RoutineKey{3U, 0x7C34U}
                            || request.routine == RoutineKey{2U, 0x432CU}
                            || request.routine == RoutineKey{2U, 0x4E24U}
                            || request.routine == RoutineKey{0x45U, 0x4B85U}
                            || request.routine == RoutineKey{0x45U, 0x49E3U}
                            || request.routine == RoutineKey{2U, 0x4E29U}
                            || request.routine == RoutineKey{2U, 0x4E2DU}
                            || request.routine == RoutineKey{3U, 0x7DE0U}
                            || request.routine == RoutineKey{2U, 0x4324U}
                            || request.routine == RoutineKey{2U, 0x431BU}
                            || request.routine == RoutineKey{2U, 0x4327U});
                        const bool proven_bumper_sprite_seam = request.all_calls && (
                            request.routine == RoutineKey{0x03U, 0x7A71U}
                            || request.routine == RoutineKey{0x03U, 0x7A77U}
                            || request.routine == RoutineKey{0x03U, 0x7A7FU}
                            || request.routine == RoutineKey{0x03U, 0x7A82U}
                            || request.routine == RoutineKey{0x03U, 0x7ADBU}
                            || request.routine == RoutineKey{0x03U, 0x7ADEU}
                            || request.routine == RoutineKey{0x03U, 0x76C5U}
                            || request.routine == RoutineKey{0x08U, 0x40DFU}
                            || request.routine == RoutineKey{0x08U, 0x40F1U}
                            || request.routine == RoutineKey{0x08U, 0x40F6U}
                            || request.routine == RoutineKey{0x08U, 0x40FBU}
                            || request.routine == RoutineKey{0x08U, 0x4106U});
                        const bool proven_bonus_countdown_seam = request.all_calls && (
                            request.routine == RoutineKey{0x05U, 0x7376U}
                            || request.routine == RoutineKey{0x05U, 0x7379U}
                            || request.routine == RoutineKey{0x05U, 0x7387U}
                            || request.routine == RoutineKey{0x05U, 0x738AU}
                            || request.routine == RoutineKey{0x05U, 0x738FU}
                            || request.routine == RoutineKey{0x05U, 0x7390U}
                            || request.routine == RoutineKey{0x05U, 0x7393U}
                            || request.routine == RoutineKey{0x05U, 0x7396U}
                            || request.routine == RoutineKey{0x05U, 0x7398U}
                            || request.routine == RoutineKey{0x02U, 0x4FC1U}
                            || request.routine == RoutineKey{0x02U, 0x4FE1U}
                            || request.routine == RoutineKey{0x04U, 0x5197U});
                        const bool proven_math_gate_seam = request.all_calls && (
                            request.routine == RoutineKey{0x02U, 0x5CC5U}
                            || request.routine == RoutineKey{0x02U, 0x5CC8U}
                            || request.routine == RoutineKey{0x02U, 0x5CCBU}
                            || request.routine == RoutineKey{0x01U, 0xDA3EU}
                            || request.routine == RoutineKey{0x00U, 0xE5AFU}
                            || request.routine == RoutineKey{0x00U, 0xE5ECU}
                            || request.routine == RoutineKey{0x00U, 0xE5F1U}
                            || request.routine == RoutineKey{0x00U, 0xE5F8U}
                            || request.routine == RoutineKey{0x00U, 0xE824U}
                            || request.routine == RoutineKey{0x00U, 0xE5BBU}
                            || request.routine == RoutineKey{0x00U, 0xE5EBU}
                            || request.routine == RoutineKey{0x00U, 0xE836U});
                        const bool proven_ranked_actor_seam = request.all_calls && (
                            request.routine == RoutineKey{0x02U, 0x45A9U}
                            || request.routine == RoutineKey{0x02U, 0x4571U}
                            || request.routine == RoutineKey{0x02U, 0x45A4U}
                            || request.routine == RoutineKey{0x02U, 0x59F4U}
                            || request.routine == RoutineKey{0x02U, 0x4484U}
                            || request.routine == RoutineKey{0x02U, 0x56ABU}
                            || request.routine == RoutineKey{0x00U, 0xE60AU}
                            || request.routine == RoutineKey{0x03U, 0x61B2U}
                            || request.routine == RoutineKey{0x02U, 0x4567U}
                            || request.routine == RoutineKey{0x02U, 0x456AU}
                            || request.routine == RoutineKey{0x02U, 0x5674U}
                            || request.routine == RoutineKey{0x02U, 0x5721U});
                        const bool proven_tower_source_scheduler_call =
                            request.routine == RoutineKey{0x02U, 0x5F73U}
                            && request.all_calls;
                        const bool proven_collision_table_call =
                            request.routine.physical_bank == 0x02U
                            && (request.routine.logical_pc == 0x441EU
                                || request.routine.logical_pc == 0x4421U)
                            && request.all_calls;
                        const bool proven_three_blobs_first_movement_prefix =
                            request.routine.physical_bank == 0x03U
                            && (request.routine.logical_pc == 0x66A3U
                                || request.routine.logical_pc == 0x66A6U)
                            && request.all_calls;
                        const bool proven_three_blobs_rng_initialization =
                            request.routine.physical_bank == 0x03U
                            && (request.routine.logical_pc == 0x6678U
                                || request.routine.logical_pc == 0x667BU
                                || request.routine.logical_pc == 0x6697U
                                || request.routine.logical_pc == 0x669AU)
                            && request.all_calls;
                        const bool proven_three_blobs_rng_loop_branch =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66DCU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_first_movement_continuation =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66B5U,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_y_expiry_negation_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66BFU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_alternate_motion_reset_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66E2U,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_alternate_motion_arithmetic =
                            request.routine.physical_bank == 0x03U
                            && (request.routine.logical_pc == 0x670FU
                                || request.routine.logical_pc == 0x6729U
                                || request.routine.logical_pc == 0x6740U)
                            && request.all_calls;
                        const bool proven_three_blobs_movement_call_seam =
                            request.routine.physical_bank == 0x03U
                            && (request.routine.logical_pc == 0x66D3U
                                || request.routine.logical_pc == 0x66D6U
                                || request.routine.logical_pc == 0x66D9U
                                || request.routine.logical_pc == 0x670AU
                                || request.routine.logical_pc == 0x670CU
                                || request.routine.logical_pc == 0x6726U
                                || request.routine.logical_pc == 0x673DU)
                            && request.all_calls;
                        const bool proven_three_blobs_direct_continuation =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66DEU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_shared_motion_idle =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x676BU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_shared_return =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x67E5U,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_collision_gate =
                            request.routine.physical_bank == 0x03U
                            && (request.routine.logical_pc == 0x67C0U
                                || request.routine.logical_pc == 0x67CFU)
                            && request.all_calls;
                        const bool proven_three_blobs_collision_marker =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x694AU,
                                    }
                            && request.all_calls;
                        const bool proven_fixed_scheduler_submission =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B4EU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B5DU,
                                    })
                           && request.all_calls;
                       const bool proven_fixed_marker_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7B7AU,
                                   }
                           && request.all_calls;
                       const bool proven_fixed_marker_submission =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BBBU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BCDU,
                                    })
                           && request.all_calls;
                       const bool proven_fixed_marker_helper =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BE0U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BE5U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BE8U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BE9U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BECU,
                                    })
                           && request.all_calls;
                       const bool proven_fixed_marker_outer_segment =
                           request.routine.physical_bank == 0x03U
                           && (request.routine.logical_pc == 0x7BB1U
                               || request.routine.logical_pc == 0x7BB5U
                               || request.routine.logical_pc == 0x7BC3U
                               || request.routine.logical_pc == 0x7BC7U
                               || request.routine.logical_pc == 0x7BD5U
                               || request.routine.logical_pc == 0x7BD9U
                               || request.routine.logical_pc == 0x7BDFU)
                           && request.all_calls;
                       const bool proven_scaled_word_add =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5955U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_table_pass =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6966U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 2U
                               || request.ordinal == 3U
                               || request.ordinal == 4U
                               || request.ordinal == 5U
                               || request.ordinal == 6U
                               || request.ordinal == 7U
                               || request.ordinal == 8U
                               || request.ordinal == 9U
                               || request.ordinal == 10U
                               || request.ordinal == 12U
                               || request.ordinal == 14U
                               || request.ordinal == 15U
                               || request.ordinal == 16U
                               || request.ordinal == 17U
                               || request.ordinal == 18U
                               || request.ordinal == 19U
                               || request.ordinal == 20U
                               || request.ordinal == 21U
                               || request.ordinal == 22U
                               || request.ordinal == 23U);
                       const bool proven_upper_table_parent_idle_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x449BU,
                                   }
                           && request.all_calls;
                       const bool proven_bonus_hazard_timer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x05U,
                                       .logical_pc = 0x7356U,
                                   }
                           && request.all_calls;
                       const bool proven_actor_scan_preamble =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xC4DEU
                               || request.routine.logical_pc == 0xC500U)
                           && request.all_calls;
                       const bool proven_main_session_loop =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xC157U
                               || request.routine.logical_pc == 0xC15AU
                               || request.routine.logical_pc == 0xC15DU
                               || request.routine.logical_pc == 0xC166U
                               || request.routine.logical_pc == 0xC169U
                               || request.routine.logical_pc == 0xC16CU
                               || request.routine.logical_pc == 0xC16FU
                               || request.routine.logical_pc == 0xC17AU
                               || request.routine.logical_pc == 0xC185U
                               || request.routine.logical_pc == 0xC190U)
                           && request.all_calls;
                       const bool proven_bank_one_jump_table =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xC012U
                               || request.routine.logical_pc == 0xC018U
                               || request.routine.logical_pc == 0xC021U
                               || request.routine.logical_pc == 0xC024U
                               || request.routine.logical_pc == 0xC027U
                               || request.routine.logical_pc == 0xC02AU
                               || request.routine.logical_pc == 0xC03CU
                               || request.routine.logical_pc == 0xC042U
                               || request.routine.logical_pc == 0xC05DU
                               || request.routine.logical_pc == 0xC060U
                               || request.routine.logical_pc == 0xC063U
                               || request.routine.logical_pc == 0xC069U
                               || request.routine.logical_pc == 0xC06CU
                               || request.routine.logical_pc == 0xC06FU
                               || request.routine.logical_pc == 0xC081U
                               || request.routine.logical_pc == 0xC087U
                               || request.routine.logical_pc == 0xC09FU
                               || request.routine.logical_pc == 0xC0A2U)
                           && request.all_calls;
                       const bool proven_palette_colour_cycle =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x09U,
                                       .logical_pc = 0x60C7U,
                                   }
                           && request.all_calls;
                       const bool proven_score_panel_poll =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x09U,
                                       .logical_pc = 0x7237U,
                                   }
                           && request.all_calls;
                       const bool proven_score_animation =
                           request.routine.physical_bank == 0x09U
                           && (request.routine.logical_pc == 0x70C1U
                               || request.routine.logical_pc == 0x70C4U
                               || request.routine.logical_pc == 0x71EAU)
                           && request.all_calls;
                       const bool proven_score_panel_glyph =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x09U,
                                       .logical_pc = 0x72FDU,
                                   }
                           && request.all_calls;
                       const bool proven_score_panel_redraw =
                           request.routine.physical_bank == 0x09U
                           && (request.routine.logical_pc == 0x727DU
                               || request.routine.logical_pc == 0x72A2U
                               || request.routine.logical_pc == 0x72BDU
                               || request.routine.logical_pc == 0x72C5U)
                           && request.all_calls;
                       const bool proven_actor_scan_run =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC510U,
                                   }
                           && request.all_calls;
                       const bool proven_actor_scan_advance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC523U,
                                   }
                           && request.all_calls;
                       const bool proven_sat_tail_clear =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x08U,
                                       .logical_pc = 0x40C3U,
                                   }
                           && request.all_calls;
                       const bool proven_actor_dispatch_vector =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC52CU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_queue_registration =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC606U,
                                   }
                           && request.all_calls;
                       const bool proven_sound_timer_driver =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xFC00U
                               || request.routine.logical_pc == 0xFC82U
                               || request.routine.logical_pc == 0xFD76U)
                           && request.all_calls;
                       const bool proven_music_sequencer =
                           request.routine.physical_bank == 0x45U
                           && (request.routine.logical_pc == 0x4000U
                               || request.routine.logical_pc == 0x4003U
                               || request.routine.logical_pc == 0x4006U
                               || request.routine.logical_pc == 0x4009U
                               || request.routine.logical_pc == 0x4015U
                               || request.routine.logical_pc == 0x4044U
                               || request.routine.logical_pc == 0x404FU
                               || request.routine.logical_pc == 0x409CU
                               || request.routine.logical_pc == 0x409FU
                               || request.routine.logical_pc == 0x40A2U
                               || request.routine.logical_pc == 0x413BU
                               || request.routine.logical_pc == 0x4142U
                               || request.routine.logical_pc == 0x4143U
                               || request.routine.logical_pc == 0x4154U
                               || request.routine.logical_pc == 0x4169U
                               || request.routine.logical_pc == 0x47E0U
                               || request.routine.logical_pc == 0x482EU
                               || request.routine.logical_pc == 0x485EU
                               || request.routine.logical_pc == 0x49E4U
                               || request.routine.logical_pc == 0x4211U
                               || request.routine.logical_pc == 0x4228U
                               || request.routine.logical_pc == 0x4389U
                               || request.routine.logical_pc == 0x4434U
                               || request.routine.logical_pc == 0x4368U
                               || request.routine.logical_pc == 0x46E2U
                               || request.routine.logical_pc == 0x457AU
                               || request.routine.logical_pc == 0x4667U
                               || request.routine.logical_pc == 0x469BU
                               || request.routine.logical_pc == 0x4883U
                               || request.routine.logical_pc == 0x48B4U
                               || request.routine.logical_pc == 0x48F6U
                               || request.routine.logical_pc == 0x4936U
                               || request.routine.logical_pc == 0x49A2U
                               || request.routine.logical_pc == 0x49B6U
                               || request.routine.logical_pc == 0x49F9U
                               || request.routine.logical_pc == 0x4A16U
                               || request.routine.logical_pc == 0x4BCCU
                               || request.routine.logical_pc == 0x4C42U
                               || request.routine.logical_pc == 0x4B4EU)
                           && request.all_calls;
                       const bool proven_bank8_export_vectors =
                           request.routine.physical_bank == 0x08U
                           && (request.routine.logical_pc == 0x4000U
                               || request.routine.logical_pc == 0x4003U
                               || request.routine.logical_pc == 0x4006U
                               || request.routine.logical_pc == 0x4009U
                               || request.routine.logical_pc == 0x400CU
                               || request.routine.logical_pc == 0x400FU
                               || request.routine.logical_pc == 0x4012U
                               || request.routine.logical_pc == 0x4015U
                               || request.routine.logical_pc == 0x4018U
                               || request.routine.logical_pc == 0x401BU
                               || request.routine.logical_pc == 0x4030U)
                           && request.all_calls;
                       const bool proven_palette_shadow =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xE93CU
                               || request.routine.logical_pc == 0xE9B6U
                               || request.routine.logical_pc == 0xEAA7U
                               || request.routine.logical_pc == 0xEAB0U
                               || request.routine.logical_pc == 0xEB30U
                               || request.routine.logical_pc == 0xEB4DU
                               || request.routine.logical_pc == 0xEBA5U)
                           && request.all_calls;
                       const bool proven_setup_screen_palette_stager =
                           request.routine.physical_bank == 0x08U
                           && (request.routine.logical_pc == 0x5961U
                               || request.routine.logical_pc == 0x5977U
                               || request.routine.logical_pc == 0x5982U)
                           && request.all_calls;
                       const bool proven_text_tile_write =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x08U,
                                       .logical_pc = 0x5B01U,
                                   }
                           && request.all_calls;
                       const bool proven_music_voice_gate =
                           (request.routine == RoutineKey{0x45U, 0x4095U}
                            || request.routine == RoutineKey{0x45U, 0x4097U})
                           && request.all_calls;
                       const bool proven_interrupt_chain_jump =
                           (request.routine == RoutineKey{0x00U, 0xE003U}
                            || request.routine == RoutineKey{0x00U, 0xFC03U}
                            || request.routine == RoutineKey{0x00U, 0xFC89U})
                           && request.all_calls;
                       const bool proven_bit_branch =
                           (request.routine == RoutineKey{0x01U, 0xCB68U}
                            || request.routine == RoutineKey{0x00U, 0xFA67U})
                           && request.all_calls;
                       const bool proven_vram_stream_tail =
                           (request.routine == RoutineKey{0x00U, 0xE479U}
                            || request.routine == RoutineKey{0x00U, 0xE47CU})
                           && request.all_calls;
                       const bool proven_upper_scheduler_return =
                           (request.routine == RoutineKey{0x08U, 0x5976U}
                            || request.routine == RoutineKey{0x02U, 0x4F75U}
                            || request.routine == RoutineKey{0x02U, 0x51D2U}
                            || request.routine == RoutineKey{0x02U, 0x51D5U}
                            || request.routine == RoutineKey{0x45U, 0x4B85U}
                            || request.routine == RoutineKey{0x00U, 0xED85U}
                            || request.routine == RoutineKey{0x02U, 0x4F92U}
                            || request.routine == RoutineKey{0x09U, 0x704EU}
                            || request.routine == RoutineKey{0x09U, 0x7186U}
                            || request.routine == RoutineKey{0x09U, 0x7099U}
                            || request.routine == RoutineKey{0x09U, 0x71C4U}
                            || request.routine == RoutineKey{0x09U, 0x7071U}
                            || request.routine == RoutineKey{0x09U, 0x71A9U}
                            || request.routine == RoutineKey{0x03U, 0x7D81U}
                            || request.routine == RoutineKey{0x03U, 0x7E1FU}
                            || request.routine == RoutineKey{0x09U, 0x703CU}
                            || request.routine == RoutineKey{0x09U, 0x7174U}
                            || request.routine == RoutineKey{0x00U, 0xE8E6U}
                            || request.routine == RoutineKey{0x01U, 0xC8E1U}
                            || request.routine == RoutineKey{0x00U, 0xECFDU}
                            || request.routine == RoutineKey{0x00U, 0xED18U}
                            || request.routine == RoutineKey{0x00U, 0xED23U}
                            || request.routine == RoutineKey{0x00U, 0xED2CU}
                            || request.routine == RoutineKey{0x00U, 0xE333U}
                            || request.routine == RoutineKey{0x01U, 0xDA3FU}
                            || request.routine == RoutineKey{0x09U, 0x70BBU} || request.routine == RoutineKey{0x09U, 0x71E1U}
                            || request.routine == RoutineKey{0x01U, 0xC4C8U}
                            || request.routine == RoutineKey{0x08U, 0x5A69U}
                            || request.routine == RoutineKey{0x01U, 0xC5EAU}
                            || request.routine == RoutineKey{0x01U, 0xC624U}
                            || request.routine == RoutineKey{0x01U, 0xC627U}
                            || request.routine == RoutineKey{0x03U, 0x68BFU}
                            || request.routine == RoutineKey{0x01U, 0xC693U}
                            || request.routine == RoutineKey{0x01U, 0xC696U}
                            || request.routine == RoutineKey{0x01U, 0xC69BU}
                            || request.routine == RoutineKey{0x01U, 0xC6A6U}
                            || request.routine == RoutineKey{0x01U, 0xC6BFU})
                           && request.all_calls;
                       const bool proven_upper_table_actor_tile_schedule =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x08U,
                                       .logical_pc = 0x5A2EU,
                                   }
                           && request.all_calls;
                       const bool proven_upper_table_string_record =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x08U,
                                       .logical_pc = 0x5ABFU,
                                   }
                           && request.all_calls;
                       const bool proven_video_update_idle_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x08U,
                                       .logical_pc = 0x5E42U,
                                   }
                           && request.all_calls;
                       const bool proven_boot_work_ram_clear_seams =
                           request.routine.physical_bank == 0x08U
                           && (request.routine.logical_pc == 0x584CU
                               || request.routine.logical_pc == 0x584FU
                               || request.routine.logical_pc == 0x5852U
                               || request.routine.logical_pc == 0x5861U)
                           && request.all_calls;
                       const bool proven_bank_call_wrappers =
                           ((request.routine.physical_bank == 0x45U
                               && (request.routine.logical_pc == 0x4B7BU
                                   || request.routine.logical_pc == 0x4B82U))
                           || (request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xE1FDU
                               || request.routine.logical_pc == 0xE200U
                               || request.routine.logical_pc == 0xE20DU
                               || request.routine.logical_pc == 0xE219U
                               || request.routine.logical_pc == 0xE241U
                               || request.routine.logical_pc == 0xE255U
                               || request.routine.logical_pc == 0xE262U
                               || request.routine.logical_pc == 0xECC6U
                               || request.routine.logical_pc == 0xECE2U
                               || request.routine.logical_pc == 0xECF2U
                               || request.routine.logical_pc == 0xED58U
                               || request.routine.logical_pc == 0xED75U)))
                           && request.all_calls;
                       const bool proven_vblank_handler =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xE058U
                               || request.routine.logical_pc == 0xE05FU
                               || request.routine.logical_pc == 0xE0EBU
                               || request.routine.logical_pc == 0xE09EU
                               || request.routine.logical_pc == 0xE114U
                               || request.routine.logical_pc == 0xE117U
                               || request.routine.logical_pc == 0xE123U
                               || request.routine.logical_pc == 0xE135U
                               || request.routine.logical_pc == 0xE136U
                               || request.routine.logical_pc == 0xE179U
                               || request.routine.logical_pc == 0xE183U
                               || request.routine.logical_pc == 0xE194U
                               || request.routine.logical_pc == 0xE195U)
                           && request.all_calls;
                       const bool proven_vram_upload =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xE271U
                               || request.routine.logical_pc == 0xE34CU
                               || request.routine.logical_pc == 0xE365U
                               || request.routine.logical_pc == 0xE37EU
                               || request.routine.logical_pc == 0xEF2AU
                               || request.routine.logical_pc == 0xE3C0U
                               || request.routine.logical_pc == 0xE3D7U
                               || request.routine.logical_pc == 0xE3FCU
                               || request.routine.logical_pc == 0xE403U
                               || request.routine.logical_pc == 0xE41FU
                               || request.routine.logical_pc == 0xE439U
                               || request.routine.logical_pc == 0xE4E3U
                               || request.routine.logical_pc == 0xFA40U
                               || request.routine.logical_pc == 0xFA6AU
                               || request.routine.logical_pc == 0xFA6EU)
                           && request.all_calls;
                       const bool proven_resident_pattern_loader =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xF768U
                               || request.routine.logical_pc == 0xF778U
                               || request.routine.logical_pc == 0xF781U)
                           && request.all_calls;
                       const bool proven_tilemap_upload =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xF261U
                               || request.routine.logical_pc == 0xF2ABU
                               || request.routine.logical_pc == 0xF301U
                               || request.routine.logical_pc == 0xF330U
                               || request.routine.logical_pc == 0xF35FU
                               || request.routine.logical_pc == 0xF3B5U
                               || request.routine.logical_pc == 0xF3E4U
                               || request.routine.logical_pc == 0xF3E7U
                               || request.routine.logical_pc == 0xF426U
                               || request.routine.logical_pc == 0xF42AU)
                           && request.all_calls;
                       const bool scene_selection_controller =
                           request.routine.physical_bank == 0x08U
                           && ((request.routine.logical_pc >= 0x55A6U
                                && request.routine.logical_pc <= 0x577AU)
                               || request.routine.logical_pc == 0x5A3EU
                               || request.routine.logical_pc == 0x5A6AU
                               || request.routine.logical_pc == 0x5A7AU
                               || request.routine.logical_pc == 0x5A86U)
                           && request.all_calls;
                       const bool proven_collision_tail =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xCA84U
                               || request.routine.logical_pc == 0xCAA1U
                               || request.routine.logical_pc == 0xCAB6U
                               || request.routine.logical_pc == 0xCAD3U
                               || request.routine.logical_pc == 0xCAE0U
                               || request.routine.logical_pc == 0xCAEDU
                               || request.routine.logical_pc == 0xCB04U
                               || request.routine.logical_pc == 0xCB11U
                               || request.routine.logical_pc == 0xCB2AU
                               || request.routine.logical_pc == 0xCB46U
                               || request.routine.logical_pc == 0xCB65U
                               || request.routine.logical_pc == 0xCB74U
                               || request.routine.logical_pc == 0xCB76U
                               || request.routine.logical_pc == 0xCB79U
                               || request.routine.logical_pc == 0xCB81U
                               || request.routine.logical_pc == 0xCB8AU
                               || request.routine.logical_pc == 0xCBABU
                               || request.routine.logical_pc == 0xCBB6U
                               || request.routine.logical_pc == 0xCBC2U
                               || request.routine.logical_pc == 0xCBCAU
                               || request.routine.logical_pc == 0xCBCFU
                               || request.routine.logical_pc == 0xCD74U
                               || request.routine.logical_pc == 0xCDF8U
                               || request.routine.logical_pc == 0xCE22U
                               || request.routine.logical_pc == 0xCE4DU
                               || request.routine.logical_pc == 0xCEB5U
                               || request.routine.logical_pc == 0xCF33U
                               || request.routine.logical_pc == 0xCF42U
                               || request.routine.logical_pc == 0xCF45U
                               || request.routine.logical_pc == 0xCF53U
                               || request.routine.logical_pc == 0xCF56U
                               || request.routine.logical_pc == 0xCF60U
                               || request.routine.logical_pc == 0xCF63U
                               || request.routine.logical_pc == 0xCF6AU
                               || request.routine.logical_pc == 0xCF8CU)
                           && request.all_calls;
                       const bool proven_sorcerer_tick =
                           request.routine.physical_bank == 0x02U
                           && (request.routine.logical_pc == 0x586FU
                               || request.routine.logical_pc == 0x5886U
                               || request.routine.logical_pc == 0x5893U
                               || request.routine.logical_pc == 0x58C2U
                               || request.routine.logical_pc == 0x58DDU)
                           && request.all_calls;
                       const bool proven_actor_geometry_leaf =
                           request.routine.physical_bank == 0x01U
                           && (request.routine.logical_pc == 0xC8F3U
                               || request.routine.logical_pc == 0xC969U
                               || request.routine.logical_pc == 0xDAF8U)
                           && request.all_calls;
                       const bool proven_range_helper_return =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC9DEU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_camera_sweep =
                           request.routine.physical_bank == 0x00U
                           && (request.routine.logical_pc == 0xF4F2U
                               || request.routine.logical_pc == 0xF523U
                               || request.routine.logical_pc == 0xF545U
                               || request.routine.logical_pc == 0xF54EU)
                           && request.all_calls;
                       const bool proven_sat_submission_step =
                           request.routine.physical_bank == 0x08U
                           && (request.routine.logical_pc == 0x4036U
                               || request.routine.logical_pc == 0x4063U
                               || request.routine.logical_pc == 0x4075U
                               || request.routine.logical_pc == 0x4088U
                               || request.routine.logical_pc == 0x4097U)
                           && request.all_calls;
                       const bool proven_actor_dispatch_call =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC51EU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_queue_call =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC520U,
                                   }
                           && request.all_calls;
                       const bool proven_actor_queue_return =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC654U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_scan_run =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCB48U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_scan_candidate_branch =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCB61U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_scan_advance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCB6BU,
                                   }
                           && request.all_calls;
                       const bool proven_sprite_emit_run =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x08U,
                                       .logical_pc = 0x40DFU,
                                   }
                           && request.all_calls;
                       return !cached_position_restore
                           && !proven_sprite_emit_run
                           && !proven_collision_scan_run
                           && !proven_collision_scan_candidate_branch
                           && !proven_collision_scan_advance
                           && !proven_actor_scan_preamble
                           && !proven_main_session_loop
                           && !proven_bank_one_jump_table
                             && !proven_palette_colour_cycle
                            && !proven_score_animation
                            && !proven_score_panel_poll
                           && !proven_score_panel_glyph
                           && !proven_score_panel_redraw
                           && !proven_actor_scan_run
                           && !proven_actor_dispatch_vector
                           && !proven_actor_queue_registration
                           && !proven_actor_scan_advance
                           && !proven_actor_dispatch_call
                           && !proven_actor_queue_call
                           && !proven_actor_queue_return
                           && !proven_sat_tail_clear
                           && !proven_sat_submission_step
                           && !proven_actor_camera_sweep
                           && !proven_sound_timer_driver
                           && !proven_music_sequencer
                           && !proven_bank8_export_vectors
                           && !proven_palette_shadow
                           && !proven_setup_screen_palette_stager
                           && !proven_text_tile_write
                           && !proven_upper_table_actor_tile_schedule
                           && !proven_upper_scheduler_return
                           && !proven_music_voice_gate
                           && !proven_interrupt_chain_jump
                           && !proven_vram_stream_tail
                           && !proven_bit_branch
                           && !proven_upper_table_string_record
                           && !proven_video_update_idle_gate
                           && !proven_boot_work_ram_clear_seams
                           && !proven_bank_call_wrappers
                           && !proven_vblank_handler
                           && !proven_vram_upload
                           && !(request.routine == RoutineKey{0x01U, 0xDA0EU} && request.all_calls)
                           && !proven_resident_pattern_loader
                           && !proven_tilemap_upload
                           && !scene_selection_controller
                           && !proven_collision_tail
                           && !proven_sorcerer_tick
                           && !proven_actor_geometry_leaf
                           && !proven_range_helper_return
                           && !proven_position_integration
                           && !proven_motion_damping
                           && !proven_motion_rotation
                           && !proven_flipper_projection
                           && !proven_flipper_radius_projection
                           && !proven_flipper_angle_projection
                           && !proven_collision_motion_adjustment
                           && !proven_collision_response
                           && !proven_collision_consumer
                           && !proven_initial_collision_consumer
                           && !proven_bank7_mode_gate
                           && !proven_score_accumulator
                           && !proven_score_repeat_wrapper
                           && !proven_rng_update
                           && !proven_signed_word_negation
                           && !proven_unsigned_square_root
                           && !proven_signed_division
                           && !proven_unsigned_fixed_division
                           && !proven_unsigned_24_division
                           && !proven_unsigned_byte_division
                           && !proven_unsigned_byte_multiply
                           && !proven_unsigned_word_multiply_partial
                           && !proven_signed_word_multiply
                           && !proven_side_gate_deadline_check
                           && !proven_unsigned_multiply_round
                           && !proven_signed_fixed_multiply
                           && !proven_actor_camera_activation
                           && !proven_actor_slot_reset
                           && !proven_flipper_input_gate
                           && !proven_flipper_release_arm
                           && !proven_flipper_active_update
                           && !proven_collision_scan_vertical_reject
                           && !proven_collision_mask_lookup
                           && !proven_collision_geometry
                           && !proven_flipper_phase_step
                           && !proven_flipper_collision_response_continuation
                           && !proven_flipper_geometry
                           && !proven_coroutine_scheduler
                           && !proven_nudge_input_gate
                           && !proven_session_input_gate
                           && !proven_collision_vector_rts
                           && !proven_ball_drain_threshold
                           && !proven_motion_substep_split
                           && !proven_ball_substep_loop
                           && !proven_actor_motion_step
                           && !proven_remaining_ball_presentation
                           && !proven_zero_ball_scheduler
                           && !proven_alternate_ball_scheduler
                           && !proven_zero_ball_countdown_tick
                           && !proven_zero_ball_expiry_scheduler
                           && !proven_zero_ball_presentation_gate
                           && !proven_zero_ball_state_gate
                           && !proven_remaining_ball_transition_gate
                           && !proven_remaining_ball_decision
                           && !proven_giant_skeleton_eyeball_gate
                           && !proven_giant_skeleton_eyeball_initializer
                           && !proven_giant_skeleton_eyeball_opening_wait
                           && !proven_giant_skeleton_eyeball_steady_idle
                           && !proven_giant_skeleton_eyeball_final_wait
                           && !proven_gray_blob_initializer
                           && !proven_gray_blob_first_live_prefix
                           && !proven_sorcerer_live_controller_gate
                           && !proven_mercy_star_initializer
                           && !proven_devil_shooter_initializer
                           && !proven_little_skull_initializer
                           && !proven_little_skull_gray_initializer
                           && !proven_little_skull_gray_release
                           && !proven_little_skull_live_continuation
                            && !proven_little_skull_pair_initializer
                            && !proven_little_skull_hidden_initializer
                            && !proven_big_star_initializer
                            && !proven_sorcerer_deployment_initializer
                            && !proven_sorcerer_staging_initializer
                            && !proven_dragon_egg_initializer
                           && !proven_undead_head_initializer
                           && !proven_side_worm_initializers
                           && !proven_side_worm_selector_gate
                           && !proven_side_worm_waiting_parent
                           && !proven_side_worm_active_timer_initializer
                           && !proven_dragon_head_initializer
                           && !proven_giant_skeleton_initializer
                           && !proven_giant_skeleton_controller
                           && !proven_three_blobs_initializer
                           && !proven_three_blobs_controller_initializer
                           && !proven_undead_head_selector_gate
                           && !proven_undead_head_waiting_parent
                           && !proven_walled_star_initializer
                           && !proven_walled_star_access_initializer
                           && !proven_side_star_initializers
                           && !proven_knight_initializers
                           && !proven_side_spider_initializer
                           && !proven_front_spider_initializer
                           && !proven_giant_face_helmet_initializer
                           && !proven_giant_face_helmet_outer_initializers
                           && !proven_mercy_star_gate
                           && !proven_dragon_egg_controller
                           && !proven_dragon_egg_hit_gate
                           && !proven_dragon_egg_hit_continuation
                           && !proven_dragon_egg_post_allocation
                           && !proven_dragon_egg_reset_gate
                           && !proven_big_star_feature_gate
                           && !proven_big_star_pending_gate
                           && !proven_big_star_pending_helper
                           && !proven_big_star_pending_right_helper
                           && !proven_big_star_rotation_wait
                           && !proven_devil_shooter_companion_dormant
                           && !proven_devil_shooter_companion_position
                           && !proven_devil_shooter_companion_appearance
                           && !proven_devil_shooter_companion_active_timer
                           && !proven_devil_shooter_companion_left_active_timer
                           && !proven_devil_shooter_left_active_controller
                           && !proven_devil_shooter_right_active_controller
                           && !proven_dragon_head_hit_gate
                           && !proven_dragon_head_hit_continuation
                           && !proven_devil_shooter_hit_gate
                           && !proven_devil_shooter_release_delay_gate
                           && !proven_devil_shooter_charge_after_audio
                           && !proven_devil_shooter_charge_tail
                           && !(request.routine == RoutineKey{0x02U, 0x51AFU}
                                && request.all_calls)
                           && !((request.routine == RoutineKey{0x01U, 0xC655U}
                                 || request.routine == RoutineKey{0x01U, 0xC699U}
                                 || request.routine == RoutineKey{0x01U, 0xC69EU}
                                 || request.routine == RoutineKey{0x01U, 0xC6A9U})
                                && request.all_calls)
                           && !proven_devil_shooter_appearance_submission_prefix
                           && !proven_devil_shooter_appearance_submission_tail
                           && !proven_devil_shooter_post_launch_gate
                           && !proven_devil_shooter_launch_continuation
                           && !proven_devil_shooter_charge_retraction_calls
                           && !proven_devil_shooter_charge_entry_controller
                           && !proven_upper_parent_animation
                           && !proven_scene_layout_record_loop
                           && !proven_scene_layout_record_loader
                           && !proven_devil_shooter_charge_controller
                           && !proven_devil_shooter_charge_controller_tail
                           && !proven_devil_shooter_charge_position
                           && !proven_devil_shooter_charge_return
                           && !proven_devil_shooter_charge_retraction
                           && !proven_devil_shooter_catch_continuation
                           && !proven_devil_shooter_catch_retraction_tail
                           && !proven_skeleton_insect_left_gate
                           && !proven_skeleton_insect_startup_countdown
                           && !proven_skeleton_insect_left_post_trigger
                           && !proven_skeleton_insect_right_gate
                           && !proven_skeleton_insect_right_startup_countdown
                           && !proven_skeleton_insect_right_post_trigger
                           && !proven_skeleton_insect_center_gate
                           && !proven_skeleton_insect_center_continuation
                           && !proven_skeleton_insect_center_toggle
                           && !proven_skeleton_insect_center_post_delay_handoff
                           && !proven_skeleton_insect_center_opening_gate
                           && !proven_devils_coffin_gate
                           && !proven_devils_coffin_initializer
                           && !proven_devils_coffin_opening
                           && !proven_devils_coffin_closing
                           && !proven_giant_door_hit_gate
                           && !proven_giant_door_hit_continuation
                           && !proven_giant_door_completion_score_prefix
                           && !proven_giant_door_completion_scheduler_prefix
                           && !proven_giant_door_completion_request_prefix
                           && !proven_giant_door_completion_wait_gate
                           && !proven_giant_door_completion_channel_advance
                           && !proven_giant_door_initializer
                           && !proven_giant_face_request_gate
                           && !proven_giant_face_animator_gate
                           && !proven_giant_face_animator_active_wait
                           && !proven_giant_face_crown_gate
                           && !proven_giant_face_mouth_gate
                           && !proven_giant_face_helmet_controller
                           && !proven_giant_face_helmet_shared_return
                           && !proven_giant_face_helmet_hit_continuation
                           && !proven_giant_face_helmet_followup_timer
                           && !proven_four_star_wall_pocket_controller
                           && !proven_walled_star_access_controller
                           && !proven_side_star_access_controller
                           && !proven_four_star_wall_pocket_post_audio_score
                           && !proven_four_star_wall_pocket_post_appearance
                           && !proven_four_star_wall_pocket_animation
                           && !proven_side_star_bonus_entrance_animation
                           && !proven_upper_wall_pocket_hand_initializer
                           && !proven_upper_wall_pocket_hand_live_suffix
                           && !proven_upper_wall_pocket_hand_live_setup
                           && !proven_upper_wall_pocket_hand_countdown
                           && !proven_upper_wall_pocket_hand_opening
                           && !proven_upper_wall_pocket_hand_opening_followup
                           && !proven_upper_wall_pocket_hand_opening_expiry
                           && !proven_upper_wall_pocket_hand_post_opening_schedule
                           && !proven_upper_wall_pocket_hand_post_opening_state
                           && !proven_upper_wall_pocket_hand_post_opening_countdown
                           && !proven_walled_star_hit_gate
                           && !proven_walled_star_post_destruction_idle
                           && !proven_tower_hit_gate
                           && !proven_tower_bonus_terminal
                           && !proven_tower_bonus_continuation
                           && !proven_tower_initializer
                            && !proven_knight_hit_gate
                            && !proven_knight_terminal_score_prefix
                            && !proven_knight_terminal_release_prefix
                            && !proven_knight_terminal_acknowledgement_prefix
                            && !proven_knight_terminal_scheduler
                            && !proven_knight_terminal_release_helper_prefix
                            && !proven_knight_terminal_release_helper_setup_prefix
                            && !proven_knight_terminal_release_helper_table_setup_prefix
                            && !proven_knight_terminal_release_helper_call_prefix
                            && !proven_knight_terminal_release_helper_pointer_prefix
                            && !proven_knight_terminal_release_helper_shared_call_prefix
                            && !proven_knight_terminal_release_helper_post_return_mode_branch_prefix
                            && !proven_knight_terminal_release_helper_post_return_pointer_low_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_pointer_low_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_pointer_high_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_pointer_high_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_scratch_clear_prefix
                            && !proven_knight_terminal_release_helper_post_return_mode_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_mode_signed_branch_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_carry_clear_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_base_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_mode_add_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_add_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_mode_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_base_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix
                            && !proven_knight_terminal_release_helper_post_return_position_high_actor_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_clear_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_attribute_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_attribute_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix
                            && !proven_knight_terminal_release_helper_post_return_actor_field_second_constant_store_prefix
                            && !proven_knight_terminal_release_helper_shared_jump_prefix
                            && !proven_knight_terminal_release_helper_shared_scan_initializer_prefix
                            && !proven_knight_terminal_release_helper_shared_scan_read_prefix
                            && !proven_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix
                            && !proven_knight_terminal_release_helper_shared_scan_increment_prefix
                            && !proven_knight_terminal_release_helper_shared_scan_range_compare_prefix
                            && !proven_knight_terminal_release_helper_shared_scan_loop_branch_prefix
                            && !proven_knight_terminal_release_helper_allocator_load_prefix
                            && !proven_knight_terminal_release_helper_allocator_store_prefix
                            && !proven_knight_terminal_release_helper_allocator_mode_load_prefix
                            && !proven_knight_terminal_release_helper_allocator_mode_branch_prefix
                            && !proven_knight_terminal_release_helper_allocator_return_carry_clear_prefix
                            && !proven_knight_waiting_parent
                           && !proven_knight_movement_parent
                           && !proven_spider_appearance
                           && !proven_spider_controller_parents
                           && !proven_front_spider_scheduler_call_seams
                           && !proven_tower_insect_animation
                           && !proven_impact_burst_lifetime
                           && !proven_impact_burst_audio_command_call
                           && !proven_impact_burst_allocator_success_return
                           && !proven_tower_insect_initial_controller
                           && !proven_tower_insect_controller
                           && !proven_small_soldier_source_gates
                           && !proven_small_soldier_source_initializers
                           && !proven_small_soldier_delayed_spawn_gates
                           && !proven_small_soldier_child_allocation_seams
                           && !proven_collision_effect_allocator
                           && !proven_small_soldier_initial_controller
                           && !proven_giant_face_controller_initializers
                           && !proven_giant_face_animator_initializer
                           && !proven_giant_face_crown_initializer
                           && !proven_giant_face_mouth_initializer
                           && !proven_tower_insect_source_gate
                           && !proven_tower_insect_source_countdown_wait
                           && !proven_tower_insect_source_countdown_return
                           && !proven_tower_insect_child_spawner_seams
                           && !proven_tower_insect_source_initializer
                           && !proven_sorcerer_color_gate
                           && !proven_tower_hit_continuation
                           && !proven_tower_post_dispatch
                           && !proven_tower_guard_selector
                           && !proven_bumper_hit_gate
                           && !proven_bumper_initializers
                           && !proven_bumper_child_initializer
                           && !proven_bumper_primary_progress
                            && !proven_bumper_secondary_progress
                            && !proven_fixed_response_gate
                            && !proven_fixed_scheduler_gate
                            && !proven_fixed_scheduler_initializer
                            && !proven_fixed_marker_initializer
                            && !proven_fixed_response_initializer
                            && !proven_dragon_egg_controller_initializer
                            && !proven_three_blobs_rng_initialization
                            && !proven_three_blobs_rng_loop_branch
                            && !proven_math_gate_seam
                            && !proven_math_transfer_seam
                            && !proven_ranked_control_seam
                            && !proven_ranked_timer_seam
                            && !proven_eyeball_countdown_seam
                            && !proven_bumper_sprite_seam
                            && !proven_bonus_countdown_seam
                            && !proven_ranked_actor_seam
                            && !proven_tower_source_scheduler_call
                            && !proven_collision_table_call
                            && !proven_three_blobs_first_movement_prefix
                            && !proven_three_blobs_first_movement_continuation
                            && !proven_three_blobs_y_expiry_negation_prefix
                            && !proven_three_blobs_alternate_motion_reset_prefix
                            && !proven_three_blobs_alternate_motion_arithmetic
                            && !proven_three_blobs_movement_call_seam
                            && !proven_three_blobs_direct_continuation
                            && !proven_three_blobs_shared_motion_idle
                            && !proven_three_blobs_shared_return
                            && !proven_three_blobs_collision_gate
                            && !proven_three_blobs_collision_marker
                            && !proven_fixed_scheduler_submission
                           && !proven_fixed_marker_gate
                           && !proven_fixed_marker_submission
                           && !proven_fixed_marker_helper
                            && !proven_fixed_marker_outer_segment
                           && !proven_scaled_word_add
                           && !proven_three_blobs_controller
                           && !proven_three_blobs_appearance_gate
                           && !proven_collision_table_pass
                           && !proven_upper_table_parent_idle_gate
                           && !proven_bonus_hazard_timer;
                   })) {
        options.error =
            "Native replacement currently supports 00:F241, semantic 02:449B@all, semantic 05:7356@all, "
            "semantic "
            "00:F1B4@all, and the proven no-interrupt 00:F1B4@1, 00:F1B4@10, "
            "00:F1B4@17, semantic 01:D1A2@all, 01:D1A2@23, "
            "semantic 01:DD09@all, 01:DD09@1, 01:DD09@2, "
            "01:DD09@181, 03:6966@1, 03:6966@2, 03:6966@3, "
            "03:6966@4, 03:6966@5, 03:6966@6, 03:6966@7, "
            "03:6966@8, 03:6966@9, 03:6966@10, 03:6966@12, "
            "03:6966@14, 03:6966@15, 03:6966@16, 03:6966@17, "
            "03:6966@18, 03:6966@19, 03:6966@20, 03:6966@21, "
            "03:6966@22, 03:6966@23, semantic 03:6966@all, semantic "
            "03:7FB1@all, semantic 03:7F1A@all, semantic 03:7F57@all, "
            "semantic 07:4003@all, "
            "semantic 08:584C@all, semantic 08:584F@all, "
            "semantic 08:5852@all, semantic 08:5861@all, "
            "semantic 00:F54F@all, semantic 00:F55C@all, "
            "semantic 00:E8B3@all, semantic 00:E837@all, "
            "semantic 00:E905@all, semantic 00:E7D7@all, "
            "semantic 00:E728@all, "
            "semantic 00:E6DC@all, "
            "semantic 00:E569@all, "
            "semantic 00:E5BC@all, "
            "semantic 02:4FC4@all, "
            "semantic 00:E624@all, "
            "semantic 00:E65C@all, "
            "semantic 01:D09E@all, semantic 01:D612@all, "
            "semantic 01:D62E@all, "
            "semantic 01:CFD8@all, "
            "semantic 01:CDF8/CE22/CE4D/CEB5@all, semantic "
            "01:CF33/CF42/CF45/CF53/CF56/CF60/CF63/CF6A/CF8C@all, "
            "semantic 00:F4FC@all, semantic "
            "01:C574@all, semantic 01:D466@all, semantic 01:D4B3@all, "
            "semantic 01:D458@all, semantic 01:D48E@all, semantic 01:D491@all, "
            "semantic 01:D494@all, "
            "semantic 01:D4C6@all, semantic 01:D4C9@all, "
            "semantic 01:D4CC@all, "
            "semantic 01:CC1A@all, "
            "01:CC1A@19176, "
            "semantic 01:CC98@all, "
            "semantic 01:CCD5@all, "
            "semantic "
            "01:D500@all, semantic 01:D525/D52C/D53B/D54A/D54D/D550/D553/"
            "D556/D589/D594/D599/D5A9/D5B8/D5BB/D5BE/"
            "D5C1/D5C4/D5D8@all, "
            "semantic 01:D081@all, semantic 01:C5C9@all, "
            "semantic 01:C3CB@all, semantic 01:C49E@all, semantic 01:DD6E@all, "
            "semantic 01:DC95@all, "
            "semantic 01:C6C2/C6C5/C6D9/C6E6/C6E9/C6EB/C6F2/C6F5/C708@all, "
            "semantic 01:C721@all, semantic 01:C72E@all, semantic 01:C742@all, "
            "semantic 01:C74C@all, semantic 01:C759@all, semantic 01:C75C@all, "
            "semantic 01:C75E@all, semantic 01:C761@all, semantic 01:C768@all, "
            "semantic 01:C76B@all, semantic 01:C76E@all, "
            "semantic 00:F08E@all, semantic 00:F09A@all, semantic 00:F0A4@all, "
            "semantic 00:F0AE@all, semantic 00:F0B8@all, semantic 00:F0C0@all, "
            "semantic 00:F0FE@all, semantic 00:F104@all, semantic 00:F142@all, "
            "semantic 00:F148@all, semantic 00:F17E@all, "
            "semantic 01:C4DE@all, "
            "semantic 01:C157@all, semantic 01:C15A@all, semantic 01:C15D@all, "
            "semantic 01:C166@all, semantic 01:C169@all, semantic 01:C16C@all, "
            "semantic 01:C16F@all, semantic 01:C17A@all, semantic 01:C185@all, "
            "semantic 01:C190@all, "
            "semantic 01:C012@all, semantic 01:C018@all, semantic 01:C021@all, "
            "semantic 01:C024@all, semantic 01:C027@all, semantic 01:C02A@all, "
            "semantic 01:C03C@all, semantic 01:C042@all, semantic 01:C05D@all, "
            "semantic 01:C060@all, semantic 01:C063@all, semantic 01:C069@all, "
            "semantic 01:C06C@all, semantic 01:C06F@all, semantic 01:C081@all, "
            "semantic 01:C087@all, semantic 01:C09F@all, semantic 01:C0A2@all, "
            "semantic 09:60C7@all, "
            "semantic 09:70C1@all, semantic 09:70C4@all, "
            "semantic 09:71EA@all, semantic 09:7237@all, "
            "semantic 09:727D/72A2/72BD/72C5@all, "
            "semantic 09:72FD@all, "
            "semantic 01:C80A@all, semantic 01:C7F4@all, semantic 01:C7EF@all, semantic 01:C7F9@all, semantic 01:C7FC@all, semantic 01:C804@all, semantic 01:C812@all, semantic 01:C7EA@all, semantic 01:C7E5@all, "
            "semantic 02:427C@all, semantic 02:42A3@all, semantic 02:42B0@all, "
            "semantic 02:42EB@all, "
            "semantic 02:42F6@all, semantic 02:4345@all, "
            "semantic 02:4357@all, semantic 02:4371@all, "
            "semantic 02:4387@all, "
            "semantic 02:445B@all, "
            "semantic 02:570A@all, semantic 02:5674@all, "
            "semantic 02:5397@all, semantic 02:5408@all, semantic 02:53C8@all, semantic 02:53E4@all, semantic 02:5441@all, semantic 02:545D@all, semantic 02:53DD@all, semantic 02:5456@all, "
            "semantic 02:4DC8@all, semantic 02:4DD9@all, semantic 02:4DF7@all, semantic 02:4E24@all, semantic 02:4F3D@all, semantic 03:76C5@all, "
            "semantic 03:6876@all, semantic 03:68A1@all, semantic 02:5A31@all, semantic 02:5EE4@all, semantic 02:5EEC@all, semantic 03:664A@all, semantic 03:65BC@all, semantic 03:66DE@all, "
            "semantic 02:57AD@all, "
            "semantic 02:4FC1@all, "
            "semantic 02:4090@all, semantic 02:4096@all, semantic 02:4099@all, semantic 02:40A7@all, semantic 02:40F6@all, semantic 02:40FB@all, semantic 02:40FE@all, "
            "semantic 02:4A93@all, semantic 02:4AD2@all, "
            "semantic 02:4AE5@all, semantic 02:4AEB@all, "
            "semantic 02:4B3F@all, "
            "semantic 02:4BD5@all, semantic 02:4C0A@all, semantic 02:4C33@all, semantic 02:4CF1@all, semantic 02:4CFB@all, semantic 02:4D0B@all, "
            "semantic 02:5116@all, "
            "semantic 02:5159@all, "
            "semantic 02:518D@all, "
            "semantic 02:5197@all, "
            "semantic 02:5475@all, semantic 02:5481@all, "
            "semantic 02:5208@all, semantic 02:520B@all, "
            "semantic 02:5249@all, "
            "semantic 02:524C@all, "
            "semantic 02:525F@all, "
            "semantic 02:52E3@all, "
            "semantic 02:533A@all, "
            "semantic 02:533B@all, "
            "semantic 02:52B8@all, "
            "semantic 02:52C3@all, semantic 02:52C6@all, "
            "semantic 02:54B9@all, "
            "semantic 02:54D0@all, "
            "semantic 02:5500@all, "
            "semantic 02:5551@all, "
            "semantic 02:5568@all, "
            "semantic 02:5590@all, "
            "semantic 02:55F9@all, "
            "semantic 02:5604@all, "
            "semantic 02:561A@all, "
            "semantic 02:5625@all, "
            "semantic 02:5630@all, "
            "semantic 02:5A4B@all, "
            "semantic 02:5A6E@all, "
            "semantic 03:63F1@all, "
            "semantic 03:640A@all, "
            "semantic 03:6445@all, "
            "semantic 03:6294@all, semantic 03:62B8@all, semantic 03:62CB@all, "
            "semantic 03:62D4@all, semantic 03:62E2@all, "
            "semantic 03:74D8@all, "
            "semantic 03:767E@all, "
            "semantic 03:76FA@all, "
            "semantic 03:7726@all, "
            "semantic 03:6339@all, "
            "semantic 03:60C3@all, "
            "semantic 03:60F9@all, "
            "semantic 03:6136@all, "
            "semantic 03:60B3@all, "
            "semantic 03:7378@all, "
            "semantic 03:73A6@all, "
             "semantic 03:73D8@all, "
             "semantic 03:73FE@all, "
             "semantic 03:740A@all, "
             "semantic 03:7418@all, "
             "semantic 03:7420@all, "
             "semantic 02:41A0@all, "
             "semantic 02:41A8@all, "
             "semantic 02:41AD@all, "
             "semantic 02:41C2@all, "
             "semantic 02:41F5@all, "
             "semantic 02:420B@all, "
             "semantic 02:420E@all, "
             "semantic 02:4210@all, "
             "semantic 02:4212@all, "
             "semantic 02:4215@all, "
             "semantic 02:4217@all, "
             "semantic 02:421A@all, "
             "semantic 02:421C@all, "
             "semantic 02:421E@all, "
             "semantic 02:4222@all, "
             "semantic 02:4223@all, "
             "semantic 02:4226@all, "
             "semantic 02:4228@all, "
             "semantic 02:422B@all, "
             "semantic 02:422E@all, "
             "semantic 02:4230@all, "
             "semantic 02:4233@all, "
             "semantic 02:4235@all, "
             "semantic 02:4237@all, "
             "semantic 02:423B@all, "
             "semantic 02:423C@all, "
             "semantic 02:423F@all, "
             "semantic 02:4241@all, "
             "semantic 02:4244@all, "
             "semantic 02:4247@all, "
             "semantic 02:4249@all, "
             "semantic 02:424C@all, "
             "semantic 02:424D@all, "
             "semantic 02:4250@all, "
             "semantic 02:4253@all, "
             "semantic 02:4256@all, "
             "semantic 02:4258@all, "
             "semantic 02:425A@all, "
             "semantic 02:425D@all, "
             "semantic 02:425F@all, "
             "semantic 02:4262@all, "
             "semantic 02:4264@all, "
             "semantic 01:C024@all, "
             "semantic 01:C5AB@all, "
             "semantic 01:C5AD@all, "
             "semantic 01:C5B0@all, "
             "semantic 01:C5B2@all, "
             "semantic 01:C5B3@all, "
             "semantic 01:C5B5@all, "
             "semantic 01:C5B9@all, "
             "semantic 01:C5BB@all, "
             "semantic 01:C5BE@all, "
             "semantic 01:C5C0@all, "
             "semantic 01:C5C7@all, "
             "semantic 03:6490@all, semantic 03:651F@all, "
             "semantic 03:65A4@all, semantic 03:64D2/64DD/64ED@all, "
            "semantic 03:6054@all, "
            "semantic 02:5F22@all, "
            "semantic 02:5F4A@all, "
            "semantic 02:5F5B@all, "
            "semantic 02:5F78/5F7B/5F7E/5FD6@all, "
            "semantic 02:5F5F@all, "
            "semantic 02:5FDB@all, semantic 02:5FE6@all, "
            "semantic 03:7D85@all, semantic 03:7E23@all, "
            "semantic 03:7D4D@all, semantic 03:7DE9@all, "
            "semantic 03:7D6F@all, semantic 03:7E0D@all, "
            "semantic 03:7428@all, semantic 03:742B@all, "
            "semantic 03:742E@all, "
            "semantic 03:766C@all, "
            "semantic 03:76F7@all, "
            "semantic 03:7718@all, "
            "semantic 02:57E9@all, "
            "semantic 03:60E0@all, "
            "semantic 03:611C@all, "
            "semantic 03:60A6@all, "
            "semantic 03:78C7@all, "
            "semantic 03:789A@all, semantic 03:78A4@all, "
            "semantic 03:79DE@all, "
            "semantic 03:78F5@all, "
            "semantic 03:7912@all, "
            "semantic 03:7CAF@all, "
            "semantic 03:7CA7@all, "
            "semantic 03:7B27@all, "
            "semantic 03:7B4E@all, semantic 03:7B5D@all, "
            "semantic 03:7B7A@all, "
            "semantic 03:7BB1/7BB5/7BC3/7BC7/7BD5/7BD9/7BDF@all, "
            "semantic 03:7BBB@all, semantic 03:7BCD@all, "
            "semantic 03:7BE0@all, semantic 03:7BE5@all, "
            "semantic 03:7BE8@all, semantic 03:7BE9@all, "
            "semantic 03:7BEC@all, "
            "semantic 02:5955@all, "
            "semantic 02:5972@all, semantic 02:5975@all, semantic 02:5991@all, "
            "semantic 02:59A5@all, "
              "semantic 02:59C7@all, "
              "semantic 02:59E0@all, "
              "semantic 02:5A08@all, "
              "semantic 02:5A0B@all, "
              "semantic 02:5A1B@all, "
            "semantic 03:65BC@all, semantic 03:65C2@all, semantic 03:67B3@all, "
            "semantic 03:6678/667B/6697/669A@all, "
            "semantic 03:66A3/66B5@all, semantic 03:670F@all, "
            "semantic 03:6729@all, semantic 03:6740@all, and semantic "
            "03:676B@all, semantic 03:67E5@all, semantic 03:67C0/67CF@all, "
            "semantic 03:694A@all, "
            "semantic 03:7BF6/7C02/7C05@all, semantic "
            "03:7E87/7E8A/7EEE@all, semantic 03:7EFA/7F05@all, "
            "the proven 03:7FB1@1, 03:7FB1@3, 03:7FB1@7, 00:F55C@1, "
            "and 00:E8B3@1 calls.";
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
