// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_host_session.hpp"

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

void HostSession::run()
{
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
        handle_events();
        const auto* focused_window = SDL_GetKeyboardFocus();
        // These are desktop windows (including borderless cabinet placement).
        // GetCurrentDisplayMode lazily enumerates every fullscreen mode before
        // returning, delaying the first present on some Windows drivers. The
        // cached desktop mode supplies the refresh rate without that scan and
        // still follows display changes processed by SDL_PollEvent above.
        const auto* refresh_mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
        const bool smooth_table = smoothing_enabled && vsync_enabled
            && layers == CabinetLayers::all
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
        if (!nudge_focused) nudge.model.rearm();
        run_due_frames(smooth_table, nudge_focused);
        present_playfield(smooth_table);
        present_cabinet_displays();
        if (frame_limit > 0 && emulated_frames >= frame_limit) {
            running = false;
        }
        if (!vsync_enabled) {
            SDL_Delay(1);
        }
    }
}

SDL_Window* HostSession::selected_cabinet_window() const noexcept
{
    SDL_Window* const selected = selected_window == 1 ? dmd.window
        : selected_window == 2                       ? backglass.window
                                                     : window;
    return selected != nullptr ? selected : window;
}

void HostSession::handle_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
            && event.key.windowID == SDL_GetWindowID(window)
            && event.key.scancode == SDL_SCANCODE_R) {
            auto& rotation = cabinet_config.playfield_rotation_degrees;
            rotation = (rotation + ((event.key.mod & SDL_KMOD_SHIFT) ? 270 : 90)) % 360;
            std::cout << "Playfield rotation: " << rotation << " degrees\n";
        }
        const bool key_press =
            event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat;
        const bool shift =
            key_press && (event.key.mod & SDL_KMOD_SHIFT) != 0U;
        if (key_press
            && key_is_bound(key_bindings.smoothing, event.key.scancode)) {
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
            cabinet_mode && key_press
            && key_is_bound(key_bindings.next_window, event.key.scancode)) {
            // Windows that are closed or never opened are skipped; the
            // playfield is always there.
            SDL_Window* const windows[]{window, dmd.window, backglass.window};
            static constexpr const char* names[]{
                "Playfield", "DMD", "Backglass"};
            for (int step = 0; step < 3; ++step) {
                selected_window = (selected_window + (shift ? 2 : 1)) % 3;
                if (windows[selected_window] != nullptr) {
                    break;
                }
            }
            std::cout << "Selected cabinet window: "
                      << names[selected_window] << '\n';
        } else if (
            cabinet_mode && key_press
            && key_is_bound(key_bindings.next_monitor, event.key.scancode)) {
            host_displays = enumerate_host_displays();
            SDL_Window* const windows[]{window, dmd.window, backglass.window};
            (void)move_to_next_display(
                selected_cabinet_window(), shift ? -1 : 1, host_displays,
                windows);
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
            key_press
            && key_is_bound(key_bindings.fullscreen, event.key.scancode)) {
            SDL_Window* const focused =
                cabinet_mode ? selected_cabinet_window() : window;
            const bool fullscreen =
                (SDL_GetWindowFlags(focused) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_Window* const windows[]{window, dmd.window, backglass.window};
            if (!fullscreen
                && fullscreen_beside(
                    focused, SDL_GetDisplayForWindow(focused), windows)) {
                std::cout << "Another cabinet window is fullscreen on that "
                             "monitor; take it out of fullscreen first.\n";
            } else if (!SDL_SetWindowFullscreen(focused, !fullscreen)) {
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
            if (event.type == SDL_EVENT_JOYSTICK_REMOVED) {
                joystick_inputs.remove(event.jdevice.which);
            }
            joystick_inputs.refresh();
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
}

void HostSession::run_due_frames(
    const bool smooth_table,
    const bool nudge_focused)
{
    auto now = Clock::now();
    int catch_up_frames = 0;
    constexpr int kMaximumCatchUpFrames = 4;
    while (running && host_ok
           && now >= next_frame_time
           && catch_up_frames < kMaximumCatchUpFrames
           && (frame_limit <= 0 || emulated_frames < frame_limit)) {
        if (cabinet_mode) {
            keep_fixed_actors_simulating(runtime);
        }
        const bool ball_in_launcher = apply_flipper_inputs(
            runtime, devils_crush::cabinet::flipper_inputs());
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
            joystick_inputs,
            key_bindings,
            ball_in_launcher,
            devils_crush::cabinet::flipper_inputs(),
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
}

void HostSession::present_playfield(const bool smooth_table)
{
    if (running && host_ok) {
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

void HostSession::present_cabinet_displays()
{
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
        && !present_dmd_panel(dmd,
                    dmd_panel.compose(*dmd_font,
                        high_scores.table_seen ? &high_scores.last : nullptr,
                        static_cast<std::uint64_t>(dmd_high_score_elapsed),
                        dmd_ending_head ? &*dmd_ending_head : nullptr),
                    dmd_source, dmd_stage)) {
        destroy_stage(dmd_stage);
        destroy_source(dmd_source);
        std::cerr << "Cabinet DMD renderer lost; disabling DMD.\n";
        destroy_auxiliary_window(dmd);
    }
    if (running && backglass.renderer != nullptr
        && !(backglass_logo_source.texture != nullptr
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
}

} // namespace devils_crush::app::host
