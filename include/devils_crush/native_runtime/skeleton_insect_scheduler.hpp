// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/skeleton_insect.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// Runs the three source-gated Skeleton Insects at their fixed actor-order
// turns. A source latch can remain set after the trigger; the lifecycle gate
// makes the score event one-shot before presentation takes over.
class SkeletonInsectRuleScheduler {
public:
    explicit SkeletonInsectRuleScheduler(ScoreState& score) noexcept;

    [[nodiscard]] std::optional<SkeletonInsectRulesUpdate> advance_actor(
        std::uint8_t slot,
        const TableRuleLatches& latches) noexcept;
    [[nodiscard]] bool complete_transition(std::uint8_t slot) noexcept;
    [[nodiscard]] bool seed(SkeletonInsectState state) noexcept;
    [[nodiscard]] const SkeletonInsectState* insect(
        SkeletonInsectKind kind) const noexcept;

private:
    [[nodiscard]] static std::optional<std::size_t> index_for(
        SkeletonInsectKind kind) noexcept;

    ScoreState& score_;
    std::array<SkeletonInsectState, 3U> insects_{{
        {.kind = SkeletonInsectKind::left},
        {.kind = SkeletonInsectKind::center},
        {.kind = SkeletonInsectKind::right},
    }};
};

} // namespace devils_crush::native_runtime
