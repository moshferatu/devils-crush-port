// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/game_math.hpp"

#include <bit>

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint32_t k24BitMask = 0x00FFFFFFU;

// The size of a signed number. The most negative value has no positive
// counterpart and keeps its bit pattern, which is the right size when read
// as unsigned.
[[nodiscard]] constexpr std::uint16_t magnitude(const std::int16_t value) noexcept
{
    const auto bits = std::bit_cast<std::uint16_t>(value);
    return value < 0 ? static_cast<std::uint16_t>(0U - bits) : bits;
}

[[nodiscard]] constexpr std::int16_t with_sign(
    const std::uint16_t size,
    const bool negative) noexcept
{
    return std::bit_cast<std::int16_t>(
        negative ? static_cast<std::uint16_t>(0U - size) : size);
}

// Multiplies the sizes of two signed numbers the way the game does: the
// factor's high byte first, then its low byte on top of that result.
[[nodiscard]] MultiplierPass multiply_magnitudes(
    const std::int16_t value,
    const std::int16_t factor) noexcept
{
    const auto value_size = magnitude(value);
    const auto factor_size = magnitude(factor);
    const auto high_pass = multiply_and_accumulate(
        0U, value_size, static_cast<std::uint8_t>(factor_size >> 8U));
    return multiply_and_accumulate(
        high_pass.total, value_size, static_cast<std::uint8_t>(factor_size));
}

struct LongDivision {
    std::uint32_t quotient = 0U;
    std::uint32_t remainder = 0U;
};

// Long division, one bit of the dividend at a time from the top. The
// remainder is held in `remainder_bits` bits and silently loses anything
// shifted past them, as the game's does. There is no check for a zero
// divisor: every step then subtracts nothing and records a one.
[[nodiscard]] constexpr LongDivision divide_bit_by_bit(
    const std::uint32_t dividend,
    const int dividend_bits,
    const std::uint32_t divisor,
    const int remainder_bits) noexcept
{
    const auto remainder_mask = (std::uint32_t{1U} << remainder_bits) - 1U;
    LongDivision division;
    for (int bit = dividend_bits - 1; bit >= 0; --bit) {
        division.remainder =
            ((division.remainder << 1U) | ((dividend >> bit) & 1U))
            & remainder_mask;
        division.quotient <<= 1U;
        if (division.remainder >= divisor) {
            division.remainder -= divisor;
            division.quotient |= 1U;
        }
    }
    return division;
}

} // namespace

std::uint16_t multiply_bytes(
    const std::uint8_t first,
    const std::uint8_t second) noexcept
{
    return static_cast<std::uint16_t>(first * second);
}

MultiplierPass multiply_and_accumulate(
    const std::uint16_t running_total,
    const std::uint16_t value,
    const std::uint8_t factor) noexcept
{
    // The factor sits above the running total in one 24-bit register. Each
    // step shifts the register up and, when the bit shifted out of the factor
    // was set, adds the value.
    auto shifted = (static_cast<std::uint32_t>(factor) << 16U) | running_total;
    for (int step = 0; step < 8; ++step) {
        const bool add_value = (shifted & 0x00800000U) != 0U;
        shifted = (shifted << 1U) & k24BitMask;
        if (add_value) {
            shifted = (shifted + value) & k24BitMask;
        }
    }
    return {
        .total = static_cast<std::uint16_t>(shifted),
        .overflow = static_cast<std::uint8_t>(shifted >> 16U),
    };
}

std::int16_t multiply_fixed(
    const std::int16_t value,
    const std::int16_t factor) noexcept
{
    // The product of two 8.8 numbers has 16 fraction bits. Dropping the low
    // byte leaves 8.
    const auto product = multiply_magnitudes(value, factor);
    const auto size = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(product.overflow) << 8U)
        | (product.total >> 8U));
    return with_sign(size, (value < 0) != (factor < 0));
}

std::int16_t multiply_whole(
    const std::int16_t value,
    const std::int16_t factor) noexcept
{
    return with_sign(
        multiply_magnitudes(value, factor).total, (value < 0) != (factor < 0));
}

ByteDivision divide_byte(
    const std::uint8_t dividend,
    const std::uint8_t divisor) noexcept
{
    const auto division = divide_bit_by_bit(dividend, 8, divisor, 8);
    return {
        .quotient = division.quotient,
        .remainder = static_cast<std::uint8_t>(division.remainder),
    };
}

ByteDivision divide_24_bit(
    const std::uint32_t dividend,
    const std::uint8_t divisor) noexcept
{
    const auto division =
        divide_bit_by_bit(dividend & k24BitMask, 24, divisor, 8);
    return {
        .quotient = division.quotient,
        .remainder = static_cast<std::uint8_t>(division.remainder),
    };
}

std::int16_t divide_signed(
    const std::int16_t dividend,
    const std::int8_t divisor) noexcept
{
    if (divisor == 0) {
        return dividend < 0 ? std::int16_t{1} : std::int16_t{-1};
    }
    // -32768 / -1 does not fit and wraps back to -32768.
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
        static_cast<std::int32_t>(dividend) / divisor));
}

std::uint16_t divide_to_fixed(
    const std::uint16_t dividend,
    const std::uint16_t divisor) noexcept
{
    const auto whole = divide_bit_by_bit(dividend, 16, divisor, 16);
    const auto fraction = divide_bit_by_bit(
        (whole.remainder & 0xFFU) << 8U, 16, divisor, 16);
    return static_cast<std::uint16_t>(
        ((whole.quotient & 0xFFU) << 8U) | (fraction.quotient & 0xFFU));
}

std::uint8_t square_root(const std::uint16_t value) noexcept
{
    // Settle the root from its top bit down, keeping each bit whose square
    // still fits.
    std::uint32_t root = 0U;
    for (std::uint32_t bit = 0x80U; bit != 0U; bit >>= 1U) {
        const auto candidate = root | bit;
        if (candidate * candidate <= value) {
            root = candidate;
        }
    }
    return static_cast<std::uint8_t>(root);
}

std::uint8_t RandomGenerator::next(const std::uint8_t frame_counter) noexcept
{
    const auto low = static_cast<std::uint8_t>(seed);
    const auto high = static_cast<std::uint8_t>(seed >> 8U);

    const auto doubled = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(seed + 0x503FU) << 1U);
    const auto low_with_frame = static_cast<std::uint32_t>(low) + frame_counter;
    const auto mixed = static_cast<std::uint8_t>(
        high + (doubled >> 8U) + (low_with_frame >> 8U));

    const auto next_high = static_cast<std::uint8_t>(low + mixed);
    const auto next_low = static_cast<std::uint8_t>(low_with_frame);
    seed = static_cast<std::uint16_t>((next_high << 8U) | next_low);
    return next_high;
}

} // namespace devils_crush::native_runtime
