// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_state.hpp"

namespace devils_crush::native_runtime {

ActorState::ActorState() noexcept
{
    for (std::uint16_t slot = 0U; slot < kSlotCount; ++slot) {
        actors_[slot].slot = static_cast<std::uint8_t>(slot);
    }
    reset_small_soldier_group(SmallSoldierGroupSide::lower_left);
    reset_small_soldier_group(SmallSoldierGroupSide::upper_right);
}

const ActorRecord* ActorState::actor(const std::uint8_t slot) const noexcept
{
    return slot < kSlotCount ? &actors_[slot] : nullptr;
}

ActorRecord* ActorState::actor(const std::uint8_t slot) noexcept
{
    return slot < kSlotCount ? &actors_[slot] : nullptr;
}

bool ActorState::claim_dynamic_slot(
    const std::uint8_t slot,
    const ActorBehavior behavior) noexcept
{
    if (slot < kFirstDynamicSlot || slot >= kPastLastDynamicSlot
        || behavior == ActorBehavior::inactive || actors_[slot].active()) {
        return false;
    }

    actors_[slot].behavior = behavior;
    ++actors_[slot].generation;
    actors_[slot].due_for_update = false;
    actors_[slot].motion = {};
    return true;
}

bool ActorState::set_actor_appearance(
    const std::uint8_t slot,
    const std::uint8_t appearance) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || !record->active()) {
        return false;
    }
    record->appearance = appearance;
    return true;
}

bool ActorState::set_collision_effect_association(
    const std::uint8_t slot,
    const std::uint8_t association) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || !record->active()
        || record->collision_effect_active) {
        return false;
    }
    record->collision_effect_association = association;
    return true;
}

ActorCollisionEffectAllocation ActorState::claim_collision_effect(
    const std::uint8_t source_slot,
    const std::uint8_t effect_slot) noexcept
{
    auto* source = actor(source_slot);
    if (source == nullptr || !source->active() || source_slot == effect_slot) {
        return {
            .source_slot = source_slot,
            .effect_slot = effect_slot,
            .disposition = CollisionEffectAllocationDisposition::invalid_source,
        };
    }
    if (source->collision_effect_active
        || (source->collision_effect_association & 0x0FU) == 0x0FU) {
        return {
            .source_slot = source_slot,
            .effect_slot = effect_slot,
            .disposition = CollisionEffectAllocationDisposition::source_already_has_effect,
        };
    }
    if (!claim_dynamic_slot(effect_slot, ActorBehavior::collision_effect)) {
        return {
            .source_slot = source_slot,
            .effect_slot = effect_slot,
            .disposition = CollisionEffectAllocationDisposition::allocator_rejected,
        };
    }

    const CollisionEffectState effect{
        .source_slot = source_slot,
        .source_generation = source->generation,
        .source_association = source->collision_effect_association,
        .source_appearance = source->appearance,
    };
    auto* child = actor(effect_slot);
    child->collision_effect = effect;
    child->due_for_update = true;
    source->collision_effect_active = true;
    source->collision_effect_association = static_cast<std::uint8_t>(
        (source->collision_effect_association & 0xF0U) | 0x0FU);
    return {
        .source_slot = source_slot,
        .effect_slot = effect_slot,
        .disposition = CollisionEffectAllocationDisposition::allocated,
        .effect = effect,
    };
}

bool ActorState::seed_collision_effect(
    const std::uint8_t effect_slot,
    CollisionEffectState state) noexcept
{
    auto* source = actor(state.source_slot);
    if (source == nullptr || !source->active() || source->collision_effect_active
        || source->appearance != state.source_appearance
        || (source->collision_effect_association & 0x0FU) != 0x0FU
        || (source->collision_effect_association & 0xF0U)
            != (state.source_association & 0xF0U)
        || state.ticks_remaining == 0U
        || state.ticks_remaining > kCollisionEffectLifetime
        || !claim_dynamic_slot(effect_slot, ActorBehavior::collision_effect)) {
        return false;
    }

    state.source_generation = source->generation;
    auto* child = actor(effect_slot);
    child->collision_effect = state;
    child->due_for_update = true;
    source->collision_effect_active = true;
    return true;
}

std::optional<ActorCollisionEffectUpdate>
ActorState::advance_collision_effect(const std::uint8_t slot) noexcept
{
    auto* child = actor(slot);
    if (child == nullptr || child->behavior != ActorBehavior::collision_effect
        || !child->collision_effect) {
        return std::nullopt;
    }

    const auto step = devils_crush::native_runtime::advance_collision_effect(
        *child->collision_effect);
    if (step.disposition == CollisionEffectAdvanceDisposition::invalid_state) {
        return std::nullopt;
    }
    const ActorCollisionEffectUpdate update{.slot = slot, .step = step};
    if (step.disposition == CollisionEffectAdvanceDisposition::expired) {
        if (auto* source = actor(step.previous.source_slot);
            source != nullptr && source->active()
            && source->generation == step.previous.source_generation) {
            source->collision_effect_active = false;
            source->collision_effect_association = step.previous.source_association;
        }
        release(slot);
    } else {
        child->collision_effect = step.state;
    }
    return update;
}

