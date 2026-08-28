// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4_linked_target.hpp"

#include <algorithm>

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint16_t kTargetPositionOffsetPixels = 0x10U;
constexpr std::uint16_t kTargetContactAwardUnits = 100U;
constexpr std::uint8_t kTargetContactAudioCommand = 0x3BU;
constexpr std::uint8_t kOrbitingChildCount = 4U;
constexpr std::uint16_t kOrbitingChildContactAwardUnits = 100U;
constexpr std::uint8_t kMotionRefreshTicks = 0x20U;
constexpr std::uint8_t kOrbitPhaseMask = 0x1FU;
constexpr std::uint8_t kOrbitPhaseStride = 0x08U;

struct OrbitOffset {
    std::int16_t horizontal_pixels = 0;
    std::int16_t vertical_pixels = 0;
};

constexpr std::array<OrbitOffset, 32U> kOrbitOffsets{{
    {24, 8}, {23, 11}, {23, 14}, {21, 17}, {19, 19}, {17, 21}, {14, 23},
    {11, 23}, {8, 24}, {5, 23}, {2, 23}, {-1, 21}, {-3, 19}, {-5, 17},
    {-7, 14}, {-7, 11}, {-8, 8}, {-7, 5}, {-7, 2}, {-5, -1}, {-3, -3},
    {-1, -5}, {2, -7}, {5, -7}, {8, -8}, {11, -7}, {14, -7}, {17, -5},
    {19, -3}, {21, -1}, {23, 2}, {23, 5},
}};

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

BonusStage4LinkedTargetPosition bonus_stage_4_orbit_position(
    const BonusStage4LinkedTargetPosition target_position,
    const std::uint8_t orbit_phase) noexcept
{
    const auto offset = kOrbitOffsets[orbit_phase & kOrbitPhaseMask];
    return {
        .horizontal_pixels = static_cast<std::uint16_t>(
            static_cast<std::int32_t>(target_position.horizontal_pixels)
            + offset.horizontal_pixels),
        .vertical_pixels = static_cast<std::uint16_t>(
            static_cast<std::int32_t>(target_position.vertical_pixels)
            + offset.vertical_pixels),
    };
}

BonusStage4LinkedTargetMotionStep advance_bonus_stage_4_linked_target_motion(
    BonusStage4LinkedTargetState target) noexcept
{
    const auto previous = target;
    if (target.ticks_until_motion_refresh > 1U) {
        --target.ticks_until_motion_refresh;
        return {.previous = previous, .state = target};
    }

    // The allocator initializes this byte to one. A zero snapshot is not a
    // valid steady target capture, but preserving the expiry outcome here
    // keeps a manually seeded native state bounded and deterministic.
    target.ticks_until_motion_refresh = kMotionRefreshTicks;
    return {
        .previous = previous,
        .state = target,
        .disposition = BonusStage4LinkedTargetMotionDisposition::random_motion_requested,
    };
}

BonusStage4LinkedTargetAllocationStep allocate_bonus_stage_4_linked_target(
    const std::uint8_t source_slot,
    const std::uint8_t objective_index,
    const std::uint8_t dynamic_slot,
    const BonusStage4LinkedTargetPosition source_position) noexcept
{
    BonusStage4LinkedTargetAllocationStep step{
        .source_slot = source_slot,
        .objective_index = objective_index,
        .dynamic_slot = dynamic_slot,
    };
    const auto source_index = source_index_for_slot(source_slot);
    if (!source_index || *source_index != objective_index) {
        return step;
    }
    step.disposition = BonusStage4LinkedTargetAllocationDisposition::allocated;
    step.target = BonusStage4LinkedTargetState{
        .source_slot = source_slot,
        .objective_index = objective_index,
        .dynamic_slot = dynamic_slot,
        .position = {
            .horizontal_pixels = static_cast<std::uint16_t>(
                source_position.horizontal_pixels + kTargetPositionOffsetPixels),
            .vertical_pixels = static_cast<std::uint16_t>(
                source_position.vertical_pixels + kTargetPositionOffsetPixels),
        },
    };
    return step;
}

BonusStage4LinkedTargetRulesUpdate resolve_bonus_stage_4_linked_target_contact(
    BonusStage4LinkedTargetState target,
    const ScoreState score) noexcept
{
    const auto previous = target;
    BonusStage4LinkedTargetContactStep step{
        .previous = previous,
        .state = target,
    };
    // 05:6C24 keeps the target's contact latch set while its four child
    // allocations run, then clears it only after the loop. Keep that whole
    // scheduler continuation non-contactable even though the presentation
    // handoff itself has already completed.
    if (target.hit_presentation_pending || target.orbiting_children_pending != 0U) {
        const auto no_award = apply_score_award(score, 0U);
        return {.target = step, .score_award = no_award};
    }
    target.hit_presentation_pending = true;
    step = {
        .previous = previous,
        .state = target,
        .disposition = BonusStage4LinkedTargetContactDisposition::accepted,
        .score_award_units = kTargetContactAwardUnits,
        .audio_command = kTargetContactAudioCommand,
        .requests_orbiting_children = false,
    };
    return {
        .target = step,
        .score_award = apply_score_award(score, step.score_award_units),
    };
}

