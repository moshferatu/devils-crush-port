// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/input/plunger.hpp"

#include <algorithm>
#include <cstdint>

namespace devils_crush::input {
namespace {

constexpr std::int32_t kFullDraw = 100;

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
    drawn_ = false;
    armed_ = false;
}

std::uint8_t Plunger::resolve_draw(const std::int32_t raw) const noexcept
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
    return static_cast<std::uint8_t>(apply_deadzone(
        static_cast<std::int32_t>(clamped),
        calibration_.deadzone));
}

PlungerSample Plunger::update(const std::int32_t raw) noexcept
{
    const auto previous = draw_;
    draw_ = resolve_draw(raw);

    if (!armed_) {
        // Nothing this plunger does counts until it has been seen at rest.
        armed_ = draw_ == 0;
        drawn_ = false;
        return {};
    }

    if (!drawn_) {
        if (draw_ == 0) {
            return {};
        }
        drawn_ = true;
        return {.draw = draw_, .drawn = true, .released = false};
    }

    // Let go, either because the rod fell faster in one frame than a hand
    // moves it or because it is home. The second test is what catches a rod
    // eased all the way forward by hand: it never falls fast enough to trip
    // the first, and without it the shooter would sit at zero charge with the
    // button held down and the ball would never leave.
    const auto fall = static_cast<std::int32_t>(previous) - draw_;
    const bool released =
        fall >= static_cast<std::int32_t>(calibration_.release)
        || draw_ == 0;
    if (!released) {
        return {.draw = draw_, .drawn = true, .released = false};
    }

    drawn_ = false;
    // The rod has to come home before it can be pulled again, so a spring
    // bouncing back past the deadzone cannot start a second draw.
    armed_ = false;
    // `previous` rather than `draw_`: the shot is the draw the rod held before
    // the spring took it, which is the last frame the player's hand chose.
    return {.draw = previous, .drawn = true, .released = true};
}

} // namespace devils_crush::input
