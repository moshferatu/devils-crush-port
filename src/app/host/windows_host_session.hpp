// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "cabinet_table.hpp"
#include "command_line.hpp"
#include "gameplay_bootstrap_shadow.hpp"
#include "high_score_autosave.hpp"
#include "host_audio.hpp"
#include "host_input.hpp"
#include "host_present.hpp"
#include "host_textures.hpp"
#include "host_windows.hpp"
#include "interactive_session.hpp"
#include "native_registry_install.hpp"
#include "table_motion.hpp"

namespace devils_crush::app::host {

constexpr double kEmulationFramesPerSecond = 59.8261;
inline const std::filesystem::path kGamepadMappingsPath =
    std::filesystem::path(devils_crush::core::kGamepadMappingsFileName);

// One run of the windowed host: what it was asked for, and everything it
// keeps from opening its windows to closing them. `run_windows_host` takes it
// through its phases in the order they are declared.
struct HostSession {
    using Clock = std::chrono::steady_clock;

    // What the run was asked for.
    PceRuntime& runtime;
    const std::filesystem::path& rom_path;
    const bool cabinet_mode;
    const std::uint8_t bonus_stage;
    const int frame_limit;
    const CommandLine* const native_registry;
    const CabinetLayers layers;
    const bool overlay_stats;
    const std::filesystem::path& capture_session_directory;
    const bool native_gameplay_bootstrap_shadow;
    const double nudge_multiplier;
    const bool presentation_check;
    const bool drag_ball;

    // Windows, devices and configuration.
    AudioOutput audio;
    std::optional<std::vector<std::byte>> cabinet_rom;
    devils_crush::cabinet::DisplayConfig cabinet_config;
    bool persist_cabinet_config = false;
    ResolvedKeyBindings key_bindings;
    HostDisplays host_displays;
    std::string window_title;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    AuxiliaryWindow dmd;
    AuxiliaryWindow backglass;
    bool vsync_enabled = false;
    StartupFocusClaim focus_claim;
    SDL_Gamepad* gamepad = nullptr;
    PlungerDevice plunger;
    NudgeDevice nudge;

    // What is drawn, and from what.
    SourceTexture source;
    StageTexture stage;
    SourceTexture backglass_logo_source;
    StageTexture backglass_logo_stage;
    std::optional<devils_crush::render::TitleLogoAnimation> backglass_logo;
    std::optional<devils_crush::cabinet::DmdFont> dmd_font;
    std::optional<devils_crush::render::EndingHeadAnimation> dmd_ending_head;
    bool table_view = false;
    bool bonus_view = false;
    bool reported_table_view = false;
    TableMotionPresenter table_motion;
    FrameArtworkPresenter frame_artwork;
    ViewportMotionPresenter viewport_motion;
    bool smoothing_enabled = false;
    // The cabinet window the fullscreen and next-monitor keys act on: 0 the
    // playfield, 1 the DMD, 2 the backglass. Nothing on screen shows it.
    int selected_window = 0;
    bool motion_ready = false;
    std::optional<bool> reported_smoothing;
    TablePresentationSample presentation_sample;
    devils_crush::render::PersistentActorSpriteCache persistent_actor_sprites;
    devils_crush::render::TablePaletteCache table_palette_cache;
    SeededActorTracker seeded_actors;
    devils_crush::cabinet::DmdPanel dmd_panel;
    SourceTexture dmd_source;
    StageTexture dmd_stage;
    unsigned ready_screen_frames = 0U;
    std::optional<std::vector<std::byte>> drag_rom;
    PresentedPicture playfield_picture;
    std::uint64_t backglass_logo_frame = 0U;

    // Pacing.
    Clock::duration frame_period{};
    Clock::time_point next_frame_time;
    Clock::time_point dmd_high_score_epoch;
    bool dmd_carousel_up = true;

    // The run itself, and what it deploys on the game.
    bool running = true;
    bool host_ok = true;
    int emulated_frames = 0;
    bool bonus_stage_return_pending = false;
    int playfield_dump_remaining = 0;
    bool capture_session = false;
    std::vector<InteractiveCaptureFrame> captured_inputs;
    NativeGameplayBootstrapShadow gameplay_bootstrap_shadow;
    std::optional<std::vector<std::byte>> replacement_rom;
    std::optional<std::vector<std::byte>> gameplay_shadow_rom;
    NativeRegistryDeployment registry_deployment;
    devils_crush::cabinet::MusicTransition music_transition;
    HighScoreAutosave high_scores;

    // SDL, the audio device and the windows. False, with everything it
    // opened closed again, when the playfield's cannot be had.
    [[nodiscard]] bool open_windows();
    // Input devices, artwork, and the hooks the cabinet's displays watch the
    // game through.
    void prepare();
    // The registry, the shadow's observers and the host's own changes to
    // play. A failure clears `host_ok` rather than returning: nothing may
    // return between here and `close`, which takes the hooks off again.
    void install_game_hooks();
    // The frame loop, until the window closes or the frame limit is reached.
    void run();
    // Takes the hooks off, closes everything and writes the session's
    // reports. The process's exit code.
    [[nodiscard]] int close();

    void handle_events();
    // Runs the frames that have come due, a few at most.
    void run_due_frames(bool smooth_table, bool nudge_focused);
    void present_playfield(bool smooth_table);
    void present_cabinet_displays();
    // Writes the window placements back, in a cabinet run that keeps them.
    void save_current_cabinet_config();
    // The selected window, or the playfield once the selected one has closed.
    [[nodiscard]] SDL_Window* selected_cabinet_window() const noexcept;
};

} // namespace devils_crush::app::host
