// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bumper.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A steady, collision-ready fixed Bumper capture. The extractor intentionally
// accepts only the proven 03:78C7 dispatcher, before terminal allocation or
// presentation has taken ownership of the actor.
struct BumperCapture {
    GameStateView::ActorSlot actor;
    BumperState rule_state;

    [[nodiscard]] bool operator==(const BumperCapture&) const = default;
};

// Decodes the paired lower-table Bumpers only while their known steady handler
// owns the nine-hit progression. The fixed slot, type, side, phase, and both
// appearance counters must agree, so overloaded RAM never seeds a Bumper rule.
[[nodiscard]] std::optional<BumperCapture> capture_bumper_actor(
    const GameStateView& state,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
