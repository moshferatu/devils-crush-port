// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kGiantFaceLeftHelmetSlot = 0x2AU;
inline constexpr std::uint8_t kGiantFaceRightHelmetSlot = 0x2BU;

enum class GiantFaceHelmetSide : std::uint8_t {
    left,
    right,
};

struct GiantFaceHelmetChannelState {
    GiantFaceHelmetSide side = GiantFaceHelmetSide::left;
    bool collision_ready = true;
    std::uint8_t ticks_until_appearance = 1U;
    std::uint8_t appearance = 0U;
    bool activation_presentation_pending = false;
    bool ball_return_pending = false;

    [[nodiscard]] bool operator==(const GiantFaceHelmetChannelState&) const = default;
};

struct GiantFaceHelmetState {
    std::array<GiantFaceHelmetChannelState, 2U> channels{{
        {.side = GiantFaceHelmetSide::left},
        {.side = GiantFaceHelmetSide::right},
    }};

    [[nodiscard]] bool operator==(const GiantFaceHelmetState&) const = default;
};

enum class GiantFaceHelmetDisposition : std::uint8_t {
    timer_waiting,
    appearance_advanced,
    activation_started,
    waiting_for_activation_presentation,
    invalid_state,
};

struct GiantFaceHelmetStep {
    GiantFaceHelmetState previous;
    GiantFaceHelmetState state;
    GiantFaceHelmetDisposition disposition = GiantFaceHelmetDisposition::invalid_state;
    std::uint16_t score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_face_wake = false;
    bool requests_opening_presentation = false;

    [[nodiscard]] bool operator==(const GiantFaceHelmetStep&) const = default;
};

// Resolves a fixed helmet-channel turn. A confirmed contact has already
// passed the special-condition owner; the native channel then awards three
// stored units, emits `$20`, and asks the Giant-Face controller to wake.
[[nodiscard]] GiantFaceHelmetStep advance_giant_face_helmet_channel(
    GiantFaceHelmetState state,
    std::uint8_t slot,
    bool contact_confirmed) noexcept;

struct GiantFaceHelmetCompletionStep {
    GiantFaceHelmetState previous;
    GiantFaceHelmetState state;
    bool completed = false;
    std::uint8_t audio_command = 0U;
    bool requests_ball_return = false;
    GiantFaceHelmetSide ball_return_side = GiantFaceHelmetSide::left;

    [[nodiscard]] bool operator==(const GiantFaceHelmetCompletionStep&) const = default;
};

// The source emits `$25` only once the opening presentation completes, then
// hands ball routing to its side-specific helper. Route geometry stays with
// the future native ball-launch owner.
[[nodiscard]] GiantFaceHelmetCompletionStep
complete_giant_face_helmet_activation_presentation(
    GiantFaceHelmetState state,
    std::uint8_t slot) noexcept;

// The ball-launch owner acknowledges its side-specific route before the
// channel becomes collision-ready again.
[[nodiscard]] bool complete_giant_face_helmet_ball_return(
    GiantFaceHelmetState& state,
    std::uint8_t slot) noexcept;

struct GiantFaceHelmetRulesUpdate {
    GiantFaceHelmetStep helmet;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const GiantFaceHelmetRulesUpdate&) const = default;
};

[[nodiscard]] GiantFaceHelmetRulesUpdate resolve_giant_face_helmet_channel(
    GiantFaceHelmetState state,
    std::uint8_t slot,
    bool contact_confirmed,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
