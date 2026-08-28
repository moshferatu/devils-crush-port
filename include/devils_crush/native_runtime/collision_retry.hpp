// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/ball_physics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// The contact-retry table selects from the signs of the incoming motion. The
// names describe world axes rather than the original temporary-byte encoding.
enum class CollisionRetryDirection : std::uint8_t {
    positive_x_positive_y,
    positive_x_negative_y,
    negative_x_positive_y,
    negative_x_negative_y,
};

struct CollisionRetryOffset {
    std::int16_t horizontal_pixels = 0;
    std::int16_t vertical_pixels = 0;

    [[nodiscard]] bool operator==(const CollisionRetryOffset&) const = default;
};

// ROM-derived retry offsets for each incoming-motion direction. The table is
// intentionally data, not a hidden response policy: collision geometry owns
// which table applies to its current table and stage.
struct CollisionRetryOffsets {
    std::array<CollisionRetryOffset, 4U> by_direction{};

    [[nodiscard]] bool operator==(const CollisionRetryOffsets&) const = default;
};

// CDF8 makes up to three cumulative placement moves before changing its
// motion quadrant. The ROM asset decoder owns the table bytes; this type owns
// the table's gameplay shape.
struct CollisionRetryOffsetAttempts {
    static constexpr std::size_t kAttemptsPerDirection = 3U;

    std::array<std::array<CollisionRetryOffset, kAttemptsPerDirection>, 4U>
        by_direction{};

    [[nodiscard]] bool operator==(const CollisionRetryOffsetAttempts&) const
        = default;
};

struct CollisionRetryStep {
    BallState impact_ball;
    BallState state;
    CollisionRetryDirection direction =
        CollisionRetryDirection::positive_x_positive_y;
    CollisionRetryOffset offset;
    // The original vertical-coordinate addition leaves this carry live for
    // the immediately following retry probe.
    bool vertical_integer_addition_carried = false;

    [[nodiscard]] bool operator==(const CollisionRetryStep&) const = default;
};

// The deterministic inner CDF8 path for one direction. Each step starts from
// the preceding candidate, so its offset is cumulative rather than a new
// position relative to restored_ball.
struct CollisionRetryAttemptSequence {
    BallState restored_ball;
    CollisionRetryDirection direction =
        CollisionRetryDirection::positive_x_positive_y;
    std::array<CollisionRetryStep,
               CollisionRetryOffsetAttempts::kAttemptsPerDirection>
        attempts{};

    [[nodiscard]] bool operator==(const CollisionRetryAttemptSequence&) const
        = default;
};

// CDF8 asks the shared bound-four RNG scaler for another direction only after
// the current quadrant's three moves all collide. Repeated directions retry
// the selector; a completed high-nibble mask enters CE98 instead.
enum class CollisionRetryDirectionSelectionDisposition : std::uint8_t {
    selected,
    already_attempted,
    fallback_required,
};

struct CollisionRetryDirectionSelection {
    std::optional<CollisionRetryDirection> direction;
    std::uint8_t direction_mask = 0;
    std::uint8_t attempted_direction_flags_after = 0;
    CollisionRetryDirectionSelectionDisposition disposition =
        CollisionRetryDirectionSelectionDisposition::fallback_required;

    [[nodiscard]] bool operator==(const CollisionRetryDirectionSelection&)
        const = default;
};

// After all retry quadrants are exhausted, CDF8 subtracts one whole vertical
// pixel and re-probes up to seven times before restarting this sequence. Each
// element is one cumulative CE98 fallback move.
struct CollisionRetryVerticalFallbackSequence {
    static constexpr std::size_t kStepsPerPass = 7U;

    std::array<CollisionRetryStep, kStepsPerPass> attempts{};

    [[nodiscard]] bool operator==(const CollisionRetryVerticalFallbackSequence&)
        const = default;
};

// Classifies the four retry directions from signed 8.8 ball motion. A zero
// component takes the original non-negative direction.
[[nodiscard]] CollisionRetryDirection classify_collision_retry_direction(
    std::int16_t velocity_x,
    std::int16_t velocity_y) noexcept;

// Retries a rejected contact from the pre-impact position. It retains the
// impact state's velocity and flags, then applies the selected whole-pixel
// offset without disturbing either coordinate's fractional byte. The returned
// carry is part of the observable retry state consumed by the next probe.
[[nodiscard]] CollisionRetryStep retry_ball_collision(
    BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsets& offsets) noexcept;

// Builds CDF8's first three same-quadrant placement candidates. A caller
// probes each returned state in order and must keep the first clear state;
// changing direction after all three collide remains a separate random-route
// boundary.
[[nodiscard]] CollisionRetryAttemptSequence
make_collision_retry_attempt_sequence(
    BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsetAttempts& offsets) noexcept;

// Builds CDF8's same-quadrant sequence for a caller-selected direction. This
// is the entry used after the shared RNG selector chooses an untried quadrant.
[[nodiscard]] CollisionRetryAttemptSequence
make_collision_retry_attempt_sequence_for_direction(
    BallState impact_ball,
    const BallState& previous_position,
    const CollisionRetryOffsetAttempts& offsets,
    CollisionRetryDirection direction) noexcept;

// Applies CDF8's bound-four RNG direction selection to an already-attempted
// high-nibble mask. The caller supplies the native RNG byte so this pure
// placement module does not own the global random stream.
[[nodiscard]] CollisionRetryDirectionSelection
select_collision_retry_direction(
    std::uint8_t random_byte,
    std::uint8_t attempted_direction_flags) noexcept;

// Builds one CDF8 fallback pass from the last rejected placement. If every
// returned state collides, the original restarts another pass from the final
// state; that unbounded retry policy remains with the caller.
[[nodiscard]] CollisionRetryVerticalFallbackSequence
make_collision_retry_vertical_fallback_sequence(
    BallState impact_ball,
    BallState rejected_candidate) noexcept;

} // namespace devils_crush::native_runtime
