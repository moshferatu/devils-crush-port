// SPDX-License-Identifier: GPL-3.0-or-later
#include "host_windows.hpp"

#include "host_present.hpp"
#include "host_textures.hpp"

namespace devils_crush::app::host {

namespace {

[[nodiscard]] devils_crush::cabinet::DisplayRotation display_rotation(
    const SDL_DisplayOrientation orientation) noexcept
{
    using devils_crush::cabinet::DisplayRotation;
    switch (orientation) {
    case SDL_ORIENTATION_LANDSCAPE:
        return DisplayRotation::landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return DisplayRotation::landscape_flipped;
    case SDL_ORIENTATION_PORTRAIT:
        return DisplayRotation::portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return DisplayRotation::portrait_flipped;
    case SDL_ORIENTATION_UNKNOWN:
    default:
        return DisplayRotation::unknown;
    }
}

} // namespace

[[nodiscard]] HostDisplays enumerate_host_displays()
{
    HostDisplays result;
    int count = 0;
    SDL_DisplayID* const ids = SDL_GetDisplays(&count);
    if (ids == nullptr || count <= 0) {
        report_sdl_error("SDL_GetDisplays failed");
        SDL_free(ids);
        return result;
    }
    result.ids.reserve(static_cast<std::size_t>(count));
    result.descriptors.reserve(static_cast<std::size_t>(count));
    const auto primary = SDL_GetPrimaryDisplay();
    for (int index = 0; index < count; ++index) {
        SDL_Rect bounds{};
        if (!SDL_GetDisplayBounds(ids[index], &bounds)
            || bounds.w <= 0 || bounds.h <= 0) {
            report_sdl_error("SDL_GetDisplayBounds failed");
            continue;
        }
        const char* const name = SDL_GetDisplayName(ids[index]);
        result.ids.push_back(ids[index]);
        result.descriptors.push_back({
            .identity =
                name != nullptr && name[0] != '\0'
                ? name
                : "Unnamed display",
            .bounds = {
                .x = bounds.x,
                .y = bounds.y,
                .width = bounds.w,
                .height = bounds.h,
            },
            .rotation =
                display_rotation(
                    SDL_GetCurrentDisplayOrientation(ids[index])),
            .primary = ids[index] == primary,
        });
    }
    SDL_free(ids);
    return result;
}

[[nodiscard]] bool apply_window_placement(
    SDL_Window* const window,
    const devils_crush::cabinet::WindowPlacement& placement,
    const HostDisplays& displays,
    const int default_width,
    const int default_height)
{
    if (window == nullptr || displays.ids.size() != displays.descriptors.size()) {
        return false;
    }
    const auto resolved = devils_crush::cabinet::resolve_display(
        placement,
        displays.descriptors);
    if (!resolved) {
        return true;
    }
    const auto geometry = devils_crush::cabinet::fit_window_geometry(
        placement,
        displays.descriptors[*resolved],
        default_width,
        default_height);
    const bool was_fullscreen =
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U;
    if (was_fullscreen && !SDL_SetWindowFullscreen(window, false)) {
        return report_sdl_error(
            "Leaving cabinet window fullscreen state failed");
    }
    if (!geometry.valid()
        || !SDL_SetWindowSize(window, geometry.width, geometry.height)
        || !SDL_SetWindowPosition(window, geometry.x, geometry.y)) {
        return report_sdl_error("Applying cabinet window geometry failed");
    }
    if (placement.fullscreen
        && !SDL_SetWindowFullscreen(window, true)) {
        return report_sdl_error(
            "Applying cabinet window fullscreen state failed");
    }
    return true;
}

[[nodiscard]] devils_crush::cabinet::WindowPlacement
capture_window_placement(
    SDL_Window* const window,
    const HostDisplays& displays,
    devils_crush::cabinet::WindowPlacement fallback)
{
    if (window == nullptr) {
        return fallback;
    }
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (SDL_GetWindowPosition(window, &x, &y)
        && SDL_GetWindowSize(window, &width, &height)
        && width > 0 && height > 0) {
        fallback.geometry = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        };
    }
    fallback.fullscreen =
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U;
    const auto display_id = SDL_GetDisplayForWindow(window);
    if (display_id != 0U) {
        const char* const name = SDL_GetDisplayName(display_id);
        if (name != nullptr && name[0] != '\0') {
            fallback.display_identity = name;
        }
        fallback.rotation = display_rotation(
            SDL_GetCurrentDisplayOrientation(display_id));
    }
    for (std::size_t index = 0; index < displays.ids.size(); ++index) {
        if (displays.ids[index] == display_id) {
            if (fallback.display_identity.empty()) {
                fallback.display_identity =
                    displays.descriptors[index].identity;
            }
            break;
        }
    }
    return fallback;
}

