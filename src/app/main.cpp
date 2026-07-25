// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kDisplayAspectNumerator = 4U;
constexpr std::uint32_t kDisplayAspectDenominator = 3U;
constexpr double kEmulationFramesPerSecond = 59.8261;
constexpr int kSmokeTestFrames = 10;
constexpr int kCanonicalRecordingFrames = 600;
constexpr int kMaximumAudioLatencyMilliseconds = 150;
const std::filesystem::path kBackupRamPath =
    std::filesystem::path("portable-data") / "devils-crush.sav";
const std::filesystem::path kQuickStatePath =
    std::filesystem::path("portable-data") / "devils-crush.state";
const std::filesystem::path kGamepadMappingsPath =
    std::filesystem::path("portable-data") / "gamecontrollerdb.txt";
const std::filesystem::path kCanonicalRecordingPath =
    std::filesystem::path("portable-data") / "canonical-v1.csv";
const std::filesystem::path kBackupRamTestPath =
    std::filesystem::path("portable-data") / "backup-ram-test.sav";
constexpr std::uintmax_t kMaximumSaveStateFileSize =
    64U * 1024U * 1024U + 4096U;

using devils_crush::emulation::AudioView;
using devils_crush::emulation::Button;
using devils_crush::emulation::EmulatedFrame;
using devils_crush::emulation::FramebufferView;
using devils_crush::emulation::HookMode;
using devils_crush::emulation::InputFrame;
using devils_crush::emulation::PceRuntime;
using devils_crush::emulation::RoutineEntry;
using devils_crush::emulation::RoutineKey;

struct CommandLine {
    std::filesystem::path rom_path = "Devil's Crush.pce";
    bool validate_only = false;
    bool smoke_test = false;
    bool record_canonical = false;
    std::filesystem::path canonical_path = kCanonicalRecordingPath;
    std::filesystem::path canonical_work_ram_path;
    std::filesystem::path canonical_framebuffer_path;
    std::filesystem::path canonical_audio_path;
    std::optional<RoutineKey> pass_through_hook;
    std::optional<RoutineKey> observe_hook;
    std::filesystem::path routine_trace_path;
    bool backup_ram_test = false;
    std::filesystem::path backup_ram_test_path = kBackupRamTestPath;
    bool show_help = false;
    std::string error;
};

[[nodiscard]] std::optional<RoutineKey> parse_routine_key(
    const std::string_view text)
{
    const auto separator = text.find(':');
    if (separator == std::string_view::npos
        || text.find(':', separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto parse_hex = [](std::string_view component)
        -> std::optional<std::uint32_t> {
        if (component.starts_with("0x")
            || component.starts_with("0X")) {
            component.remove_prefix(2U);
        }
        if (component.empty()) {
            return std::nullopt;
        }

        std::uint32_t value = 0;
        const auto result = std::from_chars(
            component.data(),
            component.data() + component.size(),
            value,
            16);
        if (result.ec != std::errc{}
            || result.ptr != component.data() + component.size()) {
            return std::nullopt;
        }
        return value;
    };

    const auto bank = parse_hex(text.substr(0, separator));
    const auto pc = parse_hex(text.substr(separator + 1U));
    if (!bank || !pc
        || *bank > std::numeric_limits<std::uint8_t>::max()
        || *pc > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }

    return RoutineKey{
        .physical_bank = static_cast<std::uint8_t>(*bank),
        .logical_pc = static_cast<std::uint16_t>(*pc),
    };
}

[[nodiscard]] CommandLine parse_command_line(const int argc, char* argv[])
{
    CommandLine options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else if (argument == "--rom") {
            if (index + 1 >= argc) {
                options.error = "--rom requires a path.";
                return options;
            }
            options.rom_path = argv[++index];
        } else if (argument == "--validate-rom") {
            options.validate_only = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.rom_path = argv[++index];
            }
        } else if (argument == "--smoke-test") {
            options.smoke_test = true;
        } else if (argument == "--record-canonical") {
            options.record_canonical = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.canonical_path = argv[++index];
            }
        } else if (argument == "--canonical-work-ram") {
            if (index + 1 >= argc) {
                options.error = "--canonical-work-ram requires a path.";
                return options;
            }
            options.canonical_work_ram_path = argv[++index];
        } else if (argument == "--canonical-framebuffer") {
            if (index + 1 >= argc) {
                options.error = "--canonical-framebuffer requires a path.";
                return options;
            }
            options.canonical_framebuffer_path = argv[++index];
        } else if (argument == "--canonical-audio") {
            if (index + 1 >= argc) {
                options.error = "--canonical-audio requires a path.";
                return options;
            }
            options.canonical_audio_path = argv[++index];
        } else if (argument == "--pass-through-hook") {
            if (index + 1 >= argc) {
                options.error =
                    "--pass-through-hook requires BANK:PC.";
                return options;
            }
            options.pass_through_hook =
                parse_routine_key(argv[++index]);
            if (!options.pass_through_hook) {
                options.error =
                    "--pass-through-hook requires hexadecimal BANK:PC.";
                return options;
            }
        } else if (argument == "--observe-hook") {
            if (index + 1 >= argc) {
                options.error = "--observe-hook requires BANK:PC.";
                return options;
            }
            options.observe_hook = parse_routine_key(argv[++index]);
            if (!options.observe_hook) {
                options.error =
                    "--observe-hook requires hexadecimal BANK:PC.";
                return options;
            }
        } else if (argument == "--routine-trace") {
            if (index + 1 >= argc) {
                options.error = "--routine-trace requires a path.";
                return options;
            }
            options.routine_trace_path = argv[++index];
        } else if (argument == "--backup-ram-test") {
            options.backup_ram_test = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.backup_ram_test_path = argv[++index];
            }
        } else {
            options.error = "Unknown option: " + std::string(argument);
            return options;
        }
    }

    const int operation_count =
        static_cast<int>(options.smoke_test)
        + static_cast<int>(options.record_canonical)
        + static_cast<int>(options.backup_ram_test);
    if (operation_count > 1) {
        options.error =
            "Only one test or recording operation may be selected.";
    } else if ((!options.canonical_work_ram_path.empty()
                || !options.canonical_framebuffer_path.empty()
                || !options.canonical_audio_path.empty())
               && !options.record_canonical) {
        options.error =
            "--canonical-work-ram, --canonical-framebuffer, and "
            "--canonical-audio require --record-canonical.";
    } else if (options.pass_through_hook && options.observe_hook) {
        options.error =
            "Only one execution hook mode may be selected.";
    } else if ((options.pass_through_hook || options.observe_hook)
               && !options.record_canonical) {
        options.error =
            "Execution hooks currently require "
            "--record-canonical.";
    } else if (
        options.observe_hook.has_value()
        != !options.routine_trace_path.empty()) {
        options.error =
            "--observe-hook and --routine-trace must be used together.";
    }
    return options;
}

