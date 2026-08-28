// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/small_soldier_rules.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// Owns the scheduling gap between a confirmed contact and the later
// Small-Soldier actor update. The collision dispatcher only marks a target;
// this queue keeps that mark until the actor-order phase consumes it.
class SmallSoldierRuleScheduler {
public:
    SmallSoldierRuleScheduler(ActorState& actors, ScoreState& score) noexcept;

    // Accepts only a dispatched, marked target that is still an initialized
    // Small Soldier. A duplicate mark is not a second hit.
    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;

    // Consumes the queued contact when the actor scheduler reaches `slot`.
    // A stale queued target is discarded rather than being applied to a later
    // occupant of the same dynamic slot.
    [[nodiscard]] std::optional<SmallSoldierRulesUpdate>
    advance_actor(std::uint8_t slot) noexcept;

    // The subtype-$0F completion transition is a separately scheduled actor
    // phase, so it intentionally bypasses the confirmed-contact queue.
    [[nodiscard]] std::optional<SmallSoldierRulesUpdate>
    complete_animation(std::uint8_t slot) noexcept;

    [[nodiscard]] bool contact_queued(std::uint8_t slot) const noexcept;

private:
    ActorState& actors_;
    ScoreState& score_;
    std::array<bool, ActorState::kSlotCount> queued_contacts_{};
};

} // namespace devils_crush::native_runtime
