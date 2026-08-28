// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_retry_placement.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] constexpr std::size_t direction_index(
    const CollisionRetryDirection direction) noexcept
{
    return static_cast<std::size_t>(direction);
}

[[nodiscard]] constexpr std::uint8_t direction_mask(
    const CollisionRetryDirection direction) noexcept
{
    return static_cast<std::uint8_t>(0x80U >> direction_index(direction));
}

[[nodiscard]] CollisionRetryPlacementUpdate make_probe_update(
    CollisionRetryPlacementState state) noexcept
{
    return {
        .state = state,
        .disposition = CollisionRetryPlacementDisposition::probe_ready,
        .probe = state.current_attempt,
    };
}

[[nodiscard]] CollisionRetryPlacementUpdate make_unresolved_update(
    CollisionRetryPlacementState state) noexcept
{
    return {
        .state = state,
        .disposition = CollisionRetryPlacementDisposition::unresolved,
        .probe = state.current_attempt,
    };
}

[[nodiscard]] CollisionRetryPlacementUpdate make_direction_update(
    CollisionRetryPlacementState state) noexcept
{
    return {
        .state = state,
        .disposition =
            CollisionRetryPlacementDisposition::needs_random_direction,
    };
}

[[nodiscard]] CollisionRetryPlacementUpdate make_accepted_update(
    CollisionRetryPlacementState state) noexcept
{
    return {
        .state = state,
        .disposition = CollisionRetryPlacementDisposition::accepted,
        .probe = state.current_attempt,
    };
}

[[nodiscard]] CollisionRetryPlacementUpdate make_invalid_state_update(
    CollisionRetryPlacementState state) noexcept
{
    return {
        .state = state,
        .disposition = CollisionRetryPlacementDisposition::invalid_state,
    };
}

} // namespace

CollisionRetryPlacementUpdate begin_collision_retry_placement(
    const BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsetAttempts& offsets) noexcept
{
    const auto direction = classify_collision_retry_direction(
        impact_ball.velocity_x, impact_ball.velocity_y);
    const auto sequence = make_collision_retry_attempt_sequence_for_direction(
        impact_ball, previous_position, offsets, direction);
    return make_probe_update({
        .impact_ball = impact_ball,
        .previous_position = previous_position,
        .offsets = offsets,
        .direction = direction,
        .attempted_direction_flags = direction_mask(direction),
        .current_attempt = sequence.attempts.front(),
    });
}

CollisionRetryPlacementUpdate advance_collision_retry_placement(
    CollisionRetryPlacementState state,
    const CollisionRetryFullScanOutcome outcome) noexcept
{
    if (state.phase != CollisionRetryPlacementPhase::offset_attempt
        && state.phase != CollisionRetryPlacementPhase::vertical_fallback) {
        return make_invalid_state_update(state);
    }
    if (outcome == CollisionRetryFullScanOutcome::unresolved) {
        return make_unresolved_update(state);
    }
    if (outcome == CollisionRetryFullScanOutcome::clear) {
        state.phase = CollisionRetryPlacementPhase::accepted;
        return make_accepted_update(state);
    }

    if (state.phase == CollisionRetryPlacementPhase::offset_attempt) {
        if (state.attempt_index + 1U
            < CollisionRetryOffsetAttempts::kAttemptsPerDirection) {
            ++state.attempt_index;
            const auto sequence = make_collision_retry_attempt_sequence_for_direction(
                state.impact_ball,
                state.previous_position,
                state.offsets,
                state.direction);
            state.current_attempt = sequence.attempts[state.attempt_index];
            return make_probe_update(state);
        }
        state.phase = CollisionRetryPlacementPhase::selecting_direction;
        return make_direction_update(state);
    }

    // A fallback pass is intentionally restartable. After its seventh solid
    // candidate, CE98 begins a new seven-step pass from that final candidate.
    const auto next_pass = make_collision_retry_vertical_fallback_sequence(
        state.impact_ball, state.current_attempt.state);
    if (state.attempt_index + 1U
        < CollisionRetryVerticalFallbackSequence::kStepsPerPass) {
        ++state.attempt_index;
    } else {
        state.attempt_index = 0U;
    }
    state.current_attempt = next_pass.attempts.front();
    return make_probe_update(state);
}

CollisionRetryPlacementUpdate select_collision_retry_placement_direction(
    CollisionRetryPlacementState state,
    const std::uint8_t random_byte) noexcept
{
    if (state.phase != CollisionRetryPlacementPhase::selecting_direction) {
        return make_invalid_state_update(state);
    }
    const auto selection = select_collision_retry_direction(
        random_byte, state.attempted_direction_flags);
    if (selection.disposition
        == CollisionRetryDirectionSelectionDisposition::already_attempted) {
        return make_direction_update(state);
    }
    if (selection.disposition
        == CollisionRetryDirectionSelectionDisposition::fallback_required) {
        const auto sequence = make_collision_retry_vertical_fallback_sequence(
            state.impact_ball, state.current_attempt.state);
        state.phase = CollisionRetryPlacementPhase::vertical_fallback;
        state.attempt_index = 0U;
        state.current_attempt = sequence.attempts.front();
        return make_probe_update(state);
    }

    state.direction = *selection.direction;
    state.attempted_direction_flags = selection.attempted_direction_flags_after;
    state.phase = CollisionRetryPlacementPhase::offset_attempt;
    state.attempt_index = 0U;
    const auto sequence = make_collision_retry_attempt_sequence_for_direction(
        state.impact_ball,
        state.previous_position,
        state.offsets,
        state.direction);
    state.current_attempt = sequence.attempts.front();
    return make_probe_update(state);
}

} // namespace devils_crush::native_runtime
