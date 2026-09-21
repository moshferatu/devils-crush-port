// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/display_config.hpp"

#include "devils_crush/core/atomic_file.hpp"

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
#include <ranges>
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

// Key names are joined with commas. `escape_value` leaves letters, digits and
// spaces alone, so "Left Shift,Left" survives a round trip verbatim and stays
// readable to anyone editing the file by hand.
void append_bindings(std::string& output, const InputBindings& input)
{
    for (std::size_t index = 0; index < kPadButtonCount; ++index) {
        const auto button = static_cast<PadButton>(index);
        output += "input.";
        output += pad_button_field(button);
        output += '=';
        bool first = true;
        for (const auto& key : input[button]) {
            if (!std::exchange(first, false)) {
                output += ',';
            }
            output += escape_value(key);
        }
        output += '\n';
    }
    const auto append_flipper = [&output](
                                    const std::string_view key,
                                    const std::vector<PadButton>& inputs) {
        output += "input.";
        output += key;
        output += '=';
        bool first = true;
        for (const auto input : inputs) {
            if (!std::exchange(first, false)) {
                output += ',';
            }
            output += pad_button_field(input);
        }
        output += '\n';
    };
    append_flipper("left_flipper", input.flippers.left);
    append_flipper("right_flipper", input.flippers.right);

    output += "input.launch=";
    bool first_launch = true;
    for (const auto& key : input.launch_keys) {
        if (!std::exchange(first_launch, false)) {
            output += ',';
        }
        output += escape_value(key);
    }
    output += '\n';

    // Written as six plain numbers and a name rather than as a device
    // identifier, because the two that matter -- the axis and its two
    // endpoints -- are exactly what `--cabinet-plunger-probe` prints while
    // somebody moves the rod, and the point of the probe is that its output
    // can be typed straight back in here.
    const auto& plunger = input.plunger;
    const auto& nudge = input.nudge;
    output += "input.nudge.device=" + escape_value(nudge.device);
    output += "\ninput.nudge.enabled=" + std::to_string(nudge.enabled);
    output += "\ninput.nudge.axis_x=" + std::to_string(nudge.axis_x);
    output += "\ninput.nudge.axis_y=" + std::to_string(nudge.axis_y);
    output += "\ninput.nudge.threshold=" + std::to_string(nudge.threshold);
    output += "\ninput.nudge.release=" + std::to_string(nudge.release);
    output += "\ninput.nudge.cooldown_ms=" + std::to_string(nudge.cooldown_ms);
    output += '\n';
    output += "input.plunger.device=";
    output += escape_value(plunger.device);
    output += "\ninput.plunger.axis=";
    output += std::to_string(plunger.axis);
    output += "\ninput.plunger.rest=";
    output += std::to_string(plunger.calibration.rest);
    output += "\ninput.plunger.drawn=";
    output += std::to_string(plunger.calibration.drawn);
    output += "\ninput.plunger.deadzone=";
    output += std::to_string(
        static_cast<unsigned>(plunger.calibration.deadzone));
    output += "\ninput.plunger.release=";
    output += std::to_string(
        static_cast<unsigned>(plunger.calibration.release));
    output += '\n';
}

[[nodiscard]] std::optional<PadButton> pad_button_from_field(
    const std::string_view name) noexcept
{
    for (std::size_t index = 0; index < kPadButtonCount; ++index) {
        const auto button = static_cast<PadButton>(index);
        if (pad_button_field(button) == name) {
            return button;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string_view trim(std::string_view text) noexcept
{
    const auto is_space = [](const char value) {
        return std::isspace(static_cast<unsigned char>(value)) != 0;
    };
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1U);
    }
    return text;
}

[[nodiscard]] std::optional<std::vector<std::string>> parse_binding(
    const std::string_view text)
{
    std::vector<std::string> keys;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto separator = text.find(',', start);
        const auto piece = trim(text.substr(
            start,
            separator == std::string_view::npos
                ? std::string_view::npos
                : separator - start));
        if (!piece.empty()) {
            auto key = unescape_value(piece);
            if (!key) {
                return std::nullopt;
            }
            // A key bound twice to the same input is the same key; keeping
            // both would only make the host test it twice.
            if (std::ranges::find(keys, *key) == keys.end()) {
                keys.push_back(std::move(*key));
            }
        }
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1U;
    }
    return keys;
}

