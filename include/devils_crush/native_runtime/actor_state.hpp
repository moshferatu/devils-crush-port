// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_motion.hpp"
#include "devils_crush/native_runtime/collision_effect.hpp"
#include "devils_crush/native_runtime/collision_reaction.hpp"
#include "devils_crush/native_runtime/collision_table.hpp"
#include "devils_crush/native_runtime/small_soldier.hpp"
#include "devils_crush/native_runtime/tower_insect.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace devils_crush::native_runtime {

// The first native behavior set. New behaviors are named here as their table
// semantics are recovered; ROM handler addresses never become dispatch keys.
enum class ActorBehavior {
    inactive,
    shared_motion,
    collision_effect,
    small_soldier,
    tower_insect,
    dragon_egg_hatchling,
};

struct ActorRecord {
    std::uint8_t slot = 0;
    std::uint32_t generation = 0U;
    ActorBehavior behavior = ActorBehavior::inactive;
    bool due_for_update = false;
    ActorMotionState motion;
    std::optional<CollisionReactionState> collision_reaction;
    std::optional<CollisionEffectState> collision_effect;
    std::optional<CollisionTableProgram> collision_table_program;
    std::optional<CollisionTableState> collision_table;
    std::optional<SmallSoldierState> small_soldier;
    bool small_soldier_group_registered = false;
    // Mirrors the source low-nibble $0F association without exposing a raw
    // work-RAM field to rule code.
    bool collision_effect_active = false;
    // The source's $3D00 value. The shared effect marks its low nibble $0F,
    // then restores the saved byte when its wait completes.
    std::uint8_t collision_effect_association = 0U;
    // The native presentation owner supplies/captures this value; 03:7C1E
    // copies it to the effect child on allocation.
    std::uint8_t appearance = 0U;

    [[nodiscard]] bool active() const noexcept
    {
        return behavior != ActorBehavior::inactive;
    }

    [[nodiscard]] bool operator==(const ActorRecord&) const = default;
};

struct ActorMotionUpdate {
    std::uint8_t slot = 0;
    ActorMotionStep step;

    [[nodiscard]] bool operator==(const ActorMotionUpdate&) const = default;
};

struct ActorCollisionReactionUpdate {
    std::uint8_t slot = 0;
    CollisionReactionStep step;

    [[nodiscard]] bool operator==(const ActorCollisionReactionUpdate&) const
        = default;
};

enum class CollisionEffectAllocationDisposition : std::uint8_t {
    allocated,
    source_already_has_effect,
    allocator_rejected,
    invalid_source,
};

struct ActorCollisionEffectAllocation {
    std::uint8_t source_slot = 0U;
    std::uint8_t effect_slot = 0U;
    CollisionEffectAllocationDisposition disposition =
        CollisionEffectAllocationDisposition::invalid_source;
    std::optional<CollisionEffectState> effect;

    [[nodiscard]] bool operator==(const ActorCollisionEffectAllocation&) const = default;
};

struct ActorCollisionEffectUpdate {
    std::uint8_t slot = 0U;
    CollisionEffectAdvanceStep step;

    [[nodiscard]] bool operator==(const ActorCollisionEffectUpdate&) const = default;
};

struct ActorCollisionTableUpdate {
    std::uint8_t slot = 0;
    CollisionTableAdvance step;

    [[nodiscard]] bool operator==(const ActorCollisionTableUpdate&) const
        = default;
};

struct ActorCollisionTableResolutionUpdate {
    std::uint8_t slot = 0;
    CollisionTableRecordResolution resolution;
    std::optional<CollisionTableAdvance> advance;

    [[nodiscard]] bool operator==(
        const ActorCollisionTableResolutionUpdate&) const = default;
};

// The actor-owned result of one confirmed Small-Soldier contact or a deferred
// completion-animation transition. Score effects remain named outputs, while
// the optional source transition reports the Side-Star state already committed
// by ActorState.
struct ActorSmallSoldierUpdate {
    std::uint8_t slot = 0;
    SmallSoldierContactStep step;
    std::optional<SmallSoldierGroupStep> group_step;
    std::optional<SmallSoldierSourceStep> source_step;

