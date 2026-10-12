// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "host_textures.hpp"
#include "table_motion.hpp"

namespace devils_crush::app::host {

struct StageTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

// Where `present_frame` last put the source picture in its window, in render
// output pixels, and how far it was turned.
struct PresentedPicture {
    SDL_FRect destination{};
    int rotation_degrees = 0;
    int source_width = 0;
    int source_height = 0;
};

[[nodiscard]] std::optional<SDL_FPoint> picture_point(
    SDL_Renderer* renderer,
    const PresentedPicture& picture,
    const float window_x,
    const float window_y);

void destroy_stage(StageTexture& stage);

[[nodiscard]] bool present_frame(
    SDL_Renderer* renderer,
    const SourceTexture& source,
    StageTexture& stage,
    const std::uint32_t aspect_numerator,
    const std::uint32_t aspect_denominator,
    const std::array<std::uint8_t, 3> clear_color = {8U, 8U, 12U},
    const float margin_fraction = 0.0F,
    const bool bottom_table_width = false,
    TableMotionPresenter* motion = nullptr,
    const float motion_alpha = 1.0F,
    const FrameArtworkPresenter* artwork = nullptr,
    const int rotation_degrees = 0,
    PresentedPicture* const presented = nullptr);

} // namespace devils_crush::app::host
