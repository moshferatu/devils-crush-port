// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/ball_physics.hpp"
#include "devils_crush/native_runtime/collision_map.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// One post-motion collision-map sample. The adaptive ball loop retains every
// completed substep, so a fast ball receives the same intermediate table
// checks as a slow one without exposing collision scratch memory.
struct BallPlayfieldProbe {
    std::uint8_t substep_index = 0;
    bool position_in_world = false;
    PlayfieldCollisionEvaluation collision;

    [[nodiscard]] bool operator==(const BallPlayfieldProbe&) const = default;
};

struct PlayfieldCollisionScan {
    BallSubstepAdvance movement;
    std::array<BallPlayfieldProbe, 4> probes{};
    std::uint8_t completed_probes = 0;
    std::optional<std::uint8_t> first_solid_substep;

    [[nodiscard]] bool operator==(const PlayfieldCollisionScan&) const
        = default;
};

// Samples the main playfield after each ball-motion substep, stopping at the
// first solid result because the cartridge immediately hands that contact to
// its response path. Response and target-rule behavior deliberately remain
// owned by later physics stages.
[[nodiscard]] PlayfieldCollisionScan scan_playfield_collisions(
    BallSubstepAdvance movement,
    const CollisionMap& map) noexcept;

// The first confirmed solid sample expressed as a native physics handoff.
// `entry_ball` is the state immediately before the colliding substep and
// `impact_ball` is the sampled state inside the solid map cell. A later
// resolver owns rewinding, normal selection, velocity response, and table
// rules; this value records only the observed contact.
struct PlayfieldContact {
    std::uint8_t substep_index = 0;
    BallState entry_ball;
    BallState impact_ball;
    PlayfieldCollisionEvaluation impact;

    [[nodiscard]] bool operator==(const PlayfieldContact&) const = default;
};

[[nodiscard]] std::optional<PlayfieldContact> first_playfield_contact(
    const PlayfieldCollisionScan& scan) noexcept;

} // namespace devils_crush::native_runtime
