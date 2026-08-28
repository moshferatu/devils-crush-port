// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_state.hpp"
#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// One native Small-Soldier rule outcome retains every ordinary score call.
// This is deliberately not a summed award: the original score owner checks
// its ceiling independently for the hit, completion, and source awards.
struct SmallSoldierRulesUpdate {
    ActorSmallSoldierUpdate actor;
    SmallSoldierScoreEvents score_events;
    std::array<ScoreAwardStep, 3U> score_awards{};
    std::uint8_t score_award_count = 0;
    // `03:7F57` submits this once for every accepted hit, before its
    // two-hit/completion branch. The later subtype-$0F completion animation
    // does not submit it again.
    std::uint8_t audio_command = 0;

    [[nodiscard]] bool operator==(const SmallSoldierRulesUpdate&) const = default;
};

// Resolves a confirmed contact through the actor, source, and score domains.
// The caller still owns contact detection and audio playback; the returned
// event carries the proven gameplay-side command for an accepted hit.
[[nodiscard]] std::optional<SmallSoldierRulesUpdate>
resolve_small_soldier_contact(
    ActorState& actors,
    ScoreState& score,
    std::uint8_t slot) noexcept;

// Resolves the subtype `$0F` completion-animation handoff through the same
// actor/source/score sequence.
[[nodiscard]] std::optional<SmallSoldierRulesUpdate>
resolve_small_soldier_completion_animation(
    ActorState& actors,
    ScoreState& score,
    std::uint8_t slot) noexcept;

} // namespace devils_crush::native_runtime
