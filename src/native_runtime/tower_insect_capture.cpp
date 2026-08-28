// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_insect_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kTowerInsectType = 0x82U;
constexpr std::uint16_t kSpawnCountdownSourceHandler = 0x5F4AU;
constexpr std::uint16_t kSteadySourceHandler = 0x5F5FU;
constexpr std::uint16_t kSteadyInsectHandler = 0x6016U;

} // namespace

std::optional<TowerInsectGenerationCapture>
capture_tower_insect_generation(const GameStateView& state) noexcept
{
    const auto source_actor = state.actor(kTowerInsectSourceSlot);
    const auto source = state.tower_insect_source(kTowerInsectSourceSlot);
    if (!source_actor || !source || source_actor->activity_flags != kTowerInsectType
        || (source_actor->handler_address != kSpawnCountdownSourceHandler
            && source_actor->handler_address != kSteadySourceHandler)
        || source_actor->collision_state != 0U
        || source->live_children > kTowerInsectObservedSlots.size()) {
        return std::nullopt;
    }

    const auto spawning = source_actor->handler_address
        == kSpawnCountdownSourceHandler;
    if ((spawning && (source->spawns_remaining == 0U
                      || source->spawns_remaining > kTowerInsectObservedSlots.size()
                      || source->ticks_until_next_spawn == 0U
                      || source->ticks_until_next_spawn > 48U
                      || static_cast<std::size_t>(source->spawns_remaining)
                          + source->live_children
                          > kTowerInsectObservedSlots.size()))
        || (!spawning && (source->spawns_remaining != 0U
                          || source->live_children == 0U))) {
        return std::nullopt;
    }

    TowerInsectGenerationCapture capture{
        .source_actor = *source_actor,
        .generation = {
            .source = {
                .generation_active = true,
                .spawns_remaining = source->spawns_remaining,
                .live_children = source->live_children,
                .child_appearance = source_actor->collision_shape_directory_index,
                .child_collision_effect_association = source_actor->collision_table
                    .orientation_flags,
                .ticks_until_next_spawn = source->ticks_until_next_spawn,
            },
        },
    };
    std::uint8_t captured_children = 0U;
    for (std::size_t index = 0U; index < kTowerInsectObservedSlots.size(); ++index) {
        const auto actor = state.actor(kTowerInsectObservedSlots[index]);
        if (!actor || actor->activity_flags != kTowerInsectType
            || actor->handler_address != kSteadyInsectHandler) {
            continue;
        }
        if (actor->collision_state != 0U || actor->behavior_counter == 0U
            || actor->behavior_counter > 2U
            || actor->collision_shape_directory_index
                != source_actor->collision_shape_directory_index
            || (actor->collision_table.orientation_flags & 0xF0U)
                != (source_actor->collision_table.orientation_flags & 0xF0U)
            || ((actor->collision_table.orientation_flags & 0x0FU)
                    != (source_actor->collision_table.orientation_flags & 0x0FU)
                && (actor->collision_table.orientation_flags & 0x0FU) != 0x0FU)) {
            return std::nullopt;
        }
        capture.insect_actors[index] = *actor;
        capture.generation.insects[index] = {
            .hits_remaining = actor->behavior_counter,
            .appearance = source_actor->collision_shape_directory_index,
            .collision_effect_association = actor->collision_table.orientation_flags,
        };
        ++captured_children;
    }

    if (captured_children != source->live_children) {
        return std::nullopt;
    }
    return capture;
}

} // namespace devils_crush::native_runtime
