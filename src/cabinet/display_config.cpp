// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/display_config.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <system_error>
#include <utility>

namespace devils_crush::cabinet {
namespace {

constexpr std::size_t kMaximumConfigBytes = 64U * 1024U;

[[nodiscard]] bool is_unescaped(const unsigned char value) noexcept
{
    return std::isalnum(value) != 0 || value == '-' || value == '_'
        || value == '.' || value == '/' || value == '\\'
        || value == ':' || value == ' ';
}

[[nodiscard]] char hex_digit(const unsigned int value) noexcept
{
    return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
}

[[nodiscard]] std::string escape_value(const std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const unsigned char byte : value) {
        if (is_unescaped(byte)) {
            escaped.push_back(static_cast<char>(byte));
        } else {
            escaped.push_back('%');
            escaped.push_back(hex_digit(byte >> 4U));
            escaped.push_back(hex_digit(byte & 0x0FU));
        }
    }
    return escaped;
}

[[nodiscard]] int hex_value(const char value) noexcept
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

[[nodiscard]] std::optional<std::string> unescape_value(
    const std::string_view value)
{
    std::string unescaped;
    unescaped.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            unescaped.push_back(value[index]);
            continue;
        }
        if (index + 2U >= value.size()) {
            return std::nullopt;
        }
        const auto high = hex_value(value[index + 1U]);
        const auto low = hex_value(value[index + 2U]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        unescaped.push_back(static_cast<char>((high << 4U) | low));
        index += 2U;
    }
    return unescaped;
}

template <typename Integer>
[[nodiscard]] bool parse_integer(
    const std::string_view text,
    Integer& value) noexcept
{
    if (text.empty()) {
        return false;
    }
    const auto* const first = text.data();
    const auto* const last = first + text.size();
    const auto [end, error] = std::from_chars(first, last, value);
    return error == std::errc{} && end == last;
}

[[nodiscard]] std::optional<DisplayRotation> parse_rotation(
    const std::string_view text) noexcept
{
    unsigned int raw = 0;
    if (!parse_integer(text, raw)
        || raw
            > static_cast<unsigned int>(
                DisplayRotation::portrait_flipped)) {
        return std::nullopt;
    }
    return static_cast<DisplayRotation>(raw);
}

void append_placement(
    std::string& output,
    const std::string_view prefix,
    const WindowPlacement& placement)
{
    const auto append = [&output, prefix](
                            const std::string_view key,
                            const auto& value) {
        output += prefix;
        output += '.';
        output += key;
        output += '=';
        output += value;
        output += '\n';
    };
    append("display", escape_value(placement.display_identity));
    append("x", std::to_string(placement.geometry.x));
    append("y", std::to_string(placement.geometry.y));
    append("width", std::to_string(placement.geometry.width));
    append("height", std::to_string(placement.geometry.height));
    append(
        "rotation",
        std::to_string(static_cast<unsigned int>(placement.rotation)));
    append("fullscreen", placement.fullscreen ? "1" : "0");
}

[[nodiscard]] bool parse_placement(
    const std::map<std::string, std::string, std::less<>>& fields,
    const std::string_view prefix,
    WindowPlacement& placement)
{
    const auto field = [&fields, prefix](
                           const std::string_view suffix)
        -> const std::string* {
        std::string key(prefix);
        key += '.';
        key += suffix;
        const auto found = fields.find(key);
        return found == fields.end() ? nullptr : &found->second;
    };
    const auto* const display = field("display");
    const auto* const x = field("x");
    const auto* const y = field("y");
    const auto* const width = field("width");
    const auto* const height = field("height");
    const auto* const rotation = field("rotation");
    const auto* const fullscreen = field("fullscreen");
    if (display == nullptr || x == nullptr || y == nullptr
        || width == nullptr || height == nullptr || rotation == nullptr
        || fullscreen == nullptr) {
        return false;
    }

    const auto identity = unescape_value(*display);
    const auto parsed_rotation = parse_rotation(*rotation);
    int fullscreen_value = 0;
    if (!identity || !parsed_rotation
        || !parse_integer(*x, placement.geometry.x)
        || !parse_integer(*y, placement.geometry.y)
        || !parse_integer(*width, placement.geometry.width)
        || !parse_integer(*height, placement.geometry.height)
        || !parse_integer(*fullscreen, fullscreen_value)
        || (fullscreen_value != 0 && fullscreen_value != 1)
        || placement.geometry.width < 0
        || placement.geometry.height < 0) {
        return false;
    }
    placement.display_identity = *identity;
    placement.rotation = *parsed_rotation;
    placement.fullscreen = fullscreen_value != 0;
    return true;
}

