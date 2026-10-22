// SPDX-License-Identifier: GPL-3.0-or-later
#include "deterministic_recording.hpp"

#include "command_line.hpp"
#include "host_files.hpp"
#include "play_assists.hpp"
#include "recording_plans.hpp"
#include "registry_reports.hpp"
#include "trace_writers.hpp"

namespace devils_crush::app::host {

int run_deterministic_recording(
    PceRuntime& runtime,
    const std::filesystem::path& path,
    const std::filesystem::path& work_ram_path,
    const std::filesystem::path& framebuffer_path,
    const std::filesystem::path& audio_path,
    const std::string_view core_revision,
    const RecordingPlan& plan,
    const std::span<const WorkRamPatch> work_ram_patches,
    const std::filesystem::path& table_vram_census_path,
    const std::span<const std::byte> rom,
    const std::filesystem::path& initial_state_path,
    const std::function<void()>& failure_callback)
{
    if (runtime.work_ram_size() == 0U || plan.frame_count <= 0
        || (plan.input_for_frame == nullptr
            && plan.captured_inputs.size()
                != static_cast<std::size_t>(plan.frame_count))
        || (!plan.expected_work_ram_sha256s.empty()
            && plan.expected_work_ram_sha256s.size()
                != static_cast<std::size_t>(plan.frame_count))
        || plan.format.empty()
        || plan.label.empty()) {
        std::cerr << "The emulator did not expose work RAM for recording.\n";
        return EXIT_FAILURE;
    }

    std::error_code error;
    const auto recording_directory = path.parent_path();
    if (!recording_directory.empty()) {
        std::filesystem::create_directories(recording_directory, error);
    }
    if (error) {
        std::cerr << "Could not create recording directory: "
                  << error.message() << '\n';
        return EXIT_FAILURE;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create " << plan.label << " recording: "
                  << temporary_path.string() << '\n';
        return EXIT_FAILURE;
    }

    auto work_ram_temporary_path = work_ram_path;
    std::ofstream work_ram_output;
    if (!work_ram_path.empty()) {
        const auto work_ram_directory = work_ram_path.parent_path();
        if (!work_ram_directory.empty()) {
            std::filesystem::create_directories(
                work_ram_directory,
                error);
        }
        if (error) {
            std::cerr << "Could not create work-RAM recording directory: "
                      << error.message() << '\n';
            return EXIT_FAILURE;
        }
        work_ram_temporary_path += ".tmp";
        work_ram_output.open(
            work_ram_temporary_path,
            std::ios::binary | std::ios::trunc);
        if (!work_ram_output) {
            std::cerr << "Could not create work-RAM recording: "
                      << work_ram_temporary_path.string() << '\n';
            return EXIT_FAILURE;
        }
    }

    auto framebuffer_temporary_path = framebuffer_path;
    std::ofstream framebuffer_output;
    if (!framebuffer_path.empty()) {
        const auto framebuffer_directory = framebuffer_path.parent_path();
        if (!framebuffer_directory.empty()) {
            std::filesystem::create_directories(
                framebuffer_directory,
                error);
        }
        if (error) {
            std::cerr << "Could not create framebuffer recording directory: "
                      << error.message() << '\n';
            return EXIT_FAILURE;
        }
        framebuffer_temporary_path += ".tmp";
        framebuffer_output.open(
            framebuffer_temporary_path,
            std::ios::binary | std::ios::trunc);
        if (!framebuffer_output) {
            std::cerr << "Could not create framebuffer recording: "
                      << framebuffer_temporary_path.string() << '\n';
            return EXIT_FAILURE;
        }
    }

    auto audio_temporary_path = audio_path;
    std::ofstream audio_output;
    if (!audio_path.empty()) {
        const auto audio_directory = audio_path.parent_path();
        if (!audio_directory.empty()) {
            std::filesystem::create_directories(audio_directory, error);
        }
        if (error) {
            std::cerr << "Could not create audio recording directory: "
                      << error.message() << '\n';
            return EXIT_FAILURE;
        }
        audio_temporary_path += ".tmp";
        audio_output.open(
            audio_temporary_path,
            std::ios::binary | std::ios::trunc);
        if (!audio_output) {
            std::cerr << "Could not create audio recording: "
                      << audio_temporary_path.string() << '\n';
            return EXIT_FAILURE;
        }
        if (!write_pcm16_wave_header(audio_output, 0U, 0U)) {
            std::cerr << "Could not initialize audio recording.\n";
            return EXIT_FAILURE;
        }
    }

    output
        << "# format=" << plan.format << '\n'
        << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
        << "# core_revision=" << core_revision << '\n'
        << "# frame_count=" << plan.frame_count << '\n'
        << "# input_script=" << plan.input_script << '\n';
    if (!work_ram_patches.empty()) {
        output << "# work_ram_patch_count=" << work_ram_patches.size()
               << '\n';
        for (std::size_t index = 0U;
             index < work_ram_patches.size();
             ++index) {
            const auto& patch = work_ram_patches[index];
            output << "# work_ram_patch_" << index + 1U << '='
                   << patch.frame << ':' << std::uppercase << std::hex
                   << std::setfill('0') << std::setw(4)
                   << patch.logical_address << ':' << std::setw(2)
                   << static_cast<unsigned int>(patch.value)
                   << std::nouppercase << std::dec << '\n';
        }
    }
    output << "frame,input_p1,input_p2,work_ram_sha256,"
           "framebuffer_width,framebuffer_height,framebuffer_sha256,"
           "audio_sample_rate,audio_stereo_samples,audio_sha256\n";

    if (!initial_state_path.empty()) {
        if (!load_quick_state(runtime, initial_state_path)) {
            return EXIT_FAILURE;
        }
    } else {
        runtime.reset();
    }
    const auto initial_frame_number = runtime.serialize().frame_number;
    const auto interactive_flippers =
        devils_crush::cabinet::flipper_inputs();
    if (plan.restore_interactive_flipper_host
        && !install_shooter_flipper_gate(runtime, interactive_flippers)) {
        std::cerr << "Could not restore the interactive flipper gate for "
                     "session replay.\n";
        return EXIT_FAILURE;
    }
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    int audio_sample_rate = 0;
    std::uint32_t audio_data_bytes = 0U;
    std::vector<std::byte> framebuffer_rgb;
    TableVramCensus table_vram_census;
    if (!table_vram_census_path.empty() && !table_vram_census.begin(rom)) {
        std::cerr << "Could not resolve the table map for the VRAM census.\n";
        return EXIT_FAILURE;
    }

    for (int index = 1; index <= plan.frame_count; ++index) {
        if (plan.restore_interactive_cabinet_host) {
            keep_fixed_actors_simulating(runtime);
        }
        if (plan.restore_interactive_flipper_host) {
            static_cast<void>(
                apply_flipper_inputs(runtime, interactive_flippers));
        }
        const auto first_patch = std::find_if(
            work_ram_patches.begin(),
            work_ram_patches.end(),
            [index](const WorkRamPatch& patch) {
                return patch.frame == index;
            });
        if (first_patch != work_ram_patches.end()) {
            auto patched_work_ram = runtime.export_work_ram();
            if (patched_work_ram.size() != runtime.work_ram_size()) {
                std::cerr << "Could not export work RAM for frame "
                          << index << " patches.\n";
                return EXIT_FAILURE;
            }
            for (auto patch = first_patch;
                 patch != work_ram_patches.end();
                 ++patch) {
                if (patch->frame != index) {
                    continue;
                }
                const auto offset = static_cast<std::size_t>(
                    patch->logical_address - 0x2000U);
                patched_work_ram[offset] =
                    static_cast<std::byte>(patch->value);
            }
            if (!runtime.import_work_ram(patched_work_ram)) {
                std::cerr << "Could not import work RAM for frame "
                          << index << " patches.\n";
                return EXIT_FAILURE;
            }
        }
        const auto input = input_for_recording_plan_frame(plan, index);
        const auto frame = runtime.run_frame(input);
        const auto work_ram = runtime.export_work_ram();
        if (frame.frame_number
                != initial_frame_number + static_cast<std::uint64_t>(index)
            || frame.framebuffer.pixels.empty()
            || frame.framebuffer.width <= 0
            || frame.framebuffer.height <= 0
            || work_ram.size() != runtime.work_ram_size()) {
            if (failure_callback) {
                failure_callback();
            }
            std::cerr << plan.label << " recording failed on frame " << index
                      << ".\n";
            return EXIT_FAILURE;
        }
        const auto work_ram_hash = sha256(work_ram);
        if (!plan.expected_work_ram_sha256s.empty()
            && work_ram_hash
                != plan.expected_work_ram_sha256s[static_cast<std::size_t>(
                    index - 1)]) {
            if (failure_callback) {
                failure_callback();
            }
            std::cerr << "Interactive session replay diverged at frame "
                      << index << ": expected work-RAM SHA-256 "
                      << plan.expected_work_ram_sha256s[
                             static_cast<std::size_t>(index - 1)]
                      << ", observed " << work_ram_hash << ".\n";
            return EXIT_FAILURE;
        }

        if (!table_vram_census_path.empty()) {
            const auto video = runtime.export_video_memory();
            if (video) {
                table_vram_census.observe(index, work_ram, video->vram_words);
            }
        }

        output << frame.frame_number << ',' << std::hex
               << std::setfill('0') << std::setw(4) << input.player_one
               << ',' << std::setw(4) << input.player_two << std::dec
               << ',' << work_ram_hash
               << ',' << frame.framebuffer.width
               << ',' << frame.framebuffer.height
               << ',' << sha256_values<std::uint32_t>(
                   frame.framebuffer.pixels)
               << ',' << frame.audio.sample_rate
               << ',' << frame.audio.interleaved_stereo.size()
               << ',' << sha256_values<float>(
                   frame.audio.interleaved_stereo)
               << '\n';
        if (work_ram_output) {
            work_ram_output.write(
                reinterpret_cast<const char*>(work_ram.data()),
                static_cast<std::streamsize>(work_ram.size()));
        }
        if (framebuffer_output) {
            if (index == 1) {
                framebuffer_width = frame.framebuffer.width;
                framebuffer_height = frame.framebuffer.height;
                constexpr char signature[] = {'D', 'C', 'F', '1'};
                const auto width = static_cast<std::uint32_t>(
                    framebuffer_width);
                const auto height = static_cast<std::uint32_t>(
                    framebuffer_height);
                const auto frame_count = static_cast<std::uint32_t>(
                    plan.frame_count);
                framebuffer_output.write(signature, sizeof(signature));
                framebuffer_output.write(
                    reinterpret_cast<const char*>(&width),
                    sizeof(width));
                framebuffer_output.write(
                    reinterpret_cast<const char*>(&height),
                    sizeof(height));
                framebuffer_output.write(
                    reinterpret_cast<const char*>(&frame_count),
                    sizeof(frame_count));
                framebuffer_rgb.resize(
                    frame.framebuffer.pixels.size() * 3U);
            } else if (frame.framebuffer.width != framebuffer_width
                       || frame.framebuffer.height != framebuffer_height) {
                std::cerr
                    << plan.label
                    << " framebuffer geometry changed on frame "
                    << index << ".\n";
                return EXIT_FAILURE;
            }

            for (std::size_t pixel_index = 0;
                 pixel_index < frame.framebuffer.pixels.size();
                 ++pixel_index) {
                const auto pixel = frame.framebuffer.pixels[pixel_index];
                const auto byte_index = pixel_index * 3U;
                framebuffer_rgb[byte_index] = static_cast<std::byte>(
                    (pixel >> 16U) & 0xFFU);
                framebuffer_rgb[byte_index + 1U] = static_cast<std::byte>(
                    (pixel >> 8U) & 0xFFU);
                framebuffer_rgb[byte_index + 2U] = static_cast<std::byte>(
                    pixel & 0xFFU);
            }
            framebuffer_output.write(
                reinterpret_cast<const char*>(framebuffer_rgb.data()),
                static_cast<std::streamsize>(framebuffer_rgb.size()));
        }
        if (audio_output) {
            if (index == 1) {
                audio_sample_rate = frame.audio.sample_rate;
            } else if (frame.audio.sample_rate != audio_sample_rate) {
                std::cerr << plan.label
                          << " audio sample rate changed on frame "
                          << index << ".\n";
                return EXIT_FAILURE;
            }
            if ((frame.audio.interleaved_stereo.size() % 2U) != 0U) {
                std::cerr << plan.label
                          << " audio is not stereo on frame "
                          << index << ".\n";
                return EXIT_FAILURE;
            }
            for (const float sample : frame.audio.interleaved_stereo) {
                const auto pcm_sample = static_cast<std::int16_t>(
                    sample * 32768.0F);
                audio_output.write(
                    reinterpret_cast<const char*>(&pcm_sample),
                    sizeof(pcm_sample));
                audio_data_bytes += sizeof(pcm_sample);
            }
        }
        if (!output) {
            std::cerr << "Could not write " << plan.label << " frame "
                      << index
                      << ".\n";
            return EXIT_FAILURE;
        }
        if (!work_ram_path.empty() && !work_ram_output) {
            std::cerr << "Could not write " << plan.label
                      << " work RAM on frame "
                      << index << ".\n";
            return EXIT_FAILURE;
        }
        if (!framebuffer_path.empty() && !framebuffer_output) {
            std::cerr << "Could not write " << plan.label
                      << " framebuffer on frame "
                      << index << ".\n";
            return EXIT_FAILURE;
        }
        if (!audio_path.empty() && !audio_output) {
            std::cerr << "Could not write " << plan.label
                      << " audio on frame "
                      << index << ".\n";
            return EXIT_FAILURE;
        }
    }

    output.flush();
    output.close();
    if (!output) {
        std::cerr << "Could not finalize " << plan.label << " recording.\n";
        return EXIT_FAILURE;
    }
    if (!work_ram_path.empty()) {
        work_ram_output.flush();
        work_ram_output.close();
        if (!work_ram_output) {
            std::cerr << "Could not finalize work-RAM recording.\n";
            return EXIT_FAILURE;
        }
    }
    if (!framebuffer_path.empty()) {
        framebuffer_output.flush();
        framebuffer_output.close();
        if (!framebuffer_output) {
            std::cerr << "Could not finalize framebuffer recording.\n";
            return EXIT_FAILURE;
        }
    }
    if (!audio_path.empty()) {
        audio_output.seekp(0);
        if (!write_pcm16_wave_header(
                audio_output,
                static_cast<std::uint32_t>(audio_sample_rate),
                audio_data_bytes)) {
            std::cerr << "Could not finalize " << plan.label
                      << " audio header.\n";
            return EXIT_FAILURE;
        }
        audio_output.flush();
        audio_output.close();
        if (!audio_output) {
            std::cerr << "Could not finalize " << plan.label
                      << " audio recording.\n";
            return EXIT_FAILURE;
        }
    }
    if (!commit_artifact(
            temporary_path,
            path,
            std::string(plan.label) + " recording")) {
        return EXIT_FAILURE;
    }
    if (!work_ram_path.empty()
        && !commit_artifact(
            work_ram_temporary_path,
            work_ram_path,
            "work-RAM recording")) {
        return EXIT_FAILURE;
    }
    if (!framebuffer_path.empty()
        && !commit_artifact(
            framebuffer_temporary_path,
            framebuffer_path,
            "framebuffer recording")) {
        return EXIT_FAILURE;
    }
    if (!audio_path.empty()
        && !commit_artifact(
            audio_temporary_path,
            audio_path,
            std::string(plan.label) + " audio")) {
        return EXIT_FAILURE;
    }

    if (!table_vram_census_path.empty()
        && !table_vram_census.write(table_vram_census_path, core_revision)) {
        return EXIT_FAILURE;
    }

    std::cout << plan.label << " recording complete: "
              << plan.frame_count << " frames\n"
              << "Output: " << path.string() << '\n';
    if (!work_ram_patches.empty()) {
        std::cout << "Work-RAM patches applied: "
                  << work_ram_patches.size() << '\n';
    }
    if (!work_ram_path.empty()) {
        std::cout << "Work RAM: " << work_ram_path.string() << '\n';
    }
    if (!framebuffer_path.empty()) {
        std::cout << "Framebuffer: " << framebuffer_path.string() << '\n';
    }
    if (!audio_path.empty()) {
        std::cout << "Audio: " << audio_path.string() << '\n';
    }
    return EXIT_SUCCESS;
}

int run_backup_ram_reload_test(
    PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& backup_path)
{
    const auto backup_size = runtime.backup_ram_size();
    if (backup_size == 0U) {
        std::cerr << "The emulator did not expose backup RAM for testing.\n";
        return EXIT_FAILURE;
    }

    std::vector<std::byte> expected(backup_size);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        expected[index] = static_cast<std::byte>(
            (index * 37U + index / 7U + 0x5AU) & 0xFFU);
    }
    if (!runtime.import_backup_ram(expected)
        || !save_backup_ram(runtime, backup_path)) {
        return EXIT_FAILURE;
    }

