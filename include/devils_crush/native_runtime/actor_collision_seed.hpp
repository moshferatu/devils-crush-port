// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

namespace devils_crush::native_runtime {

// Seeds the collision behavior of an already-configured native actor from a
// typed capture snapshot. Behavior selection and slot claiming are left to
// the actor-family layer; this adapter only transfers collision-table state.
[[nodiscard]] bool seed_actor_collision_state(
    ActorState& actors,
    const GameStateView::ActorSlot& source) noexcept;

} // namespace devils_crush::native_runtime