[[nodiscard]] std::uint64_t overlap_area(
    const DisplayRect& left,
    const DisplayRect& right) noexcept
{
    if (!left.valid() || !right.valid()) {
        return 0U;
    }
    const auto left_right =
        static_cast<std::int64_t>(left.x) + left.width;
    const auto right_right =
        static_cast<std::int64_t>(right.x) + right.width;
    const auto left_bottom =
        static_cast<std::int64_t>(left.y) + left.height;
    const auto right_bottom =
        static_cast<std::int64_t>(right.y) + right.height;
    const auto width = std::max<std::int64_t>(
        0,
        std::min(left_right, right_right)
            - std::max<std::int64_t>(left.x, right.x));
    const auto height = std::max<std::int64_t>(
        0,
        std::min(left_bottom, right_bottom)
            - std::max<std::int64_t>(left.y, right.y));
    return static_cast<std::uint64_t>(width)
        * static_cast<std::uint64_t>(height);
}

} // namespace

std::vector<std::byte> encode_display_config(const DisplayConfig& config)
{
    if (config.format_version != kDisplayConfigVersion) {
        return {};
    }
    std::string text = "version=";
    text += std::to_string(config.format_version);
    text += '\n';
    append_placement(text, "playfield", config.playfield);
    append_placement(text, "dmd", config.dmd);
    append_placement(text, "backglass", config.backglass);
    text += "backglass.path=";
    text += escape_value(config.backglass_path);
    text += '\n';

    std::vector<std::byte> bytes(text.size());
    std::transform(
        text.begin(),
        text.end(),
        bytes.begin(),
        [](const char value) {
            return static_cast<std::byte>(
                static_cast<unsigned char>(value));
        });
    return bytes;
}

std::optional<DisplayConfig> decode_display_config(
    const std::span<const std::byte> bytes)
{
    if (bytes.empty() || bytes.size() > kMaximumConfigBytes) {
        return std::nullopt;
    }
    std::string text(bytes.size(), '\0');
    std::transform(
        bytes.begin(),
        bytes.end(),
        text.begin(),
        [](const std::byte value) {
            return static_cast<char>(
                std::to_integer<unsigned char>(value));
        });

    std::map<std::string, std::string, std::less<>> fields;
    std::size_t line_start = 0;
    while (line_start < text.size()) {
        const auto line_end = text.find('\n', line_start);
        auto line = std::string_view(text).substr(
            line_start,
            line_end == std::string::npos
                ? std::string::npos
                : line_end - line_start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (!line.empty()) {
            const auto separator = line.find('=');
            if (separator == std::string_view::npos
                || separator == 0U) {
                return std::nullopt;
            }
            auto key = std::string(line.substr(0U, separator));
            auto value = std::string(line.substr(separator + 1U));
            if (!fields.emplace(std::move(key), std::move(value)).second) {
                return std::nullopt;
            }
        }
        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1U;
    }

    const auto version = fields.find("version");
    const auto path = fields.find("backglass.path");
    DisplayConfig config;
    if (version == fields.end() || path == fields.end()
        || !parse_integer(version->second, config.format_version)
        || config.format_version != kDisplayConfigVersion
        || !parse_placement(fields, "playfield", config.playfield)
        || !parse_placement(fields, "dmd", config.dmd)
        || !parse_placement(fields, "backglass", config.backglass)) {
        return std::nullopt;
    }
    const auto backglass_path = unescape_value(path->second);
    if (!backglass_path) {
        return std::nullopt;
    }
    config.backglass_path = *backglass_path;
    return config;
}

ConfigLoadResult load_display_config(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {
            .status =
                error ? ConfigLoadStatus::io_error
                      : ConfigLoadStatus::not_found,
            .config = {},
            .message = error
                ? "Could not inspect the cabinet configuration: "
                    + error.message()
                : "No cabinet configuration exists.",
        };
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0U || size > kMaximumConfigBytes
        || size
            > static_cast<std::uintmax_t>(
                std::numeric_limits<std::streamsize>::max())) {
        return {
            .status = error ? ConfigLoadStatus::io_error
                            : ConfigLoadStatus::invalid,
            .config = {},
            .message = error
                ? "Could not inspect the cabinet configuration: "
                    + error.message()
                : "The cabinet configuration size is invalid.",
        };
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input
        || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        return {
            .status = ConfigLoadStatus::io_error,
            .config = {},
            .message = "Could not read the cabinet configuration.",
        };
    }
    const auto decoded = decode_display_config(bytes);
    if (!decoded) {
        return {
            .status = ConfigLoadStatus::invalid,
            .config = {},
            .message =
                "The cabinet configuration is invalid or unsupported.",
        };
    }
    return {
        .status = ConfigLoadStatus::loaded,
        .config = *decoded,
        .message = "Cabinet configuration loaded.",
    };
}

