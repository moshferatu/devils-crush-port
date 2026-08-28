// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/actor_collision_scan.hpp"
#include "devils_crush/native_runtime/collision_retry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// The placement loop has three distinct waiting points: a candidate collision
// result, a new random direction, and the accepted position. Keeping those
// states named prevents the native owner from treating a collision response as
// part of the placement decision.
enum class CollisionRetryPlacementPhase : std::uint8_t {
    offset_attempt,
    vertical_fallback,
    selecting_direction,
    accepted,
};

struct CollisionRetryPlacementState {
    BallState impact_ball;
    BallState previous_position;
    CollisionRetryOffsetAttempts offsets;
    CollisionRetryDirection direction =
        CollisionRetryDirection::positive_x_positive_y;
    // Mirrors CDF8's high-nibble $70 direction mask. The low nibble is not a
    // placement-loop input and deliberately remains outside this state.
    std::uint8_t attempted_direction_flags = 0;
    CollisionRetryPlacementPhase phase =
        CollisionRetryPlacementPhase::offset_attempt;
    std::size_t attempt_index = 0;
    CollisionRetryStep current_attempt;

    [[nodiscard]] bool operator==(const CollisionRetryPlacementState&) const
        = default;
};

enum class CollisionRetryPlacementDisposition : std::uint8_t {
    probe_ready,
    needs_random_direction,
    accepted,
    // The playfield-owned CDF8 caller has accepted a clear placement. CB7E's
    // clear arm removes the signed collision marker, then the retained C6/C7
    // cursor continues at that accepted position. The placement does not by
    // itself complete the enclosing frame.
    rearmed,
    unresolved,
    invalid_state,
};

struct CollisionRetryPlacementUpdate {
    CollisionRetryPlacementState state;
    CollisionRetryPlacementDisposition disposition =
        CollisionRetryPlacementDisposition::probe_ready;
    // Present for a probe-ready or unresolved update. An unresolved result may
    // be re-evaluated after the missing collision asset becomes available.
    std::optional<CollisionRetryStep> probe;

    [[nodiscard]] bool operator==(const CollisionRetryPlacementUpdate&) const
        = default;
};

// Begins CDF8's placement loop at the initial motion quadrant and its first
// cumulative offset. The caller supplies the result of probing `probe` to the
// advance function below.
[[nodiscard]] CollisionRetryPlacementUpdate begin_collision_retry_placement(
    BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsetAttempts& offsets) noexcept;

// Advances a pending candidate after CEB5/CED2 reports its carry-like result.
// This function is valid only while state.phase is offset_attempt or
// vertical_fallback. An unresolved result intentionally retains the candidate
// rather than guessing that its unread asset was clear. Other phases return
// invalid_state without modifying the supplied state.
[[nodiscard]] CollisionRetryPlacementUpdate advance_collision_retry_placement(
    CollisionRetryPlacementState state,
    CollisionRetryFullScanOutcome outcome) noexcept;

// Supplies CDF8's shared-RNG byte after all three same-direction candidates
// collide. Repeated selections keep waiting for another byte; exhausting the
// four high-nibble direction flags enters the seven-step vertical fallback.
// Other phases return invalid_state without modifying the supplied state.
[[nodiscard]] CollisionRetryPlacementUpdate
select_collision_retry_placement_direction(
    CollisionRetryPlacementState state,
    std::uint8_t random_byte) noexcept;

} // namespace devils_crush::native_runtime
