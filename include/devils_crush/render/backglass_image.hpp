// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <SDL3/SDL_surface.h>

#include <filesystem>
#include <string>

namespace devils_crush::render {

enum class BackglassLoadStatus {
    loaded,
    io_error,
    unsupported_format,
    decode_error,
    dimensions_exceeded,
};

struct BackglassLoadResult {
    BackglassLoadStatus status = BackglassLoadStatus::io_error;
    SDL_Surface* surface = nullptr;
    std::string message;

    BackglassLoadResult() = default;
    BackglassLoadResult(
        BackglassLoadStatus status_value,
        SDL_Surface* surface_value,
        std::string message_value);
    ~BackglassLoadResult();
    BackglassLoadResult(const BackglassLoadResult&) = delete;
    BackglassLoadResult& operator=(const BackglassLoadResult&) = delete;
    BackglassLoadResult(BackglassLoadResult&& other) noexcept;
    BackglassLoadResult& operator=(BackglassLoadResult&& other) noexcept;

    [[nodiscard]] bool loaded() const noexcept
    {
        return status == BackglassLoadStatus::loaded
            && surface != nullptr;
    }
};

[[nodiscard]] BackglassLoadResult load_backglass_image(
    const std::filesystem::path& path);

} // namespace devils_crush::render