// Absent fields take the default, so a configuration written before bindings
// existed, or one that names only the inputs its author cared about, still
// produces a playable pad.
//
// Note that the host rewrites `cabinet.ini` in full on exit, so once a file
// has been written every field is present and pinned. Changing a default in
// code will not reach an existing file: that file has to be edited, or its
// `input.` lines deleted so the defaults apply again.
[[nodiscard]] bool parse_bindings(
    const std::map<std::string, std::string, std::less<>>& fields,
    InputBindings& input)
{
    input = default_input_bindings();
    for (std::size_t index = 0; index < kPadButtonCount; ++index) {
        const auto button = static_cast<PadButton>(index);
        std::string key("input.");
        key += pad_button_field(button);
        const auto found = fields.find(key);
        if (found == fields.end()) {
            continue;
        }
        auto parsed = parse_binding(found->second);
        if (!parsed) {
            return false;
        }
        input[button] = std::move(*parsed);
    }
    const auto parse_flipper =
        [&fields](
            const std::string_view key,
            std::vector<PadButton>& inputs) {
            std::string field("input.");
            field += key;
            const auto found = fields.find(field);
            if (found == fields.end()) {
                return true;
            }
            std::vector<PadButton> parsed;
            const std::string_view text(found->second);
            std::size_t start = 0;
            while (start <= text.size()) {
                const auto separator = text.find(',', start);
                const auto piece = trim(text.substr(
                    start,
                    separator == std::string_view::npos
                        ? std::string_view::npos
                        : separator - start));
                if (!piece.empty()) {
                    const auto button = pad_button_from_field(piece);
                    if (!button) {
                        return false;
                    }
                    if (std::ranges::find(parsed, *button) == parsed.end()) {
                        parsed.push_back(*button);
                    }
                }
                if (separator == std::string_view::npos) {
                    break;
                }
                start = separator + 1U;
            }
            inputs = std::move(parsed);
            return true;
        };
    const auto launch = fields.find("input.launch");
    if (launch != fields.end()) {
        auto parsed = parse_binding(launch->second);
        if (!parsed) {
            return false;
        }
        input.launch_keys = std::move(*parsed);
    }

    for (const auto& [key, value] : fields) {
        if (!key.starts_with("input.nudge.")) continue;
        if (key == "input.nudge.device") {
            auto name = unescape_value(trim(value));
            if (!name) return false;
            input.nudge.device = std::move(*name);
            continue;
        }
        int number = 0;
        if (!parse_integer(trim(value), number)) return false;
        if (key == "input.nudge.enabled" && (number == 0 || number == 1))
            input.nudge.enabled = number != 0;
        else if (key == "input.nudge.axis_x") input.nudge.axis_x = number;
        else if (key == "input.nudge.axis_y") input.nudge.axis_y = number;
        else if (key == "input.nudge.threshold") input.nudge.threshold = number;
        else if (key == "input.nudge.release") input.nudge.release = number;
        else if (key == "input.nudge.cooldown_ms") input.nudge.cooldown_ms = number;
        else return false;
    }
    if (!input.nudge.valid()) return false;

    const auto device = fields.find("input.plunger.device");
    if (device != fields.end()) {
        auto name = unescape_value(trim(device->second));
        if (!name) {
            return false;
        }
        input.plunger.device = std::move(*name);
    }
    // A percentage is range-checked and a raw axis value is not, because the
    // percentages are the port's own units and an out-of-range one is a typo,
    // while the endpoints belong to whatever controller the cabinet has and
    // this port does not get to say how wide its axis is.
    const auto parse_percent = [&fields](
                                   const std::string_view key,
                                   std::uint8_t& value) {
        std::string field("input.plunger.");
        field += key;
        const auto found = fields.find(field);
        if (found == fields.end()) {
            return true;
        }
        unsigned int raw = 0;
        if (!parse_integer(trim(found->second), raw) || raw > 100U) {
            return false;
        }
        value = static_cast<std::uint8_t>(raw);
        return true;
    };
    const auto parse_axis_value = [&fields](
                                      const std::string_view key,
                                      std::int32_t& value) {
        std::string field("input.plunger.");
        field += key;
        const auto found = fields.find(field);
        if (found == fields.end()) {
            return true;
        }
        return parse_integer(trim(found->second), value);
    };

    return parse_flipper("left_flipper", input.flippers.left)
        && parse_flipper("right_flipper", input.flippers.right)
        && parse_axis_value("axis", input.plunger.axis)
        && parse_axis_value("rest", input.plunger.calibration.rest)
        && parse_axis_value("drawn", input.plunger.calibration.drawn)
        && parse_percent("deadzone", input.plunger.calibration.deadzone)
        && parse_percent("release", input.plunger.calibration.release);
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

std::string_view pad_button_field(const PadButton button) noexcept
{
    switch (button) {
    case PadButton::one:
        return "one";
    case PadButton::two:
        return "two";
    case PadButton::select:
        return "select";
    case PadButton::run:
        return "run";
    case PadButton::up:
        return "up";
    case PadButton::right:
        return "right";
    case PadButton::down:
        return "down";
    case PadButton::left:
        return "left";
    }
    return "";
}

std::uint8_t pad_button_bit(const PadButton button) noexcept
{
    return static_cast<std::uint8_t>(
        1U << static_cast<unsigned>(static_cast<std::size_t>(button)));
}

std::uint8_t flipper_input_mask(
    const std::span<const PadButton> inputs) noexcept
{
    std::uint8_t mask = 0;
    for (const auto input : inputs) {
        mask = static_cast<std::uint8_t>(mask | pad_button_bit(input));
    }
    return mask;
}

bool rewrites_flipper_input_mask(
    const std::uint8_t current,
    const std::uint8_t cartridge_mask,
    const std::uint8_t desired) noexcept
{
    return desired != 0U && current != desired && current == cartridge_mask;
}

bool has_opposing_directions(const std::uint8_t pad) noexcept
{
    const auto set = [pad](const PadButton button) {
        return (pad & pad_button_bit(button)) != 0U;
    };
    return (set(PadButton::left) && set(PadButton::right))
        || (set(PadButton::up) && set(PadButton::down));
}

std::uint8_t resolve_opposing_directions(
    std::uint8_t pad,
    const FlipperInputs& flippers) noexcept
{
    if (!has_opposing_directions(pad)) {
        return pad;
    }
    for (const auto* const list : {&flippers.right, &flippers.left}) {
        if (list->size() < 2U) {
            continue;
        }
        const auto owned = flipper_input_mask(*list);
        if ((pad & owned) == 0U) {
            continue;
        }
        for (const auto input : *list) {
            const auto candidate = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(pad & ~owned)
                | pad_button_bit(input));
            if (!has_opposing_directions(candidate)) {
                return candidate;
            }
        }
    }
    return pad;
}

