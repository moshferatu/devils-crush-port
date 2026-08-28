// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kTowerSlot = 0x2DU;

// This owns the Tower's contact progression through its handoff to the shared
// bonus-stage scene transition. The shared scene dispatcher remains a later
// native-runtime owner.
struct TowerState {
    bool collision_ready = true;
    // `$2D3F`: Tower-local hit progress. The first qualifying hit advances
    // it from zero before the later armed-pass owner takes over.
    std::uint8_t hit_progress = 0;
    // `$2D4D`: the five-pass Tower/Guard gate that starts decreasing only
    // after the center-insect source has been armed.
    std::uint8_t remaining_armed_passes = 5;
    // The recovered 03:611C/03:611F visual handoff has three countdown
    // updates followed by one steady-dispatch install after the source hit.
    std::uint8_t presentation_updates_remaining = 0;
    // The zero-gate transition installs 03:6136, where the next collision
    // selects bonus stage 5.
    bool bonus_stage_selection_pending = false;
    // A qualifying 03:6136 collision has requested the shared scene
    // transition. The Tower stays unavailable until that owner changes table
    // state or supplies a fresh capture seed.
    bool bonus_stage_transition_requested = false;

    [[nodiscard]] bool operator==(const TowerState&) const = default;
};

enum class TowerContactDisposition : std::uint8_t {
    first_source_hit,
    transition_pending,
    armed_pass_decremented,
    bonus_stage_selection_armed,
    bonus_stage_selection_pending,
};

struct TowerContactStep {
    TowerState previous;
    TowerState state;
    TowerContactDisposition disposition = TowerContactDisposition::transition_pending;
    std::uint16_t score_award_units = 0;
    std::uint8_t audio_command = 0;
    bool sets_center_insect_source = false;

    [[nodiscard]] bool operator==(const TowerContactStep&) const = default;
};

[[nodiscard]] TowerContactStep consume_tower_contact(
    TowerState state,
    bool center_insect_source_already_set) noexcept;

enum class TowerPresentationDisposition : std::uint8_t {
    not_pending,
    countdown,
    steady_dispatch_installed,
};

struct TowerPresentationStep {
    TowerState previous;
    TowerState state;
    TowerPresentationDisposition disposition = TowerPresentationDisposition::not_pending;

    [[nodiscard]] bool operator==(const TowerPresentationStep&) const = default;
};

[[nodiscard]] TowerPresentationStep advance_tower_presentation(
    TowerState state) noexcept;

enum class TowerBonusStageSelectionDisposition : std::uint8_t {
    not_pending,
    waiting_for_collision,
    bonus_stage_selected,
    transition_pending,
};

// The Tower-owned prefix of 03:6136. A nonnegative collision latch is its
// scheduled no-hit return. A negative geometry-delivered latch requests the
// shared transition dispatcher to enter bonus stage 5; the dispatcher itself
// deliberately remains outside this table-rule owner.
struct TowerBonusStageSelectionStep {
    TowerState previous;
    TowerState state;
    TowerBonusStageSelectionDisposition disposition =
        TowerBonusStageSelectionDisposition::not_pending;
    std::uint8_t bonus_stage = 0;
    std::uint8_t audio_command = 0;
    bool sets_tower_stage_flag = false;
    bool requests_session_transition = false;
    bool starts_next_tower_insect_cycle = false;

    [[nodiscard]] bool operator==(const TowerBonusStageSelectionStep&) const = default;
};

[[nodiscard]] TowerBonusStageSelectionStep consume_tower_bonus_stage_selection(
    TowerState state,
    bool negative_collision_latched) noexcept;

struct TowerRulesUpdate {
    TowerContactStep tower;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const TowerRulesUpdate&) const = default;
};

[[nodiscard]] TowerRulesUpdate resolve_tower_contact(
    TowerState state,
    bool center_insect_source_already_set,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
