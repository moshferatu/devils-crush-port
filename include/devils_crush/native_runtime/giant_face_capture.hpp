// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/giant_face.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// The four fixed Giant-Face records captured at a coherent steady boundary.
// Native code can continue either the complete dormant state or the settled
// lower-form loop after the helmet wake; transient wake, transformation, and
// collision-presentation paths remain outside this handoff.
struct GiantFaceCapture {
    std::array<GameStateView::ActorSlot, 4U> actors;
    GiantFaceState rule_state;
    GiantFaceControllerState controller_state;
    GiantFaceAnimatorState animator_state;

    [[nodiscard]] bool operator==(const GiantFaceCapture&) const = default;
};

// Extracts the fully dormant Giant Face: controller and animator are at their
// steady dispatchers, both collision surfaces are clear, and every decoded
// family latch is inactive. Active wake, transformation, mouth, and display
// states remain with their respective native/presentation owners.
[[nodiscard]] std::optional<GiantFaceCapture> capture_dormant_giant_face(
    const GameStateView& state) noexcept;

// Extracts a settled lower-form loop after the wake sequence completed. Slot
// $36 has persisted form one, two, or three and accepts transformations; slot
// $37 is at its repeating lower-form dispatcher with a one-through-eight
// countdown. The persisted form determines completed transformations.
// This deliberately excludes the intervening wake, transformation, mouth,
// and presentation handlers where their local continuation is not yet native.
[[nodiscard]] std::optional<GiantFaceCapture> capture_awake_lower_giant_face(
    const GameStateView& state) noexcept;

// Extracts a settled upper-form loop after the third or fourth completed
// transformation. Slot $36 persists form four or five, while slot $37 runs
// the upper display forms three through five. The open-mouth sixth form and
// every transition handler remain excluded.
[[nodiscard]] std::optional<GiantFaceCapture> capture_awake_upper_giant_face(
    const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