void ActorState::release(const std::uint8_t slot) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr) {
        return;
    }
    *record = {
        .slot = slot,
        .generation = record->generation,
    };
}

bool ActorState::configure_collision_table(
    const std::uint8_t slot,
    const CollisionTableProgram program,
    const CollisionTableState state) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || !record->active()) {
        return false;
    }

    record->collision_table_program = program;
    record->collision_table = state;
    return true;
}

bool ActorState::configure_collision_reaction(
    const std::uint8_t slot,
    const CollisionReactionState state) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || !record->active()) {
        return false;
    }

    record->collision_reaction = state;
    return true;
}

bool ActorState::configure_small_soldier(
    const std::uint8_t slot,
    const SmallSoldierState state) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || record->behavior != ActorBehavior::small_soldier) {
        return false;
    }

    record->small_soldier = state;
    record->small_soldier_group_registered = false;
    return true;
}

void ActorState::reset_small_soldier_group(
    const SmallSoldierGroupSide side) noexcept
{
    auto& source = small_soldier_sources_[static_cast<std::size_t>(side)];
    source.group = {.side = side};
    source.regeneration_pending = false;
}

const SmallSoldierGroupState* ActorState::small_soldier_group(
    const SmallSoldierGroupSide side) const noexcept
{
    return &small_soldier_sources_[static_cast<std::size_t>(side)].group;
}

const SmallSoldierSourceState* ActorState::small_soldier_source(
    const SmallSoldierGroupSide side) const noexcept
{
    return &small_soldier_sources_[static_cast<std::size_t>(side)];
}

std::optional<SmallSoldierSourceStep>
ActorState::regenerate_small_soldier_source(const SmallSoldierGroupSide side) noexcept
{
    auto& source = small_soldier_sources_[static_cast<std::size_t>(side)];
    const auto step = devils_crush::native_runtime::regenerate_small_soldier_source(
        source);
    if (step.disposition != SmallSoldierSourceDisposition::regenerated) {
        return std::nullopt;
    }
    source = step.state;
    return step;
}

std::optional<ActorSmallSoldierSpawnUpdate> ActorState::spawn_small_soldier(
    const std::uint8_t slot,
    const SmallSoldierState state) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || record->behavior != ActorBehavior::small_soldier
        || record->small_soldier) {
        return std::nullopt;
    }

    auto& source = small_soldier_sources_[static_cast<std::size_t>(state.group_side)];
    const auto source_step = spawn_small_soldier_source_child(source);
    if (source_step.disposition != SmallSoldierSourceDisposition::child_spawned) {
        return std::nullopt;
    }

    source = source_step.state;
    record->small_soldier = state;
    record->small_soldier_group_registered = true;
    return ActorSmallSoldierSpawnUpdate{
        .slot = slot,
        .group_step = *source_step.group_step,
        .source_step = source_step,
    };
}

bool ActorState::register_captured_small_soldier(const std::uint8_t slot) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || record->behavior != ActorBehavior::small_soldier
        || !record->small_soldier || record->small_soldier_group_registered) {
        return false;
    }

    const auto& source = small_soldier_sources_[static_cast<std::size_t>(
        record->small_soldier->group_side)];
    if (source.regeneration_pending || source.group.live_children == 0U) {
        return false;
    }
    record->small_soldier_group_registered = true;
    return true;
}

bool ActorState::seed_small_soldier_source(SmallSoldierSourceState state) noexcept
{
    if (state.group.pending_spawns > 5U || state.group.live_children > 5U
        || static_cast<std::uint8_t>(
               state.group.pending_spawns + state.group.live_children) > 5U
        || state.appearance > 3U || state.regeneration_pending) {
        return false;
    }
    const auto index = static_cast<std::size_t>(state.group.side);
    small_soldier_sources_[index] = state;
    captured_small_soldier_sources_[index] = true;
    return true;
}

std::optional<SmallSoldierSourceStep>
ActorState::advance_captured_small_soldier_source(
    const SmallSoldierGroupSide side) noexcept
{
    const auto index = static_cast<std::size_t>(side);
    if (!captured_small_soldier_sources_[index]
        || small_soldier_sources_[index].regeneration_pending
        || small_soldier_sources_[index].group.pending_spawns == 0U) {
        return std::nullopt;
    }
    const auto step = advance_small_soldier_source_clock(
        small_soldier_sources_[index]);
    small_soldier_sources_[index] = step.state;
    return step;
}

