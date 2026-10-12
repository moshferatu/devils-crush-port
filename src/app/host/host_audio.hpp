// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

struct AudioOutput {
    SDL_AudioStream* stream = nullptr;
    int sample_rate = 0;
    bool resumed = false;
    bool available = false;
    bool open_failure_reported = false;
    std::uint64_t latency_resets = 0;
};

void destroy_audio(AudioOutput& output);

[[nodiscard]] bool queue_audio(
    const AudioView& audio,
    AudioOutput& output);

void handle_audio_device_event(
    const SDL_AudioDeviceEvent& event,
    AudioOutput& output);

} // namespace devils_crush::app::host
