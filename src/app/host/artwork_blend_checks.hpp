// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

[[nodiscard]] bool check_playfield_rotation();

[[nodiscard]] bool check_artwork_blending(SDL_Renderer* renderer);

[[nodiscard]] bool check_viewport_artwork(SDL_Renderer* renderer, PceRuntime& runtime,
    const std::filesystem::path& directory);

} // namespace devils_crush::app::host
