// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/giant_face_helmet.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// A coherent pair of collision-ready Giant-Face helmet openings. The native
// scheduler can continue their independent idle cadence, while a collision or
// the opening/ball-return presentation remains with its current owner.
struct GiantFaceHelmetCapture {
    std::array<GameStateView::ActorSlot, 2U> actors;
    GiantFaceHelmetState rule_state;

    [[nodiscard]] bool operator==(const GiantFaceHelmetCapture&) const = default;
};

// Extracts both mirrored helmet channels only when each fixed record is at
// the proven 02:4C91 idle dispatcher. A collision-marked or active opening is
// deliberately not synthesized into a native completion state.
[[nodiscard]] std::optional<GiantFaceHelmetCapture> capture_giant_face_helmet(
    const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
