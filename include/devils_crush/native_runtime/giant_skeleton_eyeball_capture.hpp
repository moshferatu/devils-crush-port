// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/giant_skeleton_eyeball.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A Giant Skeleton's Eyeball captured after its source setup has installed
// the 02:4DF7 opening-timer continuation. The capture names only the proven
// countdown state, leaving dormant setup and the expiry/final animation with
// their respective owners.
struct GiantSkeletonEyeballCapture {
    GameStateView::ActorSlot actor;
    GiantSkeletonEyeballState rule_state;

    [[nodiscard]] bool operator==(const GiantSkeletonEyeballCapture&) const = default;
};

// Extracts the fixed Eyeball only while its opening timer is still owned by
// the direct 02:4DF7 continuation. A one-tick timer is deliberately excluded:
// its source follows an expiry branch that the native model has not ported.
[[nodiscard]] std::optional<GiantSkeletonEyeballCapture>
capture_giant_skeleton_eyeball_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

// Captures direct `02:4F3D` final-mouth waits only between their source
// appearance and phase transitions. Branch-entry presentation and final reset
// remain separate owners; this boundary transfers the named countdown and
// current appearance without exposing raw RAM to gameplay.
[[nodiscard]] std::optional<GiantSkeletonEyeballCapture>
capture_giant_skeleton_eyeball_final_animation_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
