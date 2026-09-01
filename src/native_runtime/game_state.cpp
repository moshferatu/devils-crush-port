// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/game_state.hpp"

#include <bit>

namespace devils_crush::native_runtime {
namespace {

constexpr std::size_t kActorActivityOffset = 0x1000U;
constexpr std::size_t kActorAttributesOffset = 0x1080U;
constexpr std::size_t kActorPassFreezeLatchOffset = 0x0D35U;
constexpr std::size_t kActorEnableMaskOffset = 0x0CEEU;
constexpr std::size_t kActorPresentationAuxiliaryQueueOffset = 0x0C3AU;
constexpr std::size_t kActorPresentationPrimaryQueueOffset = 0x0BBAU;
constexpr std::size_t kActorPresentationAuxiliaryCountOffset = 0x0CBCU;
constexpr std::size_t kActorPresentationPrimaryLowCountOffset = 0x0CBAU;
constexpr std::size_t kActorPresentationPrimaryHighNextIndexOffset = 0x0CBBU;
constexpr std::size_t kBonusStage4ObjectiveFlagsOffset = 0x0D2FU;
constexpr std::size_t kActorHorizontalMotionLowOffset = 0x1400U;
constexpr std::size_t kActorHorizontalMotionHighOffset = 0x1480U;
constexpr std::size_t kActorVerticalMotionLowOffset = 0x1500U;
constexpr std::size_t kActorVerticalMotionHighOffset = 0x1580U;
constexpr std::size_t kActorCollisionTableSelectorOffset = 0x1700U;
constexpr std::size_t kActorCollisionOffset = 0x1800U;
constexpr std::size_t kActorCollisionTablePassOffset = 0x1880U;
constexpr std::size_t kActorCollisionShapeDirectoryIndexOffset = 0x1900U;
constexpr std::size_t kActorCollisionShapePhaseOffset = 0x1980U;
constexpr std::size_t kActorBehaviorCounterOffset = 0x1C00U;
constexpr std::size_t kActorCollisionReactionCountdownOffset = 0x1A00U;
constexpr std::size_t kActorCollisionEventOffset = 0x1A80U;
constexpr std::size_t kActorCollisionReactionPhaseOffset = 0x1C80U;
constexpr std::size_t kActorCollisionOrientationOffset = 0x1D00U;
constexpr std::size_t kActorCollisionEffectLifetimeOffset = 0x1D80U;
constexpr std::size_t kActorEyeballFinalWaitLowOffset = 0x1B00U;
constexpr std::size_t kActorEyeballFinalWaitHighOffset = 0x1B80U;
constexpr std::size_t kBonusStage4LinkedTargetMotionRefreshOffset = 0x1B00U;
constexpr std::size_t kDragonEggLiveCountOffset = 0x0CF4U;
constexpr std::size_t kDragonEggResetFlagOffset = 0x0CF7U;
constexpr std::size_t kDragonEggCompletionLatchOffset = 0x0D20U;
constexpr std::size_t kGiantFacePersistedFormOffset = 0x0258U;
constexpr std::size_t kGiantFaceRequestFlagsOffset = 0x0D0CU;
constexpr std::size_t kGiantFaceDisplaySelectorOffset = 0x0D0DU;
constexpr std::size_t kGiantFaceAnimationModeOffset = 0x0D0EU;
constexpr std::size_t kGiantFaceMouthFlagsOffset = 0x0D21U;
constexpr std::size_t kActorCollisionEntryCursorOffset = 0x1E00U;
constexpr std::size_t kActorCollisionMotionCursorOffset = 0x1E80U;
constexpr std::size_t kActorHandlerLowOffset = 0x1F00U;
constexpr std::size_t kActorHandlerHighOffset = 0x1F80U;
constexpr std::size_t kActorPositionXLowOffset = 0x1180U;
constexpr std::size_t kActorPositionXHighOffset = 0x1200U;
constexpr std::size_t kActorPositionYLowOffset = 0x1300U;
constexpr std::size_t kActorPositionYHighOffset = 0x1380U;
constexpr std::size_t kActorBumperPrimaryAppearanceOffset = 0x1B00U;
constexpr std::size_t kActorBumperSecondaryDamageOffset = 0x1B80U;
constexpr std::size_t kTowerHitProgressOffset = 0x0D3FU;
constexpr std::size_t kTowerRemainingArmedPassesOffset = 0x0D4DU;
constexpr std::size_t kTowerInsectLiveChildrenOffset = 0x0D2BU;
constexpr std::size_t kActorSpawnBudgetOffset = 0x1B00U;
constexpr std::size_t kLeftSmallSoldierLiveChildrenOffset = 0x0D16U;
constexpr std::size_t kRightSmallSoldierLiveChildrenOffset = 0x0D17U;
constexpr std::size_t kBallSlot = 0U;
constexpr std::size_t kBallPositionXFractionalOffset = 0x1100U;
constexpr std::size_t kBallPositionXLowOffset = 0x1180U;
constexpr std::size_t kBallPositionXHighOffset = 0x1200U;
constexpr std::size_t kBallPositionYFractionalOffset = 0x1280U;
constexpr std::size_t kBallPositionYLowOffset = 0x1300U;
constexpr std::size_t kBallPositionYHighOffset = 0x1380U;
constexpr std::size_t kBallMotionFlagsOffset = 0x1C80U;
constexpr std::size_t kBallSubstepHorizontalLowOffset = 0x00B9U;
constexpr std::size_t kBallSubstepHorizontalHighOffset = 0x00BAU;
constexpr std::size_t kBallSubstepVerticalLowOffset = 0x00BBU;
constexpr std::size_t kBallSubstepVerticalHighOffset = 0x00BCU;
constexpr std::size_t kBallCompletedSubstepsOffset = 0x0056U;
constexpr std::size_t kBallRequestedSubstepsOffset = 0x0057U;
constexpr std::size_t kBallVerticalGravityOffset = 0x008BU;
constexpr std::size_t kBallHorizontalDragDivisorOffset = 0x008CU;
constexpr std::size_t kBallVerticalDragDivisorOffset = 0x008DU;
constexpr std::size_t kBallCollisionCacheXFractionalOffset = 0x0085U;
constexpr std::size_t kBallCollisionCacheXLowOffset = 0x0086U;
constexpr std::size_t kBallCollisionCacheXHighOffset = 0x0087U;
constexpr std::size_t kBallCollisionCacheYFractionalOffset = 0x0088U;
constexpr std::size_t kBallCollisionCacheYLowOffset = 0x0089U;
constexpr std::size_t kBallCollisionCacheYHighOffset = 0x008AU;
constexpr std::size_t kCollisionResponseOrientationOffset = 0x0078U;
constexpr std::size_t kCollisionResponseForceReducedOffset = 0x0CC7U;
constexpr std::size_t kCollisionResponseQuarterScaleOffset = 0x022CU;
constexpr std::size_t kCollisionResponseLowTuningOffset = 0x0081U;
constexpr std::size_t kCollisionResponseHighTuningOffset = 0x0082U;
constexpr std::size_t kActiveScoreSlotOffset = 0x0251U;
constexpr std::size_t kScoreLowBaseOffset = 0x0205U;
constexpr std::size_t kScoreMiddleBaseOffset = 0x020CU;
constexpr std::size_t kScoreHighBaseOffset = 0x0213U;
constexpr std::size_t kScoreDisplayDirtyOffset = 0x0CC0U;
constexpr std::uint8_t kScoreSlotCount = 7U;
constexpr std::size_t kLeftFrontSpiderSafetyOffset = 0x0D13U;
constexpr std::size_t kRightFrontSpiderSafetyOffset = 0x0D14U;
constexpr std::size_t kTowerCompletedOffset = 0x0CFCU;
constexpr std::size_t kSideSpiderTransitionRequestedOffset = 0x0D32U;
constexpr std::size_t kActorCollisionScanSessionActiveOffset = 0x0254U;
constexpr std::size_t kActorCollisionSideFlagsOffset = 0x00B7U;
constexpr std::size_t kNormalActorCollisionScanGateOffset = 0x0055U;
constexpr std::size_t kNormalActorCollisionScanSlotCountOffset = 0x0060U;
constexpr std::size_t kNormalActorCollisionScanFirstSlotOffset = 0x0064U;
constexpr std::size_t kNormalActorCollisionProbeHorizontalOffset = 0x0065U;
constexpr std::size_t kNormalActorCollisionProbeVerticalLowOffset = 0x0066U;
constexpr std::size_t kNormalActorCollisionProbeVerticalHighOffset = 0x0067U;

} // namespace

GameStateView::GameStateView(
    const std::span<const std::byte> work_ram) noexcept
    : work_ram_(work_ram)
{
}

bool GameStateView::valid() const noexcept
{
    return work_ram_.size() == kWorkRamSize;
}

std::optional<GameStateView::DynamicActorReservationKind>
GameStateView::dynamic_actor_reservation_kind() const noexcept
{
    constexpr std::size_t kDynamicActorReservationModeOffset = 0x0055U;
    const auto mode = byte_at_offset(kDynamicActorReservationModeOffset);
    if (!mode) {
        return std::nullopt;
    }
    return *mode == 0U ? DynamicActorReservationKind::standard
                       : DynamicActorReservationKind::alternate;
}

std::optional<GameStateView::ActorSlot> GameStateView::actor(
    const std::uint8_t slot) const noexcept
{
    if (!valid() || slot >= kActorSlotCount) {
        return std::nullopt;
    }

    const auto activity = byte_at_offset(kActorActivityOffset + slot);
    const auto attributes = byte_at_offset(kActorAttributesOffset + slot);
    const auto collision = byte_at_offset(kActorCollisionOffset + slot);
    const auto collision_table_selector = byte_at_offset(
        kActorCollisionTableSelectorOffset + slot);
    const auto collision_table_pass = byte_at_offset(
        kActorCollisionTablePassOffset + slot);
    const auto collision_shape_directory_index = byte_at_offset(
        kActorCollisionShapeDirectoryIndexOffset + slot);
    const auto collision_shape_phase = byte_at_offset(
        kActorCollisionShapePhaseOffset + slot);
    const auto behavior_counter = byte_at_offset(
        kActorBehaviorCounterOffset + slot);
    const auto collision_reaction_countdown = byte_at_offset(
        kActorCollisionReactionCountdownOffset + slot);
    const auto collision_event = byte_at_offset(kActorCollisionEventOffset + slot);
    const auto collision_reaction_phase = byte_at_offset(
        kActorCollisionReactionPhaseOffset + slot);
    const auto collision_orientation = byte_at_offset(
        kActorCollisionOrientationOffset + slot);
    const auto collision_entry_cursor = byte_at_offset(
        kActorCollisionEntryCursorOffset + slot);
    const auto collision_motion_cursor = byte_at_offset(
        kActorCollisionMotionCursorOffset + slot);
    const auto handler_low = byte_at_offset(kActorHandlerLowOffset + slot);
    const auto handler_high = byte_at_offset(kActorHandlerHighOffset + slot);
    const auto horizontal_motion = fixed_8_8(
        kActorHorizontalMotionLowOffset + slot,
        kActorHorizontalMotionHighOffset + slot);
    const auto vertical_motion = fixed_8_8(
        kActorVerticalMotionLowOffset + slot,
        kActorVerticalMotionHighOffset + slot);
    const auto position_x = unsigned_word(
        kActorPositionXLowOffset + slot,
        kActorPositionXHighOffset + slot);
    const auto position_y = unsigned_word(
        kActorPositionYLowOffset + slot,
        kActorPositionYHighOffset + slot);
    if (!activity || !attributes || !collision || !collision_table_selector
        || !collision_table_pass || !collision_shape_directory_index
        || !collision_shape_phase || !behavior_counter || !collision_reaction_countdown
        || !collision_event || !collision_reaction_phase || !collision_orientation
        || !collision_entry_cursor || !collision_motion_cursor || !handler_low
        || !handler_high || !horizontal_motion || !vertical_motion || !position_x
        || !position_y) {
        return std::nullopt;
    }

    return ActorSlot{
        .index = slot,
        .activity_flags = *activity,
        .attributes = *attributes,
        .collision_state = *collision,
        .behavior_counter = *behavior_counter,
        .handler_address = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(*handler_low)
            | (static_cast<std::uint16_t>(*handler_high) << 8U)),
        .horizontal_motion = *horizontal_motion,
        .vertical_motion = *vertical_motion,
        .collision_table_program = {.selector = *collision_table_selector},
        .collision_shape_directory_index = *collision_shape_directory_index,
        .collision_shape_phase = *collision_shape_phase,
        .collision_table = {
            .pass_state = *collision_table_pass,
            .collision_event = *collision_event,
            .orientation_flags = *collision_orientation,
            .collision_entry_cursor = *collision_entry_cursor,
            .record_cursor = *collision_motion_cursor,
            .position_x = *position_x,
            .position_y = *position_y,
        },
        .collision_reaction = {
            .collision_event = *collision_event,
            .phase = *collision_reaction_phase,
            .ticks_until_next_phase = *collision_reaction_countdown,
        },
    };
}

