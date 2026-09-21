// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <charconv>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace devils_crush::input {

inline std::optional<double> parse_nudge_multiplier(std::string_view text) noexcept
{
    double value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
        || !std::isfinite(value) || value < 0 || value > 16) return std::nullopt;
    return value;
}

// The cartridge has already added the signed impulse to its 16-bit motion
// word. Recover the prior value modulo 65536, then apply the requested force.
// Saturate amplified motion rather than wrapping it into the opposite direction.
inline std::uint16_t scale_nudge_motion(std::uint16_t motion,
                                      std::uint16_t impulse, double factor) noexcept
{
    if (factor == 1) return motion;
    const auto signed_word = [](std::uint16_t value) {
        return value < 0x8000U ? static_cast<int>(value) : static_cast<int>(value) - 65536;
    };
    const int before = signed_word(static_cast<std::uint16_t>(motion - impulse));
    const auto scaled = std::clamp<long long>(before
        + std::llround(signed_word(impulse) * factor), -32768, 32767);
    return static_cast<std::uint16_t>(scaled);
}

struct NudgeBinding {
    bool enabled = true;
    std::string device = "PinOne";
    int axis_x = 0;
    int axis_y = 1;
    int threshold = 20;
    int release = 8;
    int cooldown_ms = 250;

    [[nodiscard]] bool valid() const noexcept
    {
        return !device.empty() && device.size() <= 1024
            && axis_x >= 0 && axis_x <= 255 && axis_y >= 0 && axis_y <= 255
            && axis_x != axis_y && threshold >= 1 && threshold <= 100
            && release >= 0 && release < threshold
            && cooldown_ms >= 100 && cooldown_ms <= 5000;
    }
    bool operator==(const NudgeBinding&) const = default;
};

// Match Alien Crush's digital nudge pulse and wall-clock debounce.
class Nudge {
    bool armed_ = false;
    bool quiet_ = false;
    std::uint64_t quiet_since_ = 0, pulse_until_ = 0, cooldown_until_ = 0;
public:
    void rearm() noexcept { *this = {}; }

    bool read(const NudgeBinding& binding, std::int16_t x, std::int16_t y,
              std::uint64_t now, bool active) noexcept
    {
        if (!active || !binding.enabled) { rearm(); return false; }
        const int magnitude = std::max(std::abs(static_cast<int>(x)),
                                       std::abs(static_cast<int>(y)));
        if (magnitude * 100 <= binding.release * 32767) {
            if (!quiet_) { quiet_ = true; quiet_since_ = now; }
            if (now - quiet_since_ >= 100 && now >= cooldown_until_) armed_ = true;
        } else {
            quiet_ = false;
            if (armed_ && magnitude * 100 >= binding.threshold * 32767
                && now >= cooldown_until_) {
                armed_ = false;
                pulse_until_ = now + 50;
                cooldown_until_ = now + static_cast<std::uint64_t>(binding.cooldown_ms);
            }
        }
        return now < pulse_until_;
    }
};

inline bool nudge_play_active(std::span<const std::byte> ram) noexcept
{
    // Live table/bonus only; exclude demo, drained ball and game over.
    return ram.size() == 8192 && ram[0x254] == std::byte{0}
        && std::to_integer<unsigned>(ram[0x54]) <= 1
        && ram[0xD52] != std::byte{0}
        && (std::to_integer<unsigned>(ram[0x53]) & 2U) == 0;
}

} // namespace devils_crush::input
