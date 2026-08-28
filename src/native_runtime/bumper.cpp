// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/bumper.hpp"

namespace devils_crush::native_runtime {

BumperHitStep consume_bumper_contact(BumperState state) noexcept
{
    const auto previous = state;
    if (!state.active) {
        return {
            .previous = previous,
            .state = state,
            .disposition = BumperHitDisposition::inactive,
        };
    }
    if (state.hit_phase >= 9U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = BumperHitDisposition::invalid_state,
        };
    }

    ++state.hit_phase;
    if (state.hit_phase <= 6U) {
        ++state.primary_appearance;
        return {
            .previous = previous,
            .state = state,
            .disposition = BumperHitDisposition::primary_damage,
            .score_award_units = 10U,
            .audio_command = 0x20U,
            .refresh_primary_appearance = true,
        };
    }
    if (state.hit_phase <= 8U) {
        ++state.secondary_damage;
        return {
            .previous = previous,
            .state = state,
            .disposition = BumperHitDisposition::secondary_damage,
            .score_award_units = 10U,
            .audio_command = 0x20U,
            .updates_secondary_damage = true,
        };
    }

    state.active = false;
    return {
        .previous = previous,
        .state = state,
        .disposition = BumperHitDisposition::destroyed,
        .score_award_units = 200U,
        .requested_child_count = 6U,
    };
}

BumperRulesUpdate resolve_bumper_contact(
    const BumperState state,
    const ScoreState score) noexcept
{
    const auto bumper = consume_bumper_contact(state);
    return {
        .bumper = bumper,
        .score_award = apply_score_award(score, bumper.score_award_units),
    };
}

} // namespace devils_crush::native_runtime
