// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/render/backglass_image.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <system_error>
#include <utility>

namespace devils_crush::render {
namespace {

constexpr std::uintmax_t kMaximumFileSize =
    64U * 1024U * 1024U;
constexpr int kMaximumDimension = 16'384;
constexpr std::uint64_t kMaximumPixels = 64U * 1024U * 1024U;

[[nodiscard]] std::string path_to_utf8(
    const std::filesystem::path& path)
{
    const auto encoded = path.u8string();
    return {
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size(),
    };
}

} // namespace

BackglassLoadResult::~BackglassLoadResult()
{
    if (surface != nullptr) {
        SDL_DestroySurface(surface);
    }
}

BackglassLoadResult::BackglassLoadResult(
    const BackglassLoadStatus status_value,
    SDL_Surface* const surface_value,
    std::string message_value)
    : status(status_value)
    , surface(surface_value)
    , message(std::move(message_value))
{
}

BackglassLoadResult::BackglassLoadResult(
    BackglassLoadResult&& other) noexcept
    : status(other.status)
    , surface(std::exchange(other.surface, nullptr))
    , message(std::move(other.message))
{
}

BackglassLoadResult& BackglassLoadResult::operator=(
    BackglassLoadResult&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (surface != nullptr) {
        SDL_DestroySurface(surface);
    }
    status = other.status;
    surface = std::exchange(other.surface, nullptr);
    message = std::move(other.message);
    return *this;
}

BackglassLoadResult load_backglass_image(
    const std::filesystem::path& path)
{
    std::error_code error;
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size == 0U || file_size > kMaximumFileSize) {
        return {
            BackglassLoadStatus::io_error,
            nullptr,
            "The image is missing, empty, or outside the 64 MiB limit.",
        };
    }

    const auto encoded_path = path_to_utf8(path);
    SDL_IOStream* const stream =
        SDL_IOFromFile(encoded_path.c_str(), "rb");
    if (stream == nullptr) {
        return {
            BackglassLoadStatus::io_error,
            nullptr,
            SDL_GetError(),
        };
    }
    const bool supported =
        IMG_isPNG(stream) || IMG_isJPG(stream) || IMG_isWEBP(stream);
    if (!supported) {
        SDL_CloseIO(stream);
        return {
            BackglassLoadStatus::unsupported_format,
            nullptr,
            "The file does not contain PNG, JPEG, or WebP data.",
        };
    }

    SDL_Surface* const surface = IMG_Load_IO(stream, true);
    if (surface == nullptr) {
        return {
            BackglassLoadStatus::decode_error,
            nullptr,
            SDL_GetError(),
        };
    }
    const auto pixel_count =
        static_cast<std::uint64_t>(std::max(surface->w, 0))
        * static_cast<std::uint64_t>(std::max(surface->h, 0));
    if (surface->w <= 0 || surface->h <= 0
        || surface->w > kMaximumDimension
        || surface->h > kMaximumDimension
        || pixel_count > kMaximumPixels) {
        SDL_DestroySurface(surface);
        return {
            BackglassLoadStatus::dimensions_exceeded,
            nullptr,
            "Decoded dimensions exceed 16384 pixels per side or "
            "64 megapixels.",
        };
    }

    return {
        BackglassLoadStatus::loaded,
        surface,
        "Backglass image loaded.",
    };
}

} // namespace devils_crush::render
