// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace devils_crush::input {

// The ball is actor slot zero. Its position is signed 16.8 in
// `$3100/$3180/$3200` and `$3280/$3300/$3380`, and its velocity signed 8.8 in
// `$3400/$3480` and `$3500/$3580`, a frame's worth of pixels.
inline constexpr std::size_t kDragBallXFraction = 0x1100U;
inline constexpr std::size_t kDragBallXLow = 0x1180U;
inline constexpr std::size_t kDragBallXHigh = 0x1200U;
inline constexpr std::size_t kDragBallYFraction = 0x1280U;
inline constexpr std::size_t kDragBallYLow = 0x1300U;
inline constexpr std::size_t kDragBallYHigh = 0x1380U;
inline constexpr std::size_t kDragBallVelocityXLow = 0x1400U;
inline constexpr std::size_t kDragBallVelocityXHigh = 0x1480U;
inline constexpr std::size_t kDragBallVelocityYLow = 0x1500U;
inline constexpr std::size_t kDragBallVelocityYHigh = 0x1580U;
// The signed 8.8 step of each collision check in the ball's coming frame,
// `$B9:$BA` across and `$BB:$BC` down. `01:DC95` works them out from the
// velocity at the end of the frame before.
inline constexpr std::size_t kDragBallStepFirst = 0x00B9U;
inline constexpr std::size_t kDragBallStepLast = 0x00BCU;
// The routine the actor scheduler at `01:C510` runs for the ball each frame,
// out of `$3F00/$3F80`: `01:C721` is the ball in play, its motion, collisions
// and gravity.
inline constexpr std::size_t kDragBallHandlerLow = 0x1F00U;
inline constexpr std::size_t kDragBallHandlerHigh = 0x1F80U;
inline constexpr std::uint16_t kDragBallInPlayHandler = 0xC721U;
// `01:C6C2` is the same for a ball the Devil Shooter still has, in its lane:
// it leaves out the drain, and installs `01:C721` itself once the ball is
// left of X `$F0`, out of the lane.
inline constexpr std::uint16_t kDragBallInShooterHandler = 0xC6C2U;
// The scheduler's own closing `RTS`, in the bank it dispatches from. A ball
// sent there is still filed to be drawn (`01:C520`) and does nothing else.
inline constexpr std::uint16_t kDragBallHeldHandler = 0xC52BU;
// `$53`: bit 1 is a drained ball, bit 3 a ball the Devil Shooter still has,
// bit 4 a bonus stage. Bit 3 comes down when the gate at the top of the lane
// closes behind the ball (`03:645D`), which the ball's touch sets off, and it
// stays up through a bonus stage started from the shooter.
inline constexpr std::size_t kDragSessionFlags = 0x0053U;
// The ball's lane is right of X `$F0`, where `01:C708` takes a ball to have
// left it. The gate at its top is fixed slot `$0F`: type `$83`, which a ball
// passes through, while the shooter's bit is up, and `$82`, shut, once it has
// closed behind the ball; `03:63F1` is its routine while it waits.
inline constexpr int kDragLaneLeft = 0xF0;
inline constexpr std::size_t kDragLaneGateType = 0x100FU;
inline constexpr std::size_t kDragLaneGateHandlerLow = 0x1F0FU;
inline constexpr std::size_t kDragLaneGateHandlerHigh = 0x1F8FU;
inline constexpr std::uint16_t kDragLaneGateWaiting = 0x63F1U;
// The ball's sprite attribute, `$3D00`: bit 7 puts it in front of the
// table's background. The ball's own routine keeps it as the ball moves, and
// it is clear while the ball sits in the Devil Shooter.
inline constexpr std::size_t kDragBallAttribute = 0x1D00U;
// The camera's top row in the table's world, `$A9:$AA`.
inline constexpr std::size_t kDragCameraLow = 0x00A9U;
inline constexpr std::size_t kDragCameraHigh = 0x00AAU;
// The camera rests with `$A9:$AA` between these, the table's ends
// (`00:F43D` and `00:F48D` hold it there).
inline constexpr int kDragCameraTop = 0;
inline constexpr int kDragCameraBottom = 480;
// The ball is kept this many rows inside the camera's 240, and the camera is
// brought along while the pointer is further out than the second.
inline constexpr int kDragCameraMargin = 16;
inline constexpr int kDragCameraScrollMargin = 24;
// No faster than the Devil Shooter's hardest shot, eight pixels a frame:
// what the collision loop's four checks are made for, and the two tile rows
// `00:F261` can bring into view in a frame.
inline constexpr int kDragMaximumSpeed = 0x800;

