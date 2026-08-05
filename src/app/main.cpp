// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/app/native_replacements.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/display_config.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/render/backglass_image.hpp"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/pce_sprite_overlay.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"
#include "devils_crush/render/title_logo.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
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
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kDisplayAspectNumerator = 4U;
constexpr std::uint32_t kDisplayAspectDenominator = 3U;
constexpr std::array<std::uint8_t, 3> kPureBlack{0U, 0U, 0U};
constexpr std::array<std::uint8_t, 3> kDesktopFrameColor{
    8U,
    8U,
    12U};
constexpr double kEmulationFramesPerSecond = 59.8261;
constexpr int kSmokeTestFrames = 10;
constexpr int kCanonicalRecordingFrames = 600;
constexpr int kGameplayRecordingFrames = 1800;
constexpr int kStateCensusRecordingFrames = 3600;
constexpr int kMaximumAudioLatencyMilliseconds = 150;
const std::filesystem::path kBackupRamPath =
    std::filesystem::path("portable-data") / "devils-crush.sav";
const std::filesystem::path kQuickStatePath =
    std::filesystem::path("portable-data") / "devils-crush.state";
const std::filesystem::path kGamepadMappingsPath =
    std::filesystem::path("portable-data") / "gamecontrollerdb.txt";
const std::filesystem::path kCabinetConfigPath =
    std::filesystem::path("portable-data") / "cabinet.ini";
const std::filesystem::path kCanonicalRecordingPath =
    std::filesystem::path("portable-data") / "canonical-v1.csv";
const std::filesystem::path kGameplayRecordingPath =
    std::filesystem::path("portable-data") / "gameplay-v1.csv";
const std::filesystem::path kStateCensusRecordingPath =
    std::filesystem::path("portable-data") / "state-census-v1.csv";
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
using devils_crush::emulation::ObservedMemoryWrite;
using devils_crush::emulation::PceRuntime;
using devils_crush::emulation::RoutineCall;
using devils_crush::emulation::RoutineEntry;
using devils_crush::emulation::RoutineKey;
using devils_crush::emulation::RoutineReplacement;
using devils_crush::app::replace_native_gameplay_routine;

struct LogicalAddressRange {
    std::uint16_t first = 0;
    std::uint16_t last = 0;
};

struct RoutineReplayRequest {
    RoutineKey routine;
    std::uint64_t ordinal = 0;
    bool all_calls = false;
};

struct WorkRamPatch {
    int frame = 0;
    std::uint16_t logical_address = 0;
    std::uint8_t value = 0;

    [[nodiscard]] bool operator==(const WorkRamPatch&) const = default;
};

