// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/native_runtime/giant_skeleton_eyeball.hpp"

#include <optional>

namespace devils_crush::native_runtime {

// The fixed Eyeball is explicitly present after fixed-table setup/capture.
// Its controller is independent of the Giant Skeleton's repeatable target.
class GiantSkeletonEyeballScheduler {
public:
    [[nodiscard]] std::optional<GiantSkeletonEyeballStep> advance_actor(
        std::uint8_t slot,
        bool dragon_egg_completion_signaled) noexcept;
    [[nodiscard]] std::optional<GiantSkeletonEyeballOpeningStartStep>
    begin_opening_countdown() noexcept;
    [[nodiscard]] std::optional<GiantSkeletonEyeballFinalStartStep>
    begin_final_animation(bool extended_wait) noexcept;
    [[nodiscard]] std::optional<GiantSkeletonEyeballFinalResetStep>
    complete_final_reset() noexcept;
    void seed(GiantSkeletonEyeballState state) noexcept;
    [[nodiscard]] const GiantSkeletonEyeballState* eyeball() const noexcept;

private:
    std::optional<GiantSkeletonEyeballState> eyeball_;
};

} // namespace devils_crush::native_runtime