[[nodiscard]] inline std::uint16_t dragged_ball_handler(
    const std::span<const std::byte> ram) noexcept
{
    return static_cast<std::uint16_t>(
        std::to_integer<unsigned>(ram[kDragBallHandlerLow])
        | (std::to_integer<unsigned>(ram[kDragBallHandlerHigh]) << 8U));
}

// Whether there is a ball to carry: a live table or bonus stage, not the
// demo's, with the ball drawn (`$3080` bit 5 clear), on the table (`$3C80`
// bit 0 clear) and in play, in the Devil Shooter or its lane, or already
// held. A ball still being served is in none of those routines.
[[nodiscard]] inline bool ball_can_be_dragged(
    const std::span<const std::byte> ram) noexcept
{
    if (ram.size() != 8192U) {
        return false;
    }
    const auto byte_at = [&ram](const std::size_t offset) {
        return std::to_integer<unsigned>(ram[offset]);
    };
    const auto session = byte_at(kDragSessionFlags);
    const auto handler = dragged_ball_handler(ram);
    return byte_at(0x254U) == 0U && byte_at(0x54U) <= 1U
        && byte_at(0xD52U) != 0U && (session & 0x02U) == 0U
        && byte_at(0x1000U) != 0U && (byte_at(0x1080U) & 0x20U) == 0U
        && (byte_at(0x1C80U) & 0x01U) == 0U
        && (handler == kDragBallInPlayHandler
            || handler == kDragBallInShooterHandler
            || handler == kDragBallHeldHandler);
}

// Gives a held ball back to the game, with the velocity it was last carried
// at. True when there was one. For every frame the ball is not being carried:
// it also lets go of a ball that a restored quick state was holding.
//
// A ball the Devil Shooter still has, which is one that has not been carried
// out of its lane, goes back to the shooter's routine.
inline bool release_dragged_ball(const std::span<std::byte> ram) noexcept
{
    if (ram.size() != 8192U
        || dragged_ball_handler(ram) != kDragBallHeldHandler) {
        return false;
    }
    const auto session = std::to_integer<unsigned>(ram[kDragSessionFlags]);
    const auto handler = (session & 0x18U) == 0x08U
        ? kDragBallInShooterHandler
        : kDragBallInPlayHandler;
    ram[kDragBallHandlerLow] = static_cast<std::byte>(handler & 0xFFU);
    ram[kDragBallHandlerHigh] = static_cast<std::byte>(handler >> 8U);
    return true;
}