std::optional<GameStateView::ActorFrameGate>
GameStateView::short_actor_frame_gate() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto freeze_latch = byte_at_offset(kActorPassFreezeLatchOffset);
    const auto enabled_actor_mask = byte_at_offset(kActorEnableMaskOffset);
    if (!freeze_latch || !enabled_actor_mask) {
        return std::nullopt;
    }

    // C539 retains the caller-selected mask when unfrozen. Its only mask
    // mutation is C53E's clear after the nonzero $2D35 gate.
    return ActorFrameGate{
        .frozen = *freeze_latch != 0U,
        .enabled_actor_mask = *freeze_latch != 0U ? 0U : *enabled_actor_mask,
    };
}

std::optional<GameStateView::ActorPresentationQueueImage>
GameStateView::actor_presentation_queue_image() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto auxiliary_count = byte_at_offset(
        kActorPresentationAuxiliaryCountOffset);
    const auto primary_low_count = byte_at_offset(
        kActorPresentationPrimaryLowCountOffset);
    const auto primary_high_next_index = byte_at_offset(
        kActorPresentationPrimaryHighNextIndexOffset);
    if (!auxiliary_count || !primary_low_count || !primary_high_next_index) {
        return std::nullopt;
    }

    ActorPresentationQueueImage image{
        .auxiliary_count = *auxiliary_count,
        .primary_low_count = *primary_low_count,
        .primary_high_next_index = *primary_high_next_index,
    };
    for (std::size_t index = 0U; index < kActorSlotCount; ++index) {
        const auto auxiliary = byte_at_offset(
            kActorPresentationAuxiliaryQueueOffset + index);
        const auto primary = byte_at_offset(
            kActorPresentationPrimaryQueueOffset + index);
        if (!auxiliary || !primary) {
            return std::nullopt;
        }
        image.auxiliary_slots[index] = *auxiliary;
        image.primary_slots[index] = *primary;
    }
    return image;
}