void print_usage()
{
    std::cout
        << "Devil's Crush PC port - Milestone 1\n\n"
        << "Usage:\n"
        << "  devils-crush [--rom <path>]\n"
        << "  devils-crush --validate-rom [path]\n"
        << "  devils-crush --smoke-test [--rom <path>]\n"
        << "  devils-crush --record-canonical [output.csv]\n"
           "      [--canonical-work-ram <output.bin>]\n"
           "      [--canonical-framebuffer <output.rgb>]\n"
           "      [--canonical-audio <output.wav>]\n"
           "      [--pass-through-hook <bank:pc>]\n"
           "      [--observe-hook <bank:pc> --routine-trace <output.csv>]\n"
           "      [--rom <path>]\n"
        << "  devils-crush --backup-ram-test [output.sav] [--rom <path>]\n"
        << "  devils-crush --help\n\n"
        << "Controls:\n"
        << "  Z / Left Shift    PCE button I\n"
        << "  X / Right Shift   PCE button II\n"
        << "  Backspace         Select\n"
        << "  Enter             Run\n"
        << "  Arrow keys        Direction pad\n"
        << "  Gamepad           First connected SDL gamepad\n"
        << "  F5                Save quick state\n"
        << "  F9                Restore quick state\n"
        << "  F10               Reload custom gamepad mappings\n"
        << "  F11               Toggle borderless fullscreen\n"
        << "  Escape            Exit\n";
}

void print_validation(const devils_crush::rom::ValidationResult& result)
{
    auto& output = result.supported() ? std::cout : std::cerr;
    output << result.message << '\n';
    if (result.size != 0U) {
        output << "Size:    " << result.size << " bytes\n";
    }
    if (!result.digests.md5.empty()) {
        output << "MD5:     " << result.digests.md5 << '\n';
    }
    if (!result.digests.sha256.empty()) {
        output << "SHA-256: " << result.digests.sha256 << '\n';
    }
}

[[nodiscard]] std::string path_to_utf8(
    const std::filesystem::path& path)
{
    const auto encoded = path.u8string();
    return {
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size(),
    };
}

[[nodiscard]] bool load_backup_ram(
    PceRuntime& runtime,
    const std::filesystem::path& path)
{
    const auto expected_size = runtime.backup_ram_size();
    if (expected_size == 0U) {
        std::cerr << "The emulator did not expose backup RAM.\n";
        return false;
    }

    auto result =
        devils_crush::backup::load_exact(path, expected_size);
    if (result.status == devils_crush::backup::LoadStatus::not_found) {
        return true;
    }
    if (!result.loaded()) {
        std::cerr << "Could not load backup RAM: " << result.message
                  << " (" << path.string() << ")\n";
        return false;
    }
    if (!runtime.import_backup_ram(result.bytes)) {
        std::cerr << "The emulator rejected the backup RAM data.\n";
        return false;
    }

    std::cout << "Loaded backup RAM: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool save_backup_ram(
    const PceRuntime& runtime,
    const std::filesystem::path& path)
{
    const auto bytes = runtime.export_backup_ram();
    if (bytes.empty()) {
        std::cerr << "The emulator did not return backup RAM to save.\n";
        return false;
    }

    const auto result = devils_crush::backup::save_atomic(path, bytes);
    if (!result.saved) {
        std::cerr << "Could not save backup RAM: " << result.message
                  << " (" << path.string() << ")\n";
        return false;
    }

    std::cout << "Saved backup RAM: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool save_quick_state(
    const PceRuntime& runtime,
    const std::filesystem::path& path)
{
    const auto state = runtime.serialize();
    const auto runtime_info = runtime.info();
    if (state.core_state.empty() || runtime_info.upstream_revision.empty()) {
        std::cerr << "The emulator did not return a savestate.\n";
        return false;
    }

    std::vector<std::byte> bytes;
    try {
        bytes = devils_crush::state::encode({
            .format_version =
                devils_crush::state::kSaveStateFormatVersion,
            .frame_number = state.frame_number,
            .rom_sha256 =
                std::string(devils_crush::rom::kSupportedSha256),
            .core_revision = runtime_info.upstream_revision,
            .core_state = state.core_state,
        });
    } catch (const std::exception& exception) {
        std::cerr << "Could not encode quick state: " << exception.what()
                  << '\n';
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        std::cerr << "Could not create save directory: " << error.message()
                  << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    {
        std::ofstream output(
            temporary_path,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) {
            std::cerr << "Could not write quick-state file: "
                      << temporary_path.string() << '\n';
            return false;
        }
    }

    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit quick-state file; Windows error "
                  << GetLastError() << ".\n";
        return false;
    }

    std::cout << "Saved quick state at frame " << state.frame_number << ": "
              << path.string() << '\n';
    return true;
}

