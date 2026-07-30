// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/render/pce_background.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::render {

inline constexpr int kTitleLogoWidth = 216;
inline constexpr int kTitleLogoHeight = 152;
inline constexpr std::uint64_t kTitleLogoAnimationFrames = 64U;

// A native, ROM-derived rendering of the animated title-screen logo. The
// source streams and palettes are decoded once; render() advances only the
// mapped palette and four eye-sprite descriptors and has no framebuffer or
// live VDC dependency.
class TitleLogoAnimation {
public:
    [[nodiscard]] static std::optional<TitleLogoAnimation> decode(
        std::span<const std::byte> rom_bytes);

    [[nodiscard]] std::optional<ArgbImage> render(
        std::uint64_t animation_frame) const;

private:
    std::vector<std::uint16_t> vram_words_;
    std::vector<std::uint16_t> warm_palette_;
    std::vector<std::uint16_t> bright_palette_;
    std::vector<std::uint16_t> eye_palette_;
};

} // namespace devils_crush::render