std::optional<GameStateView::BonusStage4ObjectiveSlot>
GameStateView::bonus_stage_4_objective(const std::uint8_t index) const noexcept
{
    if (!valid() || index >= 3U) {
        return std::nullopt;
    }
    constexpr std::array<std::uint8_t, 3U> kSlots{0x34U, 0x52U, 0x56U};
    const auto actor = this->actor(kSlots[index]);
    const auto active = byte_at_offset(kBonusStage4ObjectiveFlagsOffset + index);
    if (!actor || !active) {
        return std::nullopt;
    }
    return BonusStage4ObjectiveSlot{
        .index = index,
        .active = *active != 0U,
        .hits_remaining = actor->behavior_counter,
    };
}

std::optional<GameStateView::BonusStage4LinkedTargetSlot>
GameStateView::bonus_stage_4_linked_target(const std::uint8_t slot) const noexcept
{
    const auto actor = this->actor(slot);
    const auto ticks_until_motion_refresh = byte_at_offset(
        kBonusStage4LinkedTargetMotionRefreshOffset + slot);
    if (!actor || !ticks_until_motion_refresh) {
        return std::nullopt;
    }
    return BonusStage4LinkedTargetSlot{
        .index = slot,
        .source_slot = actor->collision_table.record_cursor,
        .objective_index = actor->collision_table.collision_entry_cursor,
        .ticks_until_motion_refresh = *ticks_until_motion_refresh,
    };
}

