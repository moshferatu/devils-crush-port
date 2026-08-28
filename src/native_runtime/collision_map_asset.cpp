// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_map_asset.hpp"

#include "devils_crush/native_runtime/mapped_rom.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace devils_crush::native_runtime {

CollisionMap CollisionMapAsset::view() const noexcept
{
    return {
        .tile_ids = tile_ids_,
        .mask_indices = mask_indices_,
        .masks = masks_,
    };
}

std::optional<CollisionMapAsset> decode_collision_map_asset(
    const std::span<const std::byte> tile_bytes,
    const std::span<const std::byte> attribute_bytes,
    const std::span<const std::byte> mask_bytes) noexcept
{
    if (tile_bytes.empty() || tile_bytes.size() != attribute_bytes.size()
        || tile_bytes.size() % CollisionMap::kColumns != 0U
        || mask_bytes.empty()
        || mask_bytes.size() % CollisionMask::kHeight != 0U) {
        return std::nullopt;
    }

    CollisionMapAsset asset;
    asset.tile_ids_.reserve(tile_bytes.size());
    asset.mask_indices_.reserve(attribute_bytes.size());
    for (const auto value : tile_bytes) {
        asset.tile_ids_.push_back(std::to_integer<std::uint8_t>(value));
    }
    for (const auto value : attribute_bytes) {
        asset.mask_indices_.push_back(std::to_integer<std::uint8_t>(value));
    }

    asset.masks_.reserve(mask_bytes.size() / CollisionMask::kHeight);
    for (std::size_t offset = 0U; offset < mask_bytes.size();
         offset += CollisionMask::kHeight) {
        CollisionMask mask;
        for (std::size_t row = 0U; row < CollisionMask::kHeight; ++row) {
            mask.rows[row] = std::to_integer<std::uint8_t>(
                mask_bytes[offset + row]);
        }
        asset.masks_.push_back(mask);
    }

    const auto highest_index = std::ranges::max(asset.mask_indices_);
    if (highest_index >= asset.masks_.size()) {
        return std::nullopt;
    }
    return asset;
}

std::optional<CollisionMapAsset> decode_collision_map_asset(
    const MappedRomView& rom,
    const CollisionMapAssetLayout layout)
{
    if (layout.map_rows == 0U
        || layout.map_rows
            > std::numeric_limits<std::size_t>::max()
                / CollisionMap::kColumns
        || layout.mask_count == 0U
        || layout.mask_count
            > std::numeric_limits<std::size_t>::max()
                / CollisionMask::kHeight) {
        return std::nullopt;
    }
    const auto map_bytes = layout.map_rows * CollisionMap::kColumns;
    const auto mask_bytes = layout.mask_count * CollisionMask::kHeight;
    const auto tile_ids = rom.range_at(layout.tile_ids_address, map_bytes);
    const auto attributes = rom.range_at(
        layout.collision_attributes_address, map_bytes);
    const auto masks = rom.range_at(layout.masks_address, mask_bytes);
    if (!tile_ids || !attributes || !masks) {
        return std::nullopt;
    }
    return decode_collision_map_asset(*tile_ids, *attributes, *masks);
}

std::optional<CollisionMapAsset> decode_main_playfield_collision_map(
    const MappedRomView& rom,
    const GameStateView::PlayfieldCollisionTablePointers& pointers)
{
    return decode_collision_map_asset(
        rom, main_playfield_collision_map_layout(pointers));
}

} // namespace devils_crush::native_runtime
