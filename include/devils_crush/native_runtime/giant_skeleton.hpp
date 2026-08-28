// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kGiantSkeletonSlot = 0x30U;

// 01:DAF8 compares slot zero (the ball) with this fixed target in whole
// screen pixels. Keeping these as named coordinates avoids exposing the
// classifier's transient arithmetic bytes to gameplay rules.
struct GiantSkeletonTargetPosition {
    std::int16_t horizontal_pixels = 0;
    std::int16_t vertical_pixels = 0;

    [[nodiscard]] bool operator==(const GiantSkeletonTargetPosition&) const
        = default;
};

struct GiantSkeletonState {
    // 02:5EE4 initializes the stored appearance to 3. Each later active
    // update replaces it with the direction-selected value from 2 through 6.
    std::uint8_t appearance = 3U;

    [[nodiscard]] bool operator==(const GiantSkeletonState&) const = default;
};

enum class GiantSkeletonUpdateDisposition : std::uint8_t {
    idle,
    repeatable_hit,
};

struct GiantSkeletonUpdateStep {
    GiantSkeletonState previous;
    GiantSkeletonState state;
    GiantSkeletonTargetPosition target;
    // This is the cartridge's direction-table result, retained as an asset
    // selector because the table's individual direction labels are not yet
    // named by the presentation owner.
    std::uint8_t direction_code = 0U;
    GiantSkeletonUpdateDisposition disposition =
        GiantSkeletonUpdateDisposition::idle;
    std::uint16_t hit_score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_impact_presentation = false;
    std::uint8_t requested_appearance = 0U;

    [[nodiscard]] bool operator==(const GiantSkeletonUpdateStep&) const = default;
};

// Runs one active update of fixed slot $30 from 02:5EEC. Its target position
// is the ball's whole-pixel position. A confirmed contact is repeatable: it
// clears the source collision latch, awards 40 stored units, emits $2E, and
// returns to this same live update on the following actor turn.
[[nodiscard]] GiantSkeletonUpdateStep advance_giant_skeleton(
    GiantSkeletonState state,
    GiantSkeletonTargetPosition target,
    bool confirmed_contact) noexcept;

struct GiantSkeletonRulesUpdate {
    GiantSkeletonUpdateStep giant_skeleton;
    ScoreAwardStep hit_score_award;

    [[nodiscard]] bool operator==(const GiantSkeletonRulesUpdate&) const = default;
};

[[nodiscard]] GiantSkeletonRulesUpdate resolve_giant_skeleton_update(
    GiantSkeletonState state,
    GiantSkeletonTargetPosition target,
    bool confirmed_contact,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
