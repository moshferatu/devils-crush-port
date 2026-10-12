// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace devils_crush::cabinet {

// Bonus stage 1's five dragon heads are type-`$C2` actors whose routine at
// `04:47D5` turns each to face the ball every frame: `01:DAF8` gives the
// direction from the head to actor zero as one of sixteen, and half of that
// picks the pose out of the eight at `04:482E`.
//
// The stage's layout leaves them in pose 0, facing right, and their routine
// does not run until the stage has faded in, a second later. The game's heads
// therefore fade in facing right and snap to the ball on the frame the fade
// ends. `face_waiting_dragon_heads` is the port's one departure from that: it
// gives the heads the pose their routine is about to, so they fade in already
// facing the ball.
inline constexpr std::uint8_t kDragonHeadsBonusStage = 1U;
inline constexpr std::uint8_t kDragonHeadType = 0xC2U;
// A head's routine as the scheduler has it: `04:47B5` and `04:47C7` set it
// up, a frame each, and `04:47D5` is the one that faces the ball.
inline constexpr std::uint16_t kDragonHeadFirstRoutine = 0x47B5U;
inline constexpr std::uint16_t kDragonHeadTimerRoutine = 0x47C7U;
inline constexpr std::uint16_t kDragonHeadFacingRoutine = 0x47D5U;
// The stage's sprites are built once, on its second frame, and stand through
// the fade. The ball is only put where it is served from on that same frame
// (`04:513E` takes it to the serving actor, laid out at this spot), so a head
// that has not reached its facing routine is faced here, where the ball is
// about to be.
inline constexpr int kDragonHeadsServeX = 62;
inline constexpr int kDragonHeadsServeY = 200;
inline constexpr std::array<std::uint8_t, 8U> kDragonHeadPoses{
    0U, 1U, 2U, 3U, 4U, 3U, 3U, 1U};

// `$53` bit 4 is a bonus stage and `$55` which one.
inline constexpr std::size_t kDragonHeadsSessionFlags = 0x0053U;
inline constexpr std::size_t kDragonHeadsStage = 0x0055U;
// Per actor: type `$3000`, position in whole pixels `$3180:$3200` across and
// `$3300:$3380` down, pose `$3980`, and the routine the scheduler will run
// for it next, `$3F00:$3F80`.
inline constexpr std::size_t kDragonHeadActorType = 0x1000U;
inline constexpr std::size_t kDragonHeadActorXLow = 0x1180U;
inline constexpr std::size_t kDragonHeadActorXHigh = 0x1200U;
inline constexpr std::size_t kDragonHeadActorYLow = 0x1300U;
inline constexpr std::size_t kDragonHeadActorYHigh = 0x1380U;
inline constexpr std::size_t kDragonHeadActorPose = 0x1980U;
inline constexpr std::size_t kDragonHeadActorRoutineLow = 0x1F00U;
inline constexpr std::size_t kDragonHeadActorRoutineHigh = 0x1F80U;
inline constexpr std::size_t kDragonHeadActorSlots = 0x80U;

// `01:DAF8`: the direction of (`across`, `down`), 0 to 15, 0 being right and
// 4 down. Each distance is taken as a byte the way the cartridge takes it:
// 255 when it is further than that, and nothing at exactly 256 back.
[[nodiscard]] constexpr std::uint8_t dragon_head_direction(
    const int across,
    const int down) noexcept
{
    constexpr std::array<std::uint8_t, 24U> directions{
        0x00U, 0x00U, 0x08U, 0x08U, 0x04U, 0x0CU, 0x04U, 0x0CU,
        0x01U, 0x0FU, 0x07U, 0x09U, 0x03U, 0x0DU, 0x05U, 0x0BU,
        0x02U, 0x0EU, 0x06U, 0x0AU, 0x02U, 0x0EU, 0x06U, 0x0AU,
    };
    const auto distance = [](const int difference) {
        if (difference >= 0) {
            return difference > 0xFF ? 0xFF : difference;
        }
        return difference < -0x100 ? 0xFF : (-difference & 0xFF);
    };
    int larger = distance(across);
    int smaller = distance(down);
    unsigned bits = (across < 0 ? 0x02U : 0U) | (down < 0 ? 0x01U : 0U);
    if (larger < smaller) {
        const int swapped = larger;
        larger = smaller;
        smaller = swapped;
        bits |= 0x04U;
    }
    if (smaller * 3 >= larger * 2) {
        bits |= 0x10U;
    } else if (smaller * 12 >= larger * 2) {
        bits |= 0x08U;
    }
    return directions[bits];
}

// Gives every dragon head the pose its facing routine would: the one facing
// the ball where it is now, or where it will be served from for a head still
// being set up. True when a pose changed.
// Only the pose byte is written, which nothing but the sprite builder reads
// before the routine writes it again: every byte the game keeps is as it
// would have been, frame for frame.
inline bool face_waiting_dragon_heads(const std::span<std::byte> ram) noexcept
{
    if (ram.size() != 8192U) {
        return false;
    }
    const auto byte_at = [&ram](const std::size_t offset) {
        return std::to_integer<unsigned>(ram[offset]);
    };
    if ((byte_at(kDragonHeadsSessionFlags) & 0x10U) == 0U
        || byte_at(kDragonHeadsStage) != kDragonHeadsBonusStage) {
        return false;
    }
    const auto position = [&byte_at](const std::size_t low,
                              const std::size_t high,
                              const std::size_t slot) {
        return static_cast<int>(static_cast<std::int16_t>(
            byte_at(low + slot) | (byte_at(high + slot) << 8U)));
    };
    bool changed = false;
    for (std::size_t slot = 1U; slot < kDragonHeadActorSlots; ++slot) {
        const auto routine = byte_at(kDragonHeadActorRoutineLow + slot)
            | (byte_at(kDragonHeadActorRoutineHigh + slot) << 8U);
        const bool facing = routine == kDragonHeadFacingRoutine;
        if (byte_at(kDragonHeadActorType + slot) != kDragonHeadType
            || (!facing && routine != kDragonHeadFirstRoutine
                && routine != kDragonHeadTimerRoutine)) {
            continue;
        }
        const int ball_x = facing
            ? position(kDragonHeadActorXLow, kDragonHeadActorXHigh, 0U)
            : kDragonHeadsServeX;
        const int ball_y = facing
            ? position(kDragonHeadActorYLow, kDragonHeadActorYHigh, 0U)
            : kDragonHeadsServeY;
        const auto direction = dragon_head_direction(
            ball_x - position(kDragonHeadActorXLow, kDragonHeadActorXHigh, slot),
            ball_y - position(kDragonHeadActorYLow, kDragonHeadActorYHigh, slot));
        const auto pose = static_cast<std::byte>(
            kDragonHeadPoses[direction >> 1U]);
        if (ram[kDragonHeadActorPose + slot] != pose) {
            ram[kDragonHeadActorPose + slot] = pose;
            changed = true;
        }
    }
    return changed;
}

} // namespace devils_crush::cabinet
