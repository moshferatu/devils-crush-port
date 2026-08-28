// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/giant_face.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// Preserves the two fixed Giant-Face collision turns. Form animation remains
// a separate owner, but its mouth-open completion enters here by name.
class GiantFaceRuleScheduler {
public:
    explicit GiantFaceRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<GiantFaceRulesUpdate> advance_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceControllerStep> advance_controller_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] std::optional<GiantFaceAnimatorStep> advance_animator_actor(
        std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_impact_presentation(std::uint8_t slot) noexcept;
    [[nodiscard]] bool complete_mouth_opening() noexcept;
    [[nodiscard]] bool request_wake() noexcept;
    [[nodiscard]] bool request_transformation() noexcept;
    [[nodiscard]] bool begin_animator(GiantFaceAnimatorMode mode) noexcept;
    void seed(GiantFaceState state) noexcept;
    void seed(
        GiantFaceState state,
        GiantFaceControllerState controller,
        GiantFaceAnimatorState animator) noexcept;
    [[nodiscard]] const GiantFaceState& giant_face() const noexcept;
    [[nodiscard]] const GiantFaceControllerState& controller() const noexcept;
    [[nodiscard]] const GiantFaceAnimatorState& animator() const noexcept;

private:
    ScoreState& score_;
    GiantFaceState giant_face_{};
    GiantFaceControllerState controller_{};
    GiantFaceAnimatorState animator_{};
    bool crown_contact_queued_ = false;
    bool mouth_contact_queued_ = false;
};

} // namespace devils_crush::native_runtime
