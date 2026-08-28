// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/tower.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// The collision-ready fixed Tower at its 03:60C3 steady dispatcher. Pending
// collision and 03:611C/03:611F presentation states are not steady captures.
struct TowerCapture {
    GameStateView::ActorSlot actor;
    TowerState rule_state;

    [[nodiscard]] bool operator==(const TowerCapture&) const = default;
};

[[nodiscard]] std::optional<TowerCapture> capture_tower_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
