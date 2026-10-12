// SPDX-License-Identifier: GPL-3.0-or-later
#include "host_present.hpp"

#include "host_textures.hpp"
#include "table_motion.hpp"

namespace devils_crush::app::host {

namespace {

[[nodiscard]] bool ensure_stage(
    SDL_Renderer* renderer,
    const devils_crush::render::ScaleLayout& layout,
    StageTexture& stage)
{
    if (stage.texture != nullptr && stage.width == layout.stage_width
        && stage.height == layout.stage_height) {
        return true;
    }

    destroy_stage(stage);
    stage.texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        layout.stage_width,
        layout.stage_height);
    if (stage.texture == nullptr) {
        return report_sdl_error("SDL_CreateTexture (integer stage) failed");
    }
    stage.width = layout.stage_width;
    stage.height = layout.stage_height;

    if (!configure_texture(stage.texture, SDL_SCALEMODE_LINEAR)) {
        destroy_stage(stage);
        return false;
    }
    return true;
}

} // namespace

// The source pixel a place in the window is over; it may lie outside.
[[nodiscard]] std::optional<SDL_FPoint> picture_point(
    SDL_Renderer* renderer,
    const PresentedPicture& picture,
    const float window_x,
    const float window_y)
{
    float x = 0.0F;
    float y = 0.0F;
    if (picture.source_width <= 0 || picture.source_height <= 0
        || picture.destination.w <= 0.0F || picture.destination.h <= 0.0F
        || !SDL_RenderCoordinatesFromWindow(
            renderer, window_x, window_y, &x, &y)) {
        return std::nullopt;
    }
    // `SDL_RenderTextureRotated` turned it clockwise about its center.
    const float dx =
        x - (picture.destination.x + picture.destination.w / 2.0F);
    const float dy =
        y - (picture.destination.y + picture.destination.h / 2.0F);
    const int rotation = picture.rotation_degrees;
    const float across = rotation == 90 ? dy
        : rotation == 180 ? -dx : rotation == 270 ? -dy : dx;
    const float down = rotation == 90 ? -dx
        : rotation == 180 ? -dy : rotation == 270 ? dx : dy;
    return SDL_FPoint{
        (across / picture.destination.w + 0.5F)
            * static_cast<float>(picture.source_width),
        (down / picture.destination.h + 0.5F)
            * static_cast<float>(picture.source_height),
    };
}

void destroy_stage(StageTexture& stage)
{
    if (stage.texture != nullptr) {
        SDL_DestroyTexture(stage.texture);
    }
    stage = {};
}

