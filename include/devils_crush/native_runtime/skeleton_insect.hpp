// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"
#include "devils_crush/native_runtime/table_rule_latches.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kLeftSkeletonInsectSlot = 0x31U;
inline constexpr std::uint8_t kCenterSkeletonInsectSlot = 0x32U;
inline constexpr std::uint8_t kRightSkeletonInsectSlot = 0x33U;

enum class SkeletonInsectKind : std::uint8_t {
    left,
    center,
    right,
};

[[nodiscard]] constexpr std::optional<SkeletonInsectKind>
skeleton_insect_kind_for_slot(const std::uint8_t slot) noexcept
{
    if (slot == kLeftSkeletonInsectSlot) {
        return SkeletonInsectKind::left;
    }
    if (slot == kCenterSkeletonInsectSlot) {
        return SkeletonInsectKind::center;
    }
    if (slot == kRightSkeletonInsectSlot) {
        return SkeletonInsectKind::right;
    }
    return std::nullopt;
}

enum class SkeletonInsectLifecycle : std::uint8_t {
    waiting_for_source,
    transition_pending,
    post_trigger,
};

struct SkeletonInsectState {
    SkeletonInsectKind kind = SkeletonInsectKind::left;
    SkeletonInsectLifecycle lifecycle = SkeletonInsectLifecycle::waiting_for_source;

    [[nodiscard]] bool operator==(const SkeletonInsectState&) const = default;
};

enum class SkeletonInsectTriggerDisposition : std::uint8_t {
    waiting_for_source,
    triggered,
    transition_pending,
    post_trigger,
};

struct SkeletonInsectTriggerStep {
    SkeletonInsectState previous;
    SkeletonInsectState state;
    SkeletonInsectTriggerDisposition disposition =
        SkeletonInsectTriggerDisposition::waiting_for_source;
    std::uint16_t score_award_units = 0;
    // The source target owns the audible feedback; these trigger paths do not
    // issue an audio command themselves.
    std::uint8_t audio_command = 0;
    std::uint8_t requested_startup_rounds = 0;

    [[nodiscard]] bool operator==(const SkeletonInsectTriggerStep&) const = default;
};

[[nodiscard]] SkeletonInsectTriggerStep consume_skeleton_insect_source(
    SkeletonInsectState state,
    const TableRuleLatches& latches) noexcept;
[[nodiscard]] SkeletonInsectState complete_skeleton_insect_transition(
    SkeletonInsectState state) noexcept;

struct SkeletonInsectRulesUpdate {
    SkeletonInsectTriggerStep insect;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const SkeletonInsectRulesUpdate&) const = default;
};

[[nodiscard]] SkeletonInsectRulesUpdate resolve_skeleton_insect_source(
    SkeletonInsectState state,
    const TableRuleLatches& latches,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
