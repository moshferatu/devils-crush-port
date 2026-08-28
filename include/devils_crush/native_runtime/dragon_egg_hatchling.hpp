// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/dragon_egg.hpp"
#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// The source copies the destroyed fixed egg's whole-pixel position into its
// dynamically allocated type-$82 hatchling.  Motion and drawing remain
// separate owners; this records the recovered allocation boundary.
struct DragonEggHatchlingPosition {
    std::int16_t horizontal_pixels = 0;
    std::int16_t vertical_pixels = 0;

    [[nodiscard]] bool operator==(const DragonEggHatchlingPosition&) const = default;
};

struct DragonEggHatchlingState {
    std::uint8_t source_egg_slot = 0U;
    std::uint8_t dynamic_slot = 0U;
    DragonEggHatchlingPosition position;
    // The captured child starts at $03 and its per-turn animation chronology
    // is $03/$02/$01/$00/$FF.  This is deliberately not treated as a lifetime.
    std::uint8_t animation_timer = 3U;

    [[nodiscard]] bool operator==(const DragonEggHatchlingState&) const = default;
};

enum class DragonEggHatchlingAllocationDisposition : std::uint8_t {
    allocated,
    no_pending_hatchling,
    slot_already_in_use,
};

struct DragonEggHatchlingAllocationStep {
    DragonEggClusterState previous_cluster;
    DragonEggClusterState cluster;
    std::uint8_t source_egg_slot = 0U;
    std::uint8_t dynamic_slot = 0U;
    DragonEggHatchlingAllocationDisposition disposition =
        DragonEggHatchlingAllocationDisposition::no_pending_hatchling;
    std::optional<DragonEggHatchlingState> hatchling;

    [[nodiscard]] bool operator==(const DragonEggHatchlingAllocationStep&) const = default;
};

// Completes a fixed egg's explicit child-allocation handoff after the generic
// actor allocator has chosen a dynamic slot.
[[nodiscard]] DragonEggHatchlingAllocationStep allocate_dragon_egg_hatchling(
    DragonEggClusterState cluster,
    std::uint8_t source_egg_slot,
    std::uint8_t dynamic_slot,
    const std::array<std::optional<DragonEggHatchlingState>, kDragonEggCount>&
        existing_hatchlings) noexcept;

enum class DragonEggHatchlingAdvanceDisposition : std::uint8_t {
    animation_advanced,
    released_on_contact,
};

struct DragonEggHatchlingAdvanceStep {
    DragonEggHatchlingState previous;
    DragonEggHatchlingState state;
    DragonEggHatchlingAdvanceDisposition disposition =
        DragonEggHatchlingAdvanceDisposition::animation_advanced;
    std::uint16_t score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool releases_child = false;

    [[nodiscard]] bool operator==(const DragonEggHatchlingAdvanceStep&) const = default;
};

// A child update advances the observed wrapping animation timer. A confirmed
// contact is a separate terminal event: it awards 200 stored units, emits
// `$34`, and releases only the dynamic hatchling.
[[nodiscard]] DragonEggHatchlingAdvanceStep advance_dragon_egg_hatchling(
    DragonEggHatchlingState state,
    bool contact_confirmed) noexcept;

struct DragonEggHatchlingRulesUpdate {
    DragonEggHatchlingAdvanceStep hatchling;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const DragonEggHatchlingRulesUpdate&) const = default;
};

[[nodiscard]] DragonEggHatchlingRulesUpdate resolve_dragon_egg_hatchling(
    DragonEggHatchlingState state,
    bool contact_confirmed,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
