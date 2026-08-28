// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// The named collision-response families selected by table state. These are
// velocity transforms, not collision detection; contact selection remains a
// separate physics concern.
enum class CollisionResponseMode {
    normal,
    reduced,
    half,
    minimal,
};

// Parameters for the normal response. The ROM stores these as bytes, but the
// native boundary gives each role a physics name. The existing table adapter
// will supply them from the current table configuration until it too is native.
struct NormalCollisionResponseTuning {
    std::uint8_t negative_low_byte_decrement = 0;
    std::uint8_t positive_low_byte_increment = 0;
    std::uint8_t positive_high_byte_increment = 0;

    [[nodiscard]] bool operator==(const NormalCollisionResponseTuning&) const
        = default;
};

struct CollisionResponseConfig {
    CollisionResponseMode requested_mode = CollisionResponseMode::normal;
    // The active bonus/table state promotes a normal response to the reduced
    // family before calculating velocity.
    bool force_reduced_mode = false;
    // One reduced-state variant uses a quarter rather than three-sixteenths
    // of the incoming signed velocity.
    bool use_quarter_reduced_scale = false;
    NormalCollisionResponseTuning normal_tuning;

    [[nodiscard]] bool operator==(const CollisionResponseConfig&) const
        = default;
};

struct CollisionResponse {
    std::int16_t incoming_velocity = 0;
    std::int16_t outgoing_velocity = 0;
    CollisionResponseMode effective_mode = CollisionResponseMode::normal;
    // A normal response caps the signed high-byte range used by the original
    // table physics. Scaled responses never apply this cap.
    bool clamped = false;

    [[nodiscard]] bool operator==(const CollisionResponse&) const = default;
};

// A signed 8.8 ball-motion pair, kept distinct from BallState so collision
// response can be recovered without assuming ownership of position or other
// frame physics state.
struct CollisionResponseVector {
    std::int16_t horizontal = 0;
    std::int16_t vertical = 0;

    [[nodiscard]] bool operator==(const CollisionResponseVector&) const = default;
};

// The orthonormal contact basis recovered from CF33's D081 lookup. `cosine`
// and `sine` describe the local tangent in table coordinates; a later target
// geometry owner supplies this basis from the selected target and its state.
struct CollisionResponseBasis {
    std::int16_t cosine = 0x0100;
    std::int16_t sine = 0;

    [[nodiscard]] bool operator==(const CollisionResponseBasis&) const = default;
};

// CF33 has two source legs that differ only in when they negate the projected
// normal around CFD8. Naming that order prevents the clear-selector CF5B leg
// from being treated as the signed-selector CF3F reflection.
enum class CollisionNormalReflection {
    before_response,
    after_response,
};

// CF33's readable ball-response calculation. The ball is projected into the
// contact basis, its local normal is transformed through CFD8's recovered
// response family, then the resulting vector is projected back into table
// coordinates. `normal` records CFD8's direct input/output; the selected
// reflection order determines whether its output is negated before inversion.
// Selecting the basis and response config is intentionally separate
// target/table behavior.
struct OrientedCollisionResponse {
    CollisionResponseVector incoming;
    CollisionResponseVector local;
    CollisionResponse normal;
    CollisionResponseVector outgoing;

    [[nodiscard]] bool operator==(const OrientedCollisionResponse&) const
        = default;
};

// Applies one component of the original signed 8.8 collision response. This
// preserves its asymmetric normal-mode cap and arithmetic scaling so ball and
// actor collision code can use a named, ROM-free physics operation.
[[nodiscard]] CollisionResponse apply_collision_response(
    std::int16_t velocity,
    const CollisionResponseConfig& config) noexcept;

[[nodiscard]] OrientedCollisionResponse apply_oriented_collision_response(
    CollisionResponseVector velocity,
    CollisionResponseBasis basis,
    const CollisionResponseConfig& config,
    CollisionNormalReflection reflection =
        CollisionNormalReflection::before_response) noexcept;

} // namespace devils_crush::native_runtime
