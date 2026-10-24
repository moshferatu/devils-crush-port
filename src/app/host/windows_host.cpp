// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_host.hpp"

#include "cabinet_table.hpp"
#include "census_writers.hpp"
#include "command_line.hpp"
#include "gameplay_bootstrap_shadow.hpp"
#include "high_score_autosave.hpp"
#include "host_audio.hpp"
#include "host_files.hpp"
#include "host_input.hpp"
#include "host_present.hpp"
#include "host_textures.hpp"
#include "host_windows.hpp"
#include "interactive_session.hpp"
#include "native_registry_install.hpp"
#include "play_assists.hpp"
#include "recording_inputs.hpp"
#include "registry_reports.hpp"
#include "table_motion.hpp"
#include "trace_writers.hpp"
#include "windows_host_session.hpp"

#include "devils_crush/cabinet/bonus_ball_color.hpp"

namespace devils_crush::app::host {

int run_windows_host(
    PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const bool cabinet_mode,
    const std::uint8_t bonus_stage,
    const int frame_limit,
    const CommandLine* native_registry,
    const CabinetLayers layers,
    const bool overlay_stats,
    const std::filesystem::path& capture_session_directory,
    const bool native_gameplay_bootstrap_shadow,
    const double nudge_multiplier,
    const bool presentation_check,
    const bool drag_ball,
    const bool skip_perfect_jingle)
{
    HostSession session{
        .runtime = runtime,
        .rom_path = rom_path,
        .cabinet_mode = cabinet_mode,
        .bonus_stage = bonus_stage,
        .frame_limit = frame_limit,
        .native_registry = native_registry,
        .layers = layers,
        .overlay_stats = overlay_stats,
        .capture_session_directory = capture_session_directory,
        .native_gameplay_bootstrap_shadow = native_gameplay_bootstrap_shadow,
        .nudge_multiplier = nudge_multiplier,
        .presentation_check = presentation_check,
        .drag_ball = drag_ball,
    };
    session.music_transition.set_skip_perfect_jingle(skip_perfect_jingle);
    if (!session.open_windows()) {
        return EXIT_FAILURE;
    }
    session.prepare();
    session.install_game_hooks();
    session.run();
    return session.close();
}

bool HostSession::open_windows()
{
    // Joystick as well as gamepad: the plunger is an anonymous HID axis that
    // SDL's gamepad database has no mapping for, so it is only visible through
    // the joystick subsystem.
    if (!SDL_Init(
            SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        report_sdl_error("SDL_Init failed");
        return false;
    }
    // Nothing but the ball-dragging cheat reads the mouse, and a pointer
    // parked on the playfield is just something to look at. This is
    // application-wide, so it covers the DMD and backglass windows too, and
    // `SDL_Quit` puts it back.
    if (!drag_ball && !SDL_HideCursor()) {
        std::cerr << "Could not hide the mouse cursor: " << SDL_GetError()
                  << '\n';
    }

    audio.available = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!audio.available) {
        std::cerr << "Audio disabled: " << SDL_GetError() << '\n';
    }
    (void)load_gamepad_mappings(kGamepadMappingsPath);

    cabinet_rom =
        cabinet_mode ? read_rom_bytes(rom_path) : std::nullopt;
    if (cabinet_mode && !cabinet_rom) {
        destroy_audio(audio);
        SDL_Quit();
        return false;
    }

    persist_cabinet_config =
        cabinet_mode && frame_limit <= 0;
    // Read in either mode, so the one window plays with the same keys and
    // smoothing as the three; written only by the cabinet, which is where
    // windows are assigned.
    if (frame_limit <= 0 || presentation_check) {
        const auto loaded =
            devils_crush::cabinet::load_display_config(
                kCabinetConfigPath);
        if (loaded.loaded()) {
            cabinet_config = loaded.config;
            std::cout << "Loaded cabinet configuration: "
                      << kCabinetConfigPath.string() << '\n';
        } else if (
            loaded.status
            != devils_crush::cabinet::ConfigLoadStatus::not_found) {
            std::cerr << loaded.message
                      << " Using safe window defaults.\n";
        }
    }
    // The artwork rotation is the cabinet's: its playfield monitor stands on
    // end, and a desktop window does not.
    if (!cabinet_mode) {
        cabinet_config.playfield_rotation_degrees = 0;
    }
    // Resolved once: the names cannot change while the host runs, and this is
    // where an unusable one gets reported.
    key_bindings = resolve_key_bindings(cabinet_config.input);
    host_displays =
        cabinet_mode ? enumerate_host_displays() : HostDisplays{};

    window_title =
        playfield_title(cabinet_mode, native_registry != nullptr);
    window = SDL_CreateWindow(
        window_title.c_str(),
        cabinet_mode ? 720 : 1280,
        cabinet_mode ? 1280 : 720,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY
            | (cabinet_config.play.borderless ? SDL_WINDOW_BORDERLESS
                                              : SDL_WindowFlags{0}));
    if (window == nullptr) {
        report_sdl_error("SDL_CreateWindow failed");
        destroy_audio(audio);
        SDL_Quit();
        return false;
    }
    if (cabinet_mode
        && !apply_window_placement(
            window,
            cabinet_config.playfield,
            host_displays,
            720,
            1280)) {
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        report_sdl_error("SDL_CreateRenderer failed");
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return false;
    }

    if (cabinet_mode) {
        if (!create_auxiliary_window(
                "Devil's Crush - Cabinet DMD",
                960,
                240,
                cabinet_config.dmd,
                host_displays,
                cabinet_config.play.borderless,
                dmd)) {
            std::cerr
                << "Cabinet DMD unavailable; continuing with playfield.\n";
        }
        if (!create_auxiliary_window(
                "Devil's Crush - Cabinet Backglass",
                960,
                540,
                cabinet_config.backglass,
                host_displays,
                cabinet_config.play.borderless,
                backglass)) {
            std::cerr
                << "Cabinet backglass unavailable; continuing without it.\n";
        }
    }

    vsync_enabled = SDL_SetRenderVSync(renderer, 1);
    if (!vsync_enabled) {
        std::cerr << "VSync unavailable; using timer pacing: "
                  << SDL_GetError() << '\n';
    }
    if (!SDL_ShowWindow(window)) {
        report_sdl_error("SDL_ShowWindow failed");
        destroy_auxiliary_window(dmd);
        destroy_auxiliary_window(backglass);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return false;
    }
    return true;
}

void HostSession::prepare()
{
    // Only for a run somebody is sitting in front of. The frame-limited runs
    // the tests make would otherwise pull the foreground off whatever the
    // machine was doing, several times over a suite.
    if (frame_limit <= 0) {
        focus_playfield_window(window);
        focus_claim.deadline_ms = SDL_GetTicks() + kStartupFocusWindowMs;
    }

    gamepad = open_first_gamepad();
    plunger.model.configure(cabinet_config.input.plunger.calibration);
    open_plunger(plunger, cabinet_config.input.plunger);
    nudge.binding = cabinet_config.input.nudge;
    nudge.binding.enabled = nudge.binding.enabled && cabinet_mode;
    nudge.refresh();
    joystick_inputs.wanted = std::ranges::any_of(
        key_bindings.pad_joystick,
        [](const auto& buttons) { return !buttons.empty(); });
    joystick_inputs.refresh();
    backglass_logo =
        cabinet_rom
        ? devils_crush::render::TitleLogoAnimation::decode(
              *cabinet_rom)
        : std::nullopt;
    dmd_font =
        cabinet_rom
        ? devils_crush::cabinet::DmdFont::decode(*cabinet_rom)
        : std::nullopt;
    if (cabinet_mode && !dmd_font) {
        std::cerr
            << "Original DMD font decode failed; disabling DMD.\n";
        destroy_auxiliary_window(dmd);
    }
    // Only the DMD's attract carousel draws it, and it turns without.
    dmd_ending_head =
        cabinet_rom
        ? devils_crush::render::EndingHeadAnimation::decode(*cabinet_rom)
        : std::nullopt;
    if (cabinet_mode && !backglass_logo) {
        std::cerr
            << "Native title-logo decode failed; the backglass will "
               "use its static fallback.\n";
    }
    smoothing_enabled =
        presentation_check || cabinet_config.play.smoothing;
    // This routine is a cross-frame presentation controller, outside the
    // replacement registry. Observing entry leaves all CPU execution intact.
    // The session's result bit alone is also set during bonus-stage setup.
    if (cabinet_mode && dmd.renderer != nullptr
        && !runtime.install_execution_hook({
            .routine = {.physical_bank = 0x04U, .logical_pc = 0x456CU},
            .mode = HookMode::observe,
            .callback = [this](const RoutineEntry&) {
                dmd_panel.begin_bonus_result();
            },
        })) {
        std::cerr << "Could not observe bonus results; disabling DMD.\n";
        destroy_auxiliary_window(dmd);
    }
    // Name entry keeps its letters out of the high-score table until it ends,
    // so the DMD is told when it runs rather than reading the table.
    if (cabinet_mode && dmd.renderer != nullptr
        && !(runtime.install_execution_hook({
                 .routine = {.physical_bank = 0x09U, .logical_pc = 0x6629U},
                 .mode = HookMode::observe,
                 .callback = [this](const RoutineEntry&) {
                     dmd_panel.begin_name_entry();
                 },
             })
            && runtime.install_execution_hook({
                .routine = {.physical_bank = 0x09U, .logical_pc = 0x6698U},
                .mode = HookMode::observe,
                .callback = [this](const RoutineEntry&) {
                    dmd_panel.end_name_entry();
                },
            }))) {
        std::cerr << "Could not observe high-score name entry; disabling DMD.\n";
        destroy_auxiliary_window(dmd);
    }
    // `09:6ED6` is the two-player "PLAYER n / READY" screen, and `09:6F2D` its
    // return. Between turns `00:F7C8` fades the table out, clears the
    // background and calls it with `$54` still selecting the table and the
    // table's attributes still in RAM, so nothing in work RAM says the table
    // is not what is on screen. The count is the fallback for a return that is
    // never seen: the screen waits `$50` frames at most.
    constexpr unsigned kReadyScreenFrames = 180U;
    if (cabinet_mode
        && !(runtime.install_execution_hook({
                 .routine = {.physical_bank = 0x09U, .logical_pc = 0x6ED6U},
                 .mode = HookMode::observe,
                 .callback = [this](const RoutineEntry&) {
                     ready_screen_frames = kReadyScreenFrames;
                 },
             })
            && runtime.install_execution_hook({
                .routine = {.physical_bank = 0x09U, .logical_pc = 0x6F2DU},
                .mode = HookMode::observe,
                .callback = [this](const RoutineEntry&) {
                    ready_screen_frames = 0U;
                },
            }))) {
        std::cerr << "Could not observe the player ready screen; "
                     "it will not be shown between turns.\n";
    }
    // The ball-dragging cheat's ROM, for the ball's sprite, and where the
    // playfield's picture last went.
    drag_rom =
        drag_ball ? read_rom_bytes(rom_path) : std::nullopt;

    frame_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / kEmulationFramesPerSecond));
    next_frame_time = Clock::now();
    dmd_high_score_epoch = Clock::now();

    bonus_stage_return_pending = bonus_stage != 0U;
    capture_session = !capture_session_directory.empty();
}

