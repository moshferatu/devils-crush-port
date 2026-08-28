// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kDragonEggFirstSlot = 0x20U;
inline constexpr std::uint8_t kDragonEggLastSlot = 0x25U;
inline constexpr std::uint8_t kDragonEggControllerSlot = 0x1FU;
inline constexpr std::size_t kDragonEggCount = 6U;

[[nodiscard]] constexpr std::optional<std::size_t> dragon_egg_index_for_slot(
    const std::uint8_t slot) noexcept
{
    if (slot < kDragonEggFirstSlot || slot > kDragonEggLastSlot) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(slot - kDragonEggFirstSlot);
}

struct DragonEggState {
    bool active = true;
    // A hit asks the separate allocator/hatchling owner to create the child.
    // The allocation identity and flight behavior are intentionally not
    // approximated in this fixed-target rule.
    bool hatchling_spawn_pending = false;

    [[nodiscard]] bool operator==(const DragonEggState&) const = default;
};

struct DragonEggClusterState {
    std::array<DragonEggState, kDragonEggCount> eggs{};
    bool completion_pending = false;
    bool completed = false;
    // The controller's completion hands reset timing to a separate owner.
    // Once that owner supplies its signal, fixed egg slots reinitialize in
    // their normal scheduler order rather than all at once.
    bool reset_pending = false;

    [[nodiscard]] bool operator==(const DragonEggClusterState&) const = default;
};

enum class DragonEggHitDisposition : std::uint8_t {
    ignored,
    destroyed,
};

struct DragonEggHitStep {
    DragonEggClusterState previous;
    DragonEggClusterState state;
    std::uint8_t slot = 0U;
    DragonEggHitDisposition disposition = DragonEggHitDisposition::ignored;
    std::uint16_t score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_hatchling_spawn = false;

    [[nodiscard]] bool operator==(const DragonEggHitStep&) const = default;
};

// Resolves a confirmed 02:4345 collision after its owning fixed egg reaches
// actor order. The source disables the egg, awards 10 stored units, emits
// `$39`, and hands hatchling allocation to its separate owner.
[[nodiscard]] DragonEggHitStep consume_dragon_egg_contact(
    DragonEggClusterState state,
    std::uint8_t slot) noexcept;

enum class DragonEggCompletionDisposition : std::uint8_t {
    waiting_for_live_eggs,
    completion_requested,
    already_completed,
};

struct DragonEggCompletionStep {
    DragonEggClusterState previous;
    DragonEggClusterState state;
    DragonEggCompletionDisposition disposition =
        DragonEggCompletionDisposition::waiting_for_live_eggs;
    std::uint16_t score_award_units = 0U;
    bool requests_group_reset = false;

    [[nodiscard]] bool operator==(const DragonEggCompletionStep&) const = default;
};

// Runs one `02:42F6` source-controller turn. Completion remains distinct from
// the last egg hit because the fixed controller owns its own actor-order turn.
[[nodiscard]] DragonEggCompletionStep advance_dragon_egg_controller(
    DragonEggClusterState state) noexcept;

enum class DragonEggGroupResetDisposition : std::uint8_t {
    ignored,
    reset_armed,
};

struct DragonEggGroupResetStep {
    DragonEggClusterState previous;
    DragonEggClusterState state;
    DragonEggGroupResetDisposition disposition =
        DragonEggGroupResetDisposition::ignored;

    [[nodiscard]] bool operator==(const DragonEggGroupResetStep&) const = default;
};

// Accepts the group-reset signal only after the controller completed its
// distinct all-eggs branch. The caller owns the timer/presentation that emits
// that signal.
[[nodiscard]] DragonEggGroupResetStep begin_dragon_egg_group_reset(
    DragonEggClusterState state) noexcept;

enum class DragonEggFixedResetDisposition : std::uint8_t {
    ignored,
    egg_reinitialized,
    cluster_reinitialized,
};

struct DragonEggFixedResetStep {
    DragonEggClusterState previous;
    DragonEggClusterState state;
    std::uint8_t slot = 0U;
    DragonEggFixedResetDisposition disposition =
        DragonEggFixedResetDisposition::ignored;

    [[nodiscard]] bool operator==(const DragonEggFixedResetStep&) const = default;
};

// Reinitializes one fixed egg only after the external group-reset owner has
// armed the operation. The final fixed slot clears completion/reset state when
// all six targets are live again.
[[nodiscard]] DragonEggFixedResetStep advance_dragon_egg_fixed_reset(
    DragonEggClusterState state,
    std::uint8_t slot) noexcept;

struct DragonEggRulesUpdate {
    DragonEggHitStep egg;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const DragonEggRulesUpdate&) const = default;
};

[[nodiscard]] DragonEggRulesUpdate resolve_dragon_egg_contact(
    DragonEggClusterState state,
    std::uint8_t slot,
    ScoreState score) noexcept;

struct DragonEggCompletionRulesUpdate {
    DragonEggCompletionStep controller;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(
        const DragonEggCompletionRulesUpdate&) const = default;
};

[[nodiscard]] DragonEggCompletionRulesUpdate resolve_dragon_egg_controller(
    DragonEggClusterState state,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
