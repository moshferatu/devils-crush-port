// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_table.hpp"
#include "devils_crush/native_runtime/mapped_rom.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

// The collision-table ROM adapter keeps the selector and motion directories
// at this boundary. Native actor behavior receives a decoded record rather
// than logical addresses or MPR state.
inline constexpr std::uint16_t kCollisionTableSelectorDirectory = 0x6A38U;
inline constexpr std::uint16_t kCollisionTableMotionDirectory = 0x6C3AU;

// Resolves the next decoded record for one collision selector. `$FE` restarts
// the table from its first entry, `$FF` ends it, and `$C0` advances to the
// next collision entry before reading again. A bounded traversal prevents a
// malformed ROM stream from becoming an unbounded native update.
[[nodiscard]] CollisionTableRecordResolution resolve_collision_table_record(
    const MappedRomView& rom,
    std::uint8_t selector,
    CollisionTableCursor cursor) noexcept;

} // namespace devils_crush::native_runtime
