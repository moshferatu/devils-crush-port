// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/small_soldier.hpp"

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint8_t kHitScoreUnits = 30U;
constexpr std::uint8_t kCompletionScoreUnits = 50U;
constexpr std::uint8_t kDeferredCompletionSubtype = 0x0FU;
constexpr std::uint8_t kSpawnDelayTicks = 48U;

[[nodiscard]] SmallSoldierContactStep complete(
    const SmallSoldierState previous,
    SmallSoldierState state) noexcept
{
    state.completion_animation_pending = false;
    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierContactDisposition::completed,
        .completion_score_units = kCompletionScoreUnits,
        .decrement_group = true,
        .release_actor = true,
    };
}

} // namespace

SmallSoldierGroupStep spawn_small_soldier_child(
    SmallSoldierGroupState state) noexcept
{
    const auto previous = state;
    if (state.pending_spawns == 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierGroupDisposition::invalid_state,
        };
    }

    --state.pending_spawns;
    ++state.live_children;
    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierGroupDisposition::child_spawned,
    };
}

SmallSoldierGroupStep complete_small_soldier_group_child(
    SmallSoldierGroupState state) noexcept
{
    const auto previous = state;
    if (state.live_children == 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierGroupDisposition::invalid_state,
        };
    }

    --state.live_children;
    if (state.live_children != 0U || state.pending_spawns != 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierGroupDisposition::child_completed,
        };
    }

    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierGroupDisposition::source_completed,
        .source_completion_score_units = 200U,
    };
}

SmallSoldierSourceStep spawn_small_soldier_source_child(
    SmallSoldierSourceState state) noexcept
{
    const auto previous = state;
    if (state.regeneration_pending) {
        return {.previous = previous, .state = state};
    }

    const auto group_step = spawn_small_soldier_child(state.group);
    if (group_step.disposition != SmallSoldierGroupDisposition::child_spawned) {
        return {
            .previous = previous,
            .state = state,
            .group_step = group_step,
        };
    }

    state.group = group_step.state;
    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierSourceDisposition::child_spawned,
        .group_step = group_step,
    };
}

SmallSoldierSourceStep advance_small_soldier_source_clock(
    SmallSoldierSourceState state) noexcept
{
    const auto previous = state;
    if (state.regeneration_pending || state.group.pending_spawns == 0U) {
        return {.previous = previous, .state = state};
    }
    if (state.ticks_until_next_spawn > 1U) {
        --state.ticks_until_next_spawn;
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierSourceDisposition::waiting,
        };
    }

    // `DEC $3A00,X` falls through on one and the source writes `$30` before
    // invoking the allocator. A captured zero is likewise already due.
    state.ticks_until_next_spawn = kSpawnDelayTicks;
    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierSourceDisposition::spawn_requested,
    };
}

SmallSoldierSourceStep complete_small_soldier_source_child(
    SmallSoldierSourceState state) noexcept
{
    const auto previous = state;
    const auto group_step = complete_small_soldier_group_child(state.group);
    if (group_step.disposition == SmallSoldierGroupDisposition::invalid_state) {
        return {
            .previous = previous,
            .state = state,
            .group_step = group_step,
        };
    }

    state.group = group_step.state;
    if (group_step.disposition == SmallSoldierGroupDisposition::source_completed) {
        state.appearance = static_cast<std::uint8_t>((state.appearance + 1U) % 4U);
        state.regeneration_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierSourceDisposition::generation_completed,
            .group_step = group_step,
        };
    }
    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierSourceDisposition::child_completed,
        .group_step = group_step,
    };
}

SmallSoldierSourceStep regenerate_small_soldier_source(
    SmallSoldierSourceState state) noexcept
{
    const auto previous = state;
    if (!state.regeneration_pending) {
        return {.previous = previous, .state = state};
    }

    state.group = {.side = state.group.side};
    state.regeneration_pending = false;
    return {
        .previous = previous,
        .state = state,
        .disposition = SmallSoldierSourceDisposition::regenerated,
    };
}

SmallSoldierContactStep consume_small_soldier_contact(
    SmallSoldierState state) noexcept
{
    const auto previous = state;
    if (!state.contact_pending) {
        return {.previous = previous, .state = state};
    }
    if (state.hits_remaining == 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierContactDisposition::invalid_state,
        };
    }

    state.contact_pending = false;
    state.hits_remaining = static_cast<std::uint8_t>(state.hits_remaining - 1U);
    if (state.hits_remaining != 0U) {
        return {
            .previous = previous,
            .state = state,
            .disposition = SmallSoldierContactDisposition::survived_hit,
            .hit_score_units = kHitScoreUnits,
        };
    }
    if (state.subtype == kDeferredCompletionSubtype) {
        state.completion_animation_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition =
                SmallSoldierContactDisposition::awaiting_completion_animation,
            .hit_score_units = kHitScoreUnits,
        };
    }

    auto result = complete(previous, state);
    result.hit_score_units = kHitScoreUnits;
    return result;
}

SmallSoldierContactStep complete_small_soldier_animation(
    SmallSoldierState state) noexcept
{
    const auto previous = state;
    if (!state.completion_animation_pending) {
        return {.previous = previous, .state = state};
    }
    return complete(previous, state);
}

} // namespace devils_crush::native_runtime
