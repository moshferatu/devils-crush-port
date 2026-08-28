// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_face.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] std::uint8_t animator_start_form(
    const GiantFaceAnimatorMode mode) noexcept
{
    return mode == GiantFaceAnimatorMode::upper_forms ? 3U : 0U;
}

[[nodiscard]] std::uint8_t animator_exclusive_upper_bound(
    const GiantFaceAnimatorMode mode) noexcept
{
    return mode == GiantFaceAnimatorMode::upper_forms ? 6U : 3U;
}

[[nodiscard]] GiantFaceContactStep make_normal_impact(
    GiantFaceState state,
    const GiantFaceContactDisposition disposition,
    const bool crown) noexcept
{
    const auto previous = state;
    if (crown) {
        state.crown_collision_ready = false;
        state.crown_impact_presentation_pending = true;
    } else {
        state.mouth_collision_ready = false;
        state.mouth_impact_presentation_pending = true;
    }
    return {
        .previous = previous,
        .state = state,
        .disposition = disposition,
        .score_award_units = 3U,
        .audio_command = 0x20U,
        .requests_impact_effect = true,
    };
}

[[nodiscard]] GiantFaceRulesUpdate resolve_contact(
    const GiantFaceContactStep step,
    const ScoreState score) noexcept
{
    return {
        .giant_face = step,
        .score_award = apply_score_award(score, step.score_award_units),
    };
}

} // namespace

bool request_giant_face_wake(GiantFaceState& state) noexcept
{
    if (state.wake_requested) {
        return false;
    }
    state.wake_requested = true;
    return true;
}

bool request_giant_face_transformation(GiantFaceState& state) noexcept
{
    if (state.transformation_requested) {
        return false;
    }
    state.transformation_requested = true;
    return true;
}

GiantFaceControllerStep advance_giant_face_controller(
    GiantFaceControllerState state,
    GiantFaceState face_state) noexcept
{
    const auto previous = state;
    if (!state.wake_active) {
        if (state.accepts_transformations && face_state.transformation_requested) {
            face_state.transformation_requested = false;
            ++state.transformations_completed;
            const auto transformation = state.transformations_completed;

            if (transformation < 5U) {
                // The source table selects persisted forms 2, 3, 4, and 5
                // for its first four transformation vectors.
                state.persisted_form = static_cast<std::uint8_t>(transformation + 1U);
                return {
                    .previous = previous,
                    .state = state,
                    .face_state = face_state,
                    .disposition = GiantFaceControllerDisposition::transformation_selected,
                    .requests_transformation_effect = true,
                    .starts_upper_form_animation = transformation == 3U,
                };
            }

            if (transformation == 5U) {
                // The fifth table entry initially names form five, but its
                // handler persists form six and sets the mouth-open latch.
                state.persisted_form = 6U;
                face_state.mouth_open = true;
                face_state.mouth_collision_ready = true;
                return {
                    .previous = previous,
                    .state = state,
                    .face_state = face_state,
                    .disposition = GiantFaceControllerDisposition::mouth_opened,
                    .requests_transformation_effect = true,
                    .opens_mouth = true,
                };
            }

            // The final vector selects form seven, stops the hidden animator,
            // then runs the controller initializer. Its initializer clears
            // persisted form, requests, and local transformation progress;
            // mouth state belongs to the collision owner and is not invented
            // as part of that reset.
            state = {};
            face_state.wake_requested = false;
            return {
                .previous = previous,
                .state = state,
                .face_state = face_state,
                .disposition = GiantFaceControllerDisposition::reset_completed,
                .requests_transformation_effect = true,
                .stops_form_animation = true,
            };
        }
        if (!face_state.wake_requested) {
            return {
                .previous = previous,
                .state = state,
                .face_state = face_state,
            };
        }
        state.wake_active = true;
        state.wake_steps_completed = 0U;
        state.ticks_until_next_wake_step = 1U;
        return {
            .previous = previous,
            .state = state,
            .face_state = face_state,
            .disposition = GiantFaceControllerDisposition::wake_started,
        };
    }

    if (state.ticks_until_next_wake_step > 1U) {
        --state.ticks_until_next_wake_step;
        return {
            .previous = previous,
            .state = state,
            .face_state = face_state,
            .disposition = GiantFaceControllerDisposition::wake_waiting,
        };
    }

    state.ticks_until_next_wake_step = 4U;
    ++state.wake_steps_completed;
    if (state.wake_steps_completed < 4U) {
        return {
            .previous = previous,
            .state = state,
            .face_state = face_state,
            .disposition = GiantFaceControllerDisposition::wake_step_advanced,
            .requests_wake_effect = true,
        };
    }

    state.wake_active = false;
    state.persisted_form = 1U;
    state.accepts_transformations = true;
    face_state.wake_requested = false;
    return {
        .previous = previous,
        .state = state,
        .face_state = face_state,
        .disposition = GiantFaceControllerDisposition::wake_completed,
        .requests_wake_effect = true,
        .starts_lower_form_animation = true,
    };
}

