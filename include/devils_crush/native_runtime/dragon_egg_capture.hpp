// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/dragon_egg.hpp"
#include "devils_crush/native_runtime/dragon_egg_hatchling.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// The complete six-target Dragon's-Egg cluster captured while every fixed egg
// is still at `02:4345`'s collision-ready steady dispatch. Partially
// destroyed and reset-requested clusters need their unported hatchling/reset
// owners, so they intentionally do not cross this startup boundary.
struct DragonEggClusterCapture {
    std::array<GameStateView::ActorSlot, kDragonEggCount> actors;
    DragonEggClusterState rule_state;

    [[nodiscard]] bool operator==(const DragonEggClusterCapture&) const = default;
};

[[nodiscard]] std::optional<DragonEggClusterCapture>
capture_dragon_egg_cluster(const GameStateView& state) noexcept;

// Captures a stable mixed Dragon's-Egg cluster after one or more destroyed
// eggs have reached their `02:4387` disabled wait and every hatchling has
// released. It deliberately rejects the hit/spawn window, all-six completion,
// reset, and a count that does not agree with the fixed records.
[[nodiscard]] std::optional<DragonEggClusterCapture>
capture_partial_dragon_egg_cluster(const GameStateView& state) noexcept;

// Captures the narrow in-flight-child boundary which has one disabled fixed
// egg and its one live type-$82 hatchling. The source does not retain an
// explicit parent link after allocation, so this deliberately requires the
// sole disabled egg; wider mixed-child states remain outside this handoff.
struct DragonEggHatchlingCapture {
    DragonEggClusterCapture cluster;
    GameStateView::ActorSlot actor;
    DragonEggHatchlingState hatchling;

    [[nodiscard]] bool operator==(const DragonEggHatchlingCapture&) const = default;
};

[[nodiscard]] std::optional<DragonEggHatchlingCapture>
capture_dragon_egg_cluster_with_hatchling(const GameStateView& state) noexcept;

// Captures the short stable interval after the slot-$1F controller completed
// the all-eggs path, but before its separate reset owner raises `$2CF7.0`.
// The controller's award has already been committed, so native takeover
// starts in `completed` rather than replaying completion.
[[nodiscard]] std::optional<DragonEggClusterCapture>
capture_completed_dragon_egg_cluster(const GameStateView& state) noexcept;

// Captures the explicit group-reset phase after its separate presentation
// owner has raised `$2CF7.0`. Fixed eggs resume at their normal actor turns,
// so this permits a mix of disabled and already-reinitialized records while
// retaining the native per-slot reset order.
[[nodiscard]] std::optional<DragonEggClusterCapture>
capture_dragon_egg_reset_cluster(const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
