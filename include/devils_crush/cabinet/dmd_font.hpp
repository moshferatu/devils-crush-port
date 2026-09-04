// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::cabinet {

inline constexpr int kDmdGlyphWidth = 8;
inline constexpr int kDmdGlyphHeight = 8;
inline constexpr int kDmdGlyphAdvance = 8;
inline constexpr int kDmdCommaAdvance = 6;

[[nodiscard]] constexpr int dmd_character_advance_cells(
    const char character,
    const char next_character = '\0') noexcept
{
    if (character == ',') {
        return kDmdCommaAdvance;
    }
    return kDmdGlyphAdvance - (next_character == ',' ? 1 : 0);
}

using DmdGlyph = std::array<std::uint8_t, kDmdGlyphHeight>;

// The cartridge's resident 8-by-8 alphanumeric font. The source is the same
// compressed stream that table setup uploads to VRAM $7900; decoding it from
// the user's ROM keeps the DMD faithful without adding a host font dependency.
class DmdFont {
public:
    [[nodiscard]] static std::optional<DmdFont> decode(
        std::span<const std::byte> rom_bytes);

    // The game stores upper-case forms only. Lower-case label characters map
    // to those same original glyphs, matching the cartridge's presentation;
    // the host also supplies the punctuation needed by its score and rank
    // labels.
    [[nodiscard]] std::optional<DmdGlyph> glyph(char character) const noexcept;

private:
    static constexpr std::size_t kGlyphCount = 36U;
    std::array<DmdGlyph, kGlyphCount> glyphs_{};
};

} // namespace devils_crush::cabinet