[[nodiscard]] bool load_quick_state(
    PceRuntime& runtime,
    const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        std::cerr << (error ? "Could not inspect quick-state file: "
                            + error.message()
                           : "No quick-state file exists.")
                  << '\n';
        return false;
    }
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size == 0U
        || file_size > kMaximumSaveStateFileSize) {
        std::cerr << "The quick-state file size is invalid.\n";
        return false;
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
    std::ifstream input(path, std::ios::binary);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input
        || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        std::cerr << "Could not read quick-state file: " << path.string()
                  << '\n';
        return false;
    }

    auto decoded = devils_crush::state::decode(bytes);
    if (!decoded.decoded) {
        std::cerr << "Could not restore quick state: " << decoded.message
                  << '\n';
        return false;
    }
    const auto runtime_info = runtime.info();
    if (decoded.container.rom_sha256
        != devils_crush::rom::kSupportedSha256) {
        std::cerr << "The quick state belongs to a different ROM.\n";
        return false;
    }
    if (decoded.container.core_revision
        != runtime_info.upstream_revision) {
        std::cerr << "The quick state belongs to a different core revision.\n";
        return false;
    }

    const auto frame_number = decoded.container.frame_number;
    if (!runtime.deserialize({
            .core_state = std::move(decoded.container.core_state),
            .frame_number = frame_number,
        })) {
        std::cerr << "The emulator rejected the quick-state payload.\n";
        return false;
    }

    std::cout << "Restored quick state at frame " << frame_number << ": "
              << path.string() << '\n';
    return true;
}

bool report_sdl_error(const std::string_view operation)
{
    std::cerr << operation << ": " << SDL_GetError() << '\n';
    return false;
}

[[nodiscard]] bool configure_texture(
    SDL_Texture* texture,
    const SDL_ScaleMode scale_mode)
{
    if (!SDL_SetTextureScaleMode(texture, scale_mode)) {
        return report_sdl_error("SDL_SetTextureScaleMode failed");
    }
    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE)) {
        return report_sdl_error("SDL_SetTextureBlendMode failed");
    }
    return true;
}

struct SourceTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

void destroy_source(SourceTexture& source)
{
    if (source.texture != nullptr) {
        SDL_DestroyTexture(source.texture);
    }
    source = {};
}

[[nodiscard]] bool upload_framebuffer(
    SDL_Renderer* renderer,
    const FramebufferView& framebuffer,
    SourceTexture& source)
{
    if (framebuffer.width <= 0 || framebuffer.height <= 0
        || framebuffer.pitch_pixels < framebuffer.width) {
        std::cerr << "The emulator returned invalid framebuffer geometry.\n";
        return false;
    }

    const auto pitch = static_cast<std::size_t>(framebuffer.pitch_pixels);
    const auto height = static_cast<std::size_t>(framebuffer.height);
    const auto width = static_cast<std::size_t>(framebuffer.width);
    const auto required_pixels = (height - 1U) * pitch + width;
    if (framebuffer.pixels.size() < required_pixels
        || pitch
            > static_cast<std::size_t>(
                std::numeric_limits<int>::max()
                / static_cast<int>(sizeof(std::uint32_t)))) {
        std::cerr << "The emulator returned an incomplete framebuffer.\n";
        return false;
    }

    if (source.texture == nullptr || source.width != framebuffer.width
        || source.height != framebuffer.height) {
        destroy_source(source);
        source.texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            framebuffer.width,
            framebuffer.height);
        if (source.texture == nullptr) {
            return report_sdl_error("SDL_CreateTexture (source) failed");
        }
        source.width = framebuffer.width;
        source.height = framebuffer.height;
        if (!configure_texture(source.texture, SDL_SCALEMODE_NEAREST)) {
            destroy_source(source);
            return false;
        }
    }

    const auto pitch_bytes = static_cast<int>(
        pitch * static_cast<std::size_t>(sizeof(std::uint32_t)));
    if (!SDL_UpdateTexture(
            source.texture,
            nullptr,
            framebuffer.pixels.data(),
            pitch_bytes)) {
        return report_sdl_error("SDL_UpdateTexture failed");
    }
    return true;
}

struct StageTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

void destroy_stage(StageTexture& stage)
{
    if (stage.texture != nullptr) {
        SDL_DestroyTexture(stage.texture);
    }
    stage = {};
}

[[nodiscard]] bool ensure_stage(
    SDL_Renderer* renderer,
    const devils_crush::render::ScaleLayout& layout,
    StageTexture& stage)
{
    if (stage.texture != nullptr && stage.width == layout.stage_width
        && stage.height == layout.stage_height) {
        return true;
    }

    destroy_stage(stage);
    stage.texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_TARGET,
        layout.stage_width,
        layout.stage_height);
    if (stage.texture == nullptr) {
        return report_sdl_error("SDL_CreateTexture (integer stage) failed");
    }
    stage.width = layout.stage_width;
    stage.height = layout.stage_height;

    if (!configure_texture(stage.texture, SDL_SCALEMODE_LINEAR)) {
        destroy_stage(stage);
        return false;
    }
    return true;
}

