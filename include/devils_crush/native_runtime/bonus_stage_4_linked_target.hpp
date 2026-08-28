// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// Each of BS-4's three eight-hit objectives has one paired source. The
// sources allocate their linked type-$C2 targets through the shared dynamic
// allocator, so these are identities rather than fixed scheduler slots.
inline constexpr std::array<std::uint8_t, 3U> kBonusStage4SourceSlots{
    0x35U, 0x53U, 0x57U};

struct BonusStage4LinkedTargetPosition {
    std::uint16_t horizontal_pixels = 0U;
    std::uint16_t vertical_pixels = 0U;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetPosition&) const = default;
};

struct BonusStage4LinkedTargetState {
    std::uint8_t source_slot = 0U;
    std::uint8_t objective_index = 0U;
    std::uint8_t dynamic_slot = 0U;
    BonusStage4LinkedTargetPosition position;
    // 05:6B88/6CB1 both decrement this counter and reload 32 on expiry
    // before leaving vector selection to the shared RNG motion helper.
    std::uint8_t ticks_until_motion_refresh = 1U;
    bool hit_presentation_pending = false;
    std::uint8_t orbiting_children_pending = 0U;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetState&) const = default;
};

enum class BonusStage4LinkedTargetAllocationDisposition : std::uint8_t {
    allocated,
    invalid_source,
};

struct BonusStage4LinkedTargetAllocationStep {
    std::uint8_t source_slot = 0U;
    std::uint8_t objective_index = 0U;
    std::uint8_t dynamic_slot = 0U;
    BonusStage4LinkedTargetAllocationDisposition disposition =
        BonusStage4LinkedTargetAllocationDisposition::invalid_source;
    std::optional<BonusStage4LinkedTargetState> target;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetAllocationStep&) const
        = default;
};

// Mirrors 05:6B16's allocation record after the generic allocator selects
// its destination. The target copies the source objective index and starts
// sixteen pixels down and right of the source, preserving 16-bit wrap.
[[nodiscard]] BonusStage4LinkedTargetAllocationStep
allocate_bonus_stage_4_linked_target(
    std::uint8_t source_slot,
    std::uint8_t objective_index,
    std::uint8_t dynamic_slot,
    BonusStage4LinkedTargetPosition source_position) noexcept;

enum class BonusStage4LinkedTargetMotionDisposition : std::uint8_t {
    waiting,
    random_motion_requested,
};

struct BonusStage4LinkedTargetMotionStep {
    BonusStage4LinkedTargetState previous;
    BonusStage4LinkedTargetState state;
    BonusStage4LinkedTargetMotionDisposition disposition =
        BonusStage4LinkedTargetMotionDisposition::waiting;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetMotionStep&) const
        = default;
};

// Mirrors the shared countdown prefix in 05:6B88 and 05:6CB1. The expiry
// requests a fresh random motion selection but deliberately does not model
// 01:C075's vector lookup or the shared RNG stream here.
[[nodiscard]] BonusStage4LinkedTargetMotionStep
advance_bonus_stage_4_linked_target_motion(
    BonusStage4LinkedTargetState target) noexcept;

enum class BonusStage4LinkedTargetContactDisposition : std::uint8_t {
    ignored,
    accepted,
};

struct BonusStage4LinkedTargetContactStep {
    BonusStage4LinkedTargetState previous;
    BonusStage4LinkedTargetState state;
    BonusStage4LinkedTargetContactDisposition disposition =
        BonusStage4LinkedTargetContactDisposition::ignored;
    std::uint16_t score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_orbiting_children = false;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetContactStep&) const
        = default;
};

struct BonusStage4LinkedTargetRulesUpdate {
    BonusStage4LinkedTargetContactStep target;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetRulesUpdate&) const
        = default;
};

// A confirmed target contact follows 05:6C06: award 100 stored units, emit
// `$3B`, set hit appearance four, and defer through state selector six. The
// selector installs 05:6C21; its coroutine resume at 05:6C24 owns the later
// four-child allocation requests. The target remains collision-latched until
// all four requests have been consumed, so contacts are ignored throughout
// that handoff.
[[nodiscard]] BonusStage4LinkedTargetRulesUpdate
resolve_bonus_stage_4_linked_target_contact(
    BonusStage4LinkedTargetState target,
    ScoreState score) noexcept;

enum class BonusStage4LinkedTargetHitPresentationDisposition : std::uint8_t {
    no_pending_presentation,
    child_allocation_requested,
};

struct BonusStage4LinkedTargetHitPresentationStep {
    BonusStage4LinkedTargetState previous;
    BonusStage4LinkedTargetState state;
    BonusStage4LinkedTargetHitPresentationDisposition disposition =
        BonusStage4LinkedTargetHitPresentationDisposition::no_pending_presentation;
    bool requests_orbiting_children = false;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetHitPresentationStep&) const
        = default;
};

// Completes the scheduler-owned 05:6C21 presentation boundary and enters
// 05:6C24. That continuation consumes exactly four allocator-selected child
// slots, holding the target collision-latched until the fourth allocation;
// a contact itself must not make those allocations visible early.
[[nodiscard]] BonusStage4LinkedTargetHitPresentationStep
complete_bonus_stage_4_linked_target_hit_presentation(
    BonusStage4LinkedTargetState target) noexcept;

