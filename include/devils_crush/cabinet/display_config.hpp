// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace devils_crush::cabinet {

inline constexpr std::uint32_t kDisplayConfigVersion = 1U;

struct DisplayRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return width > 0 && height > 0;
    }

    [[nodiscard]] bool operator==(const DisplayRect&) const = default;
};

enum class DisplayRotation : std::uint8_t {
    unknown = 0,
    landscape = 1,
    landscape_flipped = 2,
    portrait = 3,
    portrait_flipped = 4,
};

struct DisplayDescriptor {
    std::string identity;
    DisplayRect bounds;
    DisplayRotation rotation = DisplayRotation::unknown;
    bool primary = false;
};

struct WindowPlacement {
    std::string display_identity;
    DisplayRect geometry;
    DisplayRotation rotation = DisplayRotation::unknown;
    bool fullscreen = false;

    [[nodiscard]] bool operator==(const WindowPlacement&) const = default;
};

// The eight PC Engine pad inputs, in the order they are written to
// `cabinet.ini`. These are pad inputs rather than game actions on purpose:
// the cartridge decides what each one does, and it does not do the same thing
// in a menu as it does in play.
//
// During play `01:D466` gates each flipper piece on `$3B80,X`. The left-side
// pieces `$04/$06/$08` carry mask `$F0`, so *any* direction works the left
// flipper; the right-side pieces `$05/$07/$09` carry mask `$01`, so button I
// works the right flipper and also launches the ball. Button II is nudge.
enum class PadButton : std::size_t {
    one = 0,
    two = 1,
    select = 2,
    run = 3,
    up = 4,
    right = 5,
    down = 6,
    left = 7,
};

inline constexpr std::size_t kPadButtonCount = 8U;

// Keys are held as SDL scancode names -- "Left Shift", "Left", "Z" -- so the
// host can resolve them with `SDL_GetScancodeFromName` and the file stays
// hand-editable without a bespoke key table to keep in step. A bound input
// with no keys is unbound; that is distinct from the field being absent,
// which takes the default.
struct InputBindings {
    std::array<std::vector<std::string>, kPadButtonCount> keys;

    [[nodiscard]] const std::vector<std::string>& operator[](
        const PadButton button) const noexcept
    {
        return keys[static_cast<std::size_t>(button)];
    }

    [[nodiscard]] std::vector<std::string>& operator[](
        const PadButton button) noexcept
    {
        return keys[static_cast<std::size_t>(button)];
    }

    [[nodiscard]] bool operator==(const InputBindings&) const = default;
};

// The `input.` suffix each pad input is stored under.
[[nodiscard]] std::string_view pad_button_field(PadButton button) noexcept;

// Left Shift and Left work the left flipper; Right Shift and Right work the
// right flipper and launch. Z and X stay bound so the original keyboard
// layout still works, and D-pad right has no default key because Right now
// belongs to the right flipper.
[[nodiscard]] InputBindings default_input_bindings();

struct DisplayConfig {
    std::uint32_t format_version = kDisplayConfigVersion;
    WindowPlacement playfield;
    WindowPlacement dmd;
    WindowPlacement backglass;
    std::string backglass_path;
    InputBindings input = default_input_bindings();

    [[nodiscard]] bool operator==(const DisplayConfig&) const = default;
};

enum class ConfigLoadStatus {
    loaded,
    not_found,
    invalid,
    io_error,
};

struct ConfigLoadResult {
    ConfigLoadStatus status = ConfigLoadStatus::io_error;
    DisplayConfig config;
    std::string message;

    [[nodiscard]] bool loaded() const noexcept
    {
        return status == ConfigLoadStatus::loaded;
    }
};

struct ConfigSaveResult {
    bool saved = false;
    std::string message;
};

[[nodiscard]] std::vector<std::byte> encode_display_config(
    const DisplayConfig& config);

[[nodiscard]] std::optional<DisplayConfig> decode_display_config(
    std::span<const std::byte> bytes);

[[nodiscard]] ConfigLoadResult load_display_config(
    const std::filesystem::path& path);

[[nodiscard]] ConfigSaveResult save_display_config_atomic(
    const std::filesystem::path& path,
    const DisplayConfig& config);

[[nodiscard]] std::optional<std::size_t> resolve_display(
    const WindowPlacement& placement,
    std::span<const DisplayDescriptor> displays) noexcept;

[[nodiscard]] DisplayRect fit_window_geometry(
    const WindowPlacement& placement,
    const DisplayDescriptor& display,
    int default_width,
    int default_height) noexcept;

} // namespace devils_crush::cabinet