ConfigSaveResult save_display_config_atomic(
    const std::filesystem::path& path,
    const DisplayConfig& config)
{
    const auto bytes = encode_display_config(config);
    if (bytes.empty()) {
        return {
            .saved = false,
            .message = "The cabinet configuration could not be encoded.",
        };
    }
    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return {
                .saved = false,
                .message =
                    "Could not create the cabinet configuration directory: "
                    + error.message(),
            };
        }
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    {
        std::ofstream output(
            temporary_path,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) {
            return {
                .saved = false,
                .message =
                    "Could not write the temporary cabinet configuration.",
            };
        }
    }
    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return {
            .saved = false,
            .message =
                "Could not atomically commit the cabinet configuration; "
                "Windows error "
                + std::to_string(GetLastError()) + ".",
        };
    }
    return {
        .saved = true,
        .message = "Cabinet configuration saved.",
    };
}

std::optional<std::size_t> resolve_display(
    const WindowPlacement& placement,
    const std::span<const DisplayDescriptor> displays) noexcept
{
    if (displays.empty()) {
        return std::nullopt;
    }

    std::optional<std::size_t> best_identity;
    std::uint64_t best_identity_overlap = 0U;
    if (!placement.display_identity.empty()) {
        for (std::size_t index = 0; index < displays.size(); ++index) {
            if (displays[index].identity != placement.display_identity) {
                continue;
            }
            const auto overlap =
                overlap_area(placement.geometry, displays[index].bounds);
            if (!best_identity || overlap > best_identity_overlap
                || (overlap == best_identity_overlap
                    && displays[index].primary
                    && !displays[*best_identity].primary)) {
                best_identity = index;
                best_identity_overlap = overlap;
            }
        }
    }
    if (best_identity) {
        return best_identity;
    }

    std::optional<std::size_t> best_geometry;
    std::uint64_t best_overlap = 0U;
    for (std::size_t index = 0; index < displays.size(); ++index) {
        const auto overlap =
            overlap_area(placement.geometry, displays[index].bounds);
        if (overlap > best_overlap) {
            best_geometry = index;
            best_overlap = overlap;
        }
    }
    if (best_geometry) {
        return best_geometry;
    }
    for (std::size_t index = 0; index < displays.size(); ++index) {
        if (displays[index].primary) {
            return index;
        }
    }
    return 0U;
}

DisplayRect fit_window_geometry(
    const WindowPlacement& placement,
    const DisplayDescriptor& display,
    const int default_width,
    const int default_height) noexcept
{
    if (!display.bounds.valid()) {
        return {};
    }
    auto width = placement.geometry.valid()
        ? placement.geometry.width
        : default_width;
    auto height = placement.geometry.valid()
        ? placement.geometry.height
        : default_height;
    width = std::clamp(width, 1, display.bounds.width);
    height = std::clamp(height, 1, display.bounds.height);

    const auto maximum_x =
        static_cast<std::int64_t>(display.bounds.x)
        + display.bounds.width - width;
    const auto maximum_y =
        static_cast<std::int64_t>(display.bounds.y)
        + display.bounds.height - height;
    const auto desired_x = placement.geometry.valid()
        ? static_cast<std::int64_t>(placement.geometry.x)
        : static_cast<std::int64_t>(display.bounds.x)
            + (display.bounds.width - width) / 2;
    const auto desired_y = placement.geometry.valid()
        ? static_cast<std::int64_t>(placement.geometry.y)
        : static_cast<std::int64_t>(display.bounds.y)
            + (display.bounds.height - height) / 2;
    return {
        .x = static_cast<int>(std::clamp<std::int64_t>(
            desired_x,
            display.bounds.x,
            maximum_x)),
        .y = static_cast<int>(std::clamp<std::int64_t>(
            desired_y,
            display.bounds.y,
            maximum_y)),
        .width = width,
        .height = height,
    };
}

} // namespace devils_crush::cabinet
