// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::render {

struct IntRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct ScaleLayout {
    IntRect destination;
    int prescale_x = 0;
    int prescale_y = 0;
    int stage_width = 0;
    int stage_height = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return destination.width > 0 && destination.height > 0
            && prescale_x > 0 && prescale_y > 0 && stage_width > 0
            && stage_height > 0;
    }
};

[[nodiscard]] ScaleLayout calculate_scale_layout(
    int output_width,
    int output_height,
    int source_width,
    int source_height,
    std::uint32_t aspect_numerator,
    std::uint32_t aspect_denominator) noexcept;

} // namespace devils_crush::render
