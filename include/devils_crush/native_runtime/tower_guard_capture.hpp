// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/tower_guard.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A fixed Tower Guard capture at its 03:60A6 steady selector dispatcher. The
// selector is derived from the Tower-owned gate rather than copied from the
// Guard's implementation-local completion field.
struct TowerGuardCapture {
    GameStateView::ActorSlot actor;
    TowerGuardSelectorStep selector;

    [[nodiscard]] bool operator==(const TowerGuardCapture&) const = default;
};

// Extracts the Guard only while its fixed steady dispatcher owns selector
// updates. Its independent mask-collision response stays outside this capture
// boundary; a pending collision latch therefore does not invalidate a valid
// selector seed.
[[nodiscard]] std::optional<TowerGuardCapture> capture_tower_guard_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
