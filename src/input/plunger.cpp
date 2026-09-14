// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/input/plunger.hpp"

#include <algorithm>
#include <cstdint>

namespace devils_crush::input {
namespace {

constexpr std::int32_t kFullDraw = 25600;

// The deadzone is taken off the bottom of the travel and the rest is stretched
// back over the whole range, so the rod still reaches a full draw at the end of
// its physical travel rather than at 92% of it. A deadzone of 100 or more would
// leave nothing to stretch and is treated as the whole travel being dead.
[[nodiscard]] std::int32_t apply_deadzone(
    const std::int32_t draw,
    const std::int32_t deadzone) noexcept
{
    if (deadzone >= kFullDraw) {
        return 0;
    }
    if (draw <= deadzone) {
        return 0;
    }
    return ((draw - deadzone) * kFullDraw) / (kFullDraw - deadzone);
}

} // namespace

void Plunger::configure(const PlungerCalibration& calibration) noexcept
{
    calibration_ = calibration;
    rearm();
}

void Plunger::rearm() noexcept
{
    draw_ = 0;
    draw_units_ = 0;
    preceding_units_ = 0;
    drawn_ = false;
    armed_ = false;
}

std::uint16_t Plunger::resolve_draw(const std::int32_t raw) const noexcept
{
    const auto travel =
        static_cast<std::int64_t>(calibration_.drawn) - calibration_.rest;
    if (travel == 0) {
        // Two endpoints that are the same describe no plunger. Answering zero
        // rather than dividing means an unconfigured binding is inert instead
        // of undefined.
        return 0;
    }
    const auto offset =
        static_cast<std::int64_t>(raw) - calibration_.rest;
    const auto scaled = (offset * kFullDraw) / travel;
    const auto clamped = std::clamp<std::int64_t>(scaled, 0, kFullDraw);
    return static_cast<std::uint16_t>(apply_deadzone(
        static_cast<std::int32_t>(clamped),
        static_cast<std::int32_t>(calibration_.deadzone) * 256));
}

PlungerSample Plunger::update(const std::int32_t raw) noexcept
{
    const auto previous = draw_units_;
    const auto preceding = preceding_units_;
    preceding_units_ = previous;
    draw_units_ = resolve_draw(raw);
    draw_ = static_cast<std::uint8_t>(draw_units_ / 256U);

    if (!armed_) {
        // Nothing this plunger does counts until it has been seen at rest.
        armed_ = draw_units_ == 0;
        drawn_ = false;
        return {};
    }

    if (!drawn_) {
        if (draw_units_ == 0) {
            return {};
        }
        drawn_ = true;
        return {.draw = draw_, .drawn = true, .released = false,
                .draw_units = draw_units_};
    }

    // A spring return can straddle samples: 60 -> 45 -> 20 must fire from
    // 60, not 45. Test a short two-frame descent as well as the single-frame
    // threshold. Keep the history bounded so easing forward does not retain
    // an old maximum pull. A pause or reversal starts a fresh descent.
    const auto fall = static_cast<std::int32_t>(previous) - draw_units_;
    const bool descending = preceding > previous && fall > 0;
    const auto origin = descending ? preceding : previous;
    const auto threshold = static_cast<std::int32_t>(calibration_.release) * 256;
    const bool spring_return = fall > 0
        && (fall >= threshold
            || (descending && static_cast<std::int32_t>(origin) - draw_units_
                                  >= threshold));
    const bool released = spring_return || draw_units_ == 0;
    if (!released) {
        return {.draw = draw_, .drawn = true, .released = false,
                .draw_units = draw_units_};
    }

    drawn_ = false;
    // The rod has to come home before it can be pulled again, so a spring
    // bouncing back past the deadzone cannot start a second draw.
    armed_ = false;
    const auto released_draw = spring_return ? origin : previous;
    return {.draw = static_cast<std::uint8_t>(released_draw / 256U),
            .drawn = true, .released = true, .draw_units = released_draw};
}

} // namespace devils_crush::input