BonusStage4LinkedTargetHitPresentationStep
complete_bonus_stage_4_linked_target_hit_presentation(
    BonusStage4LinkedTargetState target) noexcept
{
    const auto previous = target;
    BonusStage4LinkedTargetHitPresentationStep step{
        .previous = previous,
        .state = target,
    };
    if (!target.hit_presentation_pending) {
        return step;
    }

    target.hit_presentation_pending = false;
    target.orbiting_children_pending = kOrbitingChildCount;
    return {
        .previous = previous,
        .state = target,
        .disposition =
            BonusStage4LinkedTargetHitPresentationDisposition::child_allocation_requested,
        .requests_orbiting_children = true,
    };
}

BonusStage4OrbitingChildAllocationStep allocate_bonus_stage_4_orbiting_child(
    BonusStage4LinkedTargetState target,
    const std::uint8_t dynamic_slot) noexcept
{
    const auto previous = target;
    BonusStage4OrbitingChildAllocationStep step{
        .previous_target = previous,
        .target = target,
    };
    if (target.hit_presentation_pending || target.orbiting_children_pending == 0U) {
        return step;
    }
    const auto initial_orbit_phase = static_cast<std::uint8_t>(
        (target.orbiting_children_pending - 1U) * kOrbitPhaseStride);
    --target.orbiting_children_pending;
    step.target = target;
    step.disposition = BonusStage4OrbitingChildAllocationDisposition::allocated;
    step.child = BonusStage4OrbitingChildState{
        .target_slot = target.dynamic_slot,
        .dynamic_slot = dynamic_slot,
        .orbit_phase = initial_orbit_phase,
        .position = bonus_stage_4_orbit_position(target.position, initial_orbit_phase),
    };
    return step;
}

BonusStage4OrbitingChildOrbitStep advance_bonus_stage_4_orbiting_child_orbit(
    BonusStage4OrbitingChildState child,
    const BonusStage4LinkedTargetPosition target_position,
    const bool target_is_live,
    const bool collision_pending) noexcept
{
    const auto previous = child;
    if (!target_is_live) {
        return {.previous = previous, .state = child};
    }
    if (collision_pending) {
        return {
            .previous = previous,
            .state = child,
            .disposition = BonusStage4OrbitingChildOrbitDisposition::collision_pending,
        };
    }
    child.orbit_phase = static_cast<std::uint8_t>(
        (child.orbit_phase + 1U) & kOrbitPhaseMask);
    child.position = bonus_stage_4_orbit_position(target_position, child.orbit_phase);
    return {
        .previous = previous,
        .state = child,
        .disposition = BonusStage4OrbitingChildOrbitDisposition::advanced,
    };
}

BonusStage4OrbitingChildRulesUpdate resolve_bonus_stage_4_orbiting_child_contact(
    const BonusStage4OrbitingChildState child,
    const ScoreState score) noexcept
{
    const BonusStage4OrbitingChildContactStep step{
        .previous = child,
        .state = child,
        .releases_child = true,
        .score_award_units = kOrbitingChildContactAwardUnits,
    };
    return {
        .child = step,
        .score_award = apply_score_award(score, step.score_award_units),
    };
}

BonusStage4OrbitingChildFrameUpdate advance_bonus_stage_4_orbiting_child_frame(
    const BonusStage4OrbitingChildState child,
    const BonusStage4LinkedTargetPosition target_position,
    const bool target_is_live,
    const bool collision_pending,
    const ScoreState score) noexcept
{
    const auto previous = child;
    if (!target_is_live || collision_pending) {
        const auto contact = resolve_bonus_stage_4_orbiting_child_contact(child, score);
        return {
            .child = {
                .previous = previous,
                .state = contact.child.state,
                .disposition = !target_is_live
                    ? BonusStage4OrbitingChildFrameDisposition::released_after_parent_loss
                    : BonusStage4OrbitingChildFrameDisposition::released_after_collision,
                .releases_child = contact.child.releases_child,
                .score_award_units = contact.child.score_award_units,
            },
            .score_award = contact.score_award,
        };
    }

    const auto orbit = advance_bonus_stage_4_orbiting_child_orbit(
        child, target_position, true, false);
    return {
        .child = {
            .previous = previous,
            .state = orbit.state,
            .disposition = BonusStage4OrbitingChildFrameDisposition::advanced,
        },
        .score_award = apply_score_award(score, 0U),
    };
}

} // namespace devils_crush::native_runtime
