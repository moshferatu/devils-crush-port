// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "devils_crush/render/table_motion.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::render {

// The video state a picture was drawn from: the whole VRAM, the internal SAT,
// the VCE palette, and the memory-width and scroll registers.
struct ViewportVideo {
    std::span<const std::uint16_t> vram_words;
    std::span<const std::uint16_t> sat_words;
    std::span<const std::uint16_t> palette_entries;
    std::uint16_t memory_width_register = 0;
    std::uint16_t background_x_scroll = 0;
    std::uint16_t background_y_scroll = 0;
};

// The emulator's own picture, which is the reference the pieces must rebuild.
struct ViewportPicture {
    std::span<const std::uint32_t> pixels;
    int width = 0;
    int height = 0;
    int pitch_pixels = 0;
};

// The color the emulator's picture holds for a raw VCE entry: Beetle builds
// the same 36-per-step channels as decode_pce_color, then passes them through
// RGB565.
[[nodiscard]] std::uint32_t pce_framebuffer_color(std::uint16_t raw_color) noexcept;

// Splits a screen the host otherwise shows as the emulator's finished picture
// -- a bonus room -- into the scrolled background and its hardware sprites, so
// the presenter can move each sprite between game frames the way it moves the
// stitched table's actors.
//
// The picture a frame produces is drawn from the SAT copied at the end of the
// frame before it, so each update draws the previous update's SAT. The pieces
// are only returned when, put back together, they reproduce the picture pixel
// for pixel; anything they cannot express -- a mid-frame scroll change, or a
// behind-background sprite winning an overlap over a foreground one -- returns
// nothing, and the caller shows the picture itself for that frame. Halves past
// the sixteen a line can show are cut out of their pieces, as hardware drops
// them.
//
// Sprites carry no actor identity, so each is linked to the nearest previous
// sprite of the same size within the table's own sixteen-pixel teleport limit.
// Linked sprites keep a slot and generation, which become the motion frame's
// actor slot and type; a new sprite takes a fresh generation, so it never
// inherits a vanished sprite's motion. A sprite that changes pattern and, with
// it, breaks from its own last step -- a flipper's next pose, drawn eight
// pixels along -- is a pose change, not motion, and takes a fresh generation
// as well, so it is shown as the game drew it instead of slid and blended.
// The sprites cut that way together, with the new and vanished sprites that
// touch them, are one flipper changing pose; when their two poses measure as
// a turn (table_swing), every piece carries it and the presenter swings them.
class ViewportMotionTracker {
public:
    void reset() noexcept;

    [[nodiscard]] std::optional<TableMotionFrame> update(
        const ViewportVideo& video,
        const ViewportPicture& picture);

private:
    struct Piece {
        std::uint8_t slot = 0;
        std::uint8_t generation = 0;
        int x = 0;
        int y = 0;
        std::uint16_t pattern = 0;
        std::uint16_t flags = 0;
        // The step from the linked sprite, or zero for a new pose.
        int step_x = 0;
        int step_y = 0;
    };

    void measure_swings(const std::vector<Piece>& pieces, const std::vector<bool>& linked,
        const std::vector<bool>& claimed, const std::vector<std::ptrdiff_t>& cut_from,
        std::vector<TableMotionSprite>& sprites) const;

    std::vector<std::uint16_t> shown_sat_;
    std::vector<Piece> previous_;
    // The previous update's pieces as drawn, to measure a pose change against.
    std::vector<TableMotionSprite> previous_sprites_;
    std::uint16_t scroll_x_ = 0;
    std::uint16_t scroll_y_ = 0;
    std::uint8_t generation_ = 0;
};

} // namespace devils_crush::render
