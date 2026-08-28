// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/dragon_egg_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kDragonEggType = 0x82U;
constexpr std::uint16_t kSteadyDragonEggHandler = 0x4345U;
constexpr std::uint16_t kDisabledDragonEggHandler = 0x4387U;
constexpr std::uint16_t kDragonEggHatchlingHandler = 0x441EU;
constexpr std::uint16_t kDragonEggControllerHandler = 0x42F6U;
constexpr std::uint8_t kDragonEggControllerType = 0x80U;

[[nodiscard]] bool has_live_hatchling(const GameStateView& state) noexcept
{
    for (std::uint8_t slot = 0U; slot < GameStateView::kActorSlotCount; ++slot) {
        const auto actor = state.actor(slot);
        if (actor && actor->activity_flags == kDragonEggType
            && actor->handler_address == kDragonEggHatchlingHandler) {
            return true;
        }
    }
    return false;
}

} // namespace

std::optional<DragonEggClusterCapture> capture_dragon_egg_cluster(
    const GameStateView& state) noexcept
{
    const auto cluster = state.dragon_egg_cluster();
    if (!cluster || cluster->live_eggs != kDragonEggCount
        || cluster->reset_requested || cluster->completion_latched
        || has_live_hatchling(state)) {
        return std::nullopt;
    }

    DragonEggClusterCapture capture{};
    for (std::size_t index = 0U; index < capture.actors.size(); ++index) {
        const auto slot = static_cast<std::uint8_t>(kDragonEggFirstSlot + index);
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kDragonEggType
            || actor->handler_address != kSteadyDragonEggHandler
            || (actor->collision_state & 0x80U) != 0U) {
            return std::nullopt;
        }
        capture.actors[index] = *actor;
    }
    return capture;
}

std::optional<DragonEggClusterCapture>
capture_partial_dragon_egg_cluster(const GameStateView& state) noexcept
{
    const auto cluster = state.dragon_egg_cluster();
    if (!cluster || cluster->reset_requested || cluster->live_eggs == 0U
        || cluster->live_eggs >= kDragonEggCount) {
        return std::nullopt;
    }

    DragonEggClusterCapture capture{};
    std::uint8_t active_eggs = 0U;
    for (std::size_t index = 0U; index < capture.actors.size(); ++index) {
        const auto slot = static_cast<std::uint8_t>(kDragonEggFirstSlot + index);
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kDragonEggType
            || (actor->collision_state & 0x80U) != 0U) {
            return std::nullopt;
        }
        if (actor->handler_address == kSteadyDragonEggHandler) {
            capture.rule_state.eggs[index].active = true;
            ++active_eggs;
        } else if (actor->handler_address == kDisabledDragonEggHandler) {
            capture.rule_state.eggs[index].active = false;
        } else {
            return std::nullopt;
        }
        capture.actors[index] = *actor;
    }
    if (active_eggs != cluster->live_eggs) {
        return std::nullopt;
    }

    if (cluster->completion_latched || has_live_hatchling(state)) {
        return std::nullopt;
    }
    return capture;
}

