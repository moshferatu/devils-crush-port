// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_retry.hpp"

#include <cstddef>

namespace devils_crush::native_runtime {
namespace {

constexpr std::int32_t kPositionMask = 0x00FFFFFF;
constexpr std::int32_t kPositionSignBit = 0x00800000;
constexpr std::int32_t kPositionWrap = 0x01000000;
constexpr std::uint8_t kAllCollisionRetryDirectionFlags = 0xF0U;

[[nodiscard]] constexpr std::int32_t wrap_fixed24_8(
    const std::int64_t raw) noexcept
{
    const auto bits = static_cast<std::int32_t>(raw & kPositionMask);
    return (bits & kPositionSignBit) != 0 ? bits - kPositionWrap : bits;
}

[[nodiscard]] constexpr std::size_t direction_index(
    const CollisionRetryDirection direction) noexcept
{
    return static_cast<std::size_t>(direction);
}

[[nodiscard]] constexpr bool integer_addition_carries(
    const Fixed24_8 position,
    const std::int16_t offset_pixels) noexcept
{
    const auto integer_coordinate = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(position.raw) >> 8U);
    const auto offset = static_cast<std::uint16_t>(offset_pixels);
    return static_cast<std::uint32_t>(integer_coordinate)
        + static_cast<std::uint32_t>(offset) > 0xFFFFU;
}

[[nodiscard]] CollisionRetryStep apply_collision_retry_offset(
    const BallState impact_ball,
    BallState state,
    const CollisionRetryDirection direction,
    const CollisionRetryOffset offset) noexcept
{
    const auto vertical_integer_addition_carried = integer_addition_carries(
        state.position_y, offset.vertical_pixels);
    state.position_x.raw = wrap_fixed24_8(
        static_cast<std::int64_t>(state.position_x.raw)
        + static_cast<std::int32_t>(offset.horizontal_pixels) * 256);
    state.position_y.raw = wrap_fixed24_8(
        static_cast<std::int64_t>(state.position_y.raw)
        + static_cast<std::int32_t>(offset.vertical_pixels) * 256);
    return {
        .impact_ball = impact_ball,
        .state = state,
        .direction = direction,
        .offset = offset,
        .vertical_integer_addition_carried = vertical_integer_addition_carried,
    };
}

} // namespace

CollisionRetryDirection classify_collision_retry_direction(
    const std::int16_t velocity_x,
    const std::int16_t velocity_y) noexcept
{
    if (velocity_x < 0) {
        return velocity_y < 0
            ? CollisionRetryDirection::negative_x_negative_y
            : CollisionRetryDirection::negative_x_positive_y;
    }
    return velocity_y < 0
        ? CollisionRetryDirection::positive_x_negative_y
        : CollisionRetryDirection::positive_x_positive_y;
}

CollisionRetryStep retry_ball_collision(
    const BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsets& offsets) noexcept
{
    const auto direction = classify_collision_retry_direction(
        impact_ball.velocity_x, impact_ball.velocity_y);
    const auto offset = offsets.by_direction[direction_index(direction)];
    return apply_collision_retry_offset(
        impact_ball,
        rollback_ball_position(impact_ball, previous_position),
        direction,
        offset);
}

CollisionRetryAttemptSequence make_collision_retry_attempt_sequence(
    const BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsetAttempts& offsets) noexcept
{
    const auto direction = classify_collision_retry_direction(
        impact_ball.velocity_x, impact_ball.velocity_y);
    return make_collision_retry_attempt_sequence_for_direction(
        impact_ball, previous_position, offsets, direction);
}

CollisionRetryAttemptSequence make_collision_retry_attempt_sequence_for_direction(
    const BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsetAttempts& offsets,
    const CollisionRetryDirection direction) noexcept
{
    CollisionRetryAttemptSequence sequence{
        .restored_ball = rollback_ball_position(impact_ball, previous_position),
        .direction = direction,
    };
    auto candidate = sequence.restored_ball;
    for (std::size_t attempt = 0U; attempt < sequence.attempts.size(); ++attempt) {
        sequence.attempts[attempt] = apply_collision_retry_offset(
            impact_ball,
            candidate,
            direction,
            offsets.by_direction[direction_index(direction)][attempt]);
        candidate = sequence.attempts[attempt].state;
    }
    return sequence;
}

CollisionRetryDirectionSelection select_collision_retry_direction(
    const std::uint8_t random_byte,
    const std::uint8_t attempted_direction_flags) noexcept
{
    if ((attempted_direction_flags & kAllCollisionRetryDirectionFlags)
        == kAllCollisionRetryDirectionFlags) {
        return {
            .attempted_direction_flags_after = attempted_direction_flags,
            .disposition =
                CollisionRetryDirectionSelectionDisposition::fallback_required,
        };
    }

    // E8A4 computes floor(random_byte * 4 / 256), which is the top two bits.
    const auto direction = static_cast<CollisionRetryDirection>(random_byte >> 6U);
    const auto direction_mask = static_cast<std::uint8_t>(
        0x80U >> direction_index(direction));
    if ((attempted_direction_flags & direction_mask) != 0U) {
        return {
            .direction = direction,
            .direction_mask = direction_mask,
            .attempted_direction_flags_after = attempted_direction_flags,
            .disposition =
                CollisionRetryDirectionSelectionDisposition::already_attempted,
        };
    }
    return {
        .direction = direction,
        .direction_mask = direction_mask,
        .attempted_direction_flags_after = static_cast<std::uint8_t>(
            attempted_direction_flags | direction_mask),
        .disposition = CollisionRetryDirectionSelectionDisposition::selected,
    };
}

CollisionRetryVerticalFallbackSequence
make_collision_retry_vertical_fallback_sequence(
    const BallState impact_ball,
    BallState rejected_candidate) noexcept
{
    const auto direction = classify_collision_retry_direction(
        impact_ball.velocity_x, impact_ball.velocity_y);
    CollisionRetryVerticalFallbackSequence sequence;
    for (auto& attempt : sequence.attempts) {
        attempt = apply_collision_retry_offset(
            impact_ball,
            rejected_candidate,
            direction,
            {.vertical_pixels = -1});
        rejected_candidate = attempt.state;
    }
    return sequence;
}

} // namespace devils_crush::native_runtime
