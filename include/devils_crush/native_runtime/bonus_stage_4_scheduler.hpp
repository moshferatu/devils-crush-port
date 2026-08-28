// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bonus_stage_4.hpp"
#include "devils_crush/native_runtime/contact_dispatch.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// Delivers a confirmed BS-4 target contact at its fixed actor-order turn.
// The state is opt-in: normal-table slots can share these numeric positions,
// so no objective is claimed until the bonus-stage owner explicitly seeds it.
class BonusStage4RuleScheduler {
public:
    explicit BonusStage4RuleScheduler(ScoreState& score) noexcept;

    void seed(BonusStage4State state) noexcept;
    [[nodiscard]] const BonusStage4State* stage() const noexcept;
    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<BonusStage4ObjectiveRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;

private:
    ScoreState& score_;
    std::optional<BonusStage4State> stage_;
    std::optional<std::uint8_t> pending_objective_index_;
};

} // namespace devils_crush::native_runtime
