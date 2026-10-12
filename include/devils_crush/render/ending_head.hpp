// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/render/pce_background.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::render {

// The opening of the maximum-score ending, from the frame the table gives
// way to a black screen to the last frame before that screen is cleared for
// the next one: the Giant Face fades in alone, four balls close on the jewel
// of its helmet, and six bursts go off over it.
inline constexpr std::uint64_t kEndingHeadFrames = 190U;
// Where the face stands on the game's 256-pixel-wide screen.
inline constexpr int kEndingHeadScreenX = 88;
inline constexpr int kEndingHeadScreenY = 64;
inline constexpr int kEndingHeadWidth = 56;
inline constexpr int kEndingHeadHeight = 72;

// A native, ROM-derived rendering of that sequence. The patterns and
// palettes are decoded once; draw() works the fade, the balls and the bursts
// out from the frame number alone and has no framebuffer or live VDC
// dependency. Nothing in the sequence is random or reads the session.
class EndingHeadAnimation {
public:
    [[nodiscard]] static std::optional<EndingHeadAnimation> decode(
        std::span<const std::byte> rom_bytes);

    // Draws one frame over `destination`, whose pixel (0, 0) is the game's
    // screen pixel (screen_x, screen_y). The game's screen edges are not
    // applied, so a destination wider than the screen shows the balls where
    // the cartridge has them before they come on. A frame past the last
    // draws nothing.
    void draw(
        ArgbImage& destination,
        std::uint64_t frame,
        int screen_x,
        int screen_y) const;

private:
    std::vector<std::uint16_t> vram_words_;
    // The settled sprite palettes, in the VCE's 512 entries.
    std::vector<std::uint16_t> palette_entries_;
    std::array<std::uint16_t, 16> head_palette_{};
    // One colour index per pixel of the face.
    std::vector<std::uint8_t> head_pixels_;
};

} // namespace devils_crush::render