[[nodiscard]] bool present_frame(
    SDL_Renderer* renderer,
    const SourceTexture& source,
    StageTexture& stage,
    const std::uint32_t aspect_numerator,
    const std::uint32_t aspect_denominator,
    const std::array<std::uint8_t, 3> clear_color,
    const float margin_fraction,
    const bool bottom_table_width,
    TableMotionPresenter* motion,
    const float motion_alpha,
    const FrameArtworkPresenter* artwork,
    const int rotation_degrees,
    PresentedPicture* const presented)
{
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &output_width, &output_height)) {
        return report_sdl_error("SDL_GetRenderOutputSize failed");
    }
    if (output_width <= 0 || output_height <= 0) {
        SDL_Delay(16);
        return true;
    }
    const int physical_width = output_width;
    const int physical_height = output_height;
    if (rotation_degrees == 90 || rotation_degrees == 270) {
        std::swap(output_width, output_height);
    }

    if (!SDL_SetRenderDrawColor(
            renderer,
            clear_color[0],
            clear_color[1],
            clear_color[2],
            255U)
        || !SDL_RenderClear(renderer)) {
        return report_sdl_error("SDL_RenderClear failed");
    }
    if (source.texture == nullptr) {
        return SDL_RenderPresent(renderer)
            || report_sdl_error("SDL_RenderPresent failed");
    }

    // The margin is taken from the shorter axis so the reserved border is the
    // same number of pixels on every side, rather than a wide window getting a
    // wide border and a short one a thin border.
    const auto margin = margin_fraction > 0.0F
        ? static_cast<int>(std::lround(
              static_cast<double>(std::min(output_width, output_height))
              * static_cast<double>(margin_fraction)))
        : 0;
    const auto inset_width = output_width - 2 * margin;
    const auto inset_height = output_height - 2 * margin;
    const bool inset_fits = inset_width > 0 && inset_height > 0;
    const auto inset = inset_fits ? margin : 0;

    auto layout = devils_crush::render::calculate_scale_layout(
        inset_fits ? inset_width : output_width,
        inset_fits ? inset_height : output_height,
        source.width,
        source.height,
        aspect_numerator,
        aspect_denominator);
    if (bottom_table_width) {
        const auto table_layout = devils_crush::render::calculate_scale_layout(
            output_width, output_height, source.width, source.height,
            devils_crush::cabinet::kTableAspectNumerator,
            devils_crush::cabinet::kTableAspectDenominator);
        layout = devils_crush::render::calculate_scale_layout(
            table_layout.destination.width, output_height,
            source.width, source.height, aspect_numerator, aspect_denominator);
        layout.destination.x = (output_width - layout.destination.width) / 2;
        layout.destination.y = output_height - layout.destination.height;
    }
    if (motion != nullptr) {
        // Intermediate motion must survive the nearest pass at subpixel
        // resolution, including when the window itself is relatively small.
        layout.stage_width = std::max(layout.stage_width, source.width * 4);
        layout.stage_height = std::max(layout.stage_height, source.height * 4);
    }
    if (!layout.valid() || !ensure_stage(renderer, layout, stage)) {
        return false;
    }
    // `calculate_scale_layout` centres within the box it was given, so the
    // destination only has to move by the border that box was inset by.
    layout.destination.x += inset;
    layout.destination.y += inset;

    if (!SDL_SetRenderTarget(renderer, stage.texture)) {
        return report_sdl_error("SDL_SetRenderTarget (integer stage) failed");
    }
    if (!SDL_RenderTexture(renderer, source.texture, nullptr, nullptr)) {
        return report_sdl_error("SDL_RenderTexture (nearest pass) failed");
    }
    if (motion != nullptr
        && !motion->draw(renderer, stage.width, stage.height, motion_alpha)) {
        SDL_SetRenderTarget(renderer, nullptr);
        return report_sdl_error("Table motion rendering failed");
    }
    if (artwork != nullptr && !artwork->draw(renderer, motion_alpha)) {
        SDL_SetRenderTarget(renderer, nullptr);
        return report_sdl_error("Frame artwork rendering failed");
    }
    if (!SDL_SetRenderTarget(renderer, nullptr)) {
        return report_sdl_error("SDL_SetRenderTarget (window) failed");
    }

    if (!SDL_SetRenderDrawColor(
            renderer,
            clear_color[0],
            clear_color[1],
            clear_color[2],
            255U)
        || !SDL_RenderClear(renderer)) {
        return report_sdl_error("SDL_RenderClear failed");
    }

    SDL_FRect destination{
        static_cast<float>(layout.destination.x),
        static_cast<float>(layout.destination.y),
        static_cast<float>(layout.destination.width),
        static_cast<float>(layout.destination.height),
    };
    // Rotate the destination center about the entire logical playfield, so
    // bottom-aligned screens stay at the table's foot in every orientation.
    const float dx = destination.x + destination.w / 2 - output_width / 2.0F;
    const float dy = destination.y + destination.h / 2 - output_height / 2.0F;
    const float rotated_x = rotation_degrees == 90 ? -dy
        : rotation_degrees == 180 ? -dx : rotation_degrees == 270 ? dy : dx;
    const float rotated_y = rotation_degrees == 90 ? dx
        : rotation_degrees == 180 ? -dy : rotation_degrees == 270 ? -dx : dy;
    destination.x = physical_width / 2.0F + rotated_x - destination.w / 2;
    destination.y = physical_height / 2.0F + rotated_y - destination.h / 2;
    if (presented != nullptr) {
        *presented = {
            .destination = destination,
            .rotation_degrees = rotation_degrees,
            .source_width = source.width,
            .source_height = source.height,
        };
    }
    if (!SDL_RenderTextureRotated(renderer, stage.texture, nullptr, &destination,
            rotation_degrees, nullptr, SDL_FLIP_NONE)) {
        return report_sdl_error("SDL_RenderTexture (sharp resolve) failed");
    }
    if (!SDL_RenderPresent(renderer)) {
        return report_sdl_error("SDL_RenderPresent failed");
    }
    return true;
}

} // namespace devils_crush::app::host
