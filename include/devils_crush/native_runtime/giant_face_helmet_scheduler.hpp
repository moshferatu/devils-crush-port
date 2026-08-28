// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/giant_face_helmet.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

class GiantFaceHelmetRuleScheduler {
public:
    explicit GiantFaceHelmetRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<GiantFaceHelmetRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceHelmetCompletionStep>
    complete_activation_presentation(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_ball_return(std::uint8_t slot) noexcept;
    void seed(GiantFaceHelmetState state) noexcept;
    [[nodiscard]] const GiantFaceHelmetState* helmet() const noexcept;

private:
    ScoreState& score_;
    std::optional<GiantFaceHelmetState> helmet_;
    std::array<bool, 2U> contact_queued_{};
};

} // namespace devils_crush::native_runtime