[[nodiscard]] bool present_frame(
    SDL_Renderer* renderer,
    const SourceTexture& source,
    StageTexture& stage)
{
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &output_width, &output_height)) {
        return report_sdl_error("SDL_GetRenderOutputSize failed");
    }
    if (output_width <= 0 || output_height <= 0) {
        SDL_Delay(16);
        return true;
    }

    if (!SDL_SetRenderDrawColor(renderer, 8, 8, 12, 255)
        || !SDL_RenderClear(renderer)) {
        return report_sdl_error("SDL_RenderClear failed");
    }
    if (source.texture == nullptr) {
        return SDL_RenderPresent(renderer)
            || report_sdl_error("SDL_RenderPresent failed");
    }

    const auto layout = devils_crush::render::calculate_scale_layout(
        output_width,
        output_height,
        source.width,
        source.height,
        kDisplayAspectNumerator,
        kDisplayAspectDenominator);
    if (!layout.valid() || !ensure_stage(renderer, layout, stage)) {
        return false;
    }

    if (!SDL_SetRenderTarget(renderer, stage.texture)) {
        return report_sdl_error("SDL_SetRenderTarget (integer stage) failed");
    }
    if (!SDL_RenderTexture(renderer, source.texture, nullptr, nullptr)) {
        return report_sdl_error("SDL_RenderTexture (nearest pass) failed");
    }
    if (!SDL_SetRenderTarget(renderer, nullptr)) {
        return report_sdl_error("SDL_SetRenderTarget (window) failed");
    }

    if (!SDL_SetRenderDrawColor(renderer, 8, 8, 12, 255)
        || !SDL_RenderClear(renderer)) {
        return report_sdl_error("SDL_RenderClear failed");
    }

    const SDL_FRect destination{
        static_cast<float>(layout.destination.x),
        static_cast<float>(layout.destination.y),
        static_cast<float>(layout.destination.width),
        static_cast<float>(layout.destination.height),
    };
    if (!SDL_RenderTexture(renderer, stage.texture, nullptr, &destination)) {
        return report_sdl_error("SDL_RenderTexture (sharp resolve) failed");
    }
    if (!SDL_RenderPresent(renderer)) {
        return report_sdl_error("SDL_RenderPresent failed");
    }
    return true;
}

struct AudioOutput {
    SDL_AudioStream* stream = nullptr;
    int sample_rate = 0;
    bool resumed = false;
    bool available = false;
    bool open_failure_reported = false;
    std::uint64_t latency_resets = 0;
};

void reset_audio_stream(AudioOutput& output)
{
    if (output.stream != nullptr) {
        SDL_DestroyAudioStream(output.stream);
    }
    output.stream = nullptr;
    output.sample_rate = 0;
    output.resumed = false;
}

void destroy_audio(AudioOutput& output)
{
    reset_audio_stream(output);
    output = {};
}

[[nodiscard]] bool open_audio_stream(
    const int sample_rate,
    AudioOutput& output)
{
    SDL_AudioSpec specification{};
    specification.format = SDL_AUDIO_F32;
    specification.channels = 2;
    specification.freq = sample_rate;
    output.stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &specification,
        nullptr,
        nullptr);
    if (output.stream == nullptr) {
        if (!output.open_failure_reported) {
            std::cerr << "Audio output unavailable; retrying: "
                      << SDL_GetError() << '\n';
            output.open_failure_reported = true;
        }
        return false;
    }

    output.sample_rate = sample_rate;
    output.open_failure_reported = false;
    return true;
}

void recover_audio_stream(
    AudioOutput& output,
    const std::string_view operation)
{
    std::cerr << operation << "; audio output will reconnect: "
              << SDL_GetError() << '\n';
    reset_audio_stream(output);
}

[[nodiscard]] bool queue_audio(
    const AudioView& audio,
    AudioOutput& output)
{
    if (!output.available || audio.interleaved_stereo.empty()) {
        return true;
    }
    if (audio.sample_rate <= 0
        || audio.interleaved_stereo.size()
            > static_cast<std::size_t>(
                std::numeric_limits<int>::max()
                / static_cast<int>(sizeof(float)))) {
        std::cerr << "The emulator returned an invalid audio batch.\n";
        return false;
    }

    if (output.stream != nullptr
        && output.sample_rate != audio.sample_rate) {
        reset_audio_stream(output);
    }
    if (output.stream == nullptr
        && !open_audio_stream(audio.sample_rate, output)) {
        return true;
    }

    const auto queued_bytes = SDL_GetAudioStreamQueued(output.stream);
    if (queued_bytes < 0) {
        recover_audio_stream(
            output,
            "SDL_GetAudioStreamQueued failed");
        return true;
    }
    const auto maximum_queued_bytes =
        static_cast<std::int64_t>(audio.sample_rate) * 2
        * static_cast<std::int64_t>(sizeof(float))
        * kMaximumAudioLatencyMilliseconds / 1000;
    if (static_cast<std::int64_t>(queued_bytes) > maximum_queued_bytes) {
        if (!SDL_ClearAudioStream(output.stream)) {
            recover_audio_stream(
                output,
                "SDL_ClearAudioStream failed");
            return true;
        }
        ++output.latency_resets;
        std::cerr << "Audio queue exceeded "
                  << kMaximumAudioLatencyMilliseconds
                  << " ms; dropped stale audio (recovery "
                  << output.latency_resets << ").\n";
    }

    const auto byte_count = static_cast<int>(
        audio.interleaved_stereo.size() * sizeof(float));
    if (!SDL_PutAudioStreamData(
            output.stream,
            audio.interleaved_stereo.data(),
            byte_count)) {
        recover_audio_stream(output, "SDL_PutAudioStreamData failed");
        return true;
    }
    if (!output.resumed) {
        if (!SDL_ResumeAudioStreamDevice(output.stream)) {
            recover_audio_stream(
                output,
                "SDL_ResumeAudioStreamDevice failed");
            return true;
        }
        output.resumed = true;
    }
    return true;
}

