// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/ending_head.hpp"

#include "devils_crush/render/pce_sprite_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace devils_crush::render {
namespace {

constexpr std::size_t kVramWordCount = 32768U;
constexpr std::size_t kPaletteEntryCount = 512U;
constexpr std::size_t kPaletteColors = 16U;

// The ending loads no patterns of its own: it draws with what the table left
// in VRAM. The face is the table's resident pack 0 (asset `$1222`) at
// `$7C00`, and the balls and bursts are in the table's sprite set at `$3800`.
struct EndingVramStream {
    std::uint8_t physical_bank = 0;
    std::uint16_t source_address = 0;
    std::uint16_t destination_word = 0;
    std::size_t word_count = 0;
};

constexpr std::array kEndingVramStreams{
    EndingVramStream{0x12U, 0x91EBU, 0x7C00U, 1008U},
    EndingVramStream{0x0DU, 0x862DU, 0x3800U, 4096U},
};

// Three palettes in a row in bank `$07`: the face's, background palette 1,
// and sprite palettes 0 and 14 for the balls and the bursts.
constexpr std::size_t kHeadPaletteRomOffset = 0x00EB49U;
constexpr std::size_t kBallPaletteRomOffset = 0x00EB69U;
constexpr std::size_t kBurstPaletteRomOffset = 0x00EB89U;
constexpr std::size_t kBallPalette = 0U;
constexpr std::size_t kBurstPalette = 14U;

// The face is seven tiles by nine, tile `$7C0 + 7 * row + column`, without
// its four corners, which the table's map gives to other tiles.
constexpr int kHeadTileColumns = 7;
constexpr int kHeadTileRows = 9;
constexpr std::size_t kHeadFirstTile = 0x7C0U;

// The face comes up from black in its 21 steps at frame 2, at frame 4 and
// every third frame after that, so it is whole at frame 61.

// The balls are actors `$04-$07` under `07:4261` for 63 frames. Positions
// are 16.8 and velocities 8.8, as on the table, and a ball's sprite is at
// its position less (38, 6). Each ball runs straight down one axis and bows
// along the other, starting at two pixels a frame there and losing a
// sixteenth of a pixel each frame, which brings it back to where it began.
constexpr std::uint64_t kBallsFirstFrame = 79U;
constexpr int kBallFlightFrames = 63;
constexpr int kBallSpriteOffsetX = -38;
constexpr int kBallSpriteOffsetY = -6;

struct Ball {
    int x = 0;
    int y = 0;
    // The straight axis's velocity, and which axis that is.
    int straight = 0;
    bool straight_is_x = false;
    // The bowed axis's first velocity; it changes by 16 a frame toward zero
    // and on past it.
    int bow = 0;
};

constexpr std::array kBalls{
    Ball{270 * 256, 78 * 256, -512, true, -496},
    Ball{146 * 256, 174 * 256 + 128, -384, false, 496},
    Ball{18 * 256, 82 * 256, 512, true, 496},
    Ball{142 * 256, -15 * 256 + 128, 384, false, -496},
};

// In flight a ball turns through four patterns, four frames each. Together
// at the jewel (`07:428E`) they shrink through three more: one frame, then
// four and four.
constexpr std::array<std::uint16_t, 4> kBallPatterns{
    0x01E2U, 0x01E4U, 0x01E6U, 0x01E0U};
constexpr std::uint16_t kBallFlags = 0x0080U;

// Six type-`$8F` bursts under `07:49F4`, centred here on the screen. The
// first two start a frame before the other four. A burst is one 16-by-16
// sprite for two patterns and then two 16-by-32 halves, the right one the
// left one turned half round, for seven; the first pattern is up three
// frames and the rest four.
constexpr std::uint64_t kBurstsFirstFrame = 151U;
constexpr int kBurstFrames = 35;
constexpr std::array<std::pair<int, int>, 6> kBurstCentres{{
    {136, 112}, {144, 88}, {96, 104}, {120, 64}, {96, 80}, {120, 96}}};
constexpr std::size_t kEarlyBursts = 2U;
constexpr std::array<std::uint16_t, 9> kBurstPatterns{
    0x0210U, 0x0212U, 0x0208U, 0x0200U, 0x0202U,
    0x01F8U, 0x01FAU, 0x01F0U, 0x01F2U};
constexpr std::size_t kBurstSmallPatterns = 2U;
constexpr std::uint16_t kBurstSmallFlags = 0x008EU;
constexpr std::uint16_t kBurstLeftFlags = 0x108EU;
constexpr std::uint16_t kBurstRightFlags = 0x988EU;

struct Sprite {
    int x = 0;
    int y = 0;
    std::uint16_t pattern = 0;
    std::uint16_t flags = 0;
};

[[nodiscard]] bool read_palette(
    const std::span<const std::byte> rom_bytes,
    const std::size_t offset,
    const std::span<std::uint16_t> palette)
{
    if (offset > rom_bytes.size()
        || palette.size() * 2U > rom_bytes.size() - offset) {
        return false;
    }
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto low = std::to_integer<std::uint16_t>(
            rom_bytes[offset + index * 2U]);
        const auto high = std::to_integer<std::uint16_t>(
            rom_bytes[offset + index * 2U + 1U]);
        palette[index] = static_cast<std::uint16_t>(low | (high << 8U));
    }
    return true;
}

