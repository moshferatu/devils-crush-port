// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/score_capture.hpp"

namespace devils_crush::native_runtime {

std::optional<ActiveScoreCapture> capture_active_score(
    const GameStateView& state) noexcept
{
    const auto source = state.active_score();
    if (!source) {
        return std::nullopt;
    }

    return ActiveScoreCapture{
        .source = *source,
        .state = {
            .stored_units = source->stored_units,
            .display_dirty = source->display_dirty,
        },
    };
}

} // namespace devils_crush::native_runtime
