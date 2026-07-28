// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::render {

struct ArgbImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;

    [[nodiscard]] bool valid() const noexcept;
};

struct PceBackgroundSnapshot {
    // HuC6270 VRAM uses 16-bit word addressing. The BAT begins at word zero;
    // each BAT entry selects one 8x8, 4-bit background tile and one of the
    // sixteen background subpalettes.
    std::span<const std::uint16_t> vram_words;
    // Raw 9-bit VCE entries. Background decoding consumes entries 0-255.
    std::span<const std::uint16_t> palette_entries;
    int bat_width_tiles = 0;
    int bat_height_tiles = 0;
};

struct PlacedImage {
    const ArgbImage* image = nullptr;
    int world_x = 0;
    int world_y = 0;
};

struct StitchedImage {
    ArgbImage image;
    int world_x = 0;
    int world_y = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return image.valid();
    }
};

[[nodiscard]] std::uint32_t decode_pce_color(
    std::uint16_t raw_color) noexcept;

[[nodiscard]] std::optional<ArgbImage> decode_pce_background(
    const PceBackgroundSnapshot& snapshot);

// Images are composited in input order, so later regions own overlap pixels.
// The returned origin records the minimum world coordinate represented by
// image pixel (0, 0).
[[nodiscard]] std::optional<StitchedImage> stitch_images(
    std::span<const PlacedImage> images,
    std::uint32_t clear_color = 0xFF000000U);

[[nodiscard]] std::optional<ArgbImage> crop_image(
    const ArgbImage& image,
    int x,
    int y,
    int width,
    int height);

} // namespace devils_crush::render
