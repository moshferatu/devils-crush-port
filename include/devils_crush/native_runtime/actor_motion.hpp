// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// The shared table-actor motion model. Positions retain the game's 8-bit
// fractional precision while the public API deals in world coordinates rather
// than work-RAM pages or a ROM dispatcher.
struct Fixed24_8 {
    std::int32_t raw = 0;

    [[nodiscard]] constexpr std::int32_t whole_pixels() const noexcept
    {
        // C++ division truncates toward zero, whereas a signed fixed-point
        // coordinate must preserve any negative fractional departure.
        return raw >= 0 ? raw / 256
                        : static_cast<std::int32_t>(
                              -((-static_cast<std::int64_t>(raw) + 255) / 256));
    }

    [[nodiscard]] bool operator==(const Fixed24_8&) const = default;
};

struct ActorMotionState {
    Fixed24_8 position_x;
    Fixed24_8 position_y;
    // Signed 8.8 pixels per update.
    std::int16_t velocity_x = 0;
    std::int16_t velocity_y = 0;
    // Coordinates at which automatic velocity steering changes direction.
    std::int32_t target_x = 0;
    std::int32_t target_y = 0;
    // The original behavior uses an unsigned sub-pixel adjustment (at most
    // one whole pixel per update) for both axes.
    std::uint8_t velocity_adjustment_x = 0;
    std::uint8_t velocity_adjustment_y = 0;
    bool integrate_x = false;
    bool integrate_y = false;
    bool steer_x = false;
    bool steer_y = false;

    [[nodiscard]] bool operator==(const ActorMotionState&) const = default;
};

struct PlayfieldBounds {
    std::int32_t width = 320;
    std::int32_t height = 720;

    [[nodiscard]] bool operator==(const PlayfieldBounds&) const = default;
};

struct ActorMotionStep {
    ActorMotionState state;
    // True when an enabled position integration leaves the playfield.
    bool left_playfield = false;

    [[nodiscard]] bool operator==(const ActorMotionStep&) const = default;
};

// Advances a non-ball actor by one shared motion update. Position integration
// happens before velocity steering, matching the visible game behavior.
[[nodiscard]] ActorMotionStep advance_actor_motion(
    ActorMotionState state,
    PlayfieldBounds playfield = {}) noexcept;

} // namespace devils_crush::native_runtime