struct BonusStage4OrbitingChildState {
    std::uint8_t target_slot = 0U;
    std::uint8_t dynamic_slot = 0U;
    // 05:6C24 seeds the four children at 24, 16, 8, and 0. The clear
    // 05:6D27 path advances this byte modulo 32 before using the ROM's orbit
    // vector table, which remains an asset-adapter concern.
    std::uint8_t orbit_phase = 0U;
    BonusStage4LinkedTargetPosition position;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildState&) const = default;
};

// Resolves 05:6D8E's signed 32-entry orbit asset relative to a live linked
// target. The phase is deliberately masked like 05:6D27; strict capture
// adapters still reject an unnormalized source phase before calling this.
[[nodiscard]] BonusStage4LinkedTargetPosition bonus_stage_4_orbit_position(
    BonusStage4LinkedTargetPosition target_position,
    std::uint8_t orbit_phase) noexcept;

enum class BonusStage4OrbitingChildAllocationDisposition : std::uint8_t {
    allocated,
    no_pending_child,
};

struct BonusStage4OrbitingChildAllocationStep {
    BonusStage4LinkedTargetState previous_target;
    BonusStage4LinkedTargetState target;
    BonusStage4OrbitingChildAllocationDisposition disposition =
        BonusStage4OrbitingChildAllocationDisposition::no_pending_child;
    std::optional<BonusStage4OrbitingChildState> child;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildAllocationStep&) const
        = default;
};

// Consumes one of the four requests made by the target's resumed 05:6C24 hit
// continuation after the generic allocator chooses a dynamic destination.
// The child motion and its orbit presentation remain their own owner.
[[nodiscard]] BonusStage4OrbitingChildAllocationStep
allocate_bonus_stage_4_orbiting_child(
    BonusStage4LinkedTargetState target,
    std::uint8_t dynamic_slot) noexcept;

enum class BonusStage4OrbitingChildOrbitDisposition : std::uint8_t {
    advanced,
    parent_inactive,
    collision_pending,
};

struct BonusStage4OrbitingChildOrbitStep {
    BonusStage4OrbitingChildState previous;
    BonusStage4OrbitingChildState state;
    BonusStage4OrbitingChildOrbitDisposition disposition =
        BonusStage4OrbitingChildOrbitDisposition::parent_inactive;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildOrbitStep&) const
        = default;
};

// Models only 05:6D27's clear child-orbit prefix. The complete release paths
// are represented by advance_bonus_stage_4_orbiting_child_frame below.
[[nodiscard]] BonusStage4OrbitingChildOrbitStep
advance_bonus_stage_4_orbiting_child_orbit(
    BonusStage4OrbitingChildState child,
    BonusStage4LinkedTargetPosition target_position,
    bool target_is_live,
    bool collision_pending) noexcept;

struct BonusStage4OrbitingChildContactStep {
    BonusStage4OrbitingChildState previous;
    BonusStage4OrbitingChildState state;
    bool releases_child = false;
    std::uint16_t score_award_units = 0U;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildContactStep&) const
        = default;
};

struct BonusStage4OrbitingChildRulesUpdate {
    BonusStage4OrbitingChildContactStep child;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildRulesUpdate&) const
        = default;
};

// 05:6D7F awards 100 stored units and then releases its orbiting child. It
// emits no recovered gameplay audio command.
[[nodiscard]] BonusStage4OrbitingChildRulesUpdate
resolve_bonus_stage_4_orbiting_child_contact(
    BonusStage4OrbitingChildState child,
    ScoreState score) noexcept;

// 05:6D27 sends both a child collision and a cleared parent liveness marker
// through 05:6D7F. That common continuation awards 100 stored units and
// releases this child; only an otherwise-clear turn advances the orbit.
enum class BonusStage4OrbitingChildFrameDisposition : std::uint8_t {
    advanced,
    released_after_parent_loss,
    released_after_collision,
};

struct BonusStage4OrbitingChildFrameStep {
    BonusStage4OrbitingChildState previous;
    BonusStage4OrbitingChildState state;
    BonusStage4OrbitingChildFrameDisposition disposition =
        BonusStage4OrbitingChildFrameDisposition::released_after_parent_loss;
    bool releases_child = false;
    std::uint16_t score_award_units = 0U;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildFrameStep&) const = default;
};

struct BonusStage4OrbitingChildFrameUpdate {
    BonusStage4OrbitingChildFrameStep child;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const BonusStage4OrbitingChildFrameUpdate&) const = default;
};

// Owns the complete recovered 05:6D27 decision: parent loss and a pending
// collision share the 05:6D7F score-and-release continuation, while the
// clear route advances the modulo-32 orbit by one entry. The caller retains
// allocator and presentation ownership of the released dynamic slot.
[[nodiscard]] BonusStage4OrbitingChildFrameUpdate
advance_bonus_stage_4_orbiting_child_frame(
    BonusStage4OrbitingChildState child,
    BonusStage4LinkedTargetPosition target_position,
    bool target_is_live,
    bool collision_pending,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
