// SPDX-License-Identifier: GPL-3.0-or-later
#include "table_motion.hpp"

namespace devils_crush::app::host {

// Bonus rooms are drawn as pieces only while one is on screen; everything else
// the framebuffer shows keeps blending complete pictures.
void update_bonus_room_motion(SDL_Renderer* renderer, PceRuntime& runtime,
    const std::optional<devils_crush::cabinet::CabinetState>& state,
    const FramebufferView& view, ViewportMotionPresenter& motion)
{
    const auto video = state && state->bonus_stage && state->screen_selector != 0U
        ? runtime.export_video_memory() : std::nullopt;
    if (!video) {
        motion.reset();
        return;
    }
    motion.update(renderer, *video, view);
}

} // namespace devils_crush::app::host
