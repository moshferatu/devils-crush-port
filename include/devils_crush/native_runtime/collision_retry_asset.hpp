// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_retry.hpp"
#include "devils_crush/native_runtime/mapped_rom.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// CDF8 tries three little-endian signed X/Y whole-pixel vectors for each
// incoming-motion quadrant before it asks its caller for another quadrant.
// The mapper keeps this logical address and its MPR dependency outside native
// gameplay state.
inline constexpr std::uint16_t kCollisionRetryOffsetTable = 0xCEFBU;

// Decodes all CDF8 retry candidates. The outer index is the direction order
// selected by classify_collision_retry_direction; the inner index is CDF8's
// ordered attempt within that quadrant.
[[nodiscard]] std::optional<CollisionRetryOffsetAttempts>
decode_collision_retry_offset_attempts(const MappedRomView& rom) noexcept;

// Decodes CDF8's first retry candidate for each direction. This compatibility
// adapter leaves existing single-attempt native callers unchanged while the
// placement solver is ported separately.
[[nodiscard]] std::optional<CollisionRetryOffsets>
decode_collision_retry_offsets(const MappedRomView& rom) noexcept;

} // namespace devils_crush::native_runtime
