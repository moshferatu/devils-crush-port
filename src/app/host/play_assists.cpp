// SPDX-License-Identifier: GPL-3.0-or-later
#include "play_assists.hpp"

#include "native_registry_install.hpp"
#include "recording_inputs.hpp"

namespace devils_crush::app::host {

namespace {

// The ball's actor position that puts the middle of its sprite on a pixel of
// the playfield's picture: the stitched table's, or the cartridge's own
// screen, whose sprites `08:40DF` draws at their Y less the camera anchor in
// `$A9:$AA`.
[[nodiscard]] std::optional<SDL_FPoint> ball_position_at_picture_point(
    const std::span<const std::byte> rom,
    const std::span<const std::byte> work_ram,
    const SDL_FPoint point,
    const bool stitched_table)
{
    if (work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    const auto byte_at = [&work_ram](const std::size_t offset) {
        return std::to_integer<int>(work_ram[offset]);
    };
    int offset_x = kBallSpriteOffsetX;
    int offset_y = kBallSpriteOffsetY;
    if (const auto sprites =
            devils_crush::render::decode_ball_and_flipper_sprites(
                rom, work_ram)) {
        for (const auto& sprite : *sprites) {
            if (sprite.actor_slot == devils_crush::cabinet::kBallActorSlot) {
                offset_x = sprite.world_x
                    - static_cast<std::int16_t>(
                        byte_at(0x1180U) | (byte_at(0x1200U) << 8));
                offset_y = sprite.world_y
                    - static_cast<std::int16_t>(
                        byte_at(0x1300U) | (byte_at(0x1380U) << 8));
                break;
            }
        }
    }
    const int top = stitched_table
        ? kStitchedTableSpriteTop
        : static_cast<std::int16_t>(byte_at(0xA9U) | (byte_at(0xAAU) << 8));
    return SDL_FPoint{
        point.x - static_cast<float>(kBallSpriteHalf + offset_x),
        point.y - static_cast<float>(kBallSpriteHalf + offset_y - top),
    };
}

// Gives the flipper buttons back while the ball is still in the Devil Shooter.
//
// The actor scheduler publishes the held pad byte to `$2CEE` once a frame and
// blanks it whenever the shooter owns the ball:
//
// ```
// 01:C4F1  LDA $2CC7        tilt lock
// 01:C4F4  BNE $C4FC
// 01:C4F6  LDA <$53
// 01:C4F8  AND #$08         the Devil Shooter session
// 01:C4FA  BEQ $C4FD
// 01:C4FC  CLY
// 01:C4FD  STY $2CEE
// ```
//
// `$2CEE` is read at exactly four addresses in all 384 KB -- `01:D466`,
// `01:D49B`, `01:D4AA` and `01:D4D4` -- and every one of them masks it with
// `$3B80,X` immediately. It is the control pieces' input byte and nothing
// else's, so putting bits back into it can reach nothing but them.
//
// Only the bits the two flipper keys send are put back, and only while a table
// session is running. Everything else the blanking covers -- Run, Select, the
// launch button -- stays blanked, so the shooter still charges on a held
// button and nothing outside the flippers sees an input it did not before.
//
// The masks are the host's, not the cartridge's, because they are the ones
// that describe keys: `apply_flipper_inputs` has already written them into
// `$3B80,X`. On a configuration that leaves the cartridge masks alone the
// right-hand pieces carry `$01`, which is Button I, which is also the launch
// button -- so there the right flipper rises while the shooter charges. The
// default bindings send neither flipper a Button I, exactly so that a flipper
// key can never confirm anything, and they do not have that overlap.
void restore_flipper_input(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::cabinet::FlipperInputs& flippers)
{
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::uint8_t kShooterSession = 0x08U;
    constexpr std::size_t kHeldInputBase = 0x0013U;
    constexpr std::size_t kActivePlayer = 0x0251U;
    constexpr std::size_t kPublishedInput = 0x0CEEU;

    const auto left = devils_crush::cabinet::flipper_input_mask(
        flippers.left);
    const auto right = devils_crush::cabinet::flipper_input_mask(
        flippers.right);
    const auto masks = static_cast<std::uint8_t>(
        (left != 0U ? left : devils_crush::cabinet::kCartridgeLeftFlipperMask)
        | (right != 0U ? right
                       : devils_crush::cabinet::kCartridgeRightFlipperMask));

    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() < 0x2000U) {
        return;
    }
    const auto byte_at = [&work_ram](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset]);
    };
    if ((byte_at(kSessionFlags) & kShooterSession) == 0U) {
        return;
    }
    // Slots `$04-$09` only hold flipper pieces during a table session, and
    // outside one `$2CEE` is whoever the current screen put in those slots.
    const auto state =
        devils_crush::cabinet::decode_cabinet_state(work_ram);
    if (!state || state->balls_in_session == 0U) {
        return;
    }

