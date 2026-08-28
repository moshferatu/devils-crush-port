// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/small_soldier_capture.hpp"

#include "devils_crush/native_runtime/actor_collision_seed.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kSmallSoldierType = 0x82U;
constexpr std::uint16_t kInitialCollisionHandler = 0x7F1AU;
constexpr std::uint16_t kSteadyCollisionHandler = 0x7F57U;
constexpr std::uint8_t kLowerLeftSourceSlot = 0x34U;
constexpr std::uint8_t kUpperRightSourceSlot = 0x35U;
constexpr std::uint16_t kLowerLeftDelayedHandler = 0x7D6FU;
constexpr std::uint16_t kLowerLeftSteadyHandler = 0x7D85U;
constexpr std::uint16_t kUpperRightDelayedHandler = 0x7E0DU;
constexpr std::uint16_t kUpperRightSteadyHandler = 0x7E23U;

[[nodiscard]] bool is_collision_handler(
    const std::uint16_t handler) noexcept
{
    return handler == kInitialCollisionHandler || handler == kSteadyCollisionHandler;
}

[[nodiscard]] bool is_source_handler(
    const SmallSoldierGroupSide side,
    const std::uint16_t handler) noexcept
{
    if (side == SmallSoldierGroupSide::lower_left) {
        return handler == kLowerLeftDelayedHandler || handler == kLowerLeftSteadyHandler;
    }
    return handler == kUpperRightDelayedHandler || handler == kUpperRightSteadyHandler;
}

[[nodiscard]] std::uint8_t source_slot(
    const SmallSoldierGroupSide side) noexcept
{
    return side == SmallSoldierGroupSide::lower_left ? kLowerLeftSourceSlot
                                                     : kUpperRightSourceSlot;
}

} // namespace

std::optional<SmallSoldierSourceCapture> capture_small_soldier_source(
    const GameStateView& state,
    const SmallSoldierGroupSide side) noexcept
{
    const auto slot = source_slot(side);
    const auto actor = state.actor(slot);
    const auto source = state.small_soldier_source(slot);
    if (!actor || !source || actor->activity_flags != kSmallSoldierType
        || !is_source_handler(side, actor->handler_address)
        || source->pending_spawns > 5U || source->live_children > 5U
        || static_cast<std::uint8_t>(source->pending_spawns + source->live_children)
            > 5U
        || source->appearance > 3U
        // An empty generation has entered its source-owned completion and
        // presentation path.  It cannot yet be resumed by this capture.
        || (source->pending_spawns == 0U && source->live_children == 0U)) {
        return std::nullopt;
    }

    const auto side_index = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(actor->horizontal_motion.raw) >> 8U);
    if (side_index != static_cast<std::uint8_t>(side)) {
        return std::nullopt;
    }

    return SmallSoldierSourceCapture{
        .actor = *actor,
        .source_state = {
            .group = {
                .side = side,
                .pending_spawns = source->pending_spawns,
                .live_children = source->live_children,
            },
            .appearance = source->appearance,
            .ticks_until_next_spawn = source->ticks_until_next_spawn,
        },
    };
}

std::optional<SmallSoldierActorCapture>
capture_small_soldier_collision_actor(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    if (!actor || actor->activity_flags != kSmallSoldierType
        || !is_collision_handler(actor->handler_address)
        || actor->behavior_counter == 0U || actor->behavior_counter > 2U) {
        return std::nullopt;
    }

    const auto side_index = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(actor->horizontal_motion.raw) >> 8U);
    if (side_index > static_cast<std::uint8_t>(SmallSoldierGroupSide::upper_right)) {
        return std::nullopt;
    }

    return SmallSoldierActorCapture{
        .actor = *actor,
        .rule_state = {
            .hits_remaining = actor->behavior_counter,
            .subtype = static_cast<std::uint8_t>(
                actor->collision_table.orientation_flags & 0x0FU),
            .group_side = static_cast<SmallSoldierGroupSide>(side_index),
            .contact_pending = (actor->collision_state & 0x80U) != 0U,
        },
    };
}

bool seed_small_soldier_collision_actor(
    ActorState& actors,
    const SmallSoldierActorCapture& source) noexcept
{
    const auto* target = actors.actor(source.actor.index);
    if (target == nullptr || target->behavior != ActorBehavior::small_soldier) {
        return false;
    }

    return seed_actor_collision_state(actors, source.actor)
        && actors.set_actor_appearance(
            source.actor.index, source.actor.collision_shape_directory_index)
        && actors.set_collision_effect_association(
            source.actor.index, source.actor.collision_table.orientation_flags)
        && actors.configure_small_soldier(source.actor.index, source.rule_state);
}

} // namespace devils_crush::native_runtime