std::optional<DragonEggHatchlingCapture>
capture_dragon_egg_cluster_with_hatchling(const GameStateView& state) noexcept
{
    const auto cluster = state.dragon_egg_cluster();
    if (!cluster || cluster->reset_requested || cluster->completion_latched
        || cluster->live_eggs != kDragonEggCount - 1U) {
        return std::nullopt;
    }

    DragonEggClusterCapture fixed_capture{};
    std::optional<std::uint8_t> disabled_egg_slot;
    for (std::size_t index = 0U; index < fixed_capture.actors.size(); ++index) {
        const auto slot = static_cast<std::uint8_t>(kDragonEggFirstSlot + index);
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kDragonEggType
            || (actor->collision_state & 0x80U) != 0U) {
            return std::nullopt;
        }
        if (actor->handler_address == kSteadyDragonEggHandler) {
            fixed_capture.rule_state.eggs[index].active = true;
        } else if (actor->handler_address == kDisabledDragonEggHandler
                   && !disabled_egg_slot) {
            fixed_capture.rule_state.eggs[index].active = false;
            disabled_egg_slot = slot;
        } else {
            return std::nullopt;
        }
        fixed_capture.actors[index] = *actor;
    }
    if (!disabled_egg_slot) {
        return std::nullopt;
    }

    std::optional<GameStateView::ActorSlot> hatchling_actor;
    for (std::uint8_t slot = 0U; slot < GameStateView::kActorSlotCount; ++slot) {
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kDragonEggType
            || actor->handler_address != kDragonEggHatchlingHandler) {
            continue;
        }
        if (hatchling_actor) {
            return std::nullopt;
        }
        hatchling_actor = *actor;
    }
    if (!hatchling_actor) {
        return std::nullopt;
    }

    return DragonEggHatchlingCapture{
        .cluster = fixed_capture,
        .actor = *hatchling_actor,
        .hatchling = {
            .source_egg_slot = *disabled_egg_slot,
            .dynamic_slot = hatchling_actor->index,
            .position = {
                .horizontal_pixels = static_cast<std::int16_t>(
                    hatchling_actor->collision_table.position_x),
                .vertical_pixels = static_cast<std::int16_t>(
                    hatchling_actor->collision_table.position_y),
            },
            .animation_timer = hatchling_actor->collision_reaction.ticks_until_next_phase,
        },
    };
}

std::optional<DragonEggClusterCapture>
capture_completed_dragon_egg_cluster(const GameStateView& state) noexcept
{
    const auto cluster = state.dragon_egg_cluster();
    const auto controller = state.actor(kDragonEggControllerSlot);
    if (!cluster || cluster->live_eggs != 0U || cluster->reset_requested
        || !cluster->completion_latched || !controller
        || controller->activity_flags != kDragonEggControllerType
        || controller->handler_address != kDragonEggControllerHandler
        || has_live_hatchling(state)) {
        return std::nullopt;
    }

    DragonEggClusterCapture capture{};
    for (std::size_t index = 0U; index < capture.actors.size(); ++index) {
        const auto slot = static_cast<std::uint8_t>(kDragonEggFirstSlot + index);
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kDragonEggType
            || actor->handler_address != kDisabledDragonEggHandler
            || (actor->collision_state & 0x80U) != 0U) {
            return std::nullopt;
        }
        capture.actors[index] = *actor;
        capture.rule_state.eggs[index].active = false;
    }
    capture.rule_state.completed = true;
    return capture;
}

std::optional<DragonEggClusterCapture>
capture_dragon_egg_reset_cluster(const GameStateView& state) noexcept
{
    const auto cluster = state.dragon_egg_cluster();
    const auto controller = state.actor(kDragonEggControllerSlot);
    if (!cluster || !cluster->reset_requested || !cluster->completion_latched
        || cluster->live_eggs >= kDragonEggCount || !controller
        || controller->activity_flags != kDragonEggControllerType
        || controller->handler_address != kDragonEggControllerHandler
        || has_live_hatchling(state)) {
        return std::nullopt;
    }

    DragonEggClusterCapture capture{};
    std::uint8_t active_eggs = 0U;
    for (std::size_t index = 0U; index < capture.actors.size(); ++index) {
        const auto slot = static_cast<std::uint8_t>(kDragonEggFirstSlot + index);
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kDragonEggType
            || (actor->collision_state & 0x80U) != 0U) {
            return std::nullopt;
        }
        if (actor->handler_address == kSteadyDragonEggHandler) {
            capture.rule_state.eggs[index].active = true;
            ++active_eggs;
        } else if (actor->handler_address == kDisabledDragonEggHandler) {
            capture.rule_state.eggs[index].active = false;
        } else {
            return std::nullopt;
        }
        capture.actors[index] = *actor;
    }
    if (active_eggs != cluster->live_eggs) {
        return std::nullopt;
    }
    capture.rule_state.completed = true;
    capture.rule_state.reset_pending = true;
    return capture;
}

} // namespace devils_crush::native_runtime
