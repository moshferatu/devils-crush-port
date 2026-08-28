// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_detection.hpp"

#include <bit>

namespace devils_crush::native_runtime {

CollisionBoundsEvaluation evaluate_collision_bounds(
    const CollisionProbe probe,
    const CollisionBounds bounds) noexcept
{
    CollisionBoundsEvaluation evaluation{
        .probe = probe,
        .bounds = bounds,
        .relative_horizontal = static_cast<std::uint8_t>(
            probe.horizontal - bounds.horizontal_origin),
        .relative_vertical = static_cast<std::uint16_t>(
            probe.vertical - bounds.vertical_origin),
    };

    if (evaluation.relative_horizontal >= bounds.horizontal_extent) {
        evaluation.disposition = CollisionBoundsDisposition::outside_horizontal;
    } else if (evaluation.relative_vertical >= bounds.vertical_extent) {
        evaluation.disposition = CollisionBoundsDisposition::outside_vertical;
    } else {
        evaluation.disposition = CollisionBoundsDisposition::intersecting;
    }
    return evaluation;
}

ActorVerticalProximity evaluate_actor_vertical_proximity(
    const std::uint16_t ball_vertical,
    const std::uint16_t actor_vertical) noexcept
{
    const auto difference = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(ball_vertical - actor_vertical));
    return {
        .ball_vertical = ball_vertical,
        .actor_vertical = actor_vertical,
        .signed_difference = difference,
        .intersecting = difference >= -127 && difference <= 127,
    };
}

} // namespace devils_crush::native_runtime
