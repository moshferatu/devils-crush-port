// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/cabinet/dmd_font.hpp"
#include "devils_crush/core/high_score_table.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace devils_crush::cabinet {

inline constexpr std::size_t kDmdTextCount = 3U;
// Fixed reserves keep either tier from changing size as its values gain
// digits. The score is nine digits at its verified maximum; "Bonus 9900" is
// the longest lower-row field the game exposes.
inline constexpr std::size_t kDmdMaximumScoreCharacters = 9U;
inline constexpr std::size_t kDmdMaximumStatusCharacters = 10U;
inline constexpr std::size_t kDmdMaximumHighScoreLabelCharacters = 14U;
inline constexpr std::uint64_t kDmdHighScoreDurationMilliseconds = 3'000U;

struct DmdTextContent {
    // Three display lines. Their placement depends on the selected layout.
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

// Builds one page of the startup high-score carousel. Rank zero is the
// Grand Champion; the remaining four entries follow pinball convention and
// are numbered High Score #1 through High Score #4.
[[nodiscard]] DmdTextContent make_dmd_high_score_content(
    const core::HighScoreTable& table,
    std::size_t rank);

// Selects one of the five high-score pages from elapsed wall-clock time.
[[nodiscard]] std::size_t dmd_high_score_rank(
    std::uint64_t elapsed_milliseconds) noexcept;

// Centers a large score in the upper two thirds. Ball and Bonus use one
// shared, substantially smaller size in the left and right halves of the
// lower third. All bitmap cells remain square.
[[nodiscard]] DmdTextLayout calculate_dmd_text_layout(
    int output_width,
    int output_height,
    const DmdTextContent& content) noexcept;

// Centers the rank label, initials, and score on three stacked rows. The
// score is the largest row, while every bitmap cell remains square.
[[nodiscard]] DmdTextLayout calculate_dmd_high_score_layout(
    int output_width,
    int output_height,
    const DmdTextContent& content) noexcept;

} // namespace devils_crush::cabinet
