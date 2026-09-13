// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cabinet/dmd_panel.hpp"
#include "devils_crush/cabinet/dmd_layout.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace devils_crush::cabinet {
namespace {
constexpr int width = 320;
constexpr int height = 112;

void pixel(render::ArgbImage& image, int x, int y, std::uint32_t color)
{
    if (x >= 0 && x < width && y >= 0 && y < height && (color >> 24U)) {
        image.pixels[static_cast<std::size_t>(y * width + x)] = color;
    }
}

void label(render::ArgbImage& image, const DmdFont& font,
    std::string_view text, int center, int y, std::uint32_t color = 0xFFDDD4E8U)
{
    int x = center - static_cast<int>(text.size()) * 4;
    for (const char c : text) {
        if (const auto glyph = font.glyph(c)) {
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 8; ++col) {
                    if ((*glyph)[static_cast<std::size_t>(row)] & (1U << (7 - col))) {
                        pixel(image, x + col, y + row, color);
                    }
                }
            }
        }
        x += 8;
    }
}

void number(render::ArgbImage& image, const DmdFont& font,
    std::uint64_t value, int center, int y, int scale, unsigned places = 0,
    bool special_points = false)
{
    auto text = std::to_string(value);
    if (text.size() < places) text.insert(0, places - text.size(), '0');
    const int extent = (static_cast<int>(text.size()) * 14 + 2) * scale;
    int x = center - extent / 2;
    for (const char c : text) {
        const auto& digit = font.score_digit(static_cast<unsigned>(c - '0'), special_points);
        for (int row = 0; row < 16 * scale; ++row) {
            for (int col = 0; col < 16 * scale; ++col) {
                pixel(image, x + col, y + row,
                    digit[static_cast<std::size_t>((row / scale) * 16 + col / scale)]);
            }
        }
        x += 14 * scale;
    }
}
}

void DmdPanel::begin_bonus_result() noexcept
{
    result_active_ = true;
    result_won_ = false;
    result_counting_ = false;
}

void DmdPanel::observe(const std::span<const std::byte> ram)
{
    const auto decoded = decode_cabinet_state(ram);
    if (!decoded) return;
    state_ = *decoded;
    if (state_.current_ball != 0U) last_ball_ = state_.current_ball;
    const auto byte = [&ram](std::size_t offset) {
        return std::to_integer<unsigned>(ram[offset]);
    };
    const auto points = [&byte](std::size_t offset) {
        return static_cast<std::uint64_t>(byte(offset)
            | (byte(offset + 1U) << 8U) | (byte(offset + 2U) << 16U)) * 100U;
    };
    demo_ = byte(0x254U) != 0U;
    const auto flags = byte(0x53U);
    if (!state_.bonus_stage) result_active_ = false;
    scene_ = DmdScene::score;
    if ((state_.screen_selector == 0U || state_.screen_selector == 2U)
        && (flags & 0x80U)) {
        scene_ = DmdScene::extra_ball;
    } else if (state_.screen_selector == 0U && (flags & 0x04U)) {
        scene_ = DmdScene::special_points;
        award_ = static_cast<std::uint64_t>(byte(0xCAU)) * 1'000'000U;
    } else if (state_.screen_selector == 2U) {
        scene_ = byte(0xD71U) ? DmdScene::match : DmdScene::bonus_tally;
        award_ = scene_ == DmdScene::match ? (byte(0x5DU) % 10U) * 100U
                                         : points(0xD69U);
        targets_ = byte(0xD4FU);
        multiplier_ = byte(0xD51U);
    } else if (state_.bonus_stage && result_active_) {
        scene_ = DmdScene::bonus_result;
        award_ = points(0xDB7U);
        result_won_ = result_won_ || award_ != 0U;
        perfect_ = byte(0xDBAU) != 0U;
        result_counting_ = result_counting_ || byte(0xD66U) == 0U;
        special_result_ = perfect_ && result_counting_ && byte(0xD66U) != 0U;
    }
}

bool DmdPanel::holds_playfield() const noexcept
{
    return state_.screen_selector == 2U || scene_ == DmdScene::bonus_result;
}

bool DmdPanel::attract() const noexcept
{
    return state_.screen_selector == 3U || state_.screen_selector == 4U || demo_;
}

render::ArgbImage DmdPanel::compose(const DmdFont& font,
    const core::HighScoreTable* high_scores, const std::uint64_t elapsed) const
{
    render::ArgbImage image{width, height,
        std::vector<std::uint32_t>(width * height, 0xFF000000U)};
    if (attract() && high_scores) {
        const auto rank = dmd_high_score_rank(elapsed);
        label(image, font, rank == 0U ? "GRAND CHAMPION"
            : "HIGH SCORE #" + std::to_string(rank), 160, 6, 0xFFFCE024U);
        label(image, font, core::high_score_name_at(*high_scores, rank), 160, 28);
        number(image, font, static_cast<std::uint64_t>(
            core::high_score_at(*high_scores, rank)) * 100U, 160, 46, 2);
        label(image, font, demo_ ? "DEMO   PRESS START" : "PRESS START", 160, 94);
        return image;
    }
    if (scene_ == DmdScene::score) {
        label(image, font, state_.balls_in_session == 0U ? "GAME OVER" : "SCORE", 160, 6);
        number(image, font, state_.displayed_score, 160, 22, 2);
        label(image, font, "BONUS", 88, 73);
        number(image, font, state_.bonus_points, 88, 87, 1);
        label(image, font, "BALL", 250, 73);
        number(image, font, last_ball_, 250, 87, 1);
        return image;
    }
    std::string heading;
    std::string detail;
    switch (scene_) {
    case DmdScene::special_points: heading = "SPECIAL POINTS"; break;
    case DmdScene::extra_ball: heading = "EXTRA BALL"; break;
    case DmdScene::bonus_tally:
        heading = "BONUS POINTS";
        detail = std::to_string(state_.bonus_points) + " X "
            + std::to_string(targets_) + " X " + std::to_string(multiplier_);
        break;
    case DmdScene::match: heading = "MATCH"; break;
    case DmdScene::bonus_result:
        heading = !result_won_ ? "SORRY" : special_result_ ? "PERFECT" : "YOU GET";
        detail = !result_won_ ? "YOU LOST YOUR BONUS POINTS"
            : special_result_ ? "SPECIAL POINTS" : "BONUS POINTS";
        break;
    case DmdScene::score: break;
    }
    label(image, font, heading, 160, 6, 0xFF48FC48U);
    if (scene_ == DmdScene::extra_ball) {
        label(image, font, "SHOOT AGAIN", 160, 36);
    } else if (scene_ != DmdScene::bonus_result || result_won_) {
        number(image, font, award_, 160, 22, 2, scene_ == DmdScene::match ? 3U : 0U,
            scene_ == DmdScene::special_points
                || (scene_ == DmdScene::bonus_result && special_result_));
    }
    label(image, font, detail, 160, 65);
    label(image, font, "SCORE", 58, 91);
    number(image, font, state_.displayed_score, 216, 86, 1);
    return image;
}

} // namespace devils_crush::cabinet
