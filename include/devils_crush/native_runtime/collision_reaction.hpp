// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// Per-target state for the short collision-reaction sequence. A table rule
// chooses the event when it confirms a target hit; the actor owns the phase
// and its four-tick cadence thereafter. This is visual/behavioral target
// state, not the ball's velocity response.
struct CollisionReactionState {
    std::uint8_t collision_event = 0;
    std::uint8_t phase = 0;
    std::uint8_t ticks_until_next_phase = 0;

    [[nodiscard]] bool operator==(const CollisionReactionState&) const = default;
};

struct CollisionReactionStep {
    CollisionReactionState previous;
    CollisionReactionState state;
    std::uint8_t response_code = 0;
    bool phase_advanced = false;

    [[nodiscard]] bool operator==(const CollisionReactionStep&) const = default;
};

// Produces the current reaction code from the event and low two phase bits,
// then advances the target's four-tick reaction cadence. Unsigned wrapping is
// deliberate because an uninitialized zero countdown is a distinct cartridge
// state, not an implicit reload.
[[nodiscard]] CollisionReactionStep advance_collision_reaction(
    CollisionReactionState state) noexcept;

} // namespace devils_crush::native_runtime
