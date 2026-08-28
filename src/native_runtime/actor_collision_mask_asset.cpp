// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_collision_mask_asset.hpp"

#include "devils_crush/native_runtime/mapped_rom.hpp"

#include <cstddef>

namespace devils_crush::native_runtime {

std::optional<ActorCollisionMaskResolution> resolve_actor_collision_mask(
    const MappedRomView& rom,
    const GameStateView::ActorCollisionMaskTablePointers& pointers,
    const ActorCollisionMaskCoordinates coordinates)
{
    if (coordinates.input.bounds.disposition
        != CollisionBoundsDisposition::intersecting) {
        return std::nullopt;
    }

    // CC95 scales the vertical cell by the rounded width of the actor's
    // collision shape before CC98 adds the horizontal cell.  The table is a
    // row-major grid of eight-by-eight mask cells, not a one-cell-wide strip.
    const auto table_index = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(coordinates.vertical_cell)
            * coordinates.rounded_horizontal_span
        + coordinates.horizontal_cell);
    const auto first_byte = rom.byte_at(static_cast<std::uint16_t>(
        pointers.first_table + table_index));
    const auto mask_index = rom.byte_at(static_cast<std::uint16_t>(
        pointers.second_table + static_cast<std::uint8_t>(table_index + 4U)));
    if (!first_byte || !mask_index) {
        return std::nullopt;
    }

    const auto mask_address = static_cast<std::uint16_t>(
        pointers.masks + (static_cast<std::uint16_t>(
                              std::to_integer<std::uint8_t>(*mask_index))
                          << 3U));
    const auto mask_rows = rom.range_at(mask_address, CollisionMask::kHeight);
    if (!mask_rows) {
        return std::nullopt;
    }

    ActorCollisionMaskResolution resolution{
        .coordinates = coordinates,
        .table_index = table_index,
        .first_table_value = std::to_integer<std::uint8_t>(*first_byte),
        .mask_index = std::to_integer<std::uint8_t>(*mask_index),
        .mask_address = mask_address,
    };
    for (std::size_t row = 0U; row < resolution.mask.rows.size(); ++row) {
        resolution.mask.rows[row] =
            std::to_integer<std::uint8_t>((*mask_rows)[row]);
    }
    resolution.evaluation = evaluate_collision_mask(
        resolution.mask, resolution.coordinates.cell);
    return resolution;
}

} // namespace devils_crush::native_runtime
