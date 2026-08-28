// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kGiantFaceControllerSlot = 0x36U;
inline constexpr std::uint8_t kGiantFaceAnimatorSlot = 0x37U;
inline constexpr std::uint8_t kGiantFaceMouthSlot = 0x38U;
inline constexpr std::uint8_t kGiantFaceCrownSlot = 0x39U;

// Slot $37 owns the compact, repeating display animation used while the
// controller has selected one of the two Giant-Face animation modes.  These
// values are display forms, not ROM handler addresses.
enum class GiantFaceAnimatorMode : std::uint8_t {
    idle,
    lower_forms,
    upper_forms,
};

struct GiantFaceAnimatorState {
    GiantFaceAnimatorMode mode = GiantFaceAnimatorMode::idle;
    std::uint8_t ticks_until_next_form = 0U;
    std::uint8_t displayed_form = 0U;

    [[nodiscard]] bool operator==(const GiantFaceAnimatorState&) const = default;
};

enum class GiantFaceAnimatorDisposition : std::uint8_t {
    idle,
    waiting,
    form_advanced,
    invalid_mode,
};

struct GiantFaceAnimatorStep {
    GiantFaceAnimatorState previous;
    GiantFaceAnimatorState state;
    GiantFaceAnimatorDisposition disposition = GiantFaceAnimatorDisposition::idle;
    bool updates_display = false;

    [[nodiscard]] bool operator==(const GiantFaceAnimatorStep&) const = default;
};

// Starts one source-defined hidden animation. Both modes begin with a
// one-turn delay; later display changes occur once every eight actor turns.
[[nodiscard]] bool begin_giant_face_animator(
    GiantFaceAnimatorState& state,
    GiantFaceAnimatorMode mode) noexcept;

// Resolves one fixed-slot-$37 actor turn. Lower mode cycles display forms
// 1,2,0; upper mode cycles 4,5,3, matching the source's exclusive bounds 3
// and 6 without exposing its local timer bytes.
[[nodiscard]] GiantFaceAnimatorStep advance_giant_face_animator(
    GiantFaceAnimatorState state) noexcept;

// The central Giant Face owns two independent collision surfaces. The
// controller hands its sixth transformation's mouth-open state to this owner
// explicitly rather than exposing the source's RAM latches.
struct GiantFaceState {
    bool crown_collision_ready = true;
    bool mouth_collision_ready = true;
    bool mouth_open = false;
    bool crown_impact_presentation_pending = false;
    bool mouth_impact_presentation_pending = false;
    bool bonus_stage_4_requested = false;
    bool wake_requested = false;
    bool transformation_requested = false;

    [[nodiscard]] bool operator==(const GiantFaceState&) const = default;
};

// The mirrored helmet channels request this state before their own opening
// presentation runs. The controller consumes it in its separate fixed-slot
// domain; this collision state retains it as a named cross-owner signal.
[[nodiscard]] bool request_giant_face_wake(GiantFaceState& state) noexcept;

// The table's separate condition owner raises this signal after each
// qualifying event. It is retained through the wake sequence, just as the
// source retains its independent request bit, then consumed by slot $36.
[[nodiscard]] bool request_giant_face_transformation(
    GiantFaceState& state) noexcept;

// Slot $36 owns the wake sequence started by either helmet channel and the
// six later transformations selected by the table's separate condition owner.
struct GiantFaceControllerState {
    bool wake_active = false;
    std::uint8_t wake_steps_completed = 0U;
    std::uint8_t ticks_until_next_wake_step = 0U;
    std::uint8_t persisted_form = 0U;
    bool accepts_transformations = false;
    std::uint8_t transformations_completed = 0U;

    [[nodiscard]] bool operator==(const GiantFaceControllerState&) const = default;
};

enum class GiantFaceControllerDisposition : std::uint8_t {
    dormant,
    wake_started,
    wake_waiting,
    wake_step_advanced,
    wake_completed,
    transformation_selected,
    mouth_opened,
    reset_completed,
};

struct GiantFaceControllerStep {
    GiantFaceControllerState previous;
    GiantFaceControllerState state;
    GiantFaceState face_state;
    GiantFaceControllerDisposition disposition = GiantFaceControllerDisposition::dormant;
    bool requests_wake_effect = false;
    bool requests_transformation_effect = false;
    bool starts_lower_form_animation = false;
    bool starts_upper_form_animation = false;
    bool stops_form_animation = false;
    bool opens_mouth = false;

    [[nodiscard]] bool operator==(const GiantFaceControllerStep&) const = default;
};

// Resolves one fixed controller turn. A wake begins with the source's
// one-turn delay, then advances four visual steps at four-turn intervals.
// The fourth wake step stores form one, clears the wake request, and starts
// the lower-form animator. Subsequent retained transformation requests select
// forms 2,3,4,5,6, and 7 in order. Form four starts the upper animator, form
// six opens the mouth, and form seven restores the dormant controller state.
[[nodiscard]] GiantFaceControllerStep advance_giant_face_controller(
    GiantFaceControllerState state,
    GiantFaceState face_state) noexcept;

enum class GiantFaceContactDisposition : std::uint8_t {
    crown_scored,
    closed_mouth_scored,
    open_mouth_captured,
    invalid_state,
};

struct GiantFaceContactStep {
    GiantFaceState previous;
    GiantFaceState state;
    GiantFaceContactDisposition disposition = GiantFaceContactDisposition::invalid_state;
    std::uint16_t score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_impact_effect = false;
    bool requests_bonus_stage_4 = false;
    bool places_ball = false;
    std::int16_t ball_horizontal_pixels = 0;
    std::int16_t ball_vertical_pixels = 0;

    [[nodiscard]] bool operator==(const GiantFaceContactStep&) const = default;
};

// Resolves the source's `03:76FA` crown surface. It awards three stored
// units, emits `$20`, and waits for the shared impact presentation before the
// surface is collision-ready again.
[[nodiscard]] GiantFaceContactStep consume_giant_face_crown_contact(
    GiantFaceState state) noexcept;

// Resolves the mouth surface. While closed it has the same 300-point rule as
// the crown. Once the form owner opened it, a capture awards 100 stored units,
// places the ball at (144,344), emits `$41`, and requests bonus stage four.
[[nodiscard]] GiantFaceContactStep consume_giant_face_mouth_contact(
    GiantFaceState state) noexcept;

// The form/controller owner calls this only after the verified sixth-form
// transition. It changes collision meaning but does not start bonus play.
[[nodiscard]] bool complete_giant_face_mouth_opening(GiantFaceState& state) noexcept;

// A shared visual-effect/presentation owner acknowledges a normal crown or
// closed-mouth hit before native collision can resume on that surface.
[[nodiscard]] bool complete_giant_face_impact_presentation(
    GiantFaceState& state,
    std::uint8_t slot) noexcept;

struct GiantFaceRulesUpdate {
    GiantFaceContactStep giant_face;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const GiantFaceRulesUpdate&) const = default;
};

[[nodiscard]] GiantFaceRulesUpdate resolve_giant_face_crown_contact(
    GiantFaceState state,
    ScoreState score) noexcept;
[[nodiscard]] GiantFaceRulesUpdate resolve_giant_face_mouth_contact(
    GiantFaceState state,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
