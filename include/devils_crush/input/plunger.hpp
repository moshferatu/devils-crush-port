// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace devils_crush::input {

// An analog plunger, as a rule rather than a device.
//
// A virtual pinball cabinet's plunger is a potentiometer or a linear sensor on
// a spring-loaded rod, and every board that reads one -- the Cleveland Software
// Design PinOne, VirtuaPin's, a bare Arduino with a slide pot -- presents the
// same thing to the host: one axis of an ordinary HID game controller. So this
// class takes an axis value and nothing else. Which device, which axis, and
// how SDL is asked for it are the host's business; what a *plunger* is lives
// here, and can therefore be tested without a controller plugged in.
//
// **What the cartridge wants is a distance, not a button.** The Devil Shooter
// charges by moving its own piece down the playfield: `02:52E3` adds `$0040` to
// the piece's twenty-four-bit vertical position each tick the player holds
// button I, and `$2D15` -- the charge the launch velocity is computed from at
// `02:51DA` -- is nothing but `$3300,X - $3E00,X`, the whole-pixel distance
// between where the piece is and where it rests. The shooter piece *is* a
// plunger, and `$2D15` is its draw in pixels, from 0 to `$27`.
//
// That is why this class reports a draw rather than a press. A held button
// charges at a quarter of a pixel per tick, so a full draw takes 156 frames --
// two and a half seconds -- and a real plunger is pulled and let go in well
// under one. Mapped to a button, every launch would come out weak and the pull
// distance would mean nothing at all; the only thing that would reach the game
// is how long the rod was held back, which is not how a plunger works. Mapped
// to a distance, the pull is the charge.
//
// **Releasing is a speed, not a position.** A spring-loaded rod let go from a
// full draw is back at rest in two to four frames at 60Hz, while a player
// easing the rod forward to fine-tune a shot moves it a few percent per frame.
// Those are the same journey and different intentions, so the release test is
// a short descent: fall by `release` or more over one or two consecutive
// descending samples and release at the start of that descent. Ease it
// forward and nothing fires -- the draw simply follows the rod down, and the
// shot gets weaker, which is what easing it forward asks for.
struct PlungerCalibration {
    // The axis value with the rod at rest and with it drawn fully back, in
    // whatever units the host's controller reports. Two endpoints rather than
    // a range and an inversion flag: a plunger whose axis runs backwards is
    // one whose `drawn` is the smaller of the two, and needs no separate
    // setting to say so, and getting it the wrong way round is not subtle:
    // the on-screen shooter answers the rod being pushed in rather than pulled
    // out, and only the inch or two of push travel is reachable, so every shot
    // comes out at the bottom of the range.
    //
    // The defaults are a Cleveland Software Design PinOne's, which reports its
    // plunger on an eight-bit Z axis resting at the centre and reading
    // positive as the rod is drawn -- SDL widens that to the range below. So a
    // PinOne needs only its axis named, and every other board needs the probe.
    std::int32_t rest = 0;
    std::int32_t drawn = 32767;

    // How much of the travel nearest the rest position still reads as rest, in
    // percent. A plunger sensor is never quite still, and a rod that reads one
    // percent drawn would hold button I forever and never let the ball go.
    std::uint8_t deadzone = 8;

    // The fall over one or two descending samples, in percent of calibrated
    // travel, that counts as release. Slow adjustments below this threshold
    // continue to follow the rod; reaching rest also releases.
    std::uint8_t release = 25;

    [[nodiscard]] bool operator==(const PlungerCalibration&) const = default;
};

// What the host should do with the shooter this frame.
//
// `draw` is the display percentage; `draw_units` retains the fine position
// used to place the shooter across its 39-pixel travel. On
// the frame `released` is set it carries the draw the rod held *before* the
// spring took it, which is the shot the player actually asked for.
struct PlungerSample {
    std::uint8_t draw = 0;
    // The shooter should be put at `draw` this frame.
    bool drawn = false;
    // The rod was let go this frame. `drawn` is still set with it, because the
    // shooter has to be moved to the shot's draw on this frame too: the
    // cartridge reads `$2D15` on the frame the button goes up, not before.
    //
    // Button I is therefore `drawn && !released` rather than `drawn`. Holding
    // it is what charges the shooter and letting it go is what fires, so the
    // release frame is exactly the frame it has to be up.
    bool released = false;

