// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bonus_stage_4_linked_target.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A steady BS-4 orbiting child proves its identity through the recovered
// type-$C2/$6D27 dispatch, its parent slot, normalized phase, and exact
// parent-relative ROM-table position. Contact and parent-loss paths are not
// captured here because each leaves the clear orbit owner.
struct BonusStage4OrbitingChildCapture {
    GameStateView::ActorSlot actor;
    BonusStage4OrbitingChildState state;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildCapture&) const = default;
};

[[nodiscard]] std::optional<BonusStage4OrbitingChildCapture>
capture_bonus_stage_4_orbiting_child(const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
