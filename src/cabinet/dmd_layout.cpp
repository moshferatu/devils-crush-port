// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/dmd_layout.hpp"

#include <algorithm>

namespace devils_crush::cabinet {
namespace {

// Total width of `count` digits expressed in digit heights, so that a width
// budget divides straight into a height.
[[nodiscard]] float span_in_heights(const std::size_t count) noexcept
{
    const auto digits = static_cast<float>(count);
    const auto gap = kDmdDigitAspect * kDmdGapFraction;
    return digits * kDmdDigitAspect + (digits - 1.0F) * gap;
}

} // namespace

DmdDigitLayout calculate_dmd_digit_layout(
    const int output_width,
    const int output_height,
    const std::size_t digit_count) noexcept
{
    if (output_width <= 0 || output_height <= 0 || digit_count == 0U) {
        return {};
    }

    const auto width = static_cast<float>(output_width);
    const auto height = static_cast<float>(output_height);

    // A score wider than the nominal field still has to fit, so the field
    // grows to hold it rather than overflowing.
    const auto field_digits = std::max(digit_count, kDmdScoreFieldDigits);
    const auto field_span = span_in_heights(field_digits);

    const auto digit_height = std::min(
        height * kDmdHeightUsage,
        width * kDmdWidthUsage / field_span);
    if (digit_height <= 0.0F) {
        return {};
    }

    DmdDigitLayout layout;
    layout.digit_height = digit_height;
    layout.digit_width = digit_height * kDmdDigitAspect;
    layout.gap = layout.digit_width * kDmdGapFraction;

    // The drawn digits are centred, not the field, so a short score sits in
    // the middle of the window rather than off to one side.
    const auto drawn_width = digit_height * span_in_heights(digit_count);
    layout.origin_x = (width - drawn_width) / 2.0F;
    layout.origin_y = (height - digit_height) / 2.0F;
    return layout;
}

} // namespace devils_crush::cabinet
