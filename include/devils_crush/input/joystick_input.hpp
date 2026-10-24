// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <utility>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace devils_crush::input {

// A controller input bound to a pad input, written in the same list as the
// keys. It exists for cabinets whose board reports its buttons as a joystick
// rather than as keystrokes -- a PinOne in its own controller mode, as VPX and
// Pinball FX read it -- and for gamepads and arcade sticks whose fixed layout
// is not how the cabinet is wired. A controller reports three kinds of input,
// and each is written its own way:
//
//   "PinOne Button 3"       a button
//   "Xbox Controller Hat 1 Left"   a direction of a hat, which is how SDL
//                           reports a d-pad or an arcade stick's lever
//   "DualSense Axis 5+"     an axis pushed past half way in one direction,
//                           which is how SDL reports a trigger or a stick
//
// The device is matched against the controller's name as a case-insensitive
// substring, as the plunger and nudge are, so it survives the board being
// unplugged or renumbered; with no device ("Button 3") any controller
// matches. Numbers count from 1, as Windows' own controller panel counts
// buttons; SDL counts from 0.
//
// No SDL key name ends in "Button <n>", "Hat <n> <direction>" or
// "Axis <n>+/-", so the two kinds of entry cannot be confused.
struct JoystickInput {
    enum class Kind { button, hat, axis };

    std::string device;
    Kind kind = Kind::button;
    int index = 0;      // SDL's index, from 0
    int direction = 0;  // hat: one `kHat` bit; axis: +1 or -1

    [[nodiscard]] bool operator==(const JoystickInput&) const = default;
};

// SDL's own `SDL_HAT_*` values, so a hat's state can be tested directly.
inline constexpr int kHatUp = 0x01;
inline constexpr int kHatRight = 0x02;
inline constexpr int kHatDown = 0x04;
inline constexpr int kHatLeft = 0x08;

inline constexpr int kJoystickIndexMax = 128;

// An axis counts as pressed past half its travel from centre. A trigger rests
// at the bottom of its axis, so for one that is three quarters of a pull.
inline constexpr int kAxisPressed = 16384;
// What the launcher waits for before taking an axis: well past the point the
// game reads it as pressed, so a binding it takes always works.
inline constexpr int kAxisCaptured = 24576;

[[nodiscard]] inline bool axis_pressed(const int direction, const int value) noexcept
{
    return direction > 0 ? value > kAxisPressed : value < -kAxisPressed;
}

namespace joystick_input_detail {

constexpr std::array<std::pair<std::string_view, int>, 4> kHatNames{{
    {"Up", kHatUp}, {"Right", kHatRight}, {"Down", kHatDown}, {"Left", kHatLeft}}};

[[nodiscard]] inline bool is_space(const char c) noexcept
{
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] inline bool same_ignoring_case(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i]))
            != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline std::string_view trim(std::string_view text) noexcept
{
    while (!text.empty() && is_space(text.front())) text.remove_prefix(1U);
    while (!text.empty() && is_space(text.back())) text.remove_suffix(1U);
    return text;
}

// Takes "<word> <number>" off the end of `text`, leaving what was before it
// (with its separating space). The word must start the text or follow a space.
[[nodiscard]] inline std::optional<int> take_word_number(
    std::string_view& text, const std::string_view word)
{
    std::size_t digits = text.size();
    while (digits > 0 && std::isdigit(static_cast<unsigned char>(text[digits - 1U])) != 0) --digits;
    if (digits == text.size()) return std::nullopt;
    int number = 0;
    const auto parsed = std::from_chars(text.data() + digits, text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || number < 1 || number > kJoystickIndexMax) return std::nullopt;

    std::size_t end = digits;
    while (end > 0 && is_space(text[end - 1U])) --end;
    if (end == digits || end < word.size()) return std::nullopt;  // "Button3"
    const std::size_t start = end - word.size();
    if (!same_ignoring_case(text.substr(start, word.size()), word)) return std::nullopt;
    if (start > 0 && !is_space(text[start - 1U])) return std::nullopt;  // "PinOneButton 3"
    text = text.substr(0, start);
    return number;
}

}  // namespace joystick_input_detail

[[nodiscard]] inline std::optional<JoystickInput> parse_joystick_input(std::string_view text)
{
    using namespace joystick_input_detail;
    text = trim(text);
    JoystickInput input;

    if (!text.empty() && (text.back() == '+' || text.back() == '-')) {
        input.kind = JoystickInput::Kind::axis;
        input.direction = text.back() == '+' ? 1 : -1;
        text.remove_suffix(1U);
        const auto number = take_word_number(text, "Axis");
        if (!number) return std::nullopt;
        input.index = *number - 1;
    } else {
        std::size_t word = text.size();
        while (word > 0 && !is_space(text[word - 1U])) --word;
        const auto last = text.substr(word);
        int hat = 0;
        for (const auto& [name, bit] : kHatNames) {
            if (same_ignoring_case(last, name)) hat = bit;
        }
        if (hat != 0) {
            std::string_view rest = trim(text.substr(0, word));
            const auto number = take_word_number(rest, "Hat");
            if (!number) return std::nullopt;
            input.kind = JoystickInput::Kind::hat;
            input.direction = hat;
            input.index = *number - 1;
            text = rest;
        } else {
            const auto number = take_word_number(text, "Button");
            if (!number) return std::nullopt;
            input.index = *number - 1;
        }
    }
    input.device = std::string(trim(text));
    return input;
}

[[nodiscard]] inline std::string format_joystick_input(const JoystickInput& input)
{
    std::string text = input.device;
    if (!text.empty()) text += ' ';
    switch (input.kind) {
    case JoystickInput::Kind::button:
        text += "Button " + std::to_string(input.index + 1);
        break;
    case JoystickInput::Kind::hat:
        text += "Hat " + std::to_string(input.index + 1) + ' ';
        for (const auto& [name, bit] : joystick_input_detail::kHatNames) {
            if (bit == input.direction) text += name;
        }
        break;
    case JoystickInput::Kind::axis:
        text += "Axis " + std::to_string(input.index + 1) + (input.direction > 0 ? '+' : '-');
        break;
    }
    return text;
}

}  // namespace devils_crush::input
