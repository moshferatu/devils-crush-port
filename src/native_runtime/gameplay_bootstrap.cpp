// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/gameplay_bootstrap.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] std::optional<GiantFaceCapture> capture_giant_face_bootstrap(
    const GameStateView& state) noexcept
{
    if (const auto dormant = capture_dormant_giant_face(state)) {
        return dormant;
    }
    if (const auto lower = capture_awake_lower_giant_face(state)) {
        return lower;
    }
    return capture_awake_upper_giant_face(state);
}

[[nodiscard]] std::optional<DragonEggBootstrap> capture_dragon_egg_bootstrap(
    const GameStateView& state) noexcept
{
    if (const auto cluster = capture_dragon_egg_cluster(state)) {
        return DragonEggBootstrap{.cluster = *cluster};
    }
    if (const auto cluster = capture_partial_dragon_egg_cluster(state)) {
        return DragonEggBootstrap{.cluster = *cluster};
    }
    if (const auto hatchling = capture_dragon_egg_cluster_with_hatchling(state)) {
        return DragonEggBootstrap{
            .cluster = hatchling->cluster,
            .hatchling = hatchling->hatchling,
        };
    }
    if (const auto cluster = capture_completed_dragon_egg_cluster(state)) {
        return DragonEggBootstrap{.cluster = *cluster};
    }
    if (const auto cluster = capture_dragon_egg_reset_cluster(state)) {
        return DragonEggBootstrap{.cluster = *cluster};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<GiantSkeletonBootstrap>
capture_giant_skeleton_bootstrap(const GameStateView& state) noexcept
{
    GiantSkeletonBootstrap bootstrap{
        .skeleton = capture_giant_skeleton_actor(state, kGiantSkeletonSlot),
    };
    if (const auto opening = capture_giant_skeleton_eyeball_actor(
            state, kGiantSkeletonEyeballSlot)) {
        bootstrap.eyeball = *opening;
    } else if (const auto final = capture_giant_skeleton_eyeball_final_animation_actor(
                   state, kGiantSkeletonEyeballSlot)) {
        bootstrap.eyeball = *final;
    }
    if (!bootstrap.skeleton && !bootstrap.eyeball) {
        return std::nullopt;
    }
    return bootstrap;
}

[[nodiscard]] std::optional<TowerInsectBootstrap>
capture_tower_insect_bootstrap(const GameStateView& state) noexcept
{
    const auto generation = capture_tower_insect_generation(state);
    if (!generation) {
        return std::nullopt;
    }

    TowerInsectBootstrap bootstrap{.generation = *generation};
    for (std::size_t effect_slot = 0U;
         effect_slot < GameStateView::kActorSlotCount;
         ++effect_slot) {
        const auto effect = capture_collision_effect(
            state, static_cast<std::uint8_t>(effect_slot));
        if (!effect) {
            continue;
        }
        for (std::size_t index = 0U;
             index < kTowerInsectObservedSlots.size();
             ++index) {
            if (effect->effect.source_slot != kTowerInsectObservedSlots[index]) {
                continue;
            }
            if (!generation->generation.insects[index]
                || bootstrap.collision_effects[index]) {
                return std::nullopt;
            }
            bootstrap.collision_effects[index] = *effect;
        }
    }

    for (std::size_t index = 0U;
         index < generation->generation.insects.size();
         ++index) {
        const auto& insect = generation->generation.insects[index];
        if (insect && (insect->collision_effect_association & 0x0FU) == 0x0FU
            && !bootstrap.collision_effects[index]) {
            return std::nullopt;
        }
    }
    return bootstrap;
}

[[nodiscard]] SmallSoldierBootstrap
capture_small_soldier_bootstrap(const GameStateView& state) noexcept
{
    SmallSoldierBootstrap bootstrap{};
    for (std::uint8_t slot = ActorState::kFirstDynamicSlot;
         slot < ActorState::kPastLastDynamicSlot;
         ++slot) {
        bootstrap.children[slot - ActorState::kFirstDynamicSlot]
            = capture_small_soldier_collision_actor(state, slot);
    }

    for (std::size_t effect_slot = 0U;
         effect_slot < GameStateView::kActorSlotCount;
         ++effect_slot) {
        const auto effect = capture_collision_effect(
            state, static_cast<std::uint8_t>(effect_slot));
        if (!effect) {
            continue;
        }
        const auto source_slot = effect->effect.source_slot;
        if (source_slot < ActorState::kFirstDynamicSlot
            || source_slot >= ActorState::kPastLastDynamicSlot) {
            continue;
        }
        const auto child_index = source_slot - ActorState::kFirstDynamicSlot;
        if (!bootstrap.children[child_index]) {
            continue;
        }
        if (bootstrap.collision_effects[child_index]) {
            // One source can have only one live shared effect. A duplicate
            // frame cannot be restored coherently, so reject this complete
            // Small-Soldier capture rather than choosing an arbitrary child.
            return SmallSoldierBootstrap{};
        }
        bootstrap.collision_effects[child_index] = *effect;
    }

    for (const auto side : {SmallSoldierGroupSide::lower_left,
             SmallSoldierGroupSide::upper_right}) {
        const auto index = static_cast<std::size_t>(side);
        const auto source = capture_small_soldier_source(state, side);
        if (!source) {
            continue;
        }
        std::uint8_t captured_live_children = 0U;
        for (const auto& child : bootstrap.children) {
            if (child && child->rule_state.group_side == side) {
                ++captured_live_children;
            }
        }
        if (captured_live_children == source->source_state.group.live_children) {
            bootstrap.sources[index] = *source;
        }
    }
    return bootstrap;
}

} // namespace

std::optional<NativeGameplayBootstrap> capture_native_gameplay_bootstrap(
    const GameStateView& state) noexcept
{
    const auto ball = capture_active_ball(state);
    const auto score = capture_active_score(state);
    const auto table_rule_latches = state.table_rule_latches();
    if (!ball || !score || !table_rule_latches) {
        return std::nullopt;
    }
    const auto tower = capture_tower_actor(state, kTowerSlot);

    return NativeGameplayBootstrap{
        .ball = *ball,
        .ball_frame_schedule = capture_ball_frame_schedule(state),
        .score = *score,
        .table_rule_latches = *table_rule_latches,
        .giant_face = capture_giant_face_bootstrap(state),
        .bonus_stage_4 = capture_bonus_stage_4(state),
        .bonus_stage_4_linked_target = capture_bonus_stage_4_linked_target_generation(state),
        .giant_face_helmet = capture_giant_face_helmet(state),
        .dragon_eggs = capture_dragon_egg_bootstrap(state),
        .dragon_head = capture_dragon_head_actor(state, kDragonHeadSlot),
        .giant_skeleton = capture_giant_skeleton_bootstrap(state),
        .tower = tower,
        .tower_guard = tower
            ? capture_tower_guard_actor(state, kTowerGuardSlot)
            : std::nullopt,
        .front_spiders = {
            capture_front_spider_actor(state, kLeftFrontSpiderSlot),
            capture_front_spider_actor(state, kRightFrontSpiderSlot),
        },
        .side_spiders = {
            capture_side_spider_actor(state, kLeftSideSpiderSlot),
            capture_side_spider_actor(state, kRightSideSpiderSlot),
        },
        .skeleton_insects = {
            capture_skeleton_insect_actor(state, kLeftSkeletonInsectSlot),
            capture_skeleton_insect_actor(state, kCenterSkeletonInsectSlot),
            capture_skeleton_insect_actor(state, kRightSkeletonInsectSlot),
        },
        .tower_insects = capture_tower_insect_bootstrap(state),
        .small_soldiers = capture_small_soldier_bootstrap(state),
        .bumpers = {
            capture_bumper_actor(state, kLeftBumperSlot),
            capture_bumper_actor(state, kRightBumperSlot),
        },
    };
}

bool apply_native_gameplay_bootstrap(
    NativeGameplayState& gameplay,
    const NativeGameplayBootstrap& bootstrap) noexcept
{
    gameplay.seed_ball(bootstrap.ball.state);
    gameplay.seed_score(bootstrap.score.state);
    gameplay.seed_table_rule_latches(bootstrap.table_rule_latches);
    if (bootstrap.giant_face) {
        gameplay.seed_giant_face(
            bootstrap.giant_face->rule_state,
            bootstrap.giant_face->controller_state,
            bootstrap.giant_face->animator_state);
    }
    if (bootstrap.bonus_stage_4) {
        gameplay.seed_bonus_stage_4(bootstrap.bonus_stage_4->state);
    }
    if (bootstrap.bonus_stage_4_linked_target
        && !gameplay.seed_bonus_stage_4_linked_target(
            bootstrap.bonus_stage_4_linked_target->target.state,
            {bootstrap.bonus_stage_4_linked_target->children[0U]
                    ? std::optional<BonusStage4OrbitingChildState>{
                        bootstrap.bonus_stage_4_linked_target->children[0U]->state}
                    : std::nullopt,
                bootstrap.bonus_stage_4_linked_target->children[1U]
                    ? std::optional<BonusStage4OrbitingChildState>{
                        bootstrap.bonus_stage_4_linked_target->children[1U]->state}
                    : std::nullopt,
                bootstrap.bonus_stage_4_linked_target->children[2U]
                    ? std::optional<BonusStage4OrbitingChildState>{
                        bootstrap.bonus_stage_4_linked_target->children[2U]->state}
                    : std::nullopt,
                bootstrap.bonus_stage_4_linked_target->children[3U]
                    ? std::optional<BonusStage4OrbitingChildState>{
                        bootstrap.bonus_stage_4_linked_target->children[3U]->state}
                    : std::nullopt})) {
        return false;
    }
    if (bootstrap.giant_face_helmet) {
        gameplay.seed_giant_face_helmet(bootstrap.giant_face_helmet->rule_state);
    }
    if (bootstrap.dragon_eggs) {
        gameplay.seed_dragon_egg_cluster(bootstrap.dragon_eggs->cluster.rule_state);
        if (bootstrap.dragon_eggs->hatchling
            && !gameplay.seed_dragon_egg_hatchling(
                *bootstrap.dragon_eggs->hatchling)) {
            return false;
        }
    }
    if (bootstrap.dragon_head) {
        gameplay.seed_dragon_head(bootstrap.dragon_head->rule_state);
    }
    if (bootstrap.giant_skeleton) {
        if (bootstrap.giant_skeleton->skeleton) {
            gameplay.seed_giant_skeleton(
                bootstrap.giant_skeleton->skeleton->rule_state);
        }
        if (bootstrap.giant_skeleton->eyeball) {
            gameplay.seed_giant_skeleton_eyeball(
                bootstrap.giant_skeleton->eyeball->rule_state);
        }
    }
    if (bootstrap.tower) {
        gameplay.seed_tower(bootstrap.tower->rule_state);
    }
    if (bootstrap.tower && bootstrap.tower_guard) {
        gameplay.seed_tower_guard();
    }
    for (const auto& spider : bootstrap.front_spiders) {
        if (spider && !gameplay.seed_front_spider(spider->rule_state)) {
            return false;
        }
    }
    for (const auto& spider : bootstrap.side_spiders) {
        if (spider && !gameplay.seed_side_spider(spider->rule_state)) {
            return false;
        }
    }
    for (const auto& insect : bootstrap.skeleton_insects) {
        if (insect && !gameplay.seed_skeleton_insect(insect->rule_state)) {
            return false;
        }
    }
    if (bootstrap.tower_insects
        && !gameplay.seed_tower_insect_generation(
            bootstrap.tower_insects->generation.generation)) {
        return false;
    }
    if (bootstrap.tower_insects) {
        for (const auto& effect : bootstrap.tower_insects->collision_effects) {
            if (effect && !gameplay.seed_collision_effect(
                effect->effect_actor.index, effect->effect)) {
                return false;
            }
        }
    }
    for (const auto& source : bootstrap.small_soldiers.sources) {
        if (source && !gameplay.seed_small_soldier_source(source->source_state)) {
            return false;
        }
    }
    for (const auto& soldier : bootstrap.small_soldiers.children) {
        if (!soldier) {
            continue;
        }
        const auto source_index = static_cast<std::size_t>(
            soldier->rule_state.group_side);
        const auto seeded = bootstrap.small_soldiers.sources[source_index]
            ? gameplay.seed_captured_small_soldier_from_source(*soldier)
            : gameplay.seed_captured_small_soldier(*soldier);
        if (!seeded) {
            return false;
        }
    }
    for (const auto& effect : bootstrap.small_soldiers.collision_effects) {
        if (effect && !gameplay.seed_collision_effect(
            effect->effect_actor.index, effect->effect)) {
            return false;
        }
    }
    for (const auto& bumper : bootstrap.bumpers) {
        if (bumper && !gameplay.seed_bumper(bumper->rule_state)) {
            return false;
        }
    }
    return true;
}

} // namespace devils_crush::native_runtime
