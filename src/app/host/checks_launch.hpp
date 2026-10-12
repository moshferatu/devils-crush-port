// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

[[nodiscard]] int run_drain_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path,
    const bool uncull,
    const CommandLine* const native_registry);

// Reads the flipper pieces' own state straight out of work RAM while the
// buttons are worked, so the question "why doesn't that pair move?" is
// answered by the ROM's fields rather than by what reached the screen.
//
// 01:D466 is the per-piece input helper and it opens with one gate:
// LDA $2CEE; AND $3B80,X -- a zero intersection returns immediately. If a
// piece never moves, either its mask misses the held buttons, or its
// coroutine pointer at $3F80:$3F00,X is parked somewhere that never consults
// the gate at all. Both are visible here.
// Presses Button II against the quick state and reports every gate the ROM's
// nudge routine consults, so "nudging does nothing" can be localised rather
// than guessed at.
//
// `01:C3CB` reads Button II from the per-player edge latch `$2018,Y`, not from
// the held-input copy `$2CEE` the flippers use, and it is gated on the session
// flags at `$53`, the tilt lock at `$2CC7`, the cooldown timer at `$2CC6` and
// the nudge phase at `$2CC4`. When it does fire, `01:C457` adds a halved
// signed vector to slot-0 motion at `$3500/$3580`.
// The gate opening, and the button reaching it, say nothing about whether a
// player can feel the result. Running the same route twice -- once with the
// nudge taps and once without -- and diffing the ball's own coordinates
// measures the effect rather than the mechanism.
struct NudgePassResult {
    std::vector<int> ball_x;
    std::vector<int> ball_y;
    std::size_t in_play_frames = 0;
};

[[nodiscard]] int run_nudge_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path,
    const CommandLine* const native_registry,
    const bool from_boot,
    const bool nudge_enabled,
    NudgePassResult& pass);

[[nodiscard]] int run_launch_check(
    devils_crush::emulation::PceRuntime& runtime,
    const Button ordinary_input,
    const std::string_view label);

[[nodiscard]] int run_plunger_check(
    devils_crush::emulation::PceRuntime& runtime);

} // namespace devils_crush::app::host
