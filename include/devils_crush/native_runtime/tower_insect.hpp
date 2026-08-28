// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kTowerInsectSourceSlot = 0x2CU;
inline constexpr std::array<std::uint8_t, 4U> kTowerInsectObservedSlots{
    0x64U, 0x67U, 0x6AU, 0x6DU};

// The fixed source at slot $2C creates exactly four Tower insects. The
// allocator chooses their dynamic slots; this state owns only the recovered
// source budget and live-child count.
struct TowerInsectSourceState {
    bool generation_active = false;
    std::uint8_t spawns_remaining = 4U;
    std::uint8_t live_children = 0U;
    // `$3900` of fixed source `$2C`, copied by 02:5F78 into every child.
    std::uint8_t child_appearance = 0x39U;
    // `$3D00` of fixed source `$2C`, copied by 02:5F78 into every child.
    // The shared effect temporarily changes only its low nibble to `$0F`.
    std::uint8_t child_collision_effect_association = 0x8CU;
    // 02:5F22 seeds one tick for the first allocation; 02:5F4A reloads 48
    // ticks after every allocation request.
    std::uint8_t ticks_until_next_spawn = 1U;
    bool child_spawn_pending = false;
    bool group_clear_pending = false;
    bool group_clear_awarded = false;

    [[nodiscard]] bool operator==(const TowerInsectSourceState&) const = default;
};

struct TowerInsectState {
    std::uint8_t hits_remaining = 2U;
    // `$3900`: copied from the fixed source when the child is allocated and
    // subsequently copied again by the shared collision-effect allocator.
    std::uint8_t appearance = 0x39U;
    std::uint8_t collision_effect_association = 0x8CU;

    [[nodiscard]] bool operator==(const TowerInsectState&) const = default;
};

// A self-consistent source generation suitable for a native scheduler seed.
// Its children retain their allocator-selected records; the source owns only
// their count and its remaining allocation budget.
struct TowerInsectGenerationState {
    TowerInsectSourceState source;
    std::array<std::optional<TowerInsectState>, 4U> insects{};

    [[nodiscard]] bool operator==(const TowerInsectGenerationState&) const = default;
};

enum class TowerInsectSpawnDisposition : std::uint8_t {
    child_spawned,
    source_exhausted,
    invalid_state,
};

struct TowerInsectSpawnStep {
    TowerInsectSourceState previous;
    TowerInsectSourceState state;
    TowerInsectSpawnDisposition disposition = TowerInsectSpawnDisposition::invalid_state;

    [[nodiscard]] bool operator==(const TowerInsectSpawnStep&) const = default;
};

enum class TowerInsectContactDisposition : std::uint8_t {
    survived_hit,
    child_completed,
    group_clear_armed,
    invalid_state,
};

struct TowerInsectContactStep {
    TowerInsectState previous;
    TowerInsectState state;
    TowerInsectSourceState previous_source;
    TowerInsectSourceState source;
    TowerInsectContactDisposition disposition = TowerInsectContactDisposition::invalid_state;
    std::uint16_t hit_score_award_units = 0U;
    std::uint16_t completion_score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool releases_child = false;
    bool requests_collision_effect = false;

    [[nodiscard]] bool operator==(const TowerInsectContactStep&) const = default;
};

// Applies one source allocation after the allocator has selected a dynamic
// slot. The timing/coroutine that spaces allocations remains a source owner.
[[nodiscard]] TowerInsectSpawnStep spawn_tower_insect_child(
    TowerInsectSourceState state) noexcept;

// Applies one confirmed child hit. Every hit awards 10 units and emits $29;
// the second hit additionally awards 50, releases the child, decrements the
// source count, and arms the source's 200-unit group-clear handoff only once
// all four spawned children are gone. The source consumes that handoff on its
// later actor update, preserving slot order.
[[nodiscard]] TowerInsectContactStep consume_tower_insect_contact(
    TowerInsectState insect,
    TowerInsectSourceState source) noexcept;

struct TowerInsectRulesUpdate {
    TowerInsectContactStep insect;
    ScoreAwardStep hit_score_award;
    ScoreAwardStep completion_score_award;

    [[nodiscard]] bool operator==(const TowerInsectRulesUpdate&) const = default;
};

[[nodiscard]] TowerInsectRulesUpdate resolve_tower_insect_contact(
    TowerInsectState insect,
    TowerInsectSourceState source,
    ScoreState score) noexcept;

enum class TowerInsectSourceDisposition : std::uint8_t {
    waiting,
    generation_started,
    child_spawn_requested,
    group_clear_awarded,
    waiting_for_next_cycle,
    generation_restarted,
};

struct TowerInsectSourceStep {
    TowerInsectSourceState previous;
    TowerInsectSourceState state;
    TowerInsectSourceDisposition disposition = TowerInsectSourceDisposition::waiting;
    std::uint16_t score_award_units = 0U;
    bool requests_child_spawn = false;

    [[nodiscard]] bool operator==(const TowerInsectSourceStep&) const = default;
};

// Runs one fixed-source turn. It names the initial one-tick and later
// 48-tick allocation cadence, and its zero-live group-clear branch. The
// dynamic allocator consumes a requested child through spawn_tower_insect_child.
[[nodiscard]] TowerInsectSourceStep advance_tower_insect_source(
    TowerInsectSourceState state) noexcept;

// Enters 02:5F22's four-child setup only when the Tower-owned cycle latch is
// active. A default native gameplay state deliberately has no source turn
// until this fixed actor has been initialized or captured.
[[nodiscard]] TowerInsectSourceStep begin_tower_insect_generation(
    TowerInsectSourceState state,
    bool tower_insect_cycle_requested) noexcept;

// Re-enters the source setup only after its completed generation and the
// separate Tower/insect-cycle latch.  This names 02:5F5F's branch back through
// 02:5F22 without treating a group clear as an unconditional respawn.
[[nodiscard]] TowerInsectSourceStep restart_tower_insect_generation(
    TowerInsectSourceState state,
    bool tower_insect_cycle_requested) noexcept;

} // namespace devils_crush::native_runtime
