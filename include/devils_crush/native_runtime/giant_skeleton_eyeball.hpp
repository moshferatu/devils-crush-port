// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace devils_crush::native_runtime {

inline constexpr std::uint8_t kGiantSkeletonEyeballSlot = 0x2FU;

// The fixed Eyeball is dormant until Dragon's-Egg completion reaches its own
// later actor turn. Its opening animation/timer remains a presentation owner;
// this state names only the cross-target signal and one-shot handoff.
struct GiantSkeletonEyeballState {
    bool opening_transition_pending = false;
    bool opening_countdown_active = false;
    std::uint8_t opening_ticks_remaining = 0U;
    bool opening_expiry_pending = false;
    // The final mouth sequence is driven by the same fixed actor after its
    // opening expiry. Its source chooses either the ordinary 224-turn wait or
    // the extended 992-turn wait; that scene-mode decision remains an
    // explicit caller input rather than an unnamed RAM flag.
    bool final_animation_active = false;
    std::uint16_t final_ticks_remaining = 0U;
    std::uint8_t final_appearance_ticks_remaining = 0U;
    std::uint8_t final_appearance_phase = 0U;
    std::uint8_t final_appearance = 0U;
    bool final_reset_pending = false;

    [[nodiscard]] bool operator==(const GiantSkeletonEyeballState&) const = default;
};

enum class GiantSkeletonEyeballDisposition : std::uint8_t {
    dormant,
    opening_requested,
    waiting_for_opening_transition,
    opening_countdown_wait,
    opening_timer_expired,
    waiting_for_opening_expiry,
    final_animation_wait,
    final_animation_completed,
    waiting_for_final_reset,
};

struct GiantSkeletonEyeballStep {
    GiantSkeletonEyeballState previous;
    GiantSkeletonEyeballState state;
    GiantSkeletonEyeballDisposition disposition =
        GiantSkeletonEyeballDisposition::dormant;
    bool observes_egg_completion = false;

    [[nodiscard]] bool operator==(const GiantSkeletonEyeballStep&) const = default;
};

// Runs one fixed slot-$2F activation gate. The Egg controller owns producing
// the completion signal; the Eyeball observes it only on its later turn and
// issues one mouth-opening/presentation handoff.
[[nodiscard]] GiantSkeletonEyeballStep advance_giant_skeleton_eyeball(
    GiantSkeletonEyeballState state,
    bool dragon_egg_completion_signaled) noexcept;

enum class GiantSkeletonEyeballOpeningStartDisposition : std::uint8_t {
    ignored,
    countdown_started,
};

struct GiantSkeletonEyeballOpeningStartStep {
    GiantSkeletonEyeballState previous;
    GiantSkeletonEyeballState state;
    GiantSkeletonEyeballOpeningStartDisposition disposition =
        GiantSkeletonEyeballOpeningStartDisposition::ignored;

    [[nodiscard]] bool operator==(const GiantSkeletonEyeballOpeningStartStep&) const
        = default;
};

// The opening branch reloads `$3A2F` with eight after its immediate setup
// work. The separate presentation owner acknowledges that setup here; later
// actor turns own only the verified countdown wait.
[[nodiscard]] GiantSkeletonEyeballOpeningStartStep
begin_giant_skeleton_eyeball_opening_countdown(
    GiantSkeletonEyeballState state) noexcept;

enum class GiantSkeletonEyeballFinalStartDisposition : std::uint8_t {
    ignored,
    final_animation_started,
};

struct GiantSkeletonEyeballFinalStartStep {
    GiantSkeletonEyeballState previous;
    GiantSkeletonEyeballState state;
    GiantSkeletonEyeballFinalStartDisposition disposition =
        GiantSkeletonEyeballFinalStartDisposition::ignored;

    [[nodiscard]] bool operator==(const GiantSkeletonEyeballFinalStartStep&) const
        = default;
};

// Begins the verified $4F15 mouth-closing wait after the presentation owner
// has selected that branch. The ordinary branch counts down $00E0 actor turns;
// the source's secondary mode extends it to $03E0. Appearance changes remain
// part of this state rather than a renderer-owned elapsed-frame guess.
[[nodiscard]] GiantSkeletonEyeballFinalStartStep
begin_giant_skeleton_eyeball_final_animation(
    GiantSkeletonEyeballState state,
    bool extended_wait) noexcept;

enum class GiantSkeletonEyeballFinalResetDisposition : std::uint8_t {
    ignored,
    dormant_reinitialized,
};

struct GiantSkeletonEyeballFinalResetStep {
    GiantSkeletonEyeballState previous;
    GiantSkeletonEyeballState state;
    GiantSkeletonEyeballFinalResetDisposition disposition =
        GiantSkeletonEyeballFinalResetDisposition::ignored;

    [[nodiscard]] bool operator==(const GiantSkeletonEyeballFinalResetStep&) const
        = default;
};

// The source ends its final wait by returning to `02:4DC8`, which clears the
// Eyeball's local lifecycle and resumes its dormant dispatcher. The associated
// close-presentation work is still a separate owner; it acknowledges that
// work here so native gameplay does not reinitialize early.
[[nodiscard]] GiantSkeletonEyeballFinalResetStep
complete_giant_skeleton_eyeball_final_reset(
    GiantSkeletonEyeballState state) noexcept;

} // namespace devils_crush::native_runtime
