// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/actor_motion_capture.hpp"

namespace devils_crush::native_runtime {

std::optional<SharedActorMotionCapture> capture_shared_actor_motion(
    const GameStateView& state,
    const std::uint8_t slot) noexcept
{
    const auto actor = state.actor(slot);
    if (!actor || (actor->activity_flags & 0x80U) == 0U) {
        return std::nullopt;
    }

    const auto due_for_update = (actor->attributes & 0x80U) != 0U
        || (actor->attributes & 0x02U) == 0U;
    return SharedActorMotionCapture{
        .actor = *actor,
        .motion = {
            .position_x = Fixed24_8{
                .raw = static_cast<std::int32_t>(actor->collision_table.position_x)
                    * 256,
            },
            .position_y = Fixed24_8{
                .raw = static_cast<std::int32_t>(actor->collision_table.position_y)
                    * 256,
            },
            .velocity_x = actor->horizontal_motion.raw,
            .velocity_y = actor->vertical_motion.raw,
        },
        .due_for_update = due_for_update,
    };
}

bool seed_shared_actor_motion(
    ActorState& actors,
    const SharedActorMotionCapture& source) noexcept
{
    auto* target = actors.actor(source.actor.index);
    if (target == nullptr || target->behavior != ActorBehavior::shared_motion) {
        return false;
    }

    target->motion = source.motion;
    target->due_for_update = source.due_for_update;
    return true;
}

} // namespace devils_crush::native_runtime
