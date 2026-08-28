// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/frame_oracle.hpp"

#include <algorithm>

namespace devils_crush::native_runtime {
namespace {

template <typename T>
[[nodiscard]] const FrameMismatch* compare_contents(
    const std::span<const T> reference,
    const std::span<const T> candidate,
    const FrameMismatchField field,
    FrameMismatch& mismatch) noexcept
{
    if (reference.size() != candidate.size()) {
        mismatch = {.field = field, .index = 0U};
        return &mismatch;
    }

    const auto difference = std::mismatch(
        reference.begin(), reference.end(), candidate.begin());
    if (difference.first == reference.end()) {
        return nullptr;
    }

    mismatch = {
        .field = field,
        .index = static_cast<std::size_t>(
            std::distance(reference.begin(), difference.first)),
    };
    return &mismatch;
}

} // namespace

FrameSnapshot::FrameSnapshot(const FrameObservation& observation)
    : frame_number_(observation.frame_number)
    , work_ram_(observation.work_ram.begin(), observation.work_ram.end())
    , framebuffer_(observation.framebuffer.begin(), observation.framebuffer.end())
    , framebuffer_width_(observation.framebuffer_width)
    , framebuffer_height_(observation.framebuffer_height)
    , framebuffer_pitch_pixels_(observation.framebuffer_pitch_pixels)
    , audio_(observation.audio.begin(), observation.audio.end())
    , audio_sample_rate_(observation.audio_sample_rate)
{
}

FrameObservation FrameSnapshot::observation() const noexcept
{
    return {
        .frame_number = frame_number_,
        .work_ram = work_ram_,
        .framebuffer = framebuffer_,
        .framebuffer_width = framebuffer_width_,
        .framebuffer_height = framebuffer_height_,
        .framebuffer_pitch_pixels = framebuffer_pitch_pixels_,
        .audio = audio_,
        .audio_sample_rate = audio_sample_rate_,
    };
}

const FrameMismatch* compare_frame_observations(
    const FrameObservation& reference,
    const FrameObservation& candidate,
    FrameMismatch& mismatch) noexcept
{
    if (reference.frame_number != candidate.frame_number) {
        mismatch = {.field = FrameMismatchField::frame_number, .index = 0U};
        return &mismatch;
    }

    if (const auto* const difference = compare_contents(
            reference.work_ram,
            candidate.work_ram,
            FrameMismatchField::work_ram,
            mismatch)) {
        return difference;
    }

    if (reference.framebuffer_width != candidate.framebuffer_width
        || reference.framebuffer_height != candidate.framebuffer_height
        || reference.framebuffer_pitch_pixels
            != candidate.framebuffer_pitch_pixels) {
        mismatch = {.field = FrameMismatchField::framebuffer_shape, .index = 0U};
        return &mismatch;
    }

    if (const auto* const difference = compare_contents(
            reference.framebuffer,
            candidate.framebuffer,
            FrameMismatchField::framebuffer,
            mismatch)) {
        return difference;
    }

    if (reference.audio_sample_rate != candidate.audio_sample_rate) {
        mismatch = {.field = FrameMismatchField::audio_sample_rate, .index = 0U};
        return &mismatch;
    }

    return compare_contents(
        reference.audio, candidate.audio, FrameMismatchField::audio, mismatch);
}

} // namespace devils_crush::native_runtime