std::optional<GameStateView::BumperSlot> GameStateView::bumper(
    const std::uint8_t slot) const noexcept
{
    constexpr std::uint8_t kLeftBumperSlot = 0x14U;
    constexpr std::uint8_t kRightBumperSlot = 0x15U;
    if (!valid()
        || (slot != kLeftBumperSlot && slot != kRightBumperSlot)) {
        return std::nullopt;
    }

    const auto side = byte_at_offset(kActorCollisionEntryCursorOffset + slot);
    const auto hit_phase = byte_at_offset(kActorCollisionMotionCursorOffset + slot);
    const auto primary_appearance = byte_at_offset(
        kActorBumperPrimaryAppearanceOffset + slot);
    const auto secondary_damage = byte_at_offset(
        kActorBumperSecondaryDamageOffset + slot);
    if (!side || !hit_phase || !primary_appearance || !secondary_damage) {
        return std::nullopt;
    }

    return BumperSlot{
        .index = slot,
        .side = *side,
        .hit_phase = *hit_phase,
        .primary_appearance = *primary_appearance,
        .secondary_damage = *secondary_damage,
    };
}

std::optional<GameStateView::TowerSlot> GameStateView::tower(
    const std::uint8_t slot) const noexcept
{
    constexpr std::uint8_t kTowerSlot = 0x2DU;
    if (!valid() || slot != kTowerSlot) {
        return std::nullopt;
    }

    const auto hit_progress = byte_at_offset(kTowerHitProgressOffset);
    const auto remaining_armed_passes = byte_at_offset(
        kTowerRemainingArmedPassesOffset);
    if (!hit_progress || !remaining_armed_passes) {
        return std::nullopt;
    }
    return TowerSlot{
        .index = slot,
        .hit_progress = *hit_progress,
        .remaining_armed_passes = *remaining_armed_passes,
    };
}

std::optional<GameStateView::TowerInsectSourceSlot>
GameStateView::tower_insect_source(const std::uint8_t slot) const noexcept
{
    constexpr std::uint8_t kTowerInsectSourceSlot = 0x2CU;
    if (!valid() || slot != kTowerInsectSourceSlot) {
        return std::nullopt;
    }

    const auto live_children = byte_at_offset(kTowerInsectLiveChildrenOffset);
    const auto spawns_remaining = byte_at_offset(kActorSpawnBudgetOffset + slot);
    const auto ticks_until_next_spawn = byte_at_offset(
        kActorCollisionReactionCountdownOffset + slot);
    if (!live_children || !spawns_remaining || !ticks_until_next_spawn) {
        return std::nullopt;
    }
    return TowerInsectSourceSlot{
        .index = slot,
        .live_children = *live_children,
        .spawns_remaining = *spawns_remaining,
        .ticks_until_next_spawn = *ticks_until_next_spawn,
    };
}

