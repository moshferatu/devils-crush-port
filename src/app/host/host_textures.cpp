// SPDX-License-Identifier: GPL-3.0-or-later
#include "host_textures.hpp"

namespace devils_crush::app::host {

bool report_sdl_error(const std::string_view operation)
{
    std::cerr << operation << ": " << SDL_GetError() << '\n';
    return false;
}

[[nodiscard]] bool configure_texture(
    SDL_Texture* texture,
    const SDL_ScaleMode scale_mode)
{
    if (!SDL_SetTextureScaleMode(texture, scale_mode)) {
        return report_sdl_error("SDL_SetTextureScaleMode failed");
    }
    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE)) {
        return report_sdl_error("SDL_SetTextureBlendMode failed");
    }
    return true;
}

void destroy_source(SourceTexture& source)
{
    if (source.texture != nullptr) {
        SDL_DestroyTexture(source.texture);
    }
    source = {};
}

[[nodiscard]] bool upload_framebuffer(
    SDL_Renderer* renderer,
    const FramebufferView& framebuffer,
    SourceTexture& source)
{
    if (framebuffer.width <= 0 || framebuffer.height <= 0
        || framebuffer.pitch_pixels < framebuffer.width) {
        std::cerr << "The emulator returned invalid framebuffer geometry.\n";
        return false;
    }

    const auto pitch = static_cast<std::size_t>(framebuffer.pitch_pixels);
    const auto height = static_cast<std::size_t>(framebuffer.height);
    const auto width = static_cast<std::size_t>(framebuffer.width);
    const auto required_pixels = (height - 1U) * pitch + width;
    if (framebuffer.pixels.size() < required_pixels
        || pitch
            > static_cast<std::size_t>(
                std::numeric_limits<int>::max()
                / static_cast<int>(sizeof(std::uint32_t)))) {
        std::cerr << "The emulator returned an incomplete framebuffer.\n";
        return false;
    }

    if (source.texture == nullptr || source.width != framebuffer.width
        || source.height != framebuffer.height) {
        destroy_source(source);
        source.texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            framebuffer.width,
            framebuffer.height);
        if (source.texture == nullptr) {
            return report_sdl_error("SDL_CreateTexture (source) failed");
        }
        source.width = framebuffer.width;
        source.height = framebuffer.height;
        if (!configure_texture(source.texture, SDL_SCALEMODE_NEAREST)) {
            destroy_source(source);
            return false;
        }
    }

    const auto pitch_bytes = static_cast<int>(
        pitch * static_cast<std::size_t>(sizeof(std::uint32_t)));
    if (!SDL_UpdateTexture(
            source.texture,
            nullptr,
            framebuffer.pixels.data(),
            pitch_bytes)) {
        return report_sdl_error("SDL_UpdateTexture failed");
    }
    return true;
}

[[nodiscard]] bool upload_image(
    SDL_Renderer* renderer,
    const devils_crush::render::ArgbImage& image,
    SourceTexture& source)
{
    if (!image.valid()) {
        std::cerr << "The cabinet renderer produced an invalid image.\n";
        return false;
    }
    return upload_framebuffer(
        renderer,
        {
            .pixels = image.pixels,
            .width = image.width,
            .height = image.height,
            .pitch_pixels = image.width,
        },
        source);
}

// Writes the composed playfield exactly as it reaches the scaler, so a
// reported seam can be examined in the same pixels the player saw rather
// than reasoned about from counters.
[[nodiscard]] bool save_playfield_png(
    const devils_crush::render::ArgbImage& image,
    const std::filesystem::path& path)
{
    if (!image.valid()) {
        return false;
    }
    SDL_Surface* const surface = SDL_CreateSurfaceFrom(
        image.width,
        image.height,
        SDL_PIXELFORMAT_ARGB8888,
        const_cast<std::uint32_t*>(image.pixels.data()),
        image.width * static_cast<int>(sizeof(std::uint32_t)));
    if (surface == nullptr) {
        return report_sdl_error("SDL_CreateSurfaceFrom (dump) failed");
    }
    // Core SDL's BMP writer is the only encoder linked in: this is a
    // diagnostic dump, not a deliverable asset.
    const bool saved = SDL_SaveBMP(surface, path.string().c_str());
    SDL_DestroySurface(surface);
    if (!saved) {
        return report_sdl_error("SDL_SaveBMP failed");
    }
    return true;
}

} // namespace devils_crush::app::host
