// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/emulator_oracle_adapter.hpp"

namespace devils_crush::native_runtime {

FrameSnapshot capture_emulator_frame(
    const emulation::PceRuntime& runtime,
    const emulation::EmulatedFrame& frame)
{
    return FrameSnapshot({
        .frame_number = frame.frame_number,
        .work_ram = runtime.export_work_ram(),
        .framebuffer = frame.framebuffer.pixels,
        .framebuffer_width = frame.framebuffer.width,
        .framebuffer_height = frame.framebuffer.height,
        .framebuffer_pitch_pixels = frame.framebuffer.pitch_pixels,
        .audio = frame.audio.interleaved_stereo,
        .audio_sample_rate = frame.audio.sample_rate,
    });
}

} // namespace devils_crush::native_runtime
