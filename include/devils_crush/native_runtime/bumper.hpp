// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kLeftBumperSlot = 0x14U;
inline constexpr std::uint8_t kRightBumperSlot = 0x15U;

[[nodiscard]] constexpr std::optional<std::uint8_t> bumper_side_for_slot(
    const std::uint8_t slot) noexcept
{
    if (slot == kLeftBumperSlot) {
        return std::uint8_t{0U};
    }
    if (slot == kRightBumperSlot) {
        return std::uint8_t{1U};
    }
    return std::nullopt;
}

// The paired lower-table Bumpers use the same nine-hit rule. Their fixed slot
// and side identify the presentation/layout record; this state owns only the
// gameplay progression after a confirmed contact.
struct BumperState {
    std::uint8_t side = 0;
    // Completed contacts, from zero before the first hit through nine after
    // destruction. The ROM's dispatcher increments this before selecting its
    // nine-entry phase table.
    std::uint8_t hit_phase = 0;
    std::uint8_t primary_appearance = 0;
    std::uint8_t secondary_damage = 0;
    bool active = true;

    [[nodiscard]] bool operator==(const BumperState&) const = default;
};

enum class BumperHitDisposition : std::uint8_t {
    primary_damage,
    secondary_damage,
    destroyed,
    inactive,
    invalid_state,
};

// The bumper controller emits named effects. Allocation and presentation own
// their later work: the rule only proves that terminal destruction requests
// six children and that ordinary hit paths emit command $20.
struct BumperHitStep {
    BumperState previous;
    BumperState state;
    BumperHitDisposition disposition = BumperHitDisposition::inactive;
    std::uint16_t score_award_units = 0;
    std::uint8_t audio_command = 0;
    bool refresh_primary_appearance = false;
    bool updates_secondary_damage = false;
    std::uint8_t requested_child_count = 0;

    [[nodiscard]] bool operator==(const BumperHitStep&) const = default;
};

// Applies one confirmed Bumper contact. Contacts one through six advance the
// primary appearance; contacts seven and eight advance secondary damage; the
// ninth contact awards the destruction score and requests the six-child
// release. A score owner applies the returned award separately.
[[nodiscard]] BumperHitStep consume_bumper_contact(BumperState state) noexcept;

struct BumperRulesUpdate {
    BumperHitStep bumper;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const BumperRulesUpdate&) const = default;
};

// Composes the Bumper rule with the native ordinary-score owner. Even a
// terminal score rejected at the ceiling remains a terminal Bumper transition.
[[nodiscard]] BumperRulesUpdate resolve_bumper_contact(
    BumperState state,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
