// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/pce_background.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace devils_crush::render {
namespace {

[[nodiscard]] bool checked_pixel_count(
    const int width,
    const int height,
    std::size_t& result) noexcept
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / height_size) {
        return false;
    }

    result = width_size * height_size;
    return result <= std::vector<std::uint32_t>{}.max_size();
}

[[nodiscard]] bool checked_extent(
    const int origin,
    const int length,
    std::int64_t& end) noexcept
{
    if (length <= 0) {
        return false;
    }
    end = static_cast<std::int64_t>(origin)
        + static_cast<std::int64_t>(length);
    return end <= std::numeric_limits<int>::max()
        && end >= std::numeric_limits<int>::min();
}

[[nodiscard]] std::optional<PceBackgroundLayer> decode_pce_background_map(
    const std::span<const std::uint16_t> bat_words,
    const std::span<const std::uint16_t> vram_words,
    const std::span<const std::uint16_t> palette_entries,
    const int width_tiles,
    const int height_tiles)
{
    constexpr std::size_t tile_word_count = 16U;
    constexpr int tile_size = 8;
    constexpr std::size_t background_palette_size = 256U;

    if (width_tiles <= 0 || height_tiles <= 0
        || width_tiles > std::numeric_limits<int>::max() / tile_size
        || height_tiles > std::numeric_limits<int>::max() / tile_size
        || palette_entries.size() < background_palette_size) {
        return std::nullopt;
    }

    const auto bat_width = static_cast<std::size_t>(width_tiles);
    const auto bat_height = static_cast<std::size_t>(height_tiles);
    if (bat_width
        > std::numeric_limits<std::size_t>::max() / bat_height) {
        return std::nullopt;
    }
    const auto bat_word_count = bat_width * bat_height;
    if (bat_words.size() < bat_word_count) {
        return std::nullopt;
    }

    PceBackgroundLayer layer{
        .image = {
            .width = width_tiles * tile_size,
            .height = height_tiles * tile_size,
        },
    };
    std::size_t pixel_count = 0;
    if (!checked_pixel_count(
            layer.image.width,
            layer.image.height,
            pixel_count)) {
        return std::nullopt;
    }
    layer.image.pixels.resize(pixel_count);
    layer.opaque_pixels.resize(pixel_count);

    for (int tile_y = 0; tile_y < height_tiles; ++tile_y) {
        for (int tile_x = 0; tile_x < width_tiles; ++tile_x) {
            const auto bat_index =
                static_cast<std::size_t>(tile_y) * bat_width
                + static_cast<std::size_t>(tile_x);
            const auto bat = bat_words[bat_index];
            const auto tile_number =
                static_cast<std::size_t>(bat & 0x0FFFU);
            const auto tile_offset = tile_number * tile_word_count;
            const auto palette_base =
                static_cast<std::size_t>((bat >> 12U) & 0x0FU) * 16U;
            for (int row = 0; row < tile_size; ++row) {
                // A BAT can contain dormant cells whose 12-bit pattern
                // number lies beyond the configured VRAM-backed tile cache.
                // Beetle leaves those cache entries zeroed, so mirror that
                // behavior instead of rejecting an otherwise valid map.
                const auto tile_available =
                    tile_offset <= vram_words.size()
                    && vram_words.size() - tile_offset
                        >= tile_word_count;
                const auto plane_01 = tile_available
                    ? vram_words[
                          tile_offset + static_cast<std::size_t>(row)]
                    : std::uint16_t{0};
                const auto plane_23 = tile_available
                    ? vram_words[
                          tile_offset + 8U
                          + static_cast<std::size_t>(row)]
                    : std::uint16_t{0};

                for (int column = 0; column < tile_size; ++column) {
                    const auto bit =
                        static_cast<unsigned>(7 - column);
                    auto pixel = static_cast<std::uint16_t>(
                        (plane_01 >> bit) & 0x01U);
                    pixel |= static_cast<std::uint16_t>(
                        ((plane_01 >> (bit + 8U)) & 0x01U) << 1U);
                    pixel |= static_cast<std::uint16_t>(
                        ((plane_23 >> bit) & 0x01U) << 2U);
                    pixel |= static_cast<std::uint16_t>(
                        ((plane_23 >> (bit + 8U)) & 0x01U) << 3U);

                    // On the VCE, background color zero is shared by all
                    // subpalettes.
                    const auto palette_index = pixel == 0U
                        ? std::size_t{0}
                        : palette_base + pixel;
                    const auto output_x = tile_x * tile_size + column;
                    const auto output_y = tile_y * tile_size + row;
                    const auto output_offset =
                        static_cast<std::size_t>(output_y)
                            * static_cast<std::size_t>(
                                layer.image.width)
                        + static_cast<std::size_t>(output_x);
                    layer.image.pixels[output_offset] =
                        decode_pce_color(palette_entries[palette_index]);
                    layer.opaque_pixels[output_offset] =
                        pixel == 0U ? 0U : 1U;
                }
            }
        }
    }

    return layer;
}

} // namespace

