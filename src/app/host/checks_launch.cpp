// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks_launch.hpp"

#include "cabinet_table.hpp"
#include "command_line.hpp"
#include "host_files.hpp"
#include "native_registry_install.hpp"
#include "play_assists.hpp"
#include "recording_inputs.hpp"
#include "recording_plans.hpp"

namespace devils_crush::app::host {

namespace {

// Whether how far the rod is pulled reaches the game, measured rather than
// asserted.
//
// The claim the plunger rests on is that the shot comes out of the draw. That
// is checkable without a plunger: drive the same model the host drives from a
// scripted rod, pull it to a different distance on each pass, and read back
// the two bytes the cartridge computed for itself -- the charge it settled on
// at `$2D15`, and the launch velocity `02:51DA` derived from it and stored in
// the ball's own `$3580`. Four draws that produce four velocities is the whole
// feature working end to end; four draws that produce one velocity would mean
// the rod never reached the shooter.
//
// The rod is scripted rather than sampled: it ramps back over four frames, is
// held until the shooter has followed it for ten, and is let go in one, which
// is what a briskly pulled rod and a spring return look like at 60Hz.
//
// Four frames rather than a leisurely ramp because that is the case the ball
// has to survive. Nothing in the charge path places the ball -- it rests on
// the shooter and falls after it, which works at the quarter-pixel-per-tick
// the cartridge descends at and not at the ten a rod does. The ball's distance
// above the shooter is therefore the third thing measured, and `02:52CA` says
// what it has to be: nine.
//
// What the check does *not* require is that the ball get away, because the
// cartridge does not require it either. The velocity is
// `-(($2D15 >> 2) + 2)`, which is four pixels a frame at a quarter draw, and
// four is not enough to climb the shooter lane: the ball rolls back down and
// the shooter resets to charge again. That is the original's own behaviour on
// a short press of button I, and reproducing it is the point rather than a
// defect -- so what is measured is the velocity the cartridge computed, and
// whether the ball escaped is reported beside it.
struct PlungerCheckPass {
    std::uint8_t draw = 0;
    std::uint8_t charge = 0;
    std::uint8_t velocity = 0;
    int launched_at = 0;
    int shooter_began = 0;
    int driven = 0;
    bool escaped = false;
    int ride = 0;
};

[[nodiscard]] int run_plunger_check_pass(
    devils_crush::emulation::PceRuntime& runtime,
    const std::uint8_t draw,
    PlungerCheckPass& pass,
    const int release_middle = -1)
{
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::uint8_t kShooterSession = 0x08U;
    constexpr std::size_t kChargeCounter = 0x0D15U;
    constexpr std::size_t kBallVelocityY = 0x1580U;
    constexpr int kScriptUntil = 1000;
    constexpr int kRampFrames = 4;
    // Frames the shooter has to have followed the rod before the script lets
    // it go, rather than a count of frames since the ball was served. The two
    // are not the same: `$53` bit 3 goes up at 942 on this route and the
    // shooter does not install its charge script until 1,084, so a rod let go
    // on a timer would be home before there was anything to move.
    constexpr int kDriveFrames = 10;
    constexpr int kFrames = 1600;

    runtime.reset();
    pass = {.draw = draw};
    devils_crush::input::Plunger rod;
    // An axis whose reading is its draw, so the script says what it means.
    rod.configure(
        {.rest = 0, .drawn = 100, .deadzone = 0, .release = 25});

    int shooter_began = 0;
    int released_at = 0;
    bool session = false;
    bool letting_go = false;
    bool sampled_middle = false;
    for (int frame_number = 1; frame_number <= kFrames; ++frame_number) {
        const auto work_ram = runtime.export_work_ram();
        if (work_ram.size() < 0x2000U) {
            return EXIT_FAILURE;
        }
        const bool in_launcher =
            (std::to_integer<std::uint8_t>(work_ram[kSessionFlags])
             & kShooterSession)
            != 0U;
        if (in_launcher && shooter_began == 0
            && frame_number >= kScriptUntil) {
            shooter_began = frame_number;
            pass.shooter_began = frame_number;
        }
        session = in_launcher;
        if (shooter_began != 0 && !in_launcher) {
            pass.escaped = true;
        }
        // The charge the rod settled on, read back on the frame after it was
        // written rather than trusted from the host's side, and the velocity
        // the cartridge made of it.
        //
        // The launch frame is found by the charge starting to come back rather
        // than by watching the ball, because `02:51DA` does both in the same
        // dispatch: it stores the velocity at `02:51FA` and then `02:5208 JSR
        // $533B` begins retracting the shooter. The first frame `$2D15` is
        // below what the rod set is therefore the frame the shot was computed,
        // and `$3580` on it is the shot.
        //
        // Watching the ball instead is what an earlier version of this did,
        // taking the most negative `$3580` of the following thirty frames --
        // and it read a step too fast at the two largest draws, because a ball
        // launched hard picks up more speed later in the lane than it left
        // with. The extreme of the flight is not the launch.
        if (released_at != 0 && pass.charge == 0) {
            pass.charge =
                std::to_integer<std::uint8_t>(work_ram[kChargeCounter]);
        }
        if (released_at != 0 && pass.charge != 0 && pass.launched_at == 0
            && std::to_integer<std::uint8_t>(work_ram[kChargeCounter])
                < pass.charge) {
            pass.launched_at = frame_number;
            pass.velocity =
                std::to_integer<std::uint8_t>(work_ram[kBallVelocityY]);
        }

        std::int32_t raw = 0;
        if (shooter_began != 0 && !letting_go) {
            const int elapsed = frame_number - shooter_began;
            raw = elapsed < kRampFrames
                ? (static_cast<std::int32_t>(draw) * elapsed) / kRampFrames
                : draw;
        } else if (letting_go && release_middle >= 0 && !sampled_middle) {
            raw = release_middle;
            sampled_middle = true;
        }
        const auto sample = rod.update(raw);
        const bool engaged = session && sample.drawn
            && apply_plunger_draw(runtime, sample);
        if (engaged) {
            ++pass.driven;
        }
        letting_go = letting_go || pass.driven >= kDriveFrames;
        if (sample.released && released_at == 0) {
            released_at = frame_number;
            // How far above the shooter the ball is at the moment of the
            // shot, which is where it was carried to rather than where the
            // host put it: the ball is read back out of work RAM after a
            // frame has run on it.
            std::size_t shooter = 0;
            for (std::size_t slot = 0;
                 slot < devils_crush::cabinet::kActorSlotCount;
                 ++slot) {
                const auto dispatch = static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(work_ram[0x1F00U + slot])
                    | (static_cast<std::uint16_t>(
                           std::to_integer<std::uint8_t>(
                               work_ram[0x1F80U + slot]))
                       << 8U));
                if (dispatch == 0x5159U) {
                    shooter = slot;
                }
            }
            // Observe the position actually supplied on the release frame.
            // The previous frame can straddle a whole-pixel boundary while
            // the ROM advances the fractional shooter coordinate.
            const auto placed = runtime.export_work_ram();
            const auto whole_y = [&placed](const std::size_t slot) {
                return static_cast<int>(
                    std::to_integer<std::uint8_t>(placed[0x1300U + slot])
                    | (std::to_integer<std::uint8_t>(placed[0x1380U + slot])
                       << 8U));
            };
            pass.ride = whole_y(shooter) - whole_y(0);
            // This route uses 25/50/75/100 and 60 percent pulls: the first
            // four are exact quarter steps; 60 percent rounds to 23.5 pixels.
            const auto expected_fraction = static_cast<std::uint8_t>(
                draw == 25U ? 192U : draw == 75U ? 64U
                : draw == 100U ? 0U : 128U);
            if (!engaged
                || std::to_integer<std::uint8_t>(placed[0x1280U + shooter])
                    != expected_fraction
                || placed[0x1280U] != placed[0x1280U + shooter]) {
                std::cerr << "Fractional plunger position did not reach the shooter and ball.\n";
                return EXIT_FAILURE;
            }
        }
        InputFrame input;
        if (frame_number < kScriptUntil) {
            input = state_census_input_for_frame(frame_number);
        } else if (engaged && !sample.released) {
            input.player_one = static_cast<std::uint16_t>(Button::one);
        }
        static_cast<void>(runtime.run_frame(input));
    }
    return pass.launched_at != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

// Runs the quick state hands-off until the ball drains and keeps going, with
// the full cabinet decode on every frame, so a drain that crashes or locks up
// does it here rather than in front of someone playing.
//
// A crash surfaces as an exception or a fault out of decode_cabinet_table. A
// lock-up surfaces as work RAM that stops changing: the drain sequence hands
// off to the ball-loss scene and then back to the table, and if either end of
// that handoff never completes the state simply stops moving.
[[nodiscard]] int run_drain_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path,
    const bool uncull,
    const CommandLine* const native_registry)
{
    // This probe exists to catch a fault, and a fault loses whatever is still
    // sitting in the stream buffer, so nothing here may be buffered.
    std::cout << std::unitbuf;
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    // A session runs with the registry deployed, so a drain that only fails
    // there fails because of a native routine. Probing the stock ROM alone
    // would measure a configuration nobody plays.
    NativeRegistryDeployment registry_deployment;
    if (native_registry != nullptr
        && !install_native_registry(
            runtime, *native_registry, *rom, {}, registry_deployment)) {
        std::cerr << "Could not deploy the native registry.\n";
        return EXIT_FAILURE;
    }
    if (!load_quick_state(runtime, state_path)) {
        return EXIT_FAILURE;
    }
    std::cout << "camera cull: " << (uncull ? "lifted" : "original")
              << ", registry: "
              << (native_registry != nullptr ? "deployed" : "off") << "\n";

    devils_crush::render::PersistentActorSpriteCache actors;
    devils_crush::render::TablePaletteCache palette;
    std::uint64_t previous_hash = 0;
    int stalled_since = 0;
    int longest_stall = 0;
    int longest_stall_frame = 0;
    std::optional<devils_crush::cabinet::PlayfieldView> previous_view;
    std::optional<std::uint8_t> previous_balls;
    // Hands off until the ball drains, then work the plunger: "impossible to
    // continue playing the second ball" is a claim about what happens after
    // the drain, so the probe has to actually try to launch one.
    int drained_at = 0;
    int highest_reach = 0x7FFF;
    constexpr int kFrames = 5000;
    for (int frame_number = 1; frame_number <= kFrames; ++frame_number) {
        if (uncull) {
            keep_fixed_actors_simulating(runtime);
        }
        InputFrame probe_input{};
        if (drained_at != 0) {
            const int since = frame_number - drained_at;
            // Button I charges the plunger and releasing it fires. A single
            // timed press can land while the ball is still being returned and
            // be spent on nothing, so keep cycling: if the ball never leaves
            // the lane under this it is not for want of asking.
            if (since > 240 && (since / 60) % 2 == 0) {
                probe_input.player_one =
                    static_cast<std::uint16_t>(Button::one);
            }
        }
        if (frame_number % 100 == 0) {
            std::cout << "  ... frame " << frame_number << "\n";
        }
        static_cast<void>(runtime.run_frame(probe_input));
        const auto work_ram = runtime.export_work_ram();
        const auto state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        if (!state) {
            std::cout << "frame " << frame_number
                      << ": cabinet state failed to decode\n";
            return EXIT_FAILURE;
        }
        if (!previous_view || *previous_view != state->playfield) {
            std::cout << "frame " << frame_number << ": playfield -> "
                      << (state->playfield
                                  == devils_crush::cabinet::PlayfieldView::
                                      table_background
                              ? "table"
                              : "emulated framebuffer")
                      << "\n";
            previous_view = state->playfield;
        }
        if (!previous_balls || *previous_balls != state->balls_in_session) {
            std::cout << "frame " << frame_number << ": balls -> "
                      << static_cast<unsigned>(state->balls_in_session)
                      << "\n";
            if (previous_balls && state->balls_in_session < *previous_balls
                && drained_at == 0) {
                drained_at = frame_number;
            }
            previous_balls = state->balls_in_session;
        }
        if (drained_at != 0 && work_ram.size() >= 0x2000U) {
            const int since = frame_number - drained_at;
            const auto byte_at = [&work_ram](const std::size_t offset) {
                return static_cast<int>(
                    std::to_integer<std::uint8_t>(work_ram[offset]));
            };
            const auto ball_y = static_cast<int>(
                static_cast<std::int16_t>(static_cast<std::uint16_t>(
                    byte_at(0x1300U) | (byte_at(0x1380U) << 8))));
            highest_reach = std::min(highest_reach, ball_y);
            // The table is 768 rows and a session has at most a handful of
            // balls. Either of these leaving its range means state has been
            // overwritten, which is the failure this probe exists to catch.
            if (ball_y < -2000 || ball_y > 2000
                || state->balls_in_session > 5U) {
                std::cout << "CORRUPTED at frame " << frame_number << " (+"
                          << since << " after the drain): ball y " << ball_y
                          << ", balls "
                          << static_cast<unsigned>(state->balls_in_session)
                          << "\n";
                return EXIT_FAILURE;
            }
            if (since % 120 == 0 && since <= 2400) {
                // $AA:$A9 is the anchor 00:F4FC culls against; $0C:$0B is the
                // camera the cabinet crops with. If the ball's own $3080 bit 7
                // is clear it has been culled and stops simulating, which is
                // exactly what a ball that will not play looks like.
                const auto anchor = static_cast<int>(
                    static_cast<std::int16_t>(static_cast<std::uint16_t>(
                        byte_at(0x00A9U) | (byte_at(0x00AAU) << 8))));
                std::cout << "  +" << std::setw(4) << since
                          << " after drain: ball type "
                          << byte_at(0x1000U) << " flags "
                          << std::hex << byte_at(0x1080U) << std::dec
                          << " y " << ball_y << " anchor " << anchor
                          << " camera "
                          << static_cast<int>(
                                 static_cast<std::int16_t>(
                                     static_cast<std::uint16_t>(
                                         byte_at(0x0BU)
                                         | (byte_at(0x0CU) << 8))))
                          << "\n";
            }
        }

        std::uint64_t hash = 1469598103934665603ULL;
        for (const auto byte : work_ram) {
            hash = (hash ^ std::to_integer<std::uint8_t>(byte))
                * 1099511628211ULL;
        }
        if (hash == previous_hash) {
            if (frame_number - stalled_since > longest_stall) {
                longest_stall = frame_number - stalled_since;
                longest_stall_frame = stalled_since;
            }
        } else {
            stalled_since = frame_number;
        }
        previous_hash = hash;

        const auto video = runtime.export_video_memory();
        if (!video
            || state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        const auto table = decode_cabinet_table(
            *rom, work_ram, *video, actors, palette,
            CabinetLayers::all, false, nullptr, nullptr);
        if (!table) {
            std::cout << "frame " << frame_number
                      << ": cabinet table failed to compose\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "survived " << kFrames
              << " frames; longest run of unchanged work RAM was "
              << longest_stall << " frames from frame " << longest_stall_frame
              << "\nafter the drain the ball reached y " << highest_reach
              << " (the plunger lane is around 720; the upper table is "
                 "under 300)\n";
    return EXIT_SUCCESS;
}

[[nodiscard]] int run_nudge_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path,
    const CommandLine* const native_registry,
    const bool from_boot,
    const bool nudge_enabled,
    NudgePassResult& pass)
{
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    // The hooks survive a reset, so the second pass reuses the first pass's
    // deployment rather than installing over it.
    static NativeRegistryDeployment registry_deployment;
    static bool registry_installed = false;
    if (native_registry != nullptr && !registry_installed) {
        if (!install_native_registry(
                runtime, *native_registry, *rom, {}, registry_deployment)) {
            std::cerr << "Could not deploy the native registry.\n";
            return EXIT_FAILURE;
        }
        registry_installed = true;
    }
    if (!from_boot && !load_quick_state(runtime, state_path)) {
        return EXIT_FAILURE;
    }
    std::cout << "\n=== " << (from_boot ? "from a cold boot" : "from the quick state")
              << ", registry: "
              << (native_registry != nullptr ? "deployed" : "off") << " ===\n";

    // Work RAM is exported from $2000: zero page $53 lands at $0053 and every
    // $2xxx address at its low twelve bits.
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::size_t kEdgeInputBase = 0x0018U;
    constexpr std::size_t kHeldInputBase = 0x0013U;
    constexpr std::size_t kActivePlayer = 0x0251U;
    constexpr std::size_t kNudgePhase = 0x0CC4U;
    constexpr std::size_t kNudgeCounter = 0x0CC5U;
    constexpr std::size_t kNudgeTimer = 0x0CC6U;
    constexpr std::size_t kTiltLock = 0x0CC7U;
    constexpr std::size_t kHeldInputCopy = 0x0CEEU;
    constexpr std::size_t kMotionLowBase = 0x1500U;
    constexpr std::size_t kMotionHighBase = 0x1580U;

    struct Sample {
        unsigned edge = 0;
        unsigned held = 0;
        unsigned held_copy = 0;
        unsigned session = 0;
        unsigned tilt = 0;
        unsigned timer = 0;
        unsigned phase = 0;
        unsigned counter = 0;
        unsigned player = 0;
        int motion = 0;
        int ball_x = 0;
        int ball_y = 0;
    };

    const auto sample = [&](const Sample& previous) {
        Sample now;
        const auto work_ram = runtime.export_work_ram();
        if (work_ram.size() < 0x2000U) {
            return previous;
        }
        const auto byte_at = [&work_ram](const std::size_t offset) {
            return static_cast<unsigned>(
                static_cast<std::uint8_t>(work_ram[offset]));
        };
        now.player = byte_at(kActivePlayer);
        const auto player = now.player & 0x01U;
        now.edge = byte_at(kEdgeInputBase + player);
        now.held = byte_at(kHeldInputBase + player);
        now.held_copy = byte_at(kHeldInputCopy);
        now.session = byte_at(kSessionFlags);
        now.tilt = byte_at(kTiltLock);
        now.timer = byte_at(kNudgeTimer);
        now.phase = byte_at(kNudgePhase);
        now.counter = byte_at(kNudgeCounter);
        now.motion = static_cast<int>(
            static_cast<std::int16_t>(static_cast<std::uint16_t>(
                byte_at(kMotionLowBase) | (byte_at(kMotionHighBase) << 8U))));
        now.ball_x = static_cast<int>(
            byte_at(0x1180U) | (byte_at(0x1200U) << 8U));
        now.ball_y = static_cast<int>(
            byte_at(0x1300U) | (byte_at(0x1380U) << 8U));
        return now;
    };


    // The quick state has the ball in the Devil Shooter, where `$53.3` is set
    // and `01:C3CB` returns at `$C3DC` before Button II is ever tested --
    // correctly, since a ball in the launcher cannot be nudged. So launch
    // first: hold Button I to charge the shooter, release, and only start
    // tapping once the shooter session has actually ended.
    //
    // Tap rather than hold: `$2018` is an edge latch, so a held button that
    // never re-edges would be indistinguishable from one that never arrives.
    constexpr int kFrames = 1800;
    constexpr int kPeriod = 60;
    constexpr int kPressFrames = 3;

    std::size_t pressed_frames = 0;
    std::size_t edge_frames = 0;
    std::size_t held_frames = 0;
    std::size_t motion_changes_while_pressed = 0;
    std::size_t blocked_by_session = 0;
    std::size_t blocked_by_tilt = 0;
    std::size_t phase_changes = 0;
    unsigned highest_counter = 0;
    unsigned highest_tilt = 0;
    std::vector<std::pair<int, unsigned>> session_timeline;
    std::set<unsigned> session_values;
    std::set<unsigned> timer_values;
    std::set<unsigned> phase_values;
    int printed = 0;

    // `$2CC6` is decremented by the first instruction past the session gate
    // and by nothing else, so a frame where it changed is a frame the gate
    // let through. That makes "blocked" and "let through but had no effect"
    // distinguishable without inspecting the routine itself.
    std::size_t gate_passed = 0;
    std::size_t gate_passed_while_pressed = 0;
    int first_blocked_frame = 0;
    int last_passed_frame = 0;

    const int total_frames = from_boot ? kStateCensusRecordingFrames : kFrames;
    const int nudge_from = from_boot ? 1250 : 1;

    Sample previous = sample(Sample{});
    std::cout << "frame  in  $2018 $2013 $2CEE  $53 tilt timer phase count "
                 "  motion\n";
    for (int frame_number = 1; frame_number <= total_frames; ++frame_number) {
        InputFrame replay_input{};
        if (from_boot) {
            // The census script boots the frontend, starts a session and
            // launches the ball. Its own Button-II taps are stripped so the
            // two passes differ in nothing but the nudge.
            replay_input = state_census_input_for_frame(frame_number);
            replay_input.player_one = static_cast<std::uint16_t>(
                replay_input.player_one
                & ~static_cast<std::uint16_t>(Button::two));
        }
        // Sparse taps first, then deliberate spam. `$2CC5` accumulates nudges
        // and is cleared every time the `$2CC6` counter wraps, so a tilt can
        // only be provoked by nudging repeatedly inside one wrap.
        const int period = frame_number >= 2400 ? 8 : kPeriod;
        const bool press = nudge_enabled && frame_number >= nudge_from
            && ((frame_number - nudge_from) % period) < kPressFrames;
        if (press) {
            replay_input.player_one |=
                static_cast<std::uint16_t>(Button::two);
            ++pressed_frames;
        }
        static_cast<void>(runtime.run_frame(replay_input));
        const auto now = sample(previous);

        if (press) {
            if ((now.edge & 0x02U) != 0U) {
                ++edge_frames;
            }
            if ((now.held & 0x02U) != 0U) {
                ++held_frames;
            }
            if (now.motion != previous.motion) {
                ++motion_changes_while_pressed;
            }
            // The two early returns before Button II is ever tested.
            if ((now.session & 0x02U) != 0U) {
                ++blocked_by_session;
            }
            if (now.tilt != 0U) {
                ++blocked_by_tilt;
            }
        }
        if (now.phase != previous.phase) {
            ++phase_changes;
        }
        if (now.timer != previous.timer) {
            ++gate_passed;
            last_passed_frame = frame_number;
            if (press) {
                ++gate_passed_while_pressed;
            }
        } else if (last_passed_frame != 0 && first_blocked_frame == 0) {
            first_blocked_frame = frame_number;
        }
        if (now.session != previous.session) {
            session_timeline.emplace_back(frame_number, now.session);
        }
        pass.ball_x.push_back(now.ball_x);
        pass.ball_y.push_back(now.ball_y);
        if ((now.session & 0x2AU) == 0U) {
            ++pass.in_play_frames;
        }
        highest_counter = std::max(highest_counter, now.counter);
        highest_tilt = std::max(highest_tilt, now.tilt);
        session_values.insert(now.session);
        timer_values.insert(now.timer);
        phase_values.insert(now.phase);

        const bool interesting = press || (now.phase != previous.phase)
            || now.session != previous.session;
        if (interesting && printed < 40) {
            ++printed;
            std::cout << std::setw(5) << frame_number << std::setw(4)
                      << (press ? "II" : "-") << std::hex << std::setw(7)
                      << now.edge << std::setw(6) << now.held << std::setw(6)
                      << now.held_copy << std::setw(5) << now.session
                      << std::setw(5) << now.tilt << std::dec << std::setw(6)
                      << now.timer << std::setw(6) << now.phase << std::setw(6)
                      << now.counter << std::setw(8) << now.motion << '\n';
        }
        previous = now;
    }

    const auto set_text = [](const std::set<unsigned>& values) {
        std::string text;
        for (const auto value : values) {
            if (!text.empty()) {
                text += ' ';
            }
            text += std::format("{:02X}", value);
        }
        return text;
    };

    std::cout << "\nover " << total_frames
              << " frames, Button II was pressed on " << pressed_frames << "\n"
              << "  frames the session gate let through ($2CC6 moved): "
              << gate_passed << "\n"
              << "    of those, with Button II pressed: "
              << gate_passed_while_pressed << "\n"
              << "  last frame the gate let through: " << last_passed_frame
              << "\n"
              << "  first frame it blocked after that: " << first_blocked_frame
              << "\n"
              << "  frames where the edge latch $2018 carried bit 1: "
              << edge_frames << "\n"
              << "  frames where the held latch $2013 carried bit 1: "
              << held_frames << "\n"
              << "  slot-0 motion changed while pressed: "
              << motion_changes_while_pressed << "\n"
              << "  nudge phase $2CC4 changed: " << phase_changes << "\n"
              << "  blocked before the button test by $53 bit 1: "
              << blocked_by_session << "\n"
              << "  blocked before the button test by tilt lock $2CC7: "
              << blocked_by_tilt << "\n"
              << "  $53 values seen: " << set_text(session_values) << "\n"
              << "  $2CC6 values seen: " << set_text(timer_values) << "\n"
              << "  $2CC4 values seen: " << set_text(phase_values) << "\n"
              << "  highest nudge accumulator $2CC5: " << highest_counter
              << "\n"
              << "  highest tilt lock $2CC7: " << highest_tilt << "\n";

    // `$53 & $28` is the gate. Printing every transition shows whether the
    // blocking value is a passing session state or one the session latches
    // and never leaves.
    std::cout << "  $53 timeline (frame=value, * blocks the nudge):\n   ";
    if (session_timeline.empty()) {
        std::cout << " never changed from "
                  << std::format("{:02X}", previous.session);
    }
    for (std::size_t index = 0; index < session_timeline.size(); ++index) {
        if (index != 0U && index % 8U == 0U) {
            std::cout << "\n   ";
        }
        const auto [frame, value] = session_timeline[index];
        std::cout << ' ' << frame << '='
                  << std::format("{:02X}", value)
                  << ((value & 0x2AU) != 0U ? "*" : "");
    }
    std::cout << "\n";
    return EXIT_SUCCESS;
}

[[nodiscard]] int run_launch_check(
    devils_crush::emulation::PceRuntime& runtime,
    const Button ordinary_input,
    const std::string_view label)
{
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::uint8_t kShooterSession = 0x08U;
    constexpr int kHoldFrom = 1000;
    constexpr int kReleaseAt = 1180;
    constexpr int kFrames = 1600;

    runtime.reset();
    int launched_at = 0;
    int first_shooter_frame = 0;
    std::size_t button_one_frames = 0;
    std::size_t ordinary_frames = 0;
    int ball_y_at_hold = 0;
    int ball_y_at_end = 0;
    std::uint8_t last_session = 0xFFU;
    std::uint8_t highest_charge = 0;
    std::vector<std::pair<int, unsigned>> session_timeline;

    for (int frame_number = 1; frame_number <= kFrames; ++frame_number) {
        const auto work_ram = runtime.export_work_ram();
        if (work_ram.size() < 0x2000U) {
            return EXIT_FAILURE;
        }
        const bool in_launcher =
            (std::to_integer<std::uint8_t>(work_ram[kSessionFlags])
             & kShooterSession)
            != 0U;
        const auto ball_y = static_cast<int>(
            std::to_integer<std::uint8_t>(work_ram[0x1300U])
            | (std::to_integer<std::uint8_t>(work_ram[0x1380U]) << 8U));
        if (in_launcher && first_shooter_frame == 0) {
            first_shooter_frame = frame_number;
        }
        if (first_shooter_frame != 0 && !in_launcher && launched_at == 0) {
            launched_at = frame_number;
        }

        // Before the hold the census script drives; after it, one key is held
        // and the launch rule decides what that key sends. The Devil Shooter
        // charges while button I is down and fires when it is let go, so the
        // key has to be released or nothing ever launches.
        const bool holding =
            frame_number >= kHoldFrom && frame_number < kReleaseAt;
        InputFrame input;
        if (frame_number < kHoldFrom) {
            input = state_census_input_for_frame(frame_number);
        } else if (holding) {
            if (ball_y_at_hold == 0) {
                ball_y_at_hold = ball_y;
            }
            if (in_launcher) {
                input.player_one = static_cast<std::uint16_t>(Button::one);
                ++button_one_frames;
            } else {
                input.player_one =
                    static_cast<std::uint16_t>(ordinary_input);
                ++ordinary_frames;
            }
        }
        const auto session = std::to_integer<std::uint8_t>(
            work_ram[kSessionFlags]);
        if (session != last_session) {
            session_timeline.emplace_back(frame_number, session);
            last_session = session;
        }
        // The Devil Shooter's charge counter, per docs/ROUTINE_MAP.md.
        highest_charge = std::max(
            highest_charge,
            std::to_integer<std::uint8_t>(work_ram[0x0D15U]));
        static_cast<void>(runtime.run_frame(input));
        ball_y_at_end = ball_y;
    }

    std::cout << "holding " << label << " from frame " << kHoldFrom << ":\n"
              << "  shooter session began at frame " << first_shooter_frame
              << "\n"
              << "  frames the key sent button I: " << button_one_frames
              << "\n"
              << "  frames it sent its ordinary input: " << ordinary_frames
              << "\n"
              << "  ball left the shooter at frame "
              << (launched_at != 0 ? std::to_string(launched_at)
                                   : std::string("never"))
              << "\n"
              << "  ball y at hold " << ball_y_at_hold << ", at end "
              << ball_y_at_end << "\n"
              << "  highest charge counter $2D15: "
              << static_cast<unsigned>(highest_charge) << "\n"
              << "  $53 timeline:";
    for (const auto& [frame, value] : session_timeline) {
        std::cout << ' ' << frame << '=' << std::format("{:02X}", value);
    }
    std::cout << "\n";
    return launched_at != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

[[nodiscard]] int run_plunger_check(
    devils_crush::emulation::PceRuntime& runtime)
{
    constexpr std::array<std::uint8_t, 4> kDraws{25U, 50U, 75U, 100U};
    std::array<PlungerCheckPass, kDraws.size()> passes{};
    for (std::size_t index = 0; index < kDraws.size(); ++index) {
        if (run_plunger_check_pass(runtime, kDraws[index], passes[index])
            != EXIT_SUCCESS) {
            std::cerr << "The shooter never fired at a draw of "
                      << static_cast<unsigned>(kDraws[index])
                      << "%: the session began at frame "
                      << passes[index].shooter_began << ", the rod drove it "
                      << passes[index].driven
                      << " times, and the charge settled at "
                      << static_cast<unsigned>(passes[index].charge)
                      << ".\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "scripted plunger, one pass per draw:\n"
              << "  draw   $2D15   $3580   fired at   escaped   ride\n";
    std::set<std::uint8_t> velocities;
    for (const auto& pass : passes) {
        velocities.insert(pass.velocity);
        std::cout << std::format(
            "  {:>3}%   {:>5}   ${:02X}     {:>8}   {:>7}   {:>4}\n",
            static_cast<unsigned>(pass.draw),
            static_cast<unsigned>(pass.charge),
            static_cast<unsigned>(pass.velocity),
            pass.launched_at,
            pass.escaped ? "yes" : "no",
            pass.ride);
    }
    // Four draws have to produce four velocities. Anything less means part of
    // the rod's travel is not reaching the cartridge, which is exactly the
    // failure a button-mapped plunger would have.
    if (velocities.size() != kDraws.size()) {
        std::cerr << "Only " << velocities.size()
                  << " distinct launch velocities: the draw is not reaching "
                     "the shooter.\n";
        return EXIT_FAILURE;
    }
    // And the ball has to still be on the shooter when it fires. A rod moves
    // faster than gravity carries the ball, so without the ride being taken
    // over the shooter slides out from under it and the gap grows with the
    // draw -- measured at 14, 24, 34 and 44 before this was fixed, against the
    // nine `02:52CA` asks for.
    for (const auto& pass : passes) {
        if (pass.ride != kBallRidesAbove) {
            std::cerr << "At a draw of "
                      << static_cast<unsigned>(pass.draw)
                      << "% the ball sat " << pass.ride
                      << " pixels above the shooter rather than "
                      << kBallRidesAbove
                      << ": it is not riding the rod.\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "  " << velocities.size()
              << " distinct launch velocities, and the ball rode the rod in "
                 "every one.\n";
    PlungerCheckPass direct;
    if (run_plunger_check_pass(runtime, 60U, direct) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    for (const int middle : {55, 45, 40}) {
        PlungerCheckPass phased;
        if (run_plunger_check_pass(runtime, 60U, phased, middle) != EXIT_SUCCESS
            || phased.charge != direct.charge
            || phased.velocity != direct.velocity
            || phased.ride != direct.ride) {
            std::cerr << "A sampled spring return weakened the sixty-percent launch.\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "  Fractional positions preserved; three sampled spring returns match the direct launch.\n";
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