// How far the fade has come at a frame, from 0, black, to kEndingHeadFadeSteps.
[[nodiscard]] unsigned fade_level(const std::uint64_t frame) noexcept
{
    if (frame < 2U) {
        return 0U;
    }
    if (frame < 4U) {
        return 1U;
    }
    return static_cast<unsigned>(std::min<std::uint64_t>(
        kEndingHeadFadeSteps, 2U + (frame - 4U) / 3U));
}

// A colour that many steps short of whole. Each step takes one level off one
// channel, in the order blue, green, red, and passes over a channel that has
// none left, so every colour is black within its 21 steps.
[[nodiscard]] std::uint16_t faded_color(
    const std::uint16_t color,
    const unsigned steps_short) noexcept
{
    // Blue, green, red, in the VCE's nine bits.
    constexpr std::array<unsigned, 3> shifts{0U, 6U, 3U};
    std::array<unsigned, 3> channels{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        channels[channel] = (color >> shifts[channel]) & 0x07U;
    }
    std::size_t next = 0;
    for (unsigned step = 0; step < steps_short; ++step) {
        for (std::size_t tried = 0; tried < channels.size(); ++tried) {
            const auto channel = (next + tried) % channels.size();
            if (channels[channel] != 0U) {
                --channels[channel];
                next = (channel + 1U) % channels.size();
                break;
            }
        }
    }
    unsigned faded = 0U;
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        faded |= channels[channel] << shifts[channel];
    }
    return static_cast<std::uint16_t>(faded);
}

// A ball's sprite after that many frames of flight.
[[nodiscard]] std::pair<int, int> ball_sprite_position(
    const Ball& ball,
    const int frames) noexcept
{
    const int straight = ball.straight * frames;
    // The bowed velocity moves 16 toward zero before each later frame.
    const int toward_zero = ball.bow < 0 ? 16 : -16;
    const int bow = ball.bow * frames
        + toward_zero * (frames * (frames - 1) / 2);
    const int x = ball.x + (ball.straight_is_x ? straight : bow);
    const int y = ball.y + (ball.straight_is_x ? bow : straight);
    // Arithmetic shifts: the top ball starts above the screen.
    return {(x >> 8) + kBallSpriteOffsetX, (y >> 8) + kBallSpriteOffsetY};
}

