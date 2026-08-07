// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace devils_crush::render {

// The table's packed palette-attribute plane, retained per tile row.
//
// The game writes the whole 1,536-byte plane once at scene start, then
// rewrites tile rows as play continues. Over a 3,600-frame route, 8,274 of its
// 8,669 in-play byte changes — 95.4% — land on rows the camera is not showing,
// and this class originally refreshed a row only while the camera covered it
// on the theory that off-camera writes belonged to a region being streamed in
// rather than to the row they addressed.
//
// That theory does not survive measurement, and holding rows back caused a
// worse defect than the one it was meant to prevent. The plane is indexed by
// tile row: a write to row N is about row N, and the game keeps animating rows
// it is not currently showing. Freezing them left five consecutive rows at
// world y 440-479 displaying a subpalette from a long-finished animation phase
// for 330 frames each — a tile-aligned rectangular band across the table in
// the wrong colours until the ball travelled far enough to refresh it. Nine
// displayed rows were affected across 2,264 row-frames.
//
// Every row is now refreshed. Scored at the one instant the answer is not a
// matter of opinion — when a row arrives under the camera, where the live
// plane is authoritative for it by definition — reading live agreed on 129 of
// 129 arrivals against the retained copy's 123. `--cabinet-attribute-check`
// is that measurement.
//
// The retained buffer and per-row learned marks remain: they cost one copy,
// they keep the decode reading from a stable span rather than straight out of
// work RAM, and they are where a narrower policy would go if one is ever
// shown to be needed.
class TablePaletteCache {
public:
    static constexpr std::size_t kWidthTiles = 32U;
    static constexpr std::size_t kHeightTiles = 96U;
    static constexpr std::size_t kTilePixels = 8U;
    // Two tiles share one packed byte, and a byte never straddles a row.
    static constexpr std::size_t kBytesPerRow = kWidthTiles / 2U;
    static constexpr std::size_t kByteCount = kBytesPerRow * kHeightTiles;

    void reset() noexcept;

    // Refreshes the rows the camera covers and returns the whole retained
    // plane. `viewport_top_row` locates the camera's top in the decoded
    // source's row space and `viewport_rows` is its height there.
    //
    // A plane shorter than kByteCount is passed straight back, so a caller
    // that cannot supply the full attribute region keeps its old behavior.
    [[nodiscard]] std::span<const std::byte> update(
        std::span<const std::byte> live_plane,
        int viewport_top_row,
        int viewport_rows);

    // Tile rows whose bytes have been refreshed under the camera at least
    // once. The remainder still carry the scene-start assignment.
    [[nodiscard]] std::size_t learned_rows() const noexcept;

private:
    std::vector<std::byte> retained_;
    std::vector<std::uint8_t> learned_;
};

} // namespace devils_crush::render
