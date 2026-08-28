// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/front_spider.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// Carries a dispatched Front-Spider contact to its fixed actor-order turn.
// Appearance timing remains external, but the target cannot accept another
// contact until that owner explicitly completes its four-stage transition.
class FrontSpiderRuleScheduler {
public:
    explicit FrontSpiderRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<FrontSpiderRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_transition(std::uint8_t slot) noexcept;

    [[nodiscard]] bool seed(FrontSpiderState state) noexcept;
    [[nodiscard]] const FrontSpiderState* spider(std::uint8_t side) const noexcept;
    [[nodiscard]] bool contact_queued(std::uint8_t slot) const noexcept;

private:
    ScoreState& score_;
    std::array<FrontSpiderState, 2U> spiders_{{
        {.side = 0U},
        {.side = 1U},
    }};
    std::array<bool, 2U> queued_contacts_{};
};

} // namespace devils_crush::native_runtime
