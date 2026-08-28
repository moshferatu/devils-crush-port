// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_response.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

class MappedRomView;

// The decoded D081 contact basis. Keeping the orientation byte and table
// index alongside the coefficients preserves the source evidence while the
// rest of native gameplay only consumes the named basis.
struct CollisionResponseBasisResolution {
    std::uint8_t contact_orientation = 0;
    std::uint8_t table_index = 0;
    CollisionResponseBasis basis;

    [[nodiscard]] bool operator==(const CollisionResponseBasisResolution&) const
        = default;
};

// Decodes D081's two signed 8.8 coefficients from the phase-indexed geometry
// table at 01:D2AE. The current MPR mapping remains confined to MappedRomView;
// an unmapped or incomplete table is an explicit adapter failure.
[[nodiscard]] std::optional<CollisionResponseBasisResolution>
decode_collision_response_basis(
    const MappedRomView& rom,
    std::uint8_t contact_orientation) noexcept;

// Combines CF33/CFD8's contact-orientation selector with the captured table
// latches. This turns the original bit-field and global bytes into the typed
// response configuration consumed by the readable response kernel.
[[nodiscard]] CollisionResponseConfig make_collision_response_config(
    std::uint8_t contact_orientation,
    const GameStateView::CollisionResponseTableState& table_state) noexcept;

} // namespace devils_crush::native_runtime