    const auto held =
        byte_at(kHeldInputBase + (byte_at(kActivePlayer) & 0x07U));
    const auto restored = static_cast<std::uint8_t>(
        byte_at(kPublishedInput) | (held & masks));
    if (restored == byte_at(kPublishedInput)) {
        return;
    }
    work_ram[kPublishedInput] = static_cast<std::byte>(restored);
    static_cast<void>(runtime.import_work_ram(work_ram));
}

} // namespace

[[nodiscard]] bool prepare_bonus_stage_launch(
    PceRuntime& runtime,
    const std::uint8_t stage)
{
    if (stage < 1U || stage > 6U) {
        return false;
    }

    const auto initial_frame_number = runtime.serialize().frame_number;
    for (int frame_number = 1;
         frame_number < kBonusStageLaunchFrame;
         ++frame_number) {
        const auto frame = runtime.run_frame(
            state_census_input_for_frame(frame_number));
        if (frame.frame_number
                != initial_frame_number
                    + static_cast<std::uint64_t>(frame_number)
            || frame.framebuffer.pixels.empty()) {
            std::cerr << "Could not prepare bonus-stage launch on frame "
                      << frame_number << ".\n";
            return false;
        }
    }

    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() != runtime.work_ram_size()
        || work_ram.size() <= kBonusStageIdOffset) {
        std::cerr << "Could not access work RAM for bonus-stage launch.\n";
        return false;
    }
    const auto session_flags = std::to_integer<std::uint8_t>(
        work_ram[kBonusStageSessionFlagsOffset]);
    if ((session_flags & kBonusStageLiveSessionBit) == 0U) {
        std::cerr << "Could not reach a live session for bonus-stage launch.\n";
        return false;
    }
    work_ram[kBonusStageIdOffset] = static_cast<std::byte>(stage);
    work_ram[kBonusStageSessionFlagsOffset] |=
        static_cast<std::byte>(kBonusStageRequestBit);
    if (!runtime.import_work_ram(work_ram)) {
        std::cerr << "Could not request bonus stage "
                  << static_cast<unsigned int>(stage) << ".\n";
        return false;
    }

    const auto frame = runtime.run_frame(
        state_census_input_for_frame(kBonusStageLaunchFrame));
    const auto launched_work_ram = runtime.export_work_ram();
    if (frame.frame_number
            != initial_frame_number
                + static_cast<std::uint64_t>(kBonusStageLaunchFrame)
        || frame.framebuffer.pixels.empty()
        || launched_work_ram.size() <= kBonusStageIdOffset
        || std::to_integer<std::uint8_t>(
               launched_work_ram[kBonusStageIdOffset]) != stage
        || (std::to_integer<std::uint8_t>(
                launched_work_ram[kBonusStageSessionFlagsOffset])
            & (kBonusStageLiveSessionBit | kBonusStageRequestBit))
            != (kBonusStageLiveSessionBit | kBonusStageRequestBit)) {
        std::cerr << "Bonus stage " << static_cast<unsigned int>(stage)
                  << " did not enter its transition.\n";
        return false;
    }

    std::cout << "Bonus-stage launch ready: BS-"
              << static_cast<unsigned int>(stage) << ".\n";
    return true;
}

[[nodiscard]] bool service_synthetic_bonus_stage_return(
    PceRuntime& runtime,
    bool& return_pending)
{
    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() != runtime.work_ram_size()
        || work_ram.size() <= kBonusStageSessionFlagsOffset) {
        std::cerr << "Could not inspect work RAM for bonus-stage return.\n";
        return false;
    }

    const auto session_flags = std::to_integer<std::uint8_t>(
        work_ram[kBonusStageSessionFlagsOffset]);
    if ((session_flags & kBonusStageRequestBit) != 0U) {
        // Re-arm after an F9 restore to a quick state inside the bonus stage.
        return_pending = true;
    }
    if (!return_pending
        || !devils_crush::native_runtime::release_synthetic_bonus_return_ball(
            work_ram)) {
        return true;
    }
    if (!runtime.import_work_ram(work_ram)) {
        std::cerr << "Could not release the ball after bonus-stage return.\n";
        return false;
    }

    return_pending = false;
    return true;
}

