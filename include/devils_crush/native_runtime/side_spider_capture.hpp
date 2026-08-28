// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/side_spider.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A collision-ready fixed Side-Spider capture. `$3C80,X` is decoded as a side
// only after the fixed slot and steady handler establish this actor family.
struct SideSpiderCapture {
    GameStateView::ActorSlot actor;
    SideSpiderState rule_state;

    [[nodiscard]] bool operator==(const SideSpiderCapture&) const = default;
};

// Extracts either outer Side Spider only while its proven 03:651F steady
// dispatcher owns collision readiness. Disappearance-transition handlers are
// intentionally not treated as ready target snapshots.
[[nodiscard]] std::optional<SideSpiderCapture> capture_side_spider_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
