// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"
#include "devils_crush/native_runtime/score_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// A typed transfer of the active player's score out of an emulator snapshot.
// Native score updates own the resulting state; this captures no player or
// display policy beyond the ROM's selected accumulator and dirty bit.
struct ActiveScoreCapture {
    GameStateView::ActiveScore source;
    ScoreState state;

    [[nodiscard]] bool operator==(const ActiveScoreCapture&) const = default;
};

[[nodiscard]] std::optional<ActiveScoreCapture> capture_active_score(
    const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