void destroy_auxiliary_window(AuxiliaryWindow& output)
{
    if (output.renderer != nullptr) {
        SDL_DestroyRenderer(output.renderer);
    }
    if (output.window != nullptr) {
        SDL_DestroyWindow(output.window);
    }
    output = {};
}

[[nodiscard]] bool create_auxiliary_window(
    const char* const title,
    const int width,
    const int height,
    const devils_crush::cabinet::WindowPlacement& placement,
    const HostDisplays& displays,
    const bool borderless,
    AuxiliaryWindow& output)
{
    output.window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY
            | SDL_WINDOW_NOT_FOCUSABLE
            | SDL_WINDOW_ALWAYS_ON_TOP
            | (borderless ? SDL_WINDOW_BORDERLESS : SDL_WindowFlags{0}));
    if (output.window == nullptr) {
        report_sdl_error("SDL_CreateWindow (cabinet auxiliary) failed");
        return false;
    }
    if (!apply_window_placement(
            output.window,
            placement,
            displays,
            width,
            height)) {
        destroy_auxiliary_window(output);
        return false;
    }
    output.renderer = SDL_CreateRenderer(output.window, nullptr);
    if (output.renderer == nullptr) {
        report_sdl_error("SDL_CreateRenderer (cabinet auxiliary) failed");
        destroy_auxiliary_window(output);
        return false;
    }
    (void)SDL_SetRenderVSync(output.renderer, 0);
    if (!SDL_ShowWindow(output.window)) {
        report_sdl_error("SDL_ShowWindow (cabinet auxiliary) failed");
        destroy_auxiliary_window(output);
        return false;
    }
    return true;
}

// Windows will not hand the foreground to a process that has not just been
// given input by the player, so a host started from a cabinet front end can
// come up drawing the playfield while the launcher still owns the keyboard:
// the table is on screen and the flippers do nothing. `SDL_ShowWindow` alone
// is subject to that rule. `SDL_HINT_FORCE_RAISEWINDOW` turns the raise into
// the thread-input sequence that gets around it. The DMD and backglass are
// left where they are; they are created `SDL_WINDOW_NOT_FOCUSABLE` because
// the playfield is the window that plays.
void focus_playfield_window(SDL_Window* const window)
{
    if (!SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW, "1")) {
        (void)report_sdl_error("SDL_SetHint (forceful raise) failed");
    }
    if (!SDL_RaiseWindow(window)) {
        (void)report_sdl_error("SDL_RaiseWindow (playfield) failed");
    }
}

void claim_startup_focus(SDL_Window* const window, StartupFocusClaim& claim)
{
    if (claim.deadline_ms == 0U) {
        return;
    }
    const std::uint64_t now = SDL_GetTicks();
    if (now >= claim.deadline_ms) {
        claim.deadline_ms = 0U;
        return;
    }
    if (now < claim.next_attempt_ms) {
        return;
    }
    claim.next_attempt_ms = now + kStartupFocusIntervalMs;
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) == 0U) {
        focus_playfield_window(window);
    }
}