void HostSession::install_game_hooks()
{
    // The registry is installed last, once every fallible setup step has
    // succeeded, so no early return can leave a hook pointing at a dead
    // deployment. Nothing below returns before the shutdown tail clears them.
    replacement_rom = native_registry != nullptr
        ? read_rom_bytes(rom_path)
        : std::optional<std::vector<std::byte>>{std::vector<std::byte>{}};
    gameplay_shadow_rom = native_gameplay_bootstrap_shadow
        ? read_rom_bytes(rom_path)
        : std::optional<std::vector<std::byte>>{};
    if (native_gameplay_bootstrap_shadow && !gameplay_shadow_rom) {
        std::cerr << "Could not read ROM bytes for native gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    constexpr std::array<RoutineKey, 1U> kHostClaimedRoutines{kShooterGateInstruction};
    if (native_registry != nullptr) {
        if (!replacement_rom
            || !install_native_registry(
                runtime,
                *native_registry,
                *replacement_rom,
                kHostClaimedRoutines,
                registry_deployment,
                native_gameplay_bootstrap_shadow
                    ? NativeRegistryEntryObserver{
                          [this](
                              const RoutineEntry& entry,
                              const std::span<const std::byte> work_ram) {
                              gameplay_bootstrap_shadow
                                  .observe_playfield_collision_source(
                                      entry, work_ram, *gameplay_shadow_rom);
                              gameplay_bootstrap_shadow
                                  .observe_short_actor_pass_entry(
                                      entry, work_ram);
                              gameplay_bootstrap_shadow
                                  .observe_palette_fade_block_entry(
                                      entry, work_ram);
                              gameplay_bootstrap_shadow.observe_motion_entry(
                                  entry, work_ram, *gameplay_shadow_rom);
                          }}
                    : NativeRegistryEntryObserver{})) {
            running = false;
            host_ok = false;
        } else if (native_registry->execution_details) {
            const auto deployed =
                static_cast<std::size_t>(std::count_if(
                    registry_deployment.rows.begin(),
                    registry_deployment.rows.end(),
                    [](const RegistryCensusRow& row) {
                        return !row.skipped;
                    }));
            std::cout << "Native registry: " << deployed << " of "
                      << registry_deployment.rows.size()
                      << " admitted routines deployed ("
                      << kWithheldRoutines.size()
                      << " withheld as state-diverging, "
                      << kHostClaimedRoutines.size()
                      << " claimed by the host).\n";
            // Named rather than merely counted, on the same terms as the
            // hold-backs: a routine the host takes back is one the registry
            // proved and the shipped configuration does not run, and that is
            // worth having to read.
            for (const auto& claimed : kHostClaimedRoutines) {
                std::cout << "Registry claim: "
                          << format_routine_key(claimed)
                          << " claimed by cabinet flipper input handoff.\n";
            }
        }
    }
    if (host_ok && native_gameplay_bootstrap_shadow
        && !install_gameplay_bootstrap_shadow_observers(
            runtime,
            gameplay_bootstrap_shadow,
            *gameplay_shadow_rom,
            native_registry != nullptr)) {
        running = false;
        host_ok = false;
    }
    // After the registry, so a deployed replacement owning either address
    // would be reported as the collision it is rather than silently winning.
    if (host_ok
        && !install_shooter_flipper_gate(
            runtime, devils_crush::cabinet::flipper_inputs())) {
        std::cerr << "Could not free the flippers during the launch.\n";
        running = false;
        host_ok = false;
    }
    if (host_ok && !music_transition.install(runtime)) {
        std::cerr << "Could not install the bonus music transition.\n";
        running = false;
        host_ok = false;
    }
    if (host_ok && !devils_crush::cabinet::install_bonus_ball_color(runtime)) {
        std::cerr << "Could not keep the bonus ball color in the launcher.\n";
        running = false;
        host_ok = false;
    }
    high_scores = open_high_score_autosave(kHighScorePath);
    if (host_ok && !install_high_score_restore(runtime, high_scores)) {
        std::cerr << "Could not carry the high-score table across runs.\n";
        running = false;
        host_ok = false;
    }
    // Keep the scripted power-on sequence outside interactive capture. Its
    // final frame asks the cartridge to perform the same full transition a
    // table feature would, and the first live frame therefore begins at the
    // bonus entrance rather than in the menus or on the main table.
    if (host_ok && bonus_stage != 0U
        && !prepare_bonus_stage_launch(runtime, bonus_stage)) {
        running = false;
        host_ok = false;
    }
    if (host_ok && capture_session
        && !save_quick_state(
            runtime, capture_session_directory / "initial.state")) {
        std::cerr << "Could not save the interactive capture's initial state.\n";
        running = false;
        host_ok = false;
    }
    if (host_ok && capture_session && high_scores.stored) {
        const auto saved = devils_crush::backup::save_atomic(
            capture_session_directory / "initial.hiscore",
            devils_crush::core::encode_high_score_table(*high_scores.stored),
            devils_crush::core::kDefaultCommitBudgetMilliseconds);
        if (!saved.saved) {
            std::cerr << "Could not save the interactive capture's initial "
                         "high-score table: "
                      << saved.message << '\n';
            running = false;
            host_ok = false;
        }
    }
    if (host_ok && (capture_session || native_registry != nullptr)
        && !runtime.set_instruction_profiling(true)) {
        std::cerr
            << "Could not enable instruction profiling for interpreter "
               "residue reporting.\n";
        running = false;
        host_ok = false;
    }
    if (host_ok && capture_session) {
        std::cout << "Interactive capture armed: initial state, per-frame "
                     "input, instruction profile, and deployment censuses "
                     "will be written to "
                  << capture_session_directory.string()
                  << " when this session closes. Quick-state restore is "
                     "disabled while capturing.\n";
    }
}

void HostSession::save_current_cabinet_config()
{
    if (!persist_cabinet_config) {
        return;
    }
    cabinet_config.playfield = capture_window_placement(
        window,
        host_displays,
        cabinet_config.playfield);
    cabinet_config.dmd = capture_window_placement(
        dmd.window,
        host_displays,
        cabinet_config.dmd);
    cabinet_config.backglass = capture_window_placement(
        backglass.window,
        host_displays,
        cabinet_config.backglass);
    const auto saved =
        devils_crush::cabinet::save_display_config_atomic(
            kCabinetConfigPath,
            cabinet_config);
    if (!saved.saved) {
        std::cerr << saved.message << '\n';
    }
}

int HostSession::close()
{
    if (host_ok && frame_limit > 0 && bonus_stage != 0U) {
        const auto work_ram = runtime.export_work_ram();
        const auto cabinet_state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        if (!cabinet_state || !cabinet_state->bonus_stage
            || work_ram.size() <= kBonusStageBallStateOffset
            || std::to_integer<std::uint8_t>(
                   work_ram[kBonusStageIdOffset]) != bonus_stage
            || (std::to_integer<std::uint8_t>(
                    work_ram[kBonusStageBallFlagsOffset])
                & kBonusStageBallHeldBit) != 0U
            || (std::to_integer<std::uint8_t>(
                    work_ram[kBonusStageBallStateOffset])
                & kBonusStageBallOffTableBit) != 0U) {
            std::cerr << "Cabinet host did not reach a playable ball in bonus stage "
                      << static_cast<unsigned int>(bonus_stage)
                      << " through its smoke frames.\n";
            host_ok = false;
        } else {
            std::cout << "Bonus-stage smoke reached a playable ball.\n";
        }
    }

    presentation_sample.report(smoothing_enabled);
    // Before the hooks go, so a table that changed inside the last second of
    // the session is still written.
    flush_high_scores(high_scores);
    // The hooks capture the deployment and the replacement ROM, both of which
    // die with this frame, so they must go before either does.
    runtime.clear_execution_hooks();
    runtime.clear_work_ram_write_observer();
    destroy_audio(audio);
    table_motion.reset();
    frame_artwork.reset();
    viewport_motion.reset();
    destroy_stage(stage);
    destroy_source(source);
    destroy_stage(backglass_logo_stage);
    destroy_source(backglass_logo_source);
    if (gamepad != nullptr) {
        SDL_CloseGamepad(gamepad);
    }
    close_plunger(plunger);
    nudge.close();
    joystick_inputs.close();
    save_current_cabinet_config();
    destroy_stage(dmd_stage);
    destroy_source(dmd_source);
    destroy_auxiliary_window(dmd);
    destroy_auxiliary_window(backglass);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (overlay_stats) {
        seeded_actors.report();
    }
    if (native_gameplay_bootstrap_shadow) {
        gameplay_bootstrap_shadow.report();
    }
    if (native_registry != nullptr) {
        print_native_registry_summary(
            registry_deployment,
            runtime.replacement_nesting(),
            runtime,
            native_registry->execution_details,
            true);
        if (!native_registry->native_cycle_census_path.empty()
            && !write_native_cycle_census(
                native_registry->native_cycle_census_path,
                registry_deployment.rows,
                registry_deployment.span,
                runtime.info().upstream_revision)) {
            host_ok = false;
        }
    }
    if (capture_session) {
        const auto core_revision = runtime.info().upstream_revision;
        if (!write_interactive_input_capture(
                capture_session_directory,
                captured_inputs,
                core_revision,
                cabinet_mode,
                nudge_multiplier)
            || (!captured_inputs.empty()
                && !write_instruction_profile(
                    capture_session_directory / "instruction-profile.csv",
                    runtime.export_instruction_profile(),
                    core_revision))
            || (native_registry != nullptr && !captured_inputs.empty()
                && !write_instruction_profile(
                    capture_session_directory / "interpreter-residue.csv",
                    runtime.export_interpreted_instruction_profile(),
                    core_revision,
                    "interpreter-residue"))
            || !write_abandonment_census(
                capture_session_directory / "abandonment-census.csv",
                runtime.export_abandonment_census(),
                core_revision)
            || (native_registry != nullptr
                && !write_native_cycle_census(
                    capture_session_directory / "native-cycle-census.csv",
                    registry_deployment.rows,
                    registry_deployment.span,
                    core_revision))) {
            host_ok = false;
        }
        if (captured_inputs.empty()) {
            std::cerr << "Interactive capture closed before an emulated "
                         "frame; no instruction profile was written.\n";
        }
    }
    if (host_ok && frame_limit > 0) {
        std::cout << "Cabinet host smoke test passed: "
                  << emulated_frames << " frames.\n";
    }
    return host_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace devils_crush::app::host
