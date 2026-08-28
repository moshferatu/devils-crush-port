// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kDragonHeadSlot = 0x13U;

// The lower-left Dragon's Head begins with eight hits. A normal hit has a
// shared appearance/effect handoff before it can accept another collision;
// the eighth instead starts the distinct mouth-opening sequence.
struct DragonHeadState {
    std::uint8_t hits_remaining = 8U;
    bool collision_ready = true;
    bool mouth_opening_transition_pending = false;

    [[nodiscard]] bool operator==(const DragonHeadState&) const = default;
};

enum class DragonHeadContactDisposition : std::uint8_t {
    survived_hit,
    mouth_opening_requested,
    invalid_state,
};

struct DragonHeadContactStep {
    DragonHeadState previous;
    DragonHeadState state;
    DragonHeadContactDisposition disposition =
        DragonHeadContactDisposition::invalid_state;
    std::uint16_t hit_score_award_units = 0U;
    std::uint16_t terminal_score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_impact_effect = false;
    std::uint8_t requested_appearance = 0U;

    [[nodiscard]] bool operator==(const DragonHeadContactStep&) const = default;
};

// Resolves one confirmed collision from 02:5A4B. Each hit awards 50 stored
// units and emits $2E. The eighth adds the proven second 50-unit award and
// leaves the multi-stage mouth-opening animation to its presentation owner.
[[nodiscard]] DragonHeadContactStep consume_dragon_head_contact(
    DragonHeadState state) noexcept;

// Returns a nonterminal Dragon to its collision-ready steady appearance after
// the presentation owner has completed the shared hit effect.
[[nodiscard]] bool complete_dragon_head_hit_presentation(
    DragonHeadState& state) noexcept;

struct DragonHeadRulesUpdate {
    DragonHeadContactStep dragon;
    ScoreAwardStep hit_score_award;
    ScoreAwardStep terminal_score_award;

    [[nodiscard]] bool operator==(const DragonHeadRulesUpdate&) const = default;
};

[[nodiscard]] DragonHeadRulesUpdate resolve_dragon_head_contact(
    DragonHeadState state,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
