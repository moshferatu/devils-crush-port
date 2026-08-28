// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/small_soldier.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// Scores are stored in units of 100 displayed points. The original ordinary
// score path rejects an award that reaches 9,999,999 stored units.
struct ScoreState {
    static constexpr std::uint32_t kExclusiveLimit = 9'999'999U;

    std::uint32_t stored_units = 0;
    bool display_dirty = false;

    [[nodiscard]] bool operator==(const ScoreState&) const = default;
};

enum class ScoreAwardDisposition {
    applied,
    reaches_limit,
};

struct ScoreAwardStep {
    ScoreState previous;
    ScoreState state;
    ScoreAwardDisposition disposition = ScoreAwardDisposition::applied;
    std::uint16_t awarded_units = 0;

    [[nodiscard]] bool operator==(const ScoreAwardStep&) const = default;
};

// Applies one ordinary stored-score award. A zero award is retained as a
// no-op successful event; callers normally omit it from their event sequence.
[[nodiscard]] ScoreAwardStep apply_score_award(
    ScoreState state,
    std::uint16_t awarded_units) noexcept;

// Small-Soldier awards are distinct score calls in this order: hit, child
// completion, then (only for the final fifth child) source completion.
struct SmallSoldierScoreEvents {
    std::array<std::uint16_t, 3U> awards{};
    std::uint8_t count = 0;

    [[nodiscard]] bool operator==(const SmallSoldierScoreEvents&) const = default;
};

[[nodiscard]] SmallSoldierScoreEvents small_soldier_score_events(
    const SmallSoldierContactStep& child_step,
    const std::optional<SmallSoldierGroupStep>& group_step) noexcept;

} // namespace devils_crush::native_runtime
