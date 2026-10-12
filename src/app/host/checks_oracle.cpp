// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks_oracle.hpp"

#include "cabinet_table.hpp"
#include "command_line.hpp"
#include "high_score_autosave.hpp"
#include "host_files.hpp"
#include "host_textures.hpp"
#include "play_assists.hpp"
#include "recording_inputs.hpp"

namespace devils_crush::app::host {

namespace {

// Somewhere for --high-score-check to write, so it can prove the save half
// without ever touching the table a player has actually earned.
const std::filesystem::path kHighScoreCheckPath =
    std::filesystem::path("high-score-check.hiscore");

} // namespace

// Holds one physical key through the launch and reports what the ball did,
// applying the same rule the host applies: while `$53` bit 3 marks the Devil
// Shooter session the key sends button I, and otherwise it sends its ordinary
// pad input.
// Measures both halves of the automatic high-score table without needing a
// game over: that a boot leaves the cartridge's own defaults in the block, and
// that the `01:C10E` hook can put a stored table there instead while leaving
// the two live player columns exactly as the boot left them.
[[nodiscard]] int run_high_score_check(
    devils_crush::emulation::PceRuntime& runtime)
{
    using devils_crush::core::HighScoreTable;
    using devils_crush::core::kHighScoreRankCount;
    using devils_crush::core::kHighScoreRowOffsets;

    // `01:C107` runs partway through the first frame; a handful more costs
    // nothing and keeps the check from depending on where exactly.
    constexpr int kBootFrames = 8;

    const auto boot = [&runtime](HighScoreAutosave* const autosave)
        -> std::vector<std::byte> {
        runtime.clear_execution_hooks();
        runtime.reset();
        if (autosave != nullptr
            && !install_high_score_restore(runtime, *autosave)) {
            return {};
        }
        for (int frame = 0; frame < kBootFrames; ++frame) {
            static_cast<void>(runtime.run_frame({}));
        }
        return runtime.export_work_ram();
    };
    const auto report = [](const std::string_view label,
                           const HighScoreTable& table) {
        std::cout << label << '\n';
        for (std::size_t rank = 0; rank < kHighScoreRankCount; ++rank) {
            std::cout
                << "  " << rank + 1U << "  " << std::setw(10)
                << static_cast<std::uint64_t>(
                       devils_crush::core::high_score_at(table, rank))
                    * 100U
                << "  ";
            for (std::size_t row = 3U; row < 6U; ++row) {
                const auto glyph =
                    table.bytes[row * kHighScoreRankCount + rank];
                std::cout
                    << (glyph >= 0x20U && glyph < 0x7FU
                            ? static_cast<char>(glyph)
                            : '.');
            }
            std::cout << '\n';
        }
    };
    // The two columns of every row that hold the players' live values rather
    // than a ranked entry. Restoring must not disturb one of them.
    const auto live_bytes = [](const std::span<const std::byte> work_ram) {
        std::vector<std::uint8_t> values;
        for (const auto row : kHighScoreRowOffsets) {
            values.push_back(std::to_integer<std::uint8_t>(
                work_ram[row + kHighScoreRankCount]));
            values.push_back(std::to_integer<std::uint8_t>(
                work_ram[row + kHighScoreRankCount + 1U]));
        }
        return values;
    };

    const auto plain = boot(nullptr);
    const auto plain_table = devils_crush::core::read_high_score_table(plain);
    if (!plain_table) {
        std::cerr << "The boot pass produced no work RAM.\n";
        return EXIT_FAILURE;
    }
    report("cartridge defaults after a boot:", *plain_table);

    HighScoreTable stored;
    for (std::size_t rank = 0; rank < kHighScoreRankCount; ++rank) {
        // Descending, the way the sort at `09:6B6F` leaves the table, and
        // distinct from the defaults in every one of the thirty bytes.
        const auto units = static_cast<std::uint32_t>(
            0x123456U - rank * 0x011111U);
        stored.bytes[rank] = static_cast<std::uint8_t>(units & 0xFFU);
        stored.bytes[kHighScoreRankCount + rank] =
            static_cast<std::uint8_t>((units >> 8U) & 0xFFU);
        stored.bytes[2U * kHighScoreRankCount + rank] =
            static_cast<std::uint8_t>((units >> 16U) & 0xFFU);
        for (std::size_t row = 3U; row < 6U; ++row) {
            stored.bytes[row * kHighScoreRankCount + rank] =
                static_cast<std::uint8_t>('A' + rank * 3U + (row - 3U));
        }
    }
    // Through the file format as well, so the check covers what a real run
    // would have read off disk rather than an in-memory shortcut.
    const auto encoded = devils_crush::core::encode_high_score_table(stored);
    const auto decoded = devils_crush::core::decode_high_score_table(encoded);
    if (!decoded || *decoded != stored) {
        std::cerr << "The high-score file format did not round trip.\n";
        return EXIT_FAILURE;
    }

    HighScoreAutosave autosave;
    autosave.path = kHighScoreCheckPath;
    autosave.stored = *decoded;
    const auto restored = boot(&autosave);
    runtime.clear_execution_hooks();
    const auto restored_table =
        devils_crush::core::read_high_score_table(restored);
    if (!restored_table) {
        std::cerr << "The restore pass produced no work RAM.\n";
        return EXIT_FAILURE;
    }
    report("stored table after a boot:", *restored_table);

    // The save half, driven the way a session drives it: the table changes
    // under the poll, and the poll waits for it to settle before writing.
    std::error_code remove_error;
    static_cast<void>(
        std::filesystem::remove(kHighScoreCheckPath, remove_error));
    auto earned = stored;
    earned.bytes[0] = static_cast<std::uint8_t>(earned.bytes[0] ^ 0xFFU);
    earned.bytes[3U * kHighScoreRankCount] = static_cast<std::uint8_t>('Z');
    auto earned_work_ram = restored;
    if (!devils_crush::core::write_high_score_table(earned_work_ram, earned)
        || !runtime.import_work_ram(earned_work_ram)) {
        std::cerr << "The earned table could not be staged.\n";
        return EXIT_FAILURE;
    }
    int wrote_at = 0;
    for (int frame = 30; frame <= 120 && wrote_at == 0; frame += 30) {
        poll_high_scores(runtime, autosave, frame);
        if (std::filesystem::exists(kHighScoreCheckPath)) {
            wrote_at = frame;
        }
    }
    // Reopened the way the next run opens it, rather than decoded directly,
    // so the check covers the startup path as well as the write.
    const auto reopened = open_high_score_autosave(kHighScoreCheckPath);
    const auto& written_table = reopened.stored;
    remove_error.clear();
    static_cast<void>(
        std::filesystem::remove(kHighScoreCheckPath, remove_error));

    const bool hook_ran = autosave.table_seen;
    const bool table_restored = *restored_table == stored;
    const bool live_untouched = live_bytes(plain) == live_bytes(restored);
    const bool no_write_pending = !autosave.pending;
    // Two samples of no further change, so a name being scrolled through
    // costs one write rather than one per letter.
    const bool settled_before_writing = wrote_at == 90;
    const bool table_saved = written_table && *written_table == earned;
    std::cout << "  boot hook entered:        "
              << (hook_ran ? "yes" : "no") << '\n'
              << "  stored table installed:   "
              << (table_restored ? "yes" : "no") << '\n'
              << "  live columns unchanged:   "
              << (live_untouched ? "yes" : "no") << '\n'
              << "  restore left nothing due: "
              << (no_write_pending ? "yes" : "no") << '\n'
              << "  changed table written at: frame " << wrote_at
              << (settled_before_writing ? " (settled)" : " (unexpected)")
              << '\n'
              << "  written table matches:    "
              << (table_saved ? "yes" : "no") << '\n';
    return hook_ran && table_restored && live_untouched && no_write_pending
            && settled_before_writing && table_saved
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

[[nodiscard]] int run_freeze_census(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path)
{
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    const bool from_state = !state_path.empty();
    if (from_state && !load_quick_state(runtime, state_path)) {
        return EXIT_FAILURE;
    }

    struct SlotRecord {
        std::set<std::uint32_t> on_camera_art;
        std::set<std::uint32_t> off_camera_art;
        std::set<std::uint32_t> off_camera_wanted;
        std::size_t on_camera_frames = 0;
        std::size_t off_camera_frames = 0;
        std::size_t on_camera_phase_changes = 0;
        std::size_t off_camera_phase_changes = 0;
        std::size_t off_camera_wanted_changes = 0;
        std::size_t on_camera_wanted_changes = 0;
        std::size_t off_camera_missed_changes = 0;
        int first_off_camera_art_frame = 0;
        int first_on_camera_frame = 0;
        std::uint32_t last_wanted = 0;
        std::uint32_t last_art = 0;
        bool have_wanted = false;
        std::uint8_t last_phase = 0;
        bool have_phase = false;
    };
    std::array<SlotRecord, devils_crush::cabinet::kActorSlotCount> slots;

    devils_crush::render::PersistentActorSpriteCache actors;
    devils_crush::render::TablePaletteCache palette;
    const int frame_count = from_state ? 600 : 2100;
    for (int frame_number = 1; frame_number <= frame_count; ++frame_number) {
        // Working the flippers is the whole point when replaying a state:
        // their artwork is driven by being flipped, not by the ball, so a
        // hands-off replay never asks them to change and cannot show whether
        // the ones away from the ball follow.
        InputFrame replay_input{};
        if (from_state && (frame_number / 20) % 2 == 0) {
            replay_input.player_one = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(Button::one)
                | static_cast<std::uint16_t>(Button::two));
        }
        keep_fixed_actors_simulating(runtime);
        static_cast<void>(runtime.run_frame(
            from_state
                ? replay_input
                : state_census_input_for_frame(frame_number)));
        const auto work_ram = runtime.export_work_ram();
        const auto state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        const auto video = runtime.export_video_memory();
        if (!state || !video
            || state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        devils_crush::render::PersistentActorOverlayStats frame_stats{};
        const auto table = decode_cabinet_table(
            *rom, work_ram, *video, actors, palette,
            CabinetLayers::all, false, nullptr, &frame_stats);
        if (!table) {
            continue;
        }
        for (std::size_t slot = 0;
             slot < devils_crush::cabinet::kActorSlotCount;
             ++slot) {
            if (frame_stats.actor_occupied[slot] == 0U) {
                slots[slot].have_phase = false;
                continue;
            }
            auto& record = slots[slot];
            const bool on_camera = frame_stats.actor_on_camera[slot] != 0U;
            const auto phase = frame_stats.actor_phase[slot];
            const auto art = frame_stats.actor_visual_hash[slot];
            if (on_camera && record.first_on_camera_frame == 0) {
                record.first_on_camera_frame = frame_number;
            }
            if (!on_camera && art != 0U
                && record.first_off_camera_art_frame == 0) {
                record.first_off_camera_art_frame = frame_number;
            }
            // Split the ROM's own activity by camera. If a slot's descriptors
            // change constantly on camera and not at all off it, the game
            // logic stopped simulating it -- the renderer is then faithfully
            // drawing a state that itself froze, and no rendering fix applies.
            const auto wanted_now = frame_stats.actor_metadata_hash[slot];
            if (record.have_wanted && wanted_now != record.last_wanted) {
                if (on_camera) {
                    ++record.on_camera_wanted_changes;
                } else {
                    ++record.off_camera_wanted_changes;
                }
            }
            if (on_camera) {
                ++record.on_camera_frames;
                if (art != 0U) {
                    record.on_camera_art.insert(art);
                }
            } else {
                ++record.off_camera_frames;
                if (art != 0U) {
                    record.off_camera_art.insert(art);
                }
                if (wanted_now != 0U) {
                    record.off_camera_wanted.insert(wanted_now);
                }
                // Distinct-look counts cannot see lag: artwork that follows
                // late still visits every value. Every frame the ROM changes
                // what it asks for, the drawn artwork must change with it.
                const auto wanted = wanted_now;
                if (record.have_wanted && wanted != record.last_wanted) {
                    if (art == record.last_art) {
                        ++record.off_camera_missed_changes;
                    }
                }
            }
            record.last_wanted = wanted_now;
            record.have_wanted = true;
            record.last_art = art;
            if (record.have_phase && phase != record.last_phase) {
                if (on_camera) {
                    ++record.on_camera_phase_changes;
                } else {
                    ++record.off_camera_phase_changes;
                }
            }
            record.last_phase = phase;
            record.have_phase = true;
        }
    }

    std::cout << "slot  on-frames  on-chg/100  off-frames  off-chg/100  missed  "
                 "verdict\n";
    std::size_t frozen = 0;
    for (std::size_t slot = 0;
         slot < devils_crush::cabinet::kActorSlotCount;
         ++slot) {
        const auto& record = slots[slot];
        // The ROM asked for several different looks while the actor was off
        // camera. Phase is not the test: whatever field drove the change, the
        // descriptors moved and the drawn artwork should have followed.
        const bool flipper =
            slot >= devils_crush::cabinet::kFirstFlipperControlSlot
            && slot <= devils_crush::cabinet::kLastFlipperControlSlot;
        if (!flipper && record.off_camera_wanted_changes < 2U) {
            continue;
        }
        const bool is_frozen = record.off_camera_missed_changes > 0U;
        if (is_frozen) {
            ++frozen;
        }
        std::cout << "  " << std::hex << std::setw(2) << std::setfill('0')
                  << slot << std::dec << std::setfill(' ') << std::setw(11)
                  << record.on_camera_frames << std::setw(15)
                  << (record.on_camera_frames == 0U
                          ? 0.0
                          : 100.0
                              * static_cast<double>(
                                    record.on_camera_wanted_changes)
                              / static_cast<double>(record.on_camera_frames))
                  << std::setw(12) << record.off_camera_frames
                  << std::setw(13)
                  << (record.off_camera_frames == 0U
                          ? 0.0
                          : 100.0
                              * static_cast<double>(
                                    record.off_camera_wanted_changes)
                              / static_cast<double>(record.off_camera_frames))
                  << std::setw(8) << record.off_camera_missed_changes
                  << "  "
                  << (is_frozen ? "FROZEN" : "animates") << "\n";
    }
    std::cout << "\n" << frozen
              << " slots draw fewer distinct looks off camera than the ROM "
                 "asked for\n";
    // Static skull targets do not appear in the animation-change ranking.
    // Report their first rendered appearance as well as the table portals.
    for (const auto slot : {0x10U, 0x11U, 0x12U, 0x2AU, 0x2BU,
                            0x3EU, 0x3FU, 0x42U, 0x43U,
                            0x46U, 0x47U, 0x49U, 0x4AU}) {
        const auto& record = slots[slot];
        std::cout << std::format(
            "Portal/skull slot {:02X}: first off-camera artwork frame {}; "
            "first camera visit {}\n",
            slot, record.first_off_camera_art_frame,
            record.first_on_camera_frame == 0
                ? std::string("never")
                : std::to_string(record.first_on_camera_frame));
    }
    return EXIT_SUCCESS;
}

// Measures the native reconstruction against the emulator itself.
//
// The cabinet composes the whole table, but only the 208 rows under the camera
// were ever drawn by hardware. Cropping the composition to that window makes
// the two directly comparable, so Beetle's own framebuffer becomes continuous
// ground truth for the reconstruction: every disagreement is a pixel the
// native path gets wrong. This is the check that has to exist before the
// retained and live layers can be collapsed into one, because it answers
// "is the reconstruction right?" without anyone having to look at a screen.
//
// Beetle reaches ARGB through RGB565 while the decoder scales the VCE's three
// bits directly, so both sides are quantized back to those three bits rather
// than compared as ARGB.
[[nodiscard]] int run_cabinet_oracle(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path)
{
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    const auto quantize = [](const std::uint32_t argb) {
        const auto channel = [&](const unsigned shift) {
            return ((argb >> shift) & 0xFFU) * 7U + 127U;
        };
        return std::tuple{
            channel(16U) / 255U, channel(8U) / 255U, channel(0U) / 255U};
    };

    struct Pass {
        CabinetLayers layers;
        const char* name;
    };
    // "all" measures the architecture as it stands; "retained" measures the
    // native reconstruction alone, with the live SAT withheld. The second is
    // the number that has to reach the first before the layers can collapse.
    static constexpr std::array oracle_passes{
        Pass{CabinetLayers::all, "background + retained + live SAT"},
        Pass{CabinetLayers::retained_only, "background + retained only"},
    };
    for (const auto& oracle_pass : oracle_passes) {
    runtime.reset();
    devils_crush::render::PersistentActorSpriteCache actors;
    devils_crush::render::TablePaletteCache palette;
    std::uint64_t compared_frames = 0;
    std::uint64_t decode_nanoseconds = 0;
    std::uint64_t decoded_frames = 0;
    devils_crush::render::PersistentActorOverlayStats freeze_totals{};
    std::uint64_t compared_pixels = 0;
    std::uint64_t mismatched_pixels = 0;
    std::size_t worst_frame_mismatch = 0;
    int worst_frame = 0;
    std::optional<int> previous_viewport;

    static constexpr int last_frame = 2100;
    for (int frame_number = 1; frame_number <= last_frame; ++frame_number) {
        // Measure the reconstruction under the same cull the cabinet runs
        // under. Lifting it puts ~90 more actors on the scheduler every frame,
        // and if that thrashed the pattern VRAM the two sides would part
        // company here: the native layer resolves artwork from ROM
        // descriptors while the emulator draws from live VRAM.
        keep_fixed_actors_simulating(runtime);
        const auto frame =
            runtime.run_frame(state_census_input_for_frame(frame_number));
        const auto work_ram = runtime.export_work_ram();
        const auto state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        const auto video = runtime.export_video_memory();
        if (!state || !video
            || state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        const auto decode_started = std::chrono::steady_clock::now();
        devils_crush::render::PersistentActorOverlayStats frame_stats{};
        const auto table = decode_cabinet_table(
            *rom, work_ram, *video, actors, palette,
            oracle_pass.layers, false, nullptr, &frame_stats);
        freeze_totals.missing_phase_variants +=
            frame_stats.missing_phase_variants;
        freeze_totals.static_phase_fallbacks +=
            frame_stats.static_phase_fallbacks;
        freeze_totals.stale_unusable_patterns +=
            frame_stats.stale_unusable_patterns;
        freeze_totals.stale_no_descriptor += frame_stats.stale_no_descriptor;
        freeze_totals.stale_resolve_failed += frame_stats.stale_resolve_failed;
        freeze_totals.restored_phase_variants +=
            frame_stats.restored_phase_variants;
        decode_nanoseconds += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - decode_started)
                .count());
        ++decoded_frames;
        if (!table || frame.framebuffer.width < 256
            || frame.framebuffer.height < 238) {
            continue;
        }
        const auto camera_y = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(work_ram[0x0BU]))
            | (static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(work_ram[0x0CU]))
               << 8U));
        const int work_ram_viewport = 480 + static_cast<int>(camera_y);
        // Work RAM is exported after the frame's logic, so its camera is the
        // one the *next* frame will use. The framebuffer just produced was
        // drawn with the previous value, and that is the camera the crop has
        // to use to line the two up.
        // The VDC scans the background from BYR, which settles 32 rows below
        // the table-world viewport, so that register -- not work RAM -- says
        // which rows hardware actually put on screen. Work RAM is exported
        // after the frame's logic and so already holds the *next* frame's
        // camera; cropping with it misaligns every scrolling frame by a full
        // frame of camera motion, which alone accounted for 12.1% disagreement
        // against 3.3% here. The register wraps at 256, so work RAM still
        // supplies the absolute region.
        auto viewport_top_row =
            static_cast<int>(video->background_y_scroll) - 32;
        while (viewport_top_row + 128 < work_ram_viewport) {
            viewport_top_row += 256;
        }
        while (viewport_top_row - 128 > work_ram_viewport) {
            viewport_top_row -= 256;
        }