bool begin_giant_face_animator(
    GiantFaceAnimatorState& state,
    const GiantFaceAnimatorMode mode) noexcept
{
    if (mode != GiantFaceAnimatorMode::lower_forms
        && mode != GiantFaceAnimatorMode::upper_forms) {
        return false;
    }
    state = {
        .mode = mode,
        .ticks_until_next_form = 1U,
        .displayed_form = animator_start_form(mode),
    };
    return true;
}

GiantFaceAnimatorStep advance_giant_face_animator(
    GiantFaceAnimatorState state) noexcept
{
    const auto previous = state;
    if (state.mode == GiantFaceAnimatorMode::idle) {
        return {.previous = previous, .state = state};
    }
    if (state.mode != GiantFaceAnimatorMode::lower_forms
        && state.mode != GiantFaceAnimatorMode::upper_forms) {
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantFaceAnimatorDisposition::invalid_mode,
        };
    }

    if (state.ticks_until_next_form > 1U) {
        --state.ticks_until_next_form;
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantFaceAnimatorDisposition::waiting,
        };
    }

    state.ticks_until_next_form = 8U;
    ++state.displayed_form;
    if (state.displayed_form >= animator_exclusive_upper_bound(state.mode)) {
        state.displayed_form = animator_start_form(state.mode);
    }
    return {
        .previous = previous,
        .state = state,
        .disposition = GiantFaceAnimatorDisposition::form_advanced,
        .updates_display = true,
    };
}

GiantFaceContactStep consume_giant_face_crown_contact(GiantFaceState state) noexcept
{
    if (!state.crown_collision_ready || state.crown_impact_presentation_pending) {
        return {.previous = state, .state = state};
    }
    return make_normal_impact(state, GiantFaceContactDisposition::crown_scored, true);
}

GiantFaceContactStep consume_giant_face_mouth_contact(GiantFaceState state) noexcept
{
    if (!state.mouth_collision_ready || state.mouth_impact_presentation_pending
        || state.bonus_stage_4_requested) {
        return {.previous = state, .state = state};
    }
    if (!state.mouth_open) {
        return make_normal_impact(
            state, GiantFaceContactDisposition::closed_mouth_scored, false);
    }

    const auto previous = state;
    state.mouth_collision_ready = false;
    state.bonus_stage_4_requested = true;
    return {
        .previous = previous,
        .state = state,
        .disposition = GiantFaceContactDisposition::open_mouth_captured,
        .score_award_units = 100U,
        .audio_command = 0x41U,
        .requests_bonus_stage_4 = true,
        .places_ball = true,
        .ball_horizontal_pixels = 144,
        .ball_vertical_pixels = 344,
    };
}

bool complete_giant_face_mouth_opening(GiantFaceState& state) noexcept
{
    if (state.mouth_open || state.bonus_stage_4_requested
        || state.mouth_impact_presentation_pending) {
        return false;
    }
    state.mouth_open = true;
    state.mouth_collision_ready = true;
    return true;
}

bool complete_giant_face_impact_presentation(
    GiantFaceState& state,
    const std::uint8_t slot) noexcept
{
    if (slot == kGiantFaceCrownSlot && state.crown_impact_presentation_pending) {
        state.crown_impact_presentation_pending = false;
        state.crown_collision_ready = true;
        return true;
    }
    if (slot == kGiantFaceMouthSlot && state.mouth_impact_presentation_pending) {
        state.mouth_impact_presentation_pending = false;
        state.mouth_collision_ready = true;
        return true;
    }
    return false;
}

GiantFaceRulesUpdate resolve_giant_face_crown_contact(
    const GiantFaceState state,
    const ScoreState score) noexcept
{
    return resolve_contact(consume_giant_face_crown_contact(state), score);
}

GiantFaceRulesUpdate resolve_giant_face_mouth_contact(
    const GiantFaceState state,
    const ScoreState score) noexcept
{
    return resolve_contact(consume_giant_face_mouth_contact(state), score);
}

} // namespace devils_crush::native_runtime
