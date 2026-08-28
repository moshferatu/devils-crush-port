// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/small_soldier_rules.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kSmallSoldierHitAudioCommand = 0x21U;

[[nodiscard]] SmallSoldierRulesUpdate apply_score_events(
    const ActorSmallSoldierUpdate actor,
    ScoreState& score) noexcept
{
    SmallSoldierRulesUpdate update{
        .actor = actor,
        .score_events = small_soldier_score_events(
            actor.step,
            actor.group_step),
    };
    for (std::uint8_t event = 0U; event < update.score_events.count; ++event) {
        const auto score_step = apply_score_award(
            score,
            update.score_events.awards[event]);
        score = score_step.state;
        update.score_awards[update.score_award_count++] = score_step;
    }
    if (actor.step.hit_score_units != 0U) {
        update.audio_command = kSmallSoldierHitAudioCommand;
    }
    return update;
}

} // namespace

std::optional<SmallSoldierRulesUpdate> resolve_small_soldier_contact(
    ActorState& actors,
    ScoreState& score,
    const std::uint8_t slot) noexcept
{
    const auto actor = actors.apply_small_soldier_contact(slot);
    if (!actor) {
        return std::nullopt;
    }
    return apply_score_events(*actor, score);
}

std::optional<SmallSoldierRulesUpdate>
resolve_small_soldier_completion_animation(
    ActorState& actors,
    ScoreState& score,
    const std::uint8_t slot) noexcept
{
    const auto actor = actors.complete_small_soldier_animation(slot);
    if (!actor) {
        return std::nullopt;
    }
    return apply_score_events(*actor, score);
}

} // namespace devils_crush::native_runtime
