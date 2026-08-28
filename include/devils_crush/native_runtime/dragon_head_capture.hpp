// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/dragon_head.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A collision-ready Dragon's Head captured at its 02:5A4B steady dispatcher.
// The family-local hit counter is only interpreted after the fixed identity
// confirms its meaning; terminal and presentation-transition snapshots remain
// with their respective owners.
struct DragonHeadCapture {
    GameStateView::ActorSlot actor;
    DragonHeadState rule_state;

    [[nodiscard]] bool operator==(const DragonHeadCapture&) const = default;
};

[[nodiscard]] std::optional<DragonHeadCapture> capture_dragon_head_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
