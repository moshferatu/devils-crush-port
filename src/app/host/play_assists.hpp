// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

// At this point the scripted power-on route has a live ball session ($53.3).
// Later, frame 1300 has already entered the lost-ball state ($53.1): requesting
// a bonus there loads the right layout, but the cartridge immediately resolves
// it as a loss. Requesting the transition from this live-session boundary
// preserves the same entry contract as a table feature.
constexpr int kBonusStageLaunchFrame = 1200;
constexpr int kBonusStageSmokeFrames = 240;
constexpr std::size_t kBonusStageSessionFlagsOffset = 0x0053U;
constexpr std::size_t kBonusStageIdOffset = 0x0055U;
constexpr std::size_t kBonusStageBallFlagsOffset = 0x1080U;
constexpr std::size_t kBonusStageBallStateOffset = 0x1C80U;
constexpr std::uint8_t kBonusStageLiveSessionBit = 0x08U;
constexpr std::uint8_t kBonusStageRequestBit = 0x10U;
constexpr std::uint8_t kBonusStageBallHeldBit = 0x20U;
constexpr std::uint8_t kBonusStageBallOffTableBit = 0x01U;

[[nodiscard]] bool prepare_bonus_stage_launch(
    PceRuntime& runtime,
    const std::uint8_t stage);

[[nodiscard]] bool service_synthetic_bonus_stage_return(
    PceRuntime& runtime,
    bool& return_pending);

void keep_fixed_actors_simulating(
    devils_crush::emulation::PceRuntime& runtime);

[[nodiscard]] bool apply_flipper_inputs(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::cabinet::FlipperInputs& flippers);

// The ball's sprite sits six pixels up and to the left of its actor position,
// less the `$20` the hardware takes off a sprite's X: bank `$08`'s descriptor
// for it, which is read rather than assumed wherever the ROM gives one.
constexpr int kBallSpriteOffsetX = -38;
constexpr int kBallSpriteOffsetY = -6;
constexpr int kBallSpriteHalf = 8;
// The stitched table's row zero is the playfield's first: table-world row
// `kTableWorldTop`, under the thirty lines of score band a screen carries
// above its playfield.
constexpr int kStitchedTableSpriteTop =
    devils_crush::cabinet::kTableWorldTop + 30;

void release_dragged_ball(devils_crush::emulation::PceRuntime& runtime);

void face_waiting_dragon_heads(devils_crush::emulation::PceRuntime& runtime);

bool drag_ball_to_picture_point(
    devils_crush::emulation::PceRuntime& runtime,
    const std::span<const std::byte> rom,
    SDL_FPoint point,
    const bool stitched_table);

// `02:52CA SBC #$09`, the ball's offset above the shooter it is sitting on,
// and how far it may be from there before it is taken to be something other
// than a ball sitting in the shooter.
constexpr int kBallRidesAbove = 9;
constexpr int kBallRideSlack = 16;

[[nodiscard]] bool apply_plunger_draw(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::input::PlungerSample& sample);

[[nodiscard]] bool install_shooter_flipper_gate(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::cabinet::FlipperInputs& flippers);

[[nodiscard]] bool install_nudge_multiplier(PceRuntime& runtime, const double factor);

} // namespace devils_crush::app::host
