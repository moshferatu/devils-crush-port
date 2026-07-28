// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

struct DisplayConfig {
    std::uint32_t format_version = kDisplayConfigVersion;
    WindowPlacement playfield;
    WindowPlacement dmd;
    WindowPlacement backglass;
    std::string backglass_path;

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
