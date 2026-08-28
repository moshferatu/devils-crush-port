// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/front_spider.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A collision-ready fixed Front-Spider capture. The steady-handler check is
// essential: `$3C80,X` is family-local state and is not a generic side field.
struct FrontSpiderCapture {
    GameStateView::ActorSlot actor;
    FrontSpiderState rule_state;

    [[nodiscard]] bool operator==(const FrontSpiderCapture&) const = default;
};

// Extracts either lower-table Front Spider only while its proven 03:6490
// steady dispatcher owns collision readiness. Appearance-transition handlers
// remain outside this narrow capture slice.
[[nodiscard]] std::optional<FrontSpiderCapture> capture_front_spider_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
