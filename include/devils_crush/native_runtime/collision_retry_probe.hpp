// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/collision_detection.hpp"
#include "devils_crush/native_runtime/collision_retry.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

// The retry probe's short path either clears the collision immediately or
// hands the retry position to the still-unrecovered full geometry scan.
enum class CollisionRetryProbeDisposition : std::uint8_t {
    clear,
    requires_full_scan,
};

struct CollisionRetryProbeResult {
    CollisionRetryStep retry;
    // CEB5 stores this low byte in `$65` before deciding whether CED2 runs.
    std::uint8_t adjusted_horizontal_low_byte = 0;
    // The high byte produced by the probe's carry-sensitive X comparison.
    std::uint8_t adjusted_horizontal_high_byte = 0;
    // The low whole-pixel byte that determines whether a narrow X candidate
    // enters the full scanner.
    std::uint8_t vertical_low_byte = 0;
    CollisionRetryProbeDisposition disposition =
        CollisionRetryProbeDisposition::clear;

    [[nodiscard]] bool operator==(const CollisionRetryProbeResult&) const
        = default;
};

// Evaluates the proven short probe at 01:CEB5. It preserves the original
// carry dependency from the retry's vertical integer addition and deliberately
// reports rather than implements the unobserved full-scanner continuation.
[[nodiscard]] CollisionRetryProbeResult probe_collision_retry(
    const CollisionRetryStep& retry) noexcept;

// Materializes CED2's `$65` and `$66:$67` collision inputs only after the
// short retry probe selected its full-scan branch. CEB5 supplies the adjusted
// horizontal and retry vertical low bytes; `$67` is deliberately an explicit
// parent-scan input because the retry does not rewrite it.
[[nodiscard]] std::optional<CollisionProbe>
make_collision_retry_full_scan_probe(
    const CollisionRetryProbeResult& retry_probe,
    std::uint8_t parent_vertical_high_byte) noexcept;

} // namespace devils_crush::native_runtime