std::optional<GameStateView::SmallSoldierSourceSlot>
GameStateView::small_soldier_source(const std::uint8_t slot) const noexcept
{
    constexpr std::uint8_t kLowerLeftSourceSlot = 0x34U;
    constexpr std::uint8_t kUpperRightSourceSlot = 0x35U;
    if (!valid() || (slot != kLowerLeftSourceSlot && slot != kUpperRightSourceSlot)) {
        return std::nullopt;
    }

    const auto live_children = byte_at_offset(
        slot == kLowerLeftSourceSlot ? kLeftSmallSoldierLiveChildrenOffset
                                     : kRightSmallSoldierLiveChildrenOffset);
    const auto pending_spawns = byte_at_offset(kActorSpawnBudgetOffset + slot);
    const auto ticks_until_next_spawn = byte_at_offset(
        kActorCollisionReactionCountdownOffset + slot);
    const auto appearance = byte_at_offset(kActorCollisionShapePhaseOffset + slot);
    if (!live_children || !pending_spawns || !ticks_until_next_spawn || !appearance) {
        return std::nullopt;
    }
    return SmallSoldierSourceSlot{
        .index = slot,
        .live_children = *live_children,
        .pending_spawns = *pending_spawns,
        .ticks_until_next_spawn = *ticks_until_next_spawn,
        .appearance = *appearance,
    };
}

std::optional<GameStateView::CollisionEffectSlot>
GameStateView::collision_effect(const std::uint8_t slot) const noexcept
{
    if (!valid() || slot >= kActorSlotCount) {
        return std::nullopt;
    }
    const auto source_slot = byte_at_offset(kActorCollisionEntryCursorOffset + slot);
    const auto source_association = byte_at_offset(
        kActorCollisionMotionCursorOffset + slot);
    const auto ticks_remaining = byte_at_offset(
        kActorCollisionEffectLifetimeOffset + slot);
    if (!source_slot || !source_association || !ticks_remaining) {
        return std::nullopt;
    }
    return CollisionEffectSlot{
        .index = slot,
        .source_slot = *source_slot,
        .source_association = *source_association,
        .ticks_remaining = *ticks_remaining,
    };
}

std::optional<GameStateView::GiantSkeletonEyeballFinalAnimationSlot>
GameStateView::giant_skeleton_eyeball_final_animation(
    const std::uint8_t slot) const noexcept
{
    constexpr std::uint8_t kGiantSkeletonEyeballSlot = 0x2FU;
    if (!valid() || slot != kGiantSkeletonEyeballSlot) {
        return std::nullopt;
    }
    const auto appearance_ticks = byte_at_offset(
        kActorCollisionReactionCountdownOffset + slot);
    const auto wait_low = byte_at_offset(kActorEyeballFinalWaitLowOffset + slot);
    const auto wait_high = byte_at_offset(kActorEyeballFinalWaitHighOffset + slot);
    const auto appearance = byte_at_offset(kActorCollisionShapePhaseOffset + slot);
    if (!appearance_ticks || !wait_low || !wait_high || !appearance) {
        return std::nullopt;
    }
    return GiantSkeletonEyeballFinalAnimationSlot{
        .index = slot,
        .appearance_ticks_remaining = *appearance_ticks,
        .ticks_remaining = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(*wait_low)
            | (static_cast<std::uint16_t>(*wait_high) << 8U)),
        .appearance = *appearance,
    };
}

std::optional<GameStateView::DragonEggClusterSlot>
GameStateView::dragon_egg_cluster() const noexcept
{
    const auto live_eggs = byte_at_offset(kDragonEggLiveCountOffset);
    const auto reset_flag = byte_at_offset(kDragonEggResetFlagOffset);
    const auto completion_latch = byte_at_offset(kDragonEggCompletionLatchOffset);
    if (!live_eggs || !reset_flag || !completion_latch) {
        return std::nullopt;
    }
    return DragonEggClusterSlot{
        .live_eggs = *live_eggs,
        .reset_requested = (*reset_flag & 0x01U) != 0U,
        .completion_latched = (*completion_latch & 0x01U) != 0U,
    };
}

std::optional<GameStateView::GiantFaceSlot> GameStateView::giant_face() const noexcept
{
    const auto persisted_form = byte_at_offset(kGiantFacePersistedFormOffset);
    const auto request_flags = byte_at_offset(kGiantFaceRequestFlagsOffset);
    const auto display_selector = byte_at_offset(kGiantFaceDisplaySelectorOffset);
    const auto animation_mode = byte_at_offset(kGiantFaceAnimationModeOffset);
    const auto mouth_flags = byte_at_offset(kGiantFaceMouthFlagsOffset);
    if (!persisted_form || !request_flags || !display_selector || !animation_mode
        || !mouth_flags) {
        return std::nullopt;
    }

    return GiantFaceSlot{
        .persisted_form = *persisted_form,
        .wake_requested = (*request_flags & 0x01U) != 0U,
        .transformation_requested = (*request_flags & 0x02U) != 0U,
        .display_selector = *display_selector,
        .animation_mode = *animation_mode,
        .mouth_open = (*mouth_flags & 0x01U) != 0U,
        .bonus_stage_4_requested = (*mouth_flags & 0x80U) != 0U,
    };
}