struct CommandLine {
    std::filesystem::path rom_path = "Devil's Crush.pce";
    bool validate_only = false;
    bool smoke_test = false;
    bool cabinet_smoke_test = false;
    bool cabinet = false;
    bool backglass_path_set = false;
    std::filesystem::path backglass_path;
    bool record_canonical = false;
    std::filesystem::path canonical_path = kCanonicalRecordingPath;
    bool record_gameplay = false;
    std::filesystem::path gameplay_path = kGameplayRecordingPath;
    bool record_state_census = false;
    std::filesystem::path state_census_path = kStateCensusRecordingPath;
    std::filesystem::path canonical_work_ram_path;
    std::filesystem::path canonical_framebuffer_path;
    std::filesystem::path canonical_audio_path;
    std::vector<RoutineKey> pass_through_hooks;
    std::vector<RoutineKey> observe_hooks;
    std::vector<RoutineReplayRequest> replace_hooks;
    std::filesystem::path routine_trace_path;
    std::filesystem::path routine_call_trace_path;
    std::vector<RoutineReplayRequest> routine_replays;
    std::vector<WorkRamPatch> work_ram_patches;
    std::vector<LogicalAddressRange> work_ram_write_ranges;
    std::filesystem::path work_ram_write_trace_path;
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

[[nodiscard]] std::optional<RoutineReplayRequest>
parse_routine_replay_request(const std::string_view text)
{
    const auto separator = text.find('@');
    if (separator == std::string_view::npos
        || text.find('@', separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto routine = parse_routine_key(text.substr(0, separator));
    const auto ordinal_text = text.substr(separator + 1U);
    std::uint64_t ordinal = 0;
    const auto result = std::from_chars(
        ordinal_text.data(),
        ordinal_text.data() + ordinal_text.size(),
        ordinal,
        10);
    if (!routine || ordinal_text.empty() || result.ec != std::errc{}
        || result.ptr != ordinal_text.data() + ordinal_text.size()
        || ordinal == 0U) {
        return std::nullopt;
    }

    return RoutineReplayRequest{
        .routine = *routine,
        .ordinal = ordinal,
    };
}

[[nodiscard]] std::optional<RoutineReplayRequest>
parse_replacement_request(const std::string_view text)
{
    constexpr std::string_view kAllSuffix = "@all";
    if (!text.ends_with(kAllSuffix)) {
        return parse_routine_replay_request(text);
    }
    const auto routine =
        parse_routine_key(text.substr(0U, text.size() - kAllSuffix.size()));
    if (!routine) {
        return std::nullopt;
    }
    return RoutineReplayRequest{
        .routine = *routine,
        .all_calls = true,
    };
}

[[nodiscard]] std::optional<WorkRamPatch>
parse_work_ram_patch(const std::string_view text)
{
    const auto first_separator = text.find(':');
    const auto second_separator =
        first_separator == std::string_view::npos
        ? std::string_view::npos
        : text.find(':', first_separator + 1U);
    if (first_separator == std::string_view::npos
        || second_separator == std::string_view::npos
        || text.find(':', second_separator + 1U) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto parse_number = [](
                                  std::string_view component,
                                  const int base)
        -> std::optional<std::uint32_t> {
        if (base == 16
            && (component.starts_with("0x")
                || component.starts_with("0X"))) {
            component.remove_prefix(2U);
        }
        if (component.empty()) {
            return std::nullopt;
        }
        std::uint32_t value = 0U;
        const auto result = std::from_chars(
            component.data(),
            component.data() + component.size(),
            value,
            base);
        if (result.ec != std::errc{}
            || result.ptr != component.data() + component.size()) {
            return std::nullopt;
        }
        return value;
    };

    const auto frame = parse_number(text.substr(0, first_separator), 10);
    const auto address = parse_number(
        text.substr(
            first_separator + 1U,
            second_separator - first_separator - 1U),
        16);
    const auto value =
        parse_number(text.substr(second_separator + 1U), 16);
    if (!frame || !address || !value || *frame == 0U
        || *frame > static_cast<std::uint32_t>(
                        std::numeric_limits<int>::max())
        || *address < 0x2000U || *address > 0x3FFFU
        || *value > std::numeric_limits<std::uint8_t>::max()) {
        return std::nullopt;
    }

    return WorkRamPatch{
        .frame = static_cast<int>(*frame),
        .logical_address = static_cast<std::uint16_t>(*address),
        .value = static_cast<std::uint8_t>(*value),
    };
}

[[nodiscard]] std::optional<LogicalAddressRange>
parse_logical_address_range(const std::string_view text)
{
    const auto separator = text.find('-');
    if (separator == std::string_view::npos
        || text.find('-', separator + 1U) != std::string_view::npos) {
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

    const auto first = parse_hex(text.substr(0, separator));
    const auto last = parse_hex(text.substr(separator + 1U));
    if (!first || !last || *first > *last
        || *last > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }

    return LogicalAddressRange{
        .first = static_cast<std::uint16_t>(*first),
        .last = static_cast<std::uint16_t>(*last),
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
        } else if (argument == "--cabinet-smoke-test") {
            options.cabinet_smoke_test = true;
        } else if (argument == "--cabinet") {
            options.cabinet = true;
        } else if (argument == "--backglass") {
            if (index + 1 >= argc) {
                options.error = "--backglass requires a path.";
                return options;
            }
            options.backglass_path_set = true;
            options.backglass_path = argv[++index];
        } else if (argument == "--record-canonical") {
            options.record_canonical = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.canonical_path = argv[++index];
            }
        } else if (argument == "--record-gameplay") {
            options.record_gameplay = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.gameplay_path = argv[++index];
            }
        } else if (argument == "--record-state-census") {
            options.record_state_census = true;
            const std::string_view next =
                index + 1 < argc ? std::string_view(argv[index + 1])
                                 : std::string_view{};
            if (!next.empty() && next.front() != '-') {
                options.state_census_path = argv[++index];
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
            const auto routine = parse_routine_key(argv[++index]);
            if (!routine) {
                options.error =
                    "--pass-through-hook requires hexadecimal BANK:PC.";
                return options;
            }
            if (std::find(
                    options.pass_through_hooks.begin(),
                    options.pass_through_hooks.end(),
                    *routine)
                    != options.pass_through_hooks.end()
                || std::find(
                       options.observe_hooks.begin(),
                       options.observe_hooks.end(),
                       *routine)
                    != options.observe_hooks.end()
                || std::find_if(
                       options.replace_hooks.begin(),
                       options.replace_hooks.end(),
                       [routine](
                           const RoutineReplayRequest& replacement) {
                           return replacement.routine == *routine;
                       })
                    != options.replace_hooks.end()) {
                options.error =
                    "Each execution-hook BANK:PC may be registered once.";
                return options;
            }
            options.pass_through_hooks.push_back(*routine);
        } else if (argument == "--observe-hook") {
            if (index + 1 >= argc) {
                options.error = "--observe-hook requires BANK:PC.";
                return options;
            }
            const auto routine = parse_routine_key(argv[++index]);
            if (!routine) {
                options.error =
                    "--observe-hook requires hexadecimal BANK:PC.";
                return options;
            }
            if (std::find(
                    options.pass_through_hooks.begin(),
                    options.pass_through_hooks.end(),
                    *routine)
                    != options.pass_through_hooks.end()
                || std::find(
                       options.observe_hooks.begin(),
                       options.observe_hooks.end(),
                       *routine)
                    != options.observe_hooks.end()) {
                options.error =
                    "Each execution-hook BANK:PC may be registered once.";
                return options;
            }
            options.observe_hooks.push_back(*routine);
        } else if (argument == "--replace-hook") {
            if (index + 1 >= argc) {
                options.error =
                    "--replace-hook requires BANK:PC@ORDINAL or "
                    "BANK:PC@all.";
                return options;
            }
            const auto request =
                parse_replacement_request(argv[++index]);
            if (!request) {
                options.error =
                    "--replace-hook requires hexadecimal BANK:PC and "
                    "a positive decimal ORDINAL or the suffix @all.";
                return options;
            }
            if (std::find(
                    options.pass_through_hooks.begin(),
                    options.pass_through_hooks.end(),
                    request->routine)
                    != options.pass_through_hooks.end()
                || std::find_if(
                       options.replace_hooks.begin(),
                       options.replace_hooks.end(),
                       [request](
                           const RoutineReplayRequest& existing) {
                           return existing.routine == request->routine;
                       })
                    != options.replace_hooks.end()) {
                options.error =
                    "Each replacement BANK:PC may be registered once "
                    "and cannot also be pass-through.";
                return options;
            }
            options.replace_hooks.push_back(*request);
        } else if (argument == "--routine-trace") {
            if (index + 1 >= argc) {
                options.error = "--routine-trace requires a path.";
                return options;
            }
            options.routine_trace_path = argv[++index];
        } else if (argument == "--routine-call-trace") {
            if (index + 1 >= argc) {
                options.error = "--routine-call-trace requires a path.";
                return options;
            }
            options.routine_call_trace_path = argv[++index];
        } else if (argument == "--routine-replay") {
            if (index + 1 >= argc) {
                options.error =
                    "--routine-replay requires BANK:PC@ORDINAL.";
                return options;
            }
            const auto request =
                parse_routine_replay_request(argv[++index]);
            if (!request) {
                options.error =
                    "--routine-replay requires hexadecimal BANK:PC and "
                    "a positive decimal ORDINAL.";
                return options;
            }
            const auto duplicate = std::find_if(
                options.routine_replays.begin(),
                options.routine_replays.end(),
                [request](const RoutineReplayRequest& existing) {
                    return existing.routine == request->routine;
                });
            if (duplicate != options.routine_replays.end()) {
                options.error =
                    "Each routine may have only one replay ordinal.";
                return options;
            }
            options.routine_replays.push_back(*request);
        } else if (argument == "--work-ram-patch") {
            if (index + 1 >= argc) {
                options.error =
                    "--work-ram-patch requires FRAME:ADDRESS:VALUE.";
                return options;
            }
            const auto patch = parse_work_ram_patch(argv[++index]);
            if (!patch) {
                options.error =
                    "--work-ram-patch requires a positive decimal FRAME, "
                    "a hexadecimal address from 2000 through 3FFF, and "
                    "a hexadecimal byte VALUE.";
                return options;
            }
            if (std::find_if(
                    options.work_ram_patches.begin(),
                    options.work_ram_patches.end(),
                    [patch](const WorkRamPatch& existing) {
                        return existing.frame == patch->frame
                            && existing.logical_address
                                == patch->logical_address;
                    })
                != options.work_ram_patches.end()) {
                options.error =
                    "Each work-RAM FRAME:ADDRESS location may be patched "
                    "once.";
                return options;
            }
            options.work_ram_patches.push_back(*patch);
        } else if (argument == "--work-ram-write-range") {
            if (index + 1 >= argc) {
                options.error =
                    "--work-ram-write-range requires START-END.";
                return options;
            }
            const auto range =
                parse_logical_address_range(argv[++index]);
            if (!range) {
                options.error =
                    "--work-ram-write-range requires a hexadecimal "
                    "logical START-END range.";
                return options;
            }
            options.work_ram_write_ranges.push_back(*range);
        } else if (argument == "--work-ram-write-trace") {
            if (index + 1 >= argc) {
                options.error =
                    "--work-ram-write-trace requires a path.";
                return options;
            }
            options.work_ram_write_trace_path = argv[++index];
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
        + static_cast<int>(options.cabinet_smoke_test)
        + static_cast<int>(options.record_canonical)
        + static_cast<int>(options.record_gameplay)
        + static_cast<int>(options.record_state_census)
        + static_cast<int>(options.backup_ram_test);
    const bool recording_selected =
        options.record_canonical || options.record_gameplay
        || options.record_state_census;
    const int recording_frame_count = options.record_state_census
        ? kStateCensusRecordingFrames
        : options.record_gameplay
            ? kGameplayRecordingFrames
            : options.record_canonical
                ? kCanonicalRecordingFrames
                : 0;
    if (operation_count > 1) {
        options.error =
            "Only one test or recording operation may be selected.";
    } else if (
        options.cabinet && (operation_count != 0 || options.validate_only)) {
        options.error =
            "--cabinet is available only for the interactive host.";
    } else if (options.backglass_path_set && !options.cabinet) {
        options.error = "--backglass requires --cabinet.";
    } else if ((!options.canonical_work_ram_path.empty()
                || !options.canonical_framebuffer_path.empty()
                || !options.canonical_audio_path.empty())
               && !recording_selected) {
        options.error =
            "--canonical-work-ram, --canonical-framebuffer, and "
            "--canonical-audio require a recording operation.";
    } else if ((!options.pass_through_hooks.empty()
                || !options.observe_hooks.empty()
                || !options.replace_hooks.empty())
               && !recording_selected) {
        options.error =
            "Execution hooks currently require a recording operation.";
    } else if (std::any_of(
                   options.replace_hooks.begin(),
                   options.replace_hooks.end(),
                   [](const RoutineReplayRequest& request) {
                       const bool cached_position_restore =
                           request.routine
                           == RoutineKey{
                               .physical_bank = 0x00U,
                               .logical_pc = 0xF241U,
                           };
                       const bool proven_position_integration =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF1B4U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 10U
                               || request.ordinal == 17U);
                       const bool proven_motion_damping =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xDD09U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 2U
                               || request.ordinal == 181U);
                       const bool proven_motion_rotation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD1A2U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 23U);
                       const bool proven_flipper_projection =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD09EU,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_radius_projection =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD612U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_angle_projection =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD62EU,
                                   }
                           && request.all_calls;
                       const bool proven_collision_motion_adjustment =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCFD8U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_response =
                           request.routine
                               == RoutineKey{
                                   .physical_bank = 0x03U,
                                   .logical_pc = 0x7FB1U,
                               }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 3U
                               || request.ordinal == 7U);
                       const bool proven_collision_consumer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7F57U,
                                   }
                           && request.all_calls;
                       const bool proven_score_accumulator =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF55CU,
                                   }
                           && (request.all_calls || request.ordinal == 1U);
                       const bool proven_rng_update =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE8B3U,
                                   }
                           && (request.all_calls || request.ordinal == 1U);
                       const bool proven_signed_word_negation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE837U,
                                   }
                           && request.all_calls;
                       const bool proven_signed_division =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE905U,
                                   }
                           && request.all_calls;
                       const bool proven_unsigned_multiply_round =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE624U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 2U);
                       const bool proven_unsigned_byte_multiply =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE569U,
                                   }
                           && request.all_calls;
                       const bool proven_signed_fixed_multiply =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xE65CU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_camera_activation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x00U,
                                       .logical_pc = 0xF4FCU,
                                   }
                           && request.all_calls;
                       const bool proven_actor_slot_reset =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC574U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_input_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD466U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_scan_vertical_reject =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCC1AU,
                                   }
                           && (request.all_calls
                               || request.ordinal == 19176U);
                       const bool proven_collision_mask_lookup =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCC98U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_geometry =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xCCD5U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_phase_step =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD500U,
                                   }
                           && request.all_calls;
                       const bool proven_flipper_geometry =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xD081U,
                                   }
                           && request.all_calls;
                       const bool proven_coroutine_scheduler =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC5C9U,
                                   }
                           && request.all_calls;
                       const bool proven_nudge_input_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC3CBU,
                                   }
                           && request.all_calls;
                       const bool proven_session_input_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC49EU,
                                   }
                           && request.all_calls;
                       const bool proven_ball_drain_threshold =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xDD6EU,
                                   }
                           && request.all_calls;
                       const bool proven_remaining_ball_presentation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC80AU,
                                   }
                           && request.all_calls;
                       const bool proven_remaining_ball_transition_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7EAU,
                                   }
                           && request.all_calls;
                       const bool proven_remaining_ball_decision =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x01U,
                                       .logical_pc = 0xC7E5U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4DD9U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4DC8U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_opening_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4DF7U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_steady_idle =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4E24U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_eyeball_final_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4F3DU,
                                   }
                           && request.all_calls;
                       const bool proven_gray_blob_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6876U,
                                   }
                           && request.all_calls;
                       const bool proven_gray_blob_first_live_prefix =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x68A1U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_live_controller_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x57ADU,
                                   }
                           && request.all_calls;
                       const bool proven_mercy_star_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4F9FU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x50EEU,
                           }
                           && request.all_calls;
                       const bool proven_little_skull_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x44E4U,
                           }
                           && request.all_calls;
                       const bool proven_little_skull_gray_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x458AU,
                                   }
                           && request.all_calls;
                       const bool proven_little_skull_gray_release =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x45A4U,
                                   }
                           && request.all_calls;
                       const bool proven_little_skull_live_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4515U,
                                   }
                           && request.all_calls;
                       const bool proven_little_skull_pair_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4485U,
                           }
                           && request.all_calls;
                       const bool proven_little_skull_hidden_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4455U,
                           }
                           && request.all_calls;
                       const bool proven_big_star_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x565CU,
                                    }
                            && request.all_calls;
                       const bool proven_sorcerer_deployment_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5808U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_staging_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x57F8U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x433FU,
                                    }
                            && request.all_calls;
                       const bool proven_undead_head_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x61A0U,
                                   }
                           && request.all_calls;
                       const bool proven_undead_head_selector_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x61ABU,
                                   }
                           && request.all_calls;
                       const bool proven_undead_head_waiting_parent =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x61A3U,
                                   }
                           && request.all_calls;
                       const bool proven_side_worm_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5C4BU,
                                    }
                               || request.routine
                                      == RoutineKey{
                                          .physical_bank = 0x02U,
                                          .logical_pc = 0x5C7CU,
                                      })
                           && request.all_calls;
                       const bool proven_side_worm_selector_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5CCDU,
                                   }
                           && request.all_calls;
                       const bool proven_side_worm_waiting_parent =
                           request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x5CC5U}
                           && request.all_calls;
                       const bool proven_side_worm_active_timer_initializer =
                           request.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x5D4AU}
                           && request.all_calls;
                       const bool proven_dragon_head_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5A31U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_skeleton_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5EE4U,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x664AU,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_controller_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x65BCU,
                                   }
                           && request.all_calls;
                       const bool proven_walled_star_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6319U,
                           }
                           && request.all_calls;
                       const bool proven_walled_star_access_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4A44U,
                           }
                           && request.all_calls;
                       const bool proven_side_star_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A54U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A4CU,
                                    })
                           && request.all_calls;
                       const bool proven_knight_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x734EU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7353U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x735AU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7361U,
                                    })
                           && request.all_calls;
                       const bool proven_side_spider_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x64FAU,
                                   }
                           && request.all_calls;
                       const bool proven_front_spider_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x646BU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4C78U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_outer_initializers =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A5CU,
                                    }
                            || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4A64U,
                                    })
                           && request.all_calls;
                       const bool proven_mercy_star_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4FC1U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x42F6U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4345U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4357U,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_egg_post_allocation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4371U,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_feature_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x445BU,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_pending_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x570AU,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_pending_helper =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5722U,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_pending_right_helper =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5738U,
                                   }
                           && request.all_calls;
                       const bool proven_big_star_rotation_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5674U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_dormant =
                           (request.routine
                                == RoutineKey{
                                    .physical_bank = 0x02U,
                                    .logical_pc = 0x53DDU,
                                }
                           || request.routine
                                == RoutineKey{
                                    .physical_bank = 0x02U,
                                    .logical_pc = 0x5456U,
                                })
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_position =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5397U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_appearance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5408U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_companion_active_timer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x545DU,
                                   }
                           && request.all_calls;
                       const bool
                           proven_devil_shooter_companion_left_active_timer =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x53E4U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_left_active_controller =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x53C8U,
                                       }
                               && request.all_calls;
                       const bool
                           proven_devil_shooter_right_active_controller =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5441U,
                                       }
                               && request.all_calls;
                       const bool proven_dragon_head_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5A4BU,
                                   }
                           && request.all_calls;
                       const bool proven_dragon_head_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5A6EU,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5116U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_release_delay_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5159U,
                                   }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_position =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x52E3U,
                                   }
                           && request.all_calls;
                       const bool
                           proven_devil_shooter_catch_continuation =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x52B8U,
                                       }
                               && request.all_calls;
                       const bool proven_skeleton_insect_left_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x54B9U,
                                   }
                           && request.all_calls;
                       const bool proven_skeleton_insect_right_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5551U,
                                   }
                           && request.all_calls;
                       const bool proven_skeleton_insect_center_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x55F9U,
                                   }
                           && request.all_calls;
                       const bool
                           proven_skeleton_insect_center_continuation =
                               request.routine
                                       == RoutineKey{
                                           .physical_bank = 0x02U,
                                           .logical_pc = 0x5604U,
                                       }
                               && request.all_calls;
                       const bool proven_devils_coffin_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x63F1U,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x63E1U,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_opening =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x640AU,
                                   }
                           && request.all_calls;
                       const bool proven_devils_coffin_closing =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6445U,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x65C2U,
                                   }
                           && request.all_calls;
                       const bool proven_three_blobs_appearance_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x67B3U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_door_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x62B8U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_door_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6294U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_animator_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x767EU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_animator_active_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x76C5U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_request_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x74D8U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_crown_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x76FAU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_mouth_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7726U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4C91U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4CFBU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_helmet_followup_timer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4D0BU,
                                   }
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4A93U,
                                   }
                           && request.all_calls;
                       const bool proven_walled_star_access_controller =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4B3FU,
                                   }
                           && request.all_calls;
                       const bool proven_side_star_access_controller =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4BD5U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x4C0AU,
                                    })
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_post_audio_score =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4AD2U,
                                   }
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_post_appearance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4AE5U,
                                   }
                           && request.all_calls;
                       const bool proven_four_star_wall_pocket_animation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4AEBU,
                                   }
                           && request.all_calls;
                       const bool proven_side_star_bonus_entrance_animation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x4C33U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5972U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_countdown =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5991U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_opening =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x59A5U,
                                   }
                           && request.all_calls;
                       const bool proven_upper_wall_pocket_hand_opening_followup =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x59C7U,
                                   }
                           && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_opening_expiry =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x59E0U,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_post_opening_schedule =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5A08U,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_post_opening_state =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5A0BU,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_post_opening_countdown =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x5A1BU,
                                    }
                            && request.all_calls;
                        const bool proven_upper_wall_pocket_hand_live_setup =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5975U,
                                   }
                           && request.all_calls;
                       const bool proven_walled_star_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6339U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60C3U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60B3U,
                                   }
                           && request.all_calls;
                       const bool proven_knight_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x73D8U,
                                   }
                           && request.all_calls;
                       const bool proven_knight_waiting_parent =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7378U,
                                   }
                           && request.all_calls;
                       const bool proven_spider_appearance =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x65A4U,
                           }
                           && request.all_calls;
                       const bool proven_devil_shooter_charge_retraction =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x533BU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_animation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6054U,
                                   }
                           && request.all_calls;
                       const bool proven_small_soldier_source_gates =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7D85U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7E23U,
                           })
                           && request.all_calls;
                       const bool proven_small_soldier_source_initializers =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7D4DU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7DE9U,
                           })
                           && request.all_calls;
                       const bool proven_small_soldier_delayed_spawn_gates =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7D6FU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7E0DU,
                                   })
                           && request.all_calls;
                       const bool proven_giant_face_controller_initializers =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7428U,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x742BU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x742EU,
                           })
                           && request.all_calls;
                       const bool proven_giant_face_animator_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x766CU,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_crown_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x76F7U,
                                   }
                           && request.all_calls;
                       const bool proven_giant_face_mouth_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7718U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_source_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F5FU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_source_countdown_wait =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F4AU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_insect_source_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5F22U,
                                   }
                           && request.all_calls;
                       const bool proven_sorcerer_color_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x57E9U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_hit_continuation =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60E0U,
                                   }
                           && request.all_calls;
                       const bool proven_tower_post_dispatch =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x611CU,
                                   }
                           && request.all_calls;
                       const bool proven_tower_guard_selector =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x60A6U,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_hit_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x78C7U,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_initializers =
                           (request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x789AU,
                                   }
                               || request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x78A4U,
                                   })
                           && request.all_calls;
                       const bool proven_bumper_child_initializer =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x79DEU,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_primary_progress =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x78F5U,
                                   }
                           && request.all_calls;
                       const bool proven_bumper_secondary_progress =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7912U,
                                   }
                           && request.all_calls;
                       const bool proven_fixed_response_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7CAFU,
                                   }
                           && request.all_calls;
                        const bool proven_fixed_scheduler_gate =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B27U,
                                    }
                            && request.all_calls;
                        const bool proven_fixed_scheduler_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B1FU,
                                    }
                            && request.all_calls;
                        const bool proven_fixed_marker_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B6DU,
                                    }
                            && request.all_calls;
                       const bool proven_fixed_response_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7CA7U,
                                    }
                            && request.all_calls;
                        const bool proven_dragon_egg_controller_initializer =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x02U,
                                        .logical_pc = 0x42EBU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_first_movement_prefix =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66A3U,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_direct_continuation =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x66DEU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_shared_motion_idle =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x676BU,
                                    }
                            && request.all_calls;
                        const bool proven_three_blobs_collision_gate =
                            request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x67C0U,
                                    }
                            && request.all_calls;
                        const bool proven_fixed_scheduler_submission =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B4EU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7B5DU,
                                    })
                           && request.all_calls;
                       const bool proven_fixed_marker_gate =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x7B7AU,
                                   }
                           && request.all_calls;
                       const bool proven_fixed_marker_submission =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BBBU,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BCDU,
                                    })
                           && request.all_calls;
                       const bool proven_fixed_marker_helper =
                           (request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BE0U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BE8U,
                                    }
                                || request.routine
                                    == RoutineKey{
                                        .physical_bank = 0x03U,
                                        .logical_pc = 0x7BECU,
                                    })
                           && request.all_calls;
                       const bool proven_scaled_word_add =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x02U,
                                       .logical_pc = 0x5955U,
                                   }
                           && request.all_calls;
                       const bool proven_collision_table_pass =
                           request.routine
                                   == RoutineKey{
                                       .physical_bank = 0x03U,
                                       .logical_pc = 0x6966U,
                                   }
                           && (request.all_calls
                               || request.ordinal == 1U
                               || request.ordinal == 2U
                               || request.ordinal == 3U
                               || request.ordinal == 4U
                               || request.ordinal == 5U
                               || request.ordinal == 6U
                               || request.ordinal == 7U
                               || request.ordinal == 8U
                               || request.ordinal == 9U
                               || request.ordinal == 10U
                               || request.ordinal == 12U
                               || request.ordinal == 14U
                               || request.ordinal == 15U
                               || request.ordinal == 16U
                               || request.ordinal == 17U
                               || request.ordinal == 18U
                               || request.ordinal == 19U
                               || request.ordinal == 20U
                               || request.ordinal == 21U
                               || request.ordinal == 22U
                               || request.ordinal == 23U);
                       return !cached_position_restore
                           && !proven_position_integration
                           && !proven_motion_damping
                           && !proven_motion_rotation
                           && !proven_flipper_projection
                           && !proven_flipper_radius_projection
                           && !proven_flipper_angle_projection
                           && !proven_collision_motion_adjustment
                           && !proven_collision_response
                           && !proven_collision_consumer
                           && !proven_score_accumulator
                           && !proven_rng_update
                           && !proven_signed_word_negation
                           && !proven_signed_division
                           && !proven_unsigned_byte_multiply
                           && !proven_unsigned_multiply_round
                           && !proven_signed_fixed_multiply
                           && !proven_actor_camera_activation
                           && !proven_actor_slot_reset
                           && !proven_flipper_input_gate
                           && !proven_collision_scan_vertical_reject
                           && !proven_collision_mask_lookup
                           && !proven_collision_geometry
                           && !proven_flipper_phase_step
                           && !proven_flipper_geometry
                           && !proven_coroutine_scheduler
                           && !proven_nudge_input_gate
                           && !proven_session_input_gate
                           && !proven_ball_drain_threshold
                           && !proven_remaining_ball_presentation
                           && !proven_remaining_ball_transition_gate
                           && !proven_remaining_ball_decision
                           && !proven_giant_skeleton_eyeball_gate
                           && !proven_giant_skeleton_eyeball_initializer
                           && !proven_giant_skeleton_eyeball_opening_wait
                           && !proven_giant_skeleton_eyeball_steady_idle
                           && !proven_giant_skeleton_eyeball_final_wait
                           && !proven_gray_blob_initializer
                           && !proven_gray_blob_first_live_prefix
                           && !proven_sorcerer_live_controller_gate
                           && !proven_mercy_star_initializer
                           && !proven_devil_shooter_initializer
                           && !proven_little_skull_initializer
                           && !proven_little_skull_gray_initializer
                           && !proven_little_skull_gray_release
                           && !proven_little_skull_live_continuation
                            && !proven_little_skull_pair_initializer
                            && !proven_little_skull_hidden_initializer
                            && !proven_big_star_initializer
                            && !proven_sorcerer_deployment_initializer
                            && !proven_sorcerer_staging_initializer
                            && !proven_dragon_egg_initializer
                           && !proven_undead_head_initializer
                           && !proven_side_worm_initializers
                           && !proven_side_worm_selector_gate
                           && !proven_side_worm_waiting_parent
                           && !proven_side_worm_active_timer_initializer
                           && !proven_dragon_head_initializer
                           && !proven_giant_skeleton_initializer
                           && !proven_three_blobs_initializer
                           && !proven_three_blobs_controller_initializer
                           && !proven_undead_head_selector_gate
                           && !proven_undead_head_waiting_parent
                           && !proven_walled_star_initializer
                           && !proven_walled_star_access_initializer
                           && !proven_side_star_initializers
                           && !proven_knight_initializers
                           && !proven_side_spider_initializer
                           && !proven_front_spider_initializer
                           && !proven_giant_face_helmet_initializer
                           && !proven_giant_face_helmet_outer_initializers
                           && !proven_mercy_star_gate
                           && !proven_dragon_egg_controller
                           && !proven_dragon_egg_hit_gate
                           && !proven_dragon_egg_hit_continuation
                           && !proven_dragon_egg_post_allocation
                           && !proven_big_star_feature_gate
                           && !proven_big_star_pending_gate
                           && !proven_big_star_pending_helper
                           && !proven_big_star_pending_right_helper
                           && !proven_big_star_rotation_wait
                           && !proven_devil_shooter_companion_dormant
                           && !proven_devil_shooter_companion_position
                           && !proven_devil_shooter_companion_appearance
                           && !proven_devil_shooter_companion_active_timer
                           && !proven_devil_shooter_companion_left_active_timer
                           && !proven_devil_shooter_left_active_controller
                           && !proven_devil_shooter_right_active_controller
                           && !proven_dragon_head_hit_gate
                           && !proven_dragon_head_hit_continuation
                           && !proven_devil_shooter_hit_gate
                           && !proven_devil_shooter_release_delay_gate
                           && !proven_devil_shooter_charge_position
                           && !proven_devil_shooter_charge_retraction
                           && !proven_devil_shooter_catch_continuation
                           && !proven_skeleton_insect_left_gate
                           && !proven_skeleton_insect_right_gate
                           && !proven_skeleton_insect_center_gate
                           && !proven_skeleton_insect_center_continuation
                           && !proven_devils_coffin_gate
                           && !proven_devils_coffin_initializer
                           && !proven_devils_coffin_opening
                           && !proven_devils_coffin_closing
                           && !proven_giant_door_hit_gate
                           && !proven_giant_door_initializer
                           && !proven_giant_face_request_gate
                           && !proven_giant_face_animator_gate
                           && !proven_giant_face_animator_active_wait
                           && !proven_giant_face_crown_gate
                           && !proven_giant_face_mouth_gate
                           && !proven_giant_face_helmet_controller
                           && !proven_giant_face_helmet_hit_continuation
                           && !proven_giant_face_helmet_followup_timer
                           && !proven_four_star_wall_pocket_controller
                           && !proven_walled_star_access_controller
                           && !proven_side_star_access_controller
                           && !proven_four_star_wall_pocket_post_audio_score
                           && !proven_four_star_wall_pocket_post_appearance
                           && !proven_four_star_wall_pocket_animation
                           && !proven_side_star_bonus_entrance_animation
                           && !proven_upper_wall_pocket_hand_initializer
                           && !proven_upper_wall_pocket_hand_live_setup
                           && !proven_upper_wall_pocket_hand_countdown
                           && !proven_upper_wall_pocket_hand_opening
                           && !proven_upper_wall_pocket_hand_opening_followup
                           && !proven_upper_wall_pocket_hand_opening_expiry
                           && !proven_upper_wall_pocket_hand_post_opening_schedule
                           && !proven_upper_wall_pocket_hand_post_opening_state
                           && !proven_upper_wall_pocket_hand_post_opening_countdown
                           && !proven_walled_star_hit_gate
                           && !proven_tower_hit_gate
                           && !proven_tower_initializer
                           && !proven_knight_hit_gate
                           && !proven_knight_waiting_parent
                           && !proven_spider_appearance
                           && !proven_tower_insect_animation
                           && !proven_small_soldier_source_gates
                           && !proven_small_soldier_source_initializers
                           && !proven_small_soldier_delayed_spawn_gates
                           && !proven_giant_face_controller_initializers
                           && !proven_giant_face_animator_initializer
                           && !proven_giant_face_crown_initializer
                           && !proven_giant_face_mouth_initializer
                           && !proven_tower_insect_source_gate
                           && !proven_tower_insect_source_countdown_wait
                           && !proven_tower_insect_source_initializer
                           && !proven_sorcerer_color_gate
                           && !proven_tower_hit_continuation
                           && !proven_tower_post_dispatch
                           && !proven_tower_guard_selector
                           && !proven_bumper_hit_gate
                           && !proven_bumper_initializers
                           && !proven_bumper_child_initializer
                           && !proven_bumper_primary_progress
                            && !proven_bumper_secondary_progress
                            && !proven_fixed_response_gate
                            && !proven_fixed_scheduler_gate
                            && !proven_fixed_scheduler_initializer
                            && !proven_fixed_marker_initializer
                            && !proven_fixed_response_initializer
                            && !proven_dragon_egg_controller_initializer
                            && !proven_three_blobs_first_movement_prefix
                            && !proven_three_blobs_direct_continuation
                            && !proven_three_blobs_shared_motion_idle
                            && !proven_three_blobs_collision_gate
                            && !proven_fixed_scheduler_submission
                           && !proven_fixed_marker_gate
                           && !proven_fixed_marker_submission
                           && !proven_fixed_marker_helper
                           && !proven_scaled_word_add
                           && !proven_three_blobs_controller
                           && !proven_three_blobs_appearance_gate
                           && !proven_collision_table_pass;
                   })) {
        options.error =
            "Native replacement currently supports 00:F241, semantic "
            "00:F1B4@all, and the proven no-interrupt 00:F1B4@1, 00:F1B4@10, "
            "00:F1B4@17, semantic 01:D1A2@all, 01:D1A2@23, "
            "semantic 01:DD09@all, 01:DD09@1, 01:DD09@2, "
            "01:DD09@181, 03:6966@1, 03:6966@2, 03:6966@3, "
            "03:6966@4, 03:6966@5, 03:6966@6, 03:6966@7, "
            "03:6966@8, 03:6966@9, 03:6966@10, 03:6966@12, "
            "03:6966@14, 03:6966@15, 03:6966@16, 03:6966@17, "
            "03:6966@18, 03:6966@19, 03:6966@20, 03:6966@21, "
            "03:6966@22, 03:6966@23, semantic 03:6966@all, semantic "
            "03:7FB1@all, semantic 03:7F57@all, semantic 00:F55C@all, "
            "semantic 00:E8B3@all, semantic 00:E837@all, "
            "semantic 00:E905@all, semantic 00:E569@all, "
            "semantic 00:E624@all, "
            "semantic 00:E65C@all, "
            "semantic 01:D09E@all, semantic 01:D612@all, "
            "semantic 01:D62E@all, "
            "semantic 01:CFD8@all, "
            "semantic 00:F4FC@all, semantic "
            "01:C574@all, semantic 01:D466@all, semantic 01:CC1A@all, "
            "01:CC1A@19176, "
            "semantic 01:CC98@all, "
            "semantic 01:CCD5@all, "
            "semantic "
            "01:D500@all, semantic 01:D081@all, semantic 01:C5C9@all, "
            "semantic 01:C3CB@all, semantic 01:C49E@all, semantic 01:DD6E@all, "
            "semantic 01:C80A@all, semantic 01:C7EA@all, semantic 01:C7E5@all, "
            "semantic 02:42EB@all, "
            "semantic 02:42F6@all, semantic 02:4345@all, "
            "semantic 02:4357@all, semantic 02:4371@all, "
            "semantic 02:445B@all, "
            "semantic 02:570A@all, semantic 02:5674@all, "
            "semantic 02:5397@all, semantic 02:5408@all, semantic 02:53C8@all, semantic 02:53E4@all, semantic 02:5441@all, semantic 02:545D@all, semantic 02:53DD@all, semantic 02:5456@all, "
            "semantic 02:4DC8@all, semantic 02:4DD9@all, semantic 02:4DF7@all, semantic 02:4E24@all, semantic 02:4F3D@all, semantic 03:76C5@all, "
            "semantic 03:6876@all, semantic 03:68A1@all, semantic 02:5A31@all, semantic 02:5EE4@all, semantic 03:664A@all, semantic 03:65BC@all, semantic 03:66DE@all, "
            "semantic 02:57AD@all, "
            "semantic 02:4FC1@all, "
            "semantic 02:4A93@all, semantic 02:4AD2@all, "
            "semantic 02:4AE5@all, semantic 02:4AEB@all, "
            "semantic 02:4B3F@all, "
            "semantic 02:4BD5@all, semantic 02:4C0A@all, semantic 02:4C33@all, semantic 02:4CFB@all, semantic 02:4D0B@all, "
            "semantic 02:5116@all, "
            "semantic 02:5159@all, "
            "semantic 02:52E3@all, "
            "semantic 02:533B@all, "
            "semantic 02:52B8@all, "
            "semantic 02:54B9@all, "
            "semantic 02:5551@all, "
            "semantic 02:55F9@all, "
            "semantic 02:5604@all, "
            "semantic 02:5A4B@all, "
            "semantic 02:5A6E@all, "
            "semantic 03:63F1@all, "
            "semantic 03:640A@all, "
            "semantic 03:6445@all, "
            "semantic 03:6294@all, semantic 03:62B8@all, "
            "semantic 03:74D8@all, "
            "semantic 03:767E@all, "
            "semantic 03:76FA@all, "
            "semantic 03:7726@all, "
            "semantic 03:6339@all, "
            "semantic 03:60C3@all, "
            "semantic 03:60B3@all, "
            "semantic 03:7378@all, "
            "semantic 03:73D8@all, "
            "semantic 03:65A4@all, "
            "semantic 03:6054@all, "
            "semantic 02:5F22@all, "
            "semantic 02:5F4A@all, "
            "semantic 02:5F5F@all, "
            "semantic 03:7D85@all, semantic 03:7E23@all, "
            "semantic 03:7D4D@all, semantic 03:7DE9@all, "
            "semantic 03:7D6F@all, semantic 03:7E0D@all, "
            "semantic 03:7428@all, semantic 03:742B@all, "
            "semantic 03:742E@all, "
            "semantic 03:766C@all, "
            "semantic 03:76F7@all, "
            "semantic 03:7718@all, "
            "semantic 02:57E9@all, "
            "semantic 03:60E0@all, "
            "semantic 03:611C@all, "
            "semantic 03:60A6@all, "
            "semantic 03:78C7@all, "
            "semantic 03:789A@all, semantic 03:78A4@all, "
            "semantic 03:79DE@all, "
            "semantic 03:78F5@all, "
            "semantic 03:7912@all, "
            "semantic 03:7CAF@all, "
            "semantic 03:7CA7@all, "
            "semantic 03:7B27@all, "
            "semantic 03:7B4E@all, semantic 03:7B5D@all, "
            "semantic 03:7B7A@all, "
            "semantic 03:7BBB@all, semantic 03:7BCD@all, "
            "semantic 03:7BE0@all, semantic 03:7BE8@all, "
            "semantic 03:7BEC@all, "
            "semantic 02:5955@all, "
            "semantic 02:5972@all, semantic 02:5975@all, semantic 02:5991@all, "
            "semantic 02:59A5@all, "
              "semantic 02:59C7@all, "
              "semantic 02:59E0@all, "
              "semantic 02:5A08@all, "
              "semantic 02:5A0B@all, "
              "semantic 02:5A1B@all, "
            "semantic 03:65BC@all, semantic 03:65C2@all, semantic 03:67B3@all, "
            "semantic 03:66A3@all, and semantic 03:676B@all, "
            "the proven 03:7FB1@1, 03:7FB1@3, 03:7FB1@7, 00:F55C@1, "
            "and 00:E8B3@1 calls.";
    } else if (!options.work_ram_patches.empty() && !recording_selected) {
        options.error =
            "--work-ram-patch requires a recording operation.";
    } else if (std::any_of(
                   options.work_ram_patches.begin(),
                   options.work_ram_patches.end(),
                   [recording_frame_count](const WorkRamPatch& patch) {
                       return patch.frame > recording_frame_count;
                   })) {
        options.error =
            "Each work-RAM patch frame must be within the selected "
            "recording.";
    } else if (options.observe_hooks.empty()
               && (!options.routine_trace_path.empty()
                   || !options.routine_call_trace_path.empty())) {
        options.error =
            "Routine traces require at least one --observe-hook.";
    } else if (!options.observe_hooks.empty()
               && options.routine_trace_path.empty()
               && options.routine_call_trace_path.empty()) {
        options.error =
            "--observe-hook requires --routine-trace or "
            "--routine-call-trace.";
    } else if (!options.routine_replays.empty()
               && (!recording_selected
                   || options.routine_call_trace_path.empty())) {
        options.error =
            "--routine-replay requires a recording operation and "
            "--routine-call-trace.";
    } else if (std::any_of(
                   options.routine_replays.begin(),
                   options.routine_replays.end(),
                   [&options](const RoutineReplayRequest& request) {
                       return std::find(
                                  options.observe_hooks.begin(),
                                  options.observe_hooks.end(),
                                  request.routine)
                           == options.observe_hooks.end();
                   })) {
        options.error =
            "Each --routine-replay target must also be an --observe-hook.";
    } else if ((!options.work_ram_write_ranges.empty()
                || !options.work_ram_write_trace_path.empty())
               && !recording_selected) {
        options.error =
            "Work-RAM write census options require a recording operation.";
    } else if (options.work_ram_write_ranges.empty()
               != options.work_ram_write_trace_path.empty()) {
        options.error =
            "--work-ram-write-range and --work-ram-write-trace "
            "must be used together.";
    }
    return options;
}

