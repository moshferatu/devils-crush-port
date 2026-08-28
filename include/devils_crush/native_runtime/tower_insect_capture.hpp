// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/tower_insect.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// A coherent active-generation capture. It admits either the ordinary source
// spawn countdown or the post-fourth-spawn wait for a positive live-child
// count. Startup, in-flight allocation, collision-response, and zero-count
// handoff states retain their original owners and are deliberately not
// seedable here.
struct TowerInsectGenerationCapture {
    GameStateView::ActorSlot source_actor;
    std::array<std::optional<GameStateView::ActorSlot>, 4U> insect_actors{};
    TowerInsectGenerationState generation;

    [[nodiscard]] bool operator==(const TowerInsectGenerationCapture&) const = default;
};

[[nodiscard]] std::optional<TowerInsectGenerationCapture>
capture_tower_insect_generation(const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