std::optional<GameStateView::GiantFaceHelmetChannelSlot>
GameStateView::giant_face_helmet_channel(const std::uint8_t slot) const noexcept
{
    constexpr std::uint8_t kLeftHelmetSlot = 0x2AU;
    constexpr std::uint8_t kRightHelmetSlot = 0x2BU;
    if (!valid() || (slot != kLeftHelmetSlot && slot != kRightHelmetSlot)) {
        return std::nullopt;
    }

    const auto selector = byte_at_offset(kActorCollisionReactionPhaseOffset + slot);
    const auto ticks_until_appearance = byte_at_offset(
        kActorCollisionReactionCountdownOffset + slot);
    const auto appearance = byte_at_offset(kActorCollisionShapePhaseOffset + slot);
    if (!selector || !ticks_until_appearance || !appearance) {
        return std::nullopt;
    }

    return GiantFaceHelmetChannelSlot{
        .index = slot,
        .selector = *selector,
        .ticks_until_appearance = *ticks_until_appearance,
        .appearance = *appearance,
    };
}

std::optional<GameStateView::BallSlot> GameStateView::ball() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto activity = byte_at_offset(kActorActivityOffset + kBallSlot);
    const auto position_x = fixed_16_8(
        kBallPositionXFractionalOffset,
        kBallPositionXLowOffset,
        kBallPositionXHighOffset);
    const auto position_y = fixed_16_8(
        kBallPositionYFractionalOffset,
        kBallPositionYLowOffset,
        kBallPositionYHighOffset);
    const auto horizontal_motion = fixed_8_8(
        kActorHorizontalMotionLowOffset + kBallSlot,
        kActorHorizontalMotionHighOffset + kBallSlot);
    const auto vertical_motion = fixed_8_8(
        kActorVerticalMotionLowOffset + kBallSlot,
        kActorVerticalMotionHighOffset + kBallSlot);
    const auto motion_flags = byte_at_offset(kBallMotionFlagsOffset + kBallSlot);
    const auto collision_state = byte_at_offset(
        kActorCollisionOffset + kBallSlot);
    if (!activity || !position_x || !position_y || !horizontal_motion
        || !vertical_motion || !motion_flags || !collision_state) {
        return std::nullopt;
    }

    return BallSlot{
        .activity_flags = *activity,
        .position_x = *position_x,
        .position_y = *position_y,
        .horizontal_motion = *horizontal_motion,
        .vertical_motion = *vertical_motion,
        .motion_flags = *motion_flags,
        .collision_state = *collision_state,
    };
}

std::optional<BallMotionIncrement> GameStateView::ball_substep_motion()
    const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }
    const auto horizontal = fixed_8_8(
        kBallSubstepHorizontalLowOffset,
        kBallSubstepHorizontalHighOffset);
    const auto vertical = fixed_8_8(
        kBallSubstepVerticalLowOffset,
        kBallSubstepVerticalHighOffset);
    if (!horizontal || !vertical) {
        return std::nullopt;
    }
    return BallMotionIncrement{
        .horizontal = horizontal->raw,
        .vertical = vertical->raw,
    };
}

std::optional<BallSubstepProgress> GameStateView::ball_substep_progress()
    const noexcept
{
    const auto increment = ball_substep_motion();
    const auto completed = byte_at_offset(kBallCompletedSubstepsOffset);
    const auto requested = byte_at_offset(kBallRequestedSubstepsOffset);
    if (!increment || !completed || !requested) {
        return std::nullopt;
    }
    if ((*requested != 1U && *requested != 2U && *requested != 4U)
        || *completed > *requested) {
        return std::nullopt;
    }
    return BallSubstepProgress{
        .increment = *increment,
        .completed_steps = *completed,
        .requested_steps = *requested,
    };
}

std::optional<BallMotionTuning> GameStateView::ball_motion_tuning() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto gravity = byte_at_offset(kBallVerticalGravityOffset);
    const auto horizontal_drag = byte_at_offset(kBallHorizontalDragDivisorOffset);
    const auto vertical_drag = byte_at_offset(kBallVerticalDragDivisorOffset);
    if (!gravity || !horizontal_drag || !vertical_drag) {
        return std::nullopt;
    }

    return BallMotionTuning{
        .horizontal_drag_divisor = static_cast<std::int8_t>(*horizontal_drag),
        .vertical_drag_divisor = static_cast<std::int8_t>(*vertical_drag),
        .vertical_gravity = *gravity,
    };
}

std::optional<GameStateView::BallCollisionPositionCache>
GameStateView::ball_collision_position_cache() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }
    const auto position_x = fixed_16_8(
        kBallCollisionCacheXFractionalOffset,
        kBallCollisionCacheXLowOffset,
        kBallCollisionCacheXHighOffset);
    const auto position_y = fixed_16_8(
        kBallCollisionCacheYFractionalOffset,
        kBallCollisionCacheYLowOffset,
        kBallCollisionCacheYHighOffset);
    if (!position_x || !position_y) {
        return std::nullopt;
    }
    return BallCollisionPositionCache{
        .position_x = *position_x,
        .position_y = *position_y,
    };
}

