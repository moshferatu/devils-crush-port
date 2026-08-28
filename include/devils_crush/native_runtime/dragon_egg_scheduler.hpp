// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/dragon_egg.hpp"
#include "devils_crush/native_runtime/dragon_egg_hatchling.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// Carries one confirmed fixed-egg hit to its slot and runs the independent
// group controller at `$1F` before the cluster's `$20-$25` actor turns.
class DragonEggRuleScheduler {
public:
    explicit DragonEggRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<DragonEggRulesUpdate> advance_egg_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<DragonEggCompletionRulesUpdate>
    advance_controller_actor(std::uint8_t slot) noexcept;
    // The separate timed reset owner calls this when it reaches the source's
    // reset signal. Fixed targets then reinitialize at their own actor turns.
    [[nodiscard]] std::optional<DragonEggGroupResetStep>
    begin_group_reset() noexcept;
    [[nodiscard]] std::optional<DragonEggFixedResetStep>
    advance_fixed_reset_actor(std::uint8_t slot) noexcept;
    // The caller owns generic dynamic-slot reservation. Once it succeeds,
    // this commits the egg's pending handoff and owns the resulting child.
    [[nodiscard]] std::optional<DragonEggHatchlingAllocationStep>
    allocate_hatchling(std::uint8_t source_egg_slot, std::uint8_t dynamic_slot) noexcept;
    [[nodiscard]] std::optional<DragonEggHatchlingRulesUpdate>
    advance_hatchling_actor(std::uint8_t dynamic_slot) noexcept;
    // Restores a verified in-flight child captured after allocation. Its
    // source fixed egg is already disabled, so this never replays allocation.
    [[nodiscard]] bool seed_hatchling(DragonEggHatchlingState hatchling) noexcept;
    void seed(DragonEggClusterState state) noexcept;
    [[nodiscard]] const DragonEggClusterState* cluster() const noexcept;
    [[nodiscard]] const DragonEggHatchlingState* hatchling(
        std::uint8_t dynamic_slot) const noexcept;

private:
    ScoreState& score_;
    std::optional<DragonEggClusterState> cluster_;
    std::optional<std::uint8_t> contact_slot_;
    std::optional<std::uint8_t> hatchling_contact_slot_;
    std::array<std::optional<DragonEggHatchlingState>, kDragonEggCount>
        hatchlings_{};
};

} // namespace devils_crush::native_runtime
