// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_mask.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace devils_crush::native_runtime {

// The playfield collision map is a row-major, 32-tile-wide grid. Its parallel
// tile and attribute planes mirror the game's table data: the tile ID is
// retained for rules while the attribute selects one ROM-derived eight-by-eight
// solidity mask. Map-relative pixel coordinates intentionally keep
// camera/scroll conversion outside this layer.
struct CollisionMap {
    static constexpr std::size_t kColumns = 32;
    static constexpr std::uint8_t kTilePixels = CollisionMask::kWidth;

    std::span<const std::uint8_t> tile_ids;
    std::span<const std::uint8_t> mask_indices;
    std::span<const CollisionMask> masks;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::size_t rows() const noexcept;
};

struct CollisionMapPoint {
    std::uint16_t horizontal_pixels = 0;
    std::uint16_t vertical_pixels = 0;

    [[nodiscard]] bool operator==(const CollisionMapPoint&) const = default;
};

enum class CollisionMapDisposition {
    outside_map,
    missing_mask,
    empty,
    solid,
};

struct CollisionMapEvaluation {
    CollisionMapPoint point;
    std::uint8_t tile_column = 0;
    std::uint8_t tile_row = 0;
    std::uint8_t tile_id = 0;
    std::uint8_t mask_index = 0;
    CollisionMaskEvaluation mask;
    CollisionMapDisposition disposition = CollisionMapDisposition::outside_map;

    [[nodiscard]] bool operator==(const CollisionMapEvaluation&) const
        = default;
};

// Selects the tile mask under a map-relative point and evaluates its local
// cell. A malformed map, an out-of-range point, or a missing mask is never a
// contact; this makes table-data validation explicit at the asset boundary.
[[nodiscard]] CollisionMapEvaluation evaluate_collision_map(
    const CollisionMap& map,
    CollisionMapPoint point) noexcept;

// The playfield probe uses the game's world coordinates: the table begins 32
// pixels below the vertical origin and is one byte (256 pixels) wide. This
// adapter keeps that coordinate convention out of ball and map ownership.
struct PlayfieldCollisionProbe {
    std::uint16_t horizontal_pixels = 0;
    std::uint16_t vertical_pixels = 0;

    [[nodiscard]] bool operator==(const PlayfieldCollisionProbe&) const
        = default;
};

enum class PlayfieldCollisionDisposition {
    outside_playfield,
    outside_map,
    missing_mask,
    empty,
    solid,
};

struct PlayfieldCollisionEvaluation {
    PlayfieldCollisionProbe probe;
    CollisionMapPoint map_point;
    CollisionMapEvaluation map;
    PlayfieldCollisionDisposition disposition =
        PlayfieldCollisionDisposition::outside_playfield;

    [[nodiscard]] bool operator==(const PlayfieldCollisionEvaluation&) const
        = default;
};

// Converts a table-world probe to its map-relative location and evaluates the
// selected tile. A point above the playfield or beyond its byte-wide table
// width is not a map lookup candidate.
[[nodiscard]] PlayfieldCollisionEvaluation evaluate_playfield_collision(
    const CollisionMap& map,
    PlayfieldCollisionProbe probe) noexcept;

} // namespace devils_crush::native_runtime
