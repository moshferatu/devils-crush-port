// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bonus_stage_4_linked_target.hpp"
#include "devils_crush/native_runtime/contact_dispatch.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// Owns one allocator-selected BS-4 linked target between the confirmed
// collision phase and its later actor-order update. Child actor ownership is
// deliberately returned as typed allocation results: the generic allocator
// still selects their dynamic slots.
class BonusStage4LinkedTargetRuleScheduler {
public:
    explicit BonusStage4LinkedTargetRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool seed(BonusStage4LinkedTargetState target) noexcept;
    [[nodiscard]] bool seed(
        BonusStage4LinkedTargetState target,
        std::array<std::optional<BonusStage4OrbitingChildState>, 4U> children) noexcept;
    [[nodiscard]] const BonusStage4LinkedTargetState* target() const noexcept;
    [[nodiscard]] const BonusStage4OrbitingChildState* orbiting_child(
        std::uint8_t slot) const noexcept;
    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<BonusStage4LinkedTargetRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    // A clear steady target advances its recovered 05:6B88/6CB1 countdown at
    // its actor turn. The random-vector selection itself remains the shared
    // motion owner's next seam.
    [[nodiscard]] std::optional<BonusStage4LinkedTargetMotionStep>
    advance_motion_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<BonusStage4OrbitingChildFrameUpdate>
    advance_orbiting_child_actor(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<BonusStage4OrbitingChildFrameUpdate>
    advance_orbiting_child_orbit_actor(std::uint8_t slot) noexcept;

    // This is called only by the presentation scheduler after the recovered
    // 05:6C21 handoff. It makes 05:6C24's four child requests available while
    // retaining the target's collision latch until all four are consumed.
    [[nodiscard]] std::optional<BonusStage4LinkedTargetHitPresentationStep>
    complete_hit_presentation() noexcept;
    [[nodiscard]] std::optional<BonusStage4OrbitingChildAllocationStep>
    allocate_orbiting_child(std::uint8_t dynamic_slot) noexcept;

private:
    ScoreState& score_;
    std::optional<BonusStage4LinkedTargetState> target_;
    bool contact_queued_ = false;
    std::array<std::optional<BonusStage4OrbitingChildState>, 4U> orbiting_children_;
    std::array<bool, 4U> orbiting_child_contacts_queued_{};
};

} // namespace devils_crush::native_runtime
