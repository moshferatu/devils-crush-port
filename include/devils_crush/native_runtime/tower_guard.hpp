// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/tower.hpp"

#include <cstdint>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kTowerGuardSlot = 0x2EU;
inline constexpr std::uint8_t kTowerGuardClosedSelector = 0x84U;
inline constexpr std::uint8_t kTowerGuardOpenSelector = 0x8EU;

enum class TowerGuardSelectorDisposition : std::uint8_t {
    closed,
    open_bonus_stage_entrance,
};

// The fixed Guard at 03:60A6 has no independent progression. Each update
// derives its completion selector from the Tower-owned five-pass gate.
struct TowerGuardSelectorStep {
    std::uint8_t remaining_armed_passes = 0;
    std::uint8_t completion_selector = kTowerGuardClosedSelector;
    TowerGuardSelectorDisposition disposition = TowerGuardSelectorDisposition::closed;

    [[nodiscard]] bool operator==(const TowerGuardSelectorStep&) const = default;
};

[[nodiscard]] TowerGuardSelectorStep select_tower_guard_selector(
    const TowerState& tower) noexcept;

} // namespace devils_crush::native_runtime
