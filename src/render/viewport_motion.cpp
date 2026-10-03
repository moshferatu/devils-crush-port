// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/viewport_motion.hpp"

#include "devils_crush/render/pce_sprite_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <tuple>

namespace devils_crush::render {
namespace {

constexpr std::size_t kSpriteCount = 64U;
constexpr std::size_t kSatWordCount = kSpriteCount * 4U;
constexpr int kSpriteXBias = 0x20;
constexpr int kSpriteYBias = 0x40;
constexpr int kSpriteHalvesPerLine = 16;
// The table's own link distance (table_art_linked): anything further is a cut.
constexpr int kLinkDistance = 16;
// A new pattern may change a moving sprite's step by this much, as the step
// filter's own bound allows; further is a pose drawn somewhere else.
constexpr int kPoseStepChange = 2;
constexpr std::uint8_t kFirstSlot = 1U;
constexpr std::uint8_t kSlotLimit =
    static_cast<std::uint8_t>(cabinet::kActorSlotCount);

[[nodiscard]] int sprite_width(const std::uint16_t flags) noexcept
{
    return (flags & 0x0100U) != 0U ? 32 : 16;
}

[[nodiscard]] int sprite_height(const std::uint16_t flags) noexcept
{
    constexpr std::array heights{16, 32, 64, 64};
    return heights[(flags >> 12U) & 0x03U];
}

[[nodiscard]] std::uint32_t to_framebuffer_color(const std::uint32_t argb) noexcept
{
    const auto red = (argb >> 19U) & 0x1FU;
    const auto green = (argb >> 10U) & 0x3FU;
    const auto blue = (argb >> 3U) & 0x1FU;
    return 0xFF000000U | (((red << 3U) | (red >> 2U)) << 16U)
        | (((green << 2U) | (green >> 4U)) << 8U) | ((blue << 3U) | (blue >> 2U));
}

} // namespace

std::uint32_t pce_framebuffer_color(const std::uint16_t raw_color) noexcept
{
    return to_framebuffer_color(decode_pce_color(raw_color));
}

void ViewportMotionTracker::reset() noexcept
{
    shown_sat_.clear();
    previous_.clear();
}

std::optional<TableMotionFrame> ViewportMotionTracker::update(
    const ViewportVideo& video,
    const ViewportPicture& picture)
{
    if (video.sat_words.size() < kSatWordCount) {
        reset();
        return std::nullopt;
    }
    // This picture shows the SAT the previous frame left behind; this frame's
    // copy belongs to the next picture.
    const auto shown = std::move(shown_sat_);
    shown_sat_.assign(video.sat_words.begin(),
        video.sat_words.begin() + static_cast<std::ptrdiff_t>(kSatWordCount));
    if (shown.size() != kSatWordCount || picture.width <= 0 || picture.height <= 0
        || picture.pitch_pixels < picture.width
        || picture.pixels.size() < static_cast<std::size_t>(picture.pitch_pixels)
                * static_cast<std::size_t>(picture.height - 1)
            + static_cast<std::size_t>(picture.width)) {
        previous_.clear();
        return std::nullopt;
    }
    // Rooms do not scroll while they play. A scroll change is a new screen.
    if (video.background_x_scroll != scroll_x_ || video.background_y_scroll != scroll_y_) {
        previous_.clear();
        scroll_x_ = video.background_x_scroll;
        scroll_y_ = video.background_y_scroll;
    }

    constexpr std::array bat_widths{32, 64, 128, 128};
    const auto map = decode_pce_background_layer({
        .vram_words = video.vram_words,
        .palette_entries = video.palette_entries,
        .bat_width_tiles = bat_widths[(video.memory_width_register >> 4U) & 0x03U],
        .bat_height_tiles = (video.memory_width_register & 0x40U) != 0U ? 64 : 32,
    });
    if (!map) {
        previous_.clear();
        return std::nullopt;
    }

    TableMotionFrame frame;
    frame.half_pixel_steps = true;
    auto& background = frame.background;
    background.image.width = picture.width;
    background.image.height = picture.height;
    const auto pixel_count = static_cast<std::size_t>(picture.width)
        * static_cast<std::size_t>(picture.height);
    background.image.pixels.resize(pixel_count);
    background.opaque_pixels.resize(pixel_count);
    const int map_width = map->image.width;
    const int map_height = map->image.height;
    for (int y = 0; y < picture.height; ++y) {
        const int map_y = (y + video.background_y_scroll) % map_height;
        for (int x = 0; x < picture.width; ++x) {
            const int map_x = (x + video.background_x_scroll) % map_width;
            const auto from = static_cast<std::size_t>(map_y) * static_cast<std::size_t>(map_width)
                + static_cast<std::size_t>(map_x);
            const auto to = static_cast<std::size_t>(y) * static_cast<std::size_t>(picture.width)
                + static_cast<std::size_t>(x);
            background.image.pixels[to] = to_framebuffer_color(map->image.pixels[from]);
            background.opaque_pixels[to] = map->opaque_pixels[from];
        }
    }

    // Hardware gives lower SAT indices the overlap, so list back to front.
    const PceSpriteSnapshot snapshot{
        .vram_words = video.vram_words,
        .sat_words = shown,
        .palette_entries = video.palette_entries,
        .memory_width_register = video.memory_width_register,
    };
    std::vector<Piece> pieces;
    std::array<int, kSpriteCount> piece_of{};
    piece_of.fill(-1);
    for (std::size_t index = kSpriteCount; index-- > 0U;) {
        const auto* entry = shown.data() + index * 4U;
        const std::uint16_t flags = entry[3];
        const int x = static_cast<int>(entry[1] & 0x03FFU) - kSpriteXBias;
        const int y = static_cast<int>(entry[0] & 0x03FFU) - kSpriteYBias;
        if (x + sprite_width(flags) <= 0 || x >= picture.width
            || y + sprite_height(flags) <= 0 || y >= picture.height) {
            continue;
        }
        auto image = decode_pce_sprite_image(snapshot, entry[2], flags);
        if (!image) {
            previous_.clear();
            return std::nullopt;
        }
        for (auto& pixel : image->pixels) {
            if (pixel != 0U) pixel = to_framebuffer_color(pixel);
        }
        piece_of[index] = static_cast<int>(pieces.size());
        pieces.push_back({.x = x, .y = y, .pattern = entry[2], .flags = flags});
        frame.sprites.push_back({
            .x = x,
            .y = y,
            .foreground = (flags & 0x0080U) != 0U,
            .image = std::move(*image),
        });
    }

    // A line shows only the first sixteen 16-pixel sprite halves the SAT
    // places on it, counting sprites off either side of the screen. Cut the
    // rest out of their pieces, as the hardware does.
    for (int line = 0; line < picture.height; ++line) {
        int halves = 0;
        for (std::size_t index = 0; index < kSpriteCount; ++index) {
            const auto* entry = shown.data() + index * 4U;
            const int top = static_cast<int>(entry[0] & 0x03FFU) - kSpriteYBias;
            if (line < top || line >= top + sprite_height(entry[3])) continue;
            for (int half = 0; half < sprite_width(entry[3]) / 16; ++half) {
                if (++halves <= kSpriteHalvesPerLine || piece_of[index] < 0) continue;
                auto& image = frame.sprites[static_cast<std::size_t>(piece_of[index])].image;
                std::fill_n(image.pixels.begin()
                        + static_cast<std::ptrdiff_t>(line - top) * image.width + half * 16,
                    16, 0U);
            }
        }
    }

    // Put the pieces back together exactly as the presenter will at the
    // endpoint, and keep them only if that is the picture.
    auto composed = background.image.pixels;
    for (const auto& sprite : frame.sprites) {
        for (int row = std::max(0, -sprite.y);
             row < sprite.image.height && sprite.y + row < picture.height; ++row) {
            for (int column = std::max(0, -sprite.x);
                 column < sprite.image.width && sprite.x + column < picture.width; ++column) {
                const auto pixel = sprite.image.pixels[
                    static_cast<std::size_t>(row) * static_cast<std::size_t>(sprite.image.width)
                    + static_cast<std::size_t>(column)];
                const auto to = static_cast<std::size_t>(sprite.y + row)
                        * static_cast<std::size_t>(picture.width)
                    + static_cast<std::size_t>(sprite.x + column);
                if (pixel != 0U && (sprite.foreground || background.opaque_pixels[to] == 0U)) {
                    composed[to] = pixel;
                }
            }
        }
    }
    for (int y = 0; y < picture.height; ++y) {
        if (!std::equal(composed.begin() + static_cast<std::ptrdiff_t>(y) * picture.width,
                composed.begin() + static_cast<std::ptrdiff_t>(y + 1) * picture.width,
                picture.pixels.begin() + static_cast<std::ptrdiff_t>(y) * picture.pitch_pixels)) {
            previous_.clear();
            return std::nullopt;
        }
    }

    // Link each sprite to the nearest unclaimed previous sprite of the same
    // size, preferring one that kept its pattern and palette.
    std::vector<std::tuple<int, std::size_t, std::size_t>> candidates;
    for (std::size_t now = 0; now < pieces.size(); ++now) {
        for (std::size_t then = 0; then < previous_.size(); ++then) {
            const auto& a = pieces[now];
            const auto& b = previous_[then];
            const int dx = std::abs(a.x - b.x);
            const int dy = std::abs(a.y - b.y);
            if ((a.flags & 0x3100U) != (b.flags & 0x3100U)
                || dx > kLinkDistance || dy > kLinkDistance) {
                continue;
            }
            const int cost = (dx + dy) * 4 + (a.pattern != b.pattern ? 2 : 0)
                + ((a.flags & 0x000FU) != (b.flags & 0x000FU) ? 1 : 0);
            candidates.emplace_back(cost, now, then);
        }
    }
    std::ranges::sort(candidates);
    std::vector<bool> claimed(previous_.size());
    std::array<bool, cabinet::kActorSlotCount> used{};
    std::vector<bool> linked(pieces.size());
    for (const auto& [cost, now, then] : candidates) {
        if (linked[now] || claimed[then]) continue;
        linked[now] = true;
        claimed[then] = true;
        auto& piece = pieces[now];
        const auto& old = previous_[then];
        piece.slot = old.slot;
        used[piece.slot] = true;
        const int step_x = piece.x - old.x;
        const int step_y = piece.y - old.y;
        if (piece.pattern != old.pattern
            && (std::abs(step_x - old.step_x) > kPoseStepChange
                || std::abs(step_y - old.step_y) > kPoseStepChange)) {
            if (++generation_ == 0U) generation_ = 1U;
            piece.generation = generation_;
            continue;
        }
        piece.generation = old.generation;
        piece.step_x = step_x;
        piece.step_y = step_y;
    }
    std::uint8_t next_slot = kFirstSlot;
    for (std::size_t now = 0; now < pieces.size(); ++now) {
        if (linked[now]) continue;
        while (next_slot < kSlotLimit && used[next_slot]) ++next_slot;
        if (next_slot >= kSlotLimit) {
            previous_.clear();
            return std::nullopt;
        }
        used[next_slot] = true;
        pieces[now].slot = next_slot;
        if (++generation_ == 0U) generation_ = 1U;
        pieces[now].generation = generation_;
    }
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        auto& actor = frame.sprites[index].actor;
        actor.slot = pieces[index].slot;
        actor.type = pieces[index].generation;
        actor.world_x = static_cast<std::int16_t>(pieces[index].x);
        actor.world_y = static_cast<std::int16_t>(pieces[index].y);
    }
    previous_ = std::move(pieces);
    return frame;
}

} // namespace devils_crush::render
