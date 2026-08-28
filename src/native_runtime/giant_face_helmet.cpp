// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_face_helmet.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] std::optional<std::size_t> channel_index(const std::uint8_t slot) noexcept
{
    if (slot == kGiantFaceLeftHelmetSlot) {
        return 0U;
    }
    if (slot == kGiantFaceRightHelmetSlot) {
        return 1U;
    }
    return std::nullopt;
}

[[nodiscard]] GiantFaceHelmetStep invalid_step(const GiantFaceHelmetState state) noexcept
{
    return {.previous = state, .state = state};
}

} // namespace

GiantFaceHelmetStep advance_giant_face_helmet_channel(
    GiantFaceHelmetState state,
    const std::uint8_t slot,
    const bool contact_confirmed) noexcept
{
    const auto index = channel_index(slot);
    if (!index) {
        return invalid_step(state);
    }
    const auto previous = state;
    auto& channel = state.channels[*index];

    if (channel.activation_presentation_pending || channel.ball_return_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantFaceHelmetDisposition::waiting_for_activation_presentation,
        };
    }
    if (contact_confirmed) {
        if (!channel.collision_ready) {
            return invalid_step(state);
        }
        channel.collision_ready = false;
        channel.activation_presentation_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantFaceHelmetDisposition::activation_started,
            .score_award_units = 3U,
            .audio_command = 0x20U,
            .requests_face_wake = true,
            .requests_opening_presentation = true,
        };
    }
    if (channel.ticks_until_appearance > 1U) {
        --channel.ticks_until_appearance;
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantFaceHelmetDisposition::timer_waiting,
        };
    }

    channel.ticks_until_appearance = 4U;
    channel.appearance = static_cast<std::uint8_t>((channel.appearance + 1U) % 4U);
    return {
        .previous = previous,
        .state = state,
        .disposition = GiantFaceHelmetDisposition::appearance_advanced,
    };
}

GiantFaceHelmetCompletionStep complete_giant_face_helmet_activation_presentation(
    GiantFaceHelmetState state,
    const std::uint8_t slot) noexcept
{
    const auto index = channel_index(slot);
    if (!index || !state.channels[*index].activation_presentation_pending) {
        return {.previous = state, .state = state};
    }
    const auto previous = state;
    auto& channel = state.channels[*index];
    channel.activation_presentation_pending = false;
    channel.ball_return_pending = true;
    return {
        .previous = previous,
        .state = state,
        .completed = true,
        .audio_command = 0x25U,
        .requests_ball_return = true,
        .ball_return_side = channel.side,
    };
}

bool complete_giant_face_helmet_ball_return(
    GiantFaceHelmetState& state,
    const std::uint8_t slot) noexcept
{
    const auto index = channel_index(slot);
    if (!index || !state.channels[*index].ball_return_pending) {
        return false;
    }
    auto& channel = state.channels[*index];
    channel.ball_return_pending = false;
    channel.collision_ready = true;
    channel.ticks_until_appearance = 4U;
    return true;
}

GiantFaceHelmetRulesUpdate resolve_giant_face_helmet_channel(
    const GiantFaceHelmetState state,
    const std::uint8_t slot,
    const bool contact_confirmed,
    const ScoreState score) noexcept
{
    const auto helmet = advance_giant_face_helmet_channel(state, slot, contact_confirmed);
    return {
        .helmet = helmet,
        .score_award = apply_score_award(score, helmet.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
