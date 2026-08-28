// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/collision_table_driver.hpp"

namespace devils_crush::native_runtime {

std::optional<ActorCollisionTableResolutionUpdate>
advance_actor_collision_table_from_asset(
    ActorState& actors,
    const std::uint8_t slot,
    const MappedRomView& rom) noexcept
{
    const auto* actor = actors.actor(slot);
    if (actor == nullptr || !actor->active() || !actor->collision_table
        || !actor->collision_table_program) {
        return std::nullopt;
    }

    const auto& table = *actor->collision_table;
    const auto resolution = resolve_collision_table_record(
        rom,
        actor->collision_table_program->selector,
        {
            .entry = table.collision_entry_cursor,
            .motion = table.record_cursor,
        });
    return actors.apply_collision_table_resolution(slot, resolution);
}

} // namespace devils_crush::native_runtime
