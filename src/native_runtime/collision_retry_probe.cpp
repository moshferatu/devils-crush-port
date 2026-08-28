// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_retry_probe.hpp"

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] constexpr std::uint16_t integer_coordinate(
    const Fixed24_8 position) noexcept
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(position.raw) >> 8U);
}

[[nodiscard]] constexpr std::uint8_t low_byte(
    const std::uint16_t value) noexcept
{
    return static_cast<std::uint8_t>(value);
}

[[nodiscard]] constexpr std::uint8_t high_byte(
    const std::uint16_t value) noexcept
{
    return static_cast<std::uint8_t>(value >> 8U);
}

} // namespace

CollisionRetryProbeResult probe_collision_retry(
    const CollisionRetryStep& retry) noexcept
{
    const auto horizontal = integer_coordinate(retry.state.position_x);
    const auto vertical = integer_coordinate(retry.state.position_y);

    // CEB7-C EC3 performs SBC #$20 then SBC #$00. Carry enters from the
    // immediately preceding vertical-coordinate ADC and is therefore part of
    // the retry result, rather than a new collision policy choice.
    const auto low_subtrahend = static_cast<std::uint16_t>(
        0x20U + (retry.vertical_integer_addition_carried ? 0U : 1U));
    const auto low_without_borrow = low_byte(horizontal) >= low_subtrahend;
    const auto adjusted_high = static_cast<std::uint8_t>(
        high_byte(horizontal) - (low_without_borrow ? 0U : 1U));
    const auto adjusted_low = static_cast<std::uint8_t>(
        low_byte(horizontal) - low_subtrahend);
    const auto vertical_low = low_byte(vertical);
    const auto full_scan = adjusted_high == 0U && vertical_low == 0U;

    return {
        .retry = retry,
        .adjusted_horizontal_low_byte = adjusted_low,
        .adjusted_horizontal_high_byte = adjusted_high,
        .vertical_low_byte = vertical_low,
        .disposition = full_scan
            ? CollisionRetryProbeDisposition::requires_full_scan
            : CollisionRetryProbeDisposition::clear,
    };
}

std::optional<CollisionProbe> make_collision_retry_full_scan_probe(
    const CollisionRetryProbeResult& retry_probe,
    const std::uint8_t parent_vertical_high_byte) noexcept
{
    if (retry_probe.disposition
        != CollisionRetryProbeDisposition::requires_full_scan) {
        return std::nullopt;
    }
    return CollisionProbe{
        .horizontal = retry_probe.adjusted_horizontal_low_byte,
        .vertical = static_cast<std::uint16_t>(
            retry_probe.vertical_low_byte
            | (static_cast<std::uint16_t>(parent_vertical_high_byte) << 8U)),
    };
}

} // namespace devils_crush::native_runtime