    const auto reload_result = runtime.load_rom(rom_path);
    if (!reload_result.loaded) {
        std::cerr << "Could not reload the ROM during backup-RAM test: "
                  << reload_result.message << '\n';
        return EXIT_FAILURE;
    }
    if (!load_backup_ram(runtime, backup_path)) {
        return EXIT_FAILURE;
    }

    const auto actual = runtime.export_backup_ram();
    if (actual != expected) {
        std::cerr << "Backup RAM changed across the full reload cycle.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Backup-RAM reload test passed:\n"
              << "Bytes:  " << actual.size() << '\n'
              << "File:   " << backup_path.string() << '\n'
              << "SHA-256: " << sha256(actual) << '\n';
    return EXIT_SUCCESS;
}

int run_smoke_test(PceRuntime& runtime)
{
    EmulatedFrame frame;
    for (int index = 0; index < kSmokeTestFrames; ++index) {
        frame = runtime.run_frame({});
        if (frame.frame_number == 0U || frame.framebuffer.pixels.empty()) {
            std::cerr << "Emulation smoke test failed on frame "
                      << index + 1 << ".\n";
            return EXIT_FAILURE;
        }
    }

    const auto state = runtime.serialize();
    if (state.core_state.empty()) {
        std::cerr << "Savestate smoke test could not capture frame "
                  << frame.frame_number << ".\n";
        return EXIT_FAILURE;
    }

    const auto expected = runtime.run_frame({});
    const std::vector expected_pixels(
        expected.framebuffer.pixels.begin(),
        expected.framebuffer.pixels.end());
    const auto expected_audio_samples =
        expected.audio.interleaved_stereo.size();
    const auto expected_audio_rate = expected.audio.sample_rate;
    if (!runtime.deserialize(state)) {
        std::cerr << "Savestate smoke test could not restore the capture.\n";
        return EXIT_FAILURE;
    }
    const auto replayed = runtime.run_frame({});
    const bool pixels_match = std::ranges::equal(
        replayed.framebuffer.pixels,
        expected_pixels);
    // Beetle's raw savestate omits frontend audio-resampler history. The
    // emulated PSG state and batch timing restore, but the first batch's
    // sample values are not required to equal an uninterrupted run.
    const bool audio_timing_matches =
        replayed.audio.sample_rate == expected_audio_rate
        && replayed.audio.interleaved_stereo.size()
            == expected_audio_samples;
    if (replayed.frame_number != expected.frame_number
        || replayed.framebuffer.width != expected.framebuffer.width
        || replayed.framebuffer.height != expected.framebuffer.height
        || !pixels_match
        || !audio_timing_matches) {
        std::cerr
            << "Savestate replay diverged on the first restored frame:\n"
            << "  frame numbers: " << expected.frame_number << " / "
            << replayed.frame_number << '\n'
            << "  framebuffer geometry: " << expected.framebuffer.width
            << 'x' << expected.framebuffer.height << " / "
            << replayed.framebuffer.width << 'x'
            << replayed.framebuffer.height << '\n'
            << "  framebuffer pixels match: "
            << (pixels_match ? "yes" : "no") << '\n'
            << "  audio samples: " << expected_audio_samples << " / "
            << replayed.audio.interleaved_stereo.size() << '\n'
            << "  audio timing matches: "
            << (audio_timing_matches ? "yes" : "no")
            << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Emulation smoke test passed:\n"
              << "Frames:      " << replayed.frame_number << '\n'
              << "Framebuffer: " << frame.framebuffer.width << 'x'
              << frame.framebuffer.height << " ("
              << frame.framebuffer.pixels.size() << " ARGB pixels)\n"
              << "Audio:       " << frame.audio.sample_rate << " Hz, "
              << replayed.audio.interleaved_stereo.size()
              << " samples in final batch\n"
              << "Savestate:   deterministic video/audio-timing round trip\n";
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
