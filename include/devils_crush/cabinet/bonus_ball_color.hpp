// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "devils_crush/emulation/pce_runtime.hpp"

namespace devils_crush::cabinet {
// A perfect bonus result starts the star controller (`03:7CA7`, or its bank-5
// copy at `05:696A`). For about 60 seconds it keeps the score multiplier in
// `$2D38` and paints the ball in the bonus palette for level `$CD`. While the
// ball is lost (`$53.1`) or in the Devil Shooter (`$53.3`), the original paints
// it silver instead, and the bonus color comes back only after the launch.
//
// The cabinet keeps the bonus color on the ball for that whole time. Only the
// ball's sprite palette changes; the timer, multiplier, and session are left
// as they are.

// Repaints the ball after the star's palette check, if that check made it
// silver. Returns whether anything changed.
bool keep_bonus_ball_color(std::span<std::byte> ram) noexcept;

// Observes the two places each copy of the star controller has finished
// choosing the ball's palette.
[[nodiscard]] bool install_bonus_ball_color(emulation::PceRuntime& runtime);
}
