// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/contact_dispatch.hpp"
#include "devils_crush/native_runtime/giant_skeleton.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// Delivers the Giant Skeleton's collision to fixed slot $30, but runs its
// direction-selected appearance on every active actor turn. The state is
// explicitly seeded because table setup/capture is still a separate owner.
class GiantSkeletonRuleScheduler {
public:
    explicit GiantSkeletonRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] bool queue_confirmed_contact(
        const ContactDispatch& contact) noexcept;
    [[nodiscard]] std::optional<GiantSkeletonRulesUpdate> advance_actor(
        std::uint8_t slot,
        GiantSkeletonTargetPosition target) noexcept;
    void seed(GiantSkeletonState state) noexcept;
    [[nodiscard]] const GiantSkeletonState* giant_skeleton() const noexcept;

private:
    ScoreState& score_;
    std::optional<GiantSkeletonState> giant_skeleton_;
    bool contact_queued_ = false;
};

} // namespace devils_crush::native_runtime
