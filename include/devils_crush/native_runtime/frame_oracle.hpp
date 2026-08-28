// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace devils_crush::native_runtime {

// The data a runtime exposes after one input frame. The emulator supplies the
// reference side during migration; the native runtime supplies the candidate
// side. Views are intentionally non-owning so a host can compare a frame
// without allocating another framebuffer or audio buffer.
struct FrameObservation {
    std::uint64_t frame_number = 0;
    std::span<const std::byte> work_ram;
    std::span<const std::uint32_t> framebuffer;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    int framebuffer_pitch_pixels = 0;
    std::span<const float> audio;
    int audio_sample_rate = 0;
};

// Owns one frame boundary so an oracle comparison can outlive the runtime
// buffers that produced it. A runtime may reuse its framebuffer and audio
// storage on the next update, whereas a replay harness needs a stable
// reference while it advances the other runtime.
class FrameSnapshot {
public:
    explicit FrameSnapshot(const FrameObservation& observation);

    [[nodiscard]] FrameObservation observation() const noexcept;

private:
    std::uint64_t frame_number_ = 0;
    std::vector<std::byte> work_ram_;
    std::vector<std::uint32_t> framebuffer_;
    int framebuffer_width_ = 0;
    int framebuffer_height_ = 0;
    int framebuffer_pitch_pixels_ = 0;
    std::vector<float> audio_;
    int audio_sample_rate_ = 0;
};

enum class FrameMismatchField {
    frame_number,
    work_ram,
    framebuffer_shape,
    framebuffer,
    audio_sample_rate,
    audio,
};

struct FrameMismatch {
    FrameMismatchField field = FrameMismatchField::frame_number;
    // Byte, pixel, or interleaved-sample index for a content mismatch. Shape
    // and frame-number mismatches use zero.
    std::size_t index = 0;
};

// Returns the first observable disagreement in the order in which a replay
// is certified. Exact equality is deliberate: migration begins from the
// emulator's frame output and permits no tolerance-based drift.
[[nodiscard]] const FrameMismatch* compare_frame_observations(
    const FrameObservation& reference,
    const FrameObservation& candidate,
    FrameMismatch& mismatch) noexcept;

} // namespace devils_crush::native_runtime