void handle_audio_device_event(
    const SDL_AudioDeviceEvent& event,
    AudioOutput& output)
{
    if (event.recording || output.stream == nullptr) {
        return;
    }
    const auto stream_device = SDL_GetAudioStreamDevice(output.stream);
    if (stream_device != event.which) {
        return;
    }

    if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) {
        std::cerr << "Active audio device was removed; reconnecting.\n";
        reset_audio_stream(output);
    } else if (event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) {
        std::cerr << "Active audio device format changed; reconnecting.\n";
        reset_audio_stream(output);
    }
}

void set_button(
    std::uint16_t& mask,
    const Button button,
    const bool is_pressed)
{
    if (is_pressed) {
        mask |= static_cast<std::uint16_t>(button);
    }
}

[[nodiscard]] InputFrame read_input(SDL_Gamepad* gamepad)
{
    InputFrame input;
    const bool* keys = SDL_GetKeyboardState(nullptr);

    set_button(
        input.player_one,
        Button::one,
        keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_LSHIFT]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(
                    gamepad,
                    SDL_GAMEPAD_BUTTON_SOUTH)));
    set_button(
        input.player_one,
        Button::two,
        keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_RSHIFT]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(
                    gamepad,
                    SDL_GAMEPAD_BUTTON_EAST)));
    set_button(
        input.player_one,
        Button::select,
        keys[SDL_SCANCODE_BACKSPACE]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK)));
    set_button(
        input.player_one,
        Button::run,
        keys[SDL_SCANCODE_RETURN]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START)));
    set_button(
        input.player_one,
        Button::up,
        keys[SDL_SCANCODE_UP]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(
                    gamepad,
                    SDL_GAMEPAD_BUTTON_DPAD_UP)));
    set_button(
        input.player_one,
        Button::right,
        keys[SDL_SCANCODE_RIGHT]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(
                    gamepad,
                    SDL_GAMEPAD_BUTTON_DPAD_RIGHT)));
    set_button(
        input.player_one,
        Button::down,
        keys[SDL_SCANCODE_DOWN]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(
                    gamepad,
                    SDL_GAMEPAD_BUTTON_DPAD_DOWN)));
    set_button(
        input.player_one,
        Button::left,
        keys[SDL_SCANCODE_LEFT]
            || (gamepad != nullptr
                && SDL_GetGamepadButton(
                    gamepad,
                    SDL_GAMEPAD_BUTTON_DPAD_LEFT)));
    return input;
}

void log_opened_gamepad(SDL_Gamepad* gamepad)
{
    const char* name = SDL_GetGamepadName(gamepad);
    std::cout << "Using gamepad: "
              << (name != nullptr ? name : "unnamed SDL gamepad")
              << '\n';
}

[[nodiscard]] SDL_Gamepad* open_gamepad(
    const SDL_JoystickID gamepad_id)
{
    SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepad_id);
    if (gamepad == nullptr) {
        report_sdl_error("SDL_OpenGamepad failed");
        return nullptr;
    }
    log_opened_gamepad(gamepad);
    return gamepad;
}

[[nodiscard]] SDL_Gamepad* open_first_gamepad()
{
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr || count <= 0) {
        SDL_free(gamepads);
        return nullptr;
    }

    const auto gamepad_id = gamepads[0];
    SDL_free(gamepads);
    return open_gamepad(gamepad_id);
}

[[nodiscard]] bool load_gamepad_mappings(
    const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            std::cerr << "Could not inspect gamepad mapping file: "
                      << error.message() << '\n';
        }
        return false;
    }

    const auto path_utf8 = path_to_utf8(path);
    const auto mapping_count =
        SDL_AddGamepadMappingsFromFile(path_utf8.c_str());
    if (mapping_count < 0) {
        report_sdl_error("SDL_AddGamepadMappingsFromFile failed");
        return false;
    }
    std::cout << "Loaded " << mapping_count << " custom gamepad mapping(s): "
              << path.string() << '\n';
    return true;
}

void handle_gamepad_device_event(
    const SDL_GamepadDeviceEvent& event,
    SDL_Gamepad*& gamepad)
{
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (gamepad == nullptr) {
            gamepad = open_gamepad(event.which);
        }
        return;
    }
    if (gamepad == nullptr || SDL_GetGamepadID(gamepad) != event.which) {
        return;
    }

    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        std::cerr << "Active gamepad disconnected.\n";
        SDL_CloseGamepad(gamepad);
        gamepad = open_first_gamepad();
    } else if (event.type == SDL_EVENT_GAMEPAD_REMAPPED) {
        const char* name = SDL_GetGamepadName(gamepad);
        std::cout << "Gamepad mapping updated: "
                  << (name != nullptr ? name : "unnamed SDL gamepad")
                  << '\n';
    }
}

