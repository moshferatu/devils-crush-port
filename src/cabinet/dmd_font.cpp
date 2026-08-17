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
    return font;
}

std::optional<DmdGlyph> DmdFont::glyph(const char character) const noexcept
{
    if (character == ' ') {
        return DmdGlyph{};
    }
    const auto index = glyph_index(character);
    if (!index || *index >= glyphs_.size()) {
        return std::nullopt;
    }
    return glyphs_[*index];
}

} // namespace devils_crush::cabinet
