// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4_linked_target_capture.hpp"

#include <algorithm>

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kLinkedTargetType = 0xC2U;
constexpr std::uint16_t kInitialTargetHandler = 0x6B7EU;
constexpr std::uint16_t kPostAllocationTargetHandler = 0x6CA0U;
constexpr std::uint16_t kHitPresentationHandler = 0x6C21U;
constexpr std::uint8_t kLiveTargetSourceMarker = 0xFFU;
constexpr std::uint8_t kHitPresentationAppearance = 0x04U;
constexpr std::uint16_t kOrbitingChildHandler = 0x6D27U;
constexpr std::uint8_t kOrbitPhaseCount = 0x20U;

[[nodiscard]] bool is_target_handler(const std::uint16_t handler) noexcept
{
    return handler == kInitialTargetHandler || handler == kPostAllocationTargetHandler;
}

[[nodiscard]] std::optional<std::uint8_t> source_index_for_slot(
    const std::uint8_t slot) noexcept
{
    const auto source = std::find(
        kBonusStage4SourceSlots.begin(), kBonusStage4SourceSlots.end(), slot);
    if (source == kBonusStage4SourceSlots.end()) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(source - kBonusStage4SourceSlots.begin());
}

} // namespace

std::optional<BonusStage4LinkedTargetCapture>
capture_bonus_stage_4_linked_target(const GameStateView& state) noexcept
{
    std::optional<BonusStage4LinkedTargetCapture> capture;
    for (std::uint8_t slot = 0U; slot < GameStateView::kActorSlotCount; ++slot) {
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kLinkedTargetType) {
            continue;
        }

        const auto is_hit_presentation = actor->handler_address == kHitPresentationHandler
            && actor->collision_shape_phase == kHitPresentationAppearance;
        const auto is_steady_target = is_target_handler(actor->handler_address);
        if (!is_steady_target && !is_hit_presentation) {
            continue;
        }
        const auto contact_latched = (actor->collision_state & 0x80U) != 0U;
        if (contact_latched != is_hit_presentation) {
            return std::nullopt;
        }

        const auto target = state.bonus_stage_4_linked_target(slot);
        if (!target || target->ticks_until_motion_refresh == 0U) {
            return std::nullopt;
        }
        const auto objective_index = target->objective_index;
        const auto source_slot = target->source_slot;
        const auto source_index = source_index_for_slot(source_slot);
        const auto source = state.actor(source_slot);
        if (!source_index || *source_index != objective_index || !source
            || source->collision_reaction.phase != kLiveTargetSourceMarker) {
            return std::nullopt;
        }
        if (capture) {
            return std::nullopt;
        }
        capture = BonusStage4LinkedTargetCapture{
            .actor = *actor,
            .state = {
                .source_slot = source_slot,
                .objective_index = objective_index,
                .dynamic_slot = slot,
                .position = {
                    .horizontal_pixels = actor->collision_table.position_x,
                    .vertical_pixels = actor->collision_table.position_y,
                },
                .ticks_until_motion_refresh = target->ticks_until_motion_refresh,
                .hit_presentation_pending = is_hit_presentation,
            },
        };
    }
    return capture;
}

std::optional<BonusStage4LinkedTargetGenerationCapture>
capture_bonus_stage_4_linked_target_generation(const GameStateView& state) noexcept
{
    const auto target = capture_bonus_stage_4_linked_target(state);
    if (!target) {
        return std::nullopt;
    }

    BonusStage4LinkedTargetGenerationCapture generation{.target = *target};
    std::size_t child_count = 0U;
    for (std::uint8_t slot = 0U; slot < GameStateView::kActorSlotCount; ++slot) {
        const auto actor = state.actor(slot);
        if (!actor || actor->activity_flags != kLinkedTargetType
            || actor->handler_address != kOrbitingChildHandler
            || actor->collision_table.record_cursor != target->state.dynamic_slot) {
            continue;
        }
        // A child tied to this target must be at a clear $6D27 orbit
        // boundary.  A collision or an invalid phase would need a CPU
        // continuation, so reject the whole linked generation.
        const auto orbit_phase = actor->collision_table.collision_entry_cursor;
        if ((actor->collision_state & 0x80U) != 0U
            || orbit_phase >= kOrbitPhaseCount || child_count == generation.children.size()) {
            return std::nullopt;
        }
        const auto expected_position = bonus_stage_4_orbit_position(
            target->state.position, orbit_phase);
        if (actor->collision_table.position_x != expected_position.horizontal_pixels
            || actor->collision_table.position_y != expected_position.vertical_pixels) {
            return std::nullopt;
        }
        generation.children[child_count++] = BonusStage4OrbitingChildCapture{
            .actor = *actor,
            .state = {
                .target_slot = target->state.dynamic_slot,
                .dynamic_slot = slot,
                .orbit_phase = orbit_phase,
                .position = expected_position,
            },
        };
    }
    // A pending hit and a resumed-but-not-yet-clear allocation handoff cannot
    // coexist with a clear child record.
    if ((target->state.hit_presentation_pending
         || target->state.orbiting_children_pending != 0U)
        && child_count != 0U) {
        return std::nullopt;
    }
    return generation;
}

} // namespace devils_crush::native_runtime
