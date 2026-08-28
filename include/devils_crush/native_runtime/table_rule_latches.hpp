// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace devils_crush::native_runtime {

// Cross-target gameplay latches owned by the table-rule domain. The original
// target-local handlers communicate through these flags; neither a collision
// dispatcher nor presentation code has to infer their meaning from RAM.
struct TableRuleLatches {
    bool left_front_spider_safety = false;
    bool right_front_spider_safety = false;
    bool tower_completed = false;
    bool side_spider_transition_requested = false;

    [[nodiscard]] bool operator==(const TableRuleLatches&) const = default;
};

} // namespace devils_crush::native_runtime
