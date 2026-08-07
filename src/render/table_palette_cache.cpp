// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/table_palette_cache.hpp"

#include <algorithm>

namespace devils_crush::render {

void TablePaletteCache::reset() noexcept
{
    retained_.clear();
    learned_.clear();
}

std::span<const std::byte> TablePaletteCache::update(
    const std::span<const std::byte> live_plane,
    const int viewport_top_row,
    const int viewport_rows)
{
    if (live_plane.size() < kByteCount) {
        return live_plane;
    }

    // The first sight of the plane is the scene-start whole-table assignment,
    // which is the only moment every row is simultaneously authoritative.
    if (retained_.size() != kByteCount) {
        retained_.assign(
            live_plane.begin(),
            live_plane.begin() + static_cast<std::ptrdiff_t>(kByteCount));
        learned_.assign(kHeightTiles, std::uint8_t{0});
    }

    // Refreshing only under the camera freezes a row on whatever the game had
    // written when it last scrolled past. The game keeps animating rows it is
    // not showing -- flashing table features rewrite their own attribute rows
    // continuously -- so a row can sit for hundreds of frames displaying a
    // subpalette from a phase that ended long ago. On the full-table view that
    // reads as a rectangular band, tile-row aligned and eight pixels per row,
    // in the wrong colours until the ball travels far enough to refresh it.
    //
    // Measured on the census route, five consecutive rows at world y 440-479
    // spent 330 frames each in that state, and nine displayed rows were
    // affected across 2,264 row-frames.
    //
    // The plane is indexed by tile row and a write to row N is about row N, so
    // every row is refreshed. Scored at the one instant the answer is not a
    // matter of opinion -- when a row arrives under the camera, where the live
    // plane is authoritative for it by definition -- reading live agreed on
    // 129 of 129 arrivals against the retained copy's 123.
    for (std::size_t row = 0U; row < kHeightTiles; ++row) {
        const auto offset = static_cast<std::ptrdiff_t>(row * kBytesPerRow);
        std::copy_n(
            live_plane.begin() + offset,
            kBytesPerRow,
            retained_.begin() + offset);
    }

    if (viewport_rows > 0) {
        const auto viewport_bottom_row = viewport_top_row + viewport_rows;
        for (std::size_t row = 0U; row < kHeightTiles; ++row) {
            const auto row_top = static_cast<int>(row * kTilePixels);
            const auto row_bottom = row_top + static_cast<int>(kTilePixels);
            if (row_bottom <= viewport_top_row
                || row_top >= viewport_bottom_row) {
                continue;
            }
            learned_[row] = 1U;
        }
    }

    return retained_;
}

std::size_t TablePaletteCache::learned_rows() const noexcept
{
    return static_cast<std::size_t>(
        std::count(learned_.begin(), learned_.end(), std::uint8_t{1}));
}

} // namespace devils_crush::render
