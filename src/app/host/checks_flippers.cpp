// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks_flippers.hpp"

#include "checks_launch.hpp"
#include "command_line.hpp"
#include "host_input.hpp"
#include "native_registry_install.hpp"
#include "play_assists.hpp"
#include "recording_inputs.hpp"

namespace devils_crush::app::host {

namespace {

// The middle of the ball's sprite as the stitched table draws it, and as the
// hardware's sprite table has it on the screen.
struct DraggedBallSeen {
    SDL_FPoint stitched{};
    std::optional<SDL_FPoint> screen;
    int velocity_x = 0;
    int velocity_y = 0;
    int camera = 0;
    // `$3080` bit 7: inside the camera's rows, so drawn and run.
    bool on_camera = false;
};

[[nodiscard]] std::optional<DraggedBallSeen> see_dragged_ball(
    devils_crush::emulation::PceRuntime& runtime,
    const std::span<const std::byte> rom)
{
    const auto ram = runtime.export_work_ram();
    const auto video = runtime.export_video_memory();
    const auto sprites =
        devils_crush::render::decode_ball_and_flipper_sprites(rom, ram);
    if (ram.size() < 0x2000U || !video || !sprites) {
        return std::nullopt;
    }
    const auto word = [&ram](const std::size_t low, const std::size_t high) {
        return static_cast<int>(static_cast<std::int16_t>(
            std::to_integer<unsigned>(ram[low])
            | (std::to_integer<unsigned>(ram[high]) << 8U)));
    };
    for (const auto& sprite : *sprites) {
        if (sprite.actor_slot != devils_crush::cabinet::kBallActorSlot) {
            continue;
        }
        DraggedBallSeen seen;
        seen.stitched.x = static_cast<float>(sprite.world_x + kBallSpriteHalf);
        seen.stitched.y = static_cast<float>(
            sprite.world_y + kBallSpriteHalf - kStitchedTableSpriteTop);
        seen.velocity_x = word(0x1400U, 0x1480U);
        seen.velocity_y = word(0x1500U, 0x1580U);
        seen.camera = word(0xA9U, 0xAAU);
        seen.on_camera =
            (std::to_integer<unsigned>(ram[0x1080U]) & 0x80U) != 0U;
        for (std::size_t at = 0; at + 3U < video->sat_words.size(); at += 4U) {
            if (video->sat_words[at + 2U] != sprite.pattern) {
                continue;
            }
            SDL_FPoint screen;
            screen.x = static_cast<float>(
                static_cast<int>(video->sat_words[at + 1U] & 0x3FFU)
                - 0x20 + kBallSpriteHalf);
            screen.y = static_cast<float>(
                static_cast<int>(video->sat_words[at] & 0x3FFU)
                - 0x40 + kBallSpriteHalf);
            seen.screen = screen;
            break;
        }
        return seen;
    }
    return std::nullopt;
}

// Frames of the last `frames` the ball's sprite was within a pixel of the
// point it was held at, by the picture the point is in, after `settle` frames
// for the camera to bring it there. Minus one if the ball was ever off camera.
[[nodiscard]] int hold_dragged_ball(
    devils_crush::emulation::PceRuntime& runtime,
    const std::span<const std::byte> rom,
    const SDL_FPoint point,
    const bool stitched,
    const int frames,
    const bool bonus,
    const int settle = 0)
{
    int held = 0;
    bool culled = false;
    bool return_pending = false;
    for (int frame = -settle; frame < frames; ++frame) {
        if (!drag_ball_to_picture_point(runtime, rom, point, stitched)) {
            release_dragged_ball(runtime);
        }
        static_cast<void>(runtime.run_frame(InputFrame{}));
        if (bonus) {
            static_cast<void>(service_synthetic_bonus_stage_return(
                runtime, return_pending));
        }
        const auto seen = see_dragged_ball(runtime, rom);
        culled = culled || (seen && !bonus && !seen->on_camera);
        if (frame < 0 || !seen || (!stitched && !seen->screen)) {
            continue;
        }
        const SDL_FPoint at = stitched ? seen->stitched : *seen->screen;
        if (std::abs(at.x - point.x) <= 1.0F
            && std::abs(at.y - point.y) <= 1.0F) {
            ++held;
        }
    }
    return culled ? -1 : held;
}

} // namespace

// The ball-dragging cheat, on the census route's first ball and in a bonus
// stage: that the pointer's pixel is where the ball's sprite ends up, in the
// stitched table's picture and on the cartridge's own screen, that a moving
// hold throws, and that nothing is carried out of the Devil Shooter.
[[nodiscard]] int run_ball_drag_check(
    devils_crush::emulation::PceRuntime& runtime,
    const std::vector<std::byte>& rom,
    const CommandLine* const native_registry)
{
    // The configuration people play: the held ball's `RTS` has to get through
    // the native scheduler as it does the cartridge's. The hooks capture the
    // deployment, so they go before it does.
    NativeRegistryDeployment registry_deployment;
    if (native_registry != nullptr
        && !install_native_registry(
            runtime, *native_registry, rom, {}, registry_deployment)) {
        std::cerr << "Could not deploy the native registry.\n";
        return EXIT_FAILURE;
    }
    struct HookRelease {
        devils_crush::emulation::PceRuntime& runtime;
        ~HookRelease() { runtime.clear_execution_hooks(); }
    } hook_release{runtime};
    std::cout << "Native registry: "
              << (native_registry != nullptr ? "deployed" : "off") << ".\n";

    const auto see = [&runtime, &rom]() {
        return see_dragged_ball(runtime, rom);
    };
    const auto hold = [&runtime, &rom](const SDL_FPoint point,
                          const bool stitched, const int frames,
                          const bool bonus, const int settle = 0) {
        return hold_dragged_ball(
            runtime, rom, point, stitched, frames, bonus, settle);
    };

    runtime.reset();
    bool ok = true;
    for (int frame_number = 1; frame_number <= 1400; ++frame_number) {
        InputFrame input;
        if (frame_number < 1000) {
            input = state_census_input_for_frame(frame_number);
        }
        if (frame_number == 1000) {
            // Still being served, in none of the ball's own routines:
            // nothing may be written.
            auto ram = runtime.export_work_ram();
            const auto before = ram;
            if (devils_crush::input::drag_ball(ram, 128.0, 300.0)
                || ram != before) {
                std::cerr << "A ball still being served was carried.\n";
                ok = false;
            }
        }
        static_cast<void>(runtime.run_frame(input));
    }
    // The ball is never launched: it is taken from the Devil Shooter, where
    // it sits with the shooter's bit up.
    {
        const auto ram = runtime.export_work_ram();
        if (ram.size() < 0x2000U
            || (std::to_integer<unsigned>(ram[0x53U]) & 0x08U) == 0U
            || !devils_crush::input::ball_can_be_dragged(ram)) {
            std::cerr << "The census route's ball was not waiting in the "
                         "Devil Shooter.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Ball taken from the Devil Shooter.\n";
    }

    constexpr int kHoldFrames = 40;
    constexpr std::array<SDL_FPoint, 5> kTablePoints{{
        {128.0F, 330.0F}, {70.0F, 150.0F}, {180.0F, 60.0F},
        {128.0F, 520.0F}, {60.0F, 420.0F},
    }};
    int table_points_held = 0;
    // The camera comes at eight rows a frame, and the table is 668 long.
    constexpr int kSettleFrames = 100;
    std::cout << "Held on the stitched table, frames of " << kHoldFrames
              << " within a pixel once the camera has come:\n";
    for (const auto point : kTablePoints) {
        const int held = hold(point, true, kHoldFrames, false, kSettleFrames);
        const auto seen = see();
        std::cout << "  (" << point.x << ',' << point.y << ")  " << held;
        if (seen) {
            std::cout << "  ends at (" << seen->stitched.x << ','
                      << seen->stitched.y << "), camera " << seen->camera;
        }
        std::cout << '\n';
        if (held == kHoldFrames) {
            ++table_points_held;
        }
    }
    // Carried out of the lane without touching its gate, the ball is
    // launched all the same: the shooter's bit is down and the gate shut.
    {
        const auto ram = runtime.export_work_ram();
        if ((std::to_integer<unsigned>(ram[0x53U]) & 0x08U) != 0U
            || std::to_integer<unsigned>(
                   ram[devils_crush::input::kDragLaneGateType]) != 0x82U) {
            std::cerr << "The ball carried out of the Devil Shooter's lane "
                         "was not taken as launched.\n";
            ok = false;
        }
    }
    if (table_points_held != static_cast<int>(kTablePoints.size())) {
        std::cerr << "The ball did not stay where the stitched table's "
                     "pointer held it.\n";
        ok = false;
    }

    // The same on the cartridge's own screen, as the one window shows it,
    // where the camera moves under the pointer.
    {
        const SDL_FPoint point{128.0F, 120.0F};
        const int held = hold(point, false, kHoldFrames, false);
        std::cout << "Held on the table's own screen: " << held << " of "
                  << kHoldFrames << ".\n";
        if (held != kHoldFrames) {
            std::cerr << "The ball did not stay where the screen's pointer "
                         "held it.\n";
            ok = false;
        }
    }

    // And at the screen's top edge the camera is brought up the table.
    {
        const int from = see() ? see()->camera : 0;
        const int held = hold({128.0F, 12.0F}, false, kHoldFrames, false);
        const auto seen = see();
        std::cout << "Held at the screen's top edge: camera " << from
                  << " to " << (seen ? seen->camera : 0) << ".\n";
        if (held < 0 || !seen || seen->camera > from - 4 * kHoldFrames) {
            std::cerr << "Holding the ball at the screen's edge did not "
                         "bring the camera.\n";
            ok = false;
        }
    }

    // A hold moving four pixels a frame leaves the ball going that way.
    {
        static_cast<void>(hold({100.0F, 520.0F}, true, 10, false, 100));
        for (int step = 1; step <= 8; ++step) {
            static_cast<void>(hold(
                {100.0F + 4.0F * static_cast<float>(step), 520.0F},
                true, 1, false));
        }
        const auto held_at = see();
        release_dragged_ball(runtime);
        static_cast<void>(runtime.run_frame(InputFrame{}));
        const auto let_go = see();
        static_cast<void>(runtime.run_frame(InputFrame{}));
        const auto seen = see();
        const float stayed = let_go && held_at
            ? let_go->stitched.x - held_at->stitched.x : -1.0F;
        const float thrown =
            seen && let_go ? seen->stitched.x - let_go->stitched.x : 0.0F;
        std::cout << "Let go moving four pixels a frame: " << stayed
                  << " pixels on in the first frame, " << thrown
                  << " in the second.\n";
        // Let go outside the lane, the shooter's routine has handed the ball
        // on to the one for a ball in play.
        if (devils_crush::input::dragged_ball_handler(
                runtime.export_work_ram())
            != devils_crush::input::kDragBallInPlayHandler) {
            std::cerr << "The ball let go on the table was not in play.\n";
            ok = false;
        }
        if (stayed != 0.0F || thrown < 3.0F) {
            std::cerr << "A moving hold did not throw the ball.\n";
            ok = false;
        }
    }

    runtime.reset();
    if (!prepare_bonus_stage_launch(runtime, 1U)) {
        return EXIT_FAILURE;
    }
    bool return_pending = true;
    bool room = false;
    for (int frame = 0; frame < 900 && !room; ++frame) {
        static_cast<void>(runtime.run_frame(InputFrame{}));
        static_cast<void>(service_synthetic_bonus_stage_return(
            runtime, return_pending));
        const auto ram = runtime.export_work_ram();
        const auto state = devils_crush::cabinet::decode_cabinet_state(ram);
        room = state && state->bonus_stage && state->screen_selector != 0U
            && devils_crush::input::ball_can_be_dragged(ram);
    }
    if (!room) {
        std::cerr << "The bonus stage never put a ball in play.\n";
        return EXIT_FAILURE;
    }
    {
        const SDL_FPoint point{128.0F, 120.0F};
        const int held = hold(point, false, kHoldFrames, true);
        const auto seen = see();
        std::cout << "Held in bonus stage 1: " << held << " of "
                  << kHoldFrames;
        if (seen && seen->screen) {
            std::cout << ", ends at (" << seen->screen->x << ','
                      << seen->screen->y << ')';
        }
        std::cout << ".\n";
        if (held != kHoldFrames) {
            std::cerr << "The ball did not stay where the bonus stage's "
                         "pointer held it.\n";
            ok = false;
        }
    }
    if (ok) {
        std::cout << "Ball drag check passed.\n";
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

[[nodiscard]] int run_launch_flipper_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const CommandLine* const native_registry,
    const std::vector<std::byte>& rom,
    const bool uncull,
    const bool restore_input,
    LaunchFlipperPass& pass)
{
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::uint8_t kShooterSession = 0x08U;
    constexpr std::size_t kFlagsBase = 0x1080U;
    constexpr std::size_t kPhaseBase = 0x1980U;
    constexpr int kHoldFrom = 1000;
    constexpr int kReleaseAt = 1180;
    constexpr int kFrames = 1500;
    constexpr std::size_t kPieceCount =
        devils_crush::cabinet::kLastFlipperControlSlot
        - devils_crush::cabinet::kFirstFlipperControlSlot + 1U;

    runtime.reset();
    runtime.clear_execution_hooks();
    pass = {};
    std::array<unsigned, kPieceCount> last_phase{};
    bool have_phase = false;
    bool entered_shooter = false;

    // With the registry, because without it this probe measured a machine
    // nobody plays and missed the one thing it exists to catch. The C4DE
    // prefix explicitly returns at the instruction hook, so the probe checks
    // the same native/input hand-off the interactive host ships.
    NativeRegistryDeployment registry_deployment;
    std::vector<RoutineKey> claimed_routines;
    if (restore_input) {
        claimed_routines.push_back(kShooterGateInstruction);
    }
    if (native_registry != nullptr
        && !install_native_registry(
            runtime,
            *native_registry,
            rom,
            claimed_routines,
            registry_deployment)) {
        std::cerr << "Could not deploy the native registry.\n";
        return EXIT_FAILURE;
    }

    // The host's own gate lift, so the probe measures what is shipped rather
    // than a second implementation of it. The probe leaves `$3B80,X` as the
    // cartridge wrote it, so the cartridge masks are what gets restored.
    const auto flippers =
        devils_crush::cabinet::default_input_bindings().flippers;
    if (restore_input
        && !install_shooter_flipper_gate(runtime, flippers)) {
        std::cerr << "Could not install the shooter input-gate hook.\n";
        return EXIT_FAILURE;
    }

    for (int frame_number = 1; frame_number <= kFrames; ++frame_number) {
        if (uncull) {
            keep_fixed_actors_simulating(runtime);
        }
        const auto work_ram = runtime.export_work_ram();
        if (work_ram.size() < 0x2000U) {
            return EXIT_FAILURE;
        }
        const auto byte_at = [&work_ram](const std::size_t offset) {
            return static_cast<unsigned>(
                std::to_integer<std::uint8_t>(work_ram[offset]));
        };
        const bool in_launcher =
            (byte_at(kSessionFlags) & kShooterSession) != 0U;
        entered_shooter = entered_shooter || in_launcher;
        if (entered_shooter && !in_launcher && pass.launched_at == 0) {
            pass.launched_at = frame_number;
        }

        InputFrame input;
        bool held = false;
        if (frame_number < kHoldFrom) {
            input = state_census_input_for_frame(frame_number);
        } else if (frame_number < kReleaseAt && in_launcher) {
            input.player_one = static_cast<std::uint16_t>(Button::one);
        } else {
            // Ten frames on, ten off, from the moment the button is let go.
            // A held direction moves a piece through its four phases once and
            // then stands still, which measures the first answer but not the
            // rest of the window.
            held = ((frame_number - kReleaseAt) / 10) % 2 == 0;
            if (held) {
                // `$3B80,X` carries $F0 for the left-hand pieces, so any
                // direction opens their gate at `01:D466`.
                input.player_one = static_cast<std::uint16_t>(Button::left);
            }
        }

        bool dispatched = false;
        bool moved = false;
        for (std::size_t slot =
                 devils_crush::cabinet::kFirstFlipperControlSlot;
             slot <= devils_crush::cabinet::kLastFlipperControlSlot;
             ++slot) {
            const auto piece =
                slot - devils_crush::cabinet::kFirstFlipperControlSlot;
            const auto flags = byte_at(kFlagsBase + slot);
            const auto phase = byte_at(kPhaseBase + slot);
            // The scheduler skips a slot only when bit 7 is clear and bit 1
            // is set, so either condition alone leaves it running.
            if ((flags & 0x80U) != 0U || (flags & 0x02U) == 0U) {
                dispatched = true;
            }
            if (have_phase && phase != last_phase[piece]) {
                moved = true;
            }
            last_phase[piece] = phase;
        }
        have_phase = true;

        if (frame_number >= kReleaseAt) {
            if (held) {
                ++pass.held_frames;
            }
            if (dispatched) {
                ++pass.dispatched_frames;
                if (pass.first_dispatch == 0) {
                    pass.first_dispatch = frame_number;
                }
            }
            if (moved) {
                ++pass.moves;
                if (pass.first_move == 0) {
                    pass.first_move = frame_number;
                }
            }
            if (pass.launched_at == 0 || frame_number <= pass.launched_at) {
                const auto slot =
                    devils_crush::cabinet::kFirstFlipperControlSlot;
                pass.timeline.push_back({
                    .frame = frame_number,
                    .session = byte_at(kSessionFlags),
                    .input = byte_at(0x0CEEU),
                    .flags = byte_at(kFlagsBase + slot),
                    .mask = byte_at(0x1B80U + slot),
                    .phase = byte_at(kPhaseBase + slot),
                    .dispatch = byte_at(0x1F00U + slot)
                        | (byte_at(0x1F80U + slot) << 8U),
                    .ball_y = static_cast<int>(
                        byte_at(0x1300U) | (byte_at(0x1380U) << 8U)),
                    .camera_y = static_cast<int>(static_cast<std::int16_t>(
                        static_cast<std::uint16_t>(
                            byte_at(0x0BU) | (byte_at(0x0CU) << 8U)))),
                });
            }
        }

        static_cast<void>(runtime.run_frame(input));
    }

    const auto after = [](const int frame, const int base) {
        return frame != 0 ? std::to_string(frame - base)
                          : std::string("never");
    };
    runtime.clear_execution_hooks();
    const int measured = kFrames - kReleaseAt + 1;
    std::cout << "camera cull: " << (uncull ? "lifted" : "original")
              << ", shooter input gate: "
              << (restore_input ? "lifted" : "original") << "\n"
              << "  button released at frame " << kReleaseAt << "\n"
              << "  shooter session $53 bit 3 cleared at frame "
              << pass.launched_at << " (+"
              << after(pass.launched_at, kReleaseAt) << ")\n"
              << "  frames a flipper piece was dispatched: "
              << pass.dispatched_frames << " of " << measured << "\n"
              << "  frames the direction was held: " << pass.held_frames
              << "\n"
              << "  first dispatch after release: +"
              << after(pass.first_dispatch, kReleaseAt) << " frames\n"
              << "  first flipper movement after release: +"
              << after(pass.first_move, kReleaseAt) << " frames\n"
              << "  phase changes: " << pass.moves << "\n"
              << "  frame  $53 $2CEE slot04 flags/mask/phase/dispatch"
                 "  ball_y camera_y\n";
    for (std::size_t index = 0U; index < pass.timeline.size(); ++index) {
        const auto& sample = pass.timeline[index];
        const bool edge = index == 0U || index + 1U == pass.timeline.size()
            || sample.session != pass.timeline[index - 1U].session
            || sample.flags != pass.timeline[index - 1U].flags
            || sample.dispatch != pass.timeline[index - 1U].dispatch;
        if (!edge && index % 20U != 0U) {
            continue;
        }
        std::cout << "  " << std::setw(5) << sample.frame << "  "
                  << std::format("{:02X}", sample.session) << "   "
                  << std::format("{:02X}", sample.input) << "   "
                  << std::format("{:02X}", sample.flags) << "/"
                  << std::format("{:02X}", sample.mask) << "/"
                  << std::format("{:02X}", sample.phase) << "/"
                  << std::format("{:04X}", sample.dispatch) << "     "
                  << std::setw(6) << sample.ball_y << std::setw(9)
                  << sample.camera_y << "\n";
    }
    return pass.launched_at != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void report_nudge_effect(
    const NudgePassResult& without,
    const NudgePassResult& with)
{
    const auto frames = std::min(without.ball_x.size(), with.ball_x.size());
    int first_divergence = 0;
    int worst_dx = 0;
    int worst_dy = 0;
    int worst_frame = 0;
    std::size_t diverged_frames = 0;
    for (std::size_t index = 0; index < frames; ++index) {
        const auto dx = with.ball_x[index] - without.ball_x[index];
        const auto dy = with.ball_y[index] - without.ball_y[index];
        if (dx == 0 && dy == 0) {
            continue;
        }
        ++diverged_frames;
        if (first_divergence == 0) {
            first_divergence = static_cast<int>(index) + 1;
        }
        if (std::abs(dx) + std::abs(dy) > std::abs(worst_dx) + std::abs(worst_dy)) {
            worst_dx = dx;
            worst_dy = dy;
            worst_frame = static_cast<int>(index) + 1;
        }
    }

    // Divergence compounds: a ball that bounces differently once is somewhere
    // else forever. Only the first few frames measure the impulse itself.
    std::cout << "\n=== effect of nudging, same route with and without ===\n"
              << "  first frames that differ (the impulse, before it "
                 "compounds):\n";
    int listed = 0;
    for (std::size_t index = 0; index < frames && listed < 8; ++index) {
        const auto dx = with.ball_x[index] - without.ball_x[index];
        const auto dy = with.ball_y[index] - without.ball_y[index];
        if (dx == 0 && dy == 0) {
            continue;
        }
        ++listed;
        std::cout << "    frame " << (index + 1) << "  dx " << dx << "  dy "
                  << dy << "  (ball at " << with.ball_x[index] << ","
                  << with.ball_y[index] << ")\n";
    }
    std::cout << "  frames compared: " << frames << "\n"
              << "  frames the ball was in play: " << with.in_play_frames
              << "\n"
              << "  first frame the ball diverged: "
              << (first_divergence != 0 ? std::to_string(first_divergence)
                                        : std::string("never"))
              << "\n"
              << "  frames the ball sat somewhere else: " << diverged_frames
              << "\n"
              << "  widest divergence: dx " << worst_dx << ", dy " << worst_dy
              << " at frame " << worst_frame << "\n";
}

[[nodiscard]] int run_flipper_trace(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& state_path,
    const bool uncull)
{
    // Booting into play rather than loading the quick state. The flippers are
    // only driven while the ball is out of the Devil Shooter, and a saved
    // state is whatever the last F5 captured -- against one taken with the
    // ball in the launcher this whole trace reads zero, which looks like a
    // frozen flipper and is not one. Frame 1,300 of the census route is
    // reliably mid-play.
    static_cast<void>(state_path);
    runtime.reset();
    for (int boot = 1; boot <= 1300; ++boot) {
        static_cast<void>(
            runtime.run_frame(state_census_input_for_frame(boot)));
    }
    std::cout << "camera cull: " << (uncull ? "lifted" : "original") << "\n";
    // Work RAM is exported from $2000, so the actor arrays at $3000 + n*$80
    // land at offset $1000 + n*$80 and the held-input byte $2CEE at $0CEE.
    constexpr std::size_t kHeldInput = 0x0CEEU;
    constexpr std::size_t kFlagsBase = 0x1080U;
    constexpr std::size_t kPhaseBase = 0x1980U;
    constexpr std::size_t kInputMaskBase = 0x1B80U;
    constexpr std::size_t kDispatchLow = 0x1F00U;
    constexpr std::size_t kDispatchHigh = 0x1F80U;

    struct PieceRecord {
        std::set<unsigned> masks;
        std::set<unsigned> dispatches;
        std::set<unsigned> phases;
        std::size_t frames = 0;
        std::size_t gate_open_frames = 0;
        std::size_t phase_changes = 0;
        // 00:F4FC sets $3080,X bit 7 only for actors inside
        // [camera - 48, camera + 240). Cross-tabulating that flag against the
        // frames the piece actually moved says whether it is the whole story.
        std::size_t in_window_frames = 0;
        std::size_t moved_in_window = 0;
        std::size_t moved_out_of_window = 0;
        unsigned last_phase = 0x100U;
    };
    std::array<
        PieceRecord,
        devils_crush::cabinet::kLastFlipperControlSlot
            - devils_crush::cabinet::kFirstFlipperControlSlot + 1U>
        pieces;

    std::set<std::pair<unsigned, unsigned>> probe_input_map;
    std::cout << "frame  in  cam"
                 "   |  slot mask phase disp   |  ... (first 4 button edges)\n";
    std::size_t printed_windows = 0;
    for (int frame_number = 1; frame_number <= 600; ++frame_number) {
        const bool held = (frame_number / 20) % 2 == 0;
        InputFrame replay_input{};
        if (held) {
            // Each input the flippers can respond to is held in turn, so the
            // trace measures which pad bits actually reach them rather than
            // assuming. Button I works the right-hand pieces ($3B80 mask
            // $01); the left-hand ones carry mask $F0, which is every
            // direction, not just left. Button II is the nudge, not a
            // flipper, so it stays out of this.
            const auto pair = [](const Button first, const Button second) {
                return static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(first)
                    | static_cast<std::uint16_t>(second));
            };
            const std::array<std::uint16_t, 4> kProbed{
                static_cast<std::uint16_t>(Button::left),
                static_cast<std::uint16_t>(Button::right),
                // Both flipper buttons at once, which is the case a pad
                // cannot express and the host has to resolve.
                pair(Button::left, Button::right),
                static_cast<std::uint16_t>(Button::one),
            };
            replay_input.player_one =
                kProbed[static_cast<std::size_t>(frame_number / 150) % 4U];
            if (uncull) {
                // The second pass is the host's configuration, so it applies
                // the host's substitution too.
                resolve_opposing_directions(
                    replay_input.player_one,
                    devils_crush::cabinet::default_input_bindings().flippers);
            }
        }
        if (uncull) {
            keep_fixed_actors_simulating(runtime);
            // The second pass is the host's configuration, which rewrites the
            // flipper masks.
            static_cast<void>(apply_flipper_inputs(
                runtime,
                devils_crush::cabinet::default_input_bindings().flippers));
        }
        static_cast<void>(runtime.run_frame(replay_input));
        const auto work_ram = runtime.export_work_ram();
        if (work_ram.size() < 0x2000U) {
            continue;
        }
        const auto byte_at = [&work_ram](const std::size_t offset) {
            return static_cast<unsigned>(
                static_cast<std::uint8_t>(work_ram[offset]));
        };
        const auto input = byte_at(kHeldInput);
        probe_input_map.emplace(replay_input.player_one, input);
        const auto camera = static_cast<int>(
            static_cast<std::int16_t>(static_cast<std::uint16_t>(
                byte_at(0x0BU) | (byte_at(0x0CU) << 8U))));
        const bool trace_window =
            printed_windows < 4U && frame_number > 20 && (frame_number % 20) < 6;
        if (trace_window && (frame_number % 20) == 0) {
            ++printed_windows;
        }
        if (trace_window) {
            std::cout << std::setw(5) << frame_number << std::setw(4)
                      << std::hex << input << std::dec << std::setw(5)
                      << camera << "  |";
        }
        for (std::size_t slot = devils_crush::cabinet::kFirstFlipperControlSlot;
             slot <= devils_crush::cabinet::kLastFlipperControlSlot;
             ++slot) {
            auto& piece =
                pieces[slot - devils_crush::cabinet::kFirstFlipperControlSlot];
            const auto mask = byte_at(kInputMaskBase + slot);
            const auto phase = byte_at(kPhaseBase + slot);
            const auto dispatch = byte_at(kDispatchLow + slot)
                | (byte_at(kDispatchHigh + slot) << 8U);
            ++piece.frames;
            piece.masks.insert(mask);
            piece.dispatches.insert(dispatch);
            piece.phases.insert(phase);
            if ((input & mask) != 0U) {
                ++piece.gate_open_frames;
            }
            const bool in_window = (byte_at(kFlagsBase + slot) & 0x80U) != 0U;
            if (in_window) {
                ++piece.in_window_frames;
            }
            if (piece.last_phase != 0x100U && phase != piece.last_phase) {
                ++piece.phase_changes;
                if (in_window) {
                    ++piece.moved_in_window;
                } else {
                    ++piece.moved_out_of_window;
                }
            }
            piece.last_phase = phase;
            if (trace_window) {
                std::cout << "  " << std::hex << std::setw(2)
                          << std::setfill('0') << slot << " " << std::setw(2)
                          << mask << " " << std::setw(2) << phase << " "
                          << std::setw(4) << dispatch << std::setfill(' ')
                          << std::dec;
            }
        }
        if (trace_window) {
            std::cout << "\n";
        }
    }

    std::cout << "\nslot  mask(s)      dispatch(es)        phases  "
                 "gate-open  in-window  moved-in  moved-out\n";
    for (std::size_t slot = devils_crush::cabinet::kFirstFlipperControlSlot;
         slot <= devils_crush::cabinet::kLastFlipperControlSlot;
         ++slot) {
        const auto& piece =
            pieces[slot - devils_crush::cabinet::kFirstFlipperControlSlot];
        std::cout << "  " << std::hex << std::setw(2) << std::setfill('0')
                  << slot << std::setfill(' ') << "  ";
        std::string masks;
        for (const auto mask : piece.masks) {
            masks += std::format("{:02X} ", mask);
        }
        std::string dispatches;
        for (const auto dispatch : piece.dispatches) {
            dispatches += std::format("{:04X} ", dispatch);
        }
        std::cout << std::dec << std::left << std::setw(13) << masks
                  << std::setw(20) << dispatches << std::setw(8)
                  << piece.phases.size() << std::setw(11)
                  << piece.gate_open_frames << std::setw(11)
                  << piece.in_window_frames << std::setw(10)
                  << piece.moved_in_window << piece.moved_out_of_window
                  << std::right << "\n";
    }
    // The scheduler's copy of held input is what `01:D466` masks against, so
    // this line is the ground truth for which key can work which flipper.
    std::cout << "\npressed -> $2CEE:";
    for (const auto& [pressed, copied] : probe_input_map) {
        std::cout << ' ' << std::format("{:02X}", pressed) << "->"
                  << std::format("{:02X}", copied);
    }
    std::cout << "\n";
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
