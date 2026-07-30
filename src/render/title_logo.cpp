// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/title_logo.hpp"

#include "devils_crush/render/pce_sprite_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace devils_crush::render {
namespace {

constexpr std::size_t kVramWordCount = 32768U;
constexpr std::size_t kPaletteEntryCount = 512U;
constexpr std::size_t kPaletteColors = 16U;
constexpr std::size_t kWarmPaletteRomOffset = 0x03E333U;
constexpr std::size_t kBrightPaletteRomOffset = 0x03E353U;
constexpr std::size_t kEyePaletteRomOffset = 0x03E393U;
// Keep the complete logo through source row 156 with a three-pixel lower
// margin. The right edge stops after the logo's flourish at column 228 and
// before the trademark glyphs begin at column 232.
constexpr int kLogoSourceX = 16;
constexpr int kLogoSourceY = 8;

struct TitleVramStream {
    std::uint8_t physical_bank = 0;
    std::uint16_t source_address = 0;
    std::uint16_t destination_word = 0;
};

constexpr std::array kTitleVramStreams{
    TitleVramStream{0x1EU, 0x8022U, 0x0800U},
    TitleVramStream{0x1EU, 0x9372U, 0x1800U},
    // The game selects MPR banks $40/$41 here. A 384 KiB HuCard mirrors
    // those windows onto physical ROM banks $20/$21.
    TitleVramStream{0x20U, 0x800AU, 0x2800U},
    TitleVramStream{0x20U, 0x8699U, 0x3800U},
    TitleVramStream{0x1EU, 0xACB3U, 0x0000U},
};

[[nodiscard]] std::optional<std::vector<std::uint16_t>> decode_palette(
    const std::span<const std::byte> rom_bytes,
    const std::size_t offset)
{
    constexpr auto byte_count = kPaletteColors * 2U;
    if (offset > rom_bytes.size()
        || byte_count > rom_bytes.size() - offset) {
        return std::nullopt;
    }

    std::vector<std::uint16_t> palette(kPaletteColors);
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto low = std::to_integer<std::uint16_t>(
            rom_bytes[offset + index * 2U]);
        const auto high = std::to_integer<std::uint16_t>(
            rom_bytes[offset + index * 2U + 1U]);
        palette[index] =
            static_cast<std::uint16_t>(low | (high << 8U));
    }
    return palette;
}

void set_sprite(
    std::vector<std::uint16_t>& sat,
    const std::size_t index,
    const std::uint16_t y,
    const std::uint16_t x,
    const std::uint16_t pattern,
    const std::uint16_t flags)
{
    const auto offset = index * 4U;
    sat[offset] = y;
    sat[offset + 1U] = x;
    sat[offset + 2U] = pattern;
    sat[offset + 3U] = flags;
}

} // namespace

std::optional<TitleLogoAnimation> TitleLogoAnimation::decode(
    const std::span<const std::byte> rom_bytes)
{
    std::vector<std::uint16_t> vram(kVramWordCount, 0U);
    for (const auto& stream : kTitleVramStreams) {
        std::array<std::uint8_t, 8> mpr{};
        mpr[4] = stream.physical_bank;
        mpr[5] =
            static_cast<std::uint8_t>(stream.physical_bank + 1U);
        auto words = decode_pce_vram_stream(
            rom_bytes,
            mpr,
            stream.source_address,
            kVramWordCount);
        const auto destination =
            static_cast<std::size_t>(stream.destination_word);
        if (!words || destination > vram.size()
            || words->size() > vram.size() - destination) {
            return std::nullopt;
        }
        std::copy(
            words->begin(),
            words->end(),
            vram.begin()
                + static_cast<std::ptrdiff_t>(destination));
    }

    auto warm_palette =
        decode_palette(rom_bytes, kWarmPaletteRomOffset);
    auto bright_palette =
        decode_palette(rom_bytes, kBrightPaletteRomOffset);
    auto eye_palette =
        decode_palette(rom_bytes, kEyePaletteRomOffset);
    if (!warm_palette || !bright_palette || !eye_palette) {
        return std::nullopt;
    }

    TitleLogoAnimation animation;
    animation.vram_words_ = std::move(vram);
    animation.warm_palette_ = std::move(*warm_palette);
    animation.bright_palette_ = std::move(*bright_palette);
    animation.eye_palette_ = std::move(*eye_palette);
    return animation;
}

std::optional<ArgbImage> TitleLogoAnimation::render(
    const std::uint64_t animation_frame) const
{
    if (vram_words_.size() != kVramWordCount
        || warm_palette_.size() != kPaletteColors
        || bright_palette_.size() != kPaletteColors
        || eye_palette_.size() != kPaletteColors) {
        return std::nullopt;
    }

    const auto phase = static_cast<std::uint8_t>(
        animation_frame % kTitleLogoAnimationFrames);
    const bool bright =
        static_cast<std::uint8_t>(phase - 2U) % 8U < 4U;
    std::vector<std::uint16_t> palette(kPaletteEntryCount, 0U);
    const auto& background_palette =
        bright ? bright_palette_ : warm_palette_;
    std::copy(
        background_palette.begin(),
        background_palette.end(),
        palette.begin());
    constexpr std::size_t eye_palette_base = 256U + 16U;
    std::copy(
        eye_palette_.begin(),
        eye_palette_.end(),
        palette.begin()
            + static_cast<std::ptrdiff_t>(eye_palette_base));

    auto image = decode_pce_background({
        .vram_words = vram_words_,
        .palette_entries = palette,
        .bat_width_tiles = 32,
        .bat_height_tiles = 32,
    });
    if (!image) {
        return std::nullopt;
    }

    std::vector<std::uint16_t> sat(256U, 0U);
    if (phase < 24U) {
        set_sprite(sat, 0U, 0x005DU, 0x0069U, 0x01E8U, 0x1181U);
        set_sprite(sat, 1U, 0x007DU, 0x0069U, 0x01F8U, 0x0981U);
    } else if (phase < 32U || phase >= 56U) {
        set_sprite(sat, 0U, 0x005DU, 0x0069U, 0x01D0U, 0x1181U);
        set_sprite(sat, 1U, 0x007DU, 0x0069U, 0x01D8U, 0x0181U);
    } else {
        set_sprite(sat, 0U, 0x005DU, 0x0069U, 0x01F0U, 0x1181U);
        set_sprite(sat, 1U, 0x007DU, 0x0069U, 0x01F8U, 0x0181U);
    }

    std::uint16_t lower_eye_pattern = 0x0228U;
    if ((phase >= 3U && phase <= 10U)
        || (phase >= 35U && phase <= 42U)) {
        lower_eye_pattern = 0x0230U;
    } else if (
        (phase >= 19U && phase <= 26U)
        || (phase >= 51U && phase <= 58U)) {
        lower_eye_pattern = 0x0210U;
    }
    set_sprite(
        sat,
        2U,
        0x00A8U,
        0x005EU,
        lower_eye_pattern,
        0x1181U);
    set_sprite(sat, 3U, 0x00C8U, 0x005EU, 0x0218U, 0x0181U);

    const auto overlay = composite_pce_sprites(
        *image,
        {
            .vram_words = vram_words_,
            .sat_words = sat,
            .palette_entries = palette,
            .memory_width_register = 0U,
        },
        0,
        0,
        0,
        256);
    if (!overlay) {
        return std::nullopt;
    }
    return crop_image(
        *image,
        kLogoSourceX,
        kLogoSourceY,
        kTitleLogoWidth,
        kTitleLogoHeight);
}

} // namespace devils_crush::render
