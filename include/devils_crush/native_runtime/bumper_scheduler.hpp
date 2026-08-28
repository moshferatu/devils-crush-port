// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bumper.hpp"
#include "devils_crush/native_runtime/contact_dispatch.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// Carries a confirmed fixed-Bumper contact to its later slot turn. The
// scheduler owns only the two known fixed targets; allocation and child
// behavior remain outside this narrow rule boundary.
class BumperRuleScheduler {
public:
    explicit BumperRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<BumperRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;

    [[nodiscard]] bool seed(BumperState state) noexcept;
    [[nodiscard]] const BumperState* bumper(std::uint8_t side) const noexcept;
    [[nodiscard]] bool contact_queued(std::uint8_t slot) const noexcept;

private:
    ScoreState& score_;
    std::array<BumperState, 2U> bumpers_{{
        {.side = 0U},
        {.side = 1U},
    }};
    std::array<bool, 2U> queued_contacts_{};
};

} // namespace devils_crush::native_runtime
