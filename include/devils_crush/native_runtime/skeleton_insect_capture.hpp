// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/skeleton_insect.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A source-waiting Skeleton Insect captured at its fixed steady dispatcher.
// Transition handlers deliberately remain outside this capture boundary:
// their presentation sequence has not yet been ported.
struct SkeletonInsectCapture {
    GameStateView::ActorSlot actor;
    SkeletonInsectState rule_state;

    [[nodiscard]] bool operator==(const SkeletonInsectCapture&) const = default;
};

[[nodiscard]] std::optional<SkeletonInsectCapture>
capture_skeleton_insect_actor(const GameStateView& state, std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