std::optional<GameStateView::CollisionResponseTableState>
GameStateView::collision_response_table_state() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto force_reduced = byte_at_offset(kCollisionResponseForceReducedOffset);
    const auto quarter_scale = byte_at_offset(kCollisionResponseQuarterScaleOffset);
    const auto low_tuning = byte_at_offset(kCollisionResponseLowTuningOffset);
    const auto high_tuning = byte_at_offset(kCollisionResponseHighTuningOffset);
    if (!force_reduced || !quarter_scale || !low_tuning || !high_tuning) {
        return std::nullopt;
    }
    return CollisionResponseTableState{
        .force_reduced_mode = *force_reduced != 0U,
        .use_quarter_reduced_scale = *quarter_scale != 0U,
        .normal_tuning = {
            .negative_low_byte_decrement = *low_tuning,
            .positive_low_byte_increment = *low_tuning,
            .positive_high_byte_increment = *high_tuning,
        },
    };
}

std::optional<GameStateView::CollisionResponseInput>
GameStateView::collision_response_input() const noexcept
{
    const auto orientation = byte_at_offset(kCollisionResponseOrientationOffset);
    const auto table_state = collision_response_table_state();
    if (!orientation || !table_state) {
        return std::nullopt;
    }
    return CollisionResponseInput{
        .orientation = *orientation,
        .table_state = *table_state,
    };
}

std::optional<GameStateView::ActiveScore> GameStateView::active_score() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto slot = byte_at_offset(kActiveScoreSlotOffset);
    if (!slot || *slot >= kScoreSlotCount) {
        return std::nullopt;
    }
    const auto low = byte_at_offset(kScoreLowBaseOffset + *slot);
    const auto middle = byte_at_offset(kScoreMiddleBaseOffset + *slot);
    const auto high = byte_at_offset(kScoreHighBaseOffset + *slot);
    const auto display_flags = byte_at_offset(kScoreDisplayDirtyOffset);
    if (!low || !middle || !high || !display_flags) {
        return std::nullopt;
    }

    return ActiveScore{
        .slot = *slot,
        .stored_units = static_cast<std::uint32_t>(*low)
            | (static_cast<std::uint32_t>(*middle) << 8U)
            | (static_cast<std::uint32_t>(*high) << 16U),
        .display_dirty = (*display_flags & 0x01U) != 0U,
    };
}

std::optional<GameStateView::TableRuleLatchSnapshot>
GameStateView::table_rule_latches() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto left_front_spider_safety = byte_at_offset(
        kLeftFrontSpiderSafetyOffset);
    const auto right_front_spider_safety = byte_at_offset(
        kRightFrontSpiderSafetyOffset);
    const auto tower_completed = byte_at_offset(kTowerCompletedOffset);
    const auto side_spider_transition_requested = byte_at_offset(
        kSideSpiderTransitionRequestedOffset);
    if (!left_front_spider_safety || !right_front_spider_safety
        || !tower_completed || !side_spider_transition_requested) {
        return std::nullopt;
    }

    return TableRuleLatchSnapshot{
        .left_front_spider_safety = *left_front_spider_safety != 0U,
        .right_front_spider_safety = *right_front_spider_safety != 0U,
        .tower_completed = *tower_completed != 0U,
        .side_spider_transition_requested = *side_spider_transition_requested != 0U,
    };
}

std::optional<GameStateView::PlayfieldCollisionTablePointers>
GameStateView::playfield_collision_table_pointers() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    // The table probe keeps two parallel maps in $C8:$C9 and $C6:$C7,
    // respectively; $90:$91 points at the attribute-indexed mask plane.
    const auto tile_ids = little_endian_word(0x00C8U);
    const auto collision_attributes = little_endian_word(0x00C6U);
    const auto masks = little_endian_word(0x0090U);
    if (!tile_ids || !collision_attributes || !masks) {
        return std::nullopt;
    }
    return PlayfieldCollisionTablePointers{
        .tile_ids = *tile_ids,
        .collision_attributes = *collision_attributes,
        .masks = *masks,
    };
}

std::optional<GameStateView::ActorCollisionShapeDirectoryPointers>
GameStateView::actor_collision_shape_directory_pointers() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    ActorCollisionShapeDirectoryPointers pointers;
    for (std::size_t index = 0U; index < pointers.tables.size(); ++index) {
        const auto pointer = little_endian_word(0x0096U + index * 2U);
        if (!pointer) {
            return std::nullopt;
        }
        pointers.tables[index] = *pointer;
    }
    return pointers;
}

std::optional<GameStateView::ActorCollisionMaskTablePointers>
GameStateView::actor_collision_mask_table_pointers() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto first_table = little_endian_word(0x0074U);
    const auto second_table = little_endian_word(0x0072U);
    const auto masks = little_endian_word(0x008EU);
    if (!first_table || !second_table || !masks) {
        return std::nullopt;
    }
    return ActorCollisionMaskTablePointers{
        .first_table = *first_table,
        .second_table = *second_table,
        .masks = *masks,
    };
}

