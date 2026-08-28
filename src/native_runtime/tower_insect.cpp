// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/tower_insect.hpp"

namespace devils_crush::native_runtime {

TowerInsectSpawnStep spawn_tower_insect_child(TowerInsectSourceState state) noexcept
{
    const auto previous = state;
    if (!state.generation_active || state.group_clear_pending || state.group_clear_awarded
        || state.spawns_remaining == 0U || !state.child_spawn_pending) {
        return {.previous = previous, .state = state,
                .disposition = TowerInsectSpawnDisposition::source_exhausted};
    }
    --state.spawns_remaining;
    ++state.live_children;
    state.child_spawn_pending = false;
    return {.previous = previous, .state = state,
            .disposition = TowerInsectSpawnDisposition::child_spawned};
}

TowerInsectContactStep consume_tower_insect_contact(
    TowerInsectState insect,
    TowerInsectSourceState source) noexcept
{
    const auto previous = insect;
    const auto previous_source = source;
    if (!source.generation_active || insect.hits_remaining == 0U
        || source.live_children == 0U
        || source.group_clear_pending) {
        return {.previous = previous, .state = insect,
                .previous_source = previous_source, .source = source,
                .disposition = TowerInsectContactDisposition::invalid_state};
    }

    --insect.hits_remaining;
    if (insect.hits_remaining != 0U) {
        return {.previous = previous, .state = insect,
                .previous_source = previous_source, .source = source,
                .disposition = TowerInsectContactDisposition::survived_hit,
                .hit_score_award_units = 10U, .audio_command = 0x29U,
                .requests_collision_effect = true};
    }

    --source.live_children;
    const bool group_clear = source.live_children == 0U
        && source.spawns_remaining == 0U;
    source.group_clear_pending = group_clear;
    return {.previous = previous, .state = insect,
            .previous_source = previous_source, .source = source,
            .disposition = group_clear
                ? TowerInsectContactDisposition::group_clear_armed
                : TowerInsectContactDisposition::child_completed,
            .hit_score_award_units = 10U, .completion_score_award_units = 50U,
            .audio_command = 0x29U, .releases_child = true,
            .requests_collision_effect = true};
}

TowerInsectSourceStep advance_tower_insect_source(
    TowerInsectSourceState state) noexcept
{
    const auto previous = state;
    if (!state.generation_active) {
        return {.previous = previous, .state = state};
    }
    if (state.group_clear_pending) {
        state.group_clear_pending = false;
        state.group_clear_awarded = true;
        return {.previous = previous, .state = state,
                .disposition = TowerInsectSourceDisposition::group_clear_awarded,
                .score_award_units = 200U};
    }
    if (state.group_clear_awarded || state.spawns_remaining == 0U
        || state.child_spawn_pending) {
        return {.previous = previous, .state = state};
    }
    if (state.ticks_until_next_spawn > 1U) {
        --state.ticks_until_next_spawn;
        return {.previous = previous, .state = state};
    }
    state.ticks_until_next_spawn = 0x30U;
    state.child_spawn_pending = true;
    return {.previous = previous, .state = state,
            .disposition = TowerInsectSourceDisposition::child_spawn_requested,
            .requests_child_spawn = true};
}

TowerInsectSourceStep begin_tower_insect_generation(
    TowerInsectSourceState state,
    const bool tower_insect_cycle_requested) noexcept
{
    const auto previous = state;
    if (state.generation_active) {
        return {.previous = previous, .state = state};
    }
    if (!tower_insect_cycle_requested) {
        return {.previous = previous, .state = state,
                .disposition = TowerInsectSourceDisposition::waiting_for_next_cycle};
    }
    state = {};
    state.generation_active = true;
    return {.previous = previous, .state = state,
            .disposition = TowerInsectSourceDisposition::generation_started};
}

TowerInsectSourceStep restart_tower_insect_generation(
    TowerInsectSourceState state,
    const bool tower_insect_cycle_requested) noexcept
{
    const auto previous = state;
    if (!state.group_clear_awarded) {
        return {.previous = previous, .state = state};
    }
    if (!tower_insect_cycle_requested) {
        return {.previous = previous, .state = state,
                .disposition = TowerInsectSourceDisposition::waiting_for_next_cycle};
    }
    state = {};
    state.generation_active = true;
    return {.previous = previous, .state = state,
            .disposition = TowerInsectSourceDisposition::generation_restarted};
}

TowerInsectRulesUpdate resolve_tower_insect_contact(
    const TowerInsectState insect,
    const TowerInsectSourceState source,
    ScoreState score) noexcept
{
    const auto step = consume_tower_insect_contact(insect, source);
    const auto hit_score_award = apply_score_award(score, step.hit_score_award_units);
    score = hit_score_award.state;
    const auto completion_score_award = apply_score_award(
        score, step.completion_score_award_units);
    score = completion_score_award.state;
    return {.insect = step, .hit_score_award = hit_score_award,
            .completion_score_award = completion_score_award};
}

} // namespace devils_crush::native_runtime