[[nodiscard]] bool fullscreen_beside(
    SDL_Window* const window,
    const SDL_DisplayID display,
    std::span<SDL_Window* const> cabinet_windows) noexcept
{
    for (SDL_Window* const other : cabinet_windows) {
        if (other != nullptr && other != window
            && (SDL_GetWindowFlags(other) & SDL_WINDOW_FULLSCREEN) != 0U
            && SDL_GetDisplayForWindow(other) == display) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool move_to_next_display(
    SDL_Window* const window,
    const int direction,
    const HostDisplays& displays,
    std::span<SDL_Window* const> cabinet_windows)
{
    const auto count = static_cast<std::int64_t>(displays.ids.size());
    if (window == nullptr || count < 2) {
        return true;
    }
    const auto current_id = SDL_GetDisplayForWindow(window);
    std::int64_t current = 0;
    for (std::int64_t index = 0; index < count; ++index) {
        if (displays.ids[static_cast<std::size_t>(index)] == current_id) {
            current = index;
            break;
        }
    }
    const bool was_fullscreen =
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U;
    SDL_DisplayID target_id = 0U;
    for (std::int64_t step = 1; step < count && target_id == 0U; ++step) {
        const auto candidate = displays.ids[static_cast<std::size_t>(
            ((current + direction * step) % count + count) % count)];
        if (!was_fullscreen
            || !fullscreen_beside(window, candidate, cabinet_windows)) {
            target_id = candidate;
        }
    }
    if (target_id == 0U) {
        return true;
    }
    SDL_Rect bounds{};
    if (!SDL_GetDisplayUsableBounds(target_id, &bounds)
        && !SDL_GetDisplayBounds(target_id, &bounds)) {
        return report_sdl_error("Reading the next display's bounds failed");
    }
    // Leave fullscreen before changing the windowed placement, and wait for
    // the platform to finish so the next request uses the new display.
    const bool fullscreen =
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U;
    if (fullscreen
        && (!SDL_SetWindowFullscreen(window, false)
            || !SDL_SyncWindow(window))) {
        return report_sdl_error("Leaving fullscreen to move a window failed");
    }
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSize(window, &width, &height)) {
        return report_sdl_error("Reading a window's size failed");
    }
    width = std::min(width, bounds.w);
    height = std::min(height, bounds.h);
    if (!SDL_SetWindowSize(window, width, height)
        || !SDL_SetWindowPosition(
            window,
            bounds.x + (bounds.w - width) / 2,
            bounds.y + (bounds.h - height) / 2)
        || !SDL_SyncWindow(window)) {
        return report_sdl_error("Moving a window to the next display failed");
    }
    if (fullscreen && !SDL_SetWindowFullscreen(window, true)) {
        return report_sdl_error("Restoring fullscreen after a move failed");
    }
    (void)SDL_RaiseWindow(window);
    return true;
}

// Names the game code actually running, so the emulated original is never
// mistaken for the native default (or the reverse).
[[nodiscard]] std::string playfield_title(
    const bool cabinet_mode, const bool native_code)
{
    std::string title = cabinet_mode
        ? "Devil's Crush - Cabinet Playfield"
        : "Devil's Crush";
    if (!native_code) {
        title += cabinet_mode ? " (Emulated Original)" : " - Emulated Original";
    }
    return title;
}

[[nodiscard]] bool present_black(AuxiliaryWindow& output)
{
    if (output.renderer == nullptr) {
        return true;
    }
    return (
               SDL_SetRenderDrawColor(
                   output.renderer,
                   0,
                   0,
                   0,
                   255)
               && SDL_RenderClear(output.renderer)
               && SDL_RenderPresent(output.renderer))
        || report_sdl_error("Rendering the black backglass failed");
}

[[nodiscard]] bool present_dmd_panel(
    AuxiliaryWindow& output,
    const devils_crush::render::ArgbImage& image,
    SourceTexture& source,
    StageTexture& stage)
{
    return upload_image(output.renderer, image, source)
        && present_frame(output.renderer, source, stage, 20U, 7U, {0U, 0U, 0U});
}

} // namespace devils_crush::app::host
