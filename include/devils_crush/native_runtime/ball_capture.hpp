// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/ball_physics.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// Complete native ball state reconstructed from its dedicated slot-zero
// record.  The source observation remains available to the migration layer
// for comparison without allowing raw work-RAM addresses into physics code.
struct ActiveBallCapture {
    GameStateView::BallSlot source;
    BallState state;

    [[nodiscard]] bool operator==(const ActiveBallCapture&) const = default;
};

// The CDF8 retry needs both the live ball at the confirmed contact and the
// pre-integration position cached by F1B4. This capture makes that pairing
// explicit before a later actor-contact retry owner starts the placement loop.
struct CollisionRetryBallCapture {
    ActiveBallCapture impact_ball;
    BallState previous_position;

    [[nodiscard]] bool operator==(const CollisionRetryBallCapture&) const = default;

    // CEB5 preserves the parent collision scan's whole-pixel Y high byte
    // while CDF8 changes only the retry candidate's low byte. The value comes
    // from the confirmed live impact position, not the restored F1B4 cache.
    [[nodiscard]] std::uint8_t parent_vertical_high_byte() const noexcept;
};

// Captures a live ball only.  Inactive slot-zero records are not valid native
// physics seeds; session/serve behavior decides when to allocate a new ball.
[[nodiscard]] std::optional<ActiveBallCapture> capture_active_ball(
    const GameStateView& game_state) noexcept;

[[nodiscard]] std::optional<CollisionRetryBallCapture>
capture_collision_retry_ball(const GameStateView& game_state) noexcept;

} // namespace devils_crush::native_runtime
