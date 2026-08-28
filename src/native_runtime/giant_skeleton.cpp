// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_skeleton.hpp"

#include <array>
#include <cstdlib>

namespace devils_crush::native_runtime {
namespace {

constexpr GiantSkeletonTargetPosition kGiantSkeletonPosition{
    .horizontal_pixels = 224,
    .vertical_pixels = 518,
};

// 01:DB9E's reachable direction entries. The classifier builds these bits
// from horizontal/vertical signs, dominant-axis swap, and two slope bands.
constexpr std::array<std::uint8_t, 24U> kDirectionCodes{
    0x00U, 0x00U, 0x08U, 0x08U, 0x04U, 0x0CU, 0x04U, 0x0CU,
    0x01U, 0x0FU, 0x07U, 0x09U, 0x03U, 0x0DU, 0x05U, 0x0BU,
    0x02U, 0x0EU, 0x06U, 0x0AU, 0x02U, 0x0EU, 0x06U, 0x0AU,
};

[[nodiscard]] std::uint8_t clamped_magnitude(const std::int32_t delta) noexcept
{
    const auto magnitude = std::abs(delta);
    return static_cast<std::uint8_t>(magnitude > 0xFF ? 0xFF : magnitude);
}

[[nodiscard]] std::uint8_t classify_direction(
    const GiantSkeletonTargetPosition target) noexcept
{
    const auto horizontal_delta = static_cast<std::int32_t>(
        target.horizontal_pixels) - kGiantSkeletonPosition.horizontal_pixels;
    const auto vertical_delta = static_cast<std::int32_t>(
        target.vertical_pixels) - kGiantSkeletonPosition.vertical_pixels;
    auto horizontal = clamped_magnitude(horizontal_delta);
    auto vertical = clamped_magnitude(vertical_delta);
    std::uint8_t bits = 0U;
    if (horizontal_delta < 0) {
        bits = static_cast<std::uint8_t>(bits | 0x01U);
    }
    if (vertical_delta < 0) {
        bits = static_cast<std::uint8_t>(bits | 0x02U);
    }

    if (horizontal < vertical) {
        const auto saved_horizontal = horizontal;
        horizontal = vertical;
        vertical = saved_horizontal;
        bits = static_cast<std::uint8_t>(bits | 0x04U);
    }

    const auto larger = static_cast<std::uint16_t>(horizontal);
    const auto smaller = static_cast<std::uint16_t>(vertical);
    if (smaller * 3U >= larger * 2U) {
        bits = static_cast<std::uint8_t>(bits | 0x10U);
    } else if (smaller * 12U >= larger * 2U) {
        bits = static_cast<std::uint8_t>(bits | 0x08U);
    }
    return kDirectionCodes[bits];
}

[[nodiscard]] std::uint8_t appearance_for_direction(
    const std::uint8_t direction_code) noexcept
{
    const auto clamped = direction_code < 4U ? 4U
        : direction_code >= 13U ? 12U
                                : direction_code;
    return static_cast<std::uint8_t>(clamped >> 1U);
}

} // namespace

GiantSkeletonUpdateStep advance_giant_skeleton(
    GiantSkeletonState state,
    const GiantSkeletonTargetPosition target,
    const bool confirmed_contact) noexcept
{
    const auto previous = state;
    const auto direction_code = classify_direction(target);
    state.appearance = appearance_for_direction(direction_code);
    if (!confirmed_contact) {
        return {
            .previous = previous,
            .state = state,
            .target = target,
            .direction_code = direction_code,
        };
    }

    return {
        .previous = previous,
        .state = state,
        .target = target,
        .direction_code = direction_code,
        .disposition = GiantSkeletonUpdateDisposition::repeatable_hit,
        .hit_score_award_units = 40U,
        .audio_command = 0x2EU,
        .requests_impact_presentation = true,
        .requested_appearance = 2U,
    };
}

GiantSkeletonRulesUpdate resolve_giant_skeleton_update(
    const GiantSkeletonState state,
    const GiantSkeletonTargetPosition target,
    const bool confirmed_contact,
    const ScoreState score) noexcept
{
    const auto giant_skeleton = advance_giant_skeleton(
        state, target, confirmed_contact);
    return {
        .giant_skeleton = giant_skeleton,
        .hit_score_award = apply_score_award(
            score, giant_skeleton.hit_score_award_units),
    };
}

} // namespace devils_crush::native_runtime