    [[nodiscard]] bool operator==(const ActorSmallSoldierUpdate&) const = default;
};

struct ActorSmallSoldierSpawnUpdate {
    std::uint8_t slot = 0;
    SmallSoldierGroupStep group_step;
    SmallSoldierSourceStep source_step;

    [[nodiscard]] bool operator==(const ActorSmallSoldierSpawnUpdate&) const
        = default;
};

// Owns native actor records. This is deliberately independent of a work-RAM
// image: adapters may seed it from emulation, but normal native updates mutate
// these records directly.
class ActorState {
public:
    static constexpr std::uint8_t kSlotCount = 0x80U;
    static constexpr std::uint8_t kFirstDynamicSlot = 0x04U;
    static constexpr std::uint8_t kPastLastDynamicSlot = 0x7CU;

    ActorState() noexcept;

    [[nodiscard]] const ActorRecord* actor(std::uint8_t slot) const noexcept;
    [[nodiscard]] ActorRecord* actor(std::uint8_t slot) noexcept;

    // A scheduler supplies a free slot decision; state owns the claim and
    // rejects fixed, occupied, and out-of-range dynamic records.
    [[nodiscard]] bool claim_dynamic_slot(
        std::uint8_t slot,
        ActorBehavior behavior) noexcept;
    void release(std::uint8_t slot) noexcept;

    // Stores the source appearance byte which the shared collision-effect
    // initializer copies to its new child. It deliberately does not model
    // drawing or sprite selection.
    [[nodiscard]] bool set_actor_appearance(
        std::uint8_t slot,
        std::uint8_t appearance) noexcept;
    [[nodiscard]] bool set_collision_effect_association(
        std::uint8_t slot,
        std::uint8_t association) noexcept;

    // Consumes an allocator-selected dynamic slot for the shared type-$8F
    // effect. A live source may have only one associated effect at a time;
    // allocator failure leaves both source and destination untouched.
    [[nodiscard]] ActorCollisionEffectAllocation claim_collision_effect(
        std::uint8_t source_slot,
        std::uint8_t effect_slot) noexcept;

    // Seeds a captured effect after its source actor has already been claimed
    // and supplied its captured appearance. The source generation is always
    // assigned locally; emulator generations are not a cross-runtime datum.
    [[nodiscard]] bool seed_collision_effect(
        std::uint8_t effect_slot,
        CollisionEffectState state) noexcept;

    // Advances one active collision-effect child. Expiration releases only
    // that child and clears its source association when the source generation
    // still matches, avoiding stale cleanup after dynamic-slot reuse.
    [[nodiscard]] std::optional<ActorCollisionEffectUpdate>
    advance_collision_effect(std::uint8_t slot) noexcept;

    // Associates an active actor with one ROM-derived collision-table program
    // and initializes its mutable table state. Actor families choose this
    // program during setup; per-frame updates only consume it.
    [[nodiscard]] bool configure_collision_table(
        std::uint8_t slot,
        CollisionTableProgram program,
        CollisionTableState state = {}) noexcept;

    // Installs the independently scheduled reaction state for an active
    // collision-table actor. Table records can later replace its event while
    // preserving phase and countdown ownership here.
    [[nodiscard]] bool configure_collision_reaction(
        std::uint8_t slot,
        CollisionReactionState state) noexcept;

    // Initializes the native rule state for a claimed Small-Soldier actor.
    // Other behavior families cannot accidentally acquire this rule state.
    [[nodiscard]] bool configure_small_soldier(
        std::uint8_t slot,
        SmallSoldierState state = {}) noexcept;

    // Starts one source's next five-child lifecycle. Existing child actors
    // are untouched; the source controller chooses when it is safe to reset.
    void reset_small_soldier_group(SmallSoldierGroupSide side) noexcept;
    [[nodiscard]] const SmallSoldierGroupState* small_soldier_group(
        SmallSoldierGroupSide side) const noexcept;
    [[nodiscard]] const SmallSoldierSourceState* small_soldier_source(
        SmallSoldierGroupSide side) const noexcept;