std::optional<ActorSmallSoldierUpdate>
ActorState::apply_small_soldier_contact(const std::uint8_t slot) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || record->behavior != ActorBehavior::small_soldier
        || !record->small_soldier) {
        return std::nullopt;
    }

    auto state = *record->small_soldier;
    state.contact_pending = true;
    const auto step = devils_crush::native_runtime::consume_small_soldier_contact(
        state);
    std::optional<SmallSoldierGroupStep> group_step;
    std::optional<SmallSoldierSourceStep> source_step;
    if (step.decrement_group && record->small_soldier_group_registered) {
        auto& source = small_soldier_sources_[static_cast<std::size_t>(
            record->small_soldier->group_side)];
        source_step = complete_small_soldier_source_child(source);
        group_step = source_step->group_step;
        if (source_step->disposition != SmallSoldierSourceDisposition::invalid_state) {
            source = source_step->state;
        }
    }
    const ActorSmallSoldierUpdate update{
        .slot = slot,
        .step = step,
        .group_step = group_step,
        .source_step = source_step,
    };
    if (step.release_actor) {
        release(slot);
    } else {
        record->small_soldier = step.state;
    }
    return update;
}

std::optional<ActorSmallSoldierUpdate>
ActorState::complete_small_soldier_animation(const std::uint8_t slot) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || record->behavior != ActorBehavior::small_soldier
        || !record->small_soldier) {
        return std::nullopt;
    }

    const auto step = devils_crush::native_runtime::complete_small_soldier_animation(
        *record->small_soldier);
    std::optional<SmallSoldierGroupStep> group_step;
    std::optional<SmallSoldierSourceStep> source_step;
    if (step.decrement_group && record->small_soldier_group_registered) {
        auto& source = small_soldier_sources_[static_cast<std::size_t>(
            record->small_soldier->group_side)];
        source_step = complete_small_soldier_source_child(source);
        group_step = source_step->group_step;
        if (source_step->disposition != SmallSoldierSourceDisposition::invalid_state) {
            source = source_step->state;
        }
    }
    const ActorSmallSoldierUpdate update{
        .slot = slot,
        .step = step,
        .group_step = group_step,
        .source_step = source_step,
    };
    if (step.release_actor) {
        release(slot);
    } else {
        record->small_soldier = step.state;
    }
    return update;
}

std::optional<ActorCollisionReactionUpdate>
ActorState::advance_collision_reaction(const std::uint8_t slot) noexcept
{
    auto* record = actor(slot);
    if (record == nullptr || !record->active()
        || !record->collision_reaction) {
        return std::nullopt;
    }

    const auto step = devils_crush::native_runtime::advance_collision_reaction(
        *record->collision_reaction);
    record->collision_reaction = step.state;
    return ActorCollisionReactionUpdate{.slot = slot, .step = step};
}

std::optional<ActorCollisionTableUpdate> ActorState::advance_collision_table(
    const std::uint8_t slot,
    const CollisionTableRecord record) noexcept
{
    auto* actor = this->actor(slot);
    if (actor == nullptr || !actor->active() || !actor->collision_table) {
        return std::nullopt;
    }

    const auto step = devils_crush::native_runtime::advance_collision_table(
        *actor->collision_table, record);
    actor->collision_table = step.state;
    if (step.event_changed && actor->collision_reaction) {
        actor->collision_reaction->collision_event = step.state.collision_event;
    }
    return ActorCollisionTableUpdate{.slot = slot, .step = step};
}

std::optional<ActorCollisionTableResolutionUpdate>
ActorState::apply_collision_table_resolution(
    const std::uint8_t slot,
    const CollisionTableRecordResolution resolution) noexcept
{
    auto* actor = this->actor(slot);
    if (actor == nullptr || !actor->active() || !actor->collision_table) {
        return std::nullopt;
    }

    ActorCollisionTableResolutionUpdate update{
        .slot = slot,
        .resolution = resolution,
    };
    if (resolution.disposition == CollisionTableRecordDisposition::invalid_data) {
        return update;
    }

    actor->collision_table->collision_entry_cursor = resolution.cursor.entry;
    actor->collision_table->record_cursor = resolution.cursor.motion;
    if (resolution.disposition == CollisionTableRecordDisposition::end_of_table) {
        return update;
    }

    const auto step = devils_crush::native_runtime::advance_collision_table(
        *actor->collision_table, resolution.record);
    actor->collision_table = step.state;
    if (step.event_changed && actor->collision_reaction) {
        actor->collision_reaction->collision_event = step.state.collision_event;
    }
    update.advance = step;
    return update;
}

std::vector<ActorMotionUpdate> ActorState::advance_due_actors(
    const PlayfieldBounds playfield)
{
    std::vector<ActorMotionUpdate> updates;
    for (std::uint8_t slot = 0U; slot < kSlotCount; ++slot) {
        const auto update = advance_due_actor(slot, playfield);
        if (update) {
            updates.push_back(*update);
        }
    }
    return updates;
}

std::optional<ActorMotionUpdate> ActorState::advance_due_actor(
    const std::uint8_t slot,
    const PlayfieldBounds playfield)
{
    auto* actor = this->actor(slot);
    if (actor == nullptr || !actor->due_for_update
        || actor->behavior != ActorBehavior::shared_motion) {
        return std::nullopt;
    }

    const auto step = advance_actor_motion(actor->motion, playfield);
    actor->motion = step.state;
    return ActorMotionUpdate{.slot = slot, .step = step};
}

} // namespace devils_crush::native_runtime