        std::size_t frame_mismatch = 0;
        std::size_t frame_compared = 0;
        for (int framebuffer_y = 30; framebuffer_y < 238; ++framebuffer_y) {
            const int table_row = viewport_top_row + framebuffer_y - 30
                - devils_crush::cabinet::kTableWorldTop;
            if (table_row < 0 || table_row >= table->image.height) {
                continue;
            }
            for (int x = 0; x < 256; ++x) {
                const auto native = table->image.pixels[
                    static_cast<std::size_t>(table_row)
                        * static_cast<std::size_t>(table->image.width)
                    + static_cast<std::size_t>(x)];
                const auto emulated = frame.framebuffer.pixels[
                    static_cast<std::size_t>(framebuffer_y)
                        * static_cast<std::size_t>(
                            frame.framebuffer.pitch_pixels)
                    + static_cast<std::size_t>(x)];
                ++frame_compared;
                if (quantize(native) != quantize(emulated)) {
                    ++frame_mismatch;
                }
            }
        }
        if (frame_compared == 0U) {
            continue;
        }
        ++compared_frames;
        compared_pixels += frame_compared;
        mismatched_pixels += frame_mismatch;
        if (frame_mismatch > worst_frame_mismatch) {
            worst_frame_mismatch = frame_mismatch;
            worst_frame = frame_number;
        }
        // A frame that disagrees almost everywhere is not sprite error, it is
        // the crop landing on the wrong rows. Sweeping the row offset says so
        // outright: if some shift collapses the mismatch, the camera the
        // composition used is wrong by exactly that many rows.
        if (frame_mismatch * 4U > frame_compared) {
            int best_shift = 0;
            std::size_t best_mismatch = frame_mismatch;
            for (int shift = -24; shift <= 24; ++shift) {
                if (shift == 0) {
                    continue;
                }
                std::size_t mismatch = 0;
                for (int framebuffer_y = 30; framebuffer_y < 238;
                     ++framebuffer_y) {
                    const int table_row = viewport_top_row + framebuffer_y
                        - 30 - devils_crush::cabinet::kTableWorldTop + shift;
                    if (table_row < 0
                        || table_row >= table->image.height) {
                        continue;
                    }
                    for (int x = 0; x < 256; ++x) {
                        if (quantize(table->image.pixels[
                                static_cast<std::size_t>(table_row)
                                    * static_cast<std::size_t>(
                                        table->image.width)
                                + static_cast<std::size_t>(x)])
                            != quantize(frame.framebuffer.pixels[
                                static_cast<std::size_t>(framebuffer_y)
                                    * static_cast<std::size_t>(
                                        frame.framebuffer.pitch_pixels)
                                + static_cast<std::size_t>(x)])) {
                            ++mismatch;
                        }
                    }
                }
                if (mismatch < best_mismatch) {
                    best_mismatch = mismatch;
                    best_shift = shift;
                }
            }
            std::cout << "frame " << frame_number << " camera "
                      << viewport_top_row << ": " << frame_mismatch
                      << " mismatched, best shift " << best_shift << " -> "
                      << best_mismatch << '\n';
        } else if (frame_number % 200 == 0) {
            std::cout << "frame " << frame_number << ": "
                      << frame_mismatch << " / " << frame_compared
                      << " mismatched\n";
        }
    }

    std::cout << "\n[" << oracle_pass.name << "] over " << compared_frames
              << " frames: " << mismatched_pixels << " / " << compared_pixels
              << " pixels disagree ("
              << (compared_pixels == 0U
                      ? 0.0
                      : 100.0 * static_cast<double>(mismatched_pixels)
                          / static_cast<double>(compared_pixels))
              << "%)\nworst frame " << worst_frame << " with "
              << worst_frame_mismatch << " mismatched pixels\n"
              << "compose cost: "
              << (decoded_frames == 0U
                      ? 0.0
                      : static_cast<double>(decode_nanoseconds)
                          / static_cast<double>(decoded_frames) / 1.0e6)
              << " ms/frame against a 16.7 ms budget\n"
              << "off-camera artwork: missing "
              << freeze_totals.missing_phase_variants
              << ", static-fallback " << freeze_totals.static_phase_fallbacks
              << ", unusable-patterns "
              << freeze_totals.stale_unusable_patterns
              << ", no-descriptor " << freeze_totals.stale_no_descriptor
              << ", resolve-failed " << freeze_totals.stale_resolve_failed
              << ", restored " << freeze_totals.restored_phase_variants
              << '\n';
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int dump_cabinet_route(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& directory)
{
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "Could not create dump directory: "
                  << error.message() << '\n';
        return EXIT_FAILURE;
    }

    static constexpr std::array dump_frames{
        1256, 1257, 1258, 1500, 1800, 2100};
    static constexpr int last_frame = 2100;
    struct Pass {
        CabinetLayers layers;
        const char* name;
    };
    static constexpr std::array passes{
        Pass{CabinetLayers::all, "all"},
        Pass{CabinetLayers::retained_only, "retained"},
        Pass{CabinetLayers::sat_only, "sat"},
        Pass{CabinetLayers::background_only, "background"},
    };

    for (const auto& pass : passes) {
        runtime.reset();
        devils_crush::render::PersistentActorSpriteCache actors;
        devils_crush::render::TablePaletteCache palette;
        for (int frame_number = 1;
             frame_number <= last_frame;
             ++frame_number) {
            static_cast<void>(runtime.run_frame(
                state_census_input_for_frame(frame_number)));
            const auto work_ram = runtime.export_work_ram();
            const auto state =
                devils_crush::cabinet::decode_cabinet_state(work_ram);
            if (!state
                || state->playfield
                    != devils_crush::cabinet::PlayfieldView::
                           table_background) {
                continue;
            }
            const auto video = runtime.export_video_memory();
            if (!video) {
                continue;
            }
            // The caches learn across every decoded frame, so the decode
            // must run on all of them and not only the captured ones.
            const auto table = decode_cabinet_table(
                *rom,
                work_ram,
                *video,
                actors,
                palette,
                pass.layers,
                false,
                nullptr);
            if (!table
                || std::ranges::find(dump_frames, frame_number)
                    == dump_frames.end()) {
                continue;
            }
            std::ostringstream name;
            name << "route-" << std::setw(4) << std::setfill('0')
                 << frame_number << '-' << pass.name << ".bmp";
            const auto path = directory / name.str();
            if (!save_playfield_png(table->image, path)) {
                return EXIT_FAILURE;
            }
            std::cout << "Captured " << path.string() << '\n';
        }
    }
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
