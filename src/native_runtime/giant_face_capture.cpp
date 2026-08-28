// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_face_capture.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kControllerType = 0x83U;
constexpr std::uint8_t kAnimatorType = 0x83U;
constexpr std::uint8_t kSurfaceType = 0x82U;
constexpr std::uint16_t kControllerSteadyHandler = 0x74D8U;
constexpr std::uint16_t kAnimatorSteadyHandler = 0x767EU;
constexpr std::uint16_t kMouthSteadyHandler = 0x7726U;
constexpr std::uint16_t kCrownSteadyHandler = 0x76FAU;

[[nodiscard]] bool is_steady_actor(
    const GameStateView::ActorSlot& actor,
    const std::uint8_t slot,
    const std::uint8_t type,
    const std::uint16_t handler) noexcept
{
    return actor.index == slot && actor.activity_flags == type
        && actor.handler_address == handler && actor.collision_state == 0U;
}

} // namespace

std::optional<GiantFaceCapture> capture_dormant_giant_face(
    const GameStateView& state) noexcept
{
    const auto controller = state.actor(kGiantFaceControllerSlot);
    const auto animator = state.actor(kGiantFaceAnimatorSlot);
    const auto mouth = state.actor(kGiantFaceMouthSlot);
    const auto crown = state.actor(kGiantFaceCrownSlot);
    const auto face = state.giant_face();
    if (!controller || !animator || !mouth || !crown || !face
        || !is_steady_actor(*controller, kGiantFaceControllerSlot,
            kControllerType, kControllerSteadyHandler)
        || !is_steady_actor(*animator, kGiantFaceAnimatorSlot,
            kAnimatorType, kAnimatorSteadyHandler)
        || !is_steady_actor(*mouth, kGiantFaceMouthSlot,
            kSurfaceType, kMouthSteadyHandler)
        || !is_steady_actor(*crown, kGiantFaceCrownSlot,
            kSurfaceType, kCrownSteadyHandler)
        || face->persisted_form != 0U || face->wake_requested
        || face->transformation_requested || face->display_selector != 0U
        || face->animation_mode != 0U || face->mouth_open
        || face->bonus_stage_4_requested) {
        return std::nullopt;
    }

    return GiantFaceCapture{
        .actors = {*controller, *animator, *mouth, *crown},
        .rule_state = {},
    };
}

std::optional<GiantFaceCapture> capture_awake_lower_giant_face(
    const GameStateView& state) noexcept
{
    const auto controller = state.actor(kGiantFaceControllerSlot);
    const auto animator = state.actor(kGiantFaceAnimatorSlot);
    const auto mouth = state.actor(kGiantFaceMouthSlot);
    const auto crown = state.actor(kGiantFaceCrownSlot);
    const auto face = state.giant_face();
    if (!controller || !animator || !mouth || !crown || !face
        || !is_steady_actor(*controller, kGiantFaceControllerSlot,
            kControllerType, kControllerSteadyHandler)
        || !is_steady_actor(*animator, kGiantFaceAnimatorSlot,
            kAnimatorType, kAnimatorSteadyHandler)
        || !is_steady_actor(*mouth, kGiantFaceMouthSlot,
            kSurfaceType, kMouthSteadyHandler)
        || !is_steady_actor(*crown, kGiantFaceCrownSlot,
            kSurfaceType, kCrownSteadyHandler)
        || face->persisted_form == 0U || face->persisted_form > 3U
        || face->wake_requested
        || face->transformation_requested || face->animation_mode != 1U
        || face->mouth_open || face->bonus_stage_4_requested
        || face->display_selector > 2U
        || animator->collision_reaction.ticks_until_next_phase == 0U
        || animator->collision_reaction.ticks_until_next_phase > 8U
        || animator->collision_reaction.collision_event > 2U) {
        return std::nullopt;
    }

    return GiantFaceCapture{
        .actors = {*controller, *animator, *mouth, *crown},
        .rule_state = {},
        .controller_state = {
            .persisted_form = face->persisted_form,
            .accepts_transformations = true,
            .transformations_completed = static_cast<std::uint8_t>(
                face->persisted_form - 1U),
        },
        .animator_state = {
            .mode = GiantFaceAnimatorMode::lower_forms,
            .ticks_until_next_form =
                animator->collision_reaction.ticks_until_next_phase,
            .displayed_form = animator->collision_reaction.collision_event,
        },
    };
}

std::optional<GiantFaceCapture> capture_awake_upper_giant_face(
    const GameStateView& state) noexcept
{
    const auto controller = state.actor(kGiantFaceControllerSlot);
    const auto animator = state.actor(kGiantFaceAnimatorSlot);
    const auto mouth = state.actor(kGiantFaceMouthSlot);
    const auto crown = state.actor(kGiantFaceCrownSlot);
    const auto face = state.giant_face();
    if (!controller || !animator || !mouth || !crown || !face
        || !is_steady_actor(*controller, kGiantFaceControllerSlot,
            kControllerType, kControllerSteadyHandler)
        || !is_steady_actor(*animator, kGiantFaceAnimatorSlot,
            kAnimatorType, kAnimatorSteadyHandler)
        || !is_steady_actor(*mouth, kGiantFaceMouthSlot,
            kSurfaceType, kMouthSteadyHandler)
        || !is_steady_actor(*crown, kGiantFaceCrownSlot,
            kSurfaceType, kCrownSteadyHandler)
        || face->persisted_form < 4U || face->persisted_form > 5U
        || face->wake_requested || face->transformation_requested
        || face->animation_mode != 2U || face->mouth_open
        || face->bonus_stage_4_requested || face->display_selector < 3U
        || face->display_selector > 5U
        || animator->collision_reaction.ticks_until_next_phase == 0U
        || animator->collision_reaction.ticks_until_next_phase > 8U
        || animator->collision_reaction.collision_event < 3U
        || animator->collision_reaction.collision_event > 5U) {
        return std::nullopt;
    }

    return GiantFaceCapture{
        .actors = {*controller, *animator, *mouth, *crown},
        .rule_state = {},
        .controller_state = {
            .persisted_form = face->persisted_form,
            .accepts_transformations = true,
            .transformations_completed = static_cast<std::uint8_t>(
                face->persisted_form - 1U),
        },
        .animator_state = {
            .mode = GiantFaceAnimatorMode::upper_forms,
            .ticks_until_next_form =
                animator->collision_reaction.ticks_until_next_phase,
            .displayed_form = animator->collision_reaction.collision_event,
        },
    };
}

} // namespace devils_crush::native_runtime