// A non-null native_registry deploys every admitted routine into the live
// session and supplies the slice, exclusion, and census settings for it.
// Keeps the table's fixed mechanisms simulating outside the original camera.
//
// The ROM culls actors by camera band. `00:F4FC` sweeps every slot each frame
// and sets `$3080,X` bit 7 only inside [camera - 48, camera + 240); the actor
// scheduler then skips a slot whose bit 7 is clear while bit 1 is set. Bit 1
// is the opt-in to that cull -- a slot that clears it is dispatched every
// frame wherever it stands.
//
// On the original hardware one screen was all anyone could see, so the cull
// cost nothing. The cabinet shows all 768 rows at once and it becomes the
// dominant visual defect: of the three flipper pairs only the one under the
// camera answers the buttons, and the Front Spiders at `$26/$27` stand still.
// Measured over 600 frames of worked flippers, every phase change any of the
// six pieces made happened while its bit 7 was set, and they made one change
// between them while it was clear.
//
// Bit 1 is the right lever rather than bit 7 because bit 7 is also the gate on
// `08:4036`, the original SAT presentation path. Leaving it alone keeps the
// emulator queueing exactly the actors the hardware drew, so its own 64-entry
// sprite table cannot overflow and the framebuffer the oracle measures
// against stays the same picture.
//
// Only named slots are lifted, and deliberately so. The cull is not merely an
// optimization the ROM could have done without: actor state machines are
// written on the assumption that they stop while off camera, and running all
// 88 fixed mechanisms everywhere advances timers and appearances that were
// never meant to advance unseen. Lifting it wholesale left the middle-table
// circles and the giant face reading as lit from the start of a game until the
// camera reached them and the on-camera logic put them right.
//
// The flipper pieces are safe because their state machine is driven entirely
// by the button through `01:D466` and consults nothing else, and the spiders
// were the other family the whole-table view exposed as frozen. The upper
// Big Star ($4B) and Sorcerers ($16-$1E, including their controller) also
// run from the start so the upper playfield animates before a camera visit.
// The seven upper-left Gray Blobs ($50-$56) also keep moving off camera.
// Their original rotation, deployment, and regeneration logic still runs.
// The four Wall Pockets ($3E/$3F/$42/$43) and Little Skulls with their
// controllers ($44-$4A) likewise rotate, spawn, and regenerate before the
// ball reaches the upper table.
// The middle-table Side/Walled Star portals ($10-$12) and Giant-Face
// helmet openings ($2A/$2B) also keep their original idle rotation running.
// The middle-table Three
// Blobs at `$58-$5A` also need to keep floating when the ball leaves their
// region, but only after the original camera has activated them once. Check
// their actual Y against the camera too: initialization sets bit 7 even for
// off-camera slots, and latching that flag starts the orbs in the shooter.
// This preserves their start-of-session setup while making activation a latch:
// clearing bit 1 persists after the camera sweep later clears bit 7. The
// dynamic pool at `$62-$7E` and every other fixed mechanism keep their cull.
void keep_fixed_actors_simulating(
    devils_crush::emulation::PceRuntime& runtime)
{
    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() < 0x2000U) {
        return;
    }
    bool changed = false;
    const auto read_word = [&](const std::size_t low,
                               const std::size_t high) {
        return static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(work_ram[low])
            | (std::to_integer<std::uint8_t>(work_ram[high]) << 8U));
    };
    const auto camera_y = read_word(0x00A9U, 0x00AAU);
    for (std::size_t slot = devils_crush::cabinet::kFirstFlipperControlSlot;
         slot <= devils_crush::cabinet::kLastThreeBlobSlot;
         ++slot) {
        const std::size_t offset = 0x1080U + slot;
        const auto flags = std::to_integer<std::uint8_t>(work_ram[offset]);
        if (!devils_crush::cabinet::should_disable_actor_camera_cull(
                slot,
                flags,
                read_word(0x1300U + slot, 0x1380U + slot),
                camera_y)) {
            continue;
        }
        work_ram[offset] = static_cast<std::byte>(flags & 0xFDU);
        changed = true;
    }
    if (changed) {
        static_cast<void>(runtime.import_work_ram(work_ram));
    }
}

