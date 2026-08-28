// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_collision_mask_coordinates.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

class MappedRomView;

// The complete, readable result of CC98's three mapped-ROM lookups. CC95
// first turns the horizontal and vertical mask cells into a row-major index;
// the first table byte is retained because the original reads it before
// selecting the eight-row bitmap.
struct ActorCollisionMaskResolution {
    ActorCollisionMaskCoordinates coordinates;
    std::uint8_t table_index = 0;
    std::uint8_t first_table_value = 0;
    std::uint8_t mask_index = 0;
    std::uint16_t mask_address = 0;
    CollisionMask mask;
    CollisionMaskEvaluation evaluation;

    [[nodiscard]] bool operator==(const ActorCollisionMaskResolution&) const
        = default;
};

// Resolves the target mask selected by a bounds-ready actor collision. An
// unmapped table, malformed map, or bounds rejection returns no result: those
// cases must not silently become a clear contact.
[[nodiscard]] std::optional<ActorCollisionMaskResolution>
resolve_actor_collision_mask(
    const MappedRomView& rom,
    const GameStateView::ActorCollisionMaskTablePointers& pointers,
    ActorCollisionMaskCoordinates coordinates);

} // namespace devils_crush::native_runtime
