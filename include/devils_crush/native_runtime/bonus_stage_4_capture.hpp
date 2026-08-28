// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bonus_stage_4.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// A stable BS-4 boundary requires all three fixed objectives to be at their
// steady update owner, with no queued collision or destruction transition.
// Linked targets and shared result animation remain independently captured
// later, so this adapter never infers them from the three controller flags.
struct BonusStage4Capture {
    std::array<GameStateView::ActorSlot, 3U> objectives;
    BonusStage4State state;

    [[nodiscard]] bool operator==(const BonusStage4Capture&) const = default;
};

[[nodiscard]] std::optional<BonusStage4Capture> capture_bonus_stage_4(
    const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
