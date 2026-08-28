// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// The two fixed Side-Star sources each create one five-child Small-Soldier
// group. This is source-owned state, distinct from an individual child actor.
enum class SmallSoldierGroupSide : std::uint8_t {
    lower_left = 0U,
    upper_right = 1U,
};

struct SmallSoldierGroupState {
    SmallSoldierGroupSide side = SmallSoldierGroupSide::lower_left;
    std::uint8_t pending_spawns = 5U;
    std::uint8_t live_children = 0U;

    [[nodiscard]] bool operator==(const SmallSoldierGroupState&) const = default;
};

enum class SmallSoldierGroupDisposition {
    child_spawned,
    child_completed,
    source_completed,
    invalid_state,
};

struct SmallSoldierGroupStep {
    SmallSoldierGroupState previous;
    SmallSoldierGroupState state;
    SmallSoldierGroupDisposition disposition =
        SmallSoldierGroupDisposition::invalid_state;
    // The source awards this only once all five spawned children have been
    // completed. Its appearance advance and regeneration remain source rules.
    std::uint8_t source_completion_score_units = 0;

    [[nodiscard]] bool operator==(const SmallSoldierGroupStep&) const = default;
};

// A Side-Star retains its appearance across five-child generations. The
// animation that makes a regenerated group visible remains separately
// scheduled, so this state records that handoff rather than inventing a delay.
struct SmallSoldierSourceState {
    SmallSoldierGroupState group;
    std::uint8_t appearance = 0;
    // The source-specific delayed-spawn cadence. The allocator remains a
    // separate owner, but a captured generation retains this countdown.
    std::uint8_t ticks_until_next_spawn = 0;
    bool regeneration_pending = false;

    [[nodiscard]] bool operator==(const SmallSoldierSourceState&) const = default;
};

enum class SmallSoldierSourceDisposition {
    waiting,
    spawn_requested,
    child_spawned,
    child_completed,
    generation_completed,
    regenerated,
    invalid_state,
};

struct SmallSoldierSourceStep {
    SmallSoldierSourceState previous;
    SmallSoldierSourceState state;
    SmallSoldierSourceDisposition disposition =
        SmallSoldierSourceDisposition::invalid_state;
    std::optional<SmallSoldierGroupStep> group_step;

    [[nodiscard]] bool operator==(const SmallSoldierSourceStep&) const = default;
};

// Rule state for one active Small-Soldier. The collision table and reaction
// remain actor infrastructure; this model owns only the confirmed-hit and
// completion rule recovered for the Small-Soldier family.
struct SmallSoldierState {
    std::uint8_t hits_remaining = 2U;
    std::uint8_t subtype = 0;
    SmallSoldierGroupSide group_side = SmallSoldierGroupSide::lower_left;
    bool contact_pending = false;
    bool completion_animation_pending = false;

    [[nodiscard]] bool operator==(const SmallSoldierState&) const = default;
};

enum class SmallSoldierContactDisposition {
    no_contact,
    invalid_state,
    survived_hit,
    completed,
    awaiting_completion_animation,
};

struct SmallSoldierContactStep {
    SmallSoldierState previous;
    SmallSoldierState state;
    SmallSoldierContactDisposition disposition =
        SmallSoldierContactDisposition::no_contact;
    // Preserve the observed order: a hit award occurs before a terminal
    // completion award when both are present.
    std::uint8_t hit_score_units = 0;
    std::uint8_t completion_score_units = 0;
    bool decrement_group = false;
    bool release_actor = false;

    [[nodiscard]] bool operator==(const SmallSoldierContactStep&) const = default;
};

// Consumes one confirmed ball contact. Every accepted hit awards 30 stored
// score units. The final ordinary hit awards 50 additional units, decrements
// its source group, and releases the actor. Subtype `$0F` instead waits for
// its separate completion animation before that final transition.
[[nodiscard]] SmallSoldierContactStep consume_small_soldier_contact(
    SmallSoldierState state) noexcept;

// Registers the allocation of one child from a source's five-child budget.
// The allocator and actor-slot claim remain outside this rule model.
[[nodiscard]] SmallSoldierGroupStep spawn_small_soldier_child(
    SmallSoldierGroupState state) noexcept;

// Applies one terminal child completion to its source group. Once no live
// children and no pending spawns remain, the source-completion score event is
// emitted exactly once; the source controller owns follow-up regeneration.
[[nodiscard]] SmallSoldierGroupStep complete_small_soldier_group_child(
    SmallSoldierGroupState state) noexcept;

// Allocates one child through a Side-Star source. A source whose completed
// generation is waiting for its presentation transition cannot spawn again.
[[nodiscard]] SmallSoldierSourceStep spawn_small_soldier_source_child(
    SmallSoldierSourceState state) noexcept;

// Advances one Side-Star source countdown. A due source restores its proven
// 48-tick delay before asking the separate dynamic allocator for a child.
[[nodiscard]] SmallSoldierSourceStep advance_small_soldier_source_clock(
    SmallSoldierSourceState state) noexcept;

// Completes one child through its Side-Star source. On its fifth completion,
// the source emits the group award, advances appearance modulo four, and
// records a regeneration handoff for the source animation/controller.
[[nodiscard]] SmallSoldierSourceStep complete_small_soldier_source_child(
    SmallSoldierSourceState state) noexcept;

// Begins the next five-child source generation after its source-specific
// presentation/controller work has completed.
[[nodiscard]] SmallSoldierSourceStep regenerate_small_soldier_source(
    SmallSoldierSourceState state) noexcept;

// Completes the deferred subtype `$0F` path after its animation state changes.
// Calling it before a deferred hit leaves state unchanged.
[[nodiscard]] SmallSoldierContactStep complete_small_soldier_animation(
    SmallSoldierState state) noexcept;

} // namespace devils_crush::native_runtime
