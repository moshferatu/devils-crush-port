// SPDX-License-Identifier: GPL-3.0-or-later
#include "checks_cabinet.hpp"

#include "artwork_blend_checks.hpp"
#include "cabinet_table.hpp"
#include "command_line.hpp"
#include "host_files.hpp"
#include "host_textures.hpp"
#include "play_assists.hpp"
#include "recording_inputs.hpp"
#include "recording_plans.hpp"
#include "table_motion.hpp"

namespace devils_crush::app::host {

namespace {

// The route a --cabinet-view-probe pass drives.
enum class ViewProbeRoute {
    // Nothing is ever pressed, which is what a cabinet nobody has touched
    // does: title, attract demo, title again.
    attract,
    // The census route into a live table, and then the sequence a player
    // walks to look at the high scores -- Run to pause, Select to open them.
    pause_menu,
    // The 3,600-frame state-census route, unaltered. This is the route every
    // reconstruction measurement in the project is made against, so whatever
    // gates the table view has to leave all of its table frames alone.
    census,
};

[[nodiscard]] InputFrame view_probe_input(
    const ViewProbeRoute route,
    const int frame_number)
{
    constexpr int kPauseFrom = 1530;
    constexpr int kPauseUntil = 1545;
    constexpr int kSelectFrom = 1600;
    constexpr int kSelectUntil = 1615;

    if (route == ViewProbeRoute::attract) {
        return {};
    }
    if (route == ViewProbeRoute::census) {
        return state_census_input_for_frame(frame_number);
    }
    if (frame_number < kPauseFrom) {
        return state_census_input_for_frame(frame_number);
    }
    InputFrame input;
    if (frame_number < kPauseUntil) {
        input.player_one = static_cast<std::uint16_t>(Button::run);
    } else if (frame_number >= kSelectFrom && frame_number < kSelectUntil) {
        input.player_one = static_cast<std::uint16_t>(Button::select);
    }
    return input;
}

} // namespace

// Composes the cabinet playfield over the scripted state-census route and
// writes selected frames as PNG, once per layer selection. The retained and
// SAT layers are drawn from different sources, so a disagreement between
// them is only visible by comparing the same frame across selections.
// Answers the only question that matters about off-camera actors: does the
// artwork they draw change while their phase advances?
//
// Freeze counters report code paths, and a path can report success while
// emitting the same pixels, so they cannot see this. Comparing the per-slot
// visual hash against the phase can: a slot whose phase moves many times off
// camera while its artwork takes one value is frozen, by definition, and that
// is exactly what a player sees.
// Attributes an early-game appearance defect to either the lifted camera cull
// or the renderer's own seeding.
//
// Composing the same frame twice from one snapshot separates them. The learned
// cache has already watched the whole table go past the camera, so it resolves
// every actor from artwork it has actually seen; a fresh cache has seen
// nothing and must seed from ROM descriptors. Any pixel they disagree on is a
// pixel the seeding path gets wrong, and that is what "lit until the ball goes
// up the table" looks like: the camera arrives, the actor is learned, and it
// corrects itself.
[[nodiscard]] int run_seed_check(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const bool uncull)
{
    std::cout << std::unitbuf;
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    runtime.reset();
    devils_crush::render::PersistentActorSpriteCache learned;
    devils_crush::render::TablePaletteCache palette;
    // Pass one: drive the camera over the whole table so the cache learns.
    constexpr int kLearnFrames = 2100;
    for (int frame_number = 1; frame_number <= kLearnFrames; ++frame_number) {
        if (uncull) {
            keep_fixed_actors_simulating(runtime);
        }
        static_cast<void>(
            runtime.run_frame(state_census_input_for_frame(frame_number)));
        const auto work_ram = runtime.export_work_ram();
        const auto state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        const auto video = runtime.export_video_memory();
        if (!state || !video
            || state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        static_cast<void>(decode_cabinet_table(
            *rom, work_ram, *video, learned, palette, CabinetLayers::all,
            false, nullptr, nullptr));
    }

    // Pass two: replay to just after table setup and compose that one snapshot
    // both ways.
    runtime.reset();
    // Sweep the opening rather than sampling one frame: the complaint is that
    // the table looks wrong "at the beginning" and settles once the camera
    // reaches it, so the disagreement has to be looked for across that window.
    // Probe where the camera has left the lower table, so the actors under
    // test are genuinely off camera. Sampling frames where they are on camera
    // proves nothing: both composes resolve from the same live VRAM there.
    static constexpr std::array kProbeFrames{
        1450, 1530, 1600, 1700, 1870, 1950};
    std::vector<std::byte> work_ram;
    std::optional<devils_crush::emulation::VideoMemorySnapshot> video;
    for (int frame_number = 1; frame_number <= kProbeFrames.back();
         ++frame_number) {
        if (uncull) {
            keep_fixed_actors_simulating(runtime);
        }
        static_cast<void>(
            runtime.run_frame(state_census_input_for_frame(frame_number)));
        work_ram = runtime.export_work_ram();
        video = runtime.export_video_memory();
        if (std::ranges::find(kProbeFrames, frame_number)
            == kProbeFrames.end()) {
            continue;
        }
        if (!video) {
            continue;
        }
        const auto probe_state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        if (!probe_state
            || probe_state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        devils_crush::render::PersistentActorSpriteCache probe_actors;
        devils_crush::render::TablePaletteCache probe_palette;
        const auto probe_seeded = decode_cabinet_table(
            *rom, work_ram, *video, probe_actors, probe_palette,
            CabinetLayers::all, false, nullptr, nullptr);
        devils_crush::render::PersistentActorSpriteCache learned_copy = learned;
        devils_crush::render::TablePaletteCache palette_copy = palette;
        const auto probe_learned = decode_cabinet_table(
            *rom, work_ram, *video, learned_copy, palette_copy,
            CabinetLayers::all, false, nullptr, nullptr);
        if (!probe_seeded || !probe_learned) {
            continue;
        }
        std::size_t probe_total = 0;
        for (std::size_t index = 0;
             index < probe_seeded->image.pixels.size(); ++index) {
            probe_total += probe_seeded->image.pixels[index]
                    != probe_learned->image.pixels[index]
                ? 1U
                : 0U;
        }
        std::cout << "  frame " << frame_number << ": " << probe_total
                  << " pixels differ\n";
        if (probe_total == 0) {
            continue;
        }
        // Name the off-camera actors responsible rather than a pixel count.
        const auto probe_actors_state =
            devils_crush::cabinet::decode_table_actor_state(work_ram);
        if (!probe_actors_state) {
            continue;
        }
        const int probe_width = probe_seeded->image.width;
        const int probe_height = probe_seeded->image.height;
        for (const auto& actor : *probe_actors_state) {
            if (!actor.occupied() || actor.camera_active()) {
                continue;
            }
            const int origin_x = actor.world_x - 24;
            const int origin_y =
                actor.world_y - devils_crush::cabinet::kTableWorldTop - 24;
            std::size_t count = 0;
            for (int row = std::max(origin_y, 0);
                 row < std::min(origin_y + 48, probe_height); ++row) {
                for (int column = std::max(origin_x, 0);
                     column < std::min(origin_x + 48, probe_width); ++column) {
                    const auto index =
                        static_cast<std::size_t>(row)
                            * static_cast<std::size_t>(probe_width)
                        + static_cast<std::size_t>(column);
                    count += probe_seeded->image.pixels[index]
                            != probe_learned->image.pixels[index]
                        ? 1U
                        : 0U;
                }
            }
            if (count < 64) {
                continue;
            }
            std::cout << "      off-camera slot " << std::hex << std::setw(2)
                      << std::setfill('0')
                      << static_cast<unsigned>(actor.slot) << std::dec
                      << std::setfill(' ') << " appearance "
                      << static_cast<unsigned>(actor.appearance) << " at ("
                      << actor.world_x << "," << actor.world_y
                      << "): " << count << " pixels\n";
        }
    }
    return EXIT_SUCCESS;
}

// Watches the palette-attribute plane for whole-table rewrites.
//
// TablePaletteCache seeds its retained copy from the first plane it is shown,
// on the stated grounds that scene start is the only moment every row is
// simultaneously authoritative. It never seeds again. So if the game performs
// another whole-table assignment later -- and leaving and re-entering the
// table scene is exactly that -- every row the camera has not visited since
// keeps the previous scene's attributes indefinitely, drawing those tiles
// through the wrong subpalette until the camera happens to reach them.
//
// Reports every frame in which a large number of tile rows change at once,
// which incremental streaming never does, together with how many rows the
// camera had visited by then. Rows not visited are rows left stale.
[[nodiscard]] int run_attribute_check(
    devils_crush::emulation::PceRuntime& runtime)
{
    std::cout << std::unitbuf;
    runtime.reset();
    constexpr std::size_t kPlaneOffset = 0x0259U;
    using Cache = devils_crush::render::TablePaletteCache;
    constexpr std::size_t kRows = Cache::kHeightTiles;
    constexpr std::size_t kBytesPerRow = Cache::kBytesPerRow;
    // Only rows the cabinet actually shows can be seen to be wrong.
    constexpr std::size_t kFirstShownRow =
        devils_crush::cabinet::kTableWorldTop / Cache::kTilePixels;
    constexpr std::size_t kLastShownRow =
        (devils_crush::cabinet::kTableWorldTop
         + devils_crush::cabinet::kTableWorldHeight - 1)
        / Cache::kTilePixels;

    // Mirror TablePaletteCache exactly: seed once, refresh rows under the
    // camera. Then ask what that costs -- a displayed row whose retained
    // bytes have fallen behind the live plane is a row the player is being
    // shown in the wrong colours until the camera returns to it.
    std::vector<std::byte> retained;
    std::vector<std::byte> previous_live;
    std::array<bool, kRows> was_on_camera{};
    std::size_t entries = 0;
    std::size_t retained_correct_on_entry = 0;
    std::size_t live_correct_on_entry = 0;
    std::array<std::size_t, kRows> stale_frames{};
    std::size_t worst_rows_at_once = 0;
    int worst_frame = 0;
    std::size_t compared_frames = 0;
    for (int frame_number = 1; frame_number <= 2100; ++frame_number) {
        keep_fixed_actors_simulating(runtime);
        static_cast<void>(
            runtime.run_frame(state_census_input_for_frame(frame_number)));
        const auto work_ram = runtime.export_work_ram();
        const auto state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        if (!state || work_ram.size() < kPlaneOffset + kRows * kBytesPerRow
            || state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        const auto plane = std::span<const std::byte>{work_ram}.subspan(
            kPlaneOffset, kRows * kBytesPerRow);
        if (retained.size() != plane.size()) {
            retained.assign(plane.begin(), plane.end());
        }
        const auto byte_at = [&work_ram](const std::size_t offset) {
            return static_cast<int>(
                std::to_integer<std::uint8_t>(work_ram[offset]));
        };
        const int viewport_top_row = 480
            + static_cast<int>(static_cast<std::int16_t>(
                static_cast<std::uint16_t>(
                    byte_at(0x0BU) | (byte_at(0x0CU) << 8))));
        ++compared_frames;
        std::size_t stale_now = 0;
        for (std::size_t row = kFirstShownRow; row <= kLastShownRow; ++row) {
            const auto offset = static_cast<std::ptrdiff_t>(row * kBytesPerRow);
            const auto row_top = static_cast<int>(row * Cache::kTilePixels);
            const bool on_camera =
                row_top + static_cast<int>(Cache::kTilePixels)
                    > viewport_top_row
                && row_top < viewport_top_row + 208;
            if (on_camera) {
                // The moment a row arrives under the camera the live plane is
                // authoritative for it. That is the one instant the two
                // candidate policies can be scored against ground truth: what
                // the retained copy has been showing, and what simply reading
                // the live plane would have shown.
                if (!was_on_camera[row]) {
                    const bool retained_agrees = std::equal(
                        plane.begin() + offset,
                        plane.begin() + offset
                            + static_cast<std::ptrdiff_t>(kBytesPerRow),
                        retained.begin() + offset);
                    const bool live_agrees = previous_live.empty()
                        || std::equal(
                            plane.begin() + offset,
                            plane.begin() + offset
                                + static_cast<std::ptrdiff_t>(kBytesPerRow),
                            previous_live.begin() + offset);
                    ++entries;
                    retained_correct_on_entry += retained_agrees ? 1U : 0U;
                    live_correct_on_entry += live_agrees ? 1U : 0U;
                }
                was_on_camera[row] = true;
                std::copy_n(
                    plane.begin() + offset, kBytesPerRow,
                    retained.begin() + offset);
                continue;
            }
            was_on_camera[row] = false;
            if (!std::equal(
                    plane.begin() + offset,
                    plane.begin() + offset
                        + static_cast<std::ptrdiff_t>(kBytesPerRow),
                    retained.begin() + offset)) {
                ++stale_frames[row];
                ++stale_now;
            }
        }
        previous_live.assign(plane.begin(), plane.end());

        if (stale_now > worst_rows_at_once) {
            worst_rows_at_once = stale_now;
            worst_frame = frame_number;
        }
    }

    std::size_t rows_ever_stale = 0;
    std::size_t total = 0;
    for (std::size_t row = kFirstShownRow; row <= kLastShownRow; ++row) {
        total += stale_frames[row];
        rows_ever_stale += stale_frames[row] != 0U ? 1U : 0U;
    }
    std::cout << "over " << compared_frames << " table frames, "
              << rows_ever_stale << " of "
              << (kLastShownRow - kFirstShownRow + 1)
              << " displayed tile rows were shown with stale attributes at "
                 "some point\n"
              << "worst single frame: " << worst_rows_at_once
              << " stale rows at frame " << worst_frame << "\n"
              << "row-frames drawn stale: " << total << "\n";
    // Scored where the answer is not a matter of opinion: the instant a row
    // arrives under the camera, the live plane is authoritative for it.
    std::cout << "\nat the instant each row arrives under the camera:\n"
              << "  retained copy agreed with it on "
              << retained_correct_on_entry << " of " << entries
              << " arrivals\n"
              << "  reading the live plane would have agreed on "
              << live_correct_on_entry << " of " << entries << "\n";
    std::cout << "\nworst rows (tile row, world y, frames stale)\n";
    std::vector<std::pair<std::size_t, std::size_t>> ranked;
    for (std::size_t row = kFirstShownRow; row <= kLastShownRow; ++row) {
        if (stale_frames[row] != 0U) {
            ranked.emplace_back(stale_frames[row], row);
        }
    }
    std::ranges::sort(ranked, std::ranges::greater{});
    for (std::size_t index = 0; index < std::min<std::size_t>(12, ranked.size());
         ++index) {
        std::cout << "  row " << ranked[index].second << "  y "
                  << ranked[index].second * Cache::kTilePixels << "  "
                  << ranked[index].first << " frames\n";
    }
    return EXIT_SUCCESS;
}

// Composes the whole table from a saved state and writes it out, so a defect
// that only a player can see becomes pixels that can be examined directly.
//
// A single-frame compose from a fresh cache is what a session looks like at
// the moment the state was taken only in the sense that matters here: nothing
// off camera has been learned. Dumping the learned compose beside it says
// immediately whether the defect is the cache or something upstream of it.
[[nodiscard]] int run_state_dump(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path,
    const std::filesystem::path& output_directory)
{
    std::cout << std::unitbuf;
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    std::error_code directory_error;
    std::filesystem::create_directories(output_directory, directory_error);

    // The learned pass has to run before the state is restored, because
    // restoring rewinds the emulator but not the cache.
    devils_crush::render::PersistentActorSpriteCache learned;
    devils_crush::render::TablePaletteCache learned_palette;
    for (int frame_number = 1; frame_number <= 2100; ++frame_number) {
        keep_fixed_actors_simulating(runtime);
        static_cast<void>(
            runtime.run_frame(state_census_input_for_frame(frame_number)));
        const auto work_ram = runtime.export_work_ram();
        const auto state =
            devils_crush::cabinet::decode_cabinet_state(work_ram);
        const auto video = runtime.export_video_memory();
        if (!state || !video
            || state->playfield
                != devils_crush::cabinet::PlayfieldView::table_background) {
            continue;
        }
        static_cast<void>(decode_cabinet_table(
            *rom, work_ram, *video, learned, learned_palette,
            CabinetLayers::all, false, nullptr, nullptr));
    }

    if (!load_quick_state(runtime, state_path)) {
        return EXIT_FAILURE;
    }
    keep_fixed_actors_simulating(runtime);
    const auto emulated = runtime.run_frame({});
    if (!emulated.framebuffer.pixels.empty()) {
        // Hardware's own picture of whatever the camera covers. Any
        // disagreement inside that window is the reconstruction's fault.
        devils_crush::render::ArgbImage hardware;
        hardware.width = emulated.framebuffer.width;
        hardware.height = emulated.framebuffer.height;
        hardware.pixels.resize(
            static_cast<std::size_t>(hardware.width)
            * static_cast<std::size_t>(hardware.height));
        for (int row = 0; row < hardware.height; ++row) {
            for (int column = 0; column < hardware.width; ++column) {
                hardware.pixels[
                    static_cast<std::size_t>(row) * hardware.width + column] =
                    emulated.framebuffer.pixels[
                        static_cast<std::size_t>(row)
                            * emulated.framebuffer.pitch_pixels
                        + column];
            }
        }
        static_cast<void>(save_playfield_png(
            hardware, output_directory / "state-emulated.bmp"));
    }
    const auto work_ram = runtime.export_work_ram();
    const auto video = runtime.export_video_memory();
    const auto state = devils_crush::cabinet::decode_cabinet_state(work_ram);
    if (!video || !state
        || state->playfield
            != devils_crush::cabinet::PlayfieldView::table_background) {
        std::cout << "the saved state is not on the table playfield\n";
        return EXIT_FAILURE;
    }

    devils_crush::render::PersistentActorSpriteCache fresh;
    devils_crush::render::TablePaletteCache fresh_palette;
    const auto fresh_table = decode_cabinet_table(
        *rom, work_ram, *video, fresh, fresh_palette, CabinetLayers::all,
        false, nullptr, nullptr);
    const auto learned_table = decode_cabinet_table(
        *rom, work_ram, *video, learned, learned_palette, CabinetLayers::all,
        false, nullptr, nullptr);
    const auto background_only = [&] {
        devils_crush::render::PersistentActorSpriteCache empty;
        devils_crush::render::TablePaletteCache empty_palette;
        return decode_cabinet_table(
            *rom, work_ram, *video, empty, empty_palette,
            CabinetLayers::background_only, false, nullptr, nullptr);
    }();
    if (!fresh_table || !learned_table || !background_only) {
        std::cout << "compose failed\n";
        return EXIT_FAILURE;
    }
    static_cast<void>(save_playfield_png(
        fresh_table->image, output_directory / "state-fresh.bmp"));
    static_cast<void>(save_playfield_png(
        learned_table->image, output_directory / "state-learned.bmp"));
    static_cast<void>(save_playfield_png(
        background_only->image, output_directory / "state-background.bmp"));

    std::size_t differing = 0;
    for (std::size_t index = 0; index < fresh_table->image.pixels.size();
         ++index) {
        differing += fresh_table->image.pixels[index]
                != learned_table->image.pixels[index]
            ? 1U
            : 0U;
    }
    std::cout << "wrote three composes to " << output_directory.string()
              << "\nfresh vs learned: " << differing << " pixels differ\n";
    return EXIT_SUCCESS;
}

// Asks whether the sprite palette is region-loaded.
//
// Sprite colors live in VCE entries 256-511 as sixteen subpalettes of sixteen.
// The compositor resolves an actor's palette indices against the live VCE on
// purpose, so an actor learned during a fade-in picks up settled colors instead
// of freezing as a dark silhouette. That is right for an actor under the
// camera. It is only right off camera if the subpalette it names still holds
// its colors -- and if the ROM reloads subpalettes as the camera moves between
// table regions, an off-camera actor is drawn through whichever region's colors
// happen to be resident, which reads as the wrong color or as gray.
//
// Reports, as the camera sweeps the table, which subpalettes change and whether
// the actor under test was on camera when they did.
[[nodiscard]] int run_palette_check(
    devils_crush::emulation::PceRuntime& runtime)
{
    std::cout << std::unitbuf;
    runtime.reset();
    constexpr std::size_t kSpriteSubpalettes = 16U;
    constexpr std::size_t kSpritePaletteBase = 256U;
    constexpr std::size_t kDragonHeadSlot = 0x13U;
    std::array<std::uint64_t, kSpriteSubpalettes> previous{};
    bool have_previous = false;
    std::cout << "frame  camera  slot13  changed subpalettes\n";
    for (int frame_number = 1; frame_number <= 2100; ++frame_number) {
        keep_fixed_actors_simulating(runtime);
        static_cast<void>(
            runtime.run_frame(state_census_input_for_frame(frame_number)));
        const auto video = runtime.export_video_memory();
        const auto work_ram = runtime.export_work_ram();
        if (!video || video->palette_entries.size() < 512U
            || work_ram.size() < 0x2000U) {
            continue;
        }
        std::array<std::uint64_t, kSpriteSubpalettes> now{};
        for (std::size_t sub = 0; sub < kSpriteSubpalettes; ++sub) {
            std::uint64_t hash = 1469598103934665603ULL;
            for (std::size_t entry = 0; entry < 16U; ++entry) {
                hash = (hash
                        ^ video->palette_entries
                              [kSpritePaletteBase + sub * 16U + entry])
                    * 1099511628211ULL;
            }
            now[sub] = hash;
        }
        std::string changed;
        if (have_previous) {
            for (std::size_t sub = 0; sub < kSpriteSubpalettes; ++sub) {
                if (now[sub] != previous[sub]) {
                    changed += std::format("{:X} ", sub);
                }
            }
        }
        previous = now;
        have_previous = true;
        if (changed.empty()) {
            continue;
        }
        const auto byte_at = [&work_ram](const std::size_t offset) {
            return static_cast<int>(
                std::to_integer<std::uint8_t>(work_ram[offset]));
        };
        std::cout << std::setw(5) << frame_number << std::setw(8)
                  << static_cast<int>(static_cast<std::int16_t>(
                         static_cast<std::uint16_t>(
                             byte_at(0x0BU) | (byte_at(0x0CU) << 8))))
                  << std::setw(8)
                  << (((byte_at(0x1080U + kDragonHeadSlot) & 0x80U) != 0)
                          ? "on"
                          : "off")
                  << "  " << changed << "\n";
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int run_cabinet_motion_check(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& directory,
    const bool gpu)
{
    const auto rom = read_rom_bytes(rom_path);
    if (!rom) return EXIT_FAILURE;
    std::filesystem::create_directories(directory);
    constexpr int w = devils_crush::cabinet::kTableWorldWidth;
    constexpr int h = devils_crush::cabinet::kTableWorldHeight;
    SDL_Surface* surface = SDL_CreateSurface(w * 4, h * 4, SDL_PIXELFORMAT_ARGB8888);
    SDL_Window* window = gpu && SDL_Init(SDL_INIT_VIDEO)
        ? SDL_CreateWindow("Motion GPU check", w * 4, h * 4, SDL_WINDOW_HIDDEN) : nullptr;
    SDL_Renderer* renderer = gpu ? (window ? SDL_CreateRenderer(window, nullptr) : nullptr)
        : (surface ? SDL_CreateSoftwareRenderer(surface) : nullptr);
    if (!renderer) {
        SDL_DestroySurface(surface);
        SDL_DestroyWindow(window);
        report_sdl_error("Motion check renderer failed");
        return EXIT_FAILURE;
    }
    SDL_Texture* target = gpu ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET, w * 4, h * 4) : nullptr;
    if (gpu && (!target || !SDL_SetRenderTarget(renderer, target))) {
        SDL_DestroyTexture(target);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_DestroySurface(surface);
        return EXIT_FAILURE;
    }
    std::cout << "Motion check renderer: " << SDL_GetRendererName(renderer) << '\n';
    TableMotionPresenter presenter;
    const auto draw = [&](float alpha, bool even = true) {
        if (!presenter.draw(renderer, w * 4, h * 4, alpha, even) || !SDL_FlushRenderer(renderer)) return false;
        if (!gpu) return true;
        auto* readback = SDL_RenderReadPixels(renderer, nullptr);
        auto* converted = readback ? SDL_ConvertSurface(readback, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        SDL_DestroySurface(readback);
        if (!converted) return false;
        SDL_DestroySurface(surface);
        surface = converted;
        return true;
    };
    devils_crush::render::PersistentActorSpriteCache actors;
    devils_crush::render::TablePaletteCache palette;
    runtime.reset();
    int checked = 0;
    int moving = 0;
    int moving_ball_frames = 0;
    int interpolated_ball_frames = 0;
    int swung_frames = 0;
    bool ok = check_playfield_rotation() && check_artwork_blending(renderer);
    for (int tick = 1; tick <= 2100 && ok; ++tick) {
        keep_fixed_actors_simulating(runtime);
        // Once the ball is in play, both flippers go up for twelve frames in
        // every thirty, so all six swing.
        auto input = state_census_input_for_frame(tick);
        if (tick > 1300 && tick % 30 < 12)
            input.player_one |= static_cast<std::uint16_t>(Button::left) | static_cast<std::uint16_t>(Button::one);
        static_cast<void>(runtime.run_frame(input));
        const auto ram = runtime.export_work_ram();
        const auto state = devils_crush::cabinet::decode_cabinet_state(ram);
        const auto video = runtime.export_video_memory();
        if (!state || !video || state->playfield
            != devils_crush::cabinet::PlayfieldView::table_background) {
            presenter.reset();
            continue;
        }
        devils_crush::render::TableMotionFrame motion;
        const auto table = decode_cabinet_table(*rom, ram, *video, actors, palette,
            CabinetLayers::all, false, nullptr, nullptr, &motion);
        if (!table || !presenter.update(renderer, std::move(motion))) { ok = false; break; }
        if (std::ranges::any_of(presenter.frame.sprites,
                [](const auto& sprite) { return sprite.swing.has_value(); })) ++swung_frames;
        const auto ball = std::ranges::find_if(presenter.frame.sprites, [](const auto& sprite) {
            return sprite.actor.slot == devils_crush::cabinet::kBallActorSlot;
        });
        if (ball != presenter.frame.sprites.end()) {
            const auto& prior = presenter.previous[ball->actor.slot];
            if (prior && (prior->world_x != ball->actor.world_x || prior->world_y != ball->actor.world_y)) {
                ++moving_ball_frames;
                const auto offset = devils_crush::render::table_motion_offset(ball->actor, &*prior, 0.25F);
                if (offset != std::pair{0.0F, 0.0F}) ++interpolated_ball_frames;
            }
        }
        if (tick % 17 != 0) continue;
        ok = draw(1.0F, false);
        if (!ok) break;
        // At the endpoint each original pixel must be an exact 4x4 block.
        for (int y = 0; y < h * 4 && ok; ++y) {
            const auto* row = reinterpret_cast<const std::uint32_t*>(
                static_cast<const std::byte*>(surface->pixels) + y * surface->pitch);
            for (int x = 0; x < w * 4; ++x) {
                if (row[x] != table->image.pixels[(y / 4) * w + x / 4]) {
                    std::cerr << "Motion endpoint mismatch at tick " << tick
                              << " pixel " << x << ',' << y << '\n';
                    ok = false;
                    break;
                }
            }
        }
        if (!ok) break;
        ++checked;
        std::vector<std::byte> endpoint(static_cast<std::size_t>(surface->pitch) * surface->h);
        std::memcpy(endpoint.data(), surface->pixels, endpoint.size());
        ok = draw(0.25F);
        if (!ok) break;
        if (std::memcmp(endpoint.data(), surface->pixels, endpoint.size()) != 0) {
            ++moving;
            if (moving == 10) {
                ok = SDL_SaveBMP(surface, (directory / "quarter-frame.bmp").string().c_str());
                if (ok) ok = draw(1.0F)
                    && SDL_SaveBMP(surface, (directory / "current-frame.bmp").string().c_str());
            }
        }
        if (runtime.export_work_ram() != ram) ok = false;
    }
    presenter.reset();
    if (ok) ok = check_viewport_artwork(renderer, runtime, directory);
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_DestroyTexture(target);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_DestroySurface(surface);
    std::cout << "Table motion: " << checked << " exact 4x endpoint frames; "
              << moving << " distinct quarter-frame pictures; gameplay RAM unchanged.\n";
    std::cout << "Ball interpolation: " << interpolated_ball_frames << " of "
              << moving_ball_frames << " moving ball frames (cuts excluded).\n";
    std::cout << "Flipper swings: " << swung_frames << " table frames.\n";
    return ok && checked > 0 && moving > 0 && swung_frames > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Reproduces what a cabinet shows when the picture is wrong, without
// guessing which screen is at fault.
//
// Cabinet mode draws a reconstructed table whenever `decode_cabinet_state`
// says the playfield is the table background, and the emulator's own
// framebuffer otherwise. If that decision is ever wrong the cabinet paints a
// table over whatever the game has actually put on screen, so this measures
// the decision against the picture: for every frame classified as the table
// background it crops the reconstruction to the camera window the way
// `--cabinet-oracle` does and counts the pixels that disagree with hardware.
// A handful of per cent is ordinary sprite error. Most of the frame
// disagreeing means the cabinet is not showing the game.
//
// It also watches `09:69B2` and `09:69FA`, the cartridge's high-score
// renderers, so a disagreeing span can be tied to that screen or ruled out.
[[nodiscard]] int run_cabinet_view_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& output_directory)
{
    constexpr int kAttractFrames = 9000;
    constexpr int kPauseMenuFrames = 1900;
    constexpr std::size_t kBadFramePercent = 25U;
    constexpr int kMaximumDumps = 4;

    const auto rom = read_rom_bytes(rom_path);
    if (!rom) {
        return EXIT_FAILURE;
    }
    std::error_code directory_error;
    std::filesystem::create_directories(output_directory, directory_error);

    // The oracle's tolerance: hardware and the reconstruction quantise colour
    // differently, so compare at the precision both agree on.
    const auto quantize = [](const std::uint32_t argb) {
        const auto channel = [&](const unsigned shift) {
            return ((argb >> shift) & 0xFFU) * 7U + 127U;
        };
        return std::tuple{
            channel(16U) / 255U, channel(8U) / 255U, channel(0U) / 255U};
    };
    // Carries this frame's work RAM into the next iteration on every exit
    // path, so the frame before a break can be dumped beside it.
    struct WorkRamKeeper {
        std::vector<std::byte>& previous;
        const std::vector<std::byte>& current;

        ~WorkRamKeeper()
        {
            previous = current;
        }
    };

    struct RouteSpec {
        ViewProbeRoute route;
        int frames;
        const char* name;
    };
    static constexpr std::array routes{
        RouteSpec{ViewProbeRoute::attract, kAttractFrames, "attract"},
        RouteSpec{
            ViewProbeRoute::pause_menu, kPauseMenuFrames, "pause-menu"},
        RouteSpec{
            ViewProbeRoute::census, kStateCensusRecordingFrames, "census"},
    };

    bool reproduced = false;
    for (const auto& spec : routes) {
        runtime.clear_execution_hooks();
        runtime.reset();
        bool rendered = false;
        // Bank `$09` is mapped at `$6000`, not `$C000`: its own calls go to
        // `$7F37` and `$76DF`, and the twelve-byte backup-RAM entry names it
        // hands the library live at `$771A` and `$7726`, which are bank
        // offsets `$1F37`, `$16DF`, `$171A` and `$1726` read through MPR3.
        constexpr std::array<std::uint16_t, 2> render_sites{
            std::uint16_t{0x69B2U}, std::uint16_t{0x69FAU}};
        for (const std::uint16_t site : render_sites) {
            if (!runtime.install_execution_hook({
                    .routine =
                        RoutineKey{
                            .physical_bank = 0x09U, .logical_pc = site},
                    .mode = HookMode::observe,
                    .callback =
                        [&rendered](const RoutineEntry&) {
                            rendered = true;
                        },
                })) {
                std::cerr << "Could not watch the high-score renderer.\n";
                return EXIT_FAILURE;
            }
        }

        devils_crush::render::PersistentActorSpriteCache actors;
        devils_crush::render::TablePaletteCache palette;
        std::optional<devils_crush::cabinet::PlayfieldView> last_view;
        std::vector<std::string> timeline;
        int render_frames = 0;
        devils_crush::cabinet::DmdPanel probe_dmd;
        int unexpected_live_dmd_frames = 0;
        int bad_frames = 0;
        int bad_span_start = 0;
        int last_bad_frame = 0;
        int dumps = 0;
        std::vector<std::string> bad_spans;
        const auto close_span = [&bad_spans, &bad_span_start,
                                 &last_bad_frame]() {
            if (bad_span_start == 0) {
                return;
            }
            bad_spans.push_back(std::format(
                "  frames {}-{} ({} frames)",
                bad_span_start,
                last_bad_frame,
                last_bad_frame - bad_span_start + 1));
            bad_span_start = 0;
        };

        std::vector<std::byte> previous_work_ram;
        // `$2054` reads 0 on the table, 3 on the title and attract screens
        // and 4 on the High Scores screen; this counts what it actually held
        // on every frame the current rule drew as the table.
        std::map<std::uint8_t, int> screen_selector_frames;
        std::cout << "=== " << spec.name << " ===\n";
        for (int frame_number = 1; frame_number <= spec.frames;
             ++frame_number) {
            rendered = false;
            const auto frame =
                runtime.run_frame(view_probe_input(spec.route, frame_number));
            const auto work_ram = runtime.export_work_ram();
            const WorkRamKeeper keeper{previous_work_ram, work_ram};
            const auto state =
                devils_crush::cabinet::decode_cabinet_state(work_ram);
            if (!state) {
                continue;
            }
            render_frames += rendered ? 1 : 0;
            probe_dmd.observe(work_ram);
            if (spec.route == ViewProbeRoute::attract && !probe_dmd.attract()) {
                ++unexpected_live_dmd_frames;
                reproduced = true;
            }
            const bool as_table = state->playfield
                == devils_crush::cabinet::PlayfieldView::table_background;
            if (!last_view || *last_view != state->playfield) {
                timeline.push_back(std::format(
                    "  frame {:>5}  {:<20} balls {:>3}  session ${:02X}",
                    frame_number,
                    as_table ? "table background" : "emulator framebuffer",
                    state->balls_in_session,
                    std::to_integer<std::uint8_t>(work_ram[0x0053U])));
                last_view = state->playfield;
            }
            if (as_table) {
                ++screen_selector_frames[
                    std::to_integer<std::uint8_t>(work_ram[0x0054U])];
            }
            const auto video = runtime.export_video_memory();
            if (!as_table || !video || frame.framebuffer.width < 256
                || frame.framebuffer.height < 238) {
                close_span();
                continue;
            }
            const auto table = decode_cabinet_table(
                *rom, work_ram, *video, actors, palette, CabinetLayers::all,
                false, nullptr, nullptr);
            if (!table) {
                close_span();
                continue;
            }

            const auto camera_y = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(work_ram[0x0BU]))
                | (static_cast<std::uint16_t>(
                       std::to_integer<std::uint8_t>(work_ram[0x0CU]))
                   << 8U));
            const int work_ram_viewport = 480 + static_cast<int>(camera_y);
            auto viewport_top_row =
                static_cast<int>(video->background_y_scroll) - 32;
            while (viewport_top_row + 128 < work_ram_viewport) {
                viewport_top_row += 256;
            }
            while (viewport_top_row - 128 > work_ram_viewport) {
                viewport_top_row -= 256;
            }
            std::size_t compared = 0;
            std::size_t mismatched = 0;
            for (int framebuffer_y = 30; framebuffer_y < 238;
                 ++framebuffer_y) {
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
                    ++compared;
                    if (quantize(native) != quantize(emulated)) {
                        ++mismatched;
                    }
                }
            }
            if (compared == 0U
                || mismatched * 100U < compared * kBadFramePercent) {
                close_span();
                continue;
            }
            ++bad_frames;
            reproduced = true;
            if (bad_span_start == 0) {
                bad_span_start = frame_number;
                // The frame the picture went wrong on and the last frame it
                // was right on, so the state that distinguishes them can be
                // found by subtraction rather than by guesswork.
                const auto write_bytes =
                    [&output_directory](
                        const std::string& stem,
                        const std::span<const std::byte> bytes) {
                        std::ofstream out(
                            output_directory / (stem + ".bin"),
                            std::ios::binary | std::ios::trunc);
                        out.write(
                            reinterpret_cast<const char*>(bytes.data()),
                            static_cast<std::streamsize>(bytes.size()));
                    };
                write_bytes(
                    std::format("{}-{:05}-workram", spec.name, frame_number),
                    work_ram);
                if (!previous_work_ram.empty()) {
                    write_bytes(
                        std::format(
                            "{}-{:05}-workram",
                            spec.name,
                            frame_number - 1),
                        previous_work_ram);
                }
                std::cout << std::format(
                    "  video at frame {}: MWR ${:04X} BXR ${:04X} "
                    "BYR ${:04X}\n",
                    frame_number,
                    video->memory_width_register,
                    video->background_x_scroll,
                    video->background_y_scroll);
            }
            last_bad_frame = frame_number;
            if (dumps < kMaximumDumps
                && (frame_number == bad_span_start
                    || frame_number == bad_span_start + 30)) {
                ++dumps;
                const auto stem = std::format(
                    "{}-{:05}-{}pc",
                    spec.name,
                    frame_number,
                    mismatched * 100U / compared);
                if (!frame.framebuffer.pixels.empty()) {
                    devils_crush::render::ArgbImage hardware;
                    hardware.width = frame.framebuffer.width;
                    hardware.height = frame.framebuffer.height;
                    hardware.pixels.resize(
                        static_cast<std::size_t>(hardware.width)
                        * static_cast<std::size_t>(hardware.height));
                    for (int row = 0; row < hardware.height; ++row) {
                        for (int column = 0; column < hardware.width; ++column) {
                            hardware.pixels[
                                static_cast<std::size_t>(row) * hardware.width
                                + column] = frame.framebuffer.pixels[
                                static_cast<std::size_t>(row)
                                    * frame.framebuffer.pitch_pixels
                                + column];
                        }
                    }
                    static_cast<void>(save_playfield_png(
                        hardware, output_directory / (stem + "-emulated.bmp")));
                }
                static_cast<void>(save_playfield_png(
                    table->image,
                    output_directory / (stem + "-cabinet.bmp")));
            }
        }
        close_span();
        runtime.clear_execution_hooks();

        if (spec.route == ViewProbeRoute::attract) {
            std::cout << "DMD attract loop: " << unexpected_live_dmd_frames
                      << " unexpected live-game frames across " << spec.frames
                      << " frames\n";
        }

        for (const auto& line : timeline) {
            std::cout << line << '\n';
        }
        std::cout << "  high-score renderer entered on " << render_frames
                  << " frames\n"
                  << "  frames drawn as the table that disagree with "
                  << "hardware by " << kBadFramePercent << "% or more: "
                  << bad_frames << '\n';
        for (const auto& span : bad_spans) {
            std::cout << span << '\n';
        }
        std::cout << "  $2054 over those frames:";
        for (const auto& [value, count] : screen_selector_frames) {
            std::cout << std::format(" ${:02X}={}", value, count);
        }
        std::cout << "\n  picture pairs written: " << dumps << '\n';
    }
    std::cout << "pictures are under " << output_directory.string() << '\n';
    return reproduced ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace devils_crush::app::host
