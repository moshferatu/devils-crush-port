// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_map.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::native_runtime {

class MappedRomView;

// Owned, decoded playfield collision data. This is the asset boundary between
// ROM byte ranges and the typed CollisionMap consumed by table physics and
// rules. The two map planes have one byte per tile; the mask plane has eight
// bytes per attribute value.
class CollisionMapAsset {
public:
    [[nodiscard]] CollisionMap view() const noexcept;

private:
    friend std::optional<CollisionMapAsset> decode_collision_map_asset(
        std::span<const std::byte> tile_bytes,
        std::span<const std::byte> attribute_bytes,
        std::span<const std::byte> mask_bytes) noexcept;

    std::vector<std::uint8_t> tile_ids_;
    std::vector<std::uint8_t> mask_indices_;
    std::vector<CollisionMask> masks_;
};

// The currently selected table's ROM layout. Row and mask counts belong to
// table configuration, not to the mutable pointer snapshot in GameStateView.
struct CollisionMapAssetLayout {
    std::uint16_t tile_ids_address = 0;
    std::uint16_t collision_attributes_address = 0;
    std::uint16_t masks_address = 0;
    std::size_t map_rows = 0;
    std::size_t mask_count = 0;

    [[nodiscard]] bool operator==(const CollisionMapAssetLayout&) const
        = default;
};

// Static dimensions discovered from the supported USA image and verified by
// the player-session-6 playfield-probe capture. They belong to the main-table
// asset format; live work RAM supplies only the three selected addresses.
inline constexpr std::size_t kMainPlayfieldCollisionMapRows = 94U;
inline constexpr std::size_t kMainPlayfieldCollisionMaskCount = 250U;

[[nodiscard]] constexpr CollisionMapAssetLayout
main_playfield_collision_map_layout(
    const GameStateView::PlayfieldCollisionTablePointers& pointers) noexcept
{
    return {
        .tile_ids_address = pointers.tile_ids,
        .collision_attributes_address = pointers.collision_attributes,
        .masks_address = pointers.masks,
        .map_rows = kMainPlayfieldCollisionMapRows,
        .mask_count = kMainPlayfieldCollisionMaskCount,
    };
}

// Decodes the playfield's parallel tile-ID and collision-attribute planes.
// All maps must be complete 32-column rows; every referenced mask must be
// present in the supplied mask plane. Invalid ROM ranges fail at this boundary
// rather than becoming an implicit clear tile during native gameplay.
[[nodiscard]] std::optional<CollisionMapAsset> decode_collision_map_asset(
    std::span<const std::byte> tile_bytes,
    std::span<const std::byte> attribute_bytes,
    std::span<const std::byte> mask_bytes) noexcept;

// Resolves a configured table layout through the current MPR map before using
// the ordinary byte-plane decoder. This is the only collision asset path that
// needs logical addresses or bank mapping.
[[nodiscard]] std::optional<CollisionMapAsset> decode_collision_map_asset(
    const MappedRomView& rom,
    CollisionMapAssetLayout layout);

[[nodiscard]] std::optional<CollisionMapAsset>
decode_main_playfield_collision_map(
    const MappedRomView& rom,
    const GameStateView::PlayfieldCollisionTablePointers& pointers);

} // namespace devils_crush::native_runtime
