// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/game_state.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// Per-ball collision bookkeeping. The latch records that an earlier contact
// has already been handled; a new target is deferred while it is set. The
// state also remembers the most recent target to suppress an immediate repeat
// contact.
struct ContactState {
    std::optional<std::uint8_t> last_target;
    bool contact_latched = false;

    [[nodiscard]] bool operator==(const ContactState&) const = default;
};

// Table-specific code maps this response kind to a named target behavior. The
// dispatcher only decides whether that behavior should run for this contact.
struct ContactTarget {
    std::uint8_t actor_slot = 0;
    std::uint8_t response_kind = 0;
    bool marked_for_response = false;

    [[nodiscard]] bool operator==(const ContactTarget&) const = default;
};

// CBCF receives an already-overlapping active actor. It records the selected
// slot and masks `$3000,Y` to the low six-bit type before choosing the
// response vector. Keep that capture conversion here so callers cannot
// mistake scheduler flags or actor-local collision-table state for the
// response kind. An inactive slot cannot be a confirmed-contact target.
[[nodiscard]] std::optional<ContactTarget> make_confirmed_contact_target(
    const GameStateView::ActorSlot& actor) noexcept;

enum class ContactDisposition {
    repeated_target,
    contact_already_handled,
    dispatched,
};

struct ContactDispatch {
    ContactState state;
    ContactTarget target;
    ContactDisposition disposition = ContactDisposition::repeated_target;

    [[nodiscard]] bool operator==(const ContactDispatch&) const = default;
};

// Resolves a confirmed ball-to-actor contact. A dispatched target is marked,
// becomes the remembered target, and yields its response kind to the table
// behavior layer. Response kinds at or above two release the per-ball latch
// for another contact in the same frame.
[[nodiscard]] ContactDispatch dispatch_contact(
    ContactState state,
    ContactTarget target) noexcept;

} // namespace devils_crush::native_runtime
