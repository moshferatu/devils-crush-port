// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/dmd_layout.hpp"

#include <algorithm>
#include <string>

namespace devils_crush::cabinet {

DmdTextContent make_dmd_text_content(
    const std::uint64_t displayed_score,
    const std::uint32_t bonus_points,
    const std::uint8_t current_ball)
{
    return DmdTextContent{
        .text = {
            std::to_string(displayed_score),
            "Ball " + std::to_string(static_cast<unsigned>(current_ball)),
            "Bonus " + std::to_string(bonus_points),
        },
    };
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
        const int advance = kDmdGlyphAdvance * cell_size;
        const int width =
            (static_cast<int>(text.size()) - 1) * advance
            + kDmdGlyphWidth * cell_size;
        const int height = kDmdGlyphHeight * cell_size;
        return DmdTextLineLayout{
            .origin_x = center_x - width / 2,
            .origin_y = center_y - height / 2,
            .width = width,
            .cell_size = cell_size,
            .glyph_height = height,
            .character_advance = advance,
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

} // namespace devils_crush::cabinet
