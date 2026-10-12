// SPDX-License-Identifier: GPL-3.0-or-later
#include "host_audio.hpp"

namespace devils_crush::app::host {

namespace {

constexpr int kMaximumAudioLatencyMilliseconds = 150;

void reset_audio_stream(AudioOutput& output)
{
    if (output.stream != nullptr) {
        SDL_DestroyAudioStream(output.stream);
    }
    output.stream = nullptr;
    output.sample_rate = 0;
    output.resumed = false;
}

[[nodiscard]] bool open_audio_stream(
    const int sample_rate,
    AudioOutput& output)
{
    SDL_AudioSpec specification{};
    specification.format = SDL_AUDIO_F32;
    specification.channels = 2;
    specification.freq = sample_rate;
    output.stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &specification,
        nullptr,
        nullptr);
    if (output.stream == nullptr) {
        if (!output.open_failure_reported) {
            std::cerr << "Audio output unavailable; retrying: "
                      << SDL_GetError() << '\n';
            output.open_failure_reported = true;
        }
        return false;
    }

    output.sample_rate = sample_rate;
    output.open_failure_reported = false;
    return true;
}

void recover_audio_stream(
    AudioOutput& output,
    const std::string_view operation)
{
    std::cerr << operation << "; audio output will reconnect: "
              << SDL_GetError() << '\n';
    reset_audio_stream(output);
}

} // namespace

void destroy_audio(AudioOutput& output)
{
    reset_audio_stream(output);
    output = {};
}

[[nodiscard]] bool queue_audio(
    const AudioView& audio,
    AudioOutput& output)
{
    if (!output.available || audio.interleaved_stereo.empty()) {
        return true;
    }
    if (audio.sample_rate <= 0
        || audio.interleaved_stereo.size()
            > static_cast<std::size_t>(
                std::numeric_limits<int>::max()
                / static_cast<int>(sizeof(float)))) {
        std::cerr << "The emulator returned an invalid audio batch.\n";
        return false;
    }

    if (output.stream != nullptr
        && output.sample_rate != audio.sample_rate) {
        reset_audio_stream(output);
    }
    if (output.stream == nullptr
        && !open_audio_stream(audio.sample_rate, output)) {
        return true;
    }

    const auto queued_bytes = SDL_GetAudioStreamQueued(output.stream);
    if (queued_bytes < 0) {
        recover_audio_stream(
            output,
            "SDL_GetAudioStreamQueued failed");
        return true;
    }
    const auto maximum_queued_bytes =
        static_cast<std::int64_t>(audio.sample_rate) * 2
        * static_cast<std::int64_t>(sizeof(float))
        * kMaximumAudioLatencyMilliseconds / 1000;
    if (static_cast<std::int64_t>(queued_bytes) > maximum_queued_bytes) {
        if (!SDL_ClearAudioStream(output.stream)) {
            recover_audio_stream(
                output,
                "SDL_ClearAudioStream failed");
            return true;
        }
        ++output.latency_resets;
        std::cerr << "Audio queue exceeded "
                  << kMaximumAudioLatencyMilliseconds
                  << " ms; dropped stale audio (recovery "
                  << output.latency_resets << ").\n";
    }

    const auto byte_count = static_cast<int>(
        audio.interleaved_stereo.size() * sizeof(float));
    if (!SDL_PutAudioStreamData(
            output.stream,
            audio.interleaved_stereo.data(),
            byte_count)) {
        recover_audio_stream(output, "SDL_PutAudioStreamData failed");
        return true;
    }
    if (!output.resumed) {
        if (!SDL_ResumeAudioStreamDevice(output.stream)) {
            recover_audio_stream(
                output,
                "SDL_ResumeAudioStreamDevice failed");
            return true;
        }
        output.resumed = true;
    }
    return true;
}

void handle_audio_device_event(
    const SDL_AudioDeviceEvent& event,
    AudioOutput& output)
{
    if (event.recording || output.stream == nullptr) {
        return;
    }
    const auto stream_device = SDL_GetAudioStreamDevice(output.stream);
    if (stream_device != event.which) {
        return;
    }

    if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) {
        std::cerr << "Active audio device was removed; reconnecting.\n";
        reset_audio_stream(output);
    } else if (event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) {
        std::cerr << "Active audio device format changed; reconnecting.\n";
        reset_audio_stream(output);
    }
}

} // namespace devils_crush::app::host