bool ArgbImage::valid() const noexcept
{
    std::size_t expected = 0;
    return checked_pixel_count(width, height, expected)
        && pixels.size() == expected;
}

bool PceBackgroundLayer::valid() const noexcept
{
    return image.valid()
        && opaque_pixels.size() == image.pixels.size();
}

std::uint32_t decode_pce_color(const std::uint16_t raw_color) noexcept
{
    const auto color = static_cast<std::uint32_t>(raw_color & 0x01FFU);
    const auto blue = 36U * (color & 0x07U);
    const auto red = 36U * ((color >> 3U) & 0x07U);
    const auto green = 36U * ((color >> 6U) & 0x07U);
    return 0xFF000000U | (red << 16U) | (green << 8U) | blue;
}

std::optional<ArgbImage> decode_pce_background(
    const PceBackgroundSnapshot& snapshot)
{
    auto layer = decode_pce_background_map(
        snapshot.vram_words,
        snapshot.vram_words,
        snapshot.palette_entries,
        snapshot.bat_width_tiles,
        snapshot.bat_height_tiles);
    if (!layer) {
        return std::nullopt;
    }
    return std::move(layer->image);
}

std::optional<PceBackgroundLayer> decode_pce_background_layer(
    const PceBackgroundSnapshot& snapshot)
{
    return decode_pce_background_map(
        snapshot.vram_words,
        snapshot.vram_words,
        snapshot.palette_entries,
        snapshot.bat_width_tiles,
        snapshot.bat_height_tiles);
}

std::optional<ArgbImage> decode_packed_pce_background(
    const PackedPceBackgroundSnapshot& snapshot)
{
    auto layer = decode_packed_pce_background_layer(snapshot);
    if (!layer) {
        return std::nullopt;
    }
    return std::move(layer->image);
}

std::optional<PceBackgroundLayer> decode_packed_pce_background_layer(
    const PackedPceBackgroundSnapshot& snapshot)
{
    if (snapshot.width_tiles <= 0 || snapshot.height_tiles <= 0
        || (snapshot.width_tiles & 1) != 0) {
        return std::nullopt;
    }
    const auto width = static_cast<std::size_t>(snapshot.width_tiles);
    const auto height = static_cast<std::size_t>(snapshot.height_tiles);
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        return std::nullopt;
    }
    const auto bat_word_count = width * height;
    if (bat_word_count
            > std::numeric_limits<std::size_t>::max() / 2U
        || snapshot.tile_words.size() < bat_word_count * 2U
        || snapshot.packed_palette_nibbles.size()
            < bat_word_count / 2U) {
        return std::nullopt;
    }

    std::vector<std::uint16_t> bat_words(bat_word_count);
    for (std::size_t index = 0; index < bat_word_count; ++index) {
        const auto low = std::to_integer<std::uint16_t>(
            snapshot.tile_words[index * 2U]);
        const auto tile_high = static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(
                snapshot.tile_words[index * 2U + 1U])
            & 0x0FU);
        const auto packed_palette = std::to_integer<std::uint8_t>(
            snapshot.packed_palette_nibbles[index / 2U]);
        const auto palette = static_cast<std::uint16_t>(
            (index & 1U) == 0U
                ? packed_palette >> 4U
                : packed_palette & 0x0FU);
        bat_words[index] = static_cast<std::uint16_t>(
            low | (tile_high << 8U) | (palette << 12U));
    }

    return decode_pce_background_map(
        bat_words,
        snapshot.vram_words,
        snapshot.palette_entries,
        snapshot.width_tiles,
        snapshot.height_tiles);
}

