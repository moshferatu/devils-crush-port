// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

bool report_sdl_error(const std::string_view operation);

[[nodiscard]] bool configure_texture(
    SDL_Texture* texture,
    const SDL_ScaleMode scale_mode);

struct SourceTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

void destroy_source(SourceTexture& source);

[[nodiscard]] bool upload_framebuffer(
    SDL_Renderer* renderer,
    const FramebufferView& framebuffer,
    SourceTexture& source);

[[nodiscard]] bool upload_image(
    SDL_Renderer* renderer,
    const devils_crush::render::ArgbImage& image,
    SourceTexture& source);

[[nodiscard]] bool save_playfield_png(
    const devils_crush::render::ArgbImage& image,
    const std::filesystem::path& path);

} // namespace devils_crush::app::host
