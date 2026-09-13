// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cabinet/dmd_font.hpp"

#include "devils_crush/render/pce_sprite_overlay.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace devils_crush::cabinet {
namespace {

constexpr std::uint8_t kFontPhysicalBank = 0x11U;
constexpr std::uint16_t kFontSourceAddress = 0xA9C0U;
constexpr std::size_t kWordsPerBackgroundTile = 16U;
constexpr std::size_t kFontGlyphCount = 36U;
constexpr std::size_t kRequiredFontWords =
    kFontGlyphCount * kWordsPerBackgroundTile;
constexpr std::size_t kMaximumFontStreamWords = 0x0700U;

[[nodiscard]] std::optional<std::size_t> glyph_index(
    char character) noexcept
{
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }
    if (character >= '0' && character <= '9') {
        return static_cast<std::size_t>(character - '0');
    }
    if (character >= 'A' && character <= 'Z') {
        return 10U + static_cast<std::size_t>(character - 'A');
    }
    return std::nullopt;
}

} // namespace

std::optional<DmdFont> DmdFont::decode(
    const std::span<const std::byte> rom_bytes)
{
    // The setup call observed at 00:F85A enters the generic stream decoder
    // with MPR5=$11 and source $A9C0, then uploads the result at VRAM $7900.
    std::array<std::uint8_t, 8> mpr{};
    mpr[5] = kFontPhysicalBank;
    const auto words = render::decode_pce_vram_stream(
        rom_bytes,
        mpr,
        kFontSourceAddress,
        kMaximumFontStreamWords);
    if (!words || words->size() < kRequiredFontWords) {
        return std::nullopt;
    }

    DmdFont font;
    for (std::size_t glyph = 0; glyph < font.glyphs_.size(); ++glyph) {
        const auto tile_word = glyph * kWordsPerBackgroundTile;
        bool has_pixels = false;
        for (int row = 0; row < kDmdGlyphHeight; ++row) {
            const auto plane_01 =
                (*words)[tile_word + static_cast<std::size_t>(row)];
            const auto plane_23 =
                (*words)[tile_word + 8U + static_cast<std::size_t>(row)];
            std::uint8_t mask = 0U;
            for (int column = 0; column < kDmdGlyphWidth; ++column) {
                const auto bit = static_cast<unsigned>(7 - column);
                const auto color = static_cast<std::uint16_t>(
                    ((plane_01 >> bit) & 0x01U)
                    | (((plane_01 >> (bit + 8U)) & 0x01U) << 1U)
                    | (((plane_23 >> bit) & 0x01U) << 2U)
                    | (((plane_23 >> (bit + 8U)) & 0x01U) << 3U));
                if (color != 0U) {
                    mask = static_cast<std::uint8_t>(
                        mask | (1U << bit));
                }
            }
            font.glyphs_[glyph][static_cast<std::size_t>(row)] = mask;
            has_pixels = has_pixels || mask != 0U;
        }
        if (!has_pixels) {
            return std::nullopt;
        }
    }
    // 09:73BC uploads asset 0C:8006 to VRAM $2800. 09:7390
    // selects the four background tiles composing each HUD digit.
    mpr[4] = 0x0CU;
    mpr[5] = 0x0DU;
    const auto hud = render::decode_pce_vram_stream(
        rom_bytes, mpr, 0x8006U, 0x1000U);
    constexpr std::size_t digit_map = 0x09U * 8192U + 0x1390U;
    // Table palette asset 10:0030 -> 11:A1D9, background palette F.
    constexpr std::size_t palette = 0x221D9U + 0xF0U * 2U;
    if (!hud || hud->size() != 0x1000U
        || rom_bytes.size() < palette + 32U) {
        return std::nullopt;
    }
    for (unsigned digit = 0; digit < 10U; ++digit) {
        for (unsigned y = 0; y < 16U; ++y) {
            for (unsigned x = 0; x < 16U; ++x) {
                const auto tile = std::to_integer<unsigned>(rom_bytes[
                    digit_map + digit * 4U + (y / 8U) * 2U + x / 8U]);
                if (tile < 0x80U || tile > 0xFFU) {
                    return std::nullopt;
                }
                const auto offset = (tile - 0x80U) * 16U + y % 8U;
                const auto a = (*hud)[offset];
                const auto b = (*hud)[offset + 8U];
                const auto bit = 7U - x % 8U;
                const auto index = ((a >> bit) & 1U)
                    | (((a >> (bit + 8U)) & 1U) << 1U)
                    | (((b >> bit) & 1U) << 2U)
                    | (((b >> (bit + 8U)) & 1U) << 3U);
                const auto color_offset = palette + index * 2U;
                const auto raw = std::to_integer<unsigned>(rom_bytes[color_offset])
                    | (std::to_integer<unsigned>(rom_bytes[color_offset + 1U]) << 8U);
                font.score_digits_[digit][y * 16U + x] = index == 0U ? 0U
                    : render::decode_pce_color(static_cast<std::uint16_t>(raw));
                const auto special_offset = color_offset - 32U;
                const auto special_raw = std::to_integer<unsigned>(rom_bytes[special_offset])
                    | (std::to_integer<unsigned>(rom_bytes[special_offset + 1U]) << 8U);
                font.special_digits_[digit][y * 16U + x] = index == 0U ? 0U
                    : render::decode_pce_color(static_cast<std::uint16_t>(special_raw));
            }
        }
    }
    return font;
}

const DmdScoreDigit& DmdFont::score_digit(
    const unsigned digit, const bool special_points) const
{
    return special_points ? special_digits_.at(digit) : score_digits_.at(digit);
}

std::optional<DmdGlyph> DmdFont::glyph(const char character) const noexcept
{
    if (character == ' ') {
        return DmdGlyph{};
    }
    if (character == '#') {
        return DmdGlyph{
            0x24U,
            0x24U,
            0x7EU,
            0x24U,
            0x7EU,
            0x24U,
            0x24U,
            0x00U,
        };
    }
    if (character == ',') {
        return DmdGlyph{
            0x00U,
            0x00U,
            0x00U,
            0x00U,
            0x00U,
            0x30U,
            0x30U,
            0x20U,
        };
    }
    const auto index = glyph_index(character);
    if (!index || *index >= glyphs_.size()) {
        return std::nullopt;
    }
    return glyphs_[*index];
}

} // namespace devils_crush::cabinet
