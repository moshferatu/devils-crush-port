// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// The generic portion of a live shared-motion actor. Its position, signed
// velocity, and due state are common scheduler data; integration and steering
// remain behavior-specific policy and are deliberately not inferred here.
struct SharedActorMotionCapture {
    GameStateView::ActorSlot actor;
    ActorMotionState motion;
    bool due_for_update = false;

    [[nodiscard]] bool operator==(const SharedActorMotionCapture&) const = default;
};

// Captures only an active actor record. The work-RAM view already validates
// field widths, and this boundary converts its unsigned world positions to the
// native 24.8 form without exposing RAM offsets to ActorState.
[[nodiscard]] std::optional<SharedActorMotionCapture>
capture_shared_actor_motion(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

// Seeds an already-owned shared-motion actor. This does not configure motion
// policy, collision behavior, or a family rule; those remain separate native
// behavior-selection decisions.
[[nodiscard]] bool seed_shared_actor_motion(
    ActorState& actors,
    const SharedActorMotionCapture& source) noexcept;

} // namespace devils_crush::native_runtime
