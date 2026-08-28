// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// Coordinates in the collision-mask space used by the table. Horizontal
// values intentionally wrap at one byte; vertical values use the paired
// 16-bit coordinate retained by the original collision calculation.
struct CollisionProbe {
    std::uint8_t horizontal = 0;
    std::uint16_t vertical = 0;

    [[nodiscard]] bool operator==(const CollisionProbe&) const = default;
};

// An exclusive, one-sided target window. A probe is inside only when it is at
// or after the origin on both axes and its wrapped relative coordinate is
// smaller than the corresponding extent.
struct CollisionBounds {
    std::uint8_t horizontal_origin = 0;
    std::uint16_t vertical_origin = 0;
    std::uint8_t horizontal_extent = 0;
    std::uint8_t vertical_extent = 0;

    [[nodiscard]] bool operator==(const CollisionBounds&) const = default;
};

enum class CollisionBoundsDisposition {
    outside_horizontal,
    outside_vertical,
    intersecting,
};

struct CollisionBoundsEvaluation {
    CollisionProbe probe;
    CollisionBounds bounds;
    std::uint8_t relative_horizontal = 0;
    std::uint16_t relative_vertical = 0;
    CollisionBoundsDisposition disposition =
        CollisionBoundsDisposition::outside_horizontal;

    [[nodiscard]] bool operator==(const CollisionBoundsEvaluation&) const
        = default;
};

// The actor scan's first geometry gate accepts targets within 127 pixels above
// or below the ball. The signed difference is retained for the later
// descriptor and mask stages, which choose a shape only after this cheap
// vertical rejection.
struct ActorVerticalProximity {
    std::uint16_t ball_vertical = 0;
    std::uint16_t actor_vertical = 0;
    std::int16_t signed_difference = 0;
    bool intersecting = false;

    [[nodiscard]] bool operator==(const ActorVerticalProximity&) const = default;
};

// Performs the native collision sweep's fast rectangular rejection. This is
// deliberately separate from mask lookup: an intersecting result is a
// candidate for target-specific mask and contact processing, not yet a
// confirmed collision.
[[nodiscard]] CollisionBoundsEvaluation evaluate_collision_bounds(
    CollisionProbe probe,
    CollisionBounds bounds) noexcept;

// Evaluates the recovered vertical gate shared by actor collision scans. The
// original accepts signed differences in [-127, 127] and rejects both
// endpoints at magnitude 128, before reading any actor geometry or mask data.
[[nodiscard]] ActorVerticalProximity evaluate_actor_vertical_proximity(
    std::uint16_t ball_vertical,
    std::uint16_t actor_vertical) noexcept;

} // namespace devils_crush::native_runtime