// Rewrites which pad inputs work each flipper.
//
// `01:D466` opens a piece's gate with `LDA $2CEE : AND $3B80,X`. Rewriting
// that mask is the surgical place to change which key works which flipper:
// filtering the input instead would have to go through `$2CEE`, which the
// scheduler shares with every other actor that reads held input, and would
// take the directions away from the menus too.
//
// The masks are written by the piece initializers at `01:D335`/`01:D376`
// rather than each frame, but reapplying costs one compare per piece and
// survives a re-init at any point, so it is done every frame.
// Returns whether a live-game ball is in the Devil Shooter, which the caller needs
// for the launch keys and which is free here because this already holds the
// work-RAM image.
[[nodiscard]] bool apply_flipper_inputs(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::cabinet::FlipperInputs& flippers)
{
    // Session flags $53 land at offset $0053: the HuC6280 zero page is mapped
    // at $2000 and work RAM is exported from there. Bit 3 is the Devil
    // Shooter session.
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::uint8_t kShooterSession = 0x08U;
    constexpr std::size_t kInputMaskBase = 0x1B80U;
    const auto left_mask =
        devils_crush::cabinet::flipper_input_mask(flippers.left);
    const auto right_mask =
        devils_crush::cabinet::flipper_input_mask(flippers.right);

    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() < 0x2000U) {
        return false;
    }
    // $2254 selects the real game (0) or attract demo (1). Both run the
    // shooter, but the demo's C2AF input gate needs Run to return to title.
    const bool ball_in_launcher =
        std::to_integer<std::uint8_t>(work_ram[0x0254U]) == 0U
        && (std::to_integer<std::uint8_t>(work_ram[kSessionFlags])
            & kShooterSession) != 0U;
    if (left_mask == 0U && right_mask == 0U) {
        return ball_in_launcher;
    }
    // `$3B80,X` is a general-purpose per-actor byte, not a dedicated flipper
    // field: `03:6697` and `03:68B1` keep counters there for other actors.
    // Slots `$04-$09` only hold flipper pieces during a table session, so
    // outside one they belong to whatever the current screen put in them --
    // measured, the title screen keeps a counter in slot `$05` -- and writing
    // through that corrupts it.
    const auto state =
        devils_crush::cabinet::decode_cabinet_state(work_ram);
    if (!state || state->balls_in_session == 0U) {
        return ball_in_launcher;
    }
    bool changed = false;
    const auto apply = [&](const std::size_t slot,
                           const std::uint8_t mask,
                           const std::uint8_t cartridge_mask) {
        const std::size_t offset = kInputMaskBase + slot;
        if (!devils_crush::cabinet::rewrites_flipper_input_mask(
                std::to_integer<std::uint8_t>(work_ram[offset]),
                cartridge_mask,
                mask)) {
            return;
        }
        work_ram[offset] = static_cast<std::byte>(mask);
        changed = true;
    };
    for (const auto slot : devils_crush::cabinet::kLeftFlipperSlots) {
        apply(
            slot,
            left_mask,
            devils_crush::cabinet::kCartridgeLeftFlipperMask);
    }
    for (const auto slot : devils_crush::cabinet::kRightFlipperSlots) {
        apply(
            slot,
            right_mask,
            devils_crush::cabinet::kCartridgeRightFlipperMask);
    }
    if (changed) {
        static_cast<void>(runtime.import_work_ram(work_ram));
    }
    return ball_in_launcher;
}

// Gives a ball the cheat was carrying back to the game.
void release_dragged_ball(devils_crush::emulation::PceRuntime& runtime)
{
    auto work_ram = runtime.export_work_ram();
    if (devils_crush::input::release_dragged_ball(work_ram)) {
        static_cast<void>(runtime.import_work_ram(work_ram));
    }
}