// The cheat that carries the ball: puts it at X and Y in the game's own actor
// coordinates and takes it out of play while it is there, so that nothing on
// the table knocks it out of the hand: the scheduler is pointed at an `RTS`
// in place of the ball's routine. It keeps the velocity that took it there,
// which is what it leaves with a frame after `release_dragged_ball`; a jump
// of more than 32 pixels is a move rather than a throw, and leaves it at
// rest.
//
// On the table the camera has to come too. `00:F261` only moves it by the
// ball's own vertical velocity, while the ball is in the top 64 or bottom 48
// of its 240 rows and heading out, so it never catches up with a ball that
// has simply appeared somewhere else, and what is off camera is neither drawn
// nor run. So the ball is held inside the camera's rows, and while the place
// asked for is at their edge or past it the ball is given the velocity that
// scrolls the camera toward it, and rides the edge there.
//
// False leaves every byte as it was.
[[nodiscard]] inline bool drag_ball(
    const std::span<std::byte> ram,
    const double x,
    double y) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y)
        || !ball_can_be_dragged(ram)) {
        return false;
    }
    const auto byte_at = [&ram](const std::size_t offset) {
        return std::to_integer<unsigned>(ram[offset]);
    };
    const auto read = [&byte_at](const std::size_t fraction,
                          const std::size_t low,
                          const std::size_t high) {
        return static_cast<int>(
            byte_at(fraction) | (byte_at(low) << 8U) | (byte_at(high) << 16U));
    };
    const auto put = [&ram](const std::size_t offset, const int value) {
        ram[offset] = static_cast<std::byte>(value & 0xFF);
    };
    const auto velocity = [](const int moved) {
        return std::abs(moved) > 32 * 256
            ? 0
            : std::clamp(moved, -kDragMaximumSpeed, kDragMaximumSpeed);
    };

    // Zero, or the vertical velocity that scrolls the camera.
    int scroll = 0;
    if ((byte_at(kDragSessionFlags) & 0x10U) == 0U) {
        const int camera = static_cast<std::int16_t>(
            byte_at(kDragCameraLow) | (byte_at(kDragCameraHigh) << 8U));
        const double above = camera + kDragCameraScrollMargin - y;
        const double below = y - (camera + 240 - kDragCameraScrollMargin);
        if (above > 0.0 && camera > kDragCameraTop) {
            scroll = -static_cast<int>(std::lround(
                std::min({above, 8.0, double(camera - kDragCameraTop)})
                * 256.0));
        } else if (below > 0.0 && camera < kDragCameraBottom) {
            scroll = static_cast<int>(std::lround(
                std::min({below, 8.0, double(kDragCameraBottom - camera)})
                * 256.0));
        }
        y = std::clamp(
            y,
            double(camera + kDragCameraMargin),
            double(camera + 240 - kDragCameraMargin));
    }

    const int to_x = static_cast<int>(
        std::lround(std::clamp(x, 0.0, 1023.0) * 256.0));
    const int to_y = static_cast<int>(
        std::lround(std::clamp(y, 0.0, 1023.0) * 256.0));
    const int velocity_x = velocity(
        to_x - read(kDragBallXFraction, kDragBallXLow, kDragBallXHigh));
    const int velocity_y = scroll != 0
        ? scroll
        : velocity(
              to_y - read(kDragBallYFraction, kDragBallYLow, kDragBallYHigh));
    put(kDragBallXFraction, to_x);
    put(kDragBallXLow, to_x >> 8);
    put(kDragBallXHigh, to_x >> 16);
    put(kDragBallYFraction, to_y);
    put(kDragBallYLow, to_y >> 8);
    put(kDragBallYHigh, to_y >> 16);
    put(kDragBallVelocityXLow, velocity_x);
    put(kDragBallVelocityXHigh, velocity_x >> 8);
    put(kDragBallVelocityYLow, velocity_y);
    put(kDragBallVelocityYHigh, velocity_y >> 8);
    // The steps left over from the ball's last frame in play would be its
    // first on being let go. Without them it stays where it was put for that
    // frame, and leaves with this velocity in the next.
    for (auto offset = kDragBallStepFirst; offset <= kDragBallStepLast;
         ++offset) {
        put(offset, 0);
    }
    // In the hand it is over everything, whatever it was under when taken.
    put(kDragBallAttribute, 0x80);
    // Carried out of its lane, the Devil Shooter's ball is launched: the
    // shooter's bit comes down and the lane's gate shuts, as `03:645D` has
    // them once the ball has gone through. A gate already closing does both
    // for itself.
    if ((byte_at(kDragSessionFlags) & 0x18U) == 0x08U
        && to_x < kDragLaneLeft * 256) {
        put(kDragSessionFlags,
            static_cast<int>(byte_at(kDragSessionFlags) & ~0x08U));
        if (byte_at(kDragLaneGateType) == 0x83U
            && (byte_at(kDragLaneGateHandlerLow)
                   | (byte_at(kDragLaneGateHandlerHigh) << 8U))
                == kDragLaneGateWaiting) {
            put(kDragLaneGateType, 0x82);
        }
    }
    put(kDragBallHandlerLow, kDragBallHeldHandler & 0xFF);
    put(kDragBallHandlerHigh, kDragBallHeldHandler >> 8);
    return true;
}

} // namespace devils_crush::input