[[nodiscard]] bool consume_emulated_frame(
    SDL_Renderer* renderer,
    const EmulatedFrame& frame,
    SourceTexture& source,
    AudioOutput& audio)
{
    if (frame.frame_number == 0U || frame.framebuffer.pixels.empty()) {
        std::cerr << "Beetle PCE did not produce a framebuffer.\n";
        return false;
    }
    return upload_framebuffer(renderer, frame.framebuffer, source)
        && queue_audio(frame.audio, audio);
}

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

[[nodiscard]] std::string sha256(
    const std::span<const std::byte> bytes)
{
    return devils_crush::rom::compute_digests(bytes).sha256;
}

template <typename Value>
[[nodiscard]] std::string sha256_values(
    const std::span<const Value> values)
{
    return sha256(std::as_bytes(values));
}

bool write_pcm16_wave_header(
    std::ostream& output,
    const std::uint32_t sample_rate,
    const std::uint32_t data_bytes)
{
    constexpr char riff[] = {'R', 'I', 'F', 'F'};
    constexpr char wave[] = {'W', 'A', 'V', 'E'};
    constexpr char format[] = {'f', 'm', 't', ' '};
    constexpr char data[] = {'d', 'a', 't', 'a'};
    constexpr std::uint32_t format_size = 16U;
    constexpr std::uint16_t pcm_format = 1U;
    constexpr std::uint16_t channels = 2U;
    constexpr std::uint16_t bits_per_sample = 16U;
    constexpr std::uint16_t block_alignment =
        channels * bits_per_sample / 8U;
    const std::uint32_t riff_size = 36U + data_bytes;
    const std::uint32_t bytes_per_second =
        sample_rate * block_alignment;

    output.write(riff, sizeof(riff));
    output.write(
        reinterpret_cast<const char*>(&riff_size),
        sizeof(riff_size));
    output.write(wave, sizeof(wave));
    output.write(format, sizeof(format));
    output.write(
        reinterpret_cast<const char*>(&format_size),
        sizeof(format_size));
    output.write(
        reinterpret_cast<const char*>(&pcm_format),
        sizeof(pcm_format));
    output.write(
        reinterpret_cast<const char*>(&channels),
        sizeof(channels));
    output.write(
        reinterpret_cast<const char*>(&sample_rate),
        sizeof(sample_rate));
    output.write(
        reinterpret_cast<const char*>(&bytes_per_second),
        sizeof(bytes_per_second));
    output.write(
        reinterpret_cast<const char*>(&block_alignment),
        sizeof(block_alignment));
    output.write(
        reinterpret_cast<const char*>(&bits_per_sample),
        sizeof(bits_per_sample));
    output.write(data, sizeof(data));
    output.write(
        reinterpret_cast<const char*>(&data_bytes),
        sizeof(data_bytes));
    return static_cast<bool>(output);
}

[[nodiscard]] bool write_routine_trace(
    const std::filesystem::path& path,
    const RoutineKey routine,
    const std::span<const RoutineEntry> entries,
    const std::string_view core_revision)
{
    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create routine-trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create routine trace: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output
        << "# format=devils-crush-routine-entry-v1\n"
        << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
        << "# core_revision=" << core_revision << '\n'
        << "# physical_bank=" << std::uppercase << std::hex
        << std::setfill('0') << std::setw(2)
        << static_cast<unsigned>(routine.physical_bank) << '\n'
        << "# logical_pc=" << std::setw(4) << routine.logical_pc << '\n'
        << std::dec
        << "sequence,frame,master_cycle,physical_bank,logical_pc,"
           "a,x,y,sp,status,mpr0,mpr1,mpr2,mpr3,mpr4,mpr5,mpr6,mpr7\n";

    std::uint64_t sequence = 0;
    for (const auto& entry : entries) {
        const auto& registers = entry.registers;
        output << ++sequence << ',' << entry.frame_number << ','
               << entry.master_cycle << ',' << std::uppercase << std::hex
               << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(entry.routine.physical_bank)
               << ',' << std::setw(4) << entry.routine.logical_pc
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.accumulator)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.x)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.y)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.stack_pointer)
               << ',' << std::setw(2)
               << static_cast<unsigned>(registers.status);
        for (const auto mpr : registers.mpr) {
            output << ',' << std::setw(2)
                   << static_cast<unsigned>(mpr);
        }
        output << std::dec << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize routine trace.\n";
        return false;
    }
    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit routine trace; Windows error "
                  << GetLastError() << ".\n";
        return false;
    }

    std::cout << "Routine trace complete: " << entries.size()
              << " entries\nOutput: " << path.string() << '\n';
    return true;
}

