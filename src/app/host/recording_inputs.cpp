// SPDX-License-Identifier: GPL-3.0-or-later
#include "recording_inputs.hpp"

namespace devils_crush::app::host {

[[nodiscard]] InputFrame canonical_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };

    if (frame_number == 180 || frame_number == 181
        || frame_number == 300 || frame_number == 301) {
        press(Button::run);
    } else if (frame_number == 420 || frame_number == 421) {
        press(Button::one);
    } else if (frame_number == 480 || frame_number == 481) {
        press(Button::two);
    } else if (frame_number == 540 || frame_number == 541) {
        press(Button::left);
        press(Button::one);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || is_pair(1000) || is_pair(1100) || is_pair(1200)
        || is_pair(1300) || is_pair(1400) || is_pair(1500)
        || is_pair(1600) || is_pair(1700)) {
        press(Button::one);
    }
    if (is_pair(1000) || is_pair(1100) || is_pair(1200)
        || is_pair(1300) || is_pair(1400) || is_pair(1500)
        || is_pair(1600) || is_pair(1700)) {
        press(Button::left);
    }
    if (is_pair(1050) || is_pair(1250)
        || is_pair(1450) || is_pair(1650)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_alternate_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Start through the same visible menus as the baseline, then use a
    // different, sustained flipper rhythm and six short nudge windows.
    // It deliberately changes only player input: any rule it reaches is a
    // natural table branch, not a RAM-forced fixture.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)) {
        press(Button::one);
    }
    if (frame_number >= 1000 && frame_number <= 1195) {
        press(Button::one);
    }
    if (frame_number >= 1225) {
        const int flipper_phase = (frame_number - 1225) % 24;
        if (flipper_phase <= 2) {
            press(Button::left);
        }
        if (flipper_phase >= 10 && flipper_phase <= 12) {
            press(Button::one);
        }
    }
    if (is_pair(1425) || is_pair(1825) || is_pair(2225)
        || is_pair(2625) || is_pair(3025) || is_pair(3425)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_sweep_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Start through the same visible menus, then sweep both flippers through
    // a deliberately different cadence.  This route remains input-only: its
    // purpose is to discover a naturally selected rule branch, not to force
    // one by changing the emulated game state.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1215)) {
        press(Button::one);
    }
    if (frame_number >= 1240) {
        const int flipper_phase = (frame_number - 1240) % 40;
        if (flipper_phase <= 4 || (flipper_phase >= 22 && flipper_phase <= 26)) {
            press(Button::left);
        }
        if ((flipper_phase >= 10 && flipper_phase <= 14)
            || flipper_phase >= 32) {
            press(Button::right);
        }
        if (flipper_phase == 18 || flipper_phase == 19) {
            press(Button::one);
        }
    }
    if (is_pair(1375) || is_pair(1675) || is_pair(1975)
        || is_pair(2275) || is_pair(2575) || is_pair(2875)
        || is_pair(3175) || is_pair(3475)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_crossfire_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // This pass starts through the established menus and launch sequence,
    // then gives the ball a materially different table trajectory: each
    // flipper remains held across the opposite flipper's release.  As with
    // the other discovery routes, only controller state is changed.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1225)) {
        press(Button::one);
    }
    if (frame_number >= 1260) {
        const int flipper_phase = (frame_number - 1260) % 32;
        if (flipper_phase <= 15 || flipper_phase >= 24) {
            press(Button::left);
        }
        if (flipper_phase >= 8 && flipper_phase <= 23) {
            press(Button::right);
        }
        if (flipper_phase == 6 || flipper_phase == 7
            || flipper_phase == 22 || flipper_phase == 23) {
            press(Button::one);
        }
    }
    if (is_pair(1510) || is_pair(1910) || is_pair(2310)
        || is_pair(2710) || is_pair(3110) || is_pair(3510)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_orbit_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Enter the same visible menus and launch the first ball normally.  Once
    // it is live, use a 48-frame orbit with separated flipper windows and a
    // rotating set of directional nudges.  No work-RAM state is patched: any
    // rule path reached here is selected by the game itself.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1235)) {
        press(Button::one);
    }
    if (frame_number >= 1260) {
        const int phase = (frame_number - 1260) % 48;
        if (phase <= 7 || (phase >= 32 && phase <= 35)) {
            press(Button::left);
        }
        if ((phase >= 12 && phase <= 19) || phase >= 40) {
            press(Button::right);
        }
        if (phase == 8 || phase == 9 || phase == 28 || phase == 29) {
            press(Button::one);
        }
        if (phase == 4 || phase == 5) {
            press(Button::up);
        } else if (phase == 20 || phase == 21) {
            press(Button::right);
        } else if (phase == 36 || phase == 37) {
            press(Button::down);
        }
    }
    if (is_pair(1540) || is_pair(2020) || is_pair(2500)
        || is_pair(2980) || is_pair(3460)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_lanes_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Use the normal visible menu and launch sequence, then alternate long
    // individual flipper holds.  The 96-frame pattern intentionally leaves
    // wide unheld windows and injects directional nudge bursts, producing a
    // lane-oriented trajectory instead of another dense flipper cadence.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1245)) {
        press(Button::one);
    }
    if (frame_number >= 1280) {
        const int phase = (frame_number - 1280) % 96;
        if (phase <= 23) {
            press(Button::left);
        } else if (phase >= 48 && phase <= 71) {
            press(Button::right);
        }
        if (phase == 30 || phase == 31 || phase == 78 || phase == 79) {
            press(Button::one);
        }
        if (phase == 12 || phase == 13) {
            press(Button::up);
        } else if (phase == 40 || phase == 41) {
            press(Button::left);
        } else if (phase == 60 || phase == 61) {
            press(Button::right);
        } else if (phase == 88 || phase == 89) {
            press(Button::down);
        }
    }
    if (is_pair(1460) || is_pair(1940) || is_pair(2420)
        || is_pair(2900) || is_pair(3380)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_stagger_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Follow the normal visible menu and launch path, then use a 144-frame
    // stagger. Each flipper owns a long, deliberately offset window and both
    // release for 64 frames between them. Sparse directional presses happen
    // only in those release gaps, producing a trajectory unlike the dense,
    // overlapping, or alternating patterns in the existing discovery plans.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1255)) {
        press(Button::one);
    }
    if (frame_number >= 1296) {
        const int phase = (frame_number - 1296) % 144;
        if (phase <= 19) {
            press(Button::left);
        } else if (phase >= 84 && phase <= 103) {
            press(Button::right);
        }
        if (phase == 44 || phase == 45 || phase == 124 || phase == 125) {
            press(Button::one);
        }
        if (phase == 34 || phase == 35) {
            press(Button::up);
        } else if (phase == 68 || phase == 69) {
            press(Button::right);
        } else if (phase == 114 || phase == 115) {
            press(Button::down);
        }
    }
    if (is_pair(1580) || is_pair(2180) || is_pair(2780)
        || is_pair(3380)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_pulse_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Keep the visible boot/menu/launch sequence, then use brief, asymmetric
    // flipper pulses separated by long neutral intervals. The short presses
    // aim for a low-contact ball path rather than another held-flipper orbit;
    // only controller bits change, so every reached rule remains natural.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1270)) {
        press(Button::one);
    }
    if (frame_number >= 1320) {
        const int phase = (frame_number - 1320) % 180;
        if (phase <= 3 || (phase >= 54 && phase <= 57)) {
            press(Button::left);
        }
        if ((phase >= 24 && phase <= 27) || (phase >= 114 && phase <= 117)) {
            press(Button::right);
        }
        if (phase == 39 || phase == 40 || phase == 144 || phase == 145) {
            press(Button::one);
        }
        if (phase == 72 || phase == 73) {
            press(Button::up);
        } else if (phase == 159 || phase == 160) {
            press(Button::down);
        }
    }
    if (is_pair(1710) || is_pair(2430) || is_pair(3150)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_rally_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Preserve the same visible boot, menu, and launch path.  Once the ball
    // is live, favor extended right-flipper recovery windows and short left
    // saves on a prime-length cadence; the changing phase avoids repeating
    // any of the symmetric discovery patterns already screened.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1290)) {
        press(Button::one);
    }
    if (frame_number >= 1332) {
        const int phase = (frame_number - 1332) % 137;
        if (phase <= 8 || (phase >= 71 && phase <= 77)) {
            press(Button::left);
        }
        if (phase >= 20 && phase <= 51) {
            press(Button::right);
        }
        if (phase == 58 || phase == 59 || phase == 114 || phase == 115) {
            press(Button::one);
        }
        if (phase == 12 || phase == 13) {
            press(Button::down);
        } else if (phase == 91 || phase == 92) {
            press(Button::up);
        }
    }
    if (is_pair(1680) || is_pair(2365) || is_pair(3050)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_slalom_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Keep the visible boot, menu, and launch sequence.  Once live, make a
    // wide left save, let the ball travel, then make a mirrored right save on
    // a prime-length cadence.  The nudge direction follows the opposite
    // side, which samples side-lane rebounds without writing any game RAM.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1305)) {
        press(Button::one);
    }
    if (frame_number >= 1350) {
        const int phase = (frame_number - 1350) % 211;
        if (phase <= 27) {
            press(Button::left);
        } else if (phase >= 108 && phase <= 135) {
            press(Button::right);
        }
        if (phase == 42 || phase == 43 || phase == 151 || phase == 152) {
            press(Button::one);
        }
        if (phase == 31 || phase == 32) {
            press(Button::right);
        } else if (phase == 141 || phase == 142) {
            press(Button::left);
        } else if (phase == 86 || phase == 87) {
            press(Button::up);
        }
    }
    if (is_pair(1630) || is_pair(2263) || is_pair(2896)
        || is_pair(3529)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_pendulum_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // This deliberately leaves long travel windows between two mirrored
    // saves.  Its 257-frame cadence is relatively prime to all prior discovery
    // cadences, and its directional nudges occur away from flipper holds, so
    // it explores natural rebound timing rather than only another release
    // pattern.  No emulated RAM is changed.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1320)) {
        press(Button::one);
    }
    if (frame_number >= 1350) {
        const int phase = (frame_number - 1350) % 257;
        if (phase <= 14 || (phase >= 171 && phase <= 185)) {
            press(Button::left);
        }
        if ((phase >= 78 && phase <= 92)
            || (phase >= 214 && phase <= 228)) {
            press(Button::right);
        }
        if (phase == 25 || phase == 26 || phase == 195 || phase == 196) {
            press(Button::one);
        }
        if (phase == 43 || phase == 44) {
            press(Button::up);
        } else if (phase == 120 || phase == 121) {
            press(Button::down);
        } else if (phase == 241 || phase == 242) {
            press(Button::right);
        }
    }
    if (is_pair(1764) || is_pair(2535) || is_pair(3306)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_nudge_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // The normal visible startup and launcher stay untouched. Once the ball
    // is live, a 313-frame prime cadence rotates sparse saves with separated
    // directional nudges. This makes a collision/rebound-led route rather
    // than another continuous flipper orbit, and remains entirely input-only.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1335)) {
        press(Button::one);
    }
    if (frame_number >= 1380) {
        const int phase = (frame_number - 1380) % 313;
        if ((phase >= 18 && phase <= 23)
            || (phase >= 208 && phase <= 213)) {
            press(Button::left);
        }
        if ((phase >= 104 && phase <= 109)
            || (phase >= 272 && phase <= 277)) {
            press(Button::right);
        }
        if (phase == 52 || phase == 53 || phase == 244 || phase == 245) {
            press(Button::one);
        }
        if (phase == 0 || phase == 1) {
            press(Button::up);
        } else if (phase == 76 || phase == 77) {
            press(Button::right);
        } else if (phase == 156 || phase == 157) {
            press(Button::down);
        } else if (phase == 300 || phase == 301) {
            press(Button::left);
        }
    }
    if (is_pair(1848) || is_pair(2787)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_switchback_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Use the normal visible startup, but keep the launcher held longer than
    // the other discovery routes.  The live-table section then has a
    // 233-frame switchback: long left and right saves alternate around broad
    // neutral intervals, while sparse opposite-direction nudges happen only
    // between saves.  It remains strictly input-only.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1410)) {
        press(Button::one);
    }
    if (frame_number >= 1450) {
        const int phase = (frame_number - 1450) % 233;
        if (phase >= 12 && phase <= 47) {
            press(Button::left);
        } else if (phase >= 132 && phase <= 167) {
            press(Button::right);
        }
        if (phase == 62 || phase == 63 || phase == 186 || phase == 187) {
            press(Button::one);
        }
        if (phase == 0 || phase == 1) {
            press(Button::down);
        } else if (phase == 92 || phase == 93) {
            press(Button::right);
        } else if (phase == 202 || phase == 203) {
            press(Button::left);
        }
    }
    if (is_pair(1733) || is_pair(2432) || is_pair(3131)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_spiral_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // This route releases the launcher sooner, then runs a prime-length
    // spiral of separated saves. Each directional nudge occurs while the
    // opposite flipper is held, changing both the timing and orientation of
    // a possible recovery. It deliberately remains input-only.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1180)) {
        press(Button::one);
    }
    if (frame_number >= 1210) {
        const int phase = (frame_number - 1210) % 359;
        if ((phase >= 8 && phase <= 25)
            || (phase >= 206 && phase <= 217)) {
            press(Button::left);
        }
        if ((phase >= 79 && phase <= 96)
            || (phase >= 285 && phase <= 296)) {
            press(Button::right);
        }
        if ((phase >= 40 && phase <= 45)
            || (phase >= 248 && phase <= 253)) {
            press(Button::one);
        }
        if (phase == 12 || phase == 13) {
            press(Button::up);
        } else if (phase == 84 || phase == 85) {
            press(Button::right);
        } else if (phase == 210 || phase == 211) {
            press(Button::down);
        } else if (phase == 289 || phase == 290) {
            press(Button::left);
        }
    }
    if (is_pair(1569) || is_pair(2646) || is_pair(3364)) {
        press(Button::two);
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_ricochet_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Preserve the visible startup, then release the launcher before the
    // recent long-hold routes. A 419-frame prime cycle uses brief, separated
    // saves but repeatedly taps Button II in neutral travel windows. The
    // four rotating directions bias each rebound differently while remaining
    // an entirely player-input-driven route.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1160)) {
        press(Button::one);
    }
    if (frame_number >= 1195) {
        const int phase = (frame_number - 1195) % 419;
        if ((phase >= 15 && phase <= 20)
            || (phase >= 268 && phase <= 273)) {
            press(Button::left);
        }
        if ((phase >= 112 && phase <= 117)
            || (phase >= 342 && phase <= 347)) {
            press(Button::right);
        }
        if (phase == 56 || phase == 57 || phase == 190 || phase == 191
            || phase == 306 || phase == 307 || phase == 386 || phase == 387) {
            press(Button::two);
        }
        if (phase == 0 || phase == 1) {
            press(Button::up);
        } else if (phase == 151 || phase == 152) {
            press(Button::right);
        } else if (phase == 229 || phase == 230) {
            press(Button::down);
        } else if (phase == 371 || phase == 372) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_volley_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Release the launcher before every previous long-launch route, then use
    // a 503-frame prime cycle of sustained alternating flipper banks. The
    // nudge volleys land midway through the neutral travel gaps, rather than
    // during a save, to change the ball's next contact without synthesizing
    // or patching any game state.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1090)) {
        press(Button::one);
    }
    if (frame_number >= 1130) {
        const int phase = (frame_number - 1130) % 503;
        if ((phase >= 5 && phase <= 44)
            || (phase >= 261 && phase <= 302)) {
            press(Button::one);
        }
        if ((phase >= 67 && phase <= 108)
            || (phase >= 324 && phase <= 369)) {
            press(Button::left);
        }
        if ((phase >= 132 && phase <= 135)
            || (phase >= 211 && phase <= 214)
            || (phase >= 405 && phase <= 408)
            || (phase >= 468 && phase <= 471)) {
            press(Button::two);
        }
        if (phase == 154 || phase == 155) {
            press(Button::up);
        } else if (phase == 237 || phase == 238) {
            press(Button::right);
        } else if (phase == 387 || phase == 388) {
            press(Button::down);
        } else if (phase == 487 || phase == 488) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_whirlpool_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // The normal boot and menu inputs remain unchanged.  After a mid-range
    // launcher release, this prime-length cycle keeps both flippers down in
    // two offset windows, then lets the ball travel through long neutral
    // sections.  The directional inputs occur only while neither flipper is
    // held, making this a rebound-led route rather than a variation on the
    // existing overlapping-save plans.  It changes no emulated RAM.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1140)) {
        press(Button::one);
    }
    if (frame_number >= 1188) {
        const int phase = (frame_number - 1188) % 521;
        if ((phase >= 9 && phase <= 28)
            || (phase >= 278 && phase <= 297)) {
            press(Button::left);
        }
        if ((phase >= 71 && phase <= 94)
            || (phase >= 365 && phase <= 390)) {
            press(Button::right);
        }
        if ((phase >= 154 && phase <= 159)
            || (phase >= 451 && phase <= 456)) {
            press(Button::one);
        }
        if (phase == 122 || phase == 123 || phase == 425 || phase == 426) {
            press(Button::two);
        }
        if (phase == 46 || phase == 47) {
            press(Button::up);
        } else if (phase == 226 || phase == 227) {
            press(Button::right);
        } else if (phase == 331 || phase == 332) {
            press(Button::down);
        } else if (phase == 493 || phase == 494) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_cascade_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Start from the normal boot/menu path, but release the launcher early.
    // A prime-length cycle then waits through a long neutral descent before
    // alternating short, asymmetric saves.  Directional nudges occur only in
    // the neutral intervals, so this route is a natural trajectory probe and
    // does not seed gameplay state.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1064)) {
        press(Button::one);
    }
    if (frame_number >= 1111) {
        const int phase = (frame_number - 1111) % 607;
        if (phase >= 84 && phase <= 95) {
            press(Button::left);
        }
        if (phase >= 303 && phase <= 320) {
            press(Button::right);
        }
        if (phase >= 512 && phase <= 517) {
            press(Button::one);
        }
        if (phase == 178 || phase == 179 || phase == 424 || phase == 425) {
            press(Button::two);
        }
        if (phase == 31 || phase == 32) {
            press(Button::down);
        } else if (phase == 236 || phase == 237) {
            press(Button::left);
        } else if (phase == 389 || phase == 390) {
            press(Button::up);
        } else if (phase == 569 || phase == 570) {
            press(Button::right);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_higharc_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Keep the visible startup unchanged, but give the launcher a much longer
    // charge than the early-release routes. The 701-frame prime cadence that
    // follows favors isolated recovery saves and lets the ball travel between
    // them. Directional nudges fall only in neutral intervals, preserving an
    // entirely natural trajectory without any state mutation.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1460)) {
        press(Button::one);
    }
    if (frame_number >= 1508) {
        const int phase = (frame_number - 1508) % 701;
        if (phase >= 73 && phase <= 80) {
            press(Button::left);
        }
        if (phase >= 414 && phase <= 423) {
            press(Button::right);
        }
        if (phase >= 260 && phase <= 264) {
            press(Button::one);
        }
        if (phase == 128 || phase == 129 || phase == 553 || phase == 554) {
            press(Button::two);
        }
        if (phase == 19 || phase == 20) {
            press(Button::up);
        } else if (phase == 196 || phase == 197) {
            press(Button::left);
        } else if (phase == 349 || phase == 350) {
            press(Button::down);
        } else if (phase == 628 || phase == 629) {
            press(Button::right);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_twist_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Deliberately release the launcher between the early and higharc plans,
    // then alternate two long saves on an 887-frame cadence.  Directional
    // input falls only in the neutral travel intervals, making this another
    // strictly input-only probe for a naturally selected rule branch.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1340)) {
        press(Button::one);
    }
    if (frame_number >= 1397) {
        const int phase = (frame_number - 1397) % 887;
        if (phase >= 68 && phase <= 91) {
            press(Button::left);
        }
        if (phase >= 315 && phase <= 343) {
            press(Button::right);
        }
        if (phase >= 526 && phase <= 531) {
            press(Button::one);
        }
        if (phase == 185 || phase == 186 || phase == 747 || phase == 748) {
            press(Button::two);
        }
        if (phase == 17 || phase == 18) {
            press(Button::up);
        } else if (phase == 221 || phase == 222) {
            press(Button::left);
        } else if (phase == 468 || phase == 469) {
            press(Button::down);
        } else if (phase == 702 || phase == 703) {
            press(Button::right);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_sidestep_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Keep the common visible boot/menu sequence, but use a middle-strength
    // launch and then bias the ball toward alternating side-wall recoveries.
    // The 947-frame prime period has short, unequal saves and Button-II only
    // in neutral windows, so it is a trajectory probe rather than a variant
    // of a sustained flipper bank.  It never mutates emulated game state.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1275)) {
        press(Button::one);
    }
    if (frame_number >= 1328) {
        const int phase = (frame_number - 1328) % 947;
        if ((phase >= 38 && phase <= 49)
            || (phase >= 586 && phase <= 603)) {
            press(Button::left);
        }
        if ((phase >= 217 && phase <= 223)
            || (phase >= 758 && phase <= 783)) {
            press(Button::right);
        }
        if (phase >= 404 && phase <= 409) {
            press(Button::one);
        }
        if (phase == 116 || phase == 117 || phase == 341 || phase == 342
            || phase == 690 || phase == 691 || phase == 871 || phase == 872) {
            press(Button::two);
        }
        if (phase == 12 || phase == 13) {
            press(Button::left);
        } else if (phase == 165 || phase == 166) {
            press(Button::up);
        } else if (phase == 505 || phase == 506) {
            press(Button::right);
        } else if (phase == 909 || phase == 910) {
            press(Button::down);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_weave_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Hold the launcher longer than the usual early-release probes, then
    // alternate broad saves on a prime cadence. The directional impulses lie
    // outside both saves, which keeps the route wholly input-driven while
    // steering a different sequence of natural rebounds.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1395)) {
        press(Button::one);
    }
    if (frame_number >= 1452) {
        const int phase = (frame_number - 1452) % 613;
        if (phase >= 51 && phase <= 72) {
            press(Button::left);
        }
        if (phase >= 347 && phase <= 373) {
            press(Button::right);
        }
        if (phase >= 506 && phase <= 511) {
            press(Button::one);
        }
        if (phase == 138 || phase == 139 || phase == 282 || phase == 283
            || phase == 445 || phase == 446 || phase == 578 || phase == 579) {
            press(Button::two);
        }
        if (phase == 19 || phase == 20) {
            press(Button::up);
        } else if (phase == 206 || phase == 207) {
            press(Button::right);
        } else if (phase == 425 || phase == 426) {
            press(Button::down);
        } else if (phase == 596 || phase == 597) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_bankshot_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // This plan deliberately lands between the existing early and delayed
    // launch probes. Its 1,091-frame cadence leaves broad neutral windows
    // around asymmetric bank saves, letting Button-II steer only free travel
    // rather than duplicate a held-flipper trajectory.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1310)) {
        press(Button::one);
    }
    if (frame_number >= 1373) {
        const int phase = (frame_number - 1373) % 1091;
        if ((phase >= 47 && phase <= 61)
            || (phase >= 721 && phase <= 732)) {
            press(Button::left);
        }
        if ((phase >= 284 && phase <= 304)
            || (phase >= 882 && phase <= 891)) {
            press(Button::right);
        }
        if (phase >= 514 && phase <= 518) {
            press(Button::one);
        }
        if (phase == 146 || phase == 147 || phase == 426 || phase == 427
            || phase == 650 || phase == 651 || phase == 972 || phase == 973) {
            press(Button::two);
        }
        if (phase == 19 || phase == 20) {
            press(Button::up);
        } else if (phase == 201 || phase == 202) {
            press(Button::right);
        } else if (phase == 470 || phase == 471) {
            press(Button::down);
        } else if (phase == 805 || phase == 806) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_playfield_input_for_frame(
    const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Charge farther than the prior higharc plan before releasing into long,
    // deliberately isolated saves.  Every directional input is in a neutral
    // travel interval, so this remains a purely natural table-rule probe.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1525)) {
        press(Button::one);
    }
    if (frame_number >= 1594) {
        const int phase = (frame_number - 1594) % 1277;
        if (phase >= 35 && phase <= 92) {
            press(Button::left);
        }
        if (phase >= 412 && phase <= 451) {
            press(Button::right);
        }
        if (phase >= 708 && phase <= 715) {
            press(Button::one);
        }
        if (phase == 185 || phase == 186 || phase == 583 || phase == 584
            || phase == 970 || phase == 971 || phase == 1168
            || phase == 1169) {
            press(Button::two);
        }
        if (phase == 18 || phase == 19) {
            press(Button::up);
        } else if (phase == 296 || phase == 297) {
            press(Button::right);
        } else if (phase == 644 || phase == 645) {
            press(Button::down);
        } else if (phase == 1094 || phase == 1095) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_corner_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // A late but not maximum charge is followed by a right-first corner save
    // and a far-left recovery. The 743-frame cadence deliberately leaves
    // large neutral intervals for natural table travel and directional nudges.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1480)) {
        press(Button::one);
    }
    if (frame_number >= 1543) {
        const int phase = (frame_number - 1543) % 743;
        if (phase >= 84 && phase <= 103) {
            press(Button::right);
        }
        if (phase >= 472 && phase <= 508) {
            press(Button::left);
        }
        if (phase >= 626 && phase <= 632) {
            press(Button::one);
        }
        if (phase == 173 || phase == 174 || phase == 329 || phase == 330
            || phase == 557 || phase == 558 || phase == 699 || phase == 700) {
            press(Button::two);
        }
        if (phase == 24 || phase == 25) {
            press(Button::down);
        } else if (phase == 251 || phase == 252) {
            press(Button::right);
        } else if (phase == 404 || phase == 405) {
            press(Button::up);
        } else if (phase == 718 || phase == 719) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_alley_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Charge beyond the former maximum before releasing into an alley-first
    // route. The paired left saves and shorter right bank use an 811-frame
    // prime cadence, while every directional impulse falls in free travel.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1580)) {
        press(Button::one);
    }
    if (frame_number >= 1651) {
        const int phase = (frame_number - 1651) % 811;
        if ((phase >= 58 && phase <= 83)
            || (phase >= 604 && phase <= 622)) {
            press(Button::left);
        }
        if (phase >= 381 && phase <= 404) {
            press(Button::right);
        }
        if (phase >= 628 && phase <= 634) {
            press(Button::one);
        }
        if (phase == 124 || phase == 125 || phase == 273 || phase == 274
            || phase == 524 || phase == 525 || phase == 713 || phase == 714) {
            press(Button::two);
        }
        if (phase == 18 || phase == 19) {
            press(Button::up);
        } else if (phase == 210 || phase == 211) {
            press(Button::down);
        } else if (phase == 466 || phase == 467) {
            press(Button::right);
        } else if (phase == 777 || phase == 778) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_rebound_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // Release a medium-long charge into a long right rebound and an early
    // left recovery. The 1,009-frame prime cadence gives the ball long
    // neutral travel between saves and every directional impulse is isolated.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1235)) {
        press(Button::one);
    }
    if (frame_number >= 1306) {
        const int phase = (frame_number - 1306) % 1009;
        if (phase >= 36 && phase <= 51) {
            press(Button::right);
        }
        if (phase >= 214 && phase <= 238) {
            press(Button::left);
        }
        if (phase >= 668 && phase <= 719) {
            press(Button::right);
        }
        if (phase >= 485 && phase <= 491) {
            press(Button::one);
        }
        if (phase == 129 || phase == 130 || phase == 350 || phase == 351
            || phase == 578 || phase == 579 || phase == 890 || phase == 891) {
            press(Button::two);
        }
        if (phase == 19 || phase == 20) {
            press(Button::up);
        } else if (phase == 177 || phase == 178) {
            press(Button::down);
        } else if (phase == 398 || phase == 399) {
            press(Button::right);
        } else if (phase == 823 || phase == 824) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_skew_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // The late launch creates a different initial arc, then a 1,201-frame
    // prime cadence biases a wide left return before the short right bank.
    // Button-II and directional impulses remain in neutral travel windows.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1400)) {
        press(Button::one);
    }
    if (frame_number >= 1469) {
        const int phase = (frame_number - 1469) % 1201;
        if ((phase >= 71 && phase <= 113)
            || (phase >= 844 && phase <= 865)) {
            press(Button::left);
        }
        if (phase >= 352 && phase <= 374) {
            press(Button::right);
        }
        if (phase >= 568 && phase <= 574) {
            press(Button::one);
        }
        if (phase == 146 || phase == 147 || phase == 440 || phase == 441
            || phase == 695 || phase == 696 || phase == 1049
            || phase == 1050) {
            press(Button::two);
        }
        if (phase == 24 || phase == 25) {
            press(Button::up);
        } else if (phase == 233 || phase == 234) {
            press(Button::right);
        } else if (phase == 612 || phase == 613) {
            press(Button::down);
        } else if (phase == 988 || phase == 989) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_helix_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // This late high launch enters a right-first wide-save pattern on a
    // 1,237-frame prime cadence. Every Button-II and directional impulse is
    // isolated in neutral travel so the route remains natural and input-only.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1515)) {
        press(Button::one);
    }
    if (frame_number >= 1589) {
        const int phase = (frame_number - 1589) % 1237;
        if ((phase >= 47 && phase <= 89)
            || (phase >= 761 && phase <= 789)) {
            press(Button::right);
        }
        if (phase >= 301 && phase <= 328) {
            press(Button::left);
        }
        if (phase >= 531 && phase <= 537) {
            press(Button::one);
        }
        if (phase == 150 || phase == 151 || phase == 405 || phase == 406
            || phase == 671 || phase == 672 || phase == 998
            || phase == 999) {
            press(Button::two);
        }
        if (phase == 21 || phase == 22) {
            press(Button::up);
        } else if (phase == 242 || phase == 243) {
            press(Button::left);
        } else if (phase == 583 || phase == 584) {
            press(Button::down);
        } else if (phase == 1109 || phase == 1110) {
            press(Button::right);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_vector_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // A medium-high launch leads into long right, left, then short right
    // banks. The 1,271-frame prime cadence avoids repeating prior save
    // timing, while non-flipper impulses remain in neutral travel.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1330)) {
        press(Button::one);
    }
    if (frame_number >= 1411) {
        const int phase = (frame_number - 1411) % 1271;
        if ((phase >= 69 && phase <= 100)
            || (phase >= 803 && phase <= 816)) {
            press(Button::right);
        }
        if (phase >= 361 && phase <= 383) {
            press(Button::left);
        }
        if (phase >= 574 && phase <= 580) {
            press(Button::one);
        }
        if (phase == 159 || phase == 160 || phase == 434 || phase == 435
            || phase == 703 || phase == 704 || phase == 1025
            || phase == 1026) {
            press(Button::two);
        }
        if (phase == 24 || phase == 25) {
            press(Button::up);
        } else if (phase == 265 || phase == 266) {
            press(Button::left);
        } else if (phase == 619 || phase == 620) {
            press(Button::down);
        } else if (phase == 1145 || phase == 1146) {
            press(Button::right);
        }
    }
    return input;
}

[[nodiscard]] InputFrame gameplay_radial_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    // The late launch and 1,301-frame prime cadence make long left, right,
    // then short left saves. Directional and Button-II impulses stay between
    // held-flipper windows to retain an input-only natural trajectory.
    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)
        || (frame_number >= 1000 && frame_number <= 1445)) {
        press(Button::one);
    }
    if (frame_number >= 1517) {
        const int phase = (frame_number - 1517) % 1301;
        if ((phase >= 53 && phase <= 75)
            || (phase >= 812 && phase <= 830)) {
            press(Button::left);
        }
        if (phase >= 319 && phase <= 347) {
            press(Button::right);
        }
        if (phase >= 600 && phase <= 606) {
            press(Button::one);
        }
        if (phase == 145 || phase == 146 || phase == 454 || phase == 455
            || phase == 711 || phase == 712 || phase == 1091
            || phase == 1092) {
            press(Button::two);
        }
        if (phase == 22 || phase == 23) {
            press(Button::up);
        } else if (phase == 227 || phase == 228) {
            press(Button::right);
        } else if (phase == 528 || phase == 529) {
            press(Button::down);
        } else if (phase == 1220 || phase == 1221) {
            press(Button::left);
        }
    }
    return input;
}

[[nodiscard]] InputFrame state_census_input_for_frame(const int frame_number)
{
    InputFrame input;
    const auto press = [&input](const Button button) {
        input.player_one |= static_cast<std::uint16_t>(button);
    };
    const auto is_pair = [frame_number](const int first_frame) {
        return frame_number == first_frame
            || frame_number == first_frame + 1;
    };

    if (is_pair(180) || is_pair(300)) {
        press(Button::run);
    }
    if (is_pair(420) || is_pair(700)
        || (frame_number >= 850 && frame_number <= 880)) {
        press(Button::one);
    }

    const bool launch_hold =
        frame_number >= 1000 && frame_number <= 1180;
    if (launch_hold) {
        press(Button::one);
    }

    if (frame_number >= 1250) {
        const int flipper_phase = (frame_number - 1250) % 30;
        if (flipper_phase <= 1) {
            press(Button::left);
        }
        if (flipper_phase == 15 || flipper_phase == 16) {
            press(Button::one);
        }
    }
    if (is_pair(1450) || is_pair(2250) || is_pair(3050)) {
        press(Button::two);
    }
    return input;
}

} // namespace devils_crush::app::host
