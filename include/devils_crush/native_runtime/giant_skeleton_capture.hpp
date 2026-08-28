// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/giant_skeleton.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A live Giant Skeleton captured at its fixed 02:5EEC dispatcher. The
// direction-selected appearance is family-local state, so capture only names
// it after slot, type, handler, and a clear collision latch identify the
// stable fixed target.
struct GiantSkeletonCapture {
    GameStateView::ActorSlot actor;
    GiantSkeletonState rule_state;

    [[nodiscard]] bool operator==(const GiantSkeletonCapture&) const = default;
};

// Extracts fixed lower-right Giant Skeleton only while its ordinary live
// dispatcher owns the record. A latched hit is left to the collision owner so
// startup cannot drop the source's repeatable 40-unit award.
[[nodiscard]] std::optional<GiantSkeletonCapture> capture_giant_skeleton_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
