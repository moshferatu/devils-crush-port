// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_reaction.hpp"

namespace devils_crush::native_runtime {

CollisionReactionStep advance_collision_reaction(
    CollisionReactionState state) noexcept
{
    const auto previous = state;
    const auto response_code = static_cast<std::uint8_t>(
        static_cast<std::uint8_t>((state.collision_event & 0xFEU) << 1U)
        + static_cast<std::uint8_t>(state.phase & 0x03U));
    state.ticks_until_next_phase = static_cast<std::uint8_t>(
        state.ticks_until_next_phase - 1U);
    const auto phase_advanced = state.ticks_until_next_phase == 0U;
    if (phase_advanced) {
        state.ticks_until_next_phase = 4U;
        state.phase = static_cast<std::uint8_t>(state.phase + 1U);
    }

    return {
        .previous = previous,
        .state = state,
        .response_code = response_code,
        .phase_advanced = phase_advanced,
    };
}

} // namespace devils_crush::native_runtime
