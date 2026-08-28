// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/ball_capture.hpp"
#include "devils_crush/native_runtime/ball_frame_schedule.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_capture.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_linked_target_capture.hpp"
#include "devils_crush/native_runtime/bumper_capture.hpp"
#include "devils_crush/native_runtime/collision_effect_capture.hpp"
#include "devils_crush/native_runtime/dragon_egg_capture.hpp"
#include "devils_crush/native_runtime/dragon_head_capture.hpp"
#include "devils_crush/native_runtime/front_spider_capture.hpp"
#include "devils_crush/native_runtime/giant_face_capture.hpp"
#include "devils_crush/native_runtime/giant_face_helmet_capture.hpp"
#include "devils_crush/native_runtime/giant_skeleton_capture.hpp"
#include "devils_crush/native_runtime/giant_skeleton_eyeball_capture.hpp"
#include "devils_crush/native_runtime/native_gameplay_state.hpp"
#include "devils_crush/native_runtime/score_capture.hpp"
#include "devils_crush/native_runtime/side_spider_capture.hpp"
#include "devils_crush/native_runtime/skeleton_insect_capture.hpp"
#include "devils_crush/native_runtime/small_soldier_capture.hpp"
#include "devils_crush/native_runtime/tower_capture.hpp"
#include "devils_crush/native_runtime/tower_guard_capture.hpp"
#include "devils_crush/native_runtime/tower_insect_capture.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// The source may be at any one stable Dragon's-Egg lifecycle boundary. A
// hatchling is carried separately because it owns a dynamic actor record;
// all other progress is expressed by the cluster's named rule state.
struct DragonEggBootstrap {
    DragonEggClusterCapture cluster;
    std::optional<DragonEggHatchlingState> hatchling;

    [[nodiscard]] bool operator==(const DragonEggBootstrap&) const = default;
};

// The lower-right Skeleton and its Eyeball have independent fixed actor
// lifecycles. Either can be absent from a captured handoff: the Skeleton has
// a strict steady-record boundary, while the Eyeball is admitted only during
// one of its already-native waits.
struct GiantSkeletonBootstrap {
    std::optional<GiantSkeletonCapture> skeleton;
    std::optional<GiantSkeletonEyeballCapture> eyeball;

    [[nodiscard]] bool operator==(const GiantSkeletonBootstrap&) const = default;
};

// The Tower-Insect source and its allocator-selected children can carry a
// shared collision-effect child.  Keeping that linked child here prevents a
// source marked effect-active from being restored without the effect that will
// later clear its association.
struct TowerInsectBootstrap {
    TowerInsectGenerationCapture generation;
    std::array<std::optional<CollisionEffectCapture>, 4U> collision_effects;

    [[nodiscard]] bool operator==(const TowerInsectBootstrap&) const = default;
};

inline constexpr std::size_t kSmallSoldierBootstrapSlotCount =
    static_cast<std::size_t>(ActorState::kPastLastDynamicSlot)
    - static_cast<std::size_t>(ActorState::kFirstDynamicSlot);

// A Side-Star source is included only when every currently live child has a
// collision-ready capture. This preserves the source's already-accounted
// counters while reconnecting those children to future native completion.
struct SmallSoldierBootstrap {
    std::array<std::optional<SmallSoldierSourceCapture>, 2U> sources;
    std::array<std::optional<SmallSoldierActorCapture>,
        kSmallSoldierBootstrapSlotCount> children;
    // A shared collision effect is coupled to the child only when its own
    // strict capture identifies that child's exact dynamic source slot.
    std::array<std::optional<CollisionEffectCapture>,
        kSmallSoldierBootstrapSlotCount> collision_effects;

    [[nodiscard]] bool operator==(const SmallSoldierBootstrap&) const = default;
};

// A coherent, frame-boundary startup seed for the native gameplay domains
// that can be reconstructed without a CPU continuation. Giant Face capture
// admits its complete dormant state and settled lower/upper form loops; its
// collision-ready mirrored helmet openings are restored independently. Other
// in-flight family transitions remain unavailable until their owner is native.
struct NativeGameplayBootstrap {
    ActiveBallCapture ball;
    BallFrameSchedule ball_frame_schedule;
    ActiveScoreCapture score;
    TableRuleLatches table_rule_latches;
    std::optional<GiantFaceCapture> giant_face;
    std::optional<BonusStage4Capture> bonus_stage_4;
    // The linked target is dynamic and may coexist with the fixed BS-4
    // objectives. A steady target restores every clear child already orbiting
    // it, preserving the post-$6C24 linked generation.
    std::optional<BonusStage4LinkedTargetGenerationCapture> bonus_stage_4_linked_target;
    std::optional<GiantFaceHelmetCapture> giant_face_helmet;
    std::optional<DragonEggBootstrap> dragon_eggs;
    std::optional<DragonHeadCapture> dragon_head;
    std::optional<GiantSkeletonBootstrap> giant_skeleton;
    std::optional<TowerCapture> tower;
    // The Guard carries no independent rule state: its captured steady
    // dispatcher authorizes native actor-phase output derived from tower.
    std::optional<TowerGuardCapture> tower_guard;
    // As with the Bumpers, a Front Spider may be in an appearance transition
    // while its counterpart is already at the collision-ready steady owner.
    std::array<std::optional<FrontSpiderCapture>, 2U> front_spiders;
    // The outer Side Spiders use a distinct shared transition controller, so
    // their steady captures are retained independently from Front Spiders.
    std::array<std::optional<SideSpiderCapture>, 2U> side_spiders;
    // Skeleton Insects have independent fixed steady dispatchers. Their
    // source latches are already carried by table_rule_latches, so a clear
    // insect can be restored even while a sibling is in transition.
    std::array<std::optional<SkeletonInsectCapture>, 3U> skeleton_insects;
    // A Tower-Insect source and its allocator-selected children form one
    // coherent generation, including any linked collision-effect child.
    std::optional<TowerInsectBootstrap> tower_insects;
    // Collision-ready Small Soldiers remain independently available when a
    // Side-Star is transitioning. A stable source is linked only if its live
    // counter exactly matches the captured children for that side.
    SmallSoldierBootstrap small_soldiers;
    // Each fixed Bumper remains independently optional: a collision or
    // terminal-allocation transition on one side must not discard the other
    // side's already-valid native rule state.
    std::array<std::optional<BumperCapture>, 2U> bumpers;

    [[nodiscard]] bool operator==(const NativeGameplayBootstrap&) const = default;
};

// Captures the minimum game-wide state required to start a native table-frame
// owner. A missing live ball, score selection, or table latches rejects the
// whole seed. Actor families are added independently as their stable capture
// adapters become available.
[[nodiscard]] std::optional<NativeGameplayBootstrap>
capture_native_gameplay_bootstrap(const GameStateView& state) noexcept;

// Applies a previously validated bootstrap to a freshly constructed gameplay
// state. This is intentionally separate from capture so no half-read emulator
// state can mutate native gameplay ownership.
[[nodiscard]] bool apply_native_gameplay_bootstrap(
    NativeGameplayState& gameplay,
    const NativeGameplayBootstrap& bootstrap) noexcept;

} // namespace devils_crush::native_runtime
