// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/side_spider.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

class SideSpiderRuleScheduler {
public:
    explicit SideSpiderRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<SideSpiderRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_transition(std::uint8_t slot) noexcept;

    [[nodiscard]] bool seed(SideSpiderState state) noexcept;
    [[nodiscard]] const SideSpiderState* spider(std::uint8_t side) const noexcept;

private:
    ScoreState& score_;
    std::array<SideSpiderState, 2U> spiders_{{
        {.side = 0U},
        {.side = 1U},
    }};
    std::array<bool, 2U> queued_contacts_{};
};

} // namespace devils_crush::native_runtime
