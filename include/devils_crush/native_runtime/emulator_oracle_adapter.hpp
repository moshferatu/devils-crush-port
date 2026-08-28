// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/native_runtime/frame_oracle.hpp"

namespace devils_crush::native_runtime {

// Copies all observable output from one emulated frame. The source runtime
// owns the frame's framebuffer/audio views and its work-RAM export, so callers
// should retain the returned snapshot rather than any of those transient
// values when advancing another runtime for comparison.
[[nodiscard]] FrameSnapshot capture_emulator_frame(
    const emulation::PceRuntime& runtime,
    const emulation::EmulatedFrame& frame);

} // namespace devils_crush::native_runtime