// The cabinet's one change to what the game shows: bonus stage 1's dragon
// heads fade in already facing the ball, where the game has them face right
// until the fade is over and then snap round.
void face_waiting_dragon_heads(devils_crush::emulation::PceRuntime& runtime)
{
    auto work_ram = runtime.export_work_ram();
    if (devils_crush::cabinet::face_waiting_dragon_heads(work_ram)) {
        static_cast<void>(runtime.import_work_ram(work_ram));
    }
}

// The ball-dragging cheat's frame: the ball is taken to the pointer, held
// inside the picture. Nothing is written, and false returned, unless there is
// a ball in play.
bool drag_ball_to_picture_point(
    devils_crush::emulation::PceRuntime& runtime,
    const std::span<const std::byte> rom,
    SDL_FPoint point,
    const bool stitched_table)
{
    auto work_ram = runtime.export_work_ram();
    if (!devils_crush::input::ball_can_be_dragged(work_ram)) {
        return false;
    }
    const float half = static_cast<float>(kBallSpriteHalf);
    const float width =
        static_cast<float>(devils_crush::cabinet::kTableWorldWidth);
    const float height = stitched_table
        ? static_cast<float>(devils_crush::cabinet::kTableWorldHeight)
        : 240.0F;
    point.x = std::clamp(point.x, half, width - half);
    point.y = std::clamp(point.y, half, height - half);
    const auto position =
        ball_position_at_picture_point(rom, work_ram, point, stitched_table);
    return position
        && devils_crush::input::drag_ball(work_ram, position->x, position->y)
        && runtime.import_work_ram(work_ram);
}

