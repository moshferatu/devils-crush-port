// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/collision_mask.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

// Inputs captured after an actor shape passed CC1A's rectangular bounds
// check. `session_active` and `side_flags` belong to the table/session
// domain; keeping them explicit prevents this local mask preparation from
// acquiring a dependency on emulator work RAM.
struct ActorCollisionMaskCoordinateInput {
    std::uint8_t actor_slot = 0;
    std::uint8_t shape_phase = 0;
    bool session_active = false;
    std::uint8_t side_flags = 0;
    CollisionBoundsEvaluation bounds;

    [[nodiscard]] bool operator==(
        const ActorCollisionMaskCoordinateInput&) const = default;
};

// CC41-CC97's readable output. `cell` addresses the selected bit within the
// actor's eight-by-eight collision mask; the cell and span values are also
// retained because the following ROM lookup uses their unrounded coordinates.
struct ActorCollisionMaskCoordinates {
    ActorCollisionMaskCoordinateInput input;
    bool updates_side_flags = false;
    std::uint8_t side_flags = 0;
    CollisionMaskCell cell;
    std::uint8_t horizontal_cell = 0;
    std::uint8_t vertical_cell = 0;
    std::uint8_t rounded_horizontal_span = 0;

    [[nodiscard]] bool operator==(const ActorCollisionMaskCoordinates&) const
        = default;
};

// Derives the local mask bit and the follow-on ROM-table coordinates for an
// in-range actor collision. Callers must pass a bounds-intersecting result;
// rejected bounds deliberately produce no mask coordinates or side effect.
[[nodiscard]] ActorCollisionMaskCoordinates
prepare_actor_collision_mask_coordinates(
    ActorCollisionMaskCoordinateInput input) noexcept;

} // namespace devils_crush::native_runtime
