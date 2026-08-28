// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/bonus_stage_4_linked_target.hpp"
#include "devils_crush/native_runtime/bonus_stage_4_orbiting_child_capture.hpp"
#include "devils_crush/native_runtime/game_state.hpp"

#include <array>
#include <optional>

namespace devils_crush::native_runtime {

// A linked target is captured at an observed steady dispatch, or at the exact
// pending-hit handoff (05:6C21, appearance four, and a latched contact bit).
// The source marker and copied objective identity prove that this is BS-4
// rather than another type-$C2 dynamic actor.
struct BonusStage4LinkedTargetCapture {
    GameStateView::ActorSlot actor;
    BonusStage4LinkedTargetState state;

    [[nodiscard]] bool operator==(const BonusStage4LinkedTargetCapture&) const = default;
};

[[nodiscard]] std::optional<BonusStage4LinkedTargetCapture>
capture_bonus_stage_4_linked_target(const GameStateView& state) noexcept;

// A linked target generation is the target together with every clear child
// which is already orbiting that exact target.  Capturing the generation as a
// unit avoids restoring a post-$6C24 target without the children it owns.
struct BonusStage4LinkedTargetGenerationCapture {
    BonusStage4LinkedTargetCapture target;
    std::array<std::optional<BonusStage4OrbitingChildCapture>, 4U> children;

    [[nodiscard]] bool operator==(
        const BonusStage4LinkedTargetGenerationCapture&) const = default;
};

[[nodiscard]] std::optional<BonusStage4LinkedTargetGenerationCapture>
capture_bonus_stage_4_linked_target_generation(const GameStateView& state) noexcept;

} // namespace devils_crush::native_runtime
