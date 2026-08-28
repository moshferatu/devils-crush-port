// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kLeftFrontSpiderSlot = 0x26U;
inline constexpr std::uint8_t kRightFrontSpiderSlot = 0x27U;

[[nodiscard]] constexpr std::optional<std::uint8_t> front_spider_side_for_slot(
    const std::uint8_t slot) noexcept
{
    if (slot == kLeftFrontSpiderSlot) {
        return std::uint8_t{0U};
    }
    if (slot == kRightFrontSpiderSlot) {
        return std::uint8_t{1U};
    }
    return std::nullopt;
}

// The two fixed Front Spiders share a one-hit rule. The appearance controller
// owns the elapsed four-stage animation; this state only records whether its
// collision-ready steady form is available for another confirmed contact.
struct FrontSpiderState {
    std::uint8_t side = 0;
    bool collision_ready = true;

    [[nodiscard]] bool operator==(const FrontSpiderState&) const = default;
};

enum class FrontSpiderContactDisposition : std::uint8_t {
    hit,
    transition_pending,
    invalid_state,
};

struct FrontSpiderContactStep {
    FrontSpiderState previous;
    FrontSpiderState state;
    FrontSpiderContactDisposition disposition =
        FrontSpiderContactDisposition::transition_pending;
    std::uint16_t score_award_units = 0;
    std::uint8_t audio_command = 0;
    bool sets_side_safety = false;
    std::uint8_t requested_transition_stages = 0;

    [[nodiscard]] bool operator==(const FrontSpiderContactStep&) const = default;
};

// Consumes one confirmed contact. A hit sets the side-selected trap-door or
// launcher safety, awards 4,000 points, requests the observed four-stage
// appearance transition, and makes the target unavailable until completion.
[[nodiscard]] FrontSpiderContactStep consume_front_spider_contact(
    FrontSpiderState state) noexcept;

// Applies the transition completion supplied by the appearance owner. No
// elapsed-timer approximation is embedded in this target rule.
[[nodiscard]] FrontSpiderState complete_front_spider_transition(
    FrontSpiderState state) noexcept;

struct FrontSpiderRulesUpdate {
    FrontSpiderContactStep spider;
    ScoreAwardStep score_award;

    [[nodiscard]] bool operator==(const FrontSpiderRulesUpdate&) const = default;
};

[[nodiscard]] FrontSpiderRulesUpdate resolve_front_spider_contact(
    FrontSpiderState state,
    ScoreState score) noexcept;

} // namespace devils_crush::native_runtime
