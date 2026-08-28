// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_response.hpp"

#include <bit>
#include <algorithm>

namespace devils_crush::native_runtime {
namespace {

[[nodiscard]] constexpr std::uint16_t arithmetic_shift_right(
    std::uint16_t value,
    const std::uint8_t count) noexcept
{
    for (std::uint8_t index = 0; index < count; ++index) {
        value = static_cast<std::uint16_t>(
            (value >> 1U) | (value & 0x8000U));
    }
    return value;
}

[[nodiscard]] constexpr CollisionResponseMode effective_mode(
    const CollisionResponseConfig& config) noexcept
{
    return config.requested_mode == CollisionResponseMode::normal
            && config.force_reduced_mode
        ? CollisionResponseMode::reduced
        : config.requested_mode;
}

[[nodiscard]] constexpr std::int16_t fixed_multiply(
    const std::int16_t first,
    const std::int16_t second) noexcept
{
    const auto first_bits = std::bit_cast<std::uint16_t>(first);
    const auto second_bits = std::bit_cast<std::uint16_t>(second);
    const auto first_magnitude = static_cast<std::uint32_t>(
        (first_bits & 0x8000U) != 0U ? 0U - first_bits : first_bits);
    const auto second_magnitude = static_cast<std::uint32_t>(
        (second_bits & 0x8000U) != 0U ? 0U - second_bits : second_bits);
    const auto magnitude = static_cast<std::uint16_t>(
        (first_magnitude * second_magnitude) >> 8U);
    const auto result = static_cast<std::uint16_t>(
        ((first_bits ^ second_bits) & 0x8000U) != 0U
            ? 0U - magnitude
            : magnitude);
    return std::bit_cast<std::int16_t>(result);
}

[[nodiscard]] constexpr std::int16_t add_fixed(
    const std::int16_t left,
    const std::int16_t right) noexcept
{
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
        std::bit_cast<std::uint16_t>(left)
        + std::bit_cast<std::uint16_t>(right)));
}

[[nodiscard]] constexpr std::int16_t subtract_fixed(
    const std::int16_t left,
    const std::int16_t right) noexcept
{
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
        std::bit_cast<std::uint16_t>(left)
        - std::bit_cast<std::uint16_t>(right)));
}

[[nodiscard]] constexpr std::int16_t negate_fixed(
    const std::int16_t value) noexcept
{
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
        0U - std::bit_cast<std::uint16_t>(value)));
}

} // namespace

CollisionResponse apply_collision_response(
    const std::int16_t velocity,
    const CollisionResponseConfig& config) noexcept
{
    const auto incoming_bits = std::bit_cast<std::uint16_t>(velocity);
    const auto mode = effective_mode(config);
    CollisionResponse response{
        .incoming_velocity = velocity,
        .effective_mode = mode,
    };

    if (mode == CollisionResponseMode::reduced) {
        const auto shift_count = static_cast<std::uint8_t>(
            config.use_quarter_reduced_scale ? 2U : 3U);
        auto output = arithmetic_shift_right(incoming_bits, shift_count);
        if (!config.use_quarter_reduced_scale) {
            output = static_cast<std::uint16_t>(
                output + arithmetic_shift_right(incoming_bits, 4U));
        }
        response.outgoing_velocity = std::bit_cast<std::int16_t>(output);
        return response;
    }
    if (mode == CollisionResponseMode::half) {
        response.outgoing_velocity = std::bit_cast<std::int16_t>(
            arithmetic_shift_right(incoming_bits, 1U));
        return response;
    }
    if (mode == CollisionResponseMode::minimal) {
        response.outgoing_velocity = std::bit_cast<std::int16_t>(
            arithmetic_shift_right(incoming_bits, 4U));
        return response;
    }

    const auto input_low = static_cast<std::uint8_t>(incoming_bits);
    const auto input_high = static_cast<std::uint8_t>(incoming_bits >> 8U);
    if ((incoming_bits & 0x8000U) != 0U) {
        const auto output_high = std::max(input_high, std::uint8_t{0xFAU});
        response.clamped = output_high != input_high;
        const auto output = static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(
                input_low - config.normal_tuning.negative_low_byte_decrement)
            | (static_cast<std::uint16_t>(output_high) << 8U));
        response.outgoing_velocity = std::bit_cast<std::int16_t>(output);
        return response;
    }

    const auto low_sum = static_cast<std::uint16_t>(
        input_low + config.normal_tuning.positive_low_byte_increment);
    const auto high_sum = static_cast<std::uint16_t>(
        input_high + config.normal_tuning.positive_high_byte_increment
        + static_cast<std::uint8_t>(low_sum > 0xFFU));
    constexpr std::uint16_t kMaximumNormalHighByte = 0x06U;
    const auto output_high = high_sum >= kMaximumNormalHighByte + 1U
        ? kMaximumNormalHighByte
        : high_sum;
    response.clamped = output_high != high_sum;
    const auto output = static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(low_sum)
        | (output_high << 8U));
    response.outgoing_velocity = std::bit_cast<std::int16_t>(output);
    return response;
}

OrientedCollisionResponse apply_oriented_collision_response(
    const CollisionResponseVector velocity,
    const CollisionResponseBasis basis,
    const CollisionResponseConfig& config,
    const CollisionNormalReflection reflection) noexcept
{
    // D09E: project the table vector into the surface's tangent/normal axes.
    const auto tangent = add_fixed(
        fixed_multiply(velocity.horizontal, basis.cosine),
        fixed_multiply(velocity.vertical, basis.sine));
    const auto inward_normal = add_fixed(
        negate_fixed(fixed_multiply(velocity.horizontal, basis.sine)),
        fixed_multiply(velocity.vertical, basis.cosine));

    // CF3F negates the normal before CFD8; CF5B instead negates CFD8's result.
    // The tangent remains unchanged in both source legs.
    const auto normal = apply_collision_response(
        reflection == CollisionNormalReflection::before_response
            ? negate_fixed(inward_normal)
            : inward_normal,
        config);
    const auto resolved_normal =
        reflection == CollisionNormalReflection::after_response
        ? negate_fixed(normal.outgoing_velocity)
        : normal.outgoing_velocity;

    // D1A2: transform the resolved local vector back into table coordinates.
    return {
        .incoming = velocity,
        .local = {
            .horizontal = tangent,
            .vertical = inward_normal,
        },
        .normal = normal,
        .outgoing = {
            .horizontal = subtract_fixed(
                fixed_multiply(tangent, basis.cosine),
                fixed_multiply(resolved_normal, basis.sine)),
            .vertical = add_fixed(
                fixed_multiply(tangent, basis.sine),
                fixed_multiply(resolved_normal, basis.cosine)),
        },
    };
}

} // namespace devils_crush::native_runtime
