// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace devils_crush::render {

// Presentation-only snapshots. Sprite order is the software compositor's
// back-to-front order; pieces share their actor's motion, not SAT indices.
struct TableMotionSprite {
    cabinet::TableActorState actor;
    int x = 0;
    int y = 0;
    bool foreground = true;
    ArgbImage image;
};

struct TableMotionFrame {
    PceBackgroundLayer background;
    std::vector<TableMotionSprite> sprites;
};

inline bool table_art_linked(const cabinet::TableActorState& current,
    const cabinet::TableActorState* previous) noexcept
{
    return previous && previous->occupied() && current.occupied()
        && current.slot == previous->slot && current.type == previous->type
        && std::abs(previous->world_x - current.world_x) <= 16
        && std::abs(previous->world_y - current.world_y) <= 16;
}

inline bool table_motion_linked(const cabinet::TableActorState& current,
    const cabinet::TableActorState* previous) noexcept
{
    return table_art_linked(current, previous)
        && current.appearance == previous->appearance;
}

// The same three-position filter used by the Zombies port: constant motion
// stays where it is; alternating integer steps approach their underlying line.
// A small stop can overshoot visually for one tick, so corrections are bounded
// to half a pixel for the ball and one pixel for other actors. Sharp reversals
// and cuts keep the original position, rather than smoothing across a collision.
inline std::pair<float, float> table_motion_correction(
    const cabinet::TableActorState& current,
    const cabinet::TableActorState* previous,
    const cabinet::TableActorState* older) noexcept
{
    if (!table_motion_linked(current, previous)
        || !table_motion_linked(*previous, older)) return {};
    const int dx = current.world_x - previous->world_x;
    const int dy = current.world_y - previous->world_y;
    const int qx = previous->world_x - older->world_x;
    const int qy = previous->world_y - older->world_y;
    const int limit = current.slot == cabinet::kBallActorSlot ? 2 : 4;
    if (dx * qx < 0 || dy * qy < 0
        || std::abs(dx - qx) > limit || std::abs(dy - qy) > limit) return {};
    return {static_cast<float>(qx - dx) * 0.25F,
        static_cast<float>(qy - dy) * 0.25F};
}

inline std::pair<float, float> table_motion_offset(
    const cabinet::TableActorState& current,
    const cabinet::TableActorState* previous,
    float alpha) noexcept
{
    // Reused slots, spawns and teleports are cuts. Do not compare dispatch:
    // a normal coroutine changes that pointer while an actor keeps moving.
    if (!table_motion_linked(current, previous)) return {};
    const int dx = previous->world_x - current.world_x;
    const int dy = previous->world_y - current.world_y;
    const float remaining = 1.0F - std::clamp(alpha, 0.0F, 1.0F);
    return {dx * remaining, dy * remaining};
}

} // namespace devils_crush::render
