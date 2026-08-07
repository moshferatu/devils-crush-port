// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace devils_crush::render {

// The table's packed palette-attribute plane is region-owned.
//
// The game writes the whole 1,536-byte plane once at scene start, then
// rewrites one or two tile rows at a time as the camera scrolls. Over a
// 3,600-frame route, 8,274 of its 8,669 in-play byte changes — 95.4% — land
// on rows the camera is not showing. A full-table decode that reads the plane
// live therefore repaints unrelated parts of the table every time the ball
// travels, which is what makes the cabinet playfield look like it is being
// re-rendered as the ball moves.
//
// This retains the plane per tile row. A row is refreshed only while the
// camera covers it, so the bytes describing a row are always the ones that
// were authoritative when that row was last on screen. Rows the camera has
// not reached keep the scene-start whole-table assignment.
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
