// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_collision_seed.hpp"

namespace devils_crush::native_runtime {

bool seed_actor_collision_state(
    ActorState& actors,
    const GameStateView::ActorSlot& source) noexcept
{
    const auto* target = actors.actor(source.index);
    if (target == nullptr || !target->active()) {
        return false;
    }

    return actors.configure_collision_table(
               source.index,
               source.collision_table_program,
               source.collision_table)
        && actors.configure_collision_reaction(
            source.index,
            source.collision_reaction);
}

} // namespace devils_crush::native_runtime
