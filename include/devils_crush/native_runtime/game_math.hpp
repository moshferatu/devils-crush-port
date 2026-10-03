// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// The arithmetic the game shares between physics, collisions and actors,
// written as ordinary functions of their operands. Each gives the same answer
// as the cartridge's routine for every input, including the inputs the
// cartridge handles badly: it never checks for a zero divisor and it lets
// some intermediate values overflow. Those cases are described where they
// arise so nothing here needs the original code to be understood.
//
// Numbers called "8.8" are fixed point: the high byte counts whole units and
// the low byte counts 256ths.

// The product of two bytes.
[[nodiscard]] std::uint16_t multiply_bytes(
    std::uint8_t first,
    std::uint8_t second) noexcept;

// One pass of the game's multiplier: `running_total * 256 + value * factor`,
// kept to 24 bits. The low 16 bits are `total` and the top 8 are `overflow`.
//
// The multiplier works a bit at a time and keeps the unused bits of `factor`
// in the same register the total grows into. When `running_total` is large
// enough for the total to reach those bits it changes the remaining factor,
// and the answer is no longer the formula above. A zero `running_total`
// never does this.
struct MultiplierPass {
    std::uint16_t total = 0U;
    std::uint8_t overflow = 0U;

    [[nodiscard]] bool operator==(const MultiplierPass&) const = default;
};

[[nodiscard]] MultiplierPass multiply_and_accumulate(
    std::uint16_t running_total,
    std::uint16_t value,
    std::uint8_t factor) noexcept;

// `value * factor` for two signed 8.8 numbers, as a signed 8.8 number. The
// sizes are multiplied and the sign put back afterwards, so the result is
// rounded toward zero. It is exact whenever the true product fits in a signed
// 8.8 number; larger products wrap as described for `multiply_and_accumulate`.
[[nodiscard]] std::int16_t multiply_fixed(
    std::int16_t value,
    std::int16_t factor) noexcept;

// `value * factor` for two signed whole numbers, keeping the low 16 bits.
// Products that do not fit wrap as described for `multiply_and_accumulate`.
[[nodiscard]] std::int16_t multiply_whole(
    std::int16_t value,
    std::int16_t factor) noexcept;

// The quotient and remainder of a division by a byte.
//
// The game divides one bit at a time and holds the remainder in a single
// byte. Divisors above 128 can push the remainder past that byte, and then
// the result is not the true quotient. Dividing by zero gives a quotient with
// every bit set.
struct ByteDivision {
    std::uint32_t quotient = 0U;
    std::uint8_t remainder = 0U;

    [[nodiscard]] bool operator==(const ByteDivision&) const = default;
};

[[nodiscard]] ByteDivision divide_byte(
    std::uint8_t dividend,
    std::uint8_t divisor) noexcept;

// Divides a 24-bit number by a byte. Only the low 24 bits of `dividend` are
// used.
[[nodiscard]] ByteDivision divide_24_bit(
    std::uint32_t dividend,
    std::uint8_t divisor) noexcept;

// `dividend / divisor` for a signed whole number and a signed byte, rounded
// toward zero. Dividing by zero gives -1 for a dividend of zero or more and
// 1 for a negative one.
[[nodiscard]] std::int16_t divide_signed(
    std::int16_t dividend,
    std::int8_t divisor) noexcept;

// `dividend / divisor` for two unsigned whole numbers, as an unsigned 8.8
// number.
//
// The game finds the whole part and the fraction separately. It keeps only
// the low byte of the whole part, and it finds the fraction from only the low
// byte of the first remainder, so the fraction is right only for divisors of
// 256 or less.
[[nodiscard]] std::uint16_t divide_to_fixed(
    std::uint16_t dividend,
    std::uint16_t divisor) noexcept;

// The largest whole number whose square is not above `value`.
[[nodiscard]] std::uint8_t square_root(std::uint16_t value) noexcept;

// The game's random numbers. Every draw also mixes in the frame counter, so
// the sequence depends on when each number is asked for.
struct RandomGenerator {
    std::uint16_t seed = 0U;

    // Advances the seed and returns the new random byte, which is the seed's
    // high byte.
    std::uint8_t next(std::uint8_t frame_counter) noexcept;

    [[nodiscard]] bool operator==(const RandomGenerator&) const = default;
};

} // namespace devils_crush::native_runtime
