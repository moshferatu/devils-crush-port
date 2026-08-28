// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_guard.hpp"

namespace devils_crush::native_runtime {

TowerGuardSelectorStep select_tower_guard_selector(const TowerState& tower) noexcept
{
    if (tower.remaining_armed_passes != 0U) {
        return {
            .remaining_armed_passes = tower.remaining_armed_passes,
            .completion_selector = kTowerGuardClosedSelector,
            .disposition = TowerGuardSelectorDisposition::closed,
        };
    }
    return {
        .remaining_armed_passes = 0U,
        .completion_selector = kTowerGuardOpenSelector,
        .disposition = TowerGuardSelectorDisposition::open_bonus_stage_entrance,
    };
}

} // namespace devils_crush::native_runtime