// The sprites of one frame, in the SAT's order: the first is in front.
[[nodiscard]] std::vector<Sprite> sprites_for_frame(const std::uint64_t frame)
{
    std::vector<Sprite> sprites;
    if (frame >= kBallsFirstFrame && frame < kBurstsFirstFrame) {
        const int flown = static_cast<int>(frame - kBallsFirstFrame);
        std::uint16_t pattern = 0;
        if (flown < kBallFlightFrames) {
            pattern = kBallPatterns[static_cast<std::size_t>(flown / 4) % 4U];
        } else if (flown == kBallFlightFrames) {
            pattern = 0x0228U;
        } else {
            pattern = flown <= kBallFlightFrames + 4 ? 0x022AU : 0x0232U;
        }
        for (const auto& ball : kBalls) {
            const auto [x, y] = ball_sprite_position(
                ball, std::min(flown, kBallFlightFrames));
            sprites.push_back({x, y, pattern, kBallFlags});
        }
    }
    for (std::size_t index = 0; index < kBurstCentres.size(); ++index) {
        const auto first =
            kBurstsFirstFrame + (index < kEarlyBursts ? 0U : 1U);
        if (frame < first
            || frame >= first + static_cast<std::uint64_t>(kBurstFrames)) {
            continue;
        }
        const auto stage =
            static_cast<std::size_t>((frame - first + 1U) / 4U);
        const auto pattern = kBurstPatterns[stage];
        const auto [x, y] = kBurstCentres[index];
        if (stage < kBurstSmallPatterns) {
            sprites.push_back({x - 8, y - 8, pattern, kBurstSmallFlags});
        } else {
            sprites.push_back({x - 16, y - 16, pattern, kBurstLeftFlags});
            sprites.push_back({x, y - 16, pattern, kBurstRightFlags});
        }
    }
    return sprites;
}

void put_pixel(
    ArgbImage& destination,
    const int x,
    const int y,
    const std::uint32_t color) noexcept
{
    if (x >= 0 && x < destination.width && y >= 0 && y < destination.height) {
        destination.pixels[
            static_cast<std::size_t>(y)
                * static_cast<std::size_t>(destination.width)
            + static_cast<std::size_t>(x)] = color;
    }
}

} // namespace

std::optional<EndingHeadAnimation> EndingHeadAnimation::decode(
    const std::span<const std::byte> rom_bytes)
{
    std::vector<std::uint16_t> vram(kVramWordCount, 0U);
    for (const auto& stream : kEndingVramStreams) {
        std::array<std::uint8_t, 8> mpr{};
        mpr[4] = stream.physical_bank;
        mpr[5] = static_cast<std::uint8_t>(stream.physical_bank + 1U);
        const auto words = decode_pce_vram_stream(
            rom_bytes, mpr, stream.source_address, kVramWordCount);
        const auto destination =
            static_cast<std::size_t>(stream.destination_word);
        if (!words || words->size() != stream.word_count
            || words->size() > vram.size() - destination) {
            return std::nullopt;
        }
        std::copy(
            words->begin(),
            words->end(),
            vram.begin() + static_cast<std::ptrdiff_t>(destination));
    }

    EndingHeadAnimation animation;
    animation.palette_entries_.assign(kPaletteEntryCount, 0U);
    const auto sprite_palette = [&animation](const std::size_t palette) {
        return std::span(animation.palette_entries_)
            .subspan(256U + palette * kPaletteColors, kPaletteColors);
    };
    if (!read_palette(
            rom_bytes, kHeadPaletteRomOffset, animation.head_palette_)
        || !read_palette(
            rom_bytes, kBallPaletteRomOffset, sprite_palette(kBallPalette))
        || !read_palette(
            rom_bytes,
            kBurstPaletteRomOffset,
            sprite_palette(kBurstPalette))) {
        return std::nullopt;
    }

    // A background tile is eight words of planes 0 and 1 and eight of planes
    // 2 and 3, the leftmost pixel in each byte's top bit.
    animation.head_pixels_.assign(
        static_cast<std::size_t>(kEndingHeadWidth)
            * static_cast<std::size_t>(kEndingHeadHeight),
        0U);
    for (int row = 0; row < kHeadTileRows; ++row) {
        for (int column = 0; column < kHeadTileColumns; ++column) {
            if ((row == 0 || row == kHeadTileRows - 1)
                && (column == 0 || column == kHeadTileColumns - 1)) {
                continue;
            }
            const auto tile = kHeadFirstTile
                + static_cast<std::size_t>(row * kHeadTileColumns + column);
            for (int y = 0; y < 8; ++y) {
                const auto low =
                    vram[tile * 16U + static_cast<std::size_t>(y)];
                const auto high =
                    vram[tile * 16U + 8U + static_cast<std::size_t>(y)];
                for (int x = 0; x < 8; ++x) {
                    const unsigned bit = 7U - static_cast<unsigned>(x);
                    const auto pixel = static_cast<std::uint8_t>(
                        ((low >> bit) & 1U)
                        | (((low >> (bit + 8U)) & 1U) << 1U)
                        | (((high >> bit) & 1U) << 2U)
                        | (((high >> (bit + 8U)) & 1U) << 3U));
                    animation.head_pixels_[
                        static_cast<std::size_t>(row * 8 + y)
                            * static_cast<std::size_t>(kEndingHeadWidth)
                        + static_cast<std::size_t>(column * 8 + x)] = pixel;
                }
            }
        }
    }
    animation.vram_words_ = std::move(vram);
    return animation;
}

