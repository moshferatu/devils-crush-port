// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_effect.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A strict steady snapshot of one live shared type-$8F effect. The source is
// retained so an already-native source actor can be seeded with the exact
// copied appearance and association rather than a raw parallel-array byte.
struct CollisionEffectCapture {
    GameStateView::ActorSlot source_actor;
    GameStateView::ActorSlot effect_actor;
    CollisionEffectState effect;

    [[nodiscard]] bool operator==(const CollisionEffectCapture&) const = default;
};

[[nodiscard]] std::optional<CollisionEffectCapture>
capture_collision_effect(
    const GameStateView& state,
    std::uint8_t effect_slot) noexcept;

} // namespace devils_crush::native_runtime
