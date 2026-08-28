// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_collision_geometry.hpp"

#include "devils_crush/native_runtime/mapped_rom.hpp"

#include <bit>

namespace devils_crush::native_runtime {

ActorCollisionGeometry prepare_actor_collision_geometry(
    const ActorCollisionGeometryInput input) noexcept
{
    ActorCollisionGeometry geometry{.input = input};
    if (!input.vertical_proximity.intersecting) {
        return geometry;
    }

    geometry.position_after_ball_bias = static_cast<std::uint16_t>(
        input.actor.collision_table.position_x - 0x0020U);
    if ((geometry.position_after_ball_bias & 0xFF00U) != 0U) {
        geometry.disposition =
            ActorCollisionGeometryDisposition::outside_horizontal_world;
        return geometry;
    }

    geometry.bounds = {
        .horizontal_origin = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(geometry.position_after_ball_bias)
            + input.descriptor.horizontal_offset),
        .vertical_origin = static_cast<std::uint16_t>(
            input.actor.collision_table.position_y
            + input.descriptor.vertical_offset),
        .horizontal_extent = input.descriptor.horizontal_extent,
        .vertical_extent = input.descriptor.vertical_extent,
    };
    geometry.disposition = ActorCollisionGeometryDisposition::bounds_ready;
    return geometry;
}

std::optional<ActorCollisionShapeResolution> resolve_actor_collision_shape(
    const MappedRomView& rom,
    const GameStateView::ActorCollisionShapeDirectoryPointers& directories,
    const std::uint8_t directory_index,
    const std::uint8_t phase)
{
    const auto byte_at_index = [&rom](
                                   const std::uint16_t base,
                                   const std::uint8_t index)
        -> std::optional<std::uint8_t> {
        const auto value = rom.byte_at(static_cast<std::uint16_t>(base + index));
        return value ? std::optional<std::uint8_t>{
                           std::to_integer<std::uint8_t>(*value)}
                     : std::nullopt;
    };

    std::array<std::uint8_t, 4> directory_bytes{};
    for (std::size_t index = 0U; index < directory_bytes.size(); ++index) {
        const auto value = byte_at_index(directories.tables[index], directory_index);
        if (!value) {
            return std::nullopt;
        }
        directory_bytes[index] = *value;
    }

    ActorCollisionShapeResolution resolution{
        .directory_index = directory_index,
        .phase = phase,
        .geometry_tables = {
            static_cast<std::uint16_t>(directory_bytes[0]
                | (static_cast<std::uint16_t>(directory_bytes[1]) << 8U)),
            static_cast<std::uint16_t>(directory_bytes[2]
                | (static_cast<std::uint16_t>(directory_bytes[3]) << 8U)),
        },
    };
    const auto phase_index = static_cast<std::uint8_t>(phase << 1U);
    const auto descriptor_low = byte_at_index(resolution.geometry_tables[0], phase_index);
    const auto descriptor_high = byte_at_index(
        resolution.geometry_tables[0], static_cast<std::uint8_t>(phase_index + 1U));
    const auto secondary_low = byte_at_index(resolution.geometry_tables[1], phase_index);
    const auto secondary_high = byte_at_index(
        resolution.geometry_tables[1], static_cast<std::uint8_t>(phase_index + 1U));
    if (!descriptor_low || !descriptor_high || !secondary_low || !secondary_high) {
        return std::nullopt;
    }
    resolution.descriptor_address = static_cast<std::uint16_t>(
        *descriptor_low | (static_cast<std::uint16_t>(*descriptor_high) << 8U));
    resolution.secondary_geometry_address = static_cast<std::uint16_t>(
        *secondary_low | (static_cast<std::uint16_t>(*secondary_high) << 8U));

    const auto bytes = rom.range_at(resolution.descriptor_address, 4U);
    if (!bytes) {
        return std::nullopt;
    }
    resolution.descriptor = {
        .horizontal_offset = std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>((*bytes)[0])),
        .vertical_offset = std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>((*bytes)[1])),
        .horizontal_extent = std::to_integer<std::uint8_t>((*bytes)[2]),
        .vertical_extent = std::to_integer<std::uint8_t>((*bytes)[3]),
    };
    return resolution;
}

std::optional<ActorCollisionShapeResolution> resolve_actor_collision_shape(
    const MappedRomView& rom,
    const GameStateView::ActorCollisionShapeDirectoryPointers& directories,
    const GameStateView::ActorSlot& actor)
{
    return resolve_actor_collision_shape(
        rom,
        directories,
        actor.collision_shape_directory_index,
        actor.collision_shape_phase);
}

} // namespace devils_crush::native_runtime
