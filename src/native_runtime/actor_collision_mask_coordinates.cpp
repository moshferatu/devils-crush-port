// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_collision_mask_coordinates.hpp"

namespace devils_crush::native_runtime {

ActorCollisionMaskCoordinates prepare_actor_collision_mask_coordinates(
    const ActorCollisionMaskCoordinateInput input) noexcept
{
    ActorCollisionMaskCoordinates coordinates{.input = input,
                                               .side_flags = input.side_flags};
    if (input.bounds.disposition != CollisionBoundsDisposition::intersecting) {
        return coordinates;
    }

    coordinates.updates_side_flags = input.session_active
        && input.actor_slot < 0x0AU && input.shape_phase != 0x03U;
    if (coordinates.updates_side_flags) {
        coordinates.side_flags = static_cast<std::uint8_t>(
            coordinates.side_flags
            | ((input.actor_slot & 0x01U) != 0U ? 0x01U : 0x80U));
    }

    coordinates.cell = {
        .column = static_cast<std::uint8_t>(
            input.bounds.relative_horizontal & 0x07U),
        .row = static_cast<std::uint8_t>(
            input.bounds.relative_vertical & 0x07U),
    };
    coordinates.horizontal_cell = static_cast<std::uint8_t>(
        input.bounds.relative_horizontal >> 3U);
    coordinates.vertical_cell = static_cast<std::uint8_t>(
        input.bounds.relative_vertical >> 3U);
    coordinates.rounded_horizontal_span = static_cast<std::uint8_t>(
        (static_cast<std::uint16_t>(input.bounds.bounds.horizontal_extent) + 7U)
        >> 3U);
    return coordinates;
}

} // namespace devils_crush::native_runtime