    // Calibrated travel in 1/256 percent units (0..25600). Kept separately
    // from the display percentage so fine adjustments survive normalization.
    std::uint16_t draw_units = 0;

    // The same 156 quarter-pixel positions traversed by holding button I.
    // Round to the nearest step rather than introducing extra positions
    // between the cartridge's keyboard-charge ticks. A nonzero whole-pixel
    // charge is required for the ROM to accept a release.
    [[nodiscard]] constexpr std::uint16_t shooter_position() const noexcept
    {
        const auto steps = (static_cast<std::uint32_t>(draw_units) * 156U
                            + 12800U) / 25600U;
        return static_cast<std::uint16_t>(
            (steps < 4U ? 4U : steps > 156U ? 156U : steps) * 64U);
    }

    [[nodiscard]] bool operator==(const PlungerSample&) const = default;
};

// Which controller the plunger is, and how it is read. Held as a name and an
// axis index rather than an SDL instance ID because it is written down in
// `devils-crush.ini` and has to survive the controller being unplugged,
// renumbered by Windows, or plugged into a different socket.
struct PlungerBinding {
    // Matched against the controller's name as a case-insensitive substring.
    // Empty matches the first controller with the axis, which is the ordinary
    // case on a cabinet where the plunger board is the only thing plugged in.
    std::string device;

    // The axis to read, or a negative number for no plunger, which is the
    // default.
    //
    // Off rather than on, and that is a measurement rather than caution. A
    // plunger is indistinguishable from a stick: the PinOne's rests at the
    // centre of its axis and reads negative when drawn, which is exactly what
    // a thumbstick pushed one way looks like. Axis 2 on an ordinary gamepad is
    // the right stick, so a plunger that defaulted to on would let a stick
    // charge the shooter on any desktop with a pad plugged in. Nothing can
    // tell the two apart from here, so somebody has to say -- which costs one
    // run of `--cabinet-plunger-probe`, and that has to be run anyway to learn
    // the two endpoints below.
    std::int32_t axis = -1;

    PlungerCalibration calibration;

    [[nodiscard]] bool enabled() const noexcept
    {
        return axis >= 0;
    }

    [[nodiscard]] bool operator==(const PlungerBinding&) const = default;
};

class Plunger {
public:
    void configure(const PlungerCalibration& calibration) noexcept;

    [[nodiscard]] const PlungerCalibration& calibration() const noexcept
    {
        return calibration_;
    }

    // One frame's axis reading. The value is the host's raw axis, unscaled and
    // unclamped: a rod pushed *past* its rest position -- which every spring
    // does on the way back, and which the PinOne reports as positive -- reads
    // as no draw rather than as a negative one.
    [[nodiscard]] PlungerSample update(std::int32_t raw) noexcept;

    // Forget the rod's position without forgetting the calibration.
    //
    // Called when the controller goes away and when the ball is served, and
    // for the same reason in both: a plunger that is already drawn when the
    // host starts watching it must not count as a pull. After this the rod has
    // to be seen at rest once before it can be drawn again, so a controller
    // whose axis happens to sit at full scale -- the wrong device, or one that
    // reports nothing -- can never launch anything.
    void rearm() noexcept;

    // Whether the rod has been seen at rest since the last `rearm`.
    [[nodiscard]] bool armed() const noexcept
    {
        return armed_;
    }

    // The draw the last `update` resolved, before any release test. Reported
    // for the probe, which exists to be watched while somebody moves the rod.
    [[nodiscard]] std::uint8_t draw() const noexcept
    {
        return draw_;
    }

private:
    [[nodiscard]] std::uint16_t resolve_draw(
        std::int32_t raw) const noexcept;

    PlungerCalibration calibration_{};
    std::uint8_t draw_ = 0;
    std::uint16_t draw_units_ = 0;
    std::uint16_t preceding_units_ = 0;
    bool drawn_ = false;
    bool armed_ = false;
};

} // namespace devils_crush::input