void print_usage()
{
    std::cout
        << "Devil's Crush PC port - Milestone 4\n\n"
        << "Usage:\n"
        << "  devils-crush [--rom <path>]\n"
        << "  devils-crush --cabinet [--backglass <image>] [--rom <path>]\n"
        << "  devils-crush --validate-rom [path]\n"
        << "  devils-crush --smoke-test [--rom <path>]\n"
        << "  devils-crush --cabinet-smoke-test [--rom <path>]\n"
        << "  devils-crush --record-canonical [output.csv]\n"
           "      [--canonical-work-ram <output.bin>]\n"
           "      [--canonical-framebuffer <output.rgb>]\n"
           "      [--canonical-audio <output.wav>]\n"
           "      [--pass-through-hook <bank:pc>]...\n"
           "      [--observe-hook <bank:pc>]...\n"
           "      [--replace-hook <bank:pc@ordinal|bank:pc@all>]...\n"
           "      [--routine-trace <entries.csv>]\n"
           "      [--routine-call-trace <calls.csv>]\n"
           "      [--routine-replay <bank:pc@ordinal>]...\n"
           "      [--work-ram-patch <frame:address:value>]...\n"
           "      [--work-ram-write-range <start-end>]...\n"
           "      [--work-ram-write-trace <writes.csv>]\n"
           "      [--rom <path>]\n"
        << "  devils-crush --record-gameplay [output.csv]\n"
           "      [--canonical-work-ram <output.bin>]\n"
           "      [--canonical-framebuffer <output.rgb>]\n"
           "      [--canonical-audio <output.wav>]\n"
           "      [--pass-through-hook <bank:pc>]...\n"
           "      [--observe-hook <bank:pc>]...\n"
           "      [--replace-hook <bank:pc@ordinal|bank:pc@all>]...\n"
           "      [--routine-trace <entries.csv>]\n"
           "      [--routine-call-trace <calls.csv>]\n"
           "      [--routine-replay <bank:pc@ordinal>]...\n"
           "      [--work-ram-patch <frame:address:value>]...\n"
           "      [--work-ram-write-range <start-end>]...\n"
           "      [--work-ram-write-trace <writes.csv>]\n"
           "      [--rom <path>]\n"
        << "  devils-crush --record-state-census [output.csv]\n"
           "      [--canonical-work-ram <output.bin>]\n"
           "      [--canonical-framebuffer <output.rgb>]\n"
           "      [--canonical-audio <output.wav>]\n"
           "      [--pass-through-hook <bank:pc>]...\n"
           "      [--observe-hook <bank:pc>]...\n"
           "      [--replace-hook <bank:pc@ordinal|bank:pc@all>]...\n"
           "      [--routine-trace <entries.csv>]\n"
           "      [--routine-call-trace <calls.csv>]\n"
           "      [--routine-replay <bank:pc@ordinal>]...\n"
           "      [--work-ram-patch <frame:address:value>]...\n"
           "      [--work-ram-write-range <start-end>]...\n"
           "      [--work-ram-write-trace <writes.csv>]\n"
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
        << "  F12               Cabinet display assignment\n"
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

[[nodiscard]] std::optional<std::vector<std::byte>> read_rom_bytes(
    const std::filesystem::path& path)
{
    std::vector<std::byte> bytes(
        static_cast<std::size_t>(
            devils_crush::rom::kSupportedRomSize));
    std::ifstream input(path, std::ios::binary);
    if (input) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!input
        || input.gcount()
            != static_cast<std::streamsize>(bytes.size())) {
        std::cerr << "Could not read the supported ROM bytes: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    return bytes;
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

[[nodiscard]] bool upload_image(
    SDL_Renderer* renderer,
    const devils_crush::render::ArgbImage& image,
    SourceTexture& source)
{
    if (!image.valid()) {
        std::cerr << "The cabinet renderer produced an invalid image.\n";
        return false;
    }
    return upload_framebuffer(
        renderer,
        {
            .pixels = image.pixels,
            .width = image.width,
            .height = image.height,
            .pitch_pixels = image.width,
        },
        source);
}

[[nodiscard]] std::filesystem::path path_from_utf8(
    const std::string_view text)
{
    std::u8string encoded;
    encoded.reserve(text.size());
    for (const unsigned char byte : text) {
        encoded.push_back(static_cast<char8_t>(byte));
    }
    return std::filesystem::path(encoded);
}

[[nodiscard]] bool load_backglass_texture(
    SDL_Renderer* const renderer,
    const std::filesystem::path& path,
    SourceTexture& source)
{
    const auto image =
        devils_crush::render::load_backglass_image(path);
    if (!image.loaded()) {
        std::cerr << "Could not load backglass image: "
                  << image.message << " (" << path.string() << ")\n";
        return false;
    }

    SDL_Texture* const texture =
        SDL_CreateTextureFromSurface(renderer, image.surface);
    const auto width = image.surface->w;
    const auto height = image.surface->h;
    if (texture == nullptr) {
        report_sdl_error(
            "Creating the backglass texture failed");
        return false;
    }
    if (!configure_texture(texture, SDL_SCALEMODE_NEAREST)) {
        SDL_DestroyTexture(texture);
        return false;
    }

    destroy_source(source);
    source = {
        .texture = texture,
        .width = width,
        .height = height,
    };
    std::cout << "Loaded backglass image: " << path.string() << " ("
              << width << 'x' << height << ")\n";
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
    StageTexture& stage,
    const std::uint32_t aspect_numerator,
    const std::uint32_t aspect_denominator,
    const std::array<std::uint8_t, 3> clear_color = {8U, 8U, 12U})
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

    if (!SDL_SetRenderDrawColor(
            renderer,
            clear_color[0],
            clear_color[1],
            clear_color[2],
            255U)
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
        aspect_numerator,
        aspect_denominator);
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

    if (!SDL_SetRenderDrawColor(
            renderer,
            clear_color[0],
            clear_color[1],
            clear_color[2],
            255U)
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

struct AuxiliaryWindow {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};

struct HostDisplays {
    std::vector<SDL_DisplayID> ids;
    std::vector<devils_crush::cabinet::DisplayDescriptor> descriptors;
};

[[nodiscard]] devils_crush::cabinet::DisplayRotation display_rotation(
    const SDL_DisplayOrientation orientation) noexcept
{
    using devils_crush::cabinet::DisplayRotation;
    switch (orientation) {
    case SDL_ORIENTATION_LANDSCAPE:
        return DisplayRotation::landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return DisplayRotation::landscape_flipped;
    case SDL_ORIENTATION_PORTRAIT:
        return DisplayRotation::portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return DisplayRotation::portrait_flipped;
    case SDL_ORIENTATION_UNKNOWN:
    default:
        return DisplayRotation::unknown;
    }
}

[[nodiscard]] HostDisplays enumerate_host_displays()
{
    HostDisplays result;
    int count = 0;
    SDL_DisplayID* const ids = SDL_GetDisplays(&count);
    if (ids == nullptr || count <= 0) {
        report_sdl_error("SDL_GetDisplays failed");
        SDL_free(ids);
        return result;
    }
    result.ids.reserve(static_cast<std::size_t>(count));
    result.descriptors.reserve(static_cast<std::size_t>(count));
    const auto primary = SDL_GetPrimaryDisplay();
    for (int index = 0; index < count; ++index) {
        SDL_Rect bounds{};
        if (!SDL_GetDisplayBounds(ids[index], &bounds)
            || bounds.w <= 0 || bounds.h <= 0) {
            report_sdl_error("SDL_GetDisplayBounds failed");
            continue;
        }
        const char* const name = SDL_GetDisplayName(ids[index]);
        result.ids.push_back(ids[index]);
        result.descriptors.push_back({
            .identity =
                name != nullptr && name[0] != '\0'
                ? name
                : "Unnamed display",
            .bounds = {
                .x = bounds.x,
                .y = bounds.y,
                .width = bounds.w,
                .height = bounds.h,
            },
            .rotation =
                display_rotation(
                    SDL_GetCurrentDisplayOrientation(ids[index])),
            .primary = ids[index] == primary,
        });
    }
    SDL_free(ids);
    return result;
}

[[nodiscard]] bool apply_window_placement(
    SDL_Window* const window,
    const devils_crush::cabinet::WindowPlacement& placement,
    const HostDisplays& displays,
    const int default_width,
    const int default_height)
{
    if (window == nullptr || displays.ids.size() != displays.descriptors.size()) {
        return false;
    }
    const auto resolved = devils_crush::cabinet::resolve_display(
        placement,
        displays.descriptors);
    if (!resolved) {
        return true;
    }
    const auto geometry = devils_crush::cabinet::fit_window_geometry(
        placement,
        displays.descriptors[*resolved],
        default_width,
        default_height);
    const bool was_fullscreen =
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U;
    if (was_fullscreen && !SDL_SetWindowFullscreen(window, false)) {
        return report_sdl_error(
            "Leaving cabinet window fullscreen state failed");
    }
    if (!geometry.valid()
        || !SDL_SetWindowSize(window, geometry.width, geometry.height)
        || !SDL_SetWindowPosition(window, geometry.x, geometry.y)) {
        return report_sdl_error("Applying cabinet window geometry failed");
    }
    if (placement.fullscreen
        && !SDL_SetWindowFullscreen(window, true)) {
        return report_sdl_error(
            "Applying cabinet window fullscreen state failed");
    }
    return true;
}

[[nodiscard]] devils_crush::cabinet::WindowPlacement
capture_window_placement(
    SDL_Window* const window,
    const HostDisplays& displays,
    devils_crush::cabinet::WindowPlacement fallback)
{
    if (window == nullptr) {
        return fallback;
    }
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (SDL_GetWindowPosition(window, &x, &y)
        && SDL_GetWindowSize(window, &width, &height)
        && width > 0 && height > 0) {
        fallback.geometry = {
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        };
    }
    fallback.fullscreen =
        (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0U;
    const auto display_id = SDL_GetDisplayForWindow(window);
    if (display_id != 0U) {
        const char* const name = SDL_GetDisplayName(display_id);
        if (name != nullptr && name[0] != '\0') {
            fallback.display_identity = name;
        }
        fallback.rotation = display_rotation(
            SDL_GetCurrentDisplayOrientation(display_id));
    }
    for (std::size_t index = 0; index < displays.ids.size(); ++index) {
        if (displays.ids[index] == display_id) {
            if (fallback.display_identity.empty()) {
                fallback.display_identity =
                    displays.descriptors[index].identity;
            }
            break;
        }
    }
    return fallback;
}

void destroy_auxiliary_window(AuxiliaryWindow& output)
{
    if (output.renderer != nullptr) {
        SDL_DestroyRenderer(output.renderer);
    }
    if (output.window != nullptr) {
        SDL_DestroyWindow(output.window);
    }
    output = {};
}

[[nodiscard]] bool create_auxiliary_window(
    const char* const title,
    const int width,
    const int height,
    const devils_crush::cabinet::WindowPlacement& placement,
    const HostDisplays& displays,
    AuxiliaryWindow& output)
{
    output.window = SDL_CreateWindow(
        title,
        width,
        height,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY
            | SDL_WINDOW_NOT_FOCUSABLE
            | SDL_WINDOW_ALWAYS_ON_TOP);
    if (output.window == nullptr) {
        report_sdl_error("SDL_CreateWindow (cabinet auxiliary) failed");
        return false;
    }
    if (!apply_window_placement(
            output.window,
            placement,
            displays,
            width,
            height)) {
        destroy_auxiliary_window(output);
        return false;
    }
    output.renderer = SDL_CreateRenderer(output.window, nullptr);
    if (output.renderer == nullptr) {
        report_sdl_error("SDL_CreateRenderer (cabinet auxiliary) failed");
        destroy_auxiliary_window(output);
        return false;
    }
    (void)SDL_SetRenderVSync(output.renderer, 0);
    if (!SDL_ShowWindow(output.window)) {
        report_sdl_error("SDL_ShowWindow (cabinet auxiliary) failed");
        destroy_auxiliary_window(output);
        return false;
    }
    return true;
}

enum class AssignmentRole {
    playfield,
    dmd,
    backglass,
};

constexpr std::int32_t kBackglassDialogSelected = 1;
constexpr std::int32_t kBackglassDialogCancelled = 2;
constexpr std::int32_t kBackglassDialogFailed = 3;
constexpr std::array kBackglassDialogFilters{
    SDL_DialogFileFilter{
        "Backglass images",
        "png;jpg;jpeg;webp",
    },
};

void SDLCALL backglass_dialog_callback(
    void*,
    const char* const* file_list,
    int)
{
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    if (file_list == nullptr) {
        event.user.code = kBackglassDialogFailed;
        event.user.data1 = SDL_strdup(SDL_GetError());
    } else if (file_list[0] == nullptr) {
        event.user.code = kBackglassDialogCancelled;
    } else {
        event.user.code = kBackglassDialogSelected;
        event.user.data1 = SDL_strdup(file_list[0]);
    }
    if (!SDL_PushEvent(&event) && event.user.data1 != nullptr) {
        SDL_free(event.user.data1);
    }
}

[[nodiscard]] SDL_Window* assignment_window(
    const AssignmentRole role,
    SDL_Window* const playfield,
    AuxiliaryWindow& dmd,
    AuxiliaryWindow& backglass) noexcept
{
    switch (role) {
    case AssignmentRole::playfield:
        return playfield;
    case AssignmentRole::dmd:
        return dmd.window;
    case AssignmentRole::backglass:
        return backglass.window;
    }
    return nullptr;
}

[[nodiscard]] devils_crush::cabinet::WindowPlacement&
assignment_placement(
    const AssignmentRole role,
    devils_crush::cabinet::DisplayConfig& config) noexcept
{
    switch (role) {
    case AssignmentRole::playfield:
        return config.playfield;
    case AssignmentRole::dmd:
        return config.dmd;
    case AssignmentRole::backglass:
        return config.backglass;
    }
    return config.playfield;
}

[[nodiscard]] std::pair<int, int> assignment_default_size(
    const AssignmentRole role) noexcept
{
    switch (role) {
    case AssignmentRole::playfield:
        return {720, 1280};
    case AssignmentRole::dmd:
        return {960, 240};
    case AssignmentRole::backglass:
        return {960, 540};
    }
    return {640, 480};
}

void set_assignment_titles(
    SDL_Window* const playfield,
    AuxiliaryWindow& dmd,
    AuxiliaryWindow& backglass,
    const bool active,
    const AssignmentRole selected)
{
    if (!active) {
        (void)SDL_SetWindowTitle(
            playfield,
            "Devil's Crush - Cabinet Playfield Preview");
        if (dmd.window != nullptr) {
            (void)SDL_SetWindowTitle(
                dmd.window,
                "Devil's Crush - Cabinet DMD");
        }
        if (backglass.window != nullptr) {
            (void)SDL_SetWindowTitle(
                backglass.window,
                "Devil's Crush - Cabinet Backglass");
        }
        return;
    }
    const auto title = [selected](
                           const AssignmentRole role,
                           const char* const name) {
        std::string value = role == selected ? "[SELECTED] " : "";
        value += name;
        value +=
            " - 1/2/3 select, arrows display, F fullscreen, "
            "B image, Delete clears, Enter done";
        return value;
    };
    const auto playfield_title =
        title(AssignmentRole::playfield, "Cabinet setup: Playfield");
    (void)SDL_SetWindowTitle(playfield, playfield_title.c_str());
    if (dmd.window != nullptr) {
        const auto dmd_title =
            title(AssignmentRole::dmd, "Cabinet setup: DMD");
        (void)SDL_SetWindowTitle(dmd.window, dmd_title.c_str());
    }
    if (backglass.window != nullptr) {
        const auto backglass_title =
            title(AssignmentRole::backglass, "Cabinet setup: Backglass");
        (void)SDL_SetWindowTitle(
            backglass.window,
            backglass_title.c_str());
    }
}

[[nodiscard]] bool cycle_assignment_display(
    const AssignmentRole role,
    const int direction,
    SDL_Window* const playfield,
    AuxiliaryWindow& dmd,
    AuxiliaryWindow& backglass,
    devils_crush::cabinet::DisplayConfig& config,
    const HostDisplays& displays)
{
    SDL_Window* const window =
        assignment_window(role, playfield, dmd, backglass);
    if (window == nullptr || displays.ids.empty()
        || displays.ids.size() != displays.descriptors.size()) {
        return true;
    }
    const auto current_id = SDL_GetDisplayForWindow(window);
    std::size_t current = 0U;
    for (std::size_t index = 0; index < displays.ids.size(); ++index) {
        if (displays.ids[index] == current_id) {
            current = index;
            break;
        }
    }
    const auto count = static_cast<std::int64_t>(displays.ids.size());
    auto next =
        (static_cast<std::int64_t>(current) + direction) % count;
    if (next < 0) {
        next += count;
    }
    const auto target = static_cast<std::size_t>(next);
    auto& placement = assignment_placement(role, config);
    placement.display_identity = displays.descriptors[target].identity;
    placement.rotation = displays.descriptors[target].rotation;
    const auto [default_width, default_height] =
        assignment_default_size(role);
    const auto& bounds = displays.descriptors[target].bounds;
    const auto width = std::min(default_width, bounds.width);
    const auto height = std::min(default_height, bounds.height);
    placement.geometry = {
        .x = bounds.x + (bounds.width - width) / 2,
        .y = bounds.y + (bounds.height - height) / 2,
        .width = width,
        .height = height,
    };
    return apply_window_placement(
        window,
        placement,
        displays,
        default_width,
        default_height);
}

[[nodiscard]] bool present_assignment(
    SDL_Renderer* const renderer,
    const AssignmentRole role,
    const bool selected)
{
    if (renderer == nullptr) {
        return true;
    }
    std::array<std::uint8_t, 3> color{};
    switch (role) {
    case AssignmentRole::playfield:
        color = {24U, 72U, 160U};
        break;
    case AssignmentRole::dmd:
        color = {184U, 56U, 8U};
        break;
    case AssignmentRole::backglass:
        color = {96U, 32U, 128U};
        break;
    }
    if (!SDL_SetRenderDrawColor(
            renderer,
            color[0],
            color[1],
            color[2],
            255U)
        || !SDL_RenderClear(renderer)) {
        return report_sdl_error("Clearing a cabinet assignment window failed");
    }
    int width = 0;
    int height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &width, &height)) {
        return report_sdl_error(
            "Reading a cabinet assignment window size failed");
    }
    if (selected && width > 0 && height > 0) {
        const auto border =
            std::max(4.0F, std::min(width, height) / 32.0F);
        const auto inside_width =
            std::max(0.0F, static_cast<float>(width) - 2.0F * border);
        const auto inside_height =
            std::max(0.0F, static_cast<float>(height) - 2.0F * border);
        const std::array marker{
            SDL_FRect{border, border, inside_width, border},
            SDL_FRect{
                border,
                static_cast<float>(height) - 2.0F * border,
                inside_width,
                border,
            },
            SDL_FRect{border, border, border, inside_height},
            SDL_FRect{
                static_cast<float>(width) - 2.0F * border,
                border,
                border,
                inside_height,
            },
        };
        if (!SDL_SetRenderDrawColor(renderer, 255U, 240U, 96U, 255U)
            || !SDL_RenderFillRects(
                renderer,
                marker.data(),
                static_cast<int>(marker.size()))) {
            return report_sdl_error(
                "Drawing a cabinet assignment marker failed");
        }
    }
    return SDL_RenderPresent(renderer)
        || report_sdl_error(
            "Presenting a cabinet assignment window failed");
}

[[nodiscard]] bool present_black(AuxiliaryWindow& output)
{
    if (output.renderer == nullptr) {
        return true;
    }
    return (
               SDL_SetRenderDrawColor(
                   output.renderer,
                   0,
                   0,
                   0,
                   255)
               && SDL_RenderClear(output.renderer)
               && SDL_RenderPresent(output.renderer))
        || report_sdl_error("Rendering the black backglass failed");
}

[[nodiscard]] bool draw_dmd_segment(
    SDL_Renderer* renderer,
    const SDL_FRect& rectangle,
    const bool active)
{
    if (!SDL_SetRenderDrawColor(
            renderer,
            active ? 255U : 40U,
            active ? 48U : 4U,
            0U,
            255U)
        || !SDL_RenderFillRect(renderer, &rectangle)) {
        return report_sdl_error("Rendering a DMD segment failed");
    }
    return true;
}

[[nodiscard]] bool draw_dmd_digit(
    SDL_Renderer* renderer,
    const int digit,
    const float x,
    const float y,
    const float width,
    const float height)
{
    constexpr std::array<std::uint8_t, 10> masks{
        0x3FU,
        0x06U,
        0x5BU,
        0x4FU,
        0x66U,
        0x6DU,
        0x7DU,
        0x07U,
        0x7FU,
        0x6FU,
    };
    if (digit < 0 || digit >= static_cast<int>(masks.size())) {
        return false;
    }
    const auto thickness = std::max(2.0F, height / 10.0F);
    const auto half = height / 2.0F;
    const std::array segments{
        SDL_FRect{
            x + thickness,
            y,
            width - 2.0F * thickness,
            thickness,
        },
        SDL_FRect{
            x + width - thickness,
            y + thickness,
            thickness,
            half - 1.5F * thickness,
        },
        SDL_FRect{
            x + width - thickness,
            y + half + 0.5F * thickness,
            thickness,
            half - 1.5F * thickness,
        },
        SDL_FRect{
            x + thickness,
            y + height - thickness,
            width - 2.0F * thickness,
            thickness,
        },
        SDL_FRect{
            x,
            y + half + 0.5F * thickness,
            thickness,
            half - 1.5F * thickness,
        },
        SDL_FRect{
            x,
            y + thickness,
            thickness,
            half - 1.5F * thickness,
        },
        SDL_FRect{
            x + thickness,
            y + half - thickness / 2.0F,
            width - 2.0F * thickness,
            thickness,
        },
    };
    for (std::size_t segment = 0; segment < segments.size(); ++segment) {
        if (!draw_dmd_segment(
                renderer,
                segments[segment],
                (masks[static_cast<std::size_t>(digit)]
                 & (1U << segment))
                    != 0U)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool present_dmd(
    AuxiliaryWindow& output,
    const std::uint64_t displayed_score)
{
    if (output.renderer == nullptr) {
        return true;
    }
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(
            output.renderer,
            &output_width,
            &output_height)) {
        return report_sdl_error("SDL_GetRenderOutputSize (DMD) failed");
    }
    if (output_width <= 0 || output_height <= 0) {
        return true;
    }
    if (!SDL_SetRenderDrawColor(output.renderer, 0, 0, 0, 255)
        || !SDL_RenderClear(output.renderer)) {
        return report_sdl_error("Clearing the DMD failed");
    }

    const auto digits = std::to_string(displayed_score);
    const auto digit_count = static_cast<float>(digits.size());
    const auto available_width = static_cast<float>(output_width) * 0.9F;
    const auto maximum_height = static_cast<float>(output_height) * 0.75F;
    auto digit_height = std::min(
        maximum_height,
        available_width
            / (digit_count * 0.72F - 0.12F)
            * (5.0F / 3.0F));
    auto digit_width = digit_height * 0.6F;
    auto gap = digit_width * 0.2F;
    const auto total_width =
        digit_count * digit_width + (digit_count - 1.0F) * gap;
    const auto origin_x =
        (static_cast<float>(output_width) - total_width) / 2.0F;
    const auto origin_y =
        (static_cast<float>(output_height) - digit_height) / 2.0F;

    for (std::size_t index = 0; index < digits.size(); ++index) {
        if (!draw_dmd_digit(
                output.renderer,
                digits[index] - '0',
                origin_x
                    + static_cast<float>(index) * (digit_width + gap),
                origin_y,
                digit_width,
                digit_height)) {
            return false;
        }
    }
    if (!SDL_RenderPresent(output.renderer)) {
        return report_sdl_error("SDL_RenderPresent (DMD) failed");
    }
    return true;
}

[[nodiscard]] std::optional<devils_crush::render::PceBackgroundLayer>
decode_cabinet_table(
    const std::span<const std::byte> rom_bytes,
    const std::span<const std::byte> work_ram,
    const devils_crush::emulation::VideoMemorySnapshot& video,
    devils_crush::render::PersistentActorSpriteCache&
        persistent_actor_sprites)
{
    constexpr std::size_t table_width_tiles = 32U;
    constexpr std::size_t table_height_tiles = 96U;
    constexpr std::size_t table_bat_entries =
        table_width_tiles * table_height_tiles;
    constexpr std::size_t table_rom_offset =
        0x14U * 0x2000U + 0x000AU;
    constexpr std::size_t table_tile_bytes =
        table_bat_entries * 2U;
    constexpr std::size_t table_attribute_offset = 0x0259U;
    constexpr std::size_t table_attribute_bytes =
        table_bat_entries / 2U;
    if (rom_bytes.size() < table_rom_offset
        || rom_bytes.size() - table_rom_offset < table_tile_bytes
        || work_ram.size() < table_attribute_offset
        || work_ram.size() - table_attribute_offset
            < table_attribute_bytes) {
        return std::nullopt;
    }

    const auto source =
        devils_crush::render::decode_packed_pce_background_layer({
            .tile_words =
                rom_bytes.subspan(table_rom_offset, table_tile_bytes),
            .packed_palette_nibbles =
                work_ram.subspan(
                    table_attribute_offset,
                    table_attribute_bytes),
            .vram_words = video.vram_words,
            .palette_entries = video.palette_entries,
            .width_tiles = static_cast<int>(table_width_tiles),
            .height_tiles = static_cast<int>(table_height_tiles),
        });
    if (!source) {
        return std::nullopt;
    }
    auto table = devils_crush::render::crop_background_layer(
        *source,
        0,
        devils_crush::cabinet::kTableWorldTop,
        devils_crush::cabinet::kTableWorldWidth,
        devils_crush::cabinet::kTableWorldHeight);
    if (!table || work_ram.size() <= 0x0CU) {
        return table;
    }

    const auto camera_y = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(work_ram[0x0BU]))
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(work_ram[0x0CU]))
           << 8U));
    static_cast<void>(
        persistent_actor_sprites.update_and_composite(
            table->image,
            {
                .vram_words = video.vram_words,
                .sat_words = video.sat_words,
                .palette_entries = video.palette_entries,
                .memory_width_register =
                    video.memory_width_register,
            },
            work_ram,
            480 + static_cast<int>(camera_y),
            devils_crush::cabinet::kTableWorldTop,
            30,
            208,
            table->opaque_pixels,
            rom_bytes));
    static_cast<void>(
        devils_crush::render::composite_pce_sprites(
            table->image,
            {
                .vram_words = video.vram_words,
                .sat_words = video.sat_words,
                .palette_entries = video.palette_entries,
                .memory_width_register =
                    video.memory_width_register,
            },
            480 + static_cast<int>(camera_y),
            devils_crush::cabinet::kTableWorldTop,
            30,
            208,
            table->opaque_pixels));
    return table;
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

using RecordingInputFunction = InputFrame (*)(int);

struct RecordingPlan {
    std::string_view format;
    std::string_view label;
    std::string_view input_script;
    int frame_count = 0;
    RecordingInputFunction input_for_frame = nullptr;
};

struct RoutineReplayCapture {
    RoutineReplayRequest request;
    std::uint64_t natural_entry_count = 0;
    RoutineEntry checkpoint_entry;
    devils_crush::emulation::SaveState checkpoint;
    std::vector<std::byte> pre_call_work_ram;
    std::optional<RoutineCall> original_call;
    std::vector<std::byte> original_post_call_work_ram;
    std::optional<RoutineCall> replayed_call;
    std::vector<std::byte> replayed_post_call_work_ram;
};

constexpr RecordingPlan kCanonicalRecordingPlan{
    .format = "devils-crush-canonical-v1",
    .label = "Canonical",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421;two:480-481;left+one:540-541",
    .frame_count = kCanonicalRecordingFrames,
    .input_for_frame = &canonical_input_for_frame,
};

constexpr RecordingPlan kGameplayRecordingPlan{
    .format = "devils-crush-gameplay-v1",
    .label = "Gameplay",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,"
        "1000-1001,1100-1101,1200-1201,1300-1301,"
        "1400-1401,1500-1501,1600-1601,1700-1701;"
        "left:1000-1001,1100-1101,1200-1201,1300-1301,"
        "1400-1401,1500-1501,1600-1601,1700-1701;"
        "two:1050-1051,1250-1251,1450-1451,1650-1651",
    .frame_count = kGameplayRecordingFrames,
    .input_for_frame = &gameplay_input_for_frame,
};

constexpr RecordingPlan kStateCensusRecordingPlan{
    .format = "devils-crush-state-census-v1",
    .label = "State census",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,"
        "1000-1180,1265-1266+30n;"
        "left:1250-1251+30n;"
        "two:1450-1451,2250-2251,3050-3051",
    .frame_count = kStateCensusRecordingFrames,
    .input_for_frame = &state_census_input_for_frame,
};

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
    const std::span<const RoutineKey> routines,
    const std::span<const RoutineEntry> entries,
    const std::string_view core_revision)
{
    if (routines.empty()) {
        return false;
    }

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
        << "# format=devils-crush-routine-entry-v"
        << (routines.size() == 1U ? 1 : 2) << '\n'
        << "# rom_sha256=" << devils_crush::rom::kSupportedSha256 << '\n'
        << "# core_revision=" << core_revision << '\n';
    if (routines.size() == 1U) {
        output << "# physical_bank=" << std::uppercase << std::hex
               << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(routines.front().physical_bank)
               << '\n'
               << "# logical_pc=" << std::setw(4)
               << routines.front().logical_pc << '\n';
    } else {
        output << "# routine_count=" << std::dec << routines.size() << '\n';
        std::size_t index = 0U;
        for (const auto routine : routines) {
            output << "# routine_" << ++index << '=' << std::uppercase
                   << std::hex << std::setfill('0') << std::setw(2)
                   << static_cast<unsigned>(routine.physical_bank) << ':'
                   << std::setw(4) << routine.logical_pc << '\n';
        }
    }
    output
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

[[nodiscard]] bool write_routine_call_trace(
    const std::filesystem::path& path,
    const std::span<const RoutineKey> routines,
    const std::span<const RoutineCall> calls,
    const std::string_view core_revision)
{
    if (routines.empty()) {
        return false;
    }

    auto writes_path = path;
    writes_path += ".writes.csv";
    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create routine-call trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    auto writes_temporary_path = writes_path;
    writes_temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    std::ofstream writes_output(
        writes_temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output || !writes_output) {
        std::cerr << "Could not create routine-call trace outputs.\n";
        return false;
    }

    const auto write_metadata =
        [&](std::ofstream& stream, const std::string_view format) {
            stream << "# format=" << format << '\n'
                   << "# rom_sha256="
                   << devils_crush::rom::kSupportedSha256 << '\n'
                   << "# core_revision=" << core_revision << '\n'
                   << "# routine_count=" << routines.size() << '\n';
            std::size_t index = 0U;
            for (const auto routine : routines) {
                stream << "# routine_" << ++index << '='
                       << std::uppercase << std::hex << std::setfill('0')
                       << std::setw(2)
                       << static_cast<unsigned>(routine.physical_bank)
                       << ':' << std::setw(4) << routine.logical_pc
                       << '\n';
            }
            stream << std::dec;
        };

    write_metadata(output, "devils-crush-routine-call-v1");
    output << "# writes_suffix=.writes.csv\n"
           << "sequence,frame,entry_master_cycle,"
              "entry_absolute_master_cycle,physical_bank,logical_pc,"
              "caller_pc,return_pc,return_physical_bank,"
              "return_instruction_pc,return_master_cycle,"
              "return_absolute_master_cycle,"
              "body_master_cycles,input_a,input_x,input_y,input_sp,"
              "input_status,input_mpr0,input_mpr1,input_mpr2,input_mpr3,"
              "input_mpr4,input_mpr5,input_mpr6,input_mpr7,"
              "output_a,output_x,output_y,output_sp,output_status,"
              "output_mpr0,output_mpr1,output_mpr2,output_mpr3,"
              "output_mpr4,output_mpr5,output_mpr6,output_mpr7,"
              "write_count\n";

    write_metadata(writes_output, "devils-crush-routine-write-v1");
    writes_output
        << "call_sequence,write_sequence,master_cycle,"
           "absolute_master_cycle,"
           "instruction_physical_bank,instruction_logical_pc,"
           "work_ram_offset,value\n";

    std::uint64_t call_sequence = 0U;
    std::uint64_t write_count = 0U;
    for (const auto& call : calls) {
        const auto sequence = ++call_sequence;
        const auto& input = call.entry.registers;
        const auto& output_registers = call.output_registers;
        output << sequence << ',' << call.entry.frame_number << ','
               << call.entry.master_cycle << ','
               << call.entry.absolute_master_cycle << ','
               << std::uppercase
               << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<unsigned>(
                      call.entry.routine.physical_bank)
               << ',' << std::setw(4) << call.entry.routine.logical_pc
               << ',' << std::setw(4) << call.caller_pc
               << ',' << std::setw(4) << call.return_pc
               << ',' << std::setw(2)
               << static_cast<unsigned>(
                      call.return_instruction.physical_bank)
               << ',' << std::setw(4)
               << call.return_instruction.logical_pc
               << std::dec << ',' << call.return_master_cycle
               << ',' << call.return_absolute_master_cycle
               << ',' << call.body_master_cycles << std::hex
               << ',' << std::setw(2)
               << static_cast<unsigned>(input.accumulator)
               << ',' << std::setw(2) << static_cast<unsigned>(input.x)
               << ',' << std::setw(2) << static_cast<unsigned>(input.y)
               << ',' << std::setw(2)
               << static_cast<unsigned>(input.stack_pointer)
               << ',' << std::setw(2)
               << static_cast<unsigned>(input.status);
        for (const auto mpr : input.mpr) {
            output << ',' << std::setw(2)
                   << static_cast<unsigned>(mpr);
        }
        output << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.accumulator)
               << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.x)
               << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.y)
               << ',' << std::setw(2)
               << static_cast<unsigned>(
                      output_registers.stack_pointer)
               << ',' << std::setw(2)
               << static_cast<unsigned>(output_registers.status);
        for (const auto mpr : output_registers.mpr) {
            output << ',' << std::setw(2)
                   << static_cast<unsigned>(mpr);
        }
        output << std::dec << ',' << call.work_ram_writes.size()
               << '\n';

        std::uint64_t write_sequence = 0U;
        for (const auto& write : call.work_ram_writes) {
            writes_output
                << sequence << ',' << ++write_sequence << ','
                << write.master_cycle << ','
                << write.absolute_master_cycle << ','
                << std::uppercase
                << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<unsigned>(
                       write.instruction.physical_bank)
                << ',' << std::setw(4)
                << write.instruction.logical_pc
                << ',' << std::setw(4) << write.work_ram_offset
                << ',' << std::setw(2)
                << static_cast<unsigned>(write.value)
                << std::dec << '\n';
            ++write_count;
        }
    }

    output.close();
    writes_output.close();
    if (!output || !writes_output) {
        std::cerr << "Could not finalize routine-call trace outputs.\n";
        return false;
    }
    if (!MoveFileExW(
            writes_temporary_path.c_str(),
            writes_path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr
            << "Could not commit routine-write trace; Windows error "
            << GetLastError() << ".\n";
        return false;
    }
    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit routine-call trace; Windows error "
                  << GetLastError() << ".\n";
        return false;
    }

    std::cout << "Routine call trace complete: " << calls.size()
              << " calls, " << write_count << " writes\nCalls: "
              << path.string() << "\nWrites: " << writes_path.string()
              << '\n';
    return true;
}

