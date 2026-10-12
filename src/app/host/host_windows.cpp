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

[[nodiscard]] std::pair<int, int> assignment_default_size(
    const AssignmentRole role) noexcept
{
    switch (role) {
    case AssignmentRole::playfield:
        return {720, 1280};
    case AssignmentRole::dmd:
        return {960, 240};
    case AssignmentRole::backglass:
        return {960, 540};
    }
    return {640, 480};
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
    AuxiliaryWindow& output)
{
    output.window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY
            | SDL_WINDOW_NOT_FOCUSABLE
            | SDL_WINDOW_ALWAYS_ON_TOP);
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

[[nodiscard]] SDL_Window* assignment_window(
    const AssignmentRole role,
    SDL_Window* const playfield,
    AuxiliaryWindow& dmd,
    AuxiliaryWindow& backglass) noexcept
{
    switch (role) {
    case AssignmentRole::playfield:
        return playfield;
    case AssignmentRole::dmd:
        return dmd.window;
    case AssignmentRole::backglass:
        return backglass.window;
    }
    return nullptr;
}

[[nodiscard]] devils_crush::cabinet::WindowPlacement&
assignment_placement(
    const AssignmentRole role,
    devils_crush::cabinet::DisplayConfig& config) noexcept
{
    switch (role) {
    case AssignmentRole::playfield:
        return config.playfield;
    case AssignmentRole::dmd:
        return config.dmd;
    case AssignmentRole::backglass:
        return config.backglass;
    }
    return config.playfield;
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

void set_assignment_titles(
    SDL_Window* const playfield,
    const std::string& playfield_name,
    AuxiliaryWindow& dmd,
    AuxiliaryWindow& backglass,
    const bool active,
    const AssignmentRole selected)
{
    if (!active) {
        (void)SDL_SetWindowTitle(playfield, playfield_name.c_str());
        if (dmd.window != nullptr) {
            (void)SDL_SetWindowTitle(
                dmd.window,
                "Devil's Crush - Cabinet DMD");
        }
        if (backglass.window != nullptr) {
            (void)SDL_SetWindowTitle(
                backglass.window,
                "Devil's Crush - Cabinet Backglass");
        }
        return;
    }
    const auto title = [selected](
                           const AssignmentRole role,
                           const char* const name) {
        std::string value = role == selected ? "[SELECTED] " : "";
        value += name;
        value +=
            " - 1/2/3 select, arrows display, F fullscreen, Enter done";
        return value;
    };
    const auto playfield_title =
        title(AssignmentRole::playfield, "Cabinet setup: Playfield");
    (void)SDL_SetWindowTitle(playfield, playfield_title.c_str());
    if (dmd.window != nullptr) {
        const auto dmd_title =
            title(AssignmentRole::dmd, "Cabinet setup: DMD");
        (void)SDL_SetWindowTitle(dmd.window, dmd_title.c_str());
    }
    if (backglass.window != nullptr) {
        const auto backglass_title =
            title(AssignmentRole::backglass, "Cabinet setup: Backglass");
        (void)SDL_SetWindowTitle(
            backglass.window,
            backglass_title.c_str());
    }
}

[[nodiscard]] bool cycle_assignment_display(
    const AssignmentRole role,
    const int direction,
    SDL_Window* const playfield,
    AuxiliaryWindow& dmd,
    AuxiliaryWindow& backglass,
    devils_crush::cabinet::DisplayConfig& config,
    const HostDisplays& displays)
{
    SDL_Window* const window =
        assignment_window(role, playfield, dmd, backglass);
    if (window == nullptr || displays.ids.empty()
        || displays.ids.size() != displays.descriptors.size()) {
        return true;
    }
    const auto current_id = SDL_GetDisplayForWindow(window);
    std::size_t current = 0U;
    for (std::size_t index = 0; index < displays.ids.size(); ++index) {
        if (displays.ids[index] == current_id) {
            current = index;
            break;
        }
    }
    const auto count = static_cast<std::int64_t>(displays.ids.size());
    auto next =
        (static_cast<std::int64_t>(current) + direction) % count;
    if (next < 0) {
        next += count;
    }
    const auto target = static_cast<std::size_t>(next);
    auto& placement = assignment_placement(role, config);
    placement.display_identity = displays.descriptors[target].identity;
    placement.rotation = displays.descriptors[target].rotation;
    const auto [default_width, default_height] =
        assignment_default_size(role);
    const auto& bounds = displays.descriptors[target].bounds;
    const auto width = std::min(default_width, bounds.width);
    const auto height = std::min(default_height, bounds.height);
    placement.geometry = {
        .x = bounds.x + (bounds.width - width) / 2,
        .y = bounds.y + (bounds.height - height) / 2,
        .width = width,
        .height = height,
    };
    return apply_window_placement(
        window,
        placement,
        displays,
        default_width,
        default_height);
}

[[nodiscard]] bool present_assignment(
    SDL_Renderer* const renderer,
    const AssignmentRole role,
    const bool selected)
{
    if (renderer == nullptr) {
        return true;
    }
    std::array<std::uint8_t, 3> color{};
    switch (role) {
    case AssignmentRole::playfield:
        color = {24U, 72U, 160U};
        break;
    case AssignmentRole::dmd:
        color = {184U, 56U, 8U};
        break;
    case AssignmentRole::backglass:
        color = {96U, 32U, 128U};
        break;
    }
    if (!SDL_SetRenderDrawColor(
            renderer,
            color[0],
            color[1],
            color[2],
            255U)
        || !SDL_RenderClear(renderer)) {
        return report_sdl_error("Clearing a cabinet assignment window failed");
    }
    int width = 0;
    int height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &width, &height)) {
        return report_sdl_error(
            "Reading a cabinet assignment window size failed");
    }
    if (selected && width > 0 && height > 0) {
        const auto border =
            std::max(4.0F, std::min(width, height) / 32.0F);
        const auto inside_width =
            std::max(0.0F, static_cast<float>(width) - 2.0F * border);
        const auto inside_height =
            std::max(0.0F, static_cast<float>(height) - 2.0F * border);
        const std::array marker{
            SDL_FRect{border, border, inside_width, border},
            SDL_FRect{
                border,
                static_cast<float>(height) - 2.0F * border,
                inside_width,
                border,
            },
            SDL_FRect{border, border, border, inside_height},
            SDL_FRect{
                static_cast<float>(width) - 2.0F * border,
                border,
                border,
                inside_height,
            },
        };
        if (!SDL_SetRenderDrawColor(renderer, 255U, 240U, 96U, 255U)
            || !SDL_RenderFillRects(
                renderer,
                marker.data(),
                static_cast<int>(marker.size()))) {
            return report_sdl_error(
                "Drawing a cabinet assignment marker failed");
        }
    }
    return SDL_RenderPresent(renderer)
        || report_sdl_error(
            "Presenting a cabinet assignment window failed");
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
