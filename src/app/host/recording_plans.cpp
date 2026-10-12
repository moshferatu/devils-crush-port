// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording_plans.hpp"

namespace devils_crush::app::host {

[[nodiscard]] InputFrame input_for_recording_plan_frame(
    const RecordingPlan& plan,
    const int frame_number)
{
    if (plan.input_for_frame != nullptr) {
        return plan.input_for_frame(frame_number);
    }
    return plan.captured_inputs[static_cast<std::size_t>(frame_number - 1)];
}

} // namespace devils_crush::app::host
