// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/emulation/playfield_collision_adapter.hpp"

namespace devils_crush::emulation {

std::optional<PlayfieldCollisionSource> capture_playfield_collision_source(
    const CpuRegisters& registers,
    const std::span<const std::byte> work_ram) noexcept
{
    const native_runtime::GameStateView state(work_ram);
    const auto tables = state.playfield_collision_table_pointers();
    if (!tables) {
        return std::nullopt;
    }
    return PlayfieldCollisionSource{
        .tables = *tables,
        .mpr = registers.mpr,
    };
}

} // namespace devils_crush::emulation