// Puts the Devil Shooter where the physical plunger is.
//
// The cartridge charges the shooter by moving its own piece. `02:52E3` adds
// `$0040` to the piece's twenty-four-bit vertical position every tick button I
// is held, and then derives everything else from where the piece ended up:
//
// ```
// 02:52FB  SEC
// 02:52FC  LDA $3300,X      the piece's whole-pixel Y
// 02:52FF  SBC $3E00,X      the rest position the initializer stored
// 02:5302  STA $2D15        the charge
// ```
//
// So `$2D15` is not a timer -- it is the distance the piece has been pulled
// down from its rest position, in pixels, and `02:51DA` turns it into the
// launch velocity as `-(($2D15 >> 2) + 2)`. The shooter piece *is* a plunger,
// and its draw is the only thing the launch is computed from.
//
// That is what makes an analog plunger possible at all. A held button charges
// a quarter of a pixel per tick, so the full `$27` takes 156 frames; a real
// rod is pulled and let go inside 60. Mapped to the button, the pull distance
// would reach the game as nothing whatever and every shot would come out at
// the bottom of the range. Mapped to the piece's position, the pull *is* the
// charge, and the cartridge computes the shot from it exactly as it always
// did -- `02:51DA` is untouched, and the host neither chooses the velocity nor
// decides when to fire.
//
// Three bytes are written, and they are the same three `02:52E3` writes: the
// fraction, and the two halves of the whole-pixel coordinate. `$2D15` is
// written with them because on the frame the rod is let go button I is up, so
// `02:52E3` does not run to recompute it, and `02:51DA` reads it that frame.
// The appearance at `$3980,X` is left alone: `02:52E3` sets it from `$2D15`
// on every frame the rod is still back, which is every frame but the last.
//
// **And the ball is carried with it, which the cartridge does not have to do
// and this does.** Nothing in the charge path places the ball: while the
// shooter descends the ball simply rests on it and falls after it, and that
// works because the cartridge descends a quarter of a pixel per tick.
// Measured on the census route, the ball tracks the shooter exactly nine
// pixels above through the whole 96-frame charge, 623 to 647 as the shooter
// goes 632 to 656. A rod moves several pixels a frame, gravity cannot keep
// up, and the shooter slides out from under a ball that stays where it was --
// which is what a player sees as the ball being left behind by the plunger.
//
// So the ball is placed with the shooter, and the rule is the cartridge's own
// rather than an invention: `02:52C7`-`02:52D4` is where the Devil Shooter
// puts the ball while it retracts, and it puts it at the shooter's coordinate
// less nine.
//
// ```
// 02:52C7  LDA $3300,X      the shooter's whole-pixel Y
// 02:52CA  SBC #$09
// 02:52CC  STA $3300        the ball's
// ```
//
// This is the same relationship in the phase where the cartridge leaves
// gravity to maintain it. It is only taken over while the ball is already
// holding it, so a ball that is not seated on the shooter is never moved.
//
// The slot is found rather than assumed. The Devil Shooter is fixed layout
// slot `$0E`, but the piece that charges is the live actor, and what makes it
// identifiable is its dispatch pointer: `02:5156 JSR $C018` installs `$5159`
// into `$3F00,X/$3F80,X`, so the charging shooter is the slot dispatching to
// `02:5159`. Requiring exactly one match, a shooter session at `$53` bit 3, a
// normal table at `$2254`, and a current draw already inside `$00`-`$27` is
// four conditions on a byte pair that is per-actor and general-purpose; any of
// them failing means this is not the shooter and nothing is written.
[[nodiscard]] bool apply_plunger_draw(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::input::PlungerSample& sample)
{
    constexpr std::size_t kSessionFlags = 0x0053U;
    constexpr std::uint8_t kShooterSession = 0x08U;
    constexpr std::size_t kTableMode = 0x0254U;
    constexpr std::size_t kChargeCounter = 0x0D15U;
    constexpr std::size_t kYFractionBase = 0x1280U;
    constexpr std::size_t kYLowBase = 0x1300U;
    constexpr std::size_t kYHighBase = 0x1380U;
    constexpr std::size_t kRestLowBase = 0x1E00U;
    constexpr std::size_t kRestHighBase = 0x1E80U;
    constexpr std::size_t kDispatchLowBase = 0x1F00U;
    constexpr std::size_t kDispatchHighBase = 0x1F80U;
    constexpr std::uint16_t kChargeDispatch = 0x5159U;
    constexpr std::uint8_t kFullCharge = 0x27U;

    auto work_ram = runtime.export_work_ram();
    if (work_ram.size() < 0x2000U) {
        return false;
    }
    const auto byte_at = [&work_ram](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset]);
    };
    if ((byte_at(kSessionFlags) & kShooterSession) == 0U
        || byte_at(kTableMode) != 0U) {
        return false;
    }

    std::size_t shooter = 0;
    std::size_t matches = 0;
    for (std::size_t slot = 0;
         slot < devils_crush::cabinet::kActorSlotCount;
         ++slot) {
        const auto dispatch = static_cast<std::uint16_t>(
            byte_at(kDispatchLowBase + slot)
            | (static_cast<std::uint16_t>(byte_at(kDispatchHighBase + slot))
               << 8U));
        if (dispatch == kChargeDispatch) {
            shooter = slot;
            ++matches;
        }
    }
    if (matches != 1U) {
        return false;
    }

    const auto rest_low = byte_at(kRestLowBase + shooter);
    const auto current =
        static_cast<std::uint8_t>(byte_at(kYLowBase + shooter) - rest_low);
    if (current > kFullCharge) {
        return false;
    }

    // Never zero while the rod is back. A charge of zero is how the cartridge
    // says there is nothing to launch -- `02:5173 LDA $2D15 : BEQ $5178`
    // returns without firing -- so writing it would leave the ball sitting in
    // a shooter that the player has already let go of.
    const auto position = sample.shooter_position();
    const auto charge = static_cast<std::uint8_t>(position >> 8U);
    const auto fraction = static_cast<std::uint8_t>(position & 0xFFU);
    const auto sum = static_cast<std::uint16_t>(rest_low + charge);
    const auto low = static_cast<std::uint8_t>(sum & 0xFFU);
    const auto high = static_cast<std::uint8_t>(
        byte_at(kRestHighBase + shooter) + (sum >> 8U));

    // The ball rides nine pixels above the shooter, per `02:52C7`-`02:52D4`.
    // Taken over only where the ball is holding that relationship *now*,
    // against where the shooter is before this moves it -- not against where
    // it is going. The distinction is the whole guard: "is this ball sitting
    // in the shooter" is a question about the present, and asking it about the
    // destination instead would refuse exactly the case that needs carrying,
    // a rod drawn far in one frame. A ball that is not seated -- caught
    // mid-flight, or somewhere else entirely -- is left to the cartridge.
    const auto shooter_y = static_cast<int>(low | (high << 8U));
    const auto shooter_was = static_cast<int>(
        byte_at(kYLowBase + shooter)
        | (byte_at(kYHighBase + shooter) << 8U));
    const auto ball_y = static_cast<int>(
        byte_at(kYLowBase) | (byte_at(kYHighBase) << 8U));
    const auto seated = shooter_y - kBallRidesAbove;
    const bool carries_ball =
        std::abs(ball_y - (shooter_was - kBallRidesAbove))
        <= kBallRideSlack;

    if (byte_at(kYFractionBase + shooter) == fraction
        && byte_at(kYLowBase + shooter) == low
        && byte_at(kYHighBase + shooter) == high
        && byte_at(kChargeCounter) == charge
        && (!carries_ball || (ball_y == seated
                             && byte_at(kYFractionBase) == fraction))) {
        return true;
    }
    work_ram[kYFractionBase + shooter] = static_cast<std::byte>(fraction);
    work_ram[kYLowBase + shooter] = static_cast<std::byte>(low);
    work_ram[kYHighBase + shooter] = static_cast<std::byte>(high);
    work_ram[kChargeCounter] = static_cast<std::byte>(charge);
    if (carries_ball) {
        work_ram[kYFractionBase] = static_cast<std::byte>(fraction);
        work_ram[kYLowBase] = static_cast<std::byte>(seated & 0xFF);
        work_ram[kYHighBase] =
            static_cast<std::byte>((seated >> 8) & 0xFF);
    }
    static_cast<void>(runtime.import_work_ram(work_ram));
    return true;
}

