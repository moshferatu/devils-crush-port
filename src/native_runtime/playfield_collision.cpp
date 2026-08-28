// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/playfield_collision.hpp"

#include <limits>

namespace devils_crush::native_runtime {

PlayfieldCollisionScan scan_playfield_collisions(
    const BallSubstepAdvance movement,
    const CollisionMap& map) noexcept
{
    PlayfieldCollisionScan scan{.movement = movement};
    for (std::uint8_t index = 0U; index < movement.completed_steps; ++index) {
        const auto& ball = movement.steps[index].state;
        const auto horizontal = ball.position_x.whole_pixels();
        const auto vertical = ball.position_y.whole_pixels();
        auto& probe = scan.probes[index];
        probe.substep_index = index;
        if (horizontal >= 0
            && horizontal <= std::numeric_limits<std::uint16_t>::max()
            && vertical >= 0
            && vertical <= std::numeric_limits<std::uint16_t>::max()) {
            probe.position_in_world = true;
            probe.collision = evaluate_playfield_collision(
                map,
                PlayfieldCollisionProbe{
                    .horizontal_pixels = static_cast<std::uint16_t>(horizontal),
                    .vertical_pixels = static_cast<std::uint16_t>(vertical),
                });
            if (!scan.first_solid_substep
                && probe.collision.disposition
                    == PlayfieldCollisionDisposition::solid) {
                scan.first_solid_substep = index;
                ++scan.completed_probes;
                break;
            }
        }
        ++scan.completed_probes;
    }
    return scan;
}

std::optional<PlayfieldContact> first_playfield_contact(
    const PlayfieldCollisionScan& scan) noexcept
{
    if (!scan.first_solid_substep
        || *scan.first_solid_substep >= scan.completed_probes) {
        return std::nullopt;
    }

    const auto index = *scan.first_solid_substep;
    const auto& probe = scan.probes[index];
    if (probe.collision.disposition != PlayfieldCollisionDisposition::solid) {
        return std::nullopt;
    }

    const auto& movement = scan.movement.steps[index];
    return PlayfieldContact{
        .substep_index = index,
        .entry_ball = movement.previous,
        .impact_ball = movement.state,
        .impact = probe.collision,
    };
}

} // namespace devils_crush::native_runtime
