// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace devils_crush::cabinet {

inline constexpr std::size_t kDmdTextCount = 3U;
inline constexpr int kDmdGlyphWidth = 5;
inline constexpr int kDmdGlyphHeight = 7;
inline constexpr int kDmdGlyphAdvance = 6;

// Fixed reserves keep either tier from changing size as its values gain
// digits. The score is nine digits at its verified maximum; "Bonus 9900" is
// the longest lower-row field the game exposes.
inline constexpr std::size_t kDmdMaximumScoreCharacters = 9U;
inline constexpr std::size_t kDmdMaximumStatusCharacters = 10U;

struct DmdTextContent {
    // Score, Ball, Bonus. Ball and Bonus share the lower visual row.
    std::array<std::string, kDmdTextCount> text;
};

struct DmdTextLineLayout {
    int origin_x = 0;
    int origin_y = 0;
    int width = 0;
    int cell_size = 0;
    int glyph_height = 0;
    int character_advance = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return cell_size > 0 && glyph_height > 0
            && character_advance > 0;
    }
};

struct DmdTextLayout {
    std::array<DmdTextLineLayout, kDmdTextCount> lines{};

    [[nodiscard]] bool valid() const noexcept
    {
        return lines[0].valid() && lines[1].valid() && lines[2].valid();
    }
};

// Builds the exact user-facing text. Values are already expressed in display
// units; the decoder owns the cartridge-specific representation.
[[nodiscard]] DmdTextContent make_dmd_text_content(
    std::uint64_t displayed_score,
    std::uint32_t bonus_points,
    std::uint8_t current_ball);

// Centers a large score in the upper two thirds. Ball and Bonus use one
// shared, substantially smaller size in the left and right halves of the
// lower third. All bitmap cells remain square.
[[nodiscard]] DmdTextLayout calculate_dmd_text_layout(
    int output_width,
    int output_height,
    const DmdTextContent& content) noexcept;

} // namespace devils_crush::cabinet
