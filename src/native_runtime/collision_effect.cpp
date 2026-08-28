// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_effect.hpp"

namespace devils_crush::native_runtime {

CollisionEffectAdvanceStep advance_collision_effect(
    CollisionEffectState state) noexcept
{
    const auto previous = state;
    if (state.ticks_remaining == 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = CollisionEffectAdvanceDisposition::invalid_state,
        };
    }

    --state.ticks_remaining;
    return {
        .previous = previous,
        .state = state,
        .disposition = state.ticks_remaining == 0U
            ? CollisionEffectAdvanceDisposition::expired
            : CollisionEffectAdvanceDisposition::countdown,
    };
}

} // namespace devils_crush::native_runtime