std::optional<GameStateView::ActorCollisionMaskLookupStage>
GameStateView::actor_collision_mask_lookup_stage() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto horizontal_cell = byte_at_offset(0x002DU);
    const auto vertical_offset = byte_at_offset(0x002FU);
    const auto mask_column = byte_at_offset(0x005EU);
    const auto mask_row = byte_at_offset(0x005FU);
    if (!horizontal_cell || !vertical_offset || !mask_column || !mask_row) {
        return std::nullopt;
    }
    return ActorCollisionMaskLookupStage{
        .horizontal_cell = *horizontal_cell,
        .vertical_offset = *vertical_offset,
        .mask_column = *mask_column,
        .mask_row = *mask_row,
    };
}

std::optional<GameStateView::ActorCollisionScanSession>
GameStateView::actor_collision_scan_session() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto active = byte_at_offset(kActorCollisionScanSessionActiveOffset);
    const auto side_flags = byte_at_offset(kActorCollisionSideFlagsOffset);
    if (!active || !side_flags) {
        return std::nullopt;
    }
    return ActorCollisionScanSession{
        .active = *active != 0U,
        .side_flags = *side_flags,
    };
}

std::optional<GameStateView::NormalActorCollisionScanWalk>
GameStateView::normal_actor_collision_scan_walk() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto gate = byte_at_offset(kNormalActorCollisionScanGateOffset);
    const auto slot_count = byte_at_offset(kNormalActorCollisionScanSlotCountOffset);
    const auto first_slot = byte_at_offset(kNormalActorCollisionScanFirstSlotOffset);
    if (!gate || !slot_count || !first_slot) {
        return std::nullopt;
    }
    return NormalActorCollisionScanWalk{
        .first_slot = *first_slot,
        .slot_count = *slot_count,
        .require_activity_bit_6 = *gate != 0U,
    };
}

std::optional<CollisionProbe>
GameStateView::normal_actor_collision_setup_probe() const noexcept
{
    const auto current_ball = ball();
    if (!current_ball) {
        return std::nullopt;
    }
    return CollisionProbe{
        .horizontal = static_cast<std::uint8_t>(
            static_cast<std::uint32_t>(current_ball->position_x.raw >> 8U)
            - 0x20U),
        .vertical = static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(current_ball->position_y.raw >> 8U)),
    };
}

std::optional<CollisionProbe>
GameStateView::normal_actor_collision_probe() const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto horizontal = byte_at_offset(kNormalActorCollisionProbeHorizontalOffset);
    const auto vertical = little_endian_word(
        kNormalActorCollisionProbeVerticalLowOffset);
    if (!horizontal || !vertical) {
        return std::nullopt;
    }
    return CollisionProbe{
        .horizontal = *horizontal,
        .vertical = *vertical,
    };
}

std::optional<std::uint8_t> GameStateView::byte_at_offset(
    const std::size_t offset) const noexcept
{
    if (offset >= work_ram_.size()) {
        return std::nullopt;
    }
    return std::to_integer<std::uint8_t>(work_ram_[offset]);
}

std::optional<GameStateView::Fixed8_8> GameStateView::fixed_8_8(
    const std::size_t low_offset,
    const std::size_t high_offset) const noexcept
{
    const auto low = byte_at_offset(low_offset);
    const auto high = byte_at_offset(high_offset);
    if (!low || !high) {
        return std::nullopt;
    }

    const auto bits = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(*low)
        | (static_cast<std::uint16_t>(*high) << 8U));
    return Fixed8_8{.raw = std::bit_cast<std::int16_t>(bits)};
}

std::optional<GameStateView::Fixed16_8> GameStateView::fixed_16_8(
    const std::size_t fractional_offset,
    const std::size_t low_offset,
    const std::size_t high_offset) const noexcept
{
    const auto fractional = byte_at_offset(fractional_offset);
    const auto low = byte_at_offset(low_offset);
    const auto high = byte_at_offset(high_offset);
    if (!fractional || !low || !high) {
        return std::nullopt;
    }

    const auto bits = static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(*fractional)
        | (static_cast<std::uint32_t>(*low) << 8U)
        | (static_cast<std::uint32_t>(*high) << 16U));
    return Fixed16_8{.raw = (bits & 0x00800000U) != 0U
        ? static_cast<std::int32_t>(bits | 0xFF000000U)
        : static_cast<std::int32_t>(bits)};
}

std::optional<std::uint16_t> GameStateView::unsigned_word(
    const std::size_t low_offset,
    const std::size_t high_offset) const noexcept
{
    const auto low = byte_at_offset(low_offset);
    const auto high = byte_at_offset(high_offset);
    if (!low || !high) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(*low)
        | (static_cast<std::uint16_t>(*high) << 8U));
}

std::optional<std::uint16_t> GameStateView::little_endian_word(
    const std::size_t low_offset) const noexcept
{
    const auto low = byte_at_offset(low_offset);
    const auto high = byte_at_offset(low_offset + 1U);
    if (!low || !high) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(*low)
        | (static_cast<std::uint16_t>(*high) << 8U));
}

} // namespace devils_crush::native_runtime
