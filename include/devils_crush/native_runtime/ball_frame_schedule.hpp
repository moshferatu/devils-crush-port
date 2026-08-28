// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace devils_crush::native_runtime {

// The slot-zero coroutine does not run normal DC95 physics on every emulator
// frame.  These named owners are the recovered frame-boundary choices.  The
// table-control and off-table paths remain separate owners until their state
// transitions are ported; they must not be treated as a failed normal-motion
// prediction.
enum class BallFrameOwner {
    normal_motion,
    // C6C2 is the initial entry into the native C6/C7 substep coroutine. It
    // can branch before its first DC95 handoff, so it needs its own readable
    // transition owner rather than being treated as a normal-motion epoch.
    initial_substep_transition,
    table_control_wait,
    off_table_transition,
    unrecognized,
};

// C6C2 finishes its initial collision-loop work with an unsigned horizontal
// coordinate gate.  The two outcomes are deliberately named after their
// observable scheduler effects: the hold returns to the current caller, while
// the normal path clears its collision selectors, installs C721, and enters a
// new DC95 split.  Executing those actions remains the initial-transition
// owner's job; this capture only makes the decision available to it.
enum class BallInitialTransitionDisposition {
    returns_to_caller,
    schedules_normal_motion,
};

struct BallInitialTransition {
    std::uint16_t horizontal_pixels = 0U;
    BallInitialTransitionDisposition disposition =
        BallInitialTransitionDisposition::returns_to_caller;

    [[nodiscard]] bool operator==(const BallInitialTransition&) const = default;
};

// C6C2 clears these two selectors before it installs C721. They name the
// ball's actor-local collision-shape selection without retaining work-RAM
// offsets in the native scheduler state.
struct BallCollisionShapeSelection {
    std::uint8_t directory_index = 0U;
    std::uint8_t phase = 0U;

    [[nodiscard]] bool operator==(const BallCollisionShapeSelection&) const
        = default;
};

// Recovers C6C2's C708 gate from the native ball state.  The source compares
// the unsigned whole-pixel word against $00F0, so signed world-coordinate
// arithmetic must not be substituted here.
[[nodiscard]] BallInitialTransition classify_ball_initial_transition(
    const BallState& ball) noexcept;

struct BallFrameSchedule {
    BallFrameOwner owner = BallFrameOwner::unrecognized;
    // This remains at the capture boundary only. It lets the migration
    // observer identify an unrecognized source continuation without leaking
    // ROM addresses into the runtime's scheduling API.
    std::uint16_t source_handler_address = 0U;
    // Present only for the C6/C7 owners when their DC95-prepared cursor is
    // coherent. This lets a readable scheduler resume an in-flight physics
    // epoch without manufacturing a new split at a host-frame boundary.
    std::optional<BallSubstepProgress> substep_progress;
    // Present only when a C6C2 capture has already completed its prepared
    // cursor. It retains the post-loop scheduler decision without exposing
    // its source bytes to native physics. A moving cursor must finish before
    // the source evaluates this gate.
    std::optional<BallInitialTransition> initial_transition;
    // Captured from the slot-zero actor and subsequently owned by the native
    // C6C2/C721 scheduler transition.
    std::optional<BallCollisionShapeSelection> collision_shape;

    [[nodiscard]] bool runs_normal_motion() const noexcept
    {
        return owner == BallFrameOwner::normal_motion;
    }

    [[nodiscard]] bool operator==(const BallFrameSchedule&) const = default;
};

[[nodiscard]] BallFrameSchedule capture_ball_frame_schedule(
    const GameStateView& state) noexcept;

// Applies C6C2's post-DD09 scheduler action to a typed schedule. The caller
// supplies the fresh DC95 cursor made immediately before C708. The low-side
// transition clears the shape selection and installs normal C721 motion; the
// high-side transition preserves the selection and returns to the initial
// owner. A schedule without a captured shape selection is not safe to mutate.
[[nodiscard]] std::optional<BallFrameSchedule>
advance_initial_ball_frame_schedule(
    BallFrameSchedule schedule,
    BallInitialTransition transition,
    BallSubstepProgress fresh_progress) noexcept;

[[nodiscard]] std::string_view describe_ball_frame_owner(
    BallFrameOwner owner) noexcept;

} // namespace devils_crush::native_runtime
