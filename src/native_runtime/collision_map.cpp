// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_map.hpp"

namespace devils_crush::native_runtime {

bool CollisionMap::valid() const noexcept
{
    return !tile_ids.empty() && tile_ids.size() == mask_indices.size()
        && tile_ids.size() % kColumns == 0U;
}

std::size_t CollisionMap::rows() const noexcept
{
    return valid() ? mask_indices.size() / kColumns : 0U;
}

CollisionMapEvaluation evaluate_collision_map(
    const CollisionMap& map,
    const CollisionMapPoint point) noexcept
{
    CollisionMapEvaluation evaluation{.point = point};
    if (!map.valid()) {
        return evaluation;
    }

    const auto tile_column = point.horizontal_pixels / CollisionMap::kTilePixels;
    const auto tile_row = point.vertical_pixels / CollisionMap::kTilePixels;
    if (tile_column >= CollisionMap::kColumns || tile_row >= map.rows()) {
        return evaluation;
    }

    evaluation.tile_column = static_cast<std::uint8_t>(tile_column);
    evaluation.tile_row = static_cast<std::uint8_t>(tile_row);
    const auto map_index = tile_row * CollisionMap::kColumns + tile_column;
    evaluation.tile_id = map.tile_ids[map_index];
    evaluation.mask_index = map.mask_indices[map_index];
    if (evaluation.mask_index >= map.masks.size()) {
        evaluation.disposition = CollisionMapDisposition::missing_mask;
        return evaluation;
    }

    evaluation.mask = evaluate_collision_mask(
        map.masks[evaluation.mask_index],
        CollisionMaskCell{
            .column = static_cast<std::uint8_t>(
                point.horizontal_pixels % CollisionMap::kTilePixels),
            .row = static_cast<std::uint8_t>(
                point.vertical_pixels % CollisionMap::kTilePixels),
        });
    evaluation.disposition = evaluation.mask.disposition
            == CollisionMaskDisposition::solid
        ? CollisionMapDisposition::solid
        : CollisionMapDisposition::empty;
    return evaluation;
}

PlayfieldCollisionEvaluation evaluate_playfield_collision(
    const CollisionMap& map,
    const PlayfieldCollisionProbe probe) noexcept
{
    constexpr std::uint16_t kPlayfieldTopPixels = 32U;
    constexpr std::uint16_t kPlayfieldWidthPixels =
        CollisionMap::kColumns * CollisionMap::kTilePixels;

    PlayfieldCollisionEvaluation evaluation{.probe = probe};
    if (probe.horizontal_pixels >= kPlayfieldWidthPixels
        || probe.vertical_pixels < kPlayfieldTopPixels) {
        return evaluation;
    }

    evaluation.map_point = {
        .horizontal_pixels = probe.horizontal_pixels,
        .vertical_pixels = static_cast<std::uint16_t>(
            probe.vertical_pixels - kPlayfieldTopPixels),
    };
    evaluation.map = evaluate_collision_map(map, evaluation.map_point);
    switch (evaluation.map.disposition) {
    case CollisionMapDisposition::outside_map:
        evaluation.disposition = PlayfieldCollisionDisposition::outside_map;
        break;
    case CollisionMapDisposition::missing_mask:
        evaluation.disposition = PlayfieldCollisionDisposition::missing_mask;
        break;
    case CollisionMapDisposition::empty:
        evaluation.disposition = PlayfieldCollisionDisposition::empty;
        break;
    case CollisionMapDisposition::solid:
        evaluation.disposition = PlayfieldCollisionDisposition::solid;
        break;
    }
    return evaluation;
}

} // namespace devils_crush::native_runtime
