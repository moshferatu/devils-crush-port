// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_retry_asset.hpp"

#include <cstddef>

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] constexpr std::int16_t signed_word(
    const std::uint8_t low,
    const std::uint8_t high) noexcept
{
    const auto unsigned_value = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low)
        | (static_cast<std::uint16_t>(high) << 8U));
    const auto signed_value = unsigned_value >= 0x8000U
        ? static_cast<std::int32_t>(unsigned_value) - 0x10000
        : static_cast<std::int32_t>(unsigned_value);
    return static_cast<std::int16_t>(signed_value);
}

} // namespace

std::optional<CollisionRetryOffsetAttempts> decode_collision_retry_offset_attempts(
    const MappedRomView& rom) noexcept
{
    constexpr std::size_t kBytesPerDirection = 4U;
    constexpr std::size_t kDirections = 4U;
    constexpr std::size_t kAttemptsPerDirection =
        CollisionRetryOffsetAttempts::kAttemptsPerDirection;
    const auto bytes = rom.range_at(
        kCollisionRetryOffsetTable,
        kBytesPerDirection * kAttemptsPerDirection * kDirections);
    if (!bytes) {
        return std::nullopt;
    }

    CollisionRetryOffsetAttempts attempts;
    for (std::size_t direction = 0U; direction < kDirections;
         ++direction) {
        for (std::size_t attempt = 0U;
             attempt < kAttemptsPerDirection;
             ++attempt) {
            const auto base =
                (direction * kAttemptsPerDirection + attempt)
                * kBytesPerDirection;
            attempts.by_direction[direction][attempt] = {
                .horizontal_pixels = signed_word(
                    std::to_integer<std::uint8_t>((*bytes)[base]),
                    std::to_integer<std::uint8_t>((*bytes)[base + 1U])),
                .vertical_pixels = signed_word(
                    std::to_integer<std::uint8_t>((*bytes)[base + 2U]),
                    std::to_integer<std::uint8_t>((*bytes)[base + 3U])),
            };
        }
    }
    return attempts;
}

std::optional<CollisionRetryOffsets> decode_collision_retry_offsets(
    const MappedRomView& rom) noexcept
{
    const auto attempts = decode_collision_retry_offset_attempts(rom);
    if (!attempts) {
        return std::nullopt;
    }

    CollisionRetryOffsets offsets;
    for (std::size_t direction = 0U;
         direction < offsets.by_direction.size();
         ++direction) {
        offsets.by_direction[direction] = attempts->by_direction[direction][0U];
    }
    return offsets;
}

} // namespace devils_crush::native_runtime
