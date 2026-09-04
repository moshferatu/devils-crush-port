// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_launch.hpp"

#include <array>
#include <cstdint>

namespace devils_crush::native_runtime {
namespace {

constexpr std::size_t kWorkRamSize = 0x2000U;
constexpr std::size_t kSessionFlagsOffset = 0x0053U;
constexpr std::size_t kScreenSelectorOffset = 0x0054U;
constexpr std::size_t kBallActivityFlagsOffset = 0x1080U;
constexpr std::size_t kBallPositionXLowOffset = 0x1180U;
constexpr std::size_t kBallPositionXHighOffset = 0x1200U;
constexpr std::size_t kBallPositionYLowOffset = 0x1300U;
constexpr std::size_t kBallPositionYHighOffset = 0x1380U;
constexpr std::size_t kBallMotionFlagsOffset = 0x1C80U;
constexpr std::size_t kBallHandlerLowOffset = 0x1F00U;
constexpr std::size_t kBallHandlerHighOffset = 0x1F80U;
constexpr std::array<std::size_t, 4> kSavedBonusReturnPositionOffsets{
    0x0D22U,
    0x0D23U,
    0x0D24U,
    0x0D25U,
};
constexpr std::array<std::size_t, 4> kBallPositionOffsets{
    kBallPositionXLowOffset,
    kBallPositionXHighOffset,
    kBallPositionYLowOffset,
    kBallPositionYHighOffset,
};

constexpr std::uint8_t kBonusSessionBit = 0x10U;
constexpr std::uint8_t kBallActiveBit = 0x80U;
constexpr std::uint8_t kBallHeldByTableFeatureBit = 0x01U;
constexpr std::uint16_t kBonusReturnBallHandler = 0xC8DAU;

[[nodiscard]] std::uint8_t load(
    const std::span<std::byte> work_ram,
    const std::size_t offset) noexcept
{
    return std::to_integer<std::uint8_t>(work_ram[offset]);
}

} // namespace

bool release_synthetic_bonus_return_ball(
    const std::span<std::byte> work_ram) noexcept
{
    if (work_ram.size() != kWorkRamSize
        || (load(work_ram, kSessionFlagsOffset) & kBonusSessionBit) != 0U
        || load(work_ram, kScreenSelectorOffset) != 0U
        || (load(work_ram, kBallActivityFlagsOffset) & kBallActiveBit) == 0U
        || (load(work_ram, kBallMotionFlagsOffset)
            & kBallHeldByTableFeatureBit) == 0U) {
        return false;
    }

    const auto handler = static_cast<std::uint16_t>(
        load(work_ram, kBallHandlerLowOffset)
        | (static_cast<std::uint16_t>(
               load(work_ram, kBallHandlerHighOffset))
           << 8U));
    if (handler != kBonusReturnBallHandler) {
        return false;
    }

    for (std::size_t index = 0U; index < kBallPositionOffsets.size(); ++index) {
        if (work_ram[kBallPositionOffsets[index]]
            != work_ram[kSavedBonusReturnPositionOffsets[index]]) {
            return false;
        }
    }

    work_ram[kBallMotionFlagsOffset] &=
        static_cast<std::byte>(~kBallHeldByTableFeatureBit);
    return true;
}

} // namespace devils_crush::native_runtime
