// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A collision-ready Small-Soldier capture. The handler check in the extractor
// is intentional: `$3C00`, `$3D00`, and `$3480` are reused by other actor
// families, so their Small-Soldier meanings never enter generic actor state.
struct SmallSoldierActorCapture {
    GameStateView::ActorSlot actor;
    SmallSoldierState rule_state;

    [[nodiscard]] bool operator==(const SmallSoldierActorCapture&) const = default;
};

// A stable Side-Star source snapshot.  The source is admitted only while it
// owns an in-progress generation; the all-children-complete presentation
// transition remains outside this boundary until its controller is native.
struct SmallSoldierSourceCapture {
    GameStateView::ActorSlot actor;
    SmallSoldierSourceState source_state;

    [[nodiscard]] bool operator==(const SmallSoldierSourceCapture&) const = default;
};

[[nodiscard]] std::optional<SmallSoldierSourceCapture>
capture_small_soldier_source(
    const GameStateView& state,
    SmallSoldierGroupSide side) noexcept;

// Extracts a type-$82 Small Soldier only while it is at one of its proven
// collision handlers (`03:7F1A` or `03:7F57`). Startup and deferred-completion
// handlers are deliberately left to their respective native scheduler and
// presentation slices rather than guessed from a frame snapshot.
[[nodiscard]] std::optional<SmallSoldierActorCapture>
capture_small_soldier_collision_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

// Seeds an already claimed native Small-Soldier actor from the typed capture.
// This does not register the child with a Side-Star source: reconstructing a
// whole source generation requires its own capture boundary.
[[nodiscard]] bool seed_small_soldier_collision_actor(
    ActorState& actors,
    const SmallSoldierActorCapture& source) noexcept;

} // namespace devils_crush::native_runtime
