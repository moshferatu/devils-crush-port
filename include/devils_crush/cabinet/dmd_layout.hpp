// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

namespace devils_crush::cabinet {

// The score field is sized for the widest score the game can reach so that
// digit size does not change as the score grows. The verified maximum visible
// score is 999,999,900: nine digits. The three-byte stored score could hold
// more than the game ever awards, so `calculate_dmd_digit_layout` widens the
// field rather than overflowing if a longer value is ever decoded.
inline constexpr std::size_t kDmdScoreFieldDigits = 9U;

// A seven-segment digit is three units wide by five tall, and neighbours are
// separated by a fifth of a digit width.
inline constexpr float kDmdDigitAspect = 0.6F;
inline constexpr float kDmdGapFraction = 0.2F;

// The share of the window the field may occupy. The remainder is the quiet
// border a real DMD carries.
inline constexpr float kDmdWidthUsage = 0.9F;
inline constexpr float kDmdHeightUsage = 0.75F;

struct DmdDigitLayout {
    float origin_x = 0.0F;
    float origin_y = 0.0F;
    float digit_width = 0.0F;
    float digit_height = 0.0F;
    float gap = 0.0F;

    // The advance from one digit's left edge to the next.
    [[nodiscard]] float pitch() const noexcept
    {
        return digit_width + gap;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return digit_width > 0.0F && digit_height > 0.0F;
    }
};

// Places `digit_count` seven-segment digits centred in an output of the given
// size.
//
// The height is bounded by both axes. Taking only the height bound is what
// made a fullscreen DMD illegible: at 0.75 of a 1080-pixel panel a digit is
// 810 pixels tall, and a full score at that size is several times wider than
// any panel it is drawn on, so the leading digits fall off the left edge and
// the trailing ones off the right.
//
// Digits are sized against `kDmdScoreFieldDigits` rather than the count
// actually being drawn. Sizing against the live count would have the score
// shrink each time it gains a digit, which on a scoreboard reads as a fault.
[[nodiscard]] DmdDigitLayout calculate_dmd_digit_layout(
    int output_width,
    int output_height,
    std::size_t digit_count) noexcept;

} // namespace devils_crush::cabinet
