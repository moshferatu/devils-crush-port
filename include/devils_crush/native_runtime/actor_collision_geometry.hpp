// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// The four decoded bytes at the selected actor-collision descriptor. ROM
// selection remains an asset-boundary concern; this type is the readable
// shape consumed by native geometry.
struct ActorCollisionShapeDescriptor {
    std::int8_t horizontal_offset = 0;
    std::int8_t vertical_offset = 0;
    std::uint8_t horizontal_extent = 0;
    std::uint8_t vertical_extent = 0;

    [[nodiscard]] bool operator==(const ActorCollisionShapeDescriptor&) const
        = default;
};

class MappedRomView;

// The asset-resolution evidence retained with a decoded actor shape. CC1A
// installs the phase-selected primary and secondary geometry addresses as the
// two CC98 table roots after it reads the descriptor, so retain both rather
// than treating the secondary table as incidental metadata.
struct ActorCollisionShapeResolution {
    std::uint8_t directory_index = 0;
    std::uint8_t phase = 0;
    std::array<std::uint16_t, 2> geometry_tables{};
    std::uint16_t descriptor_address = 0;
    std::uint16_t secondary_geometry_address = 0;
    ActorCollisionShapeDescriptor descriptor;

    [[nodiscard]] bool operator==(const ActorCollisionShapeResolution&) const
        = default;
};

// Inputs to the portion of CC1A after the inexpensive vertical rejection.
// `vertical_proximity` is kept rather than recomputed so the caller carries
// forward the one scan source it captured for this collision attempt.
struct ActorCollisionGeometryInput {
    GameStateView::ActorSlot actor;
    ActorVerticalProximity vertical_proximity;
    ActorCollisionShapeDescriptor descriptor;

    [[nodiscard]] bool operator==(const ActorCollisionGeometryInput&) const
        = default;
};

enum class ActorCollisionGeometryDisposition {
    outside_vertical_proximity,
    outside_horizontal_world,
    bounds_ready,
};

// The prepared target window for the shared collision-bound and mask stages.
// A bounds-ready result is still only a candidate; it has not been tested
// against the ball's horizontal coordinate, a shape mask, or a contact rule.
struct ActorCollisionGeometry {
    ActorCollisionGeometryInput input;
    std::uint16_t position_after_ball_bias = 0;
    CollisionBounds bounds;
    ActorCollisionGeometryDisposition disposition =
        ActorCollisionGeometryDisposition::outside_vertical_proximity;

    [[nodiscard]] bool operator==(const ActorCollisionGeometry&) const = default;
};

// Prepares the recovered actor-local target window. CC1A first rejects a
// target outside its signed 127-pixel vertical window. For a near target it
// applies the cartridge's 32-pixel horizontal ball bias; positions outside
// the one-byte table world are rejected before applying descriptor geometry.
// The result then feeds `evaluate_collision_bounds` and mask lookup.
[[nodiscard]] ActorCollisionGeometry prepare_actor_collision_geometry(
    ActorCollisionGeometryInput input) noexcept;

// Resolves an actor's selected collision shape through the four captured
// directory pointers and active ROM mapping. Invalid pointers or an unmapped
// range fail at this asset boundary rather than becoming a clear collision.
[[nodiscard]] std::optional<ActorCollisionShapeResolution>
resolve_actor_collision_shape(
    const MappedRomView& rom,
    const GameStateView::ActorCollisionShapeDirectoryPointers& directories,
    std::uint8_t directory_index,
    std::uint8_t phase);

// The actor-record overload is the normal native call site. It keeps the
// captured directory and phase selectors at the state boundary and prevents
// collision scheduling code from unpacking actor-record fields itself.
[[nodiscard]] std::optional<ActorCollisionShapeResolution>
resolve_actor_collision_shape(
    const MappedRomView& rom,
    const GameStateView::ActorCollisionShapeDirectoryPointers& directories,
    const GameStateView::ActorSlot& actor);

} // namespace devils_crush::native_runtime