[[nodiscard]] bool write_work_ram_write_trace(
    const std::filesystem::path& path,
    const std::span<const LogicalAddressRange> ranges,
    const std::span<const ObservedMemoryWrite> writes,
    const std::string_view core_revision)
{
    if (ranges.empty()) {
        return false;
    }

    std::error_code error;
    const auto directory = path.parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory, error);
    }
    if (error) {
        std::cerr << "Could not create work-RAM write trace directory: "
                  << error.message() << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    std::ofstream output(
        temporary_path,
        std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Could not create work-RAM write trace: "
                  << temporary_path.string() << '\n';
        return false;
    }

    output << "# format=devils-crush-work-ram-write-v1\n"
           << "# rom_sha256=" << devils_crush::rom::kSupportedSha256
           << '\n'
           << "# core_revision=" << core_revision << '\n'
           << "# range_count=" << ranges.size() << '\n';
    std::size_t range_index = 0U;
    for (const auto range : ranges) {
        output << "# range_" << ++range_index << '='
               << std::uppercase << std::hex << std::setfill('0')
               << std::setw(4) << range.first << '-' << std::setw(4)
               << range.last << '\n';
    }
    output
        << std::dec
        << "sequence,frame,master_cycle,absolute_master_cycle,"
           "instruction_physical_bank,instruction_logical_pc,"
           "logical_address,work_ram_offset,value\n";

    std::uint64_t sequence = 0U;
    for (const auto& write : writes) {
        output << ++sequence << ',' << write.frame_number << ','
               << write.master_cycle << ','
               << write.absolute_master_cycle << ','
               << std::uppercase << std::hex << std::setfill('0')
               << std::setw(2)
               << static_cast<unsigned>(
                      write.instruction.physical_bank)
               << ',' << std::setw(4)
               << write.instruction.logical_pc << ',' << std::setw(4)
               << write.logical_address << ',' << std::setw(4)
               << write.work_ram_offset << ',' << std::setw(2)
               << static_cast<unsigned>(write.value)
               << std::dec << '\n';
    }

    output.close();
    if (!output) {
        std::cerr << "Could not finalize work-RAM write trace.\n";
        return false;
    }
    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr
            << "Could not commit work-RAM write trace; Windows error "
            << GetLastError() << ".\n";
        return false;
    }

    std::cout << "Work-RAM write trace complete: " << writes.size()
              << " selected writes\nOutput: " << path.string() << '\n';
    return true;
}