std::optional<StitchedImage> stitch_images(
    const std::span<const PlacedImage> images,
    const std::uint32_t clear_color)
{
    if (images.empty()) {
        return std::nullopt;
    }

    auto minimum_x = std::numeric_limits<int>::max();
    auto minimum_y = std::numeric_limits<int>::max();
    auto maximum_x = std::numeric_limits<int>::min();
    auto maximum_y = std::numeric_limits<int>::min();
    for (const auto& placed : images) {
        if (placed.image == nullptr || !placed.image->valid()) {
            return std::nullopt;
        }
        std::int64_t end_x = 0;
        std::int64_t end_y = 0;
        if (!checked_extent(placed.world_x, placed.image->width, end_x)
            || !checked_extent(
                placed.world_y,
                placed.image->height,
                end_y)) {
            return std::nullopt;
        }
        minimum_x = std::min(minimum_x, placed.world_x);
        minimum_y = std::min(minimum_y, placed.world_y);
        maximum_x = std::max(maximum_x, static_cast<int>(end_x));
        maximum_y = std::max(maximum_y, static_cast<int>(end_y));
    }

    const auto stitched_width =
        static_cast<std::int64_t>(maximum_x)
        - static_cast<std::int64_t>(minimum_x);
    const auto stitched_height =
        static_cast<std::int64_t>(maximum_y)
        - static_cast<std::int64_t>(minimum_y);
    if (stitched_width <= 0 || stitched_height <= 0
        || stitched_width > std::numeric_limits<int>::max()
        || stitched_height > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }

    StitchedImage stitched{
        .image = {
            .width = static_cast<int>(stitched_width),
            .height = static_cast<int>(stitched_height),
        },
        .world_x = minimum_x,
        .world_y = minimum_y,
    };
    std::size_t pixel_count = 0;
    if (!checked_pixel_count(
            stitched.image.width,
            stitched.image.height,
            pixel_count)) {
        return std::nullopt;
    }
    stitched.image.pixels.assign(pixel_count, clear_color);

    for (const auto& placed : images) {
        const auto destination_x = placed.world_x - minimum_x;
        const auto destination_y = placed.world_y - minimum_y;
        for (int y = 0; y < placed.image->height; ++y) {
            const auto source_offset =
                static_cast<std::size_t>(y)
                * static_cast<std::size_t>(placed.image->width);
            const auto destination_offset =
                static_cast<std::size_t>(destination_y + y)
                    * static_cast<std::size_t>(stitched.image.width)
                + static_cast<std::size_t>(destination_x);
            std::copy_n(
                placed.image->pixels.begin()
                    + static_cast<std::ptrdiff_t>(source_offset),
                placed.image->width,
                stitched.image.pixels.begin()
                    + static_cast<std::ptrdiff_t>(destination_offset));
        }
    }

    return stitched;
}

std::optional<ArgbImage> crop_image(
    const ArgbImage& image,
    const int x,
    const int y,
    const int width,
    const int height)
{
    if (!image.valid() || x < 0 || y < 0 || width <= 0 || height <= 0
        || x > image.width - width || y > image.height - height) {
        return std::nullopt;
    }

    ArgbImage cropped{
        .width = width,
        .height = height,
    };
    std::size_t pixel_count = 0;
    if (!checked_pixel_count(width, height, pixel_count)) {
        return std::nullopt;
    }
    cropped.pixels.resize(pixel_count);
    for (int row = 0; row < height; ++row) {
        const auto source_offset =
            static_cast<std::size_t>(y + row)
                * static_cast<std::size_t>(image.width)
            + static_cast<std::size_t>(x);
        const auto destination_offset =
            static_cast<std::size_t>(row)
            * static_cast<std::size_t>(width);
        std::copy_n(
            image.pixels.begin()
                + static_cast<std::ptrdiff_t>(source_offset),
            width,
            cropped.pixels.begin()
                + static_cast<std::ptrdiff_t>(destination_offset));
    }
    return cropped;
}

std::optional<PceBackgroundLayer> crop_background_layer(
    const PceBackgroundLayer& layer,
    const int x,
    const int y,
    const int width,
    const int height)
{
    if (!layer.valid()) {
        return std::nullopt;
    }
    auto image = crop_image(layer.image, x, y, width, height);
    if (!image) {
        return std::nullopt;
    }

    PceBackgroundLayer cropped{
        .image = std::move(*image),
        .opaque_pixels = std::vector<std::uint8_t>(
            static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height)),
    };
    for (int row = 0; row < height; ++row) {
        const auto source_offset =
            static_cast<std::size_t>(y + row)
                * static_cast<std::size_t>(layer.image.width)
            + static_cast<std::size_t>(x);
        const auto destination_offset =
            static_cast<std::size_t>(row)
            * static_cast<std::size_t>(width);
        std::copy_n(
            layer.opaque_pixels.begin()
                + static_cast<std::ptrdiff_t>(source_offset),
            width,
            cropped.opaque_pixels.begin()
                + static_cast<std::ptrdiff_t>(destination_offset));
    }
    return cropped;
}

} // namespace devils_crush::render
