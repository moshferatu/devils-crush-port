// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "host_present.hpp"
#include "host_textures.hpp"

namespace devils_crush::app::host {

struct AuxiliaryWindow {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

struct HostDisplays {
    std::vector<SDL_DisplayID> ids;
    std::vector<devils_crush::cabinet::DisplayDescriptor> descriptors;
};

[[nodiscard]] HostDisplays enumerate_host_displays();

[[nodiscard]] bool apply_window_placement(
    SDL_Window* const window,
    const devils_crush::cabinet::WindowPlacement& placement,
    const HostDisplays& displays,
    const int default_width,
    const int default_height);

[[nodiscard]] devils_crush::cabinet::WindowPlacement
capture_window_placement(
    SDL_Window* const window,
    const HostDisplays& displays,
    devils_crush::cabinet::WindowPlacement fallback);

void destroy_auxiliary_window(AuxiliaryWindow& output);

[[nodiscard]] bool create_auxiliary_window(
    const char* const title,
    const int width,
    const int height,
    const devils_crush::cabinet::WindowPlacement& placement,
    const HostDisplays& displays,
    bool borderless,
    AuxiliaryWindow& output);

void focus_playfield_window(SDL_Window* const window);

// The claim made at start-up can still lose a race: a cabinet front end that
// puts its own window back up a moment after launching the game takes the
// keyboard with it, and having won the foreground once is no help. So it is
// re-made, four times a second, for the first five seconds the window is up,
// and only while something else is holding the focus. Nobody launches a
// pinball table and deliberately switches away inside five seconds; after that
// nothing here touches the foreground again, so a player who alt-tabs out
// later is left alone.
//
// Wall clock, not a frame count: the host loop runs several times faster than
// the display while the emulator warms up, so counting iterations would close
// the window in well under a second.
inline constexpr std::uint64_t kStartupFocusWindowMs = 5000U;
inline constexpr std::uint64_t kStartupFocusIntervalMs = 250U;

struct StartupFocusClaim {
    // Zero once the window has closed, and for runs nobody is watching.
    std::uint64_t deadline_ms = 0U;
    std::uint64_t next_attempt_ms = 0U;
};

void claim_startup_focus(SDL_Window* const window, StartupFocusClaim& claim);

// Whether another of the cabinet's windows fills `display`. SDL fills a
// monitor with one window: making a second fullscreen there minimizes the
// first, and a minimized playfield loses the keyboard.
[[nodiscard]] bool fullscreen_beside(
    SDL_Window* const window,
    const SDL_DisplayID display,
    std::span<SDL_Window* const> cabinet_windows) noexcept;

// Moves a window to the next display (direction 1) or the previous one (-1),
// centred at its own size, and keeps it fullscreen if it was. A fullscreen
// window passes over a display another cabinet window fills.
[[nodiscard]] bool move_to_next_display(
    SDL_Window* const window,
    const int direction,
    const HostDisplays& displays,
    std::span<SDL_Window* const> cabinet_windows);

[[nodiscard]] std::string playfield_title(
    const bool cabinet_mode, const bool native_code);

[[nodiscard]] bool present_black(AuxiliaryWindow& output);

[[nodiscard]] bool present_dmd_panel(
    AuxiliaryWindow& output,
    const devils_crush::render::ArgbImage& image,
    SourceTexture& source,
    StageTexture& stage);

} // namespace devils_crush::app::host