// The hook sits on `01:C500`, the instruction after the `STY $2CEE`, so it
// sees the byte the ROM just wrote and runs before the actor sweep at
// `01:C50D` reads it. The native C4DE prefix deliberately stops at this
// instruction. The replacement restores the host's flipper bits, then owns
// C500-C50E and returns at the registered C510 scan. `02:C020` is a second
// copy of the same code with the same gate; it is deliberately not hooked,
// because the 3,600-frame census route never executes it and an unmeasured
// second site is a claim rather than a change.
//
// `--cabinet-launch-flipper-probe` measures what this buys, and measures it
// with the registry deployed so that it is measuring what people play: on the
// census route's first launch the flippers answer two frames after the button
// is let go rather than 102, and keep answering for the whole 98-frame flight
// the shooter session used to own.
[[nodiscard]] bool install_shooter_flipper_gate(
    devils_crush::emulation::PceRuntime& runtime,
    const devils_crush::cabinet::FlipperInputs& flippers)
{
    return runtime.install_execution_hook({
        .routine = kShooterGateInstruction,
        .mode = HookMode::replace,
        .replacement_callback =
            [&runtime, &flippers](
                const RoutineEntry& entry,
                const std::span<const std::byte>) {
                restore_flipper_input(runtime, flippers);
                const auto work_ram = runtime.export_work_ram();
                return devils_crush::app::replace_native_gameplay_routine(
                    entry, work_ram, {});
            },
    });
}

[[nodiscard]] bool install_nudge_multiplier(PceRuntime& runtime, const double factor)
{
    if (factor == 1.0) return true;
    // C484 is the return AFTER the cartridge has applied its halved signed
    // impulse. Changing the completed motion word leaves the original input,
    // tilt, cooldown and vector-decay rules intact. No CPU register changes,
    // nor edits to the vector that subsequent nudge phases will decay.
    return runtime.install_execution_hook({
        .routine = {.physical_bank = 0x01U, .logical_pc = 0xC484U},
        .mode = HookMode::observe,
        .callback = [&runtime, factor](const RoutineEntry&) {
            auto ram = runtime.export_work_ram();
            if (!devils_crush::input::nudge_play_active(ram)
                || ram[0x1000] == std::byte{0}
                || (std::to_integer<unsigned>(ram[0x1C80]) & 1U) != 0) return;
            const auto word = [&ram](std::size_t lo, std::size_t hi) {
                return static_cast<std::uint16_t>(std::to_integer<unsigned>(ram[lo])
                    | (std::to_integer<unsigned>(ram[hi]) << 8U));
            };
            const auto motion = devils_crush::input::scale_nudge_motion(
                word(0x1500, 0x1580), word(0x2F, 0x30), factor);
            ram[0x1500] = static_cast<std::byte>(motion & 0xFFU);
            ram[0x1580] = static_cast<std::byte>(motion >> 8U);
            if (!runtime.import_work_ram(ram)) {
                throw std::runtime_error("Could not apply nudge multiplier.");
            }
        },
    });
}

} // namespace devils_crush::app::host