int run_deterministic_recording(
    PceRuntime& runtime,
    const std::filesystem::path& path,
    const std::filesystem::path& work_ram_path,
    const std::filesystem::path& framebuffer_path,
    const std::filesystem::path& audio_path,
    const std::string_view core_revision,
    const RecordingPlan& plan,
    const std::span<const WorkRamPatch> work_ram_patches)
{
    if (runtime.work_ram_size() == 0U || plan.frame_count <= 0
        || plan.input_for_frame == nullptr || plan.format.empty()
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

    runtime.reset();
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    int audio_sample_rate = 0;
    std::uint32_t audio_data_bytes = 0U;
    std::vector<std::byte> framebuffer_rgb;
    for (int index = 1; index <= plan.frame_count; ++index) {
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
        const auto input = plan.input_for_frame(index);
        const auto frame = runtime.run_frame(input);
        const auto work_ram = runtime.export_work_ram();
        if (frame.frame_number != static_cast<std::uint64_t>(index)
            || frame.framebuffer.pixels.empty()
            || frame.framebuffer.width <= 0
            || frame.framebuffer.height <= 0
            || work_ram.size() != runtime.work_ram_size()) {
            std::cerr << plan.label << " recording failed on frame " << index
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
    if (!MoveFileExW(
            temporary_path.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Could not commit " << plan.label
                  << " recording; Windows error "
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
        std::cerr << "Could not commit " << plan.label
                  << " audio; Windows error "
                  << GetLastError() << ".\n";
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

[[nodiscard]] std::string routine_replay_name(
    const RoutineReplayRequest& request)
{
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0')
           << std::setw(2)
           << static_cast<unsigned>(request.routine.physical_bank)
           << ':' << std::setw(4) << request.routine.logical_pc
           << std::dec << '@' << request.ordinal;
    return output.str();
}

void report_routine_call_difference(
    const RoutineCall& original,
    const RoutineCall& replayed)
{
    std::cerr
        << "  entry frame/cycle/absolute: "
        << original.entry.frame_number << '/'
        << original.entry.master_cycle << '/'
        << original.entry.absolute_master_cycle << " vs "
        << replayed.entry.frame_number << '/'
        << replayed.entry.master_cycle << '/'
        << replayed.entry.absolute_master_cycle << '\n'
        << "  input registers match: "
        << (original.entry.registers == replayed.entry.registers
                ? "yes" : "no") << '\n'
        << "  caller/return: " << std::uppercase << std::hex
        << original.caller_pc << '/' << original.return_pc << '/'
        << static_cast<unsigned>(
               original.return_instruction.physical_bank)
        << ':' << original.return_instruction.logical_pc << " vs "
        << replayed.caller_pc << '/' << replayed.return_pc << '/'
        << static_cast<unsigned>(
               replayed.return_instruction.physical_bank)
        << ':' << replayed.return_instruction.logical_pc << std::dec << '\n'
        << "  return cycle/absolute/body: "
        << original.return_master_cycle << '/'
        << original.return_absolute_master_cycle << '/'
        << original.body_master_cycles << " vs "
        << replayed.return_master_cycle << '/'
        << replayed.return_absolute_master_cycle << '/'
        << replayed.body_master_cycles << '\n'
        << "  output registers match: "
        << (original.output_registers == replayed.output_registers
                ? "yes" : "no") << '\n'
        << "  ordered writes: " << original.work_ram_writes.size()
        << " vs " << replayed.work_ram_writes.size() << '\n';
    const auto common_count = std::min(
        original.work_ram_writes.size(),
        replayed.work_ram_writes.size());
    for (std::size_t index = 0; index < common_count; ++index) {
        if (original.work_ram_writes[index]
            != replayed.work_ram_writes[index]) {
            const auto& left = original.work_ram_writes[index];
            const auto& right = replayed.work_ram_writes[index];
            std::cerr << "  first write difference at " << index + 1U
                      << ": offset/value/cycle "
                      << std::uppercase << std::hex
                      << left.work_ram_offset << '/'
                      << static_cast<unsigned>(left.value) << std::dec
                      << '/' << left.absolute_master_cycle << " vs "
                      << std::uppercase << std::hex
                      << right.work_ram_offset << '/'
                      << static_cast<unsigned>(right.value) << std::dec
                      << '/' << right.absolute_master_cycle << '\n';
            break;
        }
    }
    for (std::size_t index = 0; index < common_count; ++index) {
        const auto& left = original.work_ram_writes[index];
        const auto& right = replayed.work_ram_writes[index];
        const auto left_offset = left.absolute_master_cycle
            - original.entry.absolute_master_cycle;
        const auto right_offset = right.absolute_master_cycle
            - replayed.entry.absolute_master_cycle;
        if (left.instruction != right.instruction
            || left.logical_address != right.logical_address
            || left.work_ram_offset != right.work_ram_offset
            || left.value != right.value
            || left.frame_number != right.frame_number
            || left_offset != right_offset) {
            std::cerr << "  first normalized write difference at "
                      << index + 1U << ": instruction/address/offset/value/"
                         "frame/relative-cycle "
                      << std::uppercase << std::hex
                      << static_cast<unsigned>(
                             left.instruction.physical_bank)
                      << ':' << left.instruction.logical_pc << '/'
                      << left.logical_address << '/'
                      << left.work_ram_offset << '/'
                      << static_cast<unsigned>(left.value) << std::dec
                      << '/' << left.frame_number << '/' << left_offset
                      << " vs " << std::uppercase << std::hex
                      << static_cast<unsigned>(
                             right.instruction.physical_bank)
                      << ':' << right.instruction.logical_pc << '/'
                      << right.logical_address << '/'
                      << right.work_ram_offset << '/'
                      << static_cast<unsigned>(right.value) << std::dec
                      << '/' << right.frame_number << '/' << right_offset
                      << '\n';
            break;
        }
    }
}

[[nodiscard]] bool routine_calls_match_after_clock_normalization(
    const RoutineCall& original,
    const RoutineCall& replayed)
{
    if (original.entry.routine != replayed.entry.routine
        || original.entry.registers != replayed.entry.registers
        || original.entry.frame_number != replayed.entry.frame_number
        || original.return_instruction != replayed.return_instruction
        || original.output_registers != replayed.output_registers
        || original.caller_pc != replayed.caller_pc
        || original.return_pc != replayed.return_pc
        || original.body_master_cycles != replayed.body_master_cycles
        || original.work_ram_writes.size()
            != replayed.work_ram_writes.size()) {
        return false;
    }

    const auto original_return_offset =
        original.return_absolute_master_cycle
        - original.entry.absolute_master_cycle;
    const auto replayed_return_offset =
        replayed.return_absolute_master_cycle
        - replayed.entry.absolute_master_cycle;
    if (original_return_offset != replayed_return_offset) {
        return false;
    }

    for (std::size_t index = 0;
         index < original.work_ram_writes.size();
         ++index) {
        const auto& left = original.work_ram_writes[index];
        const auto& right = replayed.work_ram_writes[index];
        if (left.instruction != right.instruction
            || left.logical_address != right.logical_address
            || left.work_ram_offset != right.work_ram_offset
            || left.value != right.value
            || left.frame_number != right.frame_number
            || left.absolute_master_cycle
                    - original.entry.absolute_master_cycle
                != right.absolute_master_cycle
                    - replayed.entry.absolute_master_cycle) {
            return false;
        }
    }
    return true;
}

int verify_routine_replays(
    PceRuntime& runtime,
    const RecordingPlan& plan,
    std::span<RoutineReplayCapture> captures,
    RoutineReplayCapture*& active_replay,
    bool& suppress_recording_observers)
{
    for (auto& capture : captures) {
        const auto name = routine_replay_name(capture.request);
        if (capture.checkpoint.core_state.empty()
            || capture.pre_call_work_ram.empty()
            || !capture.original_call
            || capture.original_post_call_work_ram.empty()) {
            std::cerr << "Routine replay " << name
                      << " did not capture a complete original call.\n";
            return EXIT_FAILURE;
        }
        if (capture.checkpoint_entry.frame_number == 0U
            || capture.checkpoint_entry.frame_number
                > static_cast<std::uint64_t>(plan.frame_count)) {
            std::cerr << "Routine replay " << name
                      << " captured an invalid frame number.\n";
            return EXIT_FAILURE;
        }

        active_replay = &capture;
        suppress_recording_observers = true;
        if (!runtime.deserialize(capture.checkpoint)) {
            std::cerr << "Routine replay " << name
                      << " could not restore its pre-call savestate.\n";
            return EXIT_FAILURE;
        }
        if (runtime.export_work_ram() != capture.pre_call_work_ram) {
            std::cerr << "Routine replay " << name
                      << " restored different pre-call work RAM.\n";
            return EXIT_FAILURE;
        }

        const auto frame_number =
            static_cast<int>(capture.checkpoint_entry.frame_number);
        const auto frame = runtime.run_frame(
            plan.input_for_frame(frame_number));
        if (frame.frame_number
                != capture.checkpoint_entry.frame_number
            || !capture.replayed_call
            || capture.replayed_post_call_work_ram.empty()) {
            std::cerr << "Routine replay " << name
                      << " did not complete in its restored frame.\n";
            return EXIT_FAILURE;
        }
        if (!routine_calls_match_after_clock_normalization(
                *capture.original_call,
                *capture.replayed_call)) {
            std::cerr << "Routine replay " << name
                      << " changed registers, return state, relative "
                         "cycles, or ordered work-RAM writes.\n";
            report_routine_call_difference(
                *capture.original_call,
                *capture.replayed_call);
            return EXIT_FAILURE;
        }
        if (capture.replayed_post_call_work_ram
            != capture.original_post_call_work_ram) {
            std::cerr << "Routine replay " << name
                      << " changed post-call work RAM.\n";
            return EXIT_FAILURE;
        }

        std::cout << "Routine replay " << name << " verified: frame "
                  << capture.checkpoint_entry.frame_number << ", "
                  << capture.original_call->body_master_cycles
                  << " body cycles, "
                  << capture.original_call->work_ram_writes.size()
                  << " ordered writes\n";
        active_replay = nullptr;
        suppress_recording_observers = false;
    }

    std::cout << "Routine replay complete: " << captures.size()
              << " state-normalized calls\n";
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

int run_windows_host(
    PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const bool cabinet_mode,
    const std::optional<std::filesystem::path> backglass_override =
        std::nullopt,
    const int frame_limit = 0)
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

    const auto cabinet_rom =
        cabinet_mode ? read_rom_bytes(rom_path) : std::nullopt;
    if (cabinet_mode && !cabinet_rom) {
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    devils_crush::cabinet::DisplayConfig cabinet_config;
    const bool persist_cabinet_config =
        cabinet_mode && frame_limit <= 0;
    if (persist_cabinet_config) {
        const auto loaded =
            devils_crush::cabinet::load_display_config(
                kCabinetConfigPath);
        if (loaded.loaded()) {
            cabinet_config = loaded.config;
            std::cout << "Loaded cabinet configuration: "
                      << kCabinetConfigPath.string() << '\n';
        } else if (
            loaded.status
            != devils_crush::cabinet::ConfigLoadStatus::not_found) {
            std::cerr << loaded.message
                      << " Using safe window defaults.\n";
        }
    }
    std::optional<std::filesystem::path> effective_backglass_path;
    if (cabinet_mode && backglass_override) {
        if (backglass_override->empty()) {
            cabinet_config.backglass_path.clear();
        } else {
            effective_backglass_path = *backglass_override;
        }
    } else if (!cabinet_config.backglass_path.empty()) {
        effective_backglass_path =
            path_from_utf8(cabinet_config.backglass_path);
    }
    auto host_displays =
        cabinet_mode ? enumerate_host_displays() : HostDisplays{};

    SDL_Window* window = SDL_CreateWindow(
        cabinet_mode
            ? "Devil's Crush - Cabinet Playfield Preview"
            : "Devil's Crush - emulated baseline",
        cabinet_mode ? 720 : 1280,
        cabinet_mode ? 1280 : 720,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        report_sdl_error("SDL_CreateWindow failed");
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    if (cabinet_mode
        && !apply_window_placement(
            window,
            cabinet_config.playfield,
            host_displays,
            720,
            1280)) {
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        report_sdl_error("SDL_CreateRenderer failed");
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    AuxiliaryWindow dmd;
    AuxiliaryWindow backglass;
    if (cabinet_mode) {
        if (!create_auxiliary_window(
                "Devil's Crush - Cabinet DMD",
                960,
                240,
                cabinet_config.dmd,
                host_displays,
                dmd)) {
            std::cerr
                << "Cabinet DMD unavailable; continuing with playfield.\n";
        }
        if (!create_auxiliary_window(
                "Devil's Crush - Cabinet Backglass",
                960,
                540,
                cabinet_config.backglass,
                host_displays,
                backglass)) {
            std::cerr
                << "Cabinet backglass unavailable; continuing without it.\n";
        }
    }

    const bool vsync_enabled = SDL_SetRenderVSync(renderer, 1);
    if (!vsync_enabled) {
        std::cerr << "VSync unavailable; using timer pacing: "
                  << SDL_GetError() << '\n';
    }
    if (!SDL_ShowWindow(window)) {
        report_sdl_error("SDL_ShowWindow failed");
        destroy_auxiliary_window(dmd);
        destroy_auxiliary_window(backglass);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        destroy_audio(audio);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Gamepad* gamepad = open_first_gamepad();
    SourceTexture source;
    StageTexture stage;
    SourceTexture backglass_logo_source;
    StageTexture backglass_logo_stage;
    SourceTexture backglass_source;
    StageTexture backglass_stage;
    auto backglass_logo =
        cabinet_rom
        ? devils_crush::render::TitleLogoAnimation::decode(
              *cabinet_rom)
        : std::nullopt;
    if (cabinet_mode && !backglass_logo) {
        std::cerr
            << "Native title-logo decode failed; the backglass will "
               "use its static fallback.\n";
    }
    if (effective_backglass_path) {
        const bool loaded =
            backglass.renderer != nullptr
            ? load_backglass_texture(
                backglass.renderer,
                *effective_backglass_path,
                backglass_source)
            : devils_crush::render::
                  load_backglass_image(*effective_backglass_path)
                      .loaded();
        if (loaded && backglass_override) {
            cabinet_config.backglass_path =
                path_to_utf8(*effective_backglass_path);
        }
    }
    bool table_view = false;
    devils_crush::render::PersistentActorSpriteCache
        persistent_actor_sprites;
    std::uint64_t dmd_score = 0U;
    std::uint64_t backglass_logo_frame = 0U;
    bool assignment_active = false;
    bool backglass_dialog_open = false;
    auto assignment_role = AssignmentRole::playfield;
    const auto save_current_cabinet_config = [&]() {
        if (!persist_cabinet_config) {
            return;
        }
        cabinet_config.playfield = capture_window_placement(
            window,
            host_displays,
            cabinet_config.playfield);
        cabinet_config.dmd = capture_window_placement(
            dmd.window,
            host_displays,
            cabinet_config.dmd);
        cabinet_config.backglass = capture_window_placement(
            backglass.window,
            host_displays,
            cabinet_config.backglass);
        const auto saved =
            devils_crush::cabinet::save_display_config_atomic(
                kCabinetConfigPath,
                cabinet_config);
        if (!saved.saved) {
            std::cerr << saved.message << '\n';
        }
    };

    using Clock = std::chrono::steady_clock;
    const auto frame_period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / kEmulationFramesPerSecond));
    auto next_frame_time = Clock::now();

    bool running = true;
    bool host_ok = true;
    int emulated_frames = 0;
    while (running && host_ok) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_USER
                && (event.user.code == kBackglassDialogSelected
                    || event.user.code == kBackglassDialogCancelled
                    || event.user.code == kBackglassDialogFailed)) {
                backglass_dialog_open = false;
                char* const dialog_text =
                    static_cast<char*>(event.user.data1);
                if (event.user.code == kBackglassDialogSelected
                    && dialog_text != nullptr) {
                    const auto selected_path =
                        path_from_utf8(dialog_text);
                    bool selected = false;
                    if (backglass.renderer != nullptr) {
                        selected = load_backglass_texture(
                            backglass.renderer,
                            selected_path,
                            backglass_source);
                        if (selected) {
                            destroy_stage(backglass_stage);
                        }
                    } else {
                        selected =
                            devils_crush::render::
                                load_backglass_image(selected_path)
                                    .loaded();
                    }
                    if (selected) {
                        cabinet_config.backglass_path =
                            path_to_utf8(selected_path);
                        save_current_cabinet_config();
                    }
                } else if (
                    event.user.code == kBackglassDialogFailed) {
                    std::cerr << "Backglass file dialog failed: "
                              << (dialog_text != nullptr
                                      ? dialog_text
                                      : "unknown error")
                              << '\n';
                }
                SDL_free(dialog_text);
            } else if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                const auto main_window_id = SDL_GetWindowID(window);
                if (event.window.windowID == main_window_id) {
                    running = false;
                } else if (
                    dmd.window != nullptr
                    && event.window.windowID
                        == SDL_GetWindowID(dmd.window)) {
                    cabinet_config.dmd = capture_window_placement(
                        dmd.window,
                        host_displays,
                        cabinet_config.dmd);
                    destroy_auxiliary_window(dmd);
                } else if (
                    backglass.window != nullptr
                    && event.window.windowID
                        == SDL_GetWindowID(backglass.window)) {
                    cabinet_config.backglass = capture_window_placement(
                        backglass.window,
                        host_displays,
                        cabinet_config.backglass);
                    destroy_stage(backglass_logo_stage);
                    destroy_source(backglass_logo_source);
                    destroy_stage(backglass_stage);
                    destroy_source(backglass_source);
                    destroy_auxiliary_window(backglass);
                }
            } else if (
                cabinet_mode && event.type == SDL_EVENT_KEY_DOWN
                && !event.key.repeat && event.key.key == SDLK_F12) {
                assignment_active = !assignment_active;
                if (assignment_active) {
                    host_displays = enumerate_host_displays();
                    cabinet_config.playfield =
                        capture_window_placement(
                            window,
                            host_displays,
                            cabinet_config.playfield);
                    cabinet_config.dmd = capture_window_placement(
                        dmd.window,
                        host_displays,
                        cabinet_config.dmd);
                    cabinet_config.backglass =
                        capture_window_placement(
                            backglass.window,
                            host_displays,
                            cabinet_config.backglass);
                }
                set_assignment_titles(
                    window,
                    dmd,
                    backglass,
                    assignment_active,
                    assignment_role);
                if (!assignment_active) {
                    save_current_cabinet_config();
                }
                next_frame_time = Clock::now();
            } else if (
                assignment_active
                && event.type == SDL_EVENT_KEY_DOWN
                && !event.key.repeat) {
                if (event.key.key == SDLK_RETURN
                    || event.key.key == SDLK_ESCAPE) {
                    assignment_active = false;
                    save_current_cabinet_config();
                } else if (event.key.key == SDLK_1) {
                    assignment_role = AssignmentRole::playfield;
                } else if (event.key.key == SDLK_2) {
                    assignment_role = AssignmentRole::dmd;
                } else if (event.key.key == SDLK_3) {
                    assignment_role = AssignmentRole::backglass;
                } else if (
                    event.key.key == SDLK_LEFT
                    || event.key.key == SDLK_RIGHT) {
                    host_ok = cycle_assignment_display(
                        assignment_role,
                        event.key.key == SDLK_RIGHT ? 1 : -1,
                        window,
                        dmd,
                        backglass,
                        cabinet_config,
                        host_displays);
                } else if (event.key.key == SDLK_F) {
                    SDL_Window* const selected_window =
                        assignment_window(
                            assignment_role,
                            window,
                            dmd,
                            backglass);
                    if (selected_window != nullptr) {
                        auto& placement = assignment_placement(
                            assignment_role,
                            cabinet_config);
                        placement.fullscreen =
                            (SDL_GetWindowFlags(selected_window)
                             & SDL_WINDOW_FULLSCREEN)
                            == 0U;
                        if (!SDL_SetWindowFullscreen(
                                selected_window,
                                placement.fullscreen)) {
                            host_ok = report_sdl_error(
                                "Changing an assigned window's "
                                "fullscreen state failed");
                        }
                    }
                } else if (
                    event.key.key == SDLK_B
                    && !backglass_dialog_open) {
                    const char* const default_location =
                        cabinet_config.backglass_path.empty()
                        ? nullptr
                        : cabinet_config.backglass_path.c_str();
                    backglass_dialog_open = true;
                    SDL_ShowOpenFileDialog(
                        backglass_dialog_callback,
                        nullptr,
                        window,
                        kBackglassDialogFilters.data(),
                        static_cast<int>(
                            kBackglassDialogFilters.size()),
                        default_location,
                        false);
                } else if (
                    event.key.key == SDLK_DELETE
                    && assignment_role == AssignmentRole::backglass) {
                    destroy_stage(backglass_stage);
                    destroy_source(backglass_source);
                    cabinet_config.backglass_path.clear();
                    save_current_cabinet_config();
                }
                set_assignment_titles(
                    window,
                    dmd,
                    backglass,
                    assignment_active,
                    assignment_role);
                next_frame_time = Clock::now();
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
                if (event.render.windowID == SDL_GetWindowID(window)) {
                    destroy_stage(stage);
                } else if (
                    backglass.window != nullptr
                    && event.render.windowID
                        == SDL_GetWindowID(backglass.window)) {
                    destroy_stage(backglass_logo_stage);
                    destroy_stage(backglass_stage);
                }
            } else if (event.type == SDL_EVENT_RENDER_DEVICE_RESET) {
                if (event.render.windowID == SDL_GetWindowID(window)) {
                    destroy_stage(stage);
                    destroy_source(source);
                    next_frame_time = Clock::now();
                } else if (
                    backglass.window != nullptr
                    && event.render.windowID
                        == SDL_GetWindowID(backglass.window)) {
                    destroy_stage(backglass_logo_stage);
                    destroy_source(backglass_logo_source);
                    destroy_stage(backglass_stage);
                    destroy_source(backglass_source);
                    if (!cabinet_config.backglass_path.empty()) {
                        (void)load_backglass_texture(
                            backglass.renderer,
                            path_from_utf8(
                                cabinet_config.backglass_path),
                            backglass_source);
                    }
                }
            }
        }

        auto now = Clock::now();
        int catch_up_frames = 0;
        constexpr int kMaximumCatchUpFrames = 4;
        while (running && host_ok && !assignment_active
               && now >= next_frame_time
               && catch_up_frames < kMaximumCatchUpFrames
               && (frame_limit <= 0 || emulated_frames < frame_limit)) {
            const auto frame = runtime.run_frame(read_input(gamepad));
            ++emulated_frames;
            if (!cabinet_mode) {
                host_ok =
                    consume_emulated_frame(renderer, frame, source, audio);
            } else if (
                frame.frame_number == 0U
                || frame.framebuffer.pixels.empty()) {
                std::cerr
                    << "Beetle PCE did not produce a framebuffer.\n";
                host_ok = false;
            } else {
                const auto work_ram = runtime.export_work_ram();
                const auto cabinet_state =
                    devils_crush::cabinet::decode_cabinet_state(
                        work_ram);
                if (cabinet_state) {
                    dmd_score = cabinet_state->displayed_score;
                }
                if (backglass_logo
                    && backglass.renderer != nullptr) {
                    const auto logo =
                        backglass_logo->render(
                            backglass_logo_frame++);
                    if (!logo
                        || !upload_image(
                            backglass.renderer,
                            *logo,
                            backglass_logo_source)) {
                        std::cerr
                            << "Native backglass logo renderer "
                               "failed; using the static fallback.\n";
                        backglass_logo.reset();
                        destroy_stage(backglass_logo_stage);
                        destroy_source(backglass_logo_source);
                    }
                }

                table_view = false;
                bool source_uploaded = false;
                if (cabinet_state
                    && cabinet_state->playfield
                        == devils_crush::cabinet::PlayfieldView::
                            table_background) {
                    const auto video = runtime.export_video_memory();
                    const auto table =
                        video
                        ? decode_cabinet_table(
                              *cabinet_rom,
                              work_ram,
                              *video,
                              persistent_actor_sprites)
                        : std::nullopt;
                    if (table) {
                        host_ok =
                            upload_image(
                                renderer,
                                table->image,
                                source);
                        source_uploaded = host_ok;
                        table_view = host_ok;
                    }
                }
                if (host_ok && !source_uploaded) {
                    persistent_actor_sprites.reset();
                    host_ok = upload_framebuffer(
                        renderer,
                        frame.framebuffer,
                        source);
                }
                if (host_ok) {
                    host_ok = queue_audio(frame.audio, audio);
                }
            }
            next_frame_time += frame_period;
            ++catch_up_frames;
            now = Clock::now();
        }
        if (catch_up_frames == kMaximumCatchUpFrames
            && now >= next_frame_time) {
            next_frame_time = now + frame_period;
        }
        if (assignment_active) {
            next_frame_time = now + frame_period;
        }

        if (running && host_ok) {
            if (assignment_active) {
                host_ok = present_assignment(
                    renderer,
                    AssignmentRole::playfield,
                    assignment_role == AssignmentRole::playfield);
            } else {
                const auto aspect_numerator =
                    table_view
                    ? devils_crush::cabinet::kTableAspectNumerator
                    : kDisplayAspectNumerator;
                const auto aspect_denominator =
                    table_view
                    ? devils_crush::cabinet::kTableAspectDenominator
                    : kDisplayAspectDenominator;
                host_ok = present_frame(
                    renderer,
                    source,
                    stage,
                    aspect_numerator,
                    aspect_denominator,
                    cabinet_mode ? kPureBlack : kDesktopFrameColor);
            }
        }
        if (running && dmd.renderer != nullptr
            && !(assignment_active
                    ? present_assignment(
                        dmd.renderer,
                        AssignmentRole::dmd,
                        assignment_role == AssignmentRole::dmd)
                    : present_dmd(dmd, dmd_score))) {
            std::cerr << "Cabinet DMD renderer lost; disabling DMD.\n";
            destroy_auxiliary_window(dmd);
        }
        if (running && backglass.renderer != nullptr
            && !(assignment_active
                    ? present_assignment(
                        backglass.renderer,
                        AssignmentRole::backglass,
                        assignment_role == AssignmentRole::backglass)
                    : backglass_logo_source.texture != nullptr
                        ? present_frame(
                            backglass.renderer,
                            backglass_logo_source,
                            backglass_logo_stage,
                            static_cast<std::uint32_t>(
                                devils_crush::render::
                                    kTitleLogoWidth * 5),
                            static_cast<std::uint32_t>(
                                devils_crush::render::
                                    kTitleLogoHeight * 4),
                            kPureBlack)
                    : backglass_source.texture != nullptr
                        ? present_frame(
                            backglass.renderer,
                            backglass_source,
                            backglass_stage,
                            static_cast<std::uint32_t>(
                                backglass_source.width),
                            static_cast<std::uint32_t>(
                                backglass_source.height),
                            kPureBlack)
                        : present_black(backglass))) {
            std::cerr
                << "Cabinet backglass renderer lost; disabling "
                   "backglass.\n";
            destroy_stage(backglass_logo_stage);
            destroy_source(backglass_logo_source);
            destroy_stage(backglass_stage);
            destroy_source(backglass_source);
            destroy_auxiliary_window(backglass);
        }
        if (frame_limit > 0 && emulated_frames >= frame_limit) {
            running = false;
        }
        if (!vsync_enabled) {
            SDL_Delay(1);
        }
    }

    destroy_audio(audio);
    destroy_stage(stage);
    destroy_source(source);
    destroy_stage(backglass_logo_stage);
    destroy_source(backglass_logo_source);
    destroy_stage(backglass_stage);
    destroy_source(backglass_source);
    if (gamepad != nullptr) {
        SDL_CloseGamepad(gamepad);
    }
    save_current_cabinet_config();
    destroy_auxiliary_window(dmd);
    destroy_auxiliary_window(backglass);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (host_ok && frame_limit > 0) {
        std::cout << "Cabinet host smoke test passed: "
                  << emulated_frames << " frames.\n";
    }
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

        const auto replacement_rom = options.replace_hooks.empty()
            ? std::optional<std::vector<std::byte>>{
                  std::vector<std::byte>{}}
            : read_rom_bytes(options.rom_path);
        if (!replacement_rom) {
            return EXIT_FAILURE;
        }

        if (options.record_canonical || options.record_gameplay
            || options.record_state_census) {
            const auto& recording_plan = options.record_state_census
                ? kStateCensusRecordingPlan
                : options.record_gameplay
                    ? kGameplayRecordingPlan
                    : kCanonicalRecordingPlan;
            const auto& recording_path = options.record_state_census
                ? options.state_census_path
                : options.record_gameplay
                    ? options.gameplay_path
                    : options.canonical_path;
            std::vector<std::uint64_t> pass_through_hits(
                options.pass_through_hooks.size());
            std::vector<std::uint64_t> replacement_hits(
                options.replace_hooks.size());
            std::vector<std::uint64_t> replacement_entries(
                options.replace_hooks.size());
            std::vector<RoutineEntry> observed_entries;
            std::vector<RoutineCall> observed_calls;
            std::vector<ObservedMemoryWrite> observed_work_ram_writes;
            std::vector<RoutineReplayCapture> routine_replays;
            routine_replays.reserve(options.routine_replays.size());
            for (const auto request : options.routine_replays) {
                routine_replays.push_back({
                    .request = request,
                });
            }
            RoutineReplayCapture* active_replay = nullptr;
            bool suppress_recording_observers = false;
            for (std::size_t index = 0U;
                 index < options.pass_through_hooks.size();
                 ++index) {
                if (runtime->install_execution_hook({
                    .routine = options.pass_through_hooks[index],
                    .mode = HookMode::pass_through,
                    .callback = [&pass_through_hits,
                                 index](const RoutineEntry&) {
                        ++pass_through_hits[index];
                    },
                })) {
                    continue;
                }
                std::cerr << "Could not install pass-through hook "
                          << index + 1U << ".\n";
                return EXIT_FAILURE;
            }
            for (std::size_t index = 0U;
                 index < options.observe_hooks.size();
                 ++index) {
                const auto replacement = std::find_if(
                    options.replace_hooks.begin(),
                    options.replace_hooks.end(),
                    [&options, index](
                        const RoutineReplayRequest& request) {
                        return request.routine
                            == options.observe_hooks[index];
                    });
                const bool replaces =
                    replacement != options.replace_hooks.end();
                const auto replacement_index = static_cast<std::size_t>(
                    std::distance(
                        options.replace_hooks.begin(),
                        replacement));
                devils_crush::emulation::ExecutionHook hook{
                    .routine = options.observe_hooks[index],
                    .mode = replaces
                        ? HookMode::replace
                        : HookMode::observe,
                    .callback = [&runtime,
                                 &observed_entries,
                                 &routine_replays,
                                 &active_replay,
                                 &suppress_recording_observers](
                                    const RoutineEntry& entry) {
                        if (active_replay != nullptr
                            || suppress_recording_observers) {
                            return;
                        }
                        const auto replay = std::find_if(
                            routine_replays.begin(),
                            routine_replays.end(),
                            [&entry](const RoutineReplayCapture& capture) {
                                return capture.request.routine
                                    == entry.routine;
                            });
                        if (replay != routine_replays.end()
                            && ++replay->natural_entry_count
                                == replay->request.ordinal) {
                            replay->checkpoint_entry = entry;
                            replay->checkpoint = runtime->serialize();
                            replay->pre_call_work_ram =
                                runtime->export_work_ram();
                        }
                        observed_entries.push_back(entry);
                    },
                };
                if (replaces) {
                    hook.replacement_callback =
                        [&replacement_hits,
                         &replacement_entries,
                         &replacement_rom,
                         &options,
                         replacement_index](
                            const RoutineEntry& entry,
                            const std::span<const std::byte> work_ram) {
                            const auto entry_number =
                                ++replacement_entries[
                                    replacement_index];
                            if (!options.replace_hooks[
                                     replacement_index]
                                     .all_calls
                                && entry_number
                                != options.replace_hooks[
                                       replacement_index]
                                       .ordinal) {
                                return std::optional<
                                    RoutineReplacement>{};
                            }
                            auto result = replace_native_gameplay_routine(
                                entry,
                                work_ram,
                                *replacement_rom);
                            if (result
                                && options.replace_hooks[
                                       replacement_index]
                                       .all_calls
                                && result->checkpoints.empty()
                                && result->body_master_cycles
                                    >= std::min(
                                        entry.master_cycles_until_user_event,
                                        entry
                                            .master_cycles_until_timer_interrupt)) {
                                result.reset();
                            }
                            if (result) {
                                ++replacement_hits[replacement_index];
                            }
                            return result;
                        };
                }
                if (runtime->install_execution_hook(std::move(hook))) {
                    continue;
                }
                std::cerr << "Could not install observe hook "
                          << index + 1U << ".\n";
                return EXIT_FAILURE;
            }
            for (std::size_t index = 0U;
                 index < options.replace_hooks.size();
                 ++index) {
                if (std::find(
                        options.observe_hooks.begin(),
                        options.observe_hooks.end(),
                        options.replace_hooks[index].routine)
                    != options.observe_hooks.end()) {
                    continue;
                }
                if (runtime->install_execution_hook({
                        .routine = options.replace_hooks[index].routine,
                        .mode = HookMode::replace,
                        .replacement_callback =
                            [&replacement_hits,
                             &replacement_entries,
                             &replacement_rom,
                             &options,
                             index](
                                const RoutineEntry& entry,
                                const std::span<const std::byte> work_ram) {
                                const auto entry_number =
                                    ++replacement_entries[index];
                                if (!options.replace_hooks[index].all_calls
                                    && entry_number
                                    != options.replace_hooks[index]
                                           .ordinal) {
                                    return std::optional<
                                        RoutineReplacement>{};
                                }
                                auto result =
                                    replace_native_gameplay_routine(
                                        entry,
                                        work_ram,
                                        *replacement_rom);
                                if (result
                                    && options.replace_hooks[index].all_calls
                                    && result->checkpoints.empty()
                                    && result->body_master_cycles
                                        >= std::min(
                                            entry
                                                .master_cycles_until_user_event,
                                            entry
                                                .master_cycles_until_timer_interrupt)) {
                                    result.reset();
                                }
                                if (result) {
                                    ++replacement_hits[index];
                                }
                                return result;
                            },
                    })) {
                    continue;
                }
                std::cerr << "Could not install replacement hook "
                          << index + 1U << ".\n";
                return EXIT_FAILURE;
            }
            if (!options.routine_call_trace_path.empty()) {
                for (std::size_t index = 0U;
                     index < options.observe_hooks.size();
                     ++index) {
                    if (runtime->install_routine_call_observer({
                        .routine = options.observe_hooks[index],
                        .callback = [&runtime,
                                     &observed_calls,
                                     &routine_replays,
                                     &active_replay,
                                     &suppress_recording_observers](
                                        const RoutineCall& call) {
                            if (active_replay != nullptr) {
                                if (call.entry.routine
                                        == active_replay->request.routine
                                    && !active_replay->replayed_call) {
                                    active_replay->replayed_call = call;
                                    active_replay
                                        ->replayed_post_call_work_ram =
                                        runtime->export_work_ram();
                                }
                                return;
                            }
                            if (!suppress_recording_observers) {
                                observed_calls.push_back(call);
                            }
                            const auto replay = std::find_if(
                                routine_replays.begin(),
                                routine_replays.end(),
                                [&call](
                                    const RoutineReplayCapture& capture) {
                                    return !capture.checkpoint.core_state.empty()
                                        && !capture.original_call
                                        && capture.checkpoint_entry
                                                .absolute_master_cycle
                                            == call.entry
                                                .absolute_master_cycle
                                        && capture.request.routine
                                            == call.entry.routine;
                                });
                            if (replay != routine_replays.end()) {
                                replay->original_call = call;
                                replay->original_post_call_work_ram =
                                    runtime->export_work_ram();
                            }
                        },
                    })) {
                        continue;
                    }
                    std::cerr
                        << "Could not install routine-call observer "
                        << index + 1U << ".\n";
                    return EXIT_FAILURE;
                }
            }
            if (!options.work_ram_write_ranges.empty()
                && !runtime->set_work_ram_write_observer(
                    [&observed_work_ram_writes,
                      &ranges = options.work_ram_write_ranges,
                      &suppress_recording_observers](
                        const ObservedMemoryWrite& write) {
                        if (suppress_recording_observers) {
                            return;
                        }
                        const bool selected = std::any_of(
                            ranges.begin(),
                            ranges.end(),
                            [&write](const LogicalAddressRange range) {
                                return write.logical_address >= range.first
                                    && write.logical_address <= range.last;
                            });
                        if (selected) {
                            observed_work_ram_writes.push_back(write);
                        }
                    })) {
                std::cerr
                    << "Could not install work-RAM write observer.\n";
                return EXIT_FAILURE;
            }

            const int result = run_deterministic_recording(
                *runtime,
                recording_path,
                options.canonical_work_ram_path,
                options.canonical_framebuffer_path,
                options.canonical_audio_path,
                runtime_info.upstream_revision,
                recording_plan,
                options.work_ram_patches);
            if (result != EXIT_SUCCESS) {
                return result;
            }
            for (std::size_t index = 0U;
                 index < pass_through_hits.size();
                 ++index) {
                if (pass_through_hits[index] == 0U) {
                    std::cerr << "Pass-through hook " << index + 1U
                              << " was never entered.\n";
                    return EXIT_FAILURE;
                }
                std::cout << "Pass-through hook " << index + 1U
                          << " entries: " << pass_through_hits[index]
                          << '\n';
            }
            for (std::size_t index = 0U;
                 index < replacement_hits.size();
                 ++index) {
                if (replacement_hits[index] == 0U) {
                    std::cerr << "Native replacement "
                              << index + 1U
                              << " was never entered.\n";
                    return EXIT_FAILURE;
                }
                const auto request = options.replace_hooks[index];
                std::cout << "Native replacement "
                          << std::uppercase << std::hex
                          << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(
                                 request.routine.physical_bank)
                          << ':' << std::setw(4)
                          << request.routine.logical_pc << std::dec << '@';
                if (request.all_calls) {
                    std::cout << "all";
                } else {
                    std::cout << request.ordinal;
                }
                std::cout << std::setfill(' ') << " entries: "
                          << replacement_hits[index];
                if (request.all_calls) {
                    std::cout << " of " << replacement_entries[index];
                }
                std::cout << '\n';
            }
            for (std::size_t index = 0U;
                 index < options.observe_hooks.size();
                 ++index) {
                const auto routine = options.observe_hooks[index];
                const auto hit_count = static_cast<std::size_t>(std::count_if(
                    observed_entries.begin(),
                    observed_entries.end(),
                    [routine](const RoutineEntry& entry) {
                        return entry.routine == routine;
                    }));
                if (hit_count == 0U) {
                    std::cerr << "Observe hook " << index + 1U
                              << " was never entered.\n";
                    return EXIT_FAILURE;
                }
                std::cout << "Observe hook " << index + 1U
                          << " entries: " << hit_count << '\n';
            }
            if (!options.routine_trace_path.empty()
                && !write_routine_trace(
                    options.routine_trace_path,
                    options.observe_hooks,
                    observed_entries,
                    runtime_info.upstream_revision)) {
                return EXIT_FAILURE;
            }
            if (!options.routine_call_trace_path.empty()
                && !write_routine_call_trace(
                    options.routine_call_trace_path,
                    options.observe_hooks,
                    observed_calls,
                    runtime_info.upstream_revision)) {
                return EXIT_FAILURE;
            }
            if (!options.work_ram_write_trace_path.empty()
                && !write_work_ram_write_trace(
                    options.work_ram_write_trace_path,
                    options.work_ram_write_ranges,
                    observed_work_ram_writes,
                    runtime_info.upstream_revision)) {
                return EXIT_FAILURE;
            }
            if (!routine_replays.empty()) {
                const int replay_result = verify_routine_replays(
                    *runtime,
                    recording_plan,
                    routine_replays,
                    active_replay,
                    suppress_recording_observers);
                if (replay_result != EXIT_SUCCESS) {
                    return replay_result;
                }
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
        if (options.cabinet_smoke_test) {
            return run_windows_host(
                *runtime,
                options.rom_path,
                true,
                std::nullopt,
                kSmokeTestFrames);
        }

        (void)load_backup_ram(*runtime, kBackupRamPath);
        const int host_result = run_windows_host(
            *runtime,
            options.rom_path,
            options.cabinet,
            options.backglass_path_set
                ? std::optional<std::filesystem::path>{
                    options.backglass_path}
                : std::nullopt);
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