void EndingHeadAnimation::draw_head(
    ArgbImage& destination,
    const unsigned level,
    const int screen_x,
    const int screen_y) const
{
    if (level == 0U || !destination.valid()
        || head_pixels_.size()
            != static_cast<std::size_t>(kEndingHeadWidth)
                * static_cast<std::size_t>(kEndingHeadHeight)) {
        return;
    }
    std::array<std::uint32_t, kPaletteColors> colors{};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        colors[index] = decode_pce_color(faded_color(
            head_palette_[index],
            kEndingHeadFadeSteps - std::min(level, kEndingHeadFadeSteps)));
    }
    for (int y = 0; y < kEndingHeadHeight; ++y) {
        for (int x = 0; x < kEndingHeadWidth; ++x) {
            const auto pixel = head_pixels_[
                static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(kEndingHeadWidth)
                + static_cast<std::size_t>(x)];
            if (pixel != 0U) {
                put_pixel(
                    destination,
                    kEndingHeadScreenX + x - screen_x,
                    kEndingHeadScreenY + y - screen_y,
                    colors[pixel]);
            }
        }
    }
}

void EndingHeadAnimation::draw(
    ArgbImage& destination,
    const std::uint64_t frame,
    const int screen_x,
    const int screen_y) const
{
    if (frame >= kEndingHeadFrames || !destination.valid()
        || vram_words_.size() != kVramWordCount
        || palette_entries_.size() != kPaletteEntryCount) {
        return;
    }

    draw_head(destination, fade_level(frame), screen_x, screen_y);

    const PceSpriteSnapshot snapshot{
        .vram_words = vram_words_,
        .sat_words = {},
        .palette_entries = palette_entries_,
        .memory_width_register = 0U,
    };
    const auto sprites = sprites_for_frame(frame);
    for (auto sprite = sprites.rbegin(); sprite != sprites.rend(); ++sprite) {
        const auto image =
            decode_pce_sprite_image(snapshot, sprite->pattern, sprite->flags);
        if (!image) {
            continue;
        }
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                const auto color = image->pixels[
                    static_cast<std::size_t>(y)
                        * static_cast<std::size_t>(image->width)
                    + static_cast<std::size_t>(x)];
                if ((color >> 24U) != 0U) {
                    put_pixel(
                        destination,
                        sprite->x + x - screen_x,
                        sprite->y + y - screen_y,
                        color);
                }
            }
        }
    }
}

} // namespace devils_crush::render
