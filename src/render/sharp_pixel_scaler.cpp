// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/sharp_pixel_scaler.hpp"

#include <algorithm>
#include <cstdint>

namespace devils_crush::render {
namespace {

[[nodiscard]] std::uint64_t round_divide(
    const std::uint64_t numerator,
    const std::uint64_t denominator) noexcept
{
    const auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    return quotient + (remainder * 2U >= denominator ? 1U : 0U);
}

} // namespace

ScaleLayout calculate_scale_layout(
    const int output_width,
    const int output_height,
    const int source_width,
    const int source_height,
    const std::uint32_t aspect_numerator,
    const std::uint32_t aspect_denominator) noexcept
{
    if (output_width <= 0 || output_height <= 0 || source_width <= 0
        || source_height <= 0 || aspect_numerator == 0U
        || aspect_denominator == 0U) {
        return {};
    }

    const auto output_width_u = static_cast<std::uint64_t>(output_width);
    const auto output_height_u = static_cast<std::uint64_t>(output_height);
    const auto numerator_u = static_cast<std::uint64_t>(aspect_numerator);
    const auto denominator_u = static_cast<std::uint64_t>(aspect_denominator);

    std::uint64_t destination_width = 0;
    std::uint64_t destination_height = 0;

    if (output_width_u * denominator_u
        <= output_height_u * numerator_u) {
        destination_width = output_width_u;
        destination_height =
            round_divide(output_width_u * denominator_u, numerator_u);
        destination_height =
            std::clamp(destination_height, std::uint64_t{1}, output_height_u);
    } else {
        destination_height = output_height_u;
        destination_width =
            round_divide(output_height_u * numerator_u, denominator_u);
        destination_width =
            std::clamp(destination_width, std::uint64_t{1}, output_width_u);
    }

    ScaleLayout layout;
    layout.destination.width = static_cast<int>(destination_width);
    layout.destination.height = static_cast<int>(destination_height);
    layout.destination.x = (output_width - layout.destination.width) / 2;
    layout.destination.y = (output_height - layout.destination.height) / 2;
    layout.prescale_x =
        std::max(1, layout.destination.width / source_width);
    layout.prescale_y =
        std::max(1, layout.destination.height / source_height);
    layout.stage_width = source_width * layout.prescale_x;
    layout.stage_height = source_height * layout.prescale_y;
    return layout;
}

} // namespace devils_crush::render
