// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "checks_launch.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

[[nodiscard]] int run_ball_drag_check(
    devils_crush::emulation::PceRuntime& runtime,
    const std::vector<std::byte>& rom,
    const CommandLine* const native_registry);

// How long the flippers stay dead after a launch, and what is holding them
// there.
//
// The Devil Shooter serves at the bottom right and fires the ball up the right
// lane; the camera follows the ball. `00:F4FC` clears `$3080,X` bit 7 for
// every slot outside [camera - 48, camera + 240), and the actor scheduler at
// `01:C515` skips a slot whose bit 7 is clear while bit 1 is set, so the
// flipper pieces at `$04-$09` stop being dispatched the moment the camera
// leaves them behind. The button still reaches `$2CEE`; there is simply
// nothing running to read it.
//
// The probe launches from the census route, then pulses a left-flipper
// direction and reports when the pieces first answer it -- once with the cull
// as the cartridge wrote it, once with the cabinet's named-slot lift applied.
struct LaunchFlipperSample {
    int frame = 0;
    unsigned session = 0;
    unsigned input = 0;
    unsigned flags = 0;
    unsigned mask = 0;
    unsigned phase = 0;
    unsigned dispatch = 0;
    int ball_y = 0;
    int camera_y = 0;
};

struct LaunchFlipperPass {
    int launched_at = 0;
    int first_dispatch = 0;
    int first_move = 0;
    std::size_t held_frames = 0;
    std::size_t dispatched_frames = 0;
    std::size_t moves = 0;
    std::vector<LaunchFlipperSample> timeline;
};

[[nodiscard]] int run_launch_flipper_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const CommandLine* const native_registry,
    const std::vector<std::byte>& rom,
    const bool uncull,
    const bool restore_input,
    LaunchFlipperPass& pass);

void report_nudge_effect(
    const NudgePassResult& without,
    const NudgePassResult& with);

[[nodiscard]] int run_flipper_trace(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& state_path,
    const bool uncull);

} // namespace devils_crush::app::host
