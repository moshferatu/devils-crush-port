// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/dragon_head.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// Carries a confirmed fixed-slot contact to the Dragon's actor-order turn.
// The shared hit presentation remains a separate owner, so the rule cannot
// turn its recovered transition into a guessed timer.
class DragonHeadRuleScheduler {
public:
    explicit DragonHeadRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<DragonHeadRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_hit_presentation(std::uint8_t slot) noexcept;
    void seed(DragonHeadState state) noexcept;
    [[nodiscard]] const DragonHeadState& dragon() const noexcept;

private:
    ScoreState& score_;
    DragonHeadState dragon_{};
    bool contact_queued_ = false;
};

} // namespace devils_crush::native_runtime
