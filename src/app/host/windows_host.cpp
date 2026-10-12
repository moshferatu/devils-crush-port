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

namespace devils_crush::app::host {

namespace {

constexpr std::uint32_t kDisplayAspectNumerator = 4U;
constexpr std::uint32_t kDisplayAspectDenominator = 3U;
constexpr std::array<std::uint8_t, 3> kPureBlack{0U, 0U, 0U};
constexpr std::array<std::uint8_t, 3> kDesktopFrameColor{
    8U,
    8U,
    12U};
// A real backglass sits inside its frame rather than running to the edge of
// the glass. Six per cent of the shorter axis reads as deliberate breathing
// room without shrinking the artwork noticeably: on a 1108-by-739 window it
// is a 44-pixel border. The playfield takes no margin -- it is the one
// surface that should fill its display.
constexpr float kBackglassMarginFraction = 0.06F;
constexpr double kEmulationFramesPerSecond = 59.8261;

const std::filesystem::path kGamepadMappingsPath =
    std::filesystem::path(devils_crush::core::kGamepadMappingsFileName);

[[nodiscard]] bool consume_emulated_frame(
    SDL_Renderer* renderer,
    const EmulatedFrame& frame,
    SourceTexture& source,
    AudioOutput& audio)
{
    if (frame.frame_number == 0U || frame.framebuffer.pixels.empty()) {
        std::cerr << "Beetle PCE did not produce a framebuffer.\n";
        return false;
    }
    return upload_framebuffer(renderer, frame.framebuffer, source)
        && queue_audio(frame.audio, audio);
}

} // namespace

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
    const bool drag_ball)
{
    // Joystick as well as gamepad: the plunger is an anonymous HID axis that
    // SDL's gamepad database has no mapping for, so it is only visible through
    // the joystick subsystem.
    if (!SDL_Init(
            SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        report_sdl_error("SDL_Init failed");
        return EXIT_FAILURE;
    }
    // Nothing but the ball-dragging cheat reads the mouse, and a pointer
    // parked on the playfield is just something to look at. This is
    // application-wide, so it covers the DMD and backglass windows too, and
    // `SDL_Quit` puts it back.
    if (!drag_ball && !SDL_HideCursor()) {
        std::cerr << "Could not hide the mouse cursor: " << SDL_GetError()
                  << '\n';
    }

    AudioOutput audio;
    audio.available = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!audio.available) {
        std::cerr << "Audio disabled: " << SDL_GetError() << '\n';
    }
    (void)load_gamepad_mappings(kGamepadMappingsPath);

    const auto cabinet_rom =
        cabinet_mode ? read_rom_bytes(rom_path) : std::nullopt;
    if (cabinet_mode && !cabinet_rom) {
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    devils_crush::cabinet::DisplayConfig cabinet_config;
    const bool persist_cabinet_config =
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
    const auto key_bindings = resolve_key_bindings(cabinet_config.input);
    auto host_displays =
        cabinet_mode ? enumerate_host_displays() : HostDisplays{};

    const auto window_title =
        playfield_title(cabinet_mode, native_registry != nullptr);
    SDL_Window* window = SDL_CreateWindow(
        window_title.c_str(),
        cabinet_mode ? 720 : 1280,
        cabinet_mode ? 1280 : 720,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        report_sdl_error("SDL_CreateWindow failed");
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        report_sdl_error("SDL_CreateRenderer failed");
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    AuxiliaryWindow dmd;
    AuxiliaryWindow backglass;
    if (cabinet_mode) {
        if (!create_auxiliary_window(
                "Devil's Crush - Cabinet DMD",
                960,
                240,
                cabinet_config.dmd,
                host_displays,
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
                backglass)) {
            std::cerr
                << "Cabinet backglass unavailable; continuing without it.\n";
        }
    }

    const bool vsync_enabled = SDL_SetRenderVSync(renderer, 1);
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
        return EXIT_FAILURE;
    }
    // Only for a run somebody is sitting in front of. The frame-limited runs
    // the tests make would otherwise pull the foreground off whatever the
    // machine was doing, several times over a suite.
    StartupFocusClaim focus_claim;
    if (frame_limit <= 0) {
        focus_playfield_window(window);
        focus_claim.deadline_ms = SDL_GetTicks() + kStartupFocusWindowMs;
    }

    SDL_Gamepad* gamepad = open_first_gamepad();
    PlungerDevice plunger;
    plunger.model.configure(cabinet_config.input.plunger.calibration);
    open_plunger(plunger, cabinet_config.input.plunger);
    NudgeDevice nudge;
    nudge.binding = cabinet_config.input.nudge;
    nudge.binding.enabled = nudge.binding.enabled && cabinet_mode;
    nudge.refresh();
    SourceTexture source;
    StageTexture stage;
    SourceTexture backglass_logo_source;
    StageTexture backglass_logo_stage;
    auto backglass_logo =
        cabinet_rom
        ? devils_crush::render::TitleLogoAnimation::decode(
              *cabinet_rom)
        : std::nullopt;
    auto dmd_font =
        cabinet_rom
        ? devils_crush::cabinet::DmdFont::decode(*cabinet_rom)
        : std::nullopt;
    if (cabinet_mode && !dmd_font) {
        std::cerr
            << "Original DMD font decode failed; disabling DMD.\n";
        destroy_auxiliary_window(dmd);
    }
    // Only the DMD's attract carousel draws it, and it turns without.
    const auto dmd_ending_head =
        cabinet_rom
        ? devils_crush::render::EndingHeadAnimation::decode(*cabinet_rom)
        : std::nullopt;
    if (cabinet_mode && !backglass_logo) {
        std::cerr
            << "Native title-logo decode failed; the backglass will "
               "use its static fallback.\n";
    }
    bool table_view = false;
    bool bonus_view = false;
    bool reported_table_view = false;
    TableMotionPresenter table_motion;
    FrameArtworkPresenter frame_artwork;
    ViewportMotionPresenter viewport_motion;
    bool smoothing_enabled =
        presentation_check || cabinet_config.play.smoothing;
    bool motion_ready = false;
    std::optional<bool> reported_smoothing;
    TablePresentationSample presentation_sample;
    devils_crush::render::PersistentActorSpriteCache
        persistent_actor_sprites;
    devils_crush::render::TablePaletteCache table_palette_cache;
    SeededActorTracker seeded_actors;
    devils_crush::cabinet::DmdPanel dmd_panel;
    SourceTexture dmd_source;
    StageTexture dmd_stage;
    // This routine is a cross-frame presentation controller, outside the
    // replacement registry. Observing entry leaves all CPU execution intact.
    // The session's result bit alone is also set during bonus-stage setup.
    if (cabinet_mode && dmd.renderer != nullptr
        && !runtime.install_execution_hook({
            .routine = {.physical_bank = 0x04U, .logical_pc = 0x456CU},
            .mode = HookMode::observe,
            .callback = [&dmd_panel](const RoutineEntry&) {
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
                 .callback = [&dmd_panel](const RoutineEntry&) {
                     dmd_panel.begin_name_entry();
                 },
             })
            && runtime.install_execution_hook({
                .routine = {.physical_bank = 0x09U, .logical_pc = 0x6698U},
                .mode = HookMode::observe,
                .callback = [&dmd_panel](const RoutineEntry&) {
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
    unsigned ready_screen_frames = 0U;
    if (cabinet_mode
        && !(runtime.install_execution_hook({
                 .routine = {.physical_bank = 0x09U, .logical_pc = 0x6ED6U},
                 .mode = HookMode::observe,
                 .callback = [&ready_screen_frames](const RoutineEntry&) {
                     ready_screen_frames = kReadyScreenFrames;
                 },
             })
            && runtime.install_execution_hook({
                .routine = {.physical_bank = 0x09U, .logical_pc = 0x6F2DU},
                .mode = HookMode::observe,
                .callback = [&ready_screen_frames](const RoutineEntry&) {
                    ready_screen_frames = 0U;
                },
            }))) {
        std::cerr << "Could not observe the player ready screen; "
                     "it will not be shown between turns.\n";
    }
    // The ball-dragging cheat's ROM, for the ball's sprite, and where the
    // playfield's picture last went.
    const auto drag_rom =
        drag_ball ? read_rom_bytes(rom_path) : std::nullopt;
    PresentedPicture playfield_picture;
    std::uint64_t backglass_logo_frame = 0U;
    bool assignment_active = false;
    auto assignment_role = AssignmentRole::playfield;
    const auto save_current_cabinet_config = [&]() {
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
    };

    using Clock = std::chrono::steady_clock;
    const auto frame_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / kEmulationFramesPerSecond));
    auto next_frame_time = Clock::now();
    auto dmd_high_score_epoch = Clock::now();
    bool dmd_carousel_up = true;

    bool running = true;
    bool host_ok = true;
    int emulated_frames = 0;
    bool bonus_stage_return_pending = bonus_stage != 0U;
    int playfield_dump_remaining = 0;
    const bool capture_session = !capture_session_directory.empty();
    std::vector<InteractiveCaptureFrame> captured_inputs;
    NativeGameplayBootstrapShadow gameplay_bootstrap_shadow;

    // The registry is installed last, once every fallible setup step has
    // succeeded, so no early return can leave a hook pointing at a dead
    // deployment. Nothing below returns before the shutdown tail clears them.
    const auto replacement_rom = native_registry != nullptr
        ? read_rom_bytes(rom_path)
        : std::optional<std::vector<std::byte>>{std::vector<std::byte>{}};
    const auto gameplay_shadow_rom = native_gameplay_bootstrap_shadow
        ? read_rom_bytes(rom_path)
        : std::optional<std::vector<std::byte>>{};
    if (native_gameplay_bootstrap_shadow && !gameplay_shadow_rom) {
        std::cerr << "Could not read ROM bytes for native gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    NativeRegistryDeployment registry_deployment;
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
                          [&gameplay_bootstrap_shadow,
                           &gameplay_shadow_rom](
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
    // C510 itself does not write either scheduler gate byte before its C52C
    // BSR.  When their two entry snapshots disagree, retain only the writer
    // of the current slot's `$3000`/`$3080` bytes; this isolates the boundary
    // that changes eligibility without taking ownership of the pass.
    if (host_ok && native_gameplay_bootstrap_shadow
        && !runtime.set_work_ram_write_observer(
            [&gameplay_bootstrap_shadow](const ObservedMemoryWrite& write) {
                gameplay_bootstrap_shadow.observe_main_actor_pass_gate_write(
                    write);
            })) {
        std::cerr << "Could not observe C510 scheduler-gate writes for native "
                     "gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    // C539 is a direct composite call rather than an admitted registry entry,
    // so the registry's entry observer cannot see it. Separate observe-only
    // hooks at its two JSR sites establish whether the short pass was selected
    // at all; the C539 hook then supplies its preamble snapshot. None claims
    // dynamic handlers, the loop, or C606 filing.
    if (host_ok && native_gameplay_bootstrap_shadow
        && !runtime.install_execution_hook({
            .routine = {
                .physical_bank = 0x01U,
                .logical_pc = 0xC539U,
            },
            .mode = HookMode::observe,
            .callback = [&runtime, &gameplay_bootstrap_shadow](
                            const RoutineEntry& entry) {
                gameplay_bootstrap_shadow.observe_short_actor_pass_entry(
                    entry, runtime.export_work_ram());
            },
        })) {
        std::cerr << "Could not observe C539 actor-pass entry for native "
                     "gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    constexpr std::array<RoutineKey, 2U> kShortActorPassCallSites{{
        {.physical_bank = 0x01U, .logical_pc = 0xC1D3U},
        {.physical_bank = 0x00U, .logical_pc = 0xEEC1U},
    }};
    for (const auto routine : kShortActorPassCallSites) {
        if (host_ok && native_gameplay_bootstrap_shadow
            && !runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::observe,
                .callback = [&gameplay_bootstrap_shadow](
                                const RoutineEntry& entry) {
                    gameplay_bootstrap_shadow
                        .observe_short_actor_pass_call_site(entry);
                },
            })) {
            std::cerr << "Could not observe a C539 actor-pass call site for "
                         "native gameplay shadow.\n";
            running = false;
            host_ok = false;
        }
    }
    if (host_ok && native_gameplay_bootstrap_shadow
        && native_registry == nullptr
        && !runtime.install_execution_hook({
            .routine = {
                .physical_bank = 0x01U,
                .logical_pc = 0xCD74U,
            },
            .mode = HookMode::observe,
            .callback = [&runtime,
                         &gameplay_bootstrap_shadow,
                         &gameplay_shadow_rom](const RoutineEntry& entry) {
                gameplay_bootstrap_shadow.observe_playfield_collision_source(
                    entry,
                    runtime.export_work_ram(),
                    *gameplay_shadow_rom);
            },
        })) {
        std::cerr << "Could not observe the playfield collision probe for "
                     "native gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    if (host_ok && native_gameplay_bootstrap_shadow
        && native_registry == nullptr) {
        constexpr std::array<RoutineKey, 14> kGameplayShadowEntries{{
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCB2AU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xDC95U,
            },
            {
                .physical_bank = 0x00U,
                .logical_pc = 0xF1B4U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xDD09U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCF33U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCDF8U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC52CU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC606U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC510U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCBABU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCC1AU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCC98U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCBC2U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCBCFU,
            },
        }};
        for (const auto routine : kGameplayShadowEntries) {
            if (runtime.install_execution_hook({
                    .routine = routine,
                    .mode = HookMode::observe,
                    .callback = [&runtime,
                                 &gameplay_bootstrap_shadow,
                                 &gameplay_shadow_rom](
                                    const RoutineEntry& entry) {
                        gameplay_bootstrap_shadow.observe_motion_entry(
                            entry,
                            runtime.export_work_ram(),
                            *gameplay_shadow_rom);
                    },
                })) {
                continue;
            }
            std::cerr << "Could not observe " << format_routine_key(routine)
                      << " for native gameplay shadow.\n";
            running = false;
            host_ok = false;
            break;
        }
    }
    if (host_ok && native_gameplay_bootstrap_shadow) {
        constexpr std::array<RoutineKey, 7> kGameplayRoutineCompletions{{
            {
                .physical_bank = 0x00U,
                .logical_pc = 0xF241U,
            },
            {
                .physical_bank = 0x00U,
                .logical_pc = 0xF1B4U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xDD09U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCD74U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC52CU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC606U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC510U,
            },
        }};
        for (const auto routine : kGameplayRoutineCompletions) {
            if (runtime.install_routine_call_observer({
                    .routine = routine,
                    .callback = [&runtime, &gameplay_bootstrap_shadow](
                                    const RoutineCall& call) {
                        gameplay_bootstrap_shadow.observe_gameplay_completion(
                            call, runtime.export_work_ram());
                    },
                })) {
                continue;
            }
            std::cerr << "Could not observe completion of "
                      << format_routine_key(routine)
                      << " for native gameplay shadow.\n";
            running = false;
            host_ok = false;
            break;
        }
    }
    // E9B6 returns through the core-owned JSR at E976.  Its routine-call
    // observer gives the shadow the precise pre-RTS register state; E994 is
    // the only caller-tail boundary not already owned by the registry.
    if (host_ok && native_gameplay_bootstrap_shadow
        && !runtime.install_routine_call_observer({
            .routine = {
                .physical_bank = 0x00U,
                .logical_pc = 0xE9B6U,
            },
            .callback = [&runtime, &gameplay_bootstrap_shadow](
                            const RoutineCall& call) {
                gameplay_bootstrap_shadow.observe_palette_fade_block_completion(
                    call, runtime.export_work_ram());
            },
        })) {
        std::cerr << "Could not observe E9B6 palette-fade completion for "
                     "native gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    if (host_ok && native_gameplay_bootstrap_shadow
        && !runtime.install_execution_hook({
            .routine = {
                .physical_bank = 0x00U,
                .logical_pc = 0xE994U,
            },
            .mode = HookMode::observe,
            .callback = [&runtime, &gameplay_bootstrap_shadow](
                            const RoutineEntry& entry) {
                gameplay_bootstrap_shadow.observe_palette_fade_terminal_entry(
                    entry, runtime.export_work_ram());
            },
        })) {
        std::cerr << "Could not observe E994 palette-fade tail for native "
                     "gameplay shadow.\n";
        running = false;
        host_ok = false;
    }
    // After the registry, so a deployed replacement owning either address
    // would be reported as the collision it is rather than silently winning.
    if (host_ok
        && !install_shooter_flipper_gate(
            runtime, cabinet_config.input.flippers)) {
        std::cerr << "Could not free the flippers during the launch.\n";
        running = false;
        host_ok = false;
    }
    devils_crush::cabinet::MusicTransition music_transition;
    if (host_ok && (!runtime.install_execution_hook({
            .routine = {1U, 0xC204U}, .mode = HookMode::observe,
            .callback = [&runtime, &music_transition](const RoutineEntry&) {
                music_transition.table_return(runtime.export_work_ram());
            },
        }) || !runtime.install_execution_hook({
            .routine = {3U, 0x7CF6U}, .mode = HookMode::replace,
            .replacement_callback = [&music_transition](const RoutineEntry& entry,
                const std::span<const std::byte> ram) {
                return music_transition.finish_star(entry, ram);
            },
        }))) {
        std::cerr << "Could not install the bonus music transition.\n";
        running = false;
        host_ok = false;
    }
    HighScoreAutosave high_scores = open_high_score_autosave(kHighScorePath);
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
    while (running && host_ok) {
        if (presentation_check && ((emulated_frames >= 1200 && emulated_frames < 1560)
                ? smoothing_enabled : !smoothing_enabled)) {
            presentation_sample.report(smoothing_enabled);
            presentation_sample = {};
            smoothing_enabled = !smoothing_enabled;
            table_motion.reset();
            frame_artwork.reset();
            viewport_motion.reset();
            motion_ready = false;
        }
        claim_startup_focus(window, focus_claim);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.windowID == SDL_GetWindowID(window)
                && event.key.scancode == SDL_SCANCODE_R) {
                auto& rotation = cabinet_config.playfield_rotation_degrees;
                rotation = (rotation + ((event.key.mod & SDL_KMOD_SHIFT) ? 270 : 90)) % 360;
                std::cout << "Playfield rotation: " << rotation << " degrees\n";
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F6) {
                presentation_sample.report(smoothing_enabled);
                presentation_sample = {};
                smoothing_enabled = !smoothing_enabled;
                cabinet_config.play.smoothing = smoothing_enabled;
                table_motion.reset();
                frame_artwork.reset();
                viewport_motion.reset();
                motion_ready = false;
                std::cout << "Playfield smoothing: "
                          << (smoothing_enabled ? "on" : "off")
                          << " (motion and artwork on high-refresh displays)\n";
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                nudge.model.rearm();
            }
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                const auto main_window_id = SDL_GetWindowID(window);
                if (event.window.windowID == main_window_id) {
                    running = false;
                } else if (
                    dmd.window != nullptr
                    && event.window.windowID
                        == SDL_GetWindowID(dmd.window)) {
                    cabinet_config.dmd = capture_window_placement(
                        dmd.window,
                        host_displays,
                        cabinet_config.dmd);
                    destroy_stage(dmd_stage);
                    destroy_source(dmd_source);
                    destroy_auxiliary_window(dmd);
                } else if (
                    backglass.window != nullptr
                    && event.window.windowID
                        == SDL_GetWindowID(backglass.window)) {
                    cabinet_config.backglass = capture_window_placement(
                        backglass.window,
                        host_displays,
                        cabinet_config.backglass);
                    destroy_stage(backglass_logo_stage);
                    destroy_source(backglass_logo_source);
                    destroy_auxiliary_window(backglass);
                }
            } else if (
                cabinet_mode && event.type == SDL_EVENT_KEY_DOWN
                && !event.key.repeat && event.key.key == SDLK_F8) {
                // Capture a short burst rather than one frame: a seam that
                // tracks the camera only identifies itself across frames.
                playfield_dump_remaining = 4;
                std::cout << "Cabinet playfield capture armed.\n";
            } else if (
                cabinet_mode && event.type == SDL_EVENT_KEY_DOWN
                && !event.key.repeat && event.key.key == SDLK_F12) {
                assignment_active = !assignment_active;
                if (assignment_active) {
                    host_displays = enumerate_host_displays();
                    cabinet_config.playfield =
                        capture_window_placement(
                            window,
                            host_displays,
                            cabinet_config.playfield);
                    cabinet_config.dmd = capture_window_placement(
                        dmd.window,
                        host_displays,
                        cabinet_config.dmd);
                    cabinet_config.backglass =
                        capture_window_placement(
                            backglass.window,
                            host_displays,
                            cabinet_config.backglass);
                }
                set_assignment_titles(
                    window,
                    window_title,
                    dmd,
                    backglass,
                    assignment_active,
                    assignment_role);
                if (!assignment_active) {
                    save_current_cabinet_config();
                }
                next_frame_time = Clock::now();
            } else if (
                assignment_active
                && event.type == SDL_EVENT_KEY_DOWN
                && !event.key.repeat) {
                if (event.key.key == SDLK_RETURN
                    || event.key.key == SDLK_ESCAPE) {
                    assignment_active = false;
                    save_current_cabinet_config();
                } else if (event.key.key == SDLK_1) {
                    assignment_role = AssignmentRole::playfield;
                } else if (event.key.key == SDLK_2) {
                    assignment_role = AssignmentRole::dmd;
                } else if (event.key.key == SDLK_3) {
                    assignment_role = AssignmentRole::backglass;
                } else if (
                    event.key.key == SDLK_LEFT
                    || event.key.key == SDLK_RIGHT) {
                    host_ok = cycle_assignment_display(
                        assignment_role,
                        event.key.key == SDLK_RIGHT ? 1 : -1,
                        window,
                        dmd,
                        backglass,
                        cabinet_config,
                        host_displays);
                } else if (event.key.key == SDLK_F) {
                    SDL_Window* const selected_window =
                        assignment_window(
                            assignment_role,
                            window,
                            dmd,
                            backglass);
                    if (selected_window != nullptr) {
                        auto& placement = assignment_placement(
                            assignment_role,
                            cabinet_config);
                        placement.fullscreen =
                            (SDL_GetWindowFlags(selected_window)
                             & SDL_WINDOW_FULLSCREEN)
                            == 0U;
                        if (!SDL_SetWindowFullscreen(
                                selected_window,
                                placement.fullscreen)) {
                            host_ok = report_sdl_error(
                                "Changing an assigned window's "
                                "fullscreen state failed");
                        }
                    }
                }
                set_assignment_titles(
                    window,
                    window_title,
                    dmd,
                    backglass,
                    assignment_active,
                    assignment_role);
                next_frame_time = Clock::now();
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_ESCAPE) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F5) {
                // A state is not saved with the ball in the cheat's hand.
                if (drag_rom) {
                    release_dragged_ball(runtime);
                }
                (void)save_quick_state(runtime, kQuickStatePath);
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F9) {
                if (capture_session) {
                    std::cerr << "Quick-state restore is disabled during "
                                 "interactive capture.\n";
                } else if (load_quick_state(runtime, kQuickStatePath)) {
                    table_motion.reset();
                    frame_artwork.reset();
                    viewport_motion.reset();
                    motion_ready = false;
                    nudge.model.rearm();
                    if (audio.stream != nullptr
                        && !SDL_ClearAudioStream(audio.stream)) {
                        report_sdl_error("SDL_ClearAudioStream failed");
                    }
                    next_frame_time = Clock::now();
                }
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F10) {
                if (load_gamepad_mappings(kGamepadMappingsPath)) {
                    if (gamepad != nullptr) {
                        SDL_CloseGamepad(gamepad);
                    }
                    gamepad = open_first_gamepad();
                }
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F11) {
                const bool fullscreen =
                    (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                if (!SDL_SetWindowFullscreen(window, !fullscreen)) {
                    report_sdl_error("SDL_SetWindowFullscreen failed");
                }
            } else if (
                event.type == SDL_EVENT_GAMEPAD_ADDED
                || event.type == SDL_EVENT_GAMEPAD_REMOVED
                || event.type == SDL_EVENT_GAMEPAD_REMAPPED) {
                handle_gamepad_device_event(event.gdevice, gamepad);
            } else if (
                event.type == SDL_EVENT_JOYSTICK_ADDED
                || event.type == SDL_EVENT_JOYSTICK_REMOVED) {
                handle_joystick_device_event(
                    event.jdevice,
                    plunger,
                    cabinet_config.input.plunger);
                nudge.refresh();
            } else if (
                event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED
                || event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) {
                handle_audio_device_event(event.adevice, audio);
            } else if (event.type == SDL_EVENT_RENDER_TARGETS_RESET) {
                if (event.render.windowID == SDL_GetWindowID(window)) {
                    destroy_stage(stage);
                } else if (
                    backglass.window != nullptr
                    && event.render.windowID
                        == SDL_GetWindowID(backglass.window)) {
                    destroy_stage(backglass_logo_stage);
                }
            } else if (event.type == SDL_EVENT_RENDER_DEVICE_RESET) {
                if (event.render.windowID == SDL_GetWindowID(window)) {
                    destroy_stage(stage);
                    destroy_source(source);
                    table_motion.reset();
                    frame_artwork.reset();
                    viewport_motion.reset();
                    motion_ready = false;
                    next_frame_time = Clock::now();
                } else if (
                    backglass.window != nullptr
                    && event.render.windowID
                        == SDL_GetWindowID(backglass.window)) {
                    destroy_stage(backglass_logo_stage);
                    destroy_source(backglass_logo_source);
                }
            }
        }

        const auto* focused_window = SDL_GetKeyboardFocus();
        // These are desktop windows (including borderless cabinet placement).
        // GetCurrentDisplayMode lazily enumerates every fullscreen mode before
        // returning, delaying the first present on some Windows drivers. The
        // cached desktop mode supplies the refresh rate without that scan and
        // still follows display changes processed by SDL_PollEvent above.
        const auto* refresh_mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
        const bool smooth_table = smoothing_enabled && vsync_enabled
            && layers == CabinetLayers::all && !assignment_active
            && refresh_mode != nullptr
            && refresh_mode->refresh_rate > kEmulationFramesPerSecond * 1.05;
        if (!reported_smoothing || *reported_smoothing != smooth_table) {
            reported_smoothing = smooth_table;
            std::cout << "Playfield smoothing: " << (smooth_table ? "active" : "inactive")
                      << "; display " << (refresh_mode ? refresh_mode->refresh_rate : 0.0F)
                      << " Hz (F6 toggles; requires high refresh and VSync).\n";
        }
        if (!smooth_table) {
            table_motion.reset();
            frame_artwork.reset();
            viewport_motion.reset();
            motion_ready = false;
        }
        const bool nudge_focused = focused_window != nullptr
            && (focused_window == window || focused_window == dmd.window
                || focused_window == backglass.window);
        if (!nudge_focused || assignment_active) nudge.model.rearm();
        auto now = Clock::now();
        int catch_up_frames = 0;
        constexpr int kMaximumCatchUpFrames = 4;
        while (running && host_ok && !assignment_active
               && now >= next_frame_time
               && catch_up_frames < kMaximumCatchUpFrames
               && (frame_limit <= 0 || emulated_frames < frame_limit)) {
            if (cabinet_mode) {
                keep_fixed_actors_simulating(runtime);
            }
            const bool ball_in_launcher = apply_flipper_inputs(
                runtime, cabinet_config.input.flippers);
            const auto rod =
                read_plunger(plunger, cabinet_config.input.plunger);
            // Whether the shooter took the write is the gate, rather than
            // whether a ball is in play. `$53` bit 3 goes up the moment a ball
            // is served and the shooter does not catch it for another two
            // seconds -- measured, 942 against 1,064 on the census route, with
            // the charge script not installed until 1,084 -- so a rod pulled
            // the instant the ball appears has nothing to move yet. Asking
            // `apply_plunger_draw` is asking the only question that matters:
            // it writes when the Devil Shooter is holding the ball and
            // dispatching to its charge script, and refuses otherwise.
            const bool rod_engaged = ball_in_launcher && rod.drawn
                && apply_plunger_draw(runtime, rod);
            auto input = read_input(
                gamepad,
                key_bindings,
                ball_in_launcher,
                cabinet_config.input.flippers,
                rod_engaged && !rod.released);
            if (presentation_check)
                input = state_census_input_for_frame(emulated_frames + 1);
            if (cabinet_mode && nudge.read(nudge_focused
                    && devils_crush::input::nudge_play_active(runtime.export_work_ram()))) {
                input.player_one |= static_cast<std::uint16_t>(Button::two);
            }
            // The cartridge reads the pad of whoever's turn it is: every
            // reader indexes `$2013,Y` and `$2018,Y` with the active player in
            // `$2251`. A cabinet has one set of buttons and the players take
            // turns at it, so it answers as both pads.
            if (!presentation_check) {
                input.player_two = input.player_one;
            }
            if (drag_rom) {
                float mouse_x = 0.0F;
                float mouse_y = 0.0F;
                const auto point = SDL_GetMouseFocus() == window
                    && (SDL_GetMouseState(&mouse_x, &mouse_y)
                        & SDL_BUTTON_LMASK) != 0U
                    ? picture_point(
                        renderer, playfield_picture, mouse_x, mouse_y)
                    : std::nullopt;
                if (!point
                    || !drag_ball_to_picture_point(
                        runtime, *drag_rom, *point, table_view)) {
                    release_dragged_ball(runtime);
                }
            }
            if (capture_session) {
                captured_inputs.push_back({
                    .input = input,
                    .plunger = rod,
                    .plunger_engaged = rod_engaged,
                });
            }
            const auto frame = runtime.run_frame(input);
            if (bonus_stage != 0U
                && !service_synthetic_bonus_stage_return(
                    runtime, bonus_stage_return_pending)) {
                host_ok = false;
                break;
            }
            // Not in a session being captured, which is replayed as the
            // cartridge alone plays it.
            if (cabinet_mode && !capture_session && !presentation_check) {
                face_waiting_dragon_heads(runtime);
            }
            if (native_gameplay_bootstrap_shadow) {
                gameplay_bootstrap_shadow.observe(
                    runtime.export_work_ram(), input);
            }
            if (capture_session) {
                const auto work_ram = runtime.export_work_ram();
                if (work_ram.size() != runtime.work_ram_size()) {
                    std::cerr << "Could not fingerprint interactive capture "
                                 "work RAM.\n";
                    host_ok = false;
                    break;
                }
                captured_inputs.back().work_ram_sha256 = sha256(work_ram);
            }
            ++emulated_frames;
            poll_high_scores(runtime, high_scores, emulated_frames);
            if (!cabinet_mode) {
                host_ok =
                    consume_emulated_frame(renderer, frame, source, audio);
                if (host_ok && smooth_table) {
                    const auto state = devils_crush::cabinet::decode_cabinet_state(runtime.export_work_ram());
                    host_ok = frame_artwork.update(renderer, frame.framebuffer,
                        state ? state->screen_selector + (state->bonus_stage ? 256 : 0) : 0);
                    update_bonus_room_motion(renderer, runtime, state, frame.framebuffer, viewport_motion);
                }
            } else if (
                frame.frame_number == 0U
                || frame.framebuffer.pixels.empty()) {
                std::cerr
                    << "Beetle PCE did not produce a framebuffer.\n";
                host_ok = false;
            } else {
                const auto work_ram = runtime.export_work_ram();
                auto cabinet_state =
                    devils_crush::cabinet::decode_cabinet_state(
                        work_ram);
                if (cabinet_state) {
                    dmd_panel.observe(work_ram);
                }
                if (ready_screen_frames != 0U) {
                    --ready_screen_frames;
                    if (cabinet_state) {
                        cabinet_state->playfield =
                            devils_crush::cabinet::PlayfieldView::
                                emulated_framebuffer;
                    }
                }
                if (backglass_logo
                    && backglass.renderer != nullptr) {
                    const auto logo =
                        backglass_logo->render(
                            backglass_logo_frame++);
                    if (!logo
                        || !upload_image(
                            backglass.renderer,
                            *logo,
                            backglass_logo_source)) {
                        std::cerr
                            << "Native backglass logo renderer "
                               "failed; using the static fallback.\n";
                        backglass_logo.reset();
                        destroy_stage(backglass_logo_stage);
                        destroy_source(backglass_logo_source);
                    }
                }

                const bool hold_bonus_entry = table_view && cabinet_state
                    && cabinet_state->bonus_entry_pending();
                const bool hold_playfield = source.texture != nullptr
                    && (hold_bonus_entry || (dmd.renderer != nullptr && dmd_font
                        && dmd_panel.holds_playfield()));
                if (hold_playfield) {
                    table_motion.reset();
                    frame_artwork.reset();
                    viewport_motion.reset();
                    motion_ready = false;
                }
                if (!hold_playfield) {
                    bonus_view = cabinet_state && cabinet_state->bonus_stage;
                    table_view = false;
                    bool source_uploaded = false;
                    const bool overlay_stats_due =
                        overlay_stats
                        && frame.frame_number % 60U == 0U;
                    if (cabinet_state
                        && cabinet_state->playfield
                            == devils_crush::cabinet::PlayfieldView::
                                table_background) {
                        const auto video = runtime.export_video_memory();
                        devils_crush::render::TableMotionFrame motion_frame;
                        const auto table =
                            video
                            ? decode_cabinet_table(
                                  *cabinet_rom,
                                  work_ram,
                                  *video,
                                  persistent_actor_sprites,
                                  table_palette_cache,
                                  layers,
                                  overlay_stats_due,
                                  overlay_stats ? &seeded_actors : nullptr,
                                  nullptr, smooth_table ? &motion_frame : nullptr)
                            : std::nullopt;
                        if (table && playfield_dump_remaining > 0) {
                            --playfield_dump_remaining;
                            std::ostringstream name;
                            name << "playfield-" << std::setw(6)
                                 << std::setfill('0') << frame.frame_number
                                 << ".bmp";
                            const auto dump_path =
                                std::filesystem::current_path() / name.str();
                            if (save_playfield_png(table->image, dump_path)) {
                                std::cout << "Captured " << dump_path.string()
                                          << '\n';
                            }
                        }
                        if (table) {
                            frame_artwork.reset();
                            viewport_motion.reset();
                            if (smooth_table) {
                                motion_ready = table_motion.update(renderer, std::move(motion_frame));
                                if (!motion_ready) {
                                    smoothing_enabled = false;
                                    table_motion.reset();
                                    std::cerr << "Table smoothing unavailable: " << SDL_GetError() << '\n';
                                }
                            }
                            host_ok =
                                upload_image(
                                    renderer,
                                    table->image,
                                    source);
                            source_uploaded = host_ok;
                            table_view = host_ok;
                        }
                    }
                    if (host_ok && !source_uploaded) {
                        table_motion.reset();
                        motion_ready = false;
                        persistent_actor_sprites.reset();
                        table_palette_cache.reset();
                        host_ok = upload_framebuffer(
                            renderer,
                            frame.framebuffer,
                            source);
                        if (host_ok && smooth_table) {
                            const int scene = cabinet_state
                                ? cabinet_state->screen_selector + (cabinet_state->bonus_stage ? 256 : 0)
                                : (work_ram.size() > 0x54 ? std::to_integer<int>(work_ram[0x54]) : 0);
                            host_ok = frame_artwork.update(renderer, frame.framebuffer, scene);
                            update_bonus_room_motion(renderer, runtime, cabinet_state,
                                frame.framebuffer, viewport_motion);
                        }
                    }
                } // Retain it during bonus entry and DMD-owned result screens.
                // The stitched table and the emulated framebuffer behave
                // very differently — the framebuffer scrolls with the camera
                // and hardware-clips its sprites — so say which one is on
                // screen whenever that changes.
                if (table_view != reported_table_view) {
                    reported_table_view = table_view;
                    std::cout << "Cabinet playfield: "
                              << (table_view
                                      ? "stitched table"
                                      : "emulated framebuffer")
                              << " (frame " << frame.frame_number;
                    if (!table_view && cabinet_state) {
                        std::cout << "; balls="
                                  << static_cast<unsigned>(
                                         cabinet_state->balls_in_session)
                                  << ", bonus="
                                  << (cabinet_state->bonus_stage ? 1 : 0);
                    }
                    std::cout << ")\n";
                }
                if (host_ok) {
                    host_ok = queue_audio(frame.audio, audio);
                }
            }
            next_frame_time += frame_period;
            ++catch_up_frames;
            now = Clock::now();
        }
        if (catch_up_frames == kMaximumCatchUpFrames
            && now >= next_frame_time) {
            next_frame_time = now + frame_period;
        }
        if (assignment_active) {
            next_frame_time = now + frame_period;
        }

        if (running && host_ok) {
            if (assignment_active) {
                host_ok = present_assignment(
                    renderer,
                    AssignmentRole::playfield,
                    assignment_role == AssignmentRole::playfield);
            } else {
                const auto aspect_numerator =
                    table_view
                    ? devils_crush::cabinet::kTableAspectNumerator
                    : kDisplayAspectNumerator;
                const auto aspect_denominator =
                    table_view
                    ? devils_crush::cabinet::kTableAspectDenominator
                    : kDisplayAspectDenominator;
                const bool interpolate = smooth_table && table_view && motion_ready;
                const bool room_pieces = smooth_table && !table_view && viewport_motion.ready;
                const bool blend_frame = smooth_table && !table_view && !room_pieces
                    && frame_artwork.current.texture;
                const auto motion_alpha = std::clamp(static_cast<float>(
                    std::chrono::duration<double>(Clock::now() - next_frame_time + frame_period).count()
                    / std::chrono::duration<double>(frame_period).count()), 0.0F, 1.0F);
                host_ok = present_frame(
                    renderer,
                    source,
                    stage,
                    aspect_numerator,
                    aspect_denominator,
                    cabinet_mode ? kPureBlack : kDesktopFrameColor,
                    0.0F,
                    cabinet_mode && bonus_view && !table_view,
                    interpolate ? &table_motion
                        : room_pieces ? &viewport_motion.pieces : nullptr,
                    motion_alpha,
                    blend_frame ? &frame_artwork : nullptr,
                    cabinet_config.playfield_rotation_degrees,
                    &playfield_picture);
                if (host_ok && (table_view || room_pieces || blend_frame)) {
                    presentation_sample.observe(emulated_frames,
                        (interpolate || room_pieces || blend_frame) && motion_alpha < 1.0F,
                        Clock::now());
                    if (Clock::now() - presentation_sample.start >= std::chrono::seconds(5))
                        presentation_sample.report(smoothing_enabled);
                } else {
                    presentation_sample = {};
                }
            }
        }
        // A turn of the carousel begins when the carousel comes up, so what
        // follows a session is the ending's head and then the pages.
        const bool dmd_carousel =
            dmd_panel.attract() && !dmd_panel.name_entry();
        if (dmd_carousel && !dmd_carousel_up) {
            dmd_high_score_epoch = Clock::now();
        }
        dmd_carousel_up = dmd_carousel;
        const auto dmd_high_score_elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                Clock::now() - dmd_high_score_epoch)
                .count();
        if (running && dmd.renderer != nullptr && dmd_font
            && !(assignment_active
                    ? present_assignment(
                        dmd.renderer, AssignmentRole::dmd,
                        assignment_role == AssignmentRole::dmd)
                    : present_dmd_panel(dmd,
                        dmd_panel.compose(*dmd_font,
                            high_scores.table_seen ? &high_scores.last : nullptr,
                            static_cast<std::uint64_t>(dmd_high_score_elapsed),
                            dmd_ending_head ? &*dmd_ending_head : nullptr),
                        dmd_source, dmd_stage))) {
            destroy_stage(dmd_stage);
            destroy_source(dmd_source);
            std::cerr << "Cabinet DMD renderer lost; disabling DMD.\n";
            destroy_auxiliary_window(dmd);
        }
        if (running && backglass.renderer != nullptr
            && !(assignment_active
                    ? present_assignment(
                        backglass.renderer,
                        AssignmentRole::backglass,
                        assignment_role == AssignmentRole::backglass)
                    : backglass_logo_source.texture != nullptr
                        ? present_frame(
                            backglass.renderer,
                            backglass_logo_source,
                            backglass_logo_stage,
                            static_cast<std::uint32_t>(
                                devils_crush::render::
                                    kTitleLogoWidth * 5),
                            static_cast<std::uint32_t>(
                                devils_crush::render::
                                    kTitleLogoHeight * 4),
                            kPureBlack,
                            kBackglassMarginFraction)
                        : present_black(backglass))) {
            std::cerr
                << "Cabinet backglass renderer lost; disabling "
                   "backglass.\n";
            destroy_stage(backglass_logo_stage);
            destroy_source(backglass_logo_source);
            destroy_auxiliary_window(backglass);
        }
        if (frame_limit > 0 && emulated_frames >= frame_limit) {
            running = false;
        }
        if (!vsync_enabled) {
            SDL_Delay(1);
        }
    }

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
