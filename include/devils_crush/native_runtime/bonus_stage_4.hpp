// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/score_state.hpp"

#include <array>
#include <cstdint>

namespace devils_crush::native_runtime {

// BS-4 has three fixed, independently hit eight-count objectives. Their ROM
// actor slots are retained only as stable table identities; callers use the
// objective index for rule updates rather than a dispatch-vector address.
inline constexpr std::array<std::uint8_t, 3U> kBonusStage4ObjectiveSlots{
    0x34U, 0x52U, 0x56U};

struct BonusStage4ObjectiveState {
    std::uint8_t hits_remaining = 8U;
    bool active = true;

    [[nodiscard]] bool operator==(const BonusStage4ObjectiveState&) const = default;
};

// The controller initializes all three objectives and, after all three are
// destroyed, enters the shared bonus-result owner with multiplier 20. The
// animation, result transfer, and linked-target source family stay distinct
// owners so this state does not manufacture their timing.
struct BonusStage4State {
    std::array<BonusStage4ObjectiveState, 3U> objectives{};
    bool completion_requested = false;
    std::uint8_t completion_multiplier = 0U;

    [[nodiscard]] bool operator==(const BonusStage4State&) const = default;
};

enum class BonusStage4ObjectiveDisposition : std::uint8_t {
    ignored,
    survived_hit,
    destroyed,
};

struct BonusStage4ObjectiveContactStep {
    BonusStage4State previous;
    BonusStage4State state;
    std::uint8_t objective_index = 0U;
    BonusStage4ObjectiveDisposition disposition =
        BonusStage4ObjectiveDisposition::ignored;
    std::uint16_t hit_score_award_units = 0U;
    std::uint16_t destruction_score_award_units = 0U;
    std::uint8_t audio_command = 0U;
    bool requests_destruction_presentation = false;

    [[nodiscard]] bool operator==(const BonusStage4ObjectiveContactStep&) const
        = default;
};

struct BonusStage4ObjectiveRulesUpdate {
    BonusStage4ObjectiveContactStep objective;
    ScoreAwardStep hit_score_award;
    ScoreAwardStep destruction_score_award;

    [[nodiscard]] bool operator==(const BonusStage4ObjectiveRulesUpdate&) const
        = default;
};

// Resolves one confirmed BS-4 objective hit. Each of the eight hits awards
// 100 stored units and emits $2E; the eighth also clears that objective,
// awards 1,000 stored units, and requests the source's destruction owner.
// Once every objective is clear, the controller's already-proven multiplier
// 20 completion handoff is recorded without advancing shared result timing.
[[nodiscard]] BonusStage4ObjectiveRulesUpdate
resolve_bonus_stage_4_objective_contact(
    BonusStage4State state,
    ScoreState score,
    std::uint8_t objective_index) noexcept;

} // namespace devils_crush::native_runtime
