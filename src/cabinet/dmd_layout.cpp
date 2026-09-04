// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/dmd_layout.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace devils_crush::cabinet {
namespace {

[[nodiscard]] std::string format_number(const std::uint64_t value)
{
    auto text = std::to_string(value);
    for (std::size_t separator = text.size(); separator > 3U;
         separator -= 3U) {
        text.insert(separator - 3U, 1U, ',');
    }
    return text;
}

[[nodiscard]] int text_width_cells(const std::string& text) noexcept
{
    if (text.empty()) {
        return 0;
    }
    int width = kDmdGlyphWidth;
    for (std::size_t index = 0; index + 1U < text.size(); ++index) {
        width += dmd_character_advance_cells(
            text[index], text[index + 1U]);
    }
    return width;
}

} // namespace

DmdTextContent make_dmd_text_content(
    const std::uint64_t displayed_score,
    const std::uint32_t bonus_points,
    const std::uint8_t current_ball)
{
    return DmdTextContent{
        .text = {
            format_number(displayed_score),
            "Ball " + std::to_string(static_cast<unsigned>(current_ball)),
            "Bonus " + format_number(bonus_points),
        },
    };
}

DmdTextContent make_dmd_high_score_content(
    const core::HighScoreTable& table,
    const std::size_t rank)
{
    if (rank >= core::kHighScoreRankCount) {
        return {};
    }
    return DmdTextContent{
        .text = {
            rank == 0U
                ? "Grand Champion"
                : "High Score #" + std::to_string(rank),
            core::high_score_name_at(table, rank),
            format_number(
                static_cast<std::uint64_t>(
                    core::high_score_at(table, rank))
                * 100U),
        },
    };
}

std::size_t dmd_high_score_rank(
    const std::uint64_t elapsed_milliseconds) noexcept
{
    return static_cast<std::size_t>(
        elapsed_milliseconds / kDmdHighScoreDurationMilliseconds
        % core::kHighScoreRankCount);
}

DmdTextLayout calculate_dmd_text_layout(
    const int output_width,
    const int output_height,
    const DmdTextContent& content) noexcept
{
    if (output_width <= 0 || output_height <= 0) {
        return {};
    }

    if (content.text[0].empty()
        || content.text[0].size() > kDmdMaximumScoreCharacters
        || content.text[1].empty()
        || content.text[1].size() > kDmdMaximumStatusCharacters
        || content.text[2].empty()
        || content.text[2].size() > kDmdMaximumStatusCharacters) {
        return {};
    }

    constexpr int kScoreReservedCells =
        (static_cast<int>(kDmdMaximumScoreCharacters) - 1)
            * kDmdGlyphAdvance
        + kDmdGlyphWidth;
    constexpr int kStatusReservedCells =
        (static_cast<int>(kDmdMaximumStatusCharacters) - 1)
            * kDmdGlyphAdvance
        + kDmdGlyphWidth;

    const int lower_height = output_height / 3;
    const int upper_height = output_height - lower_height;
    const int total_width_budget = output_width - output_width / 10;
    const int score_width_cell = total_width_budget / kScoreReservedCells;
    const int score_height_budget = upper_height - upper_height / 5;
    const int score_height_cell = score_height_budget / kDmdGlyphHeight;
    const int score_cell_size =
        std::min(score_width_cell, score_height_cell);

    const int status_region_width = output_width / 2;
    const int status_width_budget =
        status_region_width - status_region_width / 10;
    const int status_width_cell = status_width_budget / kStatusReservedCells;
    const int status_height_budget = lower_height * 3 / 5;
    const int status_height_cell = status_height_budget / kDmdGlyphHeight;
    const int status_cell_size = std::min(
        {status_width_cell, status_height_cell, score_cell_size / 2});
    if (score_cell_size <= 0 || status_cell_size <= 0) {
        return {};
    }

    const auto make_line = [](
                               const std::string& text,
                               const int cell_size,
                               const int center_x,
                               const int center_y) {
        const int width = text_width_cells(text) * cell_size;
        const int height = kDmdGlyphHeight * cell_size;
        return DmdTextLineLayout{
            .origin_x = center_x - width / 2,
            .origin_y = center_y - height / 2,
            .width = width,
            .cell_size = cell_size,
            .glyph_height = height,
        };
    };

    DmdTextLayout layout{};
    layout.lines[0] = make_line(
        content.text[0], score_cell_size, output_width / 2, upper_height / 2);
    const int status_center_y = upper_height + lower_height / 2;
    layout.lines[1] = make_line(
        content.text[1],
        status_cell_size,
        status_region_width / 2,
        status_center_y);
    layout.lines[2] = make_line(
        content.text[2],
        status_cell_size,
        status_region_width + (output_width - status_region_width) / 2,
        status_center_y);

    for (const auto& line : layout.lines) {
        if (line.origin_x < 0 || line.origin_y < 0
            || line.origin_x + line.width > output_width
            || line.origin_y + line.glyph_height > output_height) {
            return {};
        }
    }
    return layout;
}

DmdTextLayout calculate_dmd_high_score_layout(
    const int output_width,
    const int output_height,
    const DmdTextContent& content) noexcept
{
    if (output_width <= 0 || output_height <= 0
        || content.text[0].empty()
        || content.text[0].size() > kDmdMaximumHighScoreLabelCharacters
        || content.text[1].empty() || content.text[1].size() > 3U
        || content.text[2].empty()
        || content.text[2].size() > kDmdMaximumScoreCharacters) {
        return {};
    }

    constexpr std::array<std::size_t, kDmdTextCount> kReservedCharacters{
        kDmdMaximumHighScoreLabelCharacters,
        3U,
        kDmdMaximumScoreCharacters,
    };
    const int row_height = output_height / 3;
    const int width_budget = output_width - output_width / 10;
    DmdTextLayout layout{};
    for (std::size_t line = 0; line < kDmdTextCount; ++line) {
        const int reserved_cells =
            (static_cast<int>(kReservedCharacters[line]) - 1)
                * kDmdGlyphAdvance
            + kDmdGlyphWidth;
        const int width_cell = width_budget / reserved_cells;
        const int height_budget = line == 2U
            ? row_height * 4 / 5
            : row_height * 3 / 5;
        const int cell_size =
            std::min(width_cell, height_budget / kDmdGlyphHeight);
        if (cell_size <= 0) {
            return {};
        }
        const int width = text_width_cells(content.text[line]) * cell_size;
        const int glyph_height = kDmdGlyphHeight * cell_size;
        layout.lines[line] = DmdTextLineLayout{
            .origin_x = (output_width - width) / 2,
            .origin_y = static_cast<int>(line) * row_height
                + (row_height - glyph_height) / 2,
            .width = width,
            .cell_size = cell_size,
            .glyph_height = glyph_height,
        };
    }

    for (const auto& line : layout.lines) {
        if (line.origin_x < 0 || line.origin_y < 0
            || line.origin_x + line.width > output_width
            || line.origin_y + line.glyph_height > output_height) {
            return {};
        }
    }
    return layout;
}

} // namespace devils_crush::cabinet
