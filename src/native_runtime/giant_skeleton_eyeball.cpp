// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/giant_skeleton_eyeball.hpp"

#include <array>

namespace devils_crush::native_runtime {
namespace {

constexpr std::uint16_t kOrdinaryFinalAnimationTurns = 0x00E0U;
constexpr std::uint16_t kExtendedFinalAnimationTurns = 0x03E0U;
constexpr std::uint8_t kFinalAppearancePeriod = 8U;
constexpr std::array<std::uint8_t, 4> kFinalAppearances{0U, 1U, 0U, 1U};

} // namespace

GiantSkeletonEyeballStep advance_giant_skeleton_eyeball(
    GiantSkeletonEyeballState state,
    const bool dragon_egg_completion_signaled) noexcept
{
    const auto previous = state;
    if (state.final_reset_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantSkeletonEyeballDisposition::waiting_for_final_reset,
            .observes_egg_completion = dragon_egg_completion_signaled,
        };
    }
    if (state.final_animation_active) {
        if (state.final_ticks_remaining == 0U) {
            state.final_animation_active = false;
            state.final_reset_pending = true;
            return {
                .previous = previous,
                .state = state,
                .disposition =
                    GiantSkeletonEyeballDisposition::final_animation_completed,
                .observes_egg_completion = dragon_egg_completion_signaled,
            };
        }
        if (state.final_appearance_ticks_remaining > 1U) {
            --state.final_appearance_ticks_remaining;
        } else {
            state.final_appearance_ticks_remaining = kFinalAppearancePeriod;
            state.final_appearance_phase = static_cast<std::uint8_t>(
                (state.final_appearance_phase + 1U)
                % kFinalAppearances.size());
            state.final_appearance =
                kFinalAppearances[state.final_appearance_phase];
        }

        --state.final_ticks_remaining;
        if (state.final_ticks_remaining != 0U) {
            return {
                .previous = previous,
                .state = state,
                .disposition = GiantSkeletonEyeballDisposition::final_animation_wait,
                .observes_egg_completion = dragon_egg_completion_signaled,
            };
        }

        state.final_animation_active = false;
        state.final_reset_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantSkeletonEyeballDisposition::final_animation_completed,
            .observes_egg_completion = dragon_egg_completion_signaled,
        };
    }
    if (state.opening_expiry_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition =
                GiantSkeletonEyeballDisposition::waiting_for_opening_expiry,
            .observes_egg_completion = dragon_egg_completion_signaled,
        };
    }
    if (state.opening_countdown_active) {
        if (state.opening_ticks_remaining > 1U) {
            --state.opening_ticks_remaining;
            return {
                .previous = previous,
                .state = state,
                .disposition =
                    GiantSkeletonEyeballDisposition::opening_countdown_wait,
                .observes_egg_completion = dragon_egg_completion_signaled,
            };
        }
        state.opening_countdown_active = false;
        state.opening_expiry_pending = true;
        return {
            .previous = previous,
            .state = state,
            .disposition = GiantSkeletonEyeballDisposition::opening_timer_expired,
            .observes_egg_completion = dragon_egg_completion_signaled,
        };
    }
    if (state.opening_transition_pending) {
        return {
            .previous = previous,
            .state = state,
            .disposition =
                GiantSkeletonEyeballDisposition::waiting_for_opening_transition,
            .observes_egg_completion = dragon_egg_completion_signaled,
        };
    }
    if (!dragon_egg_completion_signaled) {
        return {.previous = previous, .state = state};
    }

    state.opening_transition_pending = true;
    return {
        .previous = previous,
        .state = state,
        .disposition = GiantSkeletonEyeballDisposition::opening_requested,
        .observes_egg_completion = true,
    };
}

GiantSkeletonEyeballOpeningStartStep
begin_giant_skeleton_eyeball_opening_countdown(
    GiantSkeletonEyeballState state) noexcept
{
    const auto previous = state;
    if (!state.opening_transition_pending || state.opening_countdown_active
        || state.opening_expiry_pending) {
        return {.previous = previous, .state = state};
    }
    state.opening_transition_pending = false;
    state.opening_countdown_active = true;
    state.opening_ticks_remaining = 8U;
    return {
        .previous = previous,
        .state = state,
        .disposition = GiantSkeletonEyeballOpeningStartDisposition::countdown_started,
    };
}

GiantSkeletonEyeballFinalStartStep
begin_giant_skeleton_eyeball_final_animation(
    GiantSkeletonEyeballState state,
    const bool extended_wait) noexcept
{
    const auto previous = state;
    if (!state.opening_expiry_pending || state.final_animation_active
        || state.final_reset_pending) {
        return {.previous = previous, .state = state};
    }

    state.opening_expiry_pending = false;
    state.final_animation_active = true;
    state.final_ticks_remaining = extended_wait ? kExtendedFinalAnimationTurns
                                                : kOrdinaryFinalAnimationTurns;
    // The source initializes this timer to one, then its first later actor
    // turn reloads eight and selects phase one of the 0/1/0/1 sequence.
    state.final_appearance_ticks_remaining = 1U;
    state.final_appearance_phase = 0U;
    state.final_appearance = kFinalAppearances.front();
    return {
        .previous = previous,
        .state = state,
        .disposition =
            GiantSkeletonEyeballFinalStartDisposition::final_animation_started,
    };
}

GiantSkeletonEyeballFinalResetStep
complete_giant_skeleton_eyeball_final_reset(
    GiantSkeletonEyeballState state) noexcept
{
    const auto previous = state;
    if (!state.final_reset_pending) {
        return {.previous = previous, .state = state};
    }
    state = {};
    return {
        .previous = previous,
        .state = state,
        .disposition =
            GiantSkeletonEyeballFinalResetDisposition::dormant_reinitialized,
    };
}

} // namespace devils_crush::native_runtime
