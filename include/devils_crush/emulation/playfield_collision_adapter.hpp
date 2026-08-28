// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace devils_crush::emulation {

// Capture-side evidence for one invocation of the cartridge's playfield tile
// probe. It joins the typed table pointers from work RAM to the MPR state that
// resolves their logical addresses. This remains an emulator adapter: native
// gameplay consumes only decoded CollisionMapAsset data.
struct PlayfieldCollisionSource {
    native_runtime::GameStateView::PlayfieldCollisionTablePointers tables;
    std::array<std::uint8_t, 8> mpr{};

    [[nodiscard]] bool operator==(const PlayfieldCollisionSource&) const
        = default;
};

[[nodiscard]] std::optional<PlayfieldCollisionSource>
capture_playfield_collision_source(
    const CpuRegisters& registers,
    std::span<const std::byte> work_ram) noexcept;

} // namespace devils_crush::emulation