InputBindings default_input_bindings()
{
    InputBindings bindings;
    // Button I confirms -- it is what accepts a letter during high-score
    // entry -- and Return is the only key that should do that. Neither
    // flipper button sends it: both reach their flipper as a direction, so
    // both scroll a letter rather than accepting one.
    bindings[PadButton::one] = {"Return"};
    bindings[PadButton::two] = {"X"};
    bindings[PadButton::select] = {"Backspace"};
    bindings[PadButton::run] = {"Return"};
    bindings[PadButton::up] = {"Up"};
    // Both right-hand flipper keys. They reach the flipper through the right
    // pieces' rewritten mask rather than through button I, so on screens that
    // are not play they arrive as a direction and scroll rather than confirm.
    bindings[PadButton::right] = {"Right", "Right Shift"};
    bindings[PadButton::down] = {"Down"};
    bindings[PadButton::left] = {"Left Shift", "Left"};

    // The left flipper drops the cartridge's up/right/down, so Right and the
    // menu directions stop working it. The right flipper gains d-pad right
    // alongside button I, so Right works it while still arriving as a
    // direction on the screens that are not play.
    // Button I is dropped from the right flipper: nothing that works a
    // flipper sends it now, so leaving it would only let the confirm key flip
    // on every press.
    //
    // The right flipper additionally accepts d-pad up. A pad cannot be pushed
    // left and right at once and the cartridge zeroes the whole input byte
    // when it sees the attempt, so two flipper buttons on opposing directions
    // would silence each other. Listing a second, non-opposing input gives
    // the host somewhere to move this flipper for exactly those frames. Up is
    // never sent on its own by any flipper key; it is only ever the stand-in
    // for right while left is also held.
    bindings.flippers.left = {PadButton::left};
    bindings.flippers.right = {PadButton::right, PadButton::up};

    // Return is the cabinet's launch button. Listing it here makes it send
    // button I alone while the ball is in the shooter, so it launches instead
    // of pausing, and leaves it starting and pausing everywhere else.
    bindings.launch_keys = {"Return"};
    return bindings;
}

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
    append_bindings(text, config.input);

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

    // A hand-edited file can come back with a byte-order mark -- several
    // Windows editors add one on save -- and without this the mark becomes
    // part of the first key, so `version` goes missing and the whole file is
    // discarded. Since the documented way to change a binding is to edit this
    // file, that would silently cost the user their display assignment too.
    constexpr std::string_view kByteOrderMark = "\xEF\xBB\xBF";
    std::string_view body{text};
    if (body.starts_with(kByteOrderMark)) {
        body.remove_prefix(kByteOrderMark.size());
    }

    std::map<std::string, std::string, std::less<>> fields;
    std::size_t line_start = 0;
    while (line_start < body.size()) {
        const auto line_end = body.find('\n', line_start);
        auto line = body.substr(
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
    DisplayConfig config;
    if (version == fields.end()
        || !parse_integer(version->second, config.format_version)
        || config.format_version != kDisplayConfigVersion
        || !parse_placement(fields, "playfield", config.playfield)
        || !parse_placement(fields, "dmd", config.dmd)
        || !parse_placement(fields, "backglass", config.backglass)) {
        return std::nullopt;
    }
    // A file written before the backglass image was removed still carries
    // `backglass.path`. Unknown fields are ignored, so it stays loadable and
    // the line simply disappears the next time the file is written.
    if (!parse_bindings(fields, config.input)) {
        return std::nullopt;
    }
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
    const auto commit =
        devils_crush::core::commit_replacement(temporary_path, path);
    if (!commit.committed) {
        return {
            .saved = false,
            .message =
                "Could not atomically commit the cabinet configuration; "
                "Windows error "
                + std::to_string(commit.last_error) + " after "
                + std::to_string(commit.retries) + " retries.",
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
