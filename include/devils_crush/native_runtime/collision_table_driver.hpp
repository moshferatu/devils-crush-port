// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/collision_table_asset.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// Resolves and applies one collision-table update for an actor. This is the
// narrow composition boundary between an actor's ROM-derived table program
// and its owned collision state: MPR mapping never enters ActorState, and the
// decoder never mutates an actor record directly.
[[nodiscard]] std::optional<ActorCollisionTableResolutionUpdate>
advance_actor_collision_table_from_asset(
    ActorState& actors,
    std::uint8_t slot,
    const MappedRomView& rom) noexcept;

} // namespace devils_crush::native_runtime