    // Completes the source-owned regeneration handoff after its presentation
    // or animation controller has run. It never resets a live group.
    [[nodiscard]] std::optional<SmallSoldierSourceStep>
    regenerate_small_soldier_source(SmallSoldierGroupSide side) noexcept;

    // Records a child allocation after the scheduler has claimed `slot` for
    // the Small-Soldier behavior. This is the path that connects terminal
    // child effects to their owning Side-Star group.
    [[nodiscard]] std::optional<ActorSmallSoldierSpawnUpdate>
    spawn_small_soldier(std::uint8_t slot, SmallSoldierState state = {}) noexcept;

    // Registers a captured collision-ready child with an already seeded
    // Side-Star generation. It preserves the child's captured rule state.
    [[nodiscard]] bool register_captured_small_soldier(
        std::uint8_t slot) noexcept;

    // Replaces one Side-Star generation after a typed capture has validated
    // its source-owned counters. Dynamic child registration is separate so a
    // failed bootstrap can retain atomic ownership at its caller.
    [[nodiscard]] bool seed_small_soldier_source(
        SmallSoldierSourceState state) noexcept;

    // Advances only a source restored by typed capture. This keeps a blank
    // gameplay state from manufacturing Side-Star generations before its
    // fixed-source initialization is native.
    [[nodiscard]] std::optional<SmallSoldierSourceStep>
    advance_captured_small_soldier_source(SmallSoldierGroupSide side) noexcept;

    // Applies one confirmed ball contact to a Small-Soldier actor. The method
    // marks the contact itself, so callers do not manipulate a transient
    // contact flag in actor-owned state. A terminal step releases the slot
    // after copying the step into the returned effect.
    [[nodiscard]] std::optional<ActorSmallSoldierUpdate>
    apply_small_soldier_contact(std::uint8_t slot) noexcept;

    // Applies the separately scheduled completion-animation transition for a
    // subtype `$0F` Small-Soldier. A terminal completion releases the slot.
    [[nodiscard]] std::optional<ActorSmallSoldierUpdate>
    complete_small_soldier_animation(std::uint8_t slot) noexcept;

    // Advances a target's configured collision-reaction cadence. The table
    // rule remains responsible for creating/configuring that reaction when a
    // confirmed contact is dispatched.
    [[nodiscard]] std::optional<ActorCollisionReactionUpdate>
    advance_collision_reaction(std::uint8_t slot) noexcept;

    // Applies a decoded collision-table record to an actor. A header-selected
    // event becomes the event consumed by that actor's existing reaction
    // cadence, while pass, cursor, orientation, and target motion stay with
    // the collision-table state.
    [[nodiscard]] std::optional<ActorCollisionTableUpdate>
    advance_collision_table(
        std::uint8_t slot,
        CollisionTableRecord record) noexcept;

    // Commits a ROM-decoded collision-table result. Terminal results retain
    // their resolved cursor without inventing a record step; an invalid
    // result leaves native state untouched.
    [[nodiscard]] std::optional<ActorCollisionTableResolutionUpdate>
    apply_collision_table_resolution(
        std::uint8_t slot,
        CollisionTableRecordResolution resolution) noexcept;

    // Advances one due shared-motion actor. Other recovered behaviors retain
    // their own phase owners because they may have contact-specific ordering.
    [[nodiscard]] std::optional<ActorMotionUpdate> advance_due_actor(
        std::uint8_t slot,
        PlayfieldBounds playfield = {});
    // Advances all due shared-motion actors in slot order.
    [[nodiscard]] std::vector<ActorMotionUpdate> advance_due_actors(
        PlayfieldBounds playfield = {});

private:
    std::array<ActorRecord, kSlotCount> actors_;
    std::array<SmallSoldierSourceState, 2U> small_soldier_sources_;
    std::array<bool, 2U> captured_small_soldier_sources_{};
};

} // namespace devils_crush::native_runtime
