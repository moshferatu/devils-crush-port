// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kLeftSideSpiderSlot = 0x28U;
inline constexpr std::uint8_t kRightSideSpiderSlot = 0x29U;

[[nodiscard]] constexpr std::optional<std::uint8_t> side_spider_side_for_slot(
    const std::uint8_t slot) noexcept
{
    if (slot == kLeftSideSpiderSlot) {
        return std::uint8_t{0U};
    }
    if (slot == kRightSideSpiderSlot) {
        return std::uint8_t{1U};
    }
    return std::nullopt;
}

// The paired Side Spiders are one-hit side-shoot targets. Their shared
// disappearance controller is presentation state, so this rule records only
// the collision-ready gate until that owner completes the transition.
struct SideSpiderState {
    std::uint8_t side = 0;
    bool collision_ready = true;

    [[nodiscard]] bool operator==(const SideSpiderState&) const = default;
};

enum class SideSpiderContactDisposition : std::uint8_t {
    hit,
    transition_pending,
    invalid_state,
};

struct SideSpiderContactStep {
    SideSpiderState previous;
    SideSpiderState state;
    SideSpiderContactDisposition disposition =
        SideSpiderContactDisposition::transition_pending;
    std::uint16_t score_award_units = 0;
    std::uint8_t audio_command = 0;
    bool sets_shared_transition_latch = false;

    [[nodiscard]] bool operator==(const SideSpiderContactStep&) const = default;
};

[[nodiscard]] SideSpiderContactStep consume_side_spider_contact(
    SideSpiderState state) noexcept;
[[nodiscard]] SideSpiderState complete_side_spider_transition(
    SideSpiderState state) noexcept;

struct SideSpiderRulesUpdate {
    SideSpiderContactStep spider;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const SideSpiderRulesUpdate&) const = default;
};

[[nodiscard]] SideSpiderRulesUpdate resolve_side_spider_contact(
    SideSpiderState state,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