int run_canonical_recording(
    PceRuntime& runtime,
    const std::filesystem::path& path,
    const std::filesystem::path& work_ram_path,
    const std::filesystem::path& framebuffer_path,
    const std::filesystem::path& audio_path,
    const std::string_view core_revision)
{
    if (runtime.work_ram_size() == 0U) {
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
        std::cerr << "Could not create canonical recording: "
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
        << "# format=devils-crush-canonical-v1\n"
        << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
        << "# core_revision=" << core_revision << '\n'
        << "# frame_count=" << kCanonicalRecordingFrames << '\n'
        << "# input_script=run:180-181,300-301;"
           "one:420-421;two:480-481;left+one:540-541\n"
        << "frame,input_p1,input_p2,work_ram_sha256,"
           "framebuffer_width,framebuffer_height,framebuffer_sha256,"
           "audio_sample_rate,audio_stereo_samples,audio_sha256\n";

    runtime.reset();
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    int audio_sample_rate = 0;
    std::uint32_t audio_data_bytes = 0U;
    std::vector<std::byte> framebuffer_rgb;
    for (int index = 1; index <= kCanonicalRecordingFrames; ++index) {
        const auto input = canonical_input_for_frame(index);
        const auto frame = runtime.run_frame(input);
        const auto work_ram = runtime.export_work_ram();
        if (frame.frame_number != static_cast<std::uint64_t>(index)
            || frame.framebuffer.pixels.empty()
            || frame.framebuffer.width <= 0
            || frame.framebuffer.height <= 0
            || work_ram.size() != runtime.work_ram_size()) {
            std::cerr << "Canonical recording failed on frame " << index
                      << ".\n";
            return EXIT_FAILURE;
        }

        output << frame.frame_number << ',' << std::hex
               << std::setfill('0') << std::setw(4) << input.player_one
               << ',' << std::setw(4) << input.player_two << std::dec
               << ',' << sha256(work_ram)
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
                    kCanonicalRecordingFrames);
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
                    << "Canonical framebuffer geometry changed on frame "
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
                std::cerr << "Canonical audio sample rate changed on frame "
                          << index << ".\n";
                return EXIT_FAILURE;
            }
            if ((frame.audio.interleaved_stereo.size() % 2U) != 0U) {
                std::cerr << "Canonical audio is not stereo on frame "
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
            std::cerr << "Could not write canonical frame " << index
                      << ".\n";
            return EXIT_FAILURE;
        }
        if (!work_ram_path.empty() && !work_ram_output) {
            std::cerr << "Could not write canonical work RAM on frame "
                      << index << ".\n";
            return EXIT_FAILURE;
        }
        if (!framebuffer_path.empty() && !framebuffer_output) {
            std::cerr << "Could not write canonical framebuffer on frame "
                      << index << ".\n";
            return EXIT_FAILURE;
        }
        if (!audio_path.empty() && !audio_output) {
            std::cerr << "Could not write canonical audio on frame "
                      << index << ".\n";
            return EXIT_FAILURE;
        }
    }

    output.flush();
    output.close();
    if (!output) {
        std::cerr << "Could not finalize canonical recording.\n";
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
            std::cerr << "Could not finalize canonical audio header.\n";
            return EXIT_FAILURE;
        }
        audio_output.flush();
        audio_output.close();
        if (!audio_output) {
            std::cerr << "Could not finalize canonical audio recording.\n";
            return EXIT_FAILURE;
        }
    }
    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit canonical recording; Windows error "
                  << GetLastError() << ".\n";
        return EXIT_FAILURE;
    }
    if (!work_ram_path.empty()
        && !MoveFileExW(
            work_ram_temporary_path.c_str(),
            work_ram_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit work-RAM recording; Windows error "
                  << GetLastError() << ".\n";
        return EXIT_FAILURE;
    }
    if (!framebuffer_path.empty()
        && !MoveFileExW(
            framebuffer_temporary_path.c_str(),
            framebuffer_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit framebuffer recording; Windows error "
                  << GetLastError() << ".\n";
        return EXIT_FAILURE;
    }
    if (!audio_path.empty()
        && !MoveFileExW(
            audio_temporary_path.c_str(),
            audio_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit canonical audio; Windows error "
                  << GetLastError() << ".\n";
        return EXIT_FAILURE;
    }

    std::cout << "Canonical recording complete: "
              << kCanonicalRecordingFrames << " frames\n"
              << "Output: " << path.string() << '\n';
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

int run_windows_host(PceRuntime& runtime)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        report_sdl_error("SDL_Init failed");
        return EXIT_FAILURE;
    }

    AudioOutput audio;
    audio.available = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!audio.available) {
        std::cerr << "Audio disabled: " << SDL_GetError() << '\n';
    }
    (void)load_gamepad_mappings(kGamepadMappingsPath);

    SDL_Window* window = SDL_CreateWindow(
        "Devil's Crush - emulated baseline",
        1280,
        720,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        report_sdl_error("SDL_CreateWindow failed");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        report_sdl_error("SDL_CreateRenderer failed");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const bool vsync_enabled = SDL_SetRenderVSync(renderer, 1);
    if (!vsync_enabled) {
        std::cerr << "VSync unavailable; using timer pacing: "
                  << SDL_GetError() << '\n';
    }
    if (!SDL_ShowWindow(window)) {
        report_sdl_error("SDL_ShowWindow failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Gamepad* gamepad = open_first_gamepad();
    SourceTexture source;
    StageTexture stage;

    using Clock = std::chrono::steady_clock;
    const auto frame_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / kEmulationFramesPerSecond));
    auto next_frame_time = Clock::now();

    bool running = true;
    bool host_ok = true;
    while (running && host_ok) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT
                || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_ESCAPE) {
                running = false;
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F5) {
                (void)save_quick_state(runtime, kQuickStatePath);
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F9) {
                if (load_quick_state(runtime, kQuickStatePath)) {
                    if (audio.stream != nullptr
                        && !SDL_ClearAudioStream(audio.stream)) {
                        report_sdl_error("SDL_ClearAudioStream failed");
                    }
                    next_frame_time = Clock::now();
                }
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F10) {
                if (load_gamepad_mappings(kGamepadMappingsPath)) {
                    if (gamepad != nullptr) {
                        SDL_CloseGamepad(gamepad);
                    }
                    gamepad = open_first_gamepad();
                }
            } else if (
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && event.key.key == SDLK_F11) {
                const bool fullscreen =
                    (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                if (!SDL_SetWindowFullscreen(window, !fullscreen)) {
                    report_sdl_error("SDL_SetWindowFullscreen failed");
                }
            } else if (
                event.type == SDL_EVENT_GAMEPAD_ADDED
                || event.type == SDL_EVENT_GAMEPAD_REMOVED
                || event.type == SDL_EVENT_GAMEPAD_REMAPPED) {
                handle_gamepad_device_event(event.gdevice, gamepad);
            } else if (
                event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED
                || event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) {
                handle_audio_device_event(event.adevice, audio);
            } else if (event.type == SDL_EVENT_RENDER_TARGETS_RESET) {
                destroy_stage(stage);
            } else if (event.type == SDL_EVENT_RENDER_DEVICE_RESET) {
                destroy_stage(stage);
                destroy_source(source);
                next_frame_time = Clock::now();
            }
        }

        auto now = Clock::now();
        int catch_up_frames = 0;
        constexpr int kMaximumCatchUpFrames = 4;
        while (running && host_ok && now >= next_frame_time
               && catch_up_frames < kMaximumCatchUpFrames) {
            const auto frame = runtime.run_frame(read_input(gamepad));
            host_ok =
                consume_emulated_frame(renderer, frame, source, audio);
            next_frame_time += frame_period;
            ++catch_up_frames;
            now = Clock::now();
        }
        if (catch_up_frames == kMaximumCatchUpFrames
            && now >= next_frame_time) {
            next_frame_time = now + frame_period;
        }

        if (running && host_ok) {
            host_ok = present_frame(renderer, source, stage);
        }
        if (!vsync_enabled) {
            SDL_Delay(1);
        }
    }

    destroy_audio(audio);
    destroy_stage(stage);
    destroy_source(source);
    if (gamepad != nullptr) {
        SDL_CloseGamepad(gamepad);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return host_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

int main(const int argc, char* argv[])
{
    const auto options = parse_command_line(argc, argv);
    if (!options.error.empty()) {
        std::cerr << options.error << "\n\n";
        print_usage();
        return EXIT_FAILURE;
    }
    if (options.show_help) {
        print_usage();
        return EXIT_SUCCESS;
    }

    const auto validation =
        devils_crush::rom::validate_file(options.rom_path);
    print_validation(validation);
    if (!validation.supported()) {
        return EXIT_FAILURE;
    }
    if (options.validate_only) {
        return EXIT_SUCCESS;
    }

    try {
        const auto runtime = devils_crush::emulation::create_pce_runtime();
        const auto runtime_info = runtime->info();
        std::cout << "Runtime:  " << runtime_info.name << '\n'
                  << "Revision: " << runtime_info.upstream_revision << '\n';

        const auto load_result = runtime->load_rom(options.rom_path);
        if (!load_result.loaded) {
            std::cerr << load_result.message << '\n';
            return EXIT_FAILURE;
        }
        std::cout << load_result.message << '\n';

        if (options.record_canonical) {
            std::uint64_t hook_hits = 0;
            std::vector<RoutineEntry> observed_entries;
            if (options.pass_through_hook
                && !runtime->install_execution_hook({
                    .routine = *options.pass_through_hook,
                    .mode = HookMode::pass_through,
                    .callback = [&hook_hits](const RoutineEntry&) {
                        ++hook_hits;
                    },
                })) {
                std::cerr << "Could not install the pass-through hook.\n";
                return EXIT_FAILURE;
            }
            if (options.observe_hook
                && !runtime->install_execution_hook({
                    .routine = *options.observe_hook,
                    .mode = HookMode::observe,
                    .callback = [&observed_entries](
                                    const RoutineEntry& entry) {
                        observed_entries.push_back(entry);
                    },
                })) {
                std::cerr << "Could not install the observe hook.\n";
                return EXIT_FAILURE;
            }

            const int result = run_canonical_recording(
                *runtime,
                options.canonical_path,
                options.canonical_work_ram_path,
                options.canonical_framebuffer_path,
                options.canonical_audio_path,
                runtime_info.upstream_revision);
            if (result != EXIT_SUCCESS) {
                return result;
            }
            if (options.pass_through_hook && hook_hits == 0U) {
                std::cerr
                    << "The pass-through hook address was never entered.\n";
                return EXIT_FAILURE;
            }
            if (options.pass_through_hook) {
                std::cout << "Pass-through hook entries: "
                          << hook_hits << '\n';
            }
            if (options.observe_hook
                && (observed_entries.empty()
                    || !write_routine_trace(
                        options.routine_trace_path,
                        *options.observe_hook,
                        observed_entries,
                        runtime_info.upstream_revision))) {
                if (observed_entries.empty()) {
                    std::cerr
                        << "The observe hook address was never entered.\n";
                }
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }
        if (options.backup_ram_test) {
            return run_backup_ram_reload_test(
                *runtime,
                options.rom_path,
                options.backup_ram_test_path);
        }
        if (options.smoke_test) {
            return run_smoke_test(*runtime);
        }

        (void)load_backup_ram(*runtime, kBackupRamPath);
        const int host_result = run_windows_host(*runtime);
        const bool save_succeeded =
            save_backup_ram(*runtime, kBackupRamPath);
        return host_result == EXIT_SUCCESS && save_succeeded
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    } catch (const std::exception& exception) {
        std::cerr << "Could not start the PCE runtime: " << exception.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}
