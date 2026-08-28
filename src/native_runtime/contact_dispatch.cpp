// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/contact_dispatch.hpp"

namespace devils_crush::native_runtime {

std::optional<ContactTarget> make_confirmed_contact_target(
    const GameStateView::ActorSlot& actor) noexcept
{
    if ((actor.activity_flags & 0x80U) == 0U) {
        return std::nullopt;
    }
    return ContactTarget{
        .actor_slot = actor.index,
        .response_kind = static_cast<std::uint8_t>(actor.activity_flags & 0x3FU),
    };
}

ContactDispatch dispatch_contact(
    ContactState state,
    ContactTarget target) noexcept
{
    if (state.last_target == target.actor_slot) {
        return {
            .state = state,
            .target = target,
            .disposition = ContactDisposition::repeated_target,
        };
    }
    if (state.contact_latched) {
        return {
            .state = state,
            .target = target,
            .disposition = ContactDisposition::contact_already_handled,
        };
    }

    target.marked_for_response = true;
    state.last_target = target.actor_slot;
    if (target.response_kind >= 2U) {
        state.contact_latched = false;
    }
    return {
        .state = state,
        .target = target,
        .disposition = ContactDisposition::dispatched,
    };
}

} // namespace devils_crush::native_runtime
