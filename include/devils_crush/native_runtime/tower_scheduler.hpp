// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/tower.hpp"

#include <optional>

namespace devils_crush::native_runtime {

class TowerRuleScheduler {
public:
    explicit TowerRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<TowerRulesUpdate> advance_actor(
        std::uint8_t slot,
        bool center_insect_source_already_set) noexcept;
    [[nodiscard]] std::optional<TowerPresentationStep>
    advance_presentation(std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<TowerBonusStageSelectionStep>
    advance_bonus_stage_selection(
        std::uint8_t slot,
        bool negative_collision_latched) noexcept;
    void seed(TowerState state) noexcept;
    [[nodiscard]] const TowerState& tower() const noexcept;

private:
    ScoreState& score_;
    TowerState tower_{};
    bool contact_queued_ = false;
};

} // namespace devils_crush::native_runtime
