// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bonus_stage_4.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint16_t kOrdinaryHitAwardUnits = 100U;
constexpr std::uint16_t kDestructionAwardUnits = 1'000U;
constexpr std::uint8_t kHitAudioCommand = 0x2EU;
constexpr std::uint8_t kCompletionMultiplier = 20U;

[[nodiscard]] bool all_objectives_destroyed(
    const BonusStage4State& state) noexcept
{
    for (const auto& objective : state.objectives) {
        if (objective.active) {
            return false;
        }
    }
    return true;
}

} // namespace

BonusStage4ObjectiveRulesUpdate resolve_bonus_stage_4_objective_contact(
    BonusStage4State state,
    ScoreState score,
    const std::uint8_t objective_index) noexcept
{
    const auto previous = state;
    BonusStage4ObjectiveContactStep step{
        .previous = previous,
        .state = state,
        .objective_index = objective_index,
    };
    if (objective_index >= state.objectives.size()) {
        const auto no_hit = apply_score_award(score, 0U);
        return {
            .objective = step,
            .hit_score_award = no_hit,
            .destruction_score_award = no_hit,
        };
    }

    auto& objective = state.objectives[objective_index];
    if (!objective.active || objective.hits_remaining == 0U) {
        const auto no_hit = apply_score_award(score, 0U);
        return {
            .objective = step,
            .hit_score_award = no_hit,
            .destruction_score_award = no_hit,
        };
    }

    --objective.hits_remaining;
    step.disposition = objective.hits_remaining == 0U
        ? BonusStage4ObjectiveDisposition::destroyed
        : BonusStage4ObjectiveDisposition::survived_hit;
    step.hit_score_award_units = kOrdinaryHitAwardUnits;
    step.audio_command = kHitAudioCommand;
    if (step.disposition == BonusStage4ObjectiveDisposition::destroyed) {
        objective.active = false;
        step.destruction_score_award_units = kDestructionAwardUnits;
        step.requests_destruction_presentation = true;
        if (all_objectives_destroyed(state)) {
            state.completion_requested = true;
            state.completion_multiplier = kCompletionMultiplier;
        }
    }
    step.state = state;

    const auto hit_score_award = apply_score_award(score, step.hit_score_award_units);
    const auto destruction_score_award = apply_score_award(
        hit_score_award.state, step.destruction_score_award_units);
    return {
        .objective = step,
        .hit_score_award = hit_score_award,
        .destruction_score_award = destruction_score_award,
    };
}

} // namespace devils_crush::native_runtime
