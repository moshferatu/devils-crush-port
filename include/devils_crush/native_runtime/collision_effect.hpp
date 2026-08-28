// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

// The shared type-$8F child created by 03:7BF6 starts with this eight-turn
// lifetime. Rendering its burst is a separate presentation concern; this
// state owns only the allocator-visible association and lifetime.
inline constexpr std::uint8_t kCollisionEffectLifetime = 8U;

struct CollisionEffectState {
    std::uint8_t source_slot = 0U;
    // Guards a newly claimed source slot from an old effect's cleanup.
    std::uint32_t source_generation = 0U;
    // 03:7C09 saves the source's full $3D00 byte in $3E80,Y. The effect
    // restores it after its scheduled eight-turn wait.
    std::uint8_t source_association = 0U;
    // 03:7C1E copies the source's $3900 appearance byte into the child.
    std::uint8_t source_appearance = 0U;
    std::uint8_t ticks_remaining = kCollisionEffectLifetime;

    [[nodiscard]] bool operator==(const CollisionEffectState&) const = default;
};

enum class CollisionEffectAdvanceDisposition : std::uint8_t {
    countdown,
    expired,
    invalid_state,
};

struct CollisionEffectAdvanceStep {
    CollisionEffectState previous;
    CollisionEffectState state;
    CollisionEffectAdvanceDisposition disposition =
        CollisionEffectAdvanceDisposition::invalid_state;

    [[nodiscard]] bool operator==(const CollisionEffectAdvanceStep&) const = default;
};

// Advances the recovered eight-turn child lifetime. The caller owns the
// actor-record release and source-association cleanup at expiration.
[[nodiscard]] CollisionEffectAdvanceStep advance_collision_effect(
    CollisionEffectState state) noexcept;

} // namespace devils_crush::native_runtime
