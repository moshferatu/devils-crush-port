// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_effect_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kCollisionEffectType = 0x8FU;
constexpr std::uint8_t kCollisionEffectAttributes = 0xE0U;
constexpr std::uint16_t kCollisionEffectHandler = 0x7C34U;

} // namespace

std::optional<CollisionEffectCapture> capture_collision_effect(
    const GameStateView& state,
    const std::uint8_t effect_slot) noexcept
{
    const auto effect_actor = state.actor(effect_slot);
    const auto effect_slot_state = state.collision_effect(effect_slot);
    if (!effect_actor || !effect_slot_state
        || effect_actor->activity_flags != kCollisionEffectType
        || effect_actor->attributes != kCollisionEffectAttributes
        || effect_actor->handler_address != kCollisionEffectHandler
        || effect_slot_state->source_slot >= GameStateView::kActorSlotCount
        || effect_slot_state->ticks_remaining == 0U
        || effect_slot_state->ticks_remaining > kCollisionEffectLifetime) {
        return std::nullopt;
    }

    const auto source_actor = state.actor(effect_slot_state->source_slot);
    if (!source_actor || source_actor->activity_flags == 0U
        || (source_actor->collision_table.orientation_flags & 0x0FU) != 0x0FU
        || (effect_slot_state->source_association & 0xF0U)
            != (source_actor->collision_table.orientation_flags & 0xF0U)
        || effect_actor->collision_shape_directory_index
            != source_actor->collision_shape_directory_index) {
        return std::nullopt;
    }

    return CollisionEffectCapture{
        .source_actor = *source_actor,
        .effect_actor = *effect_actor,
        .effect = {
            .source_slot = effect_slot_state->source_slot,
            .source_association = effect_slot_state->source_association,
            .source_appearance = source_actor->collision_shape_directory_index,
            .ticks_remaining = effect_slot_state->ticks_remaining,
        },
    };
}

} // namespace devils_crush::native_runtime
