// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/display_config.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/game_logic/actor.h"
#include "devils_crush/game_logic/collision.h"
#include "devils_crush/game_logic/controls.h"
#include "devils_crush/game_logic/physics.h"
#include "devils_crush/game_logic/rng.h"
#include "devils_crush/game_logic/score.h"
#include "devils_crush/render/backglass_image.hpp"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/pce_sprite_overlay.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"

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
using devils_crush::emulation::ReplacementMemoryWrite;
using devils_crush::emulation::RoutineCall;
using devils_crush::emulation::RoutineEntry;
using devils_crush::emulation::RoutineKey;
using devils_crush::emulation::RoutineReplacement;

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
                           && !proven_unsigned_multiply_round
                           && !proven_signed_fixed_multiply
                           && !proven_actor_camera_activation
                           && !proven_actor_slot_reset
                           && !proven_flipper_input_gate
                           && !proven_collision_scan_vertical_reject
                           && !proven_collision_geometry
                           && !proven_flipper_phase_step
                           && !proven_flipper_geometry
                           && !proven_coroutine_scheduler
                           && !proven_nudge_input_gate
                           && !proven_session_input_gate
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
            "semantic 00:E905@all, semantic 00:E624@all, "
            "semantic 00:E65C@all, "
            "semantic 01:D09E@all, semantic 01:D612@all, "
            "semantic 01:D62E@all, "
            "semantic 01:CFD8@all, "
            "semantic 00:F4FC@all, semantic "
            "01:C574@all, semantic 01:D466@all, semantic 01:CC1A@all, "
            "semantic 01:CCD5@all, "
            "semantic "
            "01:D500@all, semantic 01:D081@all, semantic 01:C5C9@all, "
            "semantic 01:C3CB@all, semantic 01:C49E@all, and "
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

[[nodiscard]] std::optional<RoutineReplacement>
replace_position_integration(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF1B4U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    const auto* const bytes =
        reinterpret_cast<const std::uint8_t*>(work_ram.data());
    dc_position_integration integration{};
    if (!dc_integrate_position(
            bytes,
            work_ram.size(),
            entry.registers.x,
            &integration)) {
        throw std::runtime_error(
            "Native F1B4 replacement rejected the work-RAM snapshot.");
    }
    // The traced game route uses slot zero. Decimal or T-mode arithmetic
    // would have different HuC6280 semantics, so those inputs stay emulated.
    if (entry.registers.x != 0U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    constexpr std::array<std::size_t, 4> kComparedCacheIndexes{
        4U,
        5U,
        1U,
        2U,
    };
    std::optional<std::size_t> changed_integer_index;
    for (std::size_t index = 0U;
         index < kComparedCacheIndexes.size();
         ++index) {
        const auto byte_index = kComparedCacheIndexes[index];
        if (integration.cached_values[byte_index]
            != integration.position_values[byte_index]) {
            changed_integer_index = index;
            break;
        }
    }

    const auto sign_branch_count = static_cast<std::uint32_t>(
        integration.first_increment_negative
        + integration.second_increment_negative);
    const auto sign_branch_cycles = 12U * sign_branch_count;
    const std::uint8_t first_high_addend =
        integration.first_increment_negative != 0U ? 0xFFU : 0U;
    const std::uint8_t original_first_high =
        integration.cached_values[5];
    const std::uint8_t integrated_first_high =
        integration.position_values[5];
    const bool overflow =
        ((~(original_first_high ^ first_high_addend)
             & (original_first_high ^ integrated_first_high)
             & 0x80U)
            != 0U);

    auto output = entry.registers;
    output.program_counter = 0xF240U;
    output.status = static_cast<std::uint8_t>(
        entry.registers.status & 0x14U);
    if (overflow) {
        output.status |= 0x40U;
    }

    std::uint32_t body_master_cycles = 702U + sign_branch_cycles;
    if (changed_integer_index) {
        const auto byte_index =
            kComparedCacheIndexes[*changed_integer_index];
        const std::uint8_t left = integration.cached_values[byte_index];
        const std::uint8_t right = integration.position_values[byte_index];
        const std::uint8_t difference =
            static_cast<std::uint8_t>(left - right);
        output.accumulator = left;
        if (left >= right) {
            output.status |= 0x01U;
        }
        if (difference == 0U) {
            output.status |= 0x02U;
        }
        if ((difference & 0x80U) != 0U) {
            output.status |= 0x80U;
        }
        body_master_cycles =
            573U
            + 33U * static_cast<std::uint32_t>(*changed_integer_index)
            + sign_branch_cycles;
    } else {
        output.accumulator = integration.final_motion_flags;
        output.status |= 0x01U;
        if (integration.final_motion_flags == 0U) {
            output.status |= 0x02U;
        }
        if ((integration.final_motion_flags & 0x80U) != 0U) {
            output.status |= 0x80U;
        }
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF240U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };
    replacement.work_ram_writes.reserve(
        15U + sign_branch_count
        + static_cast<std::size_t>(!changed_integer_index.has_value()));
    constexpr std::array<std::uint16_t, 6> kCacheStorePcs{
        0xF1B7U,
        0xF1BCU,
        0xF1C1U,
        0xF1C6U,
        0xF1CBU,
        0xF1D0U,
    };
    for (std::size_t index = 0U;
         index < kCacheStorePcs.size();
         ++index) {
        replacement.work_ram_writes.push_back({
            .instruction = {
                .physical_bank = 0x00U,
                .logical_pc = kCacheStorePcs[index],
            },
            .logical_address = static_cast<std::uint16_t>(
                0x2000U
                + integration.cache_work_ram_offsets[index]),
            .value = integration.cached_values[index],
            .master_cycle_offset =
                static_cast<std::uint32_t>(27U * (index + 1U)),
        });
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x00U, 0xF1D7U},
        .logical_address = 0x3C80U,
        .value = integration.cleared_motion_flags,
        .master_cycle_offset = 198U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x00U, 0xF1DAU},
        .logical_address = 0x202BU,
        .value = 0x00U,
        .master_cycle_offset = 210U,
    });
    if (integration.second_increment_negative != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF1E0U},
            .logical_address = 0x202BU,
            .value = 0xFFU,
            .master_cycle_offset = 246U,
        });
    }
    constexpr std::array<std::uint16_t, 6> kPositionStorePcs{
        0xF1E8U,
        0xF1F0U,
        0xF1F8U,
        0xF209U,
        0xF211U,
        0xF219U,
    };
    constexpr std::array<std::uint32_t, 6> kPositiveStoreCycles{
        282U,
        324U,
        366U,
        450U,
        492U,
        534U,
    };
    const auto second_sign_cycles =
        integration.second_increment_negative != 0U ? 12U : 0U;
    for (std::size_t index = 0U;
         index < kPositionStorePcs.size();
         ++index) {
        if (index == 3U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, 0xF1FBU},
                .logical_address = 0x202BU,
                .value = 0x00U,
                .master_cycle_offset = 378U + second_sign_cycles,
            });
            if (integration.first_increment_negative != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x00U, 0xF201U},
                    .logical_address = 0x202BU,
                    .value = 0xFFU,
                    .master_cycle_offset = 414U + second_sign_cycles,
                });
            }
        }
        replacement.work_ram_writes.push_back({
            .instruction = {
                .physical_bank = 0x00U,
                .logical_pc = kPositionStorePcs[index],
            },
            .logical_address = static_cast<std::uint16_t>(
                0x2000U
                + integration.position_work_ram_offsets[index]),
            .value = integration.position_values[index],
            .master_cycle_offset =
                kPositiveStoreCycles[index]
                + (index < 3U
                        ? second_sign_cycles
                        : sign_branch_cycles),
        });
    }
    if (!changed_integer_index) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF23DU},
            .logical_address = 0x3C80U,
            .value = integration.final_motion_flags,
            .master_cycle_offset = body_master_cycles,
        });
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto difference =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, difference);
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };

    constexpr std::array<std::uint16_t, 6> kCacheLoadPcs{
        0xF1B4U,
        0xF1B9U,
        0xF1BEU,
        0xF1C3U,
        0xF1C8U,
        0xF1CDU,
    };
    constexpr std::array<std::uint16_t, 6> kCacheStoreResumePcs{
        0xF1B9U,
        0xF1BEU,
        0xF1C3U,
        0xF1C8U,
        0xF1CDU,
        0xF1D2U,
    };
    auto checkpoint = entry.registers;
    for (std::size_t index = 0U;
         index < kCacheLoadPcs.size();
         ++index) {
        checkpoint.accumulator = integration.cached_values[index];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            kCacheLoadPcs[index],
            kCacheStorePcs[index],
            15U + 27U * static_cast<std::uint32_t>(index),
            checkpoint);
        append_checkpoint(
            kCacheStorePcs[index],
            kCacheStoreResumePcs[index],
            27U + 27U * static_cast<std::uint32_t>(index),
            checkpoint);
    }

    const auto original_motion_flags =
        std::to_integer<std::uint8_t>(work_ram[0x1C80U]);
    checkpoint.accumulator = original_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF1D2U, 0xF1D5U, 177U, checkpoint);
    checkpoint.accumulator = integration.cleared_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF1D5U, 0xF1D7U, 183U, checkpoint);
    append_checkpoint(0xF1D7U, 0xF1DAU, 198U, checkpoint);
    append_checkpoint(0xF1DAU, 0xF1DCU, 210U, checkpoint);

    const auto second_increment_fraction =
        std::to_integer<std::uint8_t>(work_ram[0x00BBU]);
    const auto second_increment_integer =
        std::to_integer<std::uint8_t>(work_ram[0x00BCU]);
    const auto first_increment_fraction =
        std::to_integer<std::uint8_t>(work_ram[0x00B9U]);
    const auto first_increment_integer =
        std::to_integer<std::uint8_t>(work_ram[0x00BAU]);

    checkpoint.accumulator = second_increment_integer;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF1DCU, 0xF1DEU, 222U, checkpoint);
    if (integration.second_increment_negative != 0U) {
        append_checkpoint(0xF1DEU, 0xF1E0U, 228U, checkpoint);
        checkpoint.status = set_nz(checkpoint.status, 0xFFU);
        append_checkpoint(0xF1E0U, 0xF1E2U, 246U, checkpoint);
    } else {
        append_checkpoint(0xF1DEU, 0xF1E2U, 234U, checkpoint);
    }

    const auto second_cycles =
        integration.second_increment_negative != 0U ? 12U : 0U;
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(
        0xF1E2U,
        0xF1E3U,
        240U + second_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[0];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1E3U,
        0xF1E6U,
        255U + second_cycles,
        checkpoint);
    const auto second_fraction_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[0])
        + second_increment_fraction);
    bool arithmetic_carry = second_fraction_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[0];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[0],
        second_increment_fraction,
        false,
        checkpoint.accumulator);
    append_checkpoint(
        0xF1E6U,
        0xF1E8U,
        267U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1E8U,
        0xF1EBU,
        282U + second_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[1];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1EBU,
        0xF1EEU,
        297U + second_cycles,
        checkpoint);
    const auto second_integer_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[1])
        + static_cast<std::uint16_t>(second_increment_integer)
        + static_cast<std::uint16_t>(arithmetic_carry));
    const bool second_integer_carry = second_integer_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[1];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[1],
        second_increment_integer,
        arithmetic_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF1EEU,
        0xF1F0U,
        309U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1F0U,
        0xF1F3U,
        324U + second_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[2];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1F3U,
        0xF1F6U,
        339U + second_cycles,
        checkpoint);
    const auto second_sign_extension = static_cast<std::uint8_t>(
        integration.second_increment_negative != 0U ? 0xFFU : 0x00U);
    checkpoint.accumulator = integration.position_values[2];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[2],
        second_sign_extension,
        second_integer_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF1F6U,
        0xF1F8U,
        351U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1F8U,
        0xF1FBU,
        366U + second_cycles,
        checkpoint);
    append_checkpoint(
        0xF1FBU,
        0xF1FDU,
        378U + second_cycles,
        checkpoint);

    checkpoint.accumulator = first_increment_integer;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF1FDU,
        0xF1FFU,
        390U + second_cycles,
        checkpoint);
    if (integration.first_increment_negative != 0U) {
        append_checkpoint(
            0xF1FFU,
            0xF201U,
            396U + second_cycles,
            checkpoint);
        checkpoint.status = set_nz(checkpoint.status, 0xFFU);
        append_checkpoint(
            0xF201U,
            0xF203U,
            414U + second_cycles,
            checkpoint);
    } else {
        append_checkpoint(
            0xF1FFU,
            0xF203U,
            402U + second_cycles,
            checkpoint);
    }

    const auto both_sign_cycles = sign_branch_cycles;
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(
        0xF203U,
        0xF204U,
        408U + both_sign_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[3];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF204U,
        0xF207U,
        423U + both_sign_cycles,
        checkpoint);
    const auto first_fraction_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[3])
        + first_increment_fraction);
    arithmetic_carry = first_fraction_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[3];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[3],
        first_increment_fraction,
        false,
        checkpoint.accumulator);
    append_checkpoint(
        0xF207U,
        0xF209U,
        435U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF209U,
        0xF20CU,
        450U + both_sign_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[4];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF20CU,
        0xF20FU,
        465U + both_sign_cycles,
        checkpoint);
    const auto first_integer_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(integration.cached_values[4])
        + static_cast<std::uint16_t>(first_increment_integer)
        + static_cast<std::uint16_t>(arithmetic_carry));
    const bool first_integer_carry = first_integer_sum > 0xFFU;
    checkpoint.accumulator = integration.position_values[4];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[4],
        first_increment_integer,
        arithmetic_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF20FU,
        0xF211U,
        477U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF211U,
        0xF214U,
        492U + both_sign_cycles,
        checkpoint);

    checkpoint.accumulator = integration.cached_values[5];
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF214U,
        0xF217U,
        507U + both_sign_cycles,
        checkpoint);
    const auto first_sign_extension = static_cast<std::uint8_t>(
        integration.first_increment_negative != 0U ? 0xFFU : 0x00U);
    checkpoint.accumulator = integration.position_values[5];
    checkpoint.status = adc_status(
        checkpoint.status,
        integration.cached_values[5],
        first_sign_extension,
        first_integer_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xF217U,
        0xF219U,
        519U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF219U,
        0xF21CU,
        534U + both_sign_cycles,
        checkpoint);

    constexpr std::array<std::uint16_t, 4> kCompareLoadPcs{
        0xF21CU,
        0xF223U,
        0xF22AU,
        0xF231U,
    };
    constexpr std::array<std::uint16_t, 4> kComparePcs{
        0xF21EU,
        0xF225U,
        0xF22CU,
        0xF233U,
    };
    constexpr std::array<std::uint16_t, 4> kCompareBranchPcs{
        0xF221U,
        0xF228U,
        0xF22FU,
        0xF236U,
    };
    constexpr std::array<std::uint16_t, 4> kCompareNextPcs{
        0xF223U,
        0xF22AU,
        0xF231U,
        0xF238U,
    };
    for (std::size_t index = 0U;
         index < kComparedCacheIndexes.size();
         ++index) {
        const auto byte_index = kComparedCacheIndexes[index];
        const auto comparison_cycles =
            33U * static_cast<std::uint32_t>(index)
            + both_sign_cycles;
        checkpoint.accumulator =
            integration.cached_values[byte_index];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            kCompareLoadPcs[index],
            kComparePcs[index],
            546U + comparison_cycles,
            checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.accumulator,
            integration.position_values[byte_index]);
        append_checkpoint(
            kComparePcs[index],
            kCompareBranchPcs[index],
            561U + comparison_cycles,
            checkpoint);
        if (changed_integer_index
            && *changed_integer_index == index) {
            append_checkpoint(
                kCompareBranchPcs[index],
                0xF240U,
                573U + comparison_cycles,
                output);
            return replacement;
        }
        append_checkpoint(
            kCompareBranchPcs[index],
            kCompareNextPcs[index],
            567U + comparison_cycles,
            checkpoint);
    }

    checkpoint.accumulator = integration.cleared_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF238U,
        0xF23BU,
        681U + both_sign_cycles,
        checkpoint);
    checkpoint.accumulator = integration.final_motion_flags;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xF23BU,
        0xF23DU,
        687U + both_sign_cycles,
        checkpoint);
    append_checkpoint(
        0xF23DU,
        0xF240U,
        702U + both_sign_cycles,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_fixed_multiply(
    const RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_word_negation(
    const RoutineEntry& entry,
    std::span<const std::byte> work_ram);

[[nodiscard]] std::optional<RoutineReplacement>
replace_motion_rotation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD1A2U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_motion_rotation rotation{};
    if (!dc_rotate_motion(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &rotation)) {
        throw std::runtime_error(
            "Native D1A2 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD227U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_indexed =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    logical_base + checkpoint.x),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto begin_jsr =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(pushed_return_pc >> 8U),
                cycle);
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                target_pc,
                cycle,
                checkpoint);
        };
    const auto append_child =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_checkpoint](
            const RoutineReplacement& child,
            const std::uint16_t resume_pc) {
            const auto child_offset = cycle;
            for (auto write : child.work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child.checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child.body_master_cycles;
            checkpoint = child.output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child.return_instruction.physical_bank,
                child.return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto run_signed_multiply =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE65CU,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE65CU,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE65CU;
            auto child = replace_signed_fixed_multiply(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };
    const auto run_negation =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE837U,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE837U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE837U;
            auto child = replace_signed_word_negation(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };

    load_zero_page(0xD1A2U, 0xD1A4U, 0x00C0U);
    store_zero_page(0xD1A4U, 0xD1A6U, 0x002FU);
    load_zero_page(0xD1A6U, 0xD1A8U, 0x00C1U);
    store_zero_page(0xD1A8U, 0xD1AAU, 0x0030U);
    load_zero_page(0xD1AAU, 0xD1ACU, 0x007DU);
    store_zero_page(0xD1ACU, 0xD1AEU, 0x002DU);
    load_zero_page(0xD1AEU, 0xD1B0U, 0x007EU);
    store_zero_page(0xD1B0U, 0xD1B2U, 0x002EU);
    if (!run_signed_multiply(
            0xD1B2U,
            0xD1B4U,
            0xD1B5U)) {
        return std::nullopt;
    }
    load_zero_page(0xD1B5U, 0xD1B7U, 0x002FU);
    store_indexed(0xD1B7U, 0xD1BAU, 0x3400U);
    load_zero_page(0xD1BAU, 0xD1BCU, 0x0030U);
    store_indexed(0xD1BCU, 0xD1BFU, 0x3480U);

    load_zero_page(0xD1BFU, 0xD1C1U, 0x00BEU);
    store_zero_page(0xD1C1U, 0xD1C3U, 0x002FU);
    load_zero_page(0xD1C3U, 0xD1C5U, 0x00BFU);
    store_zero_page(0xD1C5U, 0xD1C7U, 0x0030U);
    load_zero_page(0xD1C7U, 0xD1C9U, 0x007FU);
    store_zero_page(0xD1C9U, 0xD1CBU, 0x002DU);
    load_zero_page(0xD1CBU, 0xD1CDU, 0x0080U);
    store_zero_page(0xD1CDU, 0xD1CFU, 0x002EU);
    if (!run_signed_multiply(
            0xD1CFU,
            0xD1D1U,
            0xD1D2U)
        || !run_negation(
            0xD1D2U,
            0xD1D4U,
            0xD1D5U)) {
        return std::nullopt;
    }
    clear_carry(0xD1D5U, 0xD1D6U);
    load_indexed(0xD1D6U, 0xD1D9U, 0x3400U);
    add_zero_page(0xD1D9U, 0xD1DBU, 0x002FU);
    store_indexed(0xD1DBU, 0xD1DEU, 0x3400U);
    load_indexed(0xD1DEU, 0xD1E1U, 0x3480U);
    add_zero_page(0xD1E1U, 0xD1E3U, 0x0030U);
    store_indexed(0xD1E3U, 0xD1E6U, 0x3480U);

    load_zero_page(0xD1E6U, 0xD1E8U, 0x00C0U);
    store_zero_page(0xD1E8U, 0xD1EAU, 0x002FU);
    load_zero_page(0xD1EAU, 0xD1ECU, 0x00C1U);
    store_zero_page(0xD1ECU, 0xD1EEU, 0x0030U);
    load_zero_page(0xD1EEU, 0xD1F0U, 0x007FU);
    store_zero_page(0xD1F0U, 0xD1F2U, 0x002DU);
    load_zero_page(0xD1F2U, 0xD1F4U, 0x0080U);
    store_zero_page(0xD1F4U, 0xD1F6U, 0x002EU);
    if (!run_signed_multiply(
            0xD1F6U,
            0xD1F8U,
            0xD1F9U)) {
        return std::nullopt;
    }
    load_zero_page(0xD1F9U, 0xD1FBU, 0x002FU);
    store_indexed(0xD1FBU, 0xD1FEU, 0x3500U);
    load_zero_page(0xD1FEU, 0xD200U, 0x0030U);
    store_indexed(0xD200U, 0xD203U, 0x3580U);

    load_zero_page(0xD203U, 0xD205U, 0x00BEU);
    store_zero_page(0xD205U, 0xD207U, 0x002FU);
    load_zero_page(0xD207U, 0xD209U, 0x00BFU);
    store_zero_page(0xD209U, 0xD20BU, 0x0030U);
    load_zero_page(0xD20BU, 0xD20DU, 0x007DU);
    store_zero_page(0xD20DU, 0xD20FU, 0x002DU);
    load_zero_page(0xD20FU, 0xD211U, 0x007EU);
    store_zero_page(0xD211U, 0xD213U, 0x002EU);
    if (!run_signed_multiply(
            0xD213U,
            0xD215U,
            0xD216U)) {
        return std::nullopt;
    }
    clear_carry(0xD216U, 0xD217U);
    load_indexed(0xD217U, 0xD21AU, 0x3500U);
    add_zero_page(0xD21AU, 0xD21CU, 0x002FU);
    store_indexed(0xD21CU, 0xD21FU, 0x3500U);
    load_indexed(0xD21FU, 0xD222U, 0x3580U);
    add_zero_page(0xD222U, 0xD224U, 0x0030U);
    store_indexed(0xD224U, 0xD227U, 0x3580U);

    const auto first_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1400U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1480U + checkpoint.x]))
           << 8U));
    const auto second_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1500U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1580U + checkpoint.x]))
           << 8U));
    if (first_output != rotation.first_output
        || second_output != rotation.second_output
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D1A2 replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter = 0xD227U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_radius_projection(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD612U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_flipper_radius_projection projection{};
    if (!dc_project_flipper_radius(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &projection)) {
        throw std::runtime_error(
            "Native D612 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD62DU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            staged[logical_address - 0x2000U] =
                static_cast<std::byte>(value);
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xD612U, 0xD614U, 0x0031U);
    store_zero_page(0xD614U, 0xD616U, 0x002FU);
    load_zero_page(0xD616U, 0xD618U, 0x0032U);
    store_zero_page(0xD618U, 0xD61AU, 0x0030U);
    load_zero_page(0xD61AU, 0xD61CU, 0x007DU);
    store_zero_page(0xD61CU, 0xD61EU, 0x002DU);
    load_zero_page(0xD61EU, 0xD620U, 0x007EU);
    store_zero_page(0xD620U, 0xD622U, 0x002EU);

    append_write(
        0x01U,
        0xD622U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xD6U,
        cycle);
    append_write(
        0x01U,
        0xD622U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x24U,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(
        0x01U,
        0xD622U,
        0xE65CU,
        cycle,
        checkpoint);

    RoutineEntry child_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        },
        .registers = checkpoint,
    };
    child_entry.registers.program_counter = 0xE65CU;
    auto child = replace_signed_fixed_multiply(
        child_entry,
        staged);
    if (!child || child->checkpoints.empty()) {
        return std::nullopt;
    }
    const auto child_offset = cycle;
    for (auto write : child->work_ram_writes) {
        write.master_cycle_offset += child_offset;
        replacement.work_ram_writes.push_back(write);
        staged[write.logical_address - 0x2000U] =
            static_cast<std::byte>(write.value);
    }
    for (auto child_checkpoint : child->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            child_offset;
        child_checkpoint.master_cycle_offset += child_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += child->body_master_cycles;
    checkpoint = child->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(
        child->return_instruction.physical_bank,
        child->return_instruction.logical_pc,
        0xD625U,
        cycle,
        checkpoint);

    load_zero_page(0xD625U, 0xD627U, 0x002FU);
    store_zero_page(0xD627U, 0xD629U, 0x00C2U);
    load_zero_page(0xD629U, 0xD62BU, 0x0030U);
    store_zero_page(0xD62BU, 0xD62DU, 0x00C3U);

    const auto staged_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00C2U])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00C3U]))
           << 8U));
    if (staged_output != projection.output
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D612 replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter = 0xD62DU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_angle_projection(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD62EU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || entry.registers.y >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_flipper_angle_projection projection{};
    if (!dc_project_flipper_angle(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            entry.registers.y,
            &projection)) {
        throw std::runtime_error(
            "Native D62E replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD68DU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            staged[logical_address - 0x2000U] =
                static_cast<std::byte>(value);
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base,
            const std::uint8_t index) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + index]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                0U,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto set_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                (checkpoint.status
                 & static_cast<std::uint8_t>(~0x20U))
                | 0x01U);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base,
            const std::uint8_t index) {
            const auto left = checkpoint.accumulator;
            const auto right = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + index]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_indexed(
        0xD62EU,
        0xD631U,
        0x3C80U,
        checkpoint.y);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 6U;
    append_checkpoint(
        0x01U,
        0xD631U,
        0xD633U,
        cycle,
        checkpoint);

    if (projection.uses_wrapped_difference != 0U) {
        cycle += 12U;
        append_checkpoint(
            0x01U,
            0xD633U,
            0xD641U,
            cycle,
            checkpoint);
        clear_carry(0xD641U, 0xD642U);
        load_indexed(
            0xD642U,
            0xD645U,
            0x3180U,
            checkpoint.y);
        const auto left = checkpoint.accumulator;
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator + 0x10U);
        checkpoint.status = adc_status(
            checkpoint.status,
            left,
            0x10U,
            false,
            checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD645U,
            0xD647U,
            cycle,
            checkpoint);
        set_carry(0xD647U, 0xD648U);
        subtract_indexed(
            0xD648U,
            0xD64BU,
            0x3180U,
            checkpoint.x);
        cycle += projection.returns_zero != 0U ? 12U : 6U;
        append_checkpoint(
            0x01U,
            0xD64BU,
            projection.returns_zero != 0U
                ? 0xD68EU
                : 0xD64DU,
            cycle,
            checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD633U,
            0xD635U,
            cycle,
            checkpoint);
        set_carry(0xD635U, 0xD636U);
        load_indexed(
            0xD636U,
            0xD639U,
            0x3180U,
            checkpoint.x);
        subtract_indexed(
            0xD639U,
            0xD63CU,
            0x3180U,
            checkpoint.y);
        cycle += projection.returns_zero != 0U ? 6U : 12U;
        append_checkpoint(
            0x01U,
            0xD63CU,
            projection.returns_zero != 0U
                ? 0xD63EU
                : 0xD64DU,
            cycle,
            checkpoint);
        if (projection.returns_zero != 0U) {
            cycle += 12U;
            append_checkpoint(
                0x01U,
                0xD63EU,
                0xD68EU,
                cycle,
                checkpoint);
        }
    }

    if (projection.returns_zero != 0U) {
        store_zero(0xD68EU, 0xD690U, 0x0033U);
        store_zero(0xD690U, 0xD692U, 0x0031U);
        store_zero(0xD692U, 0xD694U, 0x0032U);
        if (std::to_integer<std::uint8_t>(staged[0x0031U]) != 0U
            || std::to_integer<std::uint8_t>(staged[0x0032U]) != 0U
            || std::to_integer<std::uint8_t>(staged[0x0033U]) != 0U
            || replacement.work_ram_writes.size()
                > kMaximumReplacementWrites
            || replacement.checkpoints.size()
                > kMaximumReplacementCheckpoints) {
            throw std::runtime_error(
                "Native D62E zero-exit composition diverged from its "
                "state model.");
        }
        replacement.return_instruction.logical_pc = 0xD694U;
        checkpoint.program_counter = 0xD694U;
        replacement.output_registers = checkpoint;
        replacement.body_master_cycles = cycle;
        return replacement;
    }

    store_zero_page(0xD64DU, 0xD64FU, 0x0033U);
    store_zero(0xD64FU, 0xD651U, 0x002FU);
    constexpr std::array<std::uint16_t, 5> kShiftPcs{
        0xD651U,
        0xD654U,
        0xD657U,
        0xD65AU,
        0xD65DU,
    };
    constexpr std::array<std::uint16_t, 5> kRotatePcs{
        0xD652U,
        0xD655U,
        0xD658U,
        0xD65BU,
        0xD65EU,
    };
    for (std::size_t index = 0U;
         index < kShiftPcs.size();
         ++index) {
        const bool carry =
            (checkpoint.accumulator & 0x01U) != 0U;
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator >> 1U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x01U));
        if (carry) {
            checkpoint.status =
                static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        }
        cycle += 6U;
        append_checkpoint(
            0x01U,
            kShiftPcs[index],
            kRotatePcs[index],
            cycle,
            checkpoint);

        const auto old_value =
            std::to_integer<std::uint8_t>(staged[0x002FU]);
        const bool old_carry =
            (checkpoint.status & 0x01U) != 0U;
        const bool next_carry = (old_value & 0x01U) != 0U;
        const auto rotated = static_cast<std::uint8_t>(
            (old_value >> 1U) | (old_carry ? 0x80U : 0U));
        checkpoint.status = set_nz(checkpoint.status, rotated);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x01U));
        if (next_carry) {
            checkpoint.status =
                static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        }
        cycle += 18U;
        append_write(
            0x01U,
            kRotatePcs[index],
            0x202FU,
            rotated,
            cycle);
        append_checkpoint(
            0x01U,
            kRotatePcs[index],
            static_cast<std::uint16_t>(
                index + 1U < kShiftPcs.size()
                    ? kShiftPcs[index + 1U]
                    : 0xD660U),
            cycle,
            checkpoint);
    }

    store_zero_page(0xD660U, 0xD662U, 0x0030U);
    load_zero_page(0xD662U, 0xD664U, 0x009EU);
    store_zero_page(0xD664U, 0xD666U, 0x002DU);
    load_zero_page(0xD666U, 0xD668U, 0x009FU);
    store_zero_page(0xD668U, 0xD66AU, 0x002EU);

    append_write(
        0x01U,
        0xD66AU,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xD6U,
        cycle);
    append_write(
        0x01U,
        0xD66AU,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x6CU,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(
        0x01U,
        0xD66AU,
        0xE65CU,
        cycle,
        checkpoint);

    RoutineEntry child_entry{
        .routine = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        },
        .registers = checkpoint,
    };
    child_entry.registers.program_counter = 0xE65CU;
    auto child = replace_signed_fixed_multiply(
        child_entry,
        staged);
    if (!child || child->checkpoints.empty()) {
        return std::nullopt;
    }
    const auto child_offset = cycle;
    for (auto write : child->work_ram_writes) {
        write.master_cycle_offset += child_offset;
        replacement.work_ram_writes.push_back(write);
        staged[write.logical_address - 0x2000U] =
            static_cast<std::byte>(write.value);
    }
    for (auto child_checkpoint : child->checkpoints) {
        child_checkpoint.interrupt_sample_master_cycle_offset +=
            child_offset;
        child_checkpoint.master_cycle_offset += child_offset;
        replacement.checkpoints.push_back(child_checkpoint);
    }
    cycle += child->body_master_cycles;
    checkpoint = child->output_registers;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(
        child->return_instruction.physical_bank,
        child->return_instruction.logical_pc,
        0xD66DU,
        cycle,
        checkpoint);

    load_indexed(
        0xD66DU,
        0xD670U,
        0x3C80U,
        checkpoint.y);
    cycle += projection.negates_product != 0U ? 12U : 6U;
    append_checkpoint(
        0x01U,
        0xD670U,
        projection.negates_product != 0U
            ? 0xD67BU
            : 0xD672U,
        cycle,
        checkpoint);

    if (projection.negates_product == 0U) {
        load_zero_page(0xD672U, 0xD674U, 0x002FU);
        store_zero_page(0xD674U, 0xD676U, 0x0031U);
        load_zero_page(0xD676U, 0xD678U, 0x0030U);
        store_zero_page(0xD678U, 0xD67AU, 0x0032U);
        replacement.return_instruction.logical_pc = 0xD67AU;
    } else {
        load_zero_page(0xD67BU, 0xD67DU, 0x002FU);
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator ^ 0xFFU);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD67DU,
            0xD67FU,
            cycle,
            checkpoint);
        store_zero_page(0xD67FU, 0xD681U, 0x0031U);
        load_zero_page(0xD681U, 0xD683U, 0x0030U);
        checkpoint.accumulator =
            static_cast<std::uint8_t>(checkpoint.accumulator ^ 0xFFU);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(
            0x01U,
            0xD683U,
            0xD685U,
            cycle,
            checkpoint);
        store_zero_page(0xD685U, 0xD687U, 0x0032U);

        auto low = static_cast<std::uint8_t>(
            std::to_integer<std::uint8_t>(staged[0x0031U]) + 1U);
        checkpoint.status = set_nz(checkpoint.status, low);
        cycle += 18U;
        append_write(0x01U, 0xD687U, 0x2031U, low, cycle);
        append_checkpoint(
            0x01U,
            0xD687U,
            0xD689U,
            cycle,
            checkpoint);
        cycle += projection.increments_high != 0U ? 6U : 12U;
        append_checkpoint(
            0x01U,
            0xD689U,
            projection.increments_high != 0U
                ? 0xD68BU
                : 0xD68DU,
            cycle,
            checkpoint);
        if (projection.increments_high != 0U) {
            auto high = static_cast<std::uint8_t>(
                std::to_integer<std::uint8_t>(staged[0x0032U]) + 1U);
            checkpoint.status = set_nz(checkpoint.status, high);
            cycle += 18U;
            append_write(0x01U, 0xD68BU, 0x2032U, high, cycle);
            append_checkpoint(
                0x01U,
                0xD68BU,
                0xD68DU,
                cycle,
                checkpoint);
        }
    }

    const auto staged_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x0031U])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x0032U]))
           << 8U));
    if (staged_output != projection.output
        || std::to_integer<std::uint8_t>(staged[0x0033U])
            != projection.angle_difference
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D62E replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_motion_adjustment(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCFD8U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_collision_motion_adjustment adjustment{};
    if (!dc_adjust_collision_motion(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &adjustment)) {
        throw std::runtime_error(
            "Native CFD8 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD004U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(16U);
    replacement.checkpoints.reserve(64U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint16_t instruction_pc,
            const std::uint16_t offset,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, instruction_pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + offset),
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            staged[offset] = static_cast<std::byte>(value);
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_absolute =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 15U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                instruction_pc,
                offset,
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto compare_immediate =
        [&checkpoint,
         &cycle,
         &compare_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint8_t value) {
            cycle += 6U;
            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                value);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto rotate_word =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_write,
         &append_checkpoint](
            const std::uint16_t compare_pc,
            const std::uint16_t high_pc,
            const std::uint16_t low_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            if (checkpoint.accumulator >= 0x80U) {
                checkpoint.status =
                    static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
            }
            checkpoint.status = set_nz(
                checkpoint.status,
                static_cast<std::uint8_t>(
                    checkpoint.accumulator - 0x80U));
            append_checkpoint(
                compare_pc,
                high_pc,
                cycle,
                checkpoint);

            const auto old_high =
                std::to_integer<std::uint8_t>(staged[0x00BFU]);
            const bool high_carry =
                (checkpoint.status & 0x01U) != 0U;
            const bool low_bit = (old_high & 0x01U) != 0U;
            const auto high = static_cast<std::uint8_t>(
                (old_high >> 1U) | (high_carry ? 0x80U : 0U));
            checkpoint.status = set_nz(checkpoint.status, high);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status & static_cast<std::uint8_t>(~0x01U));
            if (low_bit) {
                checkpoint.status =
                    static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
            }
            cycle += 18U;
            append_write(high_pc, 0x00BFU, high, cycle);
            append_checkpoint(
                high_pc,
                low_pc,
                cycle,
                checkpoint);

            const auto old_low =
                std::to_integer<std::uint8_t>(staged[0x00BEU]);
            const bool word_carry =
                (checkpoint.status & 0x01U) != 0U;
            const bool next_carry = (old_low & 0x01U) != 0U;
            const auto low = static_cast<std::uint8_t>(
                (old_low >> 1U) | (word_carry ? 0x80U : 0U));
            checkpoint.status = set_nz(checkpoint.status, low);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status & static_cast<std::uint8_t>(~0x01U));
            if (next_carry) {
                checkpoint.status =
                    static_cast<std::uint8_t>(
                        checkpoint.status | 0x01U);
            }
            cycle += 18U;
            append_write(low_pc, 0x00BEU, low, cycle);
            append_checkpoint(
                low_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xCFD8U, 0xCFDAU, 0x0078U);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x60U);
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 6U;
    append_checkpoint(0xCFDAU, 0xCFDCU, cycle, checkpoint);
    compare_immediate(0xCFDCU, 0xCFDEU, 0x20U);

    if (adjustment.effective_mode == 0x20U
        && adjustment.mode == 0x20U) {
        cycle += 12U;
        append_checkpoint(0xCFDEU, 0xD017U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xCFDEU, 0xCFE0U, cycle, checkpoint);
        compare_immediate(0xCFE0U, 0xCFE2U, 0x40U);
        if (adjustment.effective_mode == 0x40U) {
            cycle += 12U;
            append_checkpoint(0xCFE2U, 0xD05DU, cycle, checkpoint);
        } else {
            cycle += 6U;
            append_checkpoint(0xCFE2U, 0xCFE4U, cycle, checkpoint);
            compare_immediate(0xCFE4U, 0xCFE6U, 0x60U);
            if (adjustment.effective_mode == 0x60U) {
                cycle += 12U;
                append_checkpoint(0xCFE6U, 0xD066U, cycle, checkpoint);
            } else {
                cycle += 6U;
                append_checkpoint(0xCFE6U, 0xCFE8U, cycle, checkpoint);
                load_absolute(0xCFE8U, 0xCFEBU, 0x0CC7U);
                cycle += adjustment.effective_mode == 0x20U
                    ? 12U
                    : 6U;
                append_checkpoint(
                    0xCFEBU,
                    adjustment.effective_mode == 0x20U
                        ? 0xD017U
                        : 0xCFEDU,
                    cycle,
                    checkpoint);
            }
        }
    }

    if (adjustment.effective_mode == 0U) {
        load_zero_page(0xCFEDU, 0xCFEFU, 0x00BFU);
        cycle += adjustment.negative_input != 0U ? 12U : 6U;
        append_checkpoint(
            0xCFEFU,
            adjustment.negative_input != 0U
                ? 0xD005U
                : 0xCFF1U,
            cycle,
            checkpoint);
        if (adjustment.negative_input == 0U) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(0xCFF1U, 0xCFF2U, cycle, checkpoint);
            load_zero_page(0xCFF2U, 0xCFF4U, 0x00BEU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x0081U]);
                const bool carry =
                    (checkpoint.status & 0x01U) != 0U;
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    left + right + (carry ? 1U : 0U));
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    carry,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xCFF4U, 0xCFF6U, cycle, checkpoint);
            }
            store_zero_page(0xCFF6U, 0xCFF8U, 0x00BEU);
            load_zero_page(0xCFF8U, 0xCFFAU, 0x00BFU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x0082U]);
                const bool carry =
                    (checkpoint.status & 0x01U) != 0U;
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    left + right + (carry ? 1U : 0U));
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    carry,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xCFFAU, 0xCFFCU, cycle, checkpoint);
            }
            compare_immediate(0xCFFCU, 0xCFFEU, 0x07U);
            if (adjustment.clamps_high != 0U) {
                cycle += 6U;
                append_checkpoint(0xCFFEU, 0xD000U, cycle, checkpoint);
                checkpoint.accumulator = 0x06U;
                checkpoint.status =
                    set_nz(checkpoint.status, checkpoint.accumulator);
                cycle += 6U;
                append_checkpoint(0xD000U, 0xD002U, cycle, checkpoint);
            } else {
                cycle += 12U;
                append_checkpoint(0xCFFEU, 0xD002U, cycle, checkpoint);
            }
            store_zero_page(0xD002U, 0xD004U, 0x00BFU);
            replacement.return_instruction.logical_pc = 0xD004U;
        } else {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                (checkpoint.status
                 & static_cast<std::uint8_t>(~0x20U))
                | 0x01U);
            append_checkpoint(0xD005U, 0xD006U, cycle, checkpoint);
            load_zero_page(0xD006U, 0xD008U, 0x00BEU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x0081U]);
                checkpoint.accumulator =
                    static_cast<std::uint8_t>(left - right);
                checkpoint.status = sbc_status(
                    checkpoint.status,
                    left,
                    right,
                    true,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xD008U, 0xD00AU, cycle, checkpoint);
            }
            store_zero_page(0xD00AU, 0xD00CU, 0x00BEU);
            load_zero_page(0xD00CU, 0xD00EU, 0x00BFU);
            compare_immediate(0xD00EU, 0xD010U, 0xFAU);
            if (adjustment.clamps_high != 0U) {
                cycle += 6U;
                append_checkpoint(0xD010U, 0xD012U, cycle, checkpoint);
                checkpoint.accumulator = 0xFAU;
                checkpoint.status =
                    set_nz(checkpoint.status, checkpoint.accumulator);
                cycle += 6U;
                append_checkpoint(0xD012U, 0xD014U, cycle, checkpoint);
            } else {
                cycle += 12U;
                append_checkpoint(0xD010U, 0xD014U, cycle, checkpoint);
            }
            store_zero_page(0xD014U, 0xD016U, 0x00BFU);
            replacement.return_instruction.logical_pc = 0xD016U;
        }
    } else if (adjustment.effective_mode == 0x20U) {
        load_absolute(0xD017U, 0xD01AU, 0x022CU);
        cycle += adjustment.uses_quarter_scale != 0U ? 12U : 6U;
        append_checkpoint(
            0xD01AU,
            adjustment.uses_quarter_scale != 0U
                ? 0xD04EU
                : 0xD01CU,
            cycle,
            checkpoint);
        if (adjustment.uses_quarter_scale == 0U) {
            load_zero_page(0xD01CU, 0xD01EU, 0x00BFU);
            rotate_word(0xD01EU, 0xD020U, 0xD022U, 0xD024U);
            rotate_word(0xD024U, 0xD026U, 0xD028U, 0xD02AU);
            rotate_word(0xD02AU, 0xD02CU, 0xD02EU, 0xD030U);
            load_zero_page(0xD030U, 0xD032U, 0x00BEU);
            store_zero_page(0xD032U, 0xD034U, 0x002DU);
            load_zero_page(0xD034U, 0xD036U, 0x00BFU);
            store_zero_page(0xD036U, 0xD038U, 0x002EU);
            load_zero_page(0xD038U, 0xD03AU, 0x00BFU);
            rotate_word(0xD03AU, 0xD03CU, 0xD03EU, 0xD040U);
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(0xD040U, 0xD041U, cycle, checkpoint);
            load_zero_page(0xD041U, 0xD043U, 0x00BEU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x002DU]);
                checkpoint.accumulator =
                    static_cast<std::uint8_t>(left + right);
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    false,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xD043U, 0xD045U, cycle, checkpoint);
            }
            store_zero_page(0xD045U, 0xD047U, 0x00BEU);
            load_zero_page(0xD047U, 0xD049U, 0x00BFU);
            {
                const auto left = checkpoint.accumulator;
                const auto right =
                    std::to_integer<std::uint8_t>(staged[0x002EU]);
                const bool carry =
                    (checkpoint.status & 0x01U) != 0U;
                checkpoint.accumulator = static_cast<std::uint8_t>(
                    left + right + (carry ? 1U : 0U));
                checkpoint.status = adc_status(
                    checkpoint.status,
                    left,
                    right,
                    carry,
                    checkpoint.accumulator);
                cycle += 12U;
                append_checkpoint(0xD049U, 0xD04BU, cycle, checkpoint);
            }
            store_zero_page(0xD04BU, 0xD04DU, 0x00BFU);
            replacement.return_instruction.logical_pc = 0xD04DU;
        } else {
            load_zero_page(0xD04EU, 0xD050U, 0x00BFU);
            rotate_word(0xD050U, 0xD052U, 0xD054U, 0xD056U);
            rotate_word(0xD056U, 0xD058U, 0xD05AU, 0xD05CU);
            replacement.return_instruction.logical_pc = 0xD05CU;
        }
    } else if (adjustment.effective_mode == 0x40U) {
        load_zero_page(0xD05DU, 0xD05FU, 0x00BFU);
        rotate_word(0xD05FU, 0xD061U, 0xD063U, 0xD065U);
        replacement.return_instruction.logical_pc = 0xD065U;
    } else {
        load_zero_page(0xD066U, 0xD068U, 0x00BFU);
        rotate_word(0xD068U, 0xD06AU, 0xD06CU, 0xD06EU);
        rotate_word(0xD06EU, 0xD070U, 0xD072U, 0xD074U);
        rotate_word(0xD074U, 0xD076U, 0xD078U, 0xD07AU);
        rotate_word(0xD07AU, 0xD07CU, 0xD07EU, 0xD080U);
        replacement.return_instruction.logical_pc = 0xD080U;
    }

    const auto staged_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00BEU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00BFU]))
           << 8U));
    if (staged_output != adjustment.output) {
        throw std::runtime_error(
            "Native CFD8 replacement composition diverged from its "
            "state model.");
    }
    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_division(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE905U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_signed_division division{};
    if (!dc_divide_signed_word(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            static_cast<std::uint8_t>(
                entry.registers.status & 0x01U),
            &division)) {
        throw std::runtime_error(
            "Native E905 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE937U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(64U);
    replacement.checkpoints.reserve(192U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result =
                static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U)
                != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto rotate_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto original =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry_in =
                (checkpoint.status & 0x01U) != 0U;
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(original << 1U)
                | static_cast<std::uint8_t>(carry_in));
            checkpoint.status =
                set_nz(checkpoint.status, result);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x01U));
            if ((original & 0x80U) != 0U) {
                checkpoint.status = static_cast<std::uint8_t>(
                    checkpoint.status | 0x01U);
            }
            cycle += 18U;
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                result,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xE905U, 0xE907U, 0x002DU);
    store_zero_page(0xE907U, 0xE909U, 0x0046U);
    cycle += division.divisor_negative != 0U ? 6U : 12U;
    append_checkpoint(
        0xE909U,
        division.divisor_negative != 0U ? 0xE90BU : 0xE910U,
        cycle,
        checkpoint);
    if (division.divisor_negative != 0U) {
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator ^ 0xFFU);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(0xE90BU, 0xE90DU, cycle, checkpoint);
        checkpoint.accumulator = static_cast<std::uint8_t>(
            checkpoint.accumulator + 1U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        cycle += 6U;
        append_checkpoint(0xE90DU, 0xE90EU, cycle, checkpoint);
        store_zero_page(0xE90EU, 0xE910U, 0x002DU);
    }

    load_zero_page(0xE910U, 0xE912U, 0x0030U);
    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator
        ^ std::to_integer<std::uint8_t>(staged[0x0046U]));
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    cycle += 12U;
    append_checkpoint(0xE912U, 0xE914U, cycle, checkpoint);
    store_zero_page(0xE914U, 0xE916U, 0x0046U);
    load_zero_page(0xE916U, 0xE918U, 0x0030U);
    cycle += division.dividend_negative != 0U ? 6U : 12U;
    append_checkpoint(
        0xE918U,
        division.dividend_negative != 0U ? 0xE91AU : 0xE925U,
        cycle,
        checkpoint);
    if (division.dividend_negative != 0U) {
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(0xE91AU, 0xE91BU, cycle, checkpoint);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE91BU, 0xE91CU, cycle, checkpoint);
        subtract_zero_page(0xE91CU, 0xE91EU, 0x002FU);
        store_zero_page(0xE91EU, 0xE920U, 0x002FU);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE920U, 0xE921U, cycle, checkpoint);
        subtract_zero_page(0xE921U, 0xE923U, 0x0030U);
        store_zero_page(0xE923U, 0xE925U, 0x0030U);
    }

    append_write(
        0xE925U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        0xE9U,
        cycle);
    append_write(
        0xE925U,
        static_cast<std::uint16_t>(
            0x2100U
            + static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 1U)),
        0x27U,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    cycle += 21U;
    append_checkpoint(0xE925U, 0xE8E7U, cycle, checkpoint);

    cycle += 9U;
    append_write(
        0xE8E7U,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.x,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0xE8E7U, 0xE8E8U, cycle, checkpoint);

    cycle += 12U;
    append_write(0xE8E8U, 0x202EU, 0U, cycle);
    append_checkpoint(0xE8E8U, 0xE8EAU, cycle, checkpoint);

    checkpoint.x = 0x10U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    cycle += 6U;
    append_checkpoint(0xE8EAU, 0xE8ECU, cycle, checkpoint);
    rotate_zero_page(0xE8ECU, 0xE8EEU, 0x002FU);
    rotate_zero_page(0xE8EEU, 0xE8F0U, 0x0030U);

    for (std::size_t index = 0U;
         index < DC_UNSIGNED_DIVIDE_STEP_COUNT;
         ++index) {
        const auto& step = division.steps[index];
        rotate_zero_page(0xE8F0U, 0xE8F2U, 0x002EU);
        load_zero_page(0xE8F2U, 0xE8F4U, 0x002EU);

        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.accumulator,
            std::to_integer<std::uint8_t>(staged[0x002DU]));
        cycle += 12U;
        append_checkpoint(0xE8F4U, 0xE8F6U, cycle, checkpoint);

        cycle += step.subtracts_divisor != 0U ? 6U : 12U;
        append_checkpoint(
            0xE8F6U,
            step.subtracts_divisor != 0U ? 0xE8F8U : 0xE8FCU,
            cycle,
            checkpoint);
        if (step.subtracts_divisor != 0U) {
            subtract_zero_page(0xE8F8U, 0xE8FAU, 0x002DU);
            store_zero_page(0xE8FAU, 0xE8FCU, 0x002EU);
        }
        rotate_zero_page(0xE8FCU, 0xE8FEU, 0x002FU);
        rotate_zero_page(0xE8FEU, 0xE900U, 0x0030U);

        checkpoint.x = static_cast<std::uint8_t>(
            checkpoint.x - 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
        cycle += 6U;
        append_checkpoint(0xE900U, 0xE901U, cycle, checkpoint);
        const bool repeats =
            index + 1U < DC_UNSIGNED_DIVIDE_STEP_COUNT;
        cycle += repeats ? 12U : 6U;
        append_checkpoint(
            0xE901U,
            repeats ? 0xE8F0U : 0xE903U,
            cycle,
            checkpoint);
    }

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.x = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    cycle += 12U;
    append_checkpoint(0xE903U, 0xE904U, cycle, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(0xE904U, 0xE928U, cycle, checkpoint);

    load_zero_page(0xE928U, 0xE92AU, 0x0046U);
    cycle += division.result_negative != 0U ? 6U : 12U;
    append_checkpoint(
        0xE92AU,
        division.result_negative != 0U ? 0xE92CU : 0xE937U,
        cycle,
        checkpoint);
    if (division.result_negative != 0U) {
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(0xE92CU, 0xE92DU, cycle, checkpoint);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE92DU, 0xE92EU, cycle, checkpoint);
        subtract_zero_page(0xE92EU, 0xE930U, 0x002FU);
        store_zero_page(0xE930U, 0xE932U, 0x002FU);
        cycle += 6U;
        checkpoint.accumulator = 0U;
        append_checkpoint(0xE932U, 0xE933U, cycle, checkpoint);
        subtract_zero_page(0xE933U, 0xE935U, 0x0030U);
        store_zero_page(0xE935U, 0xE937U, 0x0030U);
    }

    const auto staged_result = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x0030U]))
           << 8U));
    if (std::to_integer<std::uint8_t>(staged[0x002DU])
            != division.divisor_magnitude
        || std::to_integer<std::uint8_t>(staged[0x002EU])
            != division.remainder
        || staged_result != division.result
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native E905 replacement composition diverged from its "
            "state model.");
    }

    checkpoint.program_counter = 0xE937U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_motion_damping(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xDD09U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_motion_damping damping{};
    if (!dc_damp_and_accelerate_motion(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &damping)) {
        throw std::runtime_error(
            "Native DD09 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xDD6DU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(160U);
    replacement.checkpoints.reserve(448U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U)
                != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U)
                != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_indexed =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    logical_base + checkpoint.x),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto or_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            checkpoint.accumulator = static_cast<std::uint8_t>(
                checkpoint.accumulator
                | std::to_integer<std::uint8_t>(
                    staged[
                        logical_base - 0x2000U + checkpoint.x]));
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            const auto left = checkpoint.accumulator;
            const auto right = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            const bool carry =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry,
                checkpoint.accumulator);
            cycle += 15U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto begin_jsr =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(
                    pushed_return_pc >> 8U),
                cycle);
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                0xE905U,
                cycle,
                checkpoint);
        };
    const auto append_child =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_checkpoint](
            const RoutineReplacement& child,
            const std::uint16_t resume_pc) {
            const auto child_offset = cycle;
            for (auto write : child.work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child.checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child.body_master_cycles;
            checkpoint = child.output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child.return_instruction.physical_bank,
                child.return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto run_signed_division =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(instruction_pc, pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE905U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE905U;
            auto child = replace_signed_division(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };

    load_indexed(0xDD09U, 0xDD0CU, 0x3400U);
    or_indexed(0xDD0CU, 0xDD0FU, 0x3480U);
    cycle += damping.first_component_nonzero != 0U ? 6U : 12U;
    append_checkpoint(
        0x01U,
        0xDD0FU,
        damping.first_component_nonzero != 0U
            ? 0xDD11U
            : 0xDD33U,
        cycle,
        checkpoint);
    if (damping.first_component_nonzero != 0U) {
        load_indexed(0xDD11U, 0xDD14U, 0x3400U);
        store_zero_page(0xDD14U, 0xDD16U, 0x002FU);
        load_indexed(0xDD16U, 0xDD19U, 0x3480U);
        store_zero_page(0xDD19U, 0xDD1BU, 0x0030U);
        load_zero_page(0xDD1BU, 0xDD1DU, 0x008CU);
        store_zero_page(0xDD1DU, 0xDD1FU, 0x002DU);
        if (!run_signed_division(
                0xDD1FU,
                0xDD21U,
                0xDD22U)) {
            return std::nullopt;
        }
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(
            0x01U,
            0xDD22U,
            0xDD23U,
            cycle,
            checkpoint);
        load_indexed(0xDD23U, 0xDD26U, 0x3400U);
        subtract_zero_page(0xDD26U, 0xDD28U, 0x002FU);
        store_indexed(0xDD28U, 0xDD2BU, 0x3400U);
        load_indexed(0xDD2BU, 0xDD2EU, 0x3480U);
        subtract_zero_page(0xDD2EU, 0xDD30U, 0x0030U);
        store_indexed(0xDD30U, 0xDD33U, 0x3480U);
    }

    load_indexed(0xDD33U, 0xDD36U, 0x3500U);
    or_indexed(0xDD36U, 0xDD39U, 0x3580U);
    cycle += damping.second_component_nonzero != 0U ? 6U : 12U;
    append_checkpoint(
        0x01U,
        0xDD39U,
        damping.second_component_nonzero != 0U
            ? 0xDD3BU
            : 0xDD5DU,
        cycle,
        checkpoint);
    if (damping.second_component_nonzero != 0U) {
        load_indexed(0xDD3BU, 0xDD3EU, 0x3500U);
        store_zero_page(0xDD3EU, 0xDD40U, 0x002FU);
        load_indexed(0xDD40U, 0xDD43U, 0x3580U);
        store_zero_page(0xDD43U, 0xDD45U, 0x0030U);
        load_zero_page(0xDD45U, 0xDD47U, 0x008DU);
        store_zero_page(0xDD47U, 0xDD49U, 0x002DU);
        if (!run_signed_division(
                0xDD49U,
                0xDD4BU,
                0xDD4CU)) {
            return std::nullopt;
        }
        cycle += 6U;
        checkpoint.status = static_cast<std::uint8_t>(
            (checkpoint.status
             & static_cast<std::uint8_t>(~0x20U))
            | 0x01U);
        append_checkpoint(
            0x01U,
            0xDD4CU,
            0xDD4DU,
            cycle,
            checkpoint);
        load_indexed(0xDD4DU, 0xDD50U, 0x3500U);
        subtract_zero_page(0xDD50U, 0xDD52U, 0x002FU);
        store_indexed(0xDD52U, 0xDD55U, 0x3500U);
        load_indexed(0xDD55U, 0xDD58U, 0x3580U);
        subtract_zero_page(0xDD58U, 0xDD5AU, 0x0030U);
        store_indexed(0xDD5AU, 0xDD5DU, 0x3580U);
    }

    cycle += 6U;
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x21U));
    append_checkpoint(
        0x01U,
        0xDD5DU,
        0xDD5EU,
        cycle,
        checkpoint);
    load_indexed(0xDD5EU, 0xDD61U, 0x3500U);
    add_zero_page(0xDD61U, 0xDD63U, 0x008BU);
    store_indexed(0xDD63U, 0xDD66U, 0x3500U);
    cycle += 6U;
    checkpoint.accumulator = 0U;
    append_checkpoint(
        0x01U,
        0xDD66U,
        0xDD67U,
        cycle,
        checkpoint);
    add_indexed(0xDD67U, 0xDD6AU, 0x3580U);
    store_indexed(0xDD6AU, 0xDD6DU, 0x3580U);

    const auto first_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1400U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1480U + checkpoint.x]))
           << 8U));
    const auto second_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(
            staged[0x1500U + checkpoint.x])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   staged[0x1580U + checkpoint.x]))
           << 8U));
    if (first_output != damping.first_damped
        || second_output != damping.second_accelerated
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native DD09 replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }

    checkpoint.program_counter = 0xDD6DU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_unsigned_multiply_round(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE624U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_unsigned_multiply_round round{};
    if (!dc_multiply_unsigned_round(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &round)) {
        throw std::runtime_error(
            "Native E624 replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE65BU,
        },
        .output_registers = entry.registers,
        .body_master_cycles = static_cast<std::uint32_t>(
            600U + 84U * round.add_count),
    };
    replacement.work_ram_writes.reserve(
        static_cast<std::size_t>(24U + 3U * round.add_count));
    replacement.checkpoints.reserve(
        static_cast<std::size_t>(41U + 9U * round.add_count));

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto shift_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 6U;
    checkpoint.x = 4U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xE624U, 0xE626U, cycle, checkpoint);

    std::uint8_t low = static_cast<std::uint8_t>(round.accumulator);
    std::uint8_t high =
        static_cast<std::uint8_t>(round.accumulator >> 8U);
    std::uint8_t multiplier = round.multiplier;
    for (std::size_t outer = 0U; outer < 4U; ++outer) {
        for (std::size_t half = 0U; half < 2U; ++half) {
            const auto& step = round.steps[2U * outer + half];
            const auto low_shift_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE626U : 0xE63FU);
            const auto high_shift_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE628U : 0xE641U);
            const auto multiplier_shift_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62AU : 0xE643U);
            const auto admission_branch_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62CU : 0xE645U);
            const auto clear_carry_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62EU : 0xE647U);
            const auto load_low_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE62FU : 0xE648U);
            const auto add_low_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE631U : 0xE64AU);
            const auto store_low_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE633U : 0xE64CU);
            const auto load_high_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE635U : 0xE64EU);
            const auto add_high_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE637U : 0xE650U);
            const auto store_high_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE639U : 0xE652U);
            const auto carry_branch_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE63BU : 0xE654U);
            const auto increment_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE63DU : 0xE656U);
            const auto end_pc = static_cast<std::uint16_t>(
                half == 0U ? 0xE63FU : 0xE658U);

            const auto low_before_shift = low;
            cycle += 18U;
            low = step.low_after_shift;
            checkpoint.status = shift_status(
                checkpoint.status,
                low_before_shift,
                low);
            append_write(low_shift_pc, 0x2042U, low, cycle);
            append_checkpoint(
                low_shift_pc,
                high_shift_pc,
                cycle,
                checkpoint);

            const auto high_before_shift = high;
            cycle += 18U;
            high = step.high_after_shift;
            checkpoint.status = shift_status(
                checkpoint.status,
                high_before_shift,
                high);
            append_write(high_shift_pc, 0x2043U, high, cycle);
            append_checkpoint(
                high_shift_pc,
                multiplier_shift_pc,
                cycle,
                checkpoint);

            const auto multiplier_before_shift = multiplier;
            cycle += 18U;
            multiplier = step.multiplier_after_shift;
            checkpoint.status = shift_status(
                checkpoint.status,
                multiplier_before_shift,
                multiplier);
            append_write(
                multiplier_shift_pc,
                0x2030U,
                multiplier,
                cycle);
            append_checkpoint(
                multiplier_shift_pc,
                admission_branch_pc,
                cycle,
                checkpoint);

            if (step.adds_multiplicand == 0U) {
                cycle += 12U;
                append_checkpoint(
                    admission_branch_pc,
                    end_pc,
                    cycle,
                    checkpoint);
                continue;
            }

            cycle += 6U;
            append_checkpoint(
                admission_branch_pc,
                clear_carry_pc,
                cycle,
                checkpoint);
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                clear_carry_pc,
                load_low_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            checkpoint.accumulator = low;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                load_low_pc,
                add_low_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            const auto low_carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = step.low_after_add;
            checkpoint.status = adc_status(
                checkpoint.status,
                low,
                static_cast<std::uint8_t>(round.multiplicand),
                low_carry_in,
                checkpoint.accumulator);
            low = step.low_after_add;
            append_checkpoint(
                add_low_pc,
                store_low_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            append_write(store_low_pc, 0x2042U, low, cycle);
            append_checkpoint(
                store_low_pc,
                load_high_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            checkpoint.accumulator = high;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                load_high_pc,
                add_high_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            const auto high_carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = step.high_after_add;
            checkpoint.status = adc_status(
                checkpoint.status,
                high,
                static_cast<std::uint8_t>(
                    round.multiplicand >> 8U),
                high_carry_in,
                checkpoint.accumulator);
            high = step.high_after_add;
            append_checkpoint(
                add_high_pc,
                store_high_pc,
                cycle,
                checkpoint);

            cycle += 12U;
            append_write(store_high_pc, 0x2043U, high, cycle);
            append_checkpoint(
                store_high_pc,
                carry_branch_pc,
                cycle,
                checkpoint);

            if (step.add_carry == 0U) {
                cycle += 12U;
                append_checkpoint(
                    carry_branch_pc,
                    end_pc,
                    cycle,
                    checkpoint);
                multiplier = step.multiplier_after_add;
                continue;
            }

            cycle += 6U;
            append_checkpoint(
                carry_branch_pc,
                increment_pc,
                cycle,
                checkpoint);
            cycle += 18U;
            multiplier = step.multiplier_after_add;
            checkpoint.status =
                set_nz(checkpoint.status, multiplier);
            append_write(increment_pc, 0x2030U, multiplier, cycle);
            append_checkpoint(
                increment_pc,
                end_pc,
                cycle,
                checkpoint);
        }

        cycle += 6U;
        checkpoint.x = static_cast<std::uint8_t>(
            checkpoint.x - 1U);
        checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
        append_checkpoint(0xE658U, 0xE659U, cycle, checkpoint);

        const bool repeats = checkpoint.x != 0U;
        cycle += repeats ? 12U : 6U;
        append_checkpoint(
            0xE659U,
            repeats ? 0xE626U : 0xE65BU,
            cycle,
            checkpoint);
    }

    replacement.body_master_cycles = cycle;
    checkpoint.program_counter = 0xE65BU;
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_fixed_multiply(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE65CU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_signed_fixed_multiply multiply{};
    if (!dc_multiply_signed_fixed(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &multiply)) {
        throw std::runtime_error(
            "Native E65C replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE694U,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(128U);
    replacement.checkpoints.reserve(320U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::int16_t>(left)
                - static_cast<std::int16_t>(right)
                - static_cast<std::int16_t>(carry_in ? 0 : 1));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if ((((left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_zero_page =
        [&checkpoint, &cycle, &staged, &set_nz, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint, &cycle, &append_write, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto set_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                (checkpoint.status
                    & static_cast<std::uint8_t>(~0x20U))
                | 0x01U);
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_accumulator =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.accumulator = 0U;
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto subtract_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &sbc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                - static_cast<std::uint16_t>(right)
                - static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            checkpoint.status = sbc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto append_jsr =
        [&checkpoint, &cycle, &append_write, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(pushed_return_pc >> 8U),
                cycle);
            append_write(
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                instruction_pc,
                target_pc,
                cycle,
                checkpoint);
        };

    load_zero_page(0xE65CU, 0xE65EU, 0x0030U);
    store_zero_page(0xE65EU, 0xE660U, 0x0046U);
    if (multiply.negates_multiplier == 0U) {
        cycle += 12U;
        append_checkpoint(0xE660U, 0xE66DU, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE660U, 0xE662U, cycle, checkpoint);
        set_carry(0xE662U, 0xE663U);
        clear_accumulator(0xE663U, 0xE664U);
        subtract_zero_page(0xE664U, 0xE666U, 0x002FU);
        store_zero_page(0xE666U, 0xE668U, 0x002FU);
        clear_accumulator(0xE668U, 0xE669U);
        subtract_zero_page(0xE669U, 0xE66BU, 0x0030U);
        store_zero_page(0xE66BU, 0xE66DU, 0x0030U);
    }

    load_zero_page(0xE66DU, 0xE66FU, 0x002EU);
    cycle += 12U;
    checkpoint.accumulator = static_cast<std::uint8_t>(
        checkpoint.accumulator
        ^ std::to_integer<std::uint8_t>(staged[0x0046U]));
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE66FU, 0xE671U, cycle, checkpoint);
    store_zero_page(0xE671U, 0xE673U, 0x0046U);
    load_zero_page(0xE673U, 0xE675U, 0x002EU);
    if (multiply.negates_multiplicand == 0U) {
        cycle += 12U;
        append_checkpoint(0xE675U, 0xE682U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE675U, 0xE677U, cycle, checkpoint);
        set_carry(0xE677U, 0xE678U);
        clear_accumulator(0xE678U, 0xE679U);
        subtract_zero_page(0xE679U, 0xE67BU, 0x002DU);
        store_zero_page(0xE67BU, 0xE67DU, 0x002DU);
        clear_accumulator(0xE67DU, 0xE67EU);
        subtract_zero_page(0xE67EU, 0xE680U, 0x002EU);
        store_zero_page(0xE680U, 0xE682U, 0x002EU);
    }

    append_jsr(0xE682U, 0xE60BU, 0xE684U);

    cycle += 9U;
    append_write(
        0xE60BU,
        static_cast<std::uint16_t>(
            0x2100U + checkpoint.stack_pointer),
        checkpoint.x,
        cycle);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0xE60BU, 0xE60CU, cycle, checkpoint);

    cycle += 12U;
    append_write(0xE60CU, 0x2042U, 0U, cycle);
    append_checkpoint(0xE60CU, 0xE60EU, cycle, checkpoint);
    cycle += 12U;
    append_write(0xE60EU, 0x2043U, 0U, cycle);
    append_checkpoint(0xE60EU, 0xE610U, cycle, checkpoint);
    load_zero_page(0xE610U, 0xE612U, 0x0030U);

    const auto append_multiply_round =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_jsr,
         &append_checkpoint](
            const std::uint16_t jsr_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            append_jsr(jsr_pc, 0xE624U, pushed_return_pc);
            const auto child_offset = cycle;
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE624U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE624U;
            auto child = replace_unsigned_multiply_round(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            for (auto write : child->work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child->checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child->body_master_cycles;
            checkpoint = child->output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child->return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
            return true;
        };

    if (multiply.runs_high_round == 0U) {
        cycle += 12U;
        append_checkpoint(0xE612U, 0xE617U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE612U, 0xE614U, cycle, checkpoint);
        if (!append_multiply_round(
                0xE614U,
                0xE616U,
                0xE617U)) {
            return std::nullopt;
        }
    }

    load_zero_page(0xE617U, 0xE619U, 0x002FU);
    store_zero_page(0xE619U, 0xE61BU, 0x0030U);
    if (!append_multiply_round(
            0xE61BU,
            0xE61DU,
            0xE61EU)) {
        return std::nullopt;
    }
    load_zero_page(0xE61EU, 0xE620U, 0x0043U);
    store_zero_page(0xE620U, 0xE622U, 0x002FU);

    cycle += 12U;
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.x = std::to_integer<std::uint8_t>(
        staged[0x0100U + checkpoint.stack_pointer]);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(0xE622U, 0xE623U, cycle, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    cycle += 21U;
    append_checkpoint(0xE623U, 0xE685U, cycle, checkpoint);

    load_zero_page(0xE685U, 0xE687U, 0x0046U);
    if ((multiply.sign_xor & 0x80U) == 0U) {
        cycle += 12U;
        append_checkpoint(0xE687U, 0xE694U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(0xE687U, 0xE689U, cycle, checkpoint);
        set_carry(0xE689U, 0xE68AU);
        clear_accumulator(0xE68AU, 0xE68BU);
        subtract_zero_page(0xE68BU, 0xE68DU, 0x002FU);
        store_zero_page(0xE68DU, 0xE68FU, 0x002FU);
        clear_accumulator(0xE68FU, 0xE690U);
        subtract_zero_page(0xE690U, 0xE692U, 0x0030U);
        store_zero_page(0xE692U, 0xE694U, 0x0030U);
    }

    const auto staged_result = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x002FU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x0030U]))
           << 8U));
    if (staged_result != multiply.result
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native E65C replacement composition diverged from its "
            "state model.");
    }
    checkpoint.program_counter = 0xE694U;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_signed_word_negation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE837U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_signed_word_negation negation{};
    if (!dc_negate_signed_word(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &negation)) {
        throw std::runtime_error(
            "Native E837 replacement rejected the work-RAM snapshot.");
    }

    const auto input_low =
        static_cast<std::uint8_t>(negation.input & 0xFFU);
    const auto input_high =
        static_cast<std::uint8_t>(negation.input >> 8U);
    const auto complemented_low =
        static_cast<std::uint8_t>(input_low ^ 0xFFU);
    const auto result_low =
        static_cast<std::uint8_t>(negation.result & 0xFFU);
    const auto result_high =
        static_cast<std::uint8_t>(negation.result >> 8U);

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };

    auto output = entry.registers;
    output.program_counter = 0xE849U;
    output.accumulator = negation.complemented_high;
    output.status = set_nz(
        output.status,
        negation.increments_high != 0U ? result_high : result_low);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE849U,
        },
        .output_registers = output,
        .body_master_cycles =
            negation.increments_high != 0U ? 102U : 90U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x00U, 0xE83BU},
            .logical_address = 0x202FU,
            .value = complemented_low,
            .master_cycle_offset = 30U,
        },
        {
            .instruction = {0x00U, 0xE841U},
            .logical_address = 0x2030U,
            .value = negation.complemented_high,
            .master_cycle_offset = 60U,
        },
        {
            .instruction = {0x00U, 0xE843U},
            .logical_address = 0x202FU,
            .value = result_low,
            .master_cycle_offset = 78U,
        },
    };
    if (negation.increments_high != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xE847U},
            .logical_address = 0x2030U,
            .value = result_high,
            .master_cycle_offset = 102U,
        });
    }

    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = input_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE837U, 0xE839U, 12U, checkpoint);

    checkpoint.accumulator = complemented_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE839U, 0xE83BU, 18U, checkpoint);
    append_checkpoint(0xE83BU, 0xE83DU, 30U, checkpoint);

    checkpoint.accumulator = input_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE83DU, 0xE83FU, 42U, checkpoint);

    checkpoint.accumulator = negation.complemented_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE83FU, 0xE841U, 48U, checkpoint);
    append_checkpoint(0xE841U, 0xE843U, 60U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, result_low);
    append_checkpoint(0xE843U, 0xE845U, 78U, checkpoint);
    if (negation.increments_high == 0U) {
        append_checkpoint(0xE845U, 0xE849U, 90U, output);
        return replacement;
    }

    append_checkpoint(0xE845U, 0xE847U, 84U, checkpoint);
    append_checkpoint(0xE847U, 0xE849U, 102U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_projection(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr std::size_t kMaximumReplacementWrites = 512U;
    constexpr std::size_t kMaximumReplacementCheckpoints = 2048U;
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD09EU,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.x >= 0x80U
        || (entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_flipper_projection projection{};
    if (!dc_project_flipper_vectors(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &projection)) {
        throw std::runtime_error(
            "Native D09E replacement rejected the work-RAM snapshot.");
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD11FU,
        },
        .output_registers = entry.registers,
    };
    replacement.work_ram_writes.reserve(512U);
    replacement.checkpoints.reserve(512U);
    std::vector<std::byte> staged(work_ram.begin(), work_ram.end());

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .registers = registers,
            });
        };
    const auto append_write =
        [&replacement, &staged](
            const std::uint8_t instruction_bank,
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {
                    instruction_bank,
                    instruction_pc,
                },
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
            if (logical_address >= 0x2000U
                && logical_address < 0x4000U) {
                staged[logical_address - 0x2000U] =
                    static_cast<std::byte>(value);
            }
        };

    auto checkpoint = entry.registers;
    std::uint32_t cycle = 0U;
    const auto load_indexed =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t logical_base) {
            cycle += 15U;
            checkpoint.accumulator = std::to_integer<std::uint8_t>(
                staged[logical_base - 0x2000U + checkpoint.x]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto load_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &set_nz,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            checkpoint.accumulator =
                std::to_integer<std::uint8_t>(staged[offset]);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto store_zero_page =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            cycle += 12U;
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(0x2000U + offset),
                checkpoint.accumulator,
                cycle);
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto clear_carry =
        [&checkpoint, &cycle, &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc) {
            cycle += 6U;
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x21U));
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto add_zero_page =
        [&checkpoint,
         &cycle,
         &staged,
         &adc_status,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint16_t offset) {
            const auto left = checkpoint.accumulator;
            const auto right =
                std::to_integer<std::uint8_t>(staged[offset]);
            const auto carry_in =
                (checkpoint.status & 0x01U) != 0U;
            checkpoint.accumulator = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            checkpoint.status = adc_status(
                checkpoint.status,
                left,
                right,
                carry_in,
                checkpoint.accumulator);
            cycle += 12U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto begin_jsr =
        [&checkpoint,
         &cycle,
         &append_write,
         &append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t target_pc,
            const std::uint16_t pushed_return_pc) {
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + checkpoint.stack_pointer),
                static_cast<std::uint8_t>(pushed_return_pc >> 8U),
                cycle);
            append_write(
                0x01U,
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U
                    + static_cast<std::uint8_t>(
                        checkpoint.stack_pointer - 1U)),
                static_cast<std::uint8_t>(pushed_return_pc),
                cycle);
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer - 2U);
            cycle += 21U;
            append_checkpoint(
                0x01U,
                instruction_pc,
                target_pc,
                cycle,
                checkpoint);
        };
    const auto append_child =
        [&replacement,
         &staged,
         &checkpoint,
         &cycle,
         &append_checkpoint](
            const RoutineReplacement& child,
            const std::uint16_t resume_pc) {
            const auto child_offset = cycle;
            for (auto write : child.work_ram_writes) {
                write.master_cycle_offset += child_offset;
                replacement.work_ram_writes.push_back(write);
                staged[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            for (auto child_checkpoint : child.checkpoints) {
                child_checkpoint.interrupt_sample_master_cycle_offset +=
                    child_offset;
                child_checkpoint.master_cycle_offset += child_offset;
                replacement.checkpoints.push_back(child_checkpoint);
            }
            cycle += child.body_master_cycles;
            checkpoint = child.output_registers;
            checkpoint.stack_pointer = static_cast<std::uint8_t>(
                checkpoint.stack_pointer + 2U);
            cycle += 21U;
            append_checkpoint(
                child.return_instruction.physical_bank,
                child.return_instruction.logical_pc,
                resume_pc,
                cycle,
                checkpoint);
        };
    const auto run_signed_multiply =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE65CU,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE65CU,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE65CU;
            auto child = replace_signed_fixed_multiply(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };
    const auto run_negation =
        [&checkpoint,
         &staged,
         &begin_jsr,
         &append_child](
            const std::uint16_t instruction_pc,
            const std::uint16_t pushed_return_pc,
            const std::uint16_t resume_pc) {
            begin_jsr(
                instruction_pc,
                0xE837U,
                pushed_return_pc);
            RoutineEntry child_entry{
                .routine = {
                    .physical_bank = 0x00U,
                    .logical_pc = 0xE837U,
                },
                .registers = checkpoint,
            };
            child_entry.registers.program_counter = 0xE837U;
            auto child = replace_signed_word_negation(
                child_entry,
                staged);
            if (!child || child->checkpoints.empty()) {
                return false;
            }
            append_child(*child, resume_pc);
            return true;
        };

    load_indexed(0xD09EU, 0xD0A1U, 0x3400U);
    store_zero_page(0xD0A1U, 0xD0A3U, 0x002FU);
    load_indexed(0xD0A3U, 0xD0A6U, 0x3480U);
    store_zero_page(0xD0A6U, 0xD0A8U, 0x0030U);
    load_zero_page(0xD0A8U, 0xD0AAU, 0x007DU);
    store_zero_page(0xD0AAU, 0xD0ACU, 0x002DU);
    load_zero_page(0xD0ACU, 0xD0AEU, 0x007EU);
    store_zero_page(0xD0AEU, 0xD0B0U, 0x002EU);
    if (!run_signed_multiply(
            0xD0B0U,
            0xD0B2U,
            0xD0B3U)) {
        return std::nullopt;
    }
    load_zero_page(0xD0B3U, 0xD0B5U, 0x002FU);
    store_zero_page(0xD0B5U, 0xD0B7U, 0x00C0U);
    load_zero_page(0xD0B7U, 0xD0B9U, 0x0030U);
    store_zero_page(0xD0B9U, 0xD0BBU, 0x00C1U);

    load_indexed(0xD0BBU, 0xD0BEU, 0x3500U);
    store_zero_page(0xD0BEU, 0xD0C0U, 0x002FU);
    load_indexed(0xD0C0U, 0xD0C3U, 0x3580U);
    store_zero_page(0xD0C3U, 0xD0C5U, 0x0030U);
    load_zero_page(0xD0C5U, 0xD0C7U, 0x007FU);
    store_zero_page(0xD0C7U, 0xD0C9U, 0x002DU);
    load_zero_page(0xD0C9U, 0xD0CBU, 0x0080U);
    store_zero_page(0xD0CBU, 0xD0CDU, 0x002EU);
    if (!run_signed_multiply(
            0xD0CDU,
            0xD0CFU,
            0xD0D0U)) {
        return std::nullopt;
    }
    clear_carry(0xD0D0U, 0xD0D1U);
    load_zero_page(0xD0D1U, 0xD0D3U, 0x00C0U);
    add_zero_page(0xD0D3U, 0xD0D5U, 0x002FU);
    store_zero_page(0xD0D5U, 0xD0D7U, 0x00C0U);
    load_zero_page(0xD0D7U, 0xD0D9U, 0x00C1U);
    add_zero_page(0xD0D9U, 0xD0DBU, 0x0030U);
    store_zero_page(0xD0DBU, 0xD0DDU, 0x00C1U);

    load_indexed(0xD0DDU, 0xD0E0U, 0x3400U);
    store_zero_page(0xD0E0U, 0xD0E2U, 0x002FU);
    load_indexed(0xD0E2U, 0xD0E5U, 0x3480U);
    store_zero_page(0xD0E5U, 0xD0E7U, 0x0030U);
    load_zero_page(0xD0E7U, 0xD0E9U, 0x007FU);
    store_zero_page(0xD0E9U, 0xD0EBU, 0x002DU);
    load_zero_page(0xD0EBU, 0xD0EDU, 0x0080U);
    store_zero_page(0xD0EDU, 0xD0EFU, 0x002EU);
    if (!run_signed_multiply(
            0xD0EFU,
            0xD0F1U,
            0xD0F2U)
        || !run_negation(
            0xD0F2U,
            0xD0F4U,
            0xD0F5U)) {
        return std::nullopt;
    }
    load_zero_page(0xD0F5U, 0xD0F7U, 0x002FU);
    store_zero_page(0xD0F7U, 0xD0F9U, 0x00BEU);
    load_zero_page(0xD0F9U, 0xD0FBU, 0x0030U);
    store_zero_page(0xD0FBU, 0xD0FDU, 0x00BFU);

    load_indexed(0xD0FDU, 0xD100U, 0x3500U);
    store_zero_page(0xD100U, 0xD102U, 0x002FU);
    load_indexed(0xD102U, 0xD105U, 0x3580U);
    store_zero_page(0xD105U, 0xD107U, 0x0030U);
    load_zero_page(0xD107U, 0xD109U, 0x007DU);
    store_zero_page(0xD109U, 0xD10BU, 0x002DU);
    load_zero_page(0xD10BU, 0xD10DU, 0x007EU);
    store_zero_page(0xD10DU, 0xD10FU, 0x002EU);
    if (!run_signed_multiply(
            0xD10FU,
            0xD111U,
            0xD112U)) {
        return std::nullopt;
    }
    clear_carry(0xD112U, 0xD113U);
    load_zero_page(0xD113U, 0xD115U, 0x00BEU);
    add_zero_page(0xD115U, 0xD117U, 0x002FU);
    store_zero_page(0xD117U, 0xD119U, 0x00BEU);
    load_zero_page(0xD119U, 0xD11BU, 0x00BFU);
    add_zero_page(0xD11BU, 0xD11DU, 0x0030U);
    store_zero_page(0xD11DU, 0xD11FU, 0x00BFU);

    const auto first_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00C0U])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00C1U]))
           << 8U));
    const auto second_output = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(staged[0x00BEU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(staged[0x00BFU]))
           << 8U));
    if (first_output != projection.first_output
        || second_output != projection.second_output
        || checkpoint.stack_pointer
            != entry.registers.stack_pointer) {
        throw std::runtime_error(
            "Native D09E replacement composition diverged from its "
            "state model.");
    }
    if (replacement.work_ram_writes.size()
            > kMaximumReplacementWrites
        || replacement.checkpoints.size()
            > kMaximumReplacementCheckpoints) {
        return std::nullopt;
    }
    checkpoint.program_counter = 0xD11FU;
    replacement.output_registers = checkpoint;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_rng_update(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xE8B3U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_rng_update update{};
    if (!dc_advance_rng(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &update)) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xE8E6U;
    output.accumulator = update.seed_high;
    output.status =
        static_cast<std::uint8_t>(entry.registers.status & 0x3CU);
    if (update.final_carry != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (update.final_overflow != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xE8E6U,
        },
        .output_registers = output,
        .body_master_cycles = 282U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x00U, 0xE8B9U},
            .logical_address = 0x2042U,
            .value = update.constant_sum_low,
            .master_cycle_offset = 39U,
        },
        {
            .instruction = {0x00U, 0xE8C0U},
            .logical_address = 0x2043U,
            .value = update.constant_sum_high,
            .master_cycle_offset = 72U,
        },
        {
            .instruction = {0x00U, 0xE8C2U},
            .logical_address = 0x2042U,
            .value = update.doubled_low,
            .master_cycle_offset = 90U,
        },
        {
            .instruction = {0x00U, 0xE8C4U},
            .logical_address = 0x2043U,
            .value = update.doubled_high,
            .master_cycle_offset = 108U,
        },
        {
            .instruction = {0x00U, 0xE8CCU},
            .logical_address = 0x2042U,
            .value = update.frame_sum_low,
            .master_cycle_offset = 153U,
        },
        {
            .instruction = {0x00U, 0xE8D3U},
            .logical_address = 0x2043U,
            .value = update.mixed_high,
            .master_cycle_offset = 192U,
        },
        {
            .instruction = {0x00U, 0xE8DBU},
            .logical_address = 0x222BU,
            .value = update.seed_high,
            .master_cycle_offset = 240U,
        },
        {
            .instruction = {0x00U, 0xE8E0U},
            .logical_address = 0x222AU,
            .value = update.seed_low,
            .master_cycle_offset = 267U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto shift_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };

    const auto seed_low =
        std::to_integer<std::uint8_t>(work_ram[0x022AU]);
    const auto seed_high =
        std::to_integer<std::uint8_t>(work_ram[0x022BU]);
    const auto frame_counter =
        std::to_integer<std::uint8_t>(work_ram[0x0000U]);
    const auto constant_low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(seed_low) + 0x3FU);
    const auto constant_low_carry = constant_low_sum > 0xFFU;
    const auto frame_low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(seed_low)
        + static_cast<std::uint16_t>(frame_counter));
    const auto frame_low_carry = frame_low_sum > 0xFFU;

    auto checkpoint = entry.registers;
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xE8B3U, 0xE8B4U, 6U, checkpoint);

    checkpoint.accumulator = seed_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8B4U, 0xE8B7U, 21U, checkpoint);

    checkpoint.accumulator = update.constant_sum_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_low,
        0x3FU,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xE8B7U, 0xE8B9U, 27U, checkpoint);
    append_checkpoint(0xE8B9U, 0xE8BBU, 39U, checkpoint);

    checkpoint.accumulator = seed_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8BBU, 0xE8BEU, 54U, checkpoint);

    checkpoint.accumulator = update.constant_sum_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_high,
        0x50U,
        constant_low_carry,
        checkpoint.accumulator);
    append_checkpoint(0xE8BEU, 0xE8C0U, 60U, checkpoint);
    append_checkpoint(0xE8C0U, 0xE8C2U, 72U, checkpoint);

    checkpoint.status = shift_status(
        checkpoint.status,
        update.constant_sum_low,
        update.doubled_low);
    append_checkpoint(0xE8C2U, 0xE8C4U, 90U, checkpoint);

    checkpoint.status = shift_status(
        checkpoint.status,
        update.constant_sum_high,
        update.doubled_high);
    append_checkpoint(0xE8C4U, 0xE8C6U, 108U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xE8C6U, 0xE8C7U, 114U, checkpoint);

    checkpoint.accumulator = seed_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8C7U, 0xE8CAU, 129U, checkpoint);

    checkpoint.accumulator = update.frame_sum_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_low,
        frame_counter,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xE8CAU, 0xE8CCU, 141U, checkpoint);
    append_checkpoint(0xE8CCU, 0xE8CEU, 153U, checkpoint);

    checkpoint.accumulator = seed_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8CEU, 0xE8D1U, 168U, checkpoint);

    checkpoint.accumulator = update.mixed_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_high,
        update.doubled_high,
        frame_low_carry,
        checkpoint.accumulator);
    append_checkpoint(0xE8D1U, 0xE8D3U, 180U, checkpoint);
    append_checkpoint(0xE8D3U, 0xE8D5U, 192U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xE8D5U, 0xE8D6U, 198U, checkpoint);

    checkpoint.accumulator = seed_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8D6U, 0xE8D9U, 213U, checkpoint);

    checkpoint.accumulator = update.seed_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        seed_low,
        update.mixed_high,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xE8D9U, 0xE8DBU, 225U, checkpoint);
    append_checkpoint(0xE8DBU, 0xE8DEU, 240U, checkpoint);

    checkpoint.accumulator = update.frame_sum_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xE8DEU, 0xE8E0U, 252U, checkpoint);
    append_checkpoint(0xE8E0U, 0xE8E3U, 267U, checkpoint);
    append_checkpoint(0xE8E3U, 0xE8E6U, 282U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_score_accumulator(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF55CU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    // Decimal and transfer modes alter the arithmetic instruction contract
    // and are not used by the observed gameplay callers.
    if ((entry.registers.status & 0x28U) != 0U) {
        return std::nullopt;
    }

    dc_score_update update{};
    if (!dc_accumulate_score_ordinary(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &update)) {
        return std::nullopt;
    }

    bool carry = true;
    const auto subtract_byte =
        [&carry](const std::uint8_t accumulator,
                 const std::uint8_t operand) {
            const auto subtrahend = static_cast<std::uint16_t>(
                operand + (carry ? 0U : 1U));
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(accumulator) - subtrahend);
            const bool overflow =
                ((accumulator ^ result) & (accumulator ^ operand) & 0x80U)
                != 0U;
            carry = static_cast<std::uint16_t>(accumulator) >= subtrahend;
            return std::pair{result, overflow};
        };
    (void)subtract_byte(
        update.score_low,
        static_cast<std::uint8_t>(update.comparison_score & 0xFFU));
    (void)subtract_byte(
        update.score_middle,
        static_cast<std::uint8_t>(
            (update.comparison_score >> 8U) & 0xFFU));
    const auto [unused_comparison, overflow] =
        subtract_byte(
            update.score_high,
            static_cast<std::uint8_t>(
                (update.comparison_score >> 16U) & 0xFFU));
    (void)unused_comparison;

    auto output = entry.registers;
    output.program_counter = 0xF5A8U;
    output.accumulator = update.dirty_flags;
    output.y = update.slot;
    output.status =
        static_cast<std::uint8_t>(entry.registers.status & 0x3CU);
    if (carry) {
        output.status = static_cast<std::uint8_t>(output.status | 0x01U);
    }
    if (overflow) {
        output.status = static_cast<std::uint8_t>(output.status | 0x40U);
    }
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF5A8U,
        },
        .output_registers = output,
        .body_master_cycles = 558U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x00U, 0xF587U},
            .logical_address =
                static_cast<std::uint16_t>(0x2205U + update.slot),
            .value = update.score_low,
            .master_cycle_offset = 108U,
        },
        {
            .instruction = {0x00U, 0xF58FU},
            .logical_address =
                static_cast<std::uint16_t>(0x220CU + update.slot),
            .value = update.score_middle,
            .master_cycle_offset = 150U,
        },
        {
            .instruction = {0x00U, 0xF597U},
            .logical_address =
                static_cast<std::uint16_t>(0x2213U + update.slot),
            .value = update.score_high,
            .master_cycle_offset = 186U,
        },
        {
            .instruction = {0x00U, 0xF59AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0xF5U,
            .master_cycle_offset = 186U,
        },
        {
            .instruction = {0x00U, 0xF59AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            .value = 0x9CU,
            .master_cycle_offset = 186U,
        },
        {
            .instruction = {0x00U, 0xF59DU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0xF5U,
            .master_cycle_offset = 324U,
        },
        {
            .instruction = {0x00U, 0xF59DU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            .value = 0x9FU,
            .master_cycle_offset = 324U,
        },
        {
            .instruction = {0x00U, 0xF5A5U},
            .logical_address = 0x2CC0U,
            .value = update.dirty_flags,
            .master_cycle_offset = 558U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (static_cast<std::uint16_t>(left) >= subtrahend) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((left ^ result) & (left ^ right) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };
    const auto subtract_result =
        [](const std::uint8_t left,
           const std::uint8_t right,
           const bool carry_in) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            return static_cast<std::uint8_t>(
                static_cast<std::uint16_t>(left) - subtrahend);
        };

    const auto previous_score = update.score - update.award;
    const auto previous_low =
        static_cast<std::uint8_t>(previous_score & 0xFFU);
    const auto previous_middle =
        static_cast<std::uint8_t>((previous_score >> 8U) & 0xFFU);
    const auto previous_high =
        static_cast<std::uint8_t>((previous_score >> 16U) & 0xFFU);
    const auto award_low =
        static_cast<std::uint8_t>(update.award & 0xFFU);
    const auto award_high =
        static_cast<std::uint8_t>((update.award >> 8U) & 0xFFU);
    const auto low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(previous_low) + award_low);
    const bool low_carry = low_sum > 0xFFU;
    const auto middle_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(previous_middle)
        + static_cast<std::uint16_t>(award_high)
        + static_cast<std::uint16_t>(low_carry));
    const bool middle_carry = middle_sum > 0xFFU;

    auto checkpoint = entry.registers;
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF55CU, 0xF55FU, 15U, checkpoint);
    append_checkpoint(0xF55FU, 0xF561U, 21U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF561U, 0xF563U, 33U, checkpoint);
    append_checkpoint(0xF563U, 0xF57EU, 45U, checkpoint);

    checkpoint.y = update.slot;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xF57EU, 0xF581U, 60U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0xF581U, 0xF582U, 66U, checkpoint);
    checkpoint.accumulator = previous_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF582U, 0xF584U, 78U, checkpoint);
    checkpoint.accumulator = update.score_low;
    checkpoint.status = adc_status(
        checkpoint.status,
        previous_low,
        award_low,
        false,
        checkpoint.accumulator);
    append_checkpoint(0xF584U, 0xF587U, 93U, checkpoint);
    append_checkpoint(0xF587U, 0xF58AU, 108U, checkpoint);

    checkpoint.accumulator = previous_middle;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF58AU, 0xF58CU, 120U, checkpoint);
    checkpoint.accumulator = update.score_middle;
    checkpoint.status = adc_status(
        checkpoint.status,
        previous_middle,
        award_high,
        low_carry,
        checkpoint.accumulator);
    append_checkpoint(0xF58CU, 0xF58FU, 135U, checkpoint);
    append_checkpoint(0xF58FU, 0xF592U, 150U, checkpoint);

    checkpoint.accumulator = previous_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF592U, 0xF595U, 165U, checkpoint);
    checkpoint.accumulator = update.score_high;
    checkpoint.status = adc_status(
        checkpoint.status,
        previous_high,
        0U,
        middle_carry,
        checkpoint.accumulator);
    append_checkpoint(0xF595U, 0xF597U, 171U, checkpoint);
    append_checkpoint(0xF597U, 0xF59AU, 186U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0xF59AU, 0xF5A9U, 207U, checkpoint);

    checkpoint.y = update.slot;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xF5A9U, 0xF5ACU, 222U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF5ACU, 0xF5ADU, 228U, checkpoint);

    bool saturation_carry = true;
    checkpoint.accumulator = update.score_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5ADU, 0xF5B0U, 243U, checkpoint);
    auto subtraction = subtract_result(
        checkpoint.accumulator,
        0x7FU,
        saturation_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        0x7FU,
        saturation_carry,
        subtraction);
    saturation_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5B0U, 0xF5B2U, 249U, checkpoint);

    checkpoint.accumulator = update.score_middle;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5B2U, 0xF5B5U, 264U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        0x96U,
        saturation_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        0x96U,
        saturation_carry,
        subtraction);
    saturation_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5B5U, 0xF5B7U, 270U, checkpoint);

    checkpoint.accumulator = update.score_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5B7U, 0xF5BAU, 285U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        0x98U,
        saturation_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        0x98U,
        saturation_carry,
        subtraction);
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5BAU, 0xF5BCU, 291U, checkpoint);
    append_checkpoint(0xF5BCU, 0xF5D5U, 303U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0xF5D5U, 0xF59DU, 324U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 2U);
    append_checkpoint(0xF59DU, 0xF5D6U, 345U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5D6U, 0xF5D9U, 360U, checkpoint);
    append_checkpoint(0xF5D9U, 0xF5DBU, 366U, checkpoint);

    const auto original_dirty_flags =
        std::to_integer<std::uint8_t>(work_ram[0x0CC0U]);
    checkpoint.accumulator = original_dirty_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5DBU, 0xF5DEU, 381U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(original_dirty_flags & 0x40U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5DEU, 0xF5E0U, 387U, checkpoint);
    append_checkpoint(0xF5E0U, 0xF5E2U, 393U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF5E2U, 0xF5E3U, 399U, checkpoint);

    bool comparison_carry = true;
    const auto comparison_low = static_cast<std::uint8_t>(
        update.comparison_score & 0xFFU);
    const auto comparison_middle = static_cast<std::uint8_t>(
        (update.comparison_score >> 8U) & 0xFFU);
    const auto comparison_high = static_cast<std::uint8_t>(
        (update.comparison_score >> 16U) & 0xFFU);

    checkpoint.accumulator = update.score_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5E3U, 0xF5E6U, 414U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        comparison_low,
        comparison_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        comparison_low,
        comparison_carry,
        subtraction);
    comparison_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5E6U, 0xF5E9U, 429U, checkpoint);

    checkpoint.accumulator = update.score_middle;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5E9U, 0xF5ECU, 444U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        comparison_middle,
        comparison_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        comparison_middle,
        comparison_carry,
        subtraction);
    comparison_carry = (checkpoint.status & 0x01U) != 0U;
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5ECU, 0xF5EFU, 459U, checkpoint);

    checkpoint.accumulator = update.score_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5EFU, 0xF5F2U, 474U, checkpoint);
    subtraction = subtract_result(
        checkpoint.accumulator,
        comparison_high,
        comparison_carry);
    checkpoint.status = sbc_status(
        checkpoint.status,
        checkpoint.accumulator,
        comparison_high,
        comparison_carry,
        subtraction);
    checkpoint.accumulator = subtraction;
    append_checkpoint(0xF5F2U, 0xF5F5U, 489U, checkpoint);
    append_checkpoint(0xF5F5U, 0xF604U, 501U, checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(0xF604U, 0xF5A0U, 522U, checkpoint);

    checkpoint.accumulator = original_dirty_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5A0U, 0xF5A3U, 537U, checkpoint);
    checkpoint.accumulator = update.dirty_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF5A3U, 0xF5A5U, 543U, checkpoint);
    append_checkpoint(0xF5A5U, 0xF5A8U, 558U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_table_interpreter(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x6966U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    const auto selector_address = static_cast<std::uint16_t>(
        0x6A38U
        + (static_cast<std::uint16_t>(
               entry.registers.accumulator)
           << 1U));
    const auto selector_offset = static_cast<std::size_t>(
        0x03U * 0x2000U + (selector_address & 0x1FFFU));
    if (work_ram.size() < 0x2000U
        || selector_offset + 1U >= rom.size()) {
        return std::nullopt;
    }
    const std::uint16_t table_pointer = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(rom[selector_offset])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(
                   rom[selector_offset + 1U]))
           << 8U));

    // The original ordinal-era proof matrix is kept temporarily as
    // migration archaeology, but is no longer compiled or consulted.
    // Semantic ROM/cursor resolution below owns these paths.
#if 0
    const bool front_spider_reverse_full_record =
        entry.registers.accumulator == 0x2BU;
    const bool alternate_reverse_full_record =
        entry.registers.accumulator == 0x06U;
    const bool reverse_full_record =
        front_spider_reverse_full_record
        || alternate_reverse_full_record;
    const bool first_continuation_full_record =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA6U])
            == 0x02U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A6U])
            == 0x80U;
    const bool second_continuation_full_record =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA8U])
            == 0x02U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A8U])
            == 0x80U;
    const bool alternate_continuation_full_record =
        entry.registers.accumulator == 0x06U
        && entry.registers.x == 0x17U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1E97U])
            == 0x02U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1897U])
            == 0x80U;
    const bool horizontal_continuation_full_record =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA6U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A6U])
            == 0x80U;
    const bool reverse_horizontal_continuation_full_record =
        entry.registers.accumulator == 0x2BU
        && entry.registers.x == 0x27U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA7U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A7U])
            == 0x80U;
    const bool later_vertical_continuation_full_record =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA8U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A8U])
            == 0x80U;
    const bool second_later_vertical_continuation_full_record =
        entry.registers.accumulator == 0x2DU
        && entry.registers.x == 0x29U
        && entry.registers.stack_pointer == 0xF9U
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(
               work_ram[0x1EA9U])
            == 0x03U
        && std::to_integer<std::uint8_t>(
               work_ram[0x18A9U])
            == 0x80U;
    const bool continuation_full_record =
        first_continuation_full_record
        || second_continuation_full_record
        || alternate_continuation_full_record
        || horizontal_continuation_full_record
        || reverse_horizontal_continuation_full_record
        || later_vertical_continuation_full_record
        || second_later_vertical_continuation_full_record;
    const bool first_vertical_full_record =
        entry.registers.accumulator == 0x2CU;
    const bool second_vertical_full_record =
        entry.registers.accumulator == 0x2DU;
    const bool vertical_full_record =
        first_vertical_full_record || second_vertical_full_record;
    const std::uint8_t table_entry =
        alternate_reverse_full_record
            ? 0x0BU
            : (vertical_full_record
                   ? 0x05U
                   : (front_spider_reverse_full_record
                          ? 0x07U
                          : 0x03U));
    const std::uint16_t motion_pointer =
        alternate_reverse_full_record
            ? 0x6DE6U
            : (vertical_full_record
                   ? 0x6D7AU
                   : (front_spider_reverse_full_record
                          ? 0x6D9EU
                          : 0x6D56U));
    const std::uint8_t motion_header =
        continuation_full_record
            ? (alternate_continuation_full_record
                   ? 0x10U
                   : (horizontal_continuation_full_record
                          ? 0x20U
                          : (reverse_horizontal_continuation_full_record
                                 ? 0x40U
                                 : 0x08U)))
            : (alternate_reverse_full_record
            ? 0x8AU
            : (vertical_full_record
                   ? 0x84U
                   : (front_spider_reverse_full_record
                          ? 0x86U
                          : 0x82U)));
    const std::uint8_t motion_delta =
        continuation_full_record
            ? 0x00U
            : (vertical_full_record
            ? 0x08U
            : (reverse_full_record ? 0x40U : 0x20U));

#endif
    dc_collision_table_pass pass{};
    if (!dc_select_collision_table_pass(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            table_pointer,
            &pass)) {
        throw std::runtime_error(
            "Native 6966 replacement rejected the work-RAM snapshot.");
    }

    // Resolve the record path from ROM and the two per-slot cursors. The
    // semantic walker below owns control markers and headered records; the
    // one-step path that follows owns headerless records. Neither is tied to
    // a selector, actor slot, caller, or dynamic call ordinal.
    const auto read_bank_03 =
        [&rom](const std::uint16_t logical_address)
        -> std::optional<std::uint8_t> {
        if (logical_address < 0x6000U
            || logical_address > 0x7FFFU) {
            return std::nullopt;
        }
        const auto offset = static_cast<std::size_t>(
            0x03U * 0x2000U + (logical_address & 0x1FFFU));
        if (offset >= rom.size()) {
            return std::nullopt;
        }
        return std::to_integer<std::uint8_t>(rom[offset]);
    };

    // Follow control markers until the walker either terminates at $FF or
    // reaches a headered motion record. Ordinary one-step headerless records
    // continue through the simpler path below.
    const bool walker_active_pass =
        pass.collision_state == 0x80U
        && pass.collision_state_next == 0x81U
        && pass.pass_index == 1U
        && pass.returns_without_step == 0U
        && (entry.registers.status & 0x20U) == 0U;
    auto walker_collision_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E00U + entry.registers.x]);
    auto walker_motion_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E80U + entry.registers.x]);
    std::uint32_t walker_delay = 0U;
    std::uint8_t walker_table_entry = 0U;
    std::uint16_t walker_motion_pointer = 0U;
    std::uint8_t walker_motion_header = 0U;
    std::uint8_t walker_motion_delta = 0U;
    bool walker_valid = walker_active_pass;
    bool walker_record_ready = false;
    bool walker_terminates = false;
    std::vector<ReplacementMemoryWrite> walker_writes;
    walker_writes.reserve(24U);
    if (walker_active_pass) {
        constexpr std::size_t kMaximumWalkerIterations = 16U;
        for (std::size_t iteration = 0U;
             iteration < kMaximumWalkerIterations;
             ++iteration) {
            walker_writes.push_back({
                .instruction = {0x03U, 0x6995U},
                .logical_address = 0x2035U,
                .value = 0U,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        177U + walker_delay),
            });
            auto walker_entry = read_bank_03(
                static_cast<std::uint16_t>(
                    table_pointer + walker_collision_cursor));
            if (!walker_entry.has_value()) {
                walker_valid = false;
                break;
            }
            if (*walker_entry == 0xFFU) {
                walker_terminates = true;
                break;
            }
            if (*walker_entry == 0xFEU) {
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69A6U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E00U + entry.registers.x),
                    .value = 0U,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            258U + walker_delay),
                });
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69A9U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E80U + entry.registers.x),
                    .value = 0U,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            273U + walker_delay),
                });
                walker_collision_cursor = 0U;
                walker_motion_cursor = 0U;
                walker_delay =
                    static_cast<std::uint32_t>(walker_delay + 45U);
                walker_entry = read_bank_03(table_pointer);
                if (!walker_entry.has_value()
                    || *walker_entry >= 0xFEU) {
                    walker_valid = false;
                    break;
                }
            }
            // Reverse-oriented table entries execute SMB0 $35, a separate
            // read-modify-write timing class not present on this route.
            if ((*walker_entry & 0x80U) != 0U) {
                walker_valid = false;
                break;
            }
            walker_table_entry = *walker_entry;
            const auto motion_selector =
                static_cast<std::uint16_t>(
                    0x6C3AU
                    + (static_cast<std::uint16_t>(
                           walker_table_entry & 0x7FU)
                       << 1U));
            const auto motion_pointer_low =
                read_bank_03(motion_selector);
            const auto motion_pointer_high = read_bank_03(
                static_cast<std::uint16_t>(
                    motion_selector + 1U));
            if (!motion_pointer_low.has_value()
                || !motion_pointer_high.has_value()) {
                walker_valid = false;
                break;
            }
            walker_motion_pointer =
                static_cast<std::uint16_t>(
                    *motion_pointer_low
                    | (static_cast<std::uint16_t>(
                           *motion_pointer_high)
                       << 8U));
            walker_writes.push_back({
                .instruction = {0x03U, 0x69BBU},
                .logical_address = 0x202DU,
                .value = static_cast<std::uint8_t>(
                    walker_motion_pointer & 0x00FFU),
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        306U + walker_delay),
            });
            walker_writes.push_back({
                .instruction = {0x03U, 0x69C0U},
                .logical_address = 0x202EU,
                .value = static_cast<std::uint8_t>(
                    walker_motion_pointer >> 8U),
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        333U + walker_delay),
            });
            const auto walker_header = read_bank_03(
                static_cast<std::uint16_t>(
                    walker_motion_pointer + walker_motion_cursor));
            if (!walker_header.has_value()) {
                walker_valid = false;
                break;
            }
            if (*walker_header == 0xC0U) {
                walker_collision_cursor =
                    static_cast<std::uint8_t>(
                        walker_collision_cursor + 1U);
                walker_motion_cursor = 0U;
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69CBU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E00U + entry.registers.x),
                    .value = walker_collision_cursor,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            402U + walker_delay),
                });
                walker_writes.push_back({
                    .instruction = {0x03U, 0x69CEU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x3E80U + entry.registers.x),
                    .value = 0U,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            417U + walker_delay),
                });
                walker_delay =
                    static_cast<std::uint32_t>(
                        walker_delay + 264U);
                continue;
            }
            if ((*walker_header & 0x80U) == 0U) {
                walker_valid = false;
                break;
            }
            walker_motion_header = *walker_header;
            const auto walker_delta = read_bank_03(
                static_cast<std::uint16_t>(
                    walker_motion_pointer
                    + walker_motion_cursor + 1U));
            if (!walker_delta.has_value()) {
                walker_valid = false;
                break;
            }
            walker_motion_delta = *walker_delta;
            walker_record_ready = true;
            break;
        }
        if (!walker_record_ready && !walker_terminates) {
            walker_valid = false;
        }
    }

    const auto walker_pushed_status =
        static_cast<std::uint8_t>(
            (entry.registers.status & 0x4DU) | 0x10U);
    const auto append_walker_entry_writes =
        [&entry, &pass, walker_pushed_status](
            std::vector<ReplacementMemoryWrite>& writes) {
        writes.push_back({
            .instruction = {0x03U, 0x696BU},
            .logical_address = 0x202FU,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer & 0x00FFU),
            .master_cycle_offset = 39U,
        });
        writes.push_back({
            .instruction = {0x03U, 0x6970U},
            .logical_address = 0x2030U,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer >> 8U),
            .master_cycle_offset = 66U,
        });
        writes.push_back({
            .instruction = {0x03U, 0x697CU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = walker_pushed_status,
            .master_cycle_offset = 114U,
        });
        writes.push_back({
            .instruction = {0x03U, 0x697FU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + pass.collision_work_ram_offset),
            .value = pass.collision_state_next,
            .master_cycle_offset = 135U,
        });
    };
    if (walker_valid && walker_terminates) {
        auto output = entry.registers;
        output.program_counter = 0x69A1U;
        output.accumulator = 0xFFU;
        output.y = walker_collision_cursor;
        output.status = static_cast<std::uint8_t>(
            (walker_pushed_status & 0x5CU) | 0x03U);

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x69A1U,
            },
            .output_registers = output,
            .body_master_cycles =
                static_cast<std::uint32_t>(
                    231U + walker_delay),
        };
        replacement.work_ram_writes.reserve(
            4U + walker_writes.size());
        append_walker_entry_writes(
            replacement.work_ram_writes);
        replacement.work_ram_writes.insert(
            replacement.work_ram_writes.end(),
            walker_writes.begin(),
            walker_writes.end());
        return replacement;
    }
    if (walker_valid && walker_record_ready) {
        dc_collision_table_record walker_record{};
        if (!dc_advance_collision_table_record(
                reinterpret_cast<const std::uint8_t*>(
                    work_ram.data()),
                work_ram.size(),
                entry.registers.x,
                table_pointer,
                walker_table_entry,
                walker_motion_pointer,
                walker_motion_header,
                walker_motion_delta,
                &walker_record)) {
            throw std::runtime_error(
                "Native 6966 walker rejected a resolved record.");
        }
        walker_record.cursor_after_header =
            static_cast<std::uint8_t>(
                walker_motion_cursor + 1U);
        walker_record.cursor_after_delta =
            static_cast<std::uint8_t>(
                walker_record.cursor_after_header + 1U);

        const auto horizontal_direction =
            static_cast<std::uint8_t>(
                walker_motion_delta & 0x60U);
        const auto vertical_direction =
            static_cast<std::uint8_t>(
                walker_motion_delta & 0x18U);
        std::uint32_t horizontal_delay = 0U;
        if (horizontal_direction == 0x20U) {
            horizontal_delay =
                51U
                + (walker_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (horizontal_direction == 0x40U) {
            horizontal_delay =
                60U
                + (walker_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        }
        std::uint32_t vertical_delay = 0U;
        if (vertical_direction == 0x08U) {
            vertical_delay =
                51U
                + (walker_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (vertical_direction == 0x10U) {
            vertical_delay =
                60U
                + (walker_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        }
        const auto record_delay =
            static_cast<std::uint32_t>(walker_delay + 123U);
        const auto cursor_write_cycle =
            static_cast<std::uint32_t>(
                483U + record_delay
                + horizontal_delay + vertical_delay);

        auto output = entry.registers;
        output.program_counter = 0x6A37U;
        output.accumulator =
            vertical_direction == 0x08U
                ? 0x08U
                : (vertical_direction == 0x10U
                       ? std::to_integer<std::uint8_t>(
                             work_ram[
                                 walker_record
                                     .vertical_low_work_ram_offset])
                       : 0U);
        output.y = walker_record.cursor_after_header;
        output.status = static_cast<std::uint8_t>(
            walker_pushed_status & 0x5CU);
        if (walker_record.cursor_after_delta == 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x02U);
        } else if (
            (walker_record.cursor_after_delta & 0x80U) != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x80U);
        }

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x6A37U,
            },
            .output_registers = output,
            .body_master_cycles = cursor_write_cycle + 6U,
        };
        replacement.work_ram_writes.reserve(
            16U + walker_writes.size());
        append_walker_entry_writes(
            replacement.work_ram_writes);
        replacement.work_ram_writes.insert(
            replacement.work_ram_writes.end(),
            walker_writes.begin(),
            walker_writes.end());
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x69D9U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + walker_record.event_work_ram_offset),
            .value = walker_record.event,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    420U + walker_delay),
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x69EEU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U
                + walker_record.orientation_work_ram_offset),
            .value = walker_record.orientation,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    480U + walker_delay),
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x69F1U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + walker_record.cursor_work_ram_offset),
            .value = walker_record.cursor_after_header,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    501U + walker_delay),
        });
        if (horizontal_direction == 0x20U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x69FFU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record
                          .horizontal_low_work_ram_offset),
                .value = walker_record.horizontal_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        450U + record_delay),
            });
            if (walker_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A04U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .horizontal_high_work_ram_offset),
                    .value = walker_record.horizontal_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            477U + record_delay),
                });
            }
        } else if (horizontal_direction == 0x40U) {
            if (walker_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A0EU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .horizontal_high_work_ram_offset),
                    .value = walker_record.horizontal_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            477U + record_delay),
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A11U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record
                          .horizontal_low_work_ram_offset),
                .value = walker_record.horizontal_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        (walker_record.horizontal_high_written != 0U
                             ? 498U
                             : 483U)
                        + record_delay),
            });
        }
        if (vertical_direction == 0x08U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A1EU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record.vertical_low_work_ram_offset),
                .value = walker_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        489U + record_delay
                        + horizontal_delay),
            });
            if (walker_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A23U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .vertical_high_work_ram_offset),
                    .value = walker_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + record_delay
                            + horizontal_delay),
                });
            }
        } else if (vertical_direction == 0x10U) {
            if (walker_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A2DU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + walker_record
                              .vertical_high_work_ram_offset),
                    .value = walker_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + record_delay
                            + horizontal_delay),
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A30U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + walker_record.vertical_low_work_ram_offset),
                .value = walker_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        (walker_record.vertical_high_written != 0U
                             ? 537U
                             : 522U)
                        + record_delay + horizontal_delay),
            });
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6A33U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + walker_record.cursor_work_ram_offset),
            .value = walker_record.cursor_after_delta,
            .master_cycle_offset = cursor_write_cycle,
        });
        return replacement;
    }

    const auto collision_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E00U + entry.registers.x]);
    const auto resolved_table_entry = read_bank_03(
        static_cast<std::uint16_t>(
            table_pointer + collision_cursor));
    std::optional<std::uint16_t> resolved_motion_pointer;
    std::optional<std::uint8_t> resolved_motion_byte;
    const auto motion_cursor =
        std::to_integer<std::uint8_t>(
            work_ram[0x1E80U + entry.registers.x]);
    if (resolved_table_entry.has_value()
        && *resolved_table_entry < 0xFEU) {
        const auto motion_selector = static_cast<std::uint16_t>(
            0x6C3AU
            + (static_cast<std::uint16_t>(
                   *resolved_table_entry & 0x7FU)
               << 1U));
        const auto motion_pointer_low =
            read_bank_03(motion_selector);
        const auto motion_pointer_high = read_bank_03(
            static_cast<std::uint16_t>(motion_selector + 1U));
        if (motion_pointer_low.has_value()
            && motion_pointer_high.has_value()) {
            resolved_motion_pointer = static_cast<std::uint16_t>(
                *motion_pointer_low
                | (static_cast<std::uint16_t>(
                       *motion_pointer_high)
                   << 8U));
            resolved_motion_byte = read_bank_03(
                static_cast<std::uint16_t>(
                    *resolved_motion_pointer + motion_cursor));
        }
    }

    dc_collision_table_record semantic_record{};
    const bool semantic_headerless_step =
        pass.collision_state == 0x80U
        && pass.collision_state_next == 0x81U
        && pass.pass_index == 1U
        && pass.returns_without_step == 0U
        && (entry.registers.status & 0x20U) == 0U
        && resolved_table_entry.has_value()
        && resolved_motion_pointer.has_value()
        && resolved_motion_byte.has_value()
        && (*resolved_motion_byte & 0x80U) == 0U
        && *resolved_motion_byte != 0xC0U
        && dc_advance_collision_table_record(
               reinterpret_cast<const std::uint8_t*>(
                   work_ram.data()),
               work_ram.size(),
               entry.registers.x,
               table_pointer,
               *resolved_table_entry,
               *resolved_motion_pointer,
               *resolved_motion_byte,
               0U,
               &semantic_record) != 0;
    if (semantic_headerless_step) {
        const auto horizontal_direction =
            static_cast<std::uint8_t>(
                *resolved_motion_byte & 0x60U);
        const auto vertical_direction =
            static_cast<std::uint8_t>(
                *resolved_motion_byte & 0x18U);
        std::uint32_t horizontal_delay = 0U;
        if (horizontal_direction == 0x20U) {
            horizontal_delay =
                51U
                + (semantic_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (horizontal_direction == 0x40U) {
            horizontal_delay =
                60U
                + (semantic_record.horizontal_high_written != 0U
                       ? 15U
                       : 0U);
        }
        std::uint32_t vertical_delay = 0U;
        if (vertical_direction == 0x08U) {
            vertical_delay =
                51U
                + (semantic_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        } else if (vertical_direction == 0x10U) {
            vertical_delay =
                60U
                + (semantic_record.vertical_high_written != 0U
                       ? 15U
                       : 0U);
        }
        const auto cursor_write_cycle = static_cast<std::uint32_t>(
            483U + horizontal_delay + vertical_delay);
        const auto pushed_status = static_cast<std::uint8_t>(
            (entry.registers.status & 0x4DU) | 0x10U);

        auto output = entry.registers;
        output.program_counter = 0x6A37U;
        output.accumulator =
            vertical_direction == 0x08U
                ? 0x08U
                : (vertical_direction == 0x10U
                       ? std::to_integer<std::uint8_t>(
                             work_ram[
                                 semantic_record
                                     .vertical_low_work_ram_offset])
                       : 0U);
        output.y = motion_cursor;
        output.status = static_cast<std::uint8_t>(
            pushed_status & 0x5CU);
        if (semantic_record.cursor_after_delta == 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x02U);
        } else if (
            (semantic_record.cursor_after_delta & 0x80U) != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x80U);
        }

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x6A37U,
            },
            .output_registers = output,
            .body_master_cycles = cursor_write_cycle + 6U,
        };
        replacement.work_ram_writes.reserve(12U);
        replacement.work_ram_writes = {
            {
                .instruction = {0x03U, 0x696BU},
                .logical_address = 0x202FU,
                .value = static_cast<std::uint8_t>(
                    pass.table_pointer & 0x00FFU),
                .master_cycle_offset = 39U,
            },
            {
                .instruction = {0x03U, 0x6970U},
                .logical_address = 0x2030U,
                .value = static_cast<std::uint8_t>(
                    pass.table_pointer >> 8U),
                .master_cycle_offset = 66U,
            },
            {
                .instruction = {0x03U, 0x697CU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + entry.registers.stack_pointer),
                .value = pushed_status,
                .master_cycle_offset = 114U,
            },
            {
                .instruction = {0x03U, 0x697FU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + pass.collision_work_ram_offset),
                .value = pass.collision_state_next,
                .master_cycle_offset = 135U,
            },
            {
                .instruction = {0x03U, 0x6995U},
                .logical_address = 0x2035U,
                .value = 0U,
                .master_cycle_offset = 177U,
            },
            {
                .instruction = {0x03U, 0x69BBU},
                .logical_address = 0x202DU,
                .value = static_cast<std::uint8_t>(
                    semantic_record.motion_pointer & 0x00FFU),
                .master_cycle_offset = 306U,
            },
            {
                .instruction = {0x03U, 0x69C0U},
                .logical_address = 0x202EU,
                .value = static_cast<std::uint8_t>(
                    semantic_record.motion_pointer >> 8U),
                .master_cycle_offset = 333U,
            },
        };
        if (horizontal_direction == 0x20U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x69FFU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record
                          .horizontal_low_work_ram_offset),
                .value = semantic_record.horizontal_low,
                .master_cycle_offset = 450U,
            });
            if (semantic_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A04U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .horizontal_high_work_ram_offset),
                    .value = semantic_record.horizontal_high,
                    .master_cycle_offset = 477U,
                });
            }
        } else if (horizontal_direction == 0x40U) {
            if (semantic_record.horizontal_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A0EU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .horizontal_high_work_ram_offset),
                    .value = semantic_record.horizontal_high,
                    .master_cycle_offset = 477U,
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A11U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record
                          .horizontal_low_work_ram_offset),
                .value = semantic_record.horizontal_low,
                .master_cycle_offset =
                    semantic_record.horizontal_high_written != 0U
                        ? 498U
                        : 483U,
            });
        }
        if (vertical_direction == 0x08U) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A1EU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record.vertical_low_work_ram_offset),
                .value = semantic_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        489U + horizontal_delay),
            });
            if (semantic_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A23U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .vertical_high_work_ram_offset),
                    .value = semantic_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + horizontal_delay),
                });
            }
        } else if (vertical_direction == 0x10U) {
            if (semantic_record.vertical_high_written != 0U) {
                replacement.work_ram_writes.push_back({
                    .instruction = {0x03U, 0x6A2DU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + semantic_record
                              .vertical_high_work_ram_offset),
                    .value = semantic_record.vertical_high,
                    .master_cycle_offset =
                        static_cast<std::uint32_t>(
                            516U + horizontal_delay),
                });
            }
            replacement.work_ram_writes.push_back({
                .instruction = {0x03U, 0x6A30U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + semantic_record.vertical_low_work_ram_offset),
                .value = semantic_record.vertical_low,
                .master_cycle_offset =
                    static_cast<std::uint32_t>(
                        (semantic_record.vertical_high_written != 0U
                             ? 537U
                             : 522U)
                        + horizontal_delay),
            });
        }
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x6A33U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + semantic_record.cursor_work_ram_offset),
            .value = semantic_record.cursor_after_delta,
            .master_cycle_offset = cursor_write_cycle,
        });

        const bool checkpointable_bounded_axes =
            (*resolved_table_entry & 0x80U) == 0U
            && (horizontal_direction != 0U
                || vertical_direction != 0U)
            && (horizontal_direction == 0U
                || horizontal_direction == 0x20U
                || horizontal_direction == 0x40U)
            && (vertical_direction == 0U
                || vertical_direction == 0x08U
                || vertical_direction == 0x10U)
            && semantic_record.horizontal_high_written == 0U
            && semantic_record.vertical_high_written == 0U;
        if (checkpointable_bounded_axes) {
            const auto set_nz =
                [](std::uint8_t status, const std::uint8_t value) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0xA2U));
                    if (value == 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x02U);
                    } else if ((value & 0x80U) != 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x80U);
                    }
                    return status;
                };
            const auto compare_status =
                [&set_nz](
                    std::uint8_t status,
                    const std::uint8_t left,
                    const std::uint8_t right) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0x01U));
                    status = set_nz(
                        status,
                        static_cast<std::uint8_t>(
                            left - right));
                    if (left >= right) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x01U);
                    }
                    return status;
                };
            const auto bit_status =
                [](
                    std::uint8_t status,
                    const std::uint8_t accumulator,
                    const std::uint8_t operand) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0xE2U));
                    if ((accumulator & operand) == 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x02U);
                    }
                    status = static_cast<std::uint8_t>(
                        status | (operand & 0xC0U));
                    return status;
                };
            const auto shift_left_status =
                [&set_nz](
                    std::uint8_t status,
                    const std::uint8_t original,
                    const std::uint8_t result) {
                    status = static_cast<std::uint8_t>(
                        status
                        & static_cast<std::uint8_t>(~0x01U));
                    status = set_nz(status, result);
                    if ((original & 0x80U) != 0U) {
                        status =
                            static_cast<std::uint8_t>(
                                status | 0x01U);
                    }
                    return status;
                };
            const auto append_checkpoint =
                [&replacement](
                    const std::uint16_t instruction_pc,
                    const std::uint16_t resume_pc,
                    const std::uint32_t master_cycle,
                    auto registers) {
                    registers.program_counter = resume_pc;
                    replacement.checkpoints.push_back({
                        .interrupt_sample_master_cycle_offset =
                            master_cycle - 3U,
                        .master_cycle_offset = master_cycle,
                        .instruction = {0x03U, instruction_pc},
                        .registers = registers,
                    });
                };

            auto checkpoint = entry.registers;
            const auto doubled_selector =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator << 1U);
            checkpoint.status = shift_left_status(
                checkpoint.status,
                checkpoint.accumulator,
                doubled_selector);
            checkpoint.accumulator = doubled_selector;
            append_checkpoint(
                0x6966U,
                0x6967U,
                6U,
                checkpoint);

            checkpoint.y = checkpoint.accumulator;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x6967U,
                0x6968U,
                12U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    pass.table_pointer & 0x00FFU);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x6968U,
                0x696BU,
                27U,
                checkpoint);
            append_checkpoint(
                0x696BU,
                0x696DU,
                39U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    pass.table_pointer >> 8U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x696DU,
                0x6970U,
                54U,
                checkpoint);
            append_checkpoint(
                0x6970U,
                0x6972U,
                66U,
                checkpoint);

            checkpoint.y = pass.collision_state;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x6972U,
                0x6975U,
                81U,
                checkpoint);
            append_checkpoint(
                0x6975U,
                0x6977U,
                87U,
                checkpoint);

            checkpoint.accumulator = checkpoint.y;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x6977U,
                0x6978U,
                93U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x7FU);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x6978U,
                0x697AU,
                99U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator ^ 0x01U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x697AU,
                0x697CU,
                105U,
                checkpoint);

            checkpoint.stack_pointer =
                static_cast<std::uint8_t>(
                    checkpoint.stack_pointer - 1U);
            append_checkpoint(
                0x697CU,
                0x697DU,
                114U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator | 0x80U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x697DU,
                0x697FU,
                120U,
                checkpoint);
            append_checkpoint(
                0x697FU,
                0x6982U,
                135U,
                checkpoint);

            checkpoint.stack_pointer =
                static_cast<std::uint8_t>(
                    checkpoint.stack_pointer + 1U);
            checkpoint.status = pushed_status;
            append_checkpoint(
                0x6982U,
                0x6983U,
                147U,
                checkpoint);
            append_checkpoint(
                0x6983U,
                0x6985U,
                153U,
                checkpoint);
            append_checkpoint(
                0x6985U,
                0x6995U,
                165U,
                checkpoint);
            append_checkpoint(
                0x6995U,
                0x6997U,
                177U,
                checkpoint);

            checkpoint.y = collision_cursor;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x6997U,
                0x699AU,
                192U,
                checkpoint);

            checkpoint.accumulator = *resolved_table_entry;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x699AU,
                0x699CU,
                213U,
                checkpoint);

            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0xFFU);
            append_checkpoint(
                0x699CU,
                0x699EU,
                219U,
                checkpoint);
            append_checkpoint(
                0x699EU,
                0x69A2U,
                231U,
                checkpoint);

            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0xFEU);
            append_checkpoint(
                0x69A2U,
                0x69A4U,
                237U,
                checkpoint);
            append_checkpoint(
                0x69A4U,
                0x69AEU,
                249U,
                checkpoint);

            checkpoint.status = bit_status(
                checkpoint.status,
                checkpoint.accumulator,
                0x80U);
            append_checkpoint(
                0x69AEU,
                0x69B0U,
                255U,
                checkpoint);
            append_checkpoint(
                0x69B0U,
                0x69B6U,
                267U,
                checkpoint);

            const auto doubled_table_entry =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator << 1U);
            checkpoint.status = shift_left_status(
                checkpoint.status,
                checkpoint.accumulator,
                doubled_table_entry);
            checkpoint.accumulator = doubled_table_entry;
            append_checkpoint(
                0x69B6U,
                0x69B7U,
                273U,
                checkpoint);

            checkpoint.y = checkpoint.accumulator;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x69B7U,
                0x69B8U,
                279U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    semantic_record.motion_pointer & 0x00FFU);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69B8U,
                0x69BBU,
                294U,
                checkpoint);
            append_checkpoint(
                0x69BBU,
                0x69BDU,
                306U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    semantic_record.motion_pointer >> 8U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69BDU,
                0x69C0U,
                321U,
                checkpoint);
            append_checkpoint(
                0x69C0U,
                0x69C2U,
                333U,
                checkpoint);

            checkpoint.y = motion_cursor;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.y);
            append_checkpoint(
                0x69C2U,
                0x69C5U,
                348U,
                checkpoint);

            checkpoint.accumulator = *resolved_motion_byte;
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69C5U,
                0x69C7U,
                369U,
                checkpoint);
            checkpoint.status = compare_status(
                checkpoint.status,
                checkpoint.accumulator,
                0xC0U);
            append_checkpoint(
                0x69C7U,
                0x69C9U,
                375U,
                checkpoint);
            append_checkpoint(
                0x69C9U,
                0x69D3U,
                387U,
                checkpoint);
            checkpoint.status = bit_status(
                checkpoint.status,
                checkpoint.accumulator,
                0x80U);
            append_checkpoint(
                0x69D3U,
                0x69D5U,
                393U,
                checkpoint);
            append_checkpoint(
                0x69D5U,
                0x69F7U,
                405U,
                checkpoint);

            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x60U);
            checkpoint.status =
                set_nz(checkpoint.status, checkpoint.accumulator);
            append_checkpoint(
                0x69F7U,
                0x69F9U,
                411U,
                checkpoint);

            std::uint32_t vertical_load_cycle = 444U;
            if (horizontal_direction != 0U) {
                append_checkpoint(
                    0x69F9U,
                    0x69FBU,
                    417U,
                    checkpoint);
                checkpoint.status = compare_status(
                    checkpoint.status,
                    checkpoint.accumulator,
                    0x20U);
                append_checkpoint(
                    0x69FBU,
                    0x69FDU,
                    423U,
                    checkpoint);

                if (horizontal_direction == 0x20U) {
                    append_checkpoint(
                        0x69FDU,
                        0x69FFU,
                        429U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.horizontal_low);
                    append_checkpoint(
                        0x69FFU,
                        0x6A02U,
                        450U,
                        checkpoint);
                    append_checkpoint(
                        0x6A02U,
                        0x6A07U,
                        462U,
                        checkpoint);
                    append_checkpoint(
                        0x6A07U,
                        0x6A14U,
                        474U,
                        checkpoint);
                } else {
                    append_checkpoint(
                        0x69FDU,
                        0x6A09U,
                        435U,
                        checkpoint);
                    checkpoint.accumulator =
                        std::to_integer<std::uint8_t>(
                            work_ram[
                                semantic_record
                                    .horizontal_low_work_ram_offset]);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    append_checkpoint(
                        0x6A09U,
                        0x6A0CU,
                        450U,
                        checkpoint);
                    append_checkpoint(
                        0x6A0CU,
                        0x6A11U,
                        462U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.horizontal_low);
                    append_checkpoint(
                        0x6A11U,
                        0x6A14U,
                        483U,
                        checkpoint);
                }
                vertical_load_cycle =
                    horizontal_direction == 0x20U
                        ? 495U
                        : 504U;
            } else {
                append_checkpoint(
                    0x69F9U,
                    0x6A14U,
                    423U,
                    checkpoint);
            }

            checkpoint.accumulator = *resolved_motion_byte;
            checkpoint.status = set_nz(
                checkpoint.status,
                checkpoint.accumulator);
            append_checkpoint(
                0x6A14U,
                0x6A16U,
                vertical_load_cycle,
                checkpoint);
            checkpoint.accumulator =
                static_cast<std::uint8_t>(
                    checkpoint.accumulator & 0x18U);
            checkpoint.status = set_nz(
                checkpoint.status,
                checkpoint.accumulator);
            append_checkpoint(
                0x6A16U,
                0x6A18U,
                vertical_load_cycle + 6U,
                checkpoint);

            if (vertical_direction == 0U) {
                append_checkpoint(
                    0x6A18U,
                    0x6A33U,
                    vertical_load_cycle + 18U,
                    checkpoint);
            } else {
                append_checkpoint(
                    0x6A18U,
                    0x6A1AU,
                    vertical_load_cycle + 12U,
                    checkpoint);
                checkpoint.status = compare_status(
                    checkpoint.status,
                    checkpoint.accumulator,
                    0x08U);
                append_checkpoint(
                    0x6A1AU,
                    0x6A1CU,
                    vertical_load_cycle + 18U,
                    checkpoint);

                if (vertical_direction == 0x08U) {
                    append_checkpoint(
                        0x6A1CU,
                        0x6A1EU,
                        vertical_load_cycle + 24U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.vertical_low);
                    append_checkpoint(
                        0x6A1EU,
                        0x6A21U,
                        vertical_load_cycle + 45U,
                        checkpoint);
                    append_checkpoint(
                        0x6A21U,
                        0x6A26U,
                        vertical_load_cycle + 57U,
                        checkpoint);
                    append_checkpoint(
                        0x6A26U,
                        0x6A33U,
                        vertical_load_cycle + 69U,
                        checkpoint);
                } else {
                    append_checkpoint(
                        0x6A1CU,
                        0x6A28U,
                        vertical_load_cycle + 30U,
                        checkpoint);
                    checkpoint.accumulator =
                        std::to_integer<std::uint8_t>(
                            work_ram[
                                semantic_record
                                    .vertical_low_work_ram_offset]);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        checkpoint.accumulator);
                    append_checkpoint(
                        0x6A28U,
                        0x6A2BU,
                        vertical_load_cycle + 45U,
                        checkpoint);
                    append_checkpoint(
                        0x6A2BU,
                        0x6A30U,
                        vertical_load_cycle + 57U,
                        checkpoint);
                    checkpoint.status = set_nz(
                        checkpoint.status,
                        semantic_record.vertical_low);
                    append_checkpoint(
                        0x6A30U,
                        0x6A33U,
                        vertical_load_cycle + 78U,
                        checkpoint);
                }
            }

            checkpoint.status = set_nz(
                checkpoint.status,
                semantic_record.cursor_after_delta);
            append_checkpoint(
                0x6A33U,
                0x6A36U,
                cursor_write_cycle,
                checkpoint);
            checkpoint.status = static_cast<std::uint8_t>(
                checkpoint.status
                & static_cast<std::uint8_t>(~0x01U));
            append_checkpoint(
                0x6A36U,
                0x6A37U,
                cursor_write_cycle + 6U,
                output);
        }
        return replacement;
    }

#if 0
    dc_collision_table_record record{};
    const bool has_full_record = dc_advance_collision_table_record(
        reinterpret_cast<const std::uint8_t*>(work_ram.data()),
        work_ram.size(),
        entry.registers.x,
        table_pointer,
        table_entry,
        motion_pointer,
        motion_header,
        motion_delta,
        &record) != 0;
    const bool proven_forward_full_record_path =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.y == 0U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x02U
        && has_full_record
        && work_ram.size() >= 0x2000U
        && std::to_integer<std::uint8_t>(work_ram[0x1E26U]) == 0U
        && record.pass.collision_work_ram_offset == 0x18A6U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2AU
        && record.motion_pointer == 0x6D56U
        && record.event_work_ram_offset == 0x1AA6U
        && record.orientation_work_ram_offset == 0x1D26U
        && record.cursor_work_ram_offset == 0x1EA6U
        && record.horizontal_low_work_ram_offset == 0x11A6U
        && record.table_entry == 0x03U
        && record.motion_header == 0x82U
        && record.motion_delta == 0x20U
        && record.event == 0x02U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.horizontal_low == 0x41U
        && record.horizontal_high == 0U
        && record.event_written != 0U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_reverse_full_record_path =
        entry.registers.accumulator == 0x2BU
        && entry.registers.x == 0x27U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x10U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A7U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2DU
        && record.motion_pointer == 0x6D9EU
        && record.event_work_ram_offset == 0x1AA7U
        && record.orientation_work_ram_offset == 0x1D27U
        && record.cursor_work_ram_offset == 0x1EA7U
        && record.horizontal_low_work_ram_offset == 0x11A7U
        && record.table_entry == 0x07U
        && record.motion_header == 0x86U
        && record.motion_delta == 0x40U
        && record.event == 0x06U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.horizontal_low == 0xD7U
        && record.event_written != 0U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_vertical_full_record_path =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.y == 0U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x12U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A8U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C30U
        && record.motion_pointer == 0x6D7AU
        && record.event_work_ram_offset == 0x1AA8U
        && record.orientation_work_ram_offset == 0x1D28U
        && record.cursor_work_ram_offset == 0x1EA8U
        && record.vertical_low_work_ram_offset == 0x1328U
        && record.table_entry == 0x05U
        && record.motion_header == 0x84U
        && record.motion_delta == 0x08U
        && record.event == 0x04U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.vertical_low == 0x83U
        && record.event_written != 0U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_second_vertical_full_record_path =
        entry.registers.accumulator == 0x2DU
        && entry.registers.x == 0x29U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF5U
        && entry.registers.status == 0x10U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A9U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C35U
        && record.motion_pointer == 0x6D7AU
        && record.event_work_ram_offset == 0x1AA9U
        && record.orientation_work_ram_offset == 0x1D29U
        && record.cursor_work_ram_offset == 0x1EA9U
        && record.vertical_low_work_ram_offset == 0x1329U
        && record.table_entry == 0x05U
        && record.motion_header == 0x84U
        && record.motion_delta == 0x08U
        && record.event == 0x04U
        && record.orientation == 0x8CU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.vertical_low == 0x83U
        && record.event_written != 0U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_alternate_reverse_full_record_path =
        entry.registers.accumulator == 0x06U
        && entry.registers.x == 0x17U
        && entry.registers.y == 0x75U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x1897U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6B0AU
        && record.motion_pointer == 0x6DE6U
        && record.event_work_ram_offset == 0x1A97U
        && record.orientation_work_ram_offset == 0x1D17U
        && record.cursor_work_ram_offset == 0x1E97U
        && record.horizontal_low_work_ram_offset == 0x1197U
        && record.table_entry == 0x0BU
        && record.motion_header == 0x8AU
        && record.motion_delta == 0x40U
        && record.event == 0x0AU
        && record.orientation == 0x8DU
        && record.cursor_after_header == 1U
        && record.cursor_after_delta == 2U
        && record.horizontal_low == 0xF5U
        && record.event_written != 0U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_continuation_full_record_path =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x02U
        && continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A6U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2AU
        && record.motion_pointer == 0x6D56U
        && record.cursor_work_ram_offset == 0x1EA6U
        && record.vertical_low_work_ram_offset == 0x1326U
        && record.table_entry == 0x03U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 2U
        && record.cursor_after_delta == 3U
        && record.vertical_low == 0x7DU
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_second_continuation_full_record_path =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x02U
        && second_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A8U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C30U
        && record.motion_pointer == 0x6D7AU
        && record.cursor_work_ram_offset == 0x1EA8U
        && record.vertical_low_work_ram_offset == 0x1328U
        && record.table_entry == 0x05U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 2U
        && record.cursor_after_delta == 3U
        && record.vertical_low == 0x84U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_alternate_continuation_full_record_path =
        entry.registers.accumulator == 0x06U
        && entry.registers.x == 0x17U
        && entry.registers.y == 0x75U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && alternate_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x1897U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6B0AU
        && record.motion_pointer == 0x6DE6U
        && record.cursor_work_ram_offset == 0x1E97U
        && record.vertical_low_work_ram_offset == 0x1317U
        && record.table_entry == 0x0BU
        && record.motion_header == 0x10U
        && record.motion_delta == 0x10U
        && record.event_written == 0U
        && record.cursor_after_header == 2U
        && record.cursor_after_delta == 3U
        && record.vertical_low == 0xC3U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_horizontal_continuation_full_record_path =
        entry.registers.accumulator == 0x2AU
        && entry.registers.x == 0x26U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x02U
        && horizontal_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A6U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2AU
        && record.motion_pointer == 0x6D56U
        && record.cursor_work_ram_offset == 0x1EA6U
        && record.horizontal_low_work_ram_offset == 0x11A6U
        && record.table_entry == 0x03U
        && record.motion_header == 0x20U
        && record.motion_delta == 0x20U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.horizontal_low == 0x42U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_reverse_horizontal_continuation_full_record_path =
        entry.registers.accumulator == 0x2BU
        && entry.registers.x == 0x27U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && reverse_horizontal_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A7U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C2DU
        && record.motion_pointer == 0x6D9EU
        && record.cursor_work_ram_offset == 0x1EA7U
        && record.horizontal_low_work_ram_offset == 0x11A7U
        && record.table_entry == 0x07U
        && record.motion_header == 0x40U
        && record.motion_delta == 0x40U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.horizontal_low == 0xD6U
        && record.horizontal_written != 0U
        && record.horizontal_high_written == 0U
        && record.vertical_written == 0U;
    const bool proven_later_vertical_continuation_full_record_path =
        entry.registers.accumulator == 0x2CU
        && entry.registers.x == 0x28U
        && entry.registers.y == 0x00U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x12U
        && later_vertical_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A8U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C30U
        && record.motion_pointer == 0x6D7AU
        && record.cursor_work_ram_offset == 0x1EA8U
        && record.vertical_low_work_ram_offset == 0x1328U
        && record.table_entry == 0x05U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.vertical_low == 0x85U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_second_later_vertical_continuation_full_record_path =
        entry.registers.accumulator == 0x2DU
        && entry.registers.x == 0x29U
        && entry.registers.y == 0x01U
        && entry.registers.stack_pointer == 0xF9U
        && entry.registers.status == 0x00U
        && second_later_vertical_continuation_full_record
        && has_full_record
        && record.pass.collision_work_ram_offset == 0x18A9U
        && record.pass.collision_state == 0x80U
        && record.pass.collision_state_next == 0x81U
        && record.pass.pass_index == 1U
        && record.pass.returns_without_step == 0U
        && record.pass.table_pointer == 0x6C35U
        && record.motion_pointer == 0x6D7AU
        && record.cursor_work_ram_offset == 0x1EA9U
        && record.vertical_low_work_ram_offset == 0x1329U
        && record.table_entry == 0x05U
        && record.motion_header == 0x08U
        && record.motion_delta == 0x08U
        && record.event_written == 0U
        && record.cursor_after_header == 3U
        && record.cursor_after_delta == 4U
        && record.vertical_low == 0x85U
        && record.horizontal_written == 0U
        && record.vertical_written != 0U
        && record.vertical_high_written == 0U;
    const bool proven_any_later_vertical_continuation_full_record_path =
        proven_later_vertical_continuation_full_record_path
        || proven_second_later_vertical_continuation_full_record_path;
    const bool proven_any_horizontal_continuation_full_record_path =
        proven_horizontal_continuation_full_record_path
        || proven_reverse_horizontal_continuation_full_record_path;
    const bool proven_any_continuation_full_record_path =
        proven_continuation_full_record_path
        || proven_second_continuation_full_record_path
        || proven_alternate_continuation_full_record_path
        || proven_any_horizontal_continuation_full_record_path
        || proven_any_later_vertical_continuation_full_record_path;
    const bool proven_any_reverse_full_record_path =
        proven_reverse_full_record_path
        || proven_alternate_reverse_full_record_path;
    const bool proven_any_vertical_full_record_path =
        proven_vertical_full_record_path
        || proven_second_vertical_full_record_path;
    const bool proven_full_record_path =
        proven_forward_full_record_path
        || proven_any_reverse_full_record_path
        || proven_any_vertical_full_record_path
        || proven_any_continuation_full_record_path;
    if (proven_full_record_path) {
        auto output = entry.registers;
        output.program_counter = 0x6A37U;
        output.accumulator =
            proven_alternate_continuation_full_record_path
                ? 0xC4U
                : ((proven_any_vertical_full_record_path
                    || proven_continuation_full_record_path
                    || proven_second_continuation_full_record_path
                    || proven_any_later_vertical_continuation_full_record_path)
                       ? 0x08U
                       : 0U);
        output.y =
            (proven_any_horizontal_continuation_full_record_path
             || proven_any_later_vertical_continuation_full_record_path)
                ? 0x03U
                : (proven_any_continuation_full_record_path
                       ? 0x02U
                       : 0x01U);
        output.status = 0x10U;

        if (proven_any_continuation_full_record_path) {
            RoutineReplacement replacement{
                .return_instruction = {
                    .physical_bank = 0x03U,
                    .logical_pc = 0x6A37U,
                },
                .output_registers = output,
                .body_master_cycles =
                    (proven_alternate_continuation_full_record_path
                     || proven_reverse_horizontal_continuation_full_record_path)
                        ? 549U
                        : 540U,
            };
            replacement.work_ram_writes = {
                {
                    .instruction = {0x03U, 0x696BU},
                    .logical_address = 0x202FU,
                    .value = static_cast<std::uint8_t>(
                        record.pass.table_pointer & 0x00FFU),
                    .master_cycle_offset = 39U,
                },
                {
                    .instruction = {0x03U, 0x6970U},
                    .logical_address = 0x2030U,
                    .value = static_cast<std::uint8_t>(
                        record.pass.table_pointer >> 8U),
                    .master_cycle_offset = 66U,
                },
                {
                    .instruction = {0x03U, 0x697CU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2100U + entry.registers.stack_pointer),
                    .value = 0x10U,
                    .master_cycle_offset = 114U,
                },
                {
                    .instruction = {0x03U, 0x697FU},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + record.pass.collision_work_ram_offset),
                    .value = record.pass.collision_state_next,
                    .master_cycle_offset = 135U,
                },
                {
                    .instruction = {0x03U, 0x6995U},
                    .logical_address = 0x2035U,
                    .value = 0U,
                    .master_cycle_offset = 177U,
                },
                {
                    .instruction = {0x03U, 0x69BBU},
                    .logical_address = 0x202DU,
                    .value = static_cast<std::uint8_t>(
                        record.motion_pointer & 0x00FFU),
                    .master_cycle_offset = 306U,
                },
                {
                    .instruction = {0x03U, 0x69C0U},
                    .logical_address = 0x202EU,
                    .value = static_cast<std::uint8_t>(
                        record.motion_pointer >> 8U),
                    .master_cycle_offset = 333U,
                },
                {
                    .instruction = {
                        0x03U,
                        proven_alternate_continuation_full_record_path
                            ? 0x6A30U
                            : (proven_reverse_horizontal_continuation_full_record_path
                                   ? 0x6A11U
                                   : (proven_horizontal_continuation_full_record_path
                                          ? 0x69FFU
                                          : 0x6A1EU)),
                    },
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U
                        + (proven_any_horizontal_continuation_full_record_path
                               ? record.horizontal_low_work_ram_offset
                               : record.vertical_low_work_ram_offset)),
                    .value =
                        proven_any_horizontal_continuation_full_record_path
                            ? record.horizontal_low
                            : record.vertical_low,
                    .master_cycle_offset =
                        proven_alternate_continuation_full_record_path
                            ? 522U
                            : (proven_reverse_horizontal_continuation_full_record_path
                                   ? 483U
                                   : (proven_horizontal_continuation_full_record_path
                                          ? 450U
                                          : 489U)),
                },
                {
                    .instruction = {0x03U, 0x6A33U},
                    .logical_address = static_cast<std::uint16_t>(
                        0x2000U + record.cursor_work_ram_offset),
                    .value = record.cursor_after_delta,
                    .master_cycle_offset =
                        (proven_alternate_continuation_full_record_path
                         || proven_reverse_horizontal_continuation_full_record_path)
                            ? 543U
                            : 534U,
                },
            };
            return replacement;
        }

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x03U,
                .logical_pc = 0x6A37U,
            },
            .output_registers = output,
            .body_master_cycles =
                proven_any_reverse_full_record_path ? 672U : 663U,
        };
        replacement.work_ram_writes = {
            {
                .instruction = {0x03U, 0x696BU},
                .logical_address = 0x202FU,
                .value = static_cast<std::uint8_t>(
                    record.pass.table_pointer & 0x00FFU),
                .master_cycle_offset = 39U,
            },
            {
                .instruction = {0x03U, 0x6970U},
                .logical_address = 0x2030U,
                .value = static_cast<std::uint8_t>(
                    record.pass.table_pointer >> 8U),
                .master_cycle_offset = 66U,
            },
            {
                .instruction = {0x03U, 0x697CU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + entry.registers.stack_pointer),
                .value = 0x10U,
                .master_cycle_offset = 114U,
            },
            {
                .instruction = {0x03U, 0x697FU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + record.pass.collision_work_ram_offset),
                .value = record.pass.collision_state_next,
                .master_cycle_offset = 135U,
            },
            {
                .instruction = {0x03U, 0x6995U},
                .logical_address = 0x2035U,
                .value = 0U,
                .master_cycle_offset = 177U,
            },
            {
                .instruction = {0x03U, 0x69BBU},
                .logical_address = 0x202DU,
                .value = static_cast<std::uint8_t>(
                    record.motion_pointer & 0x00FFU),
                .master_cycle_offset = 306U,
            },
            {
                .instruction = {0x03U, 0x69C0U},
                .logical_address = 0x202EU,
                .value = static_cast<std::uint8_t>(
                    record.motion_pointer >> 8U),
                .master_cycle_offset = 333U,
            },
            {
                .instruction = {0x03U, 0x69D9U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.event_work_ram_offset),
                .value = record.event,
                .master_cycle_offset = 420U,
            },
            {
                .instruction = {0x03U, 0x69EEU},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.orientation_work_ram_offset),
                .value = record.orientation,
                .master_cycle_offset = 480U,
            },
            {
                .instruction = {0x03U, 0x69F1U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.cursor_work_ram_offset),
                .value = record.cursor_after_header,
                .master_cycle_offset = 501U,
            },
            {
                .instruction = {
                    0x03U,
                    static_cast<std::uint16_t>(
                        proven_any_reverse_full_record_path
                            ? 0x6A11U
                            : (proven_any_vertical_full_record_path
                                   ? 0x6A1EU
                                   : 0x69FFU)),
                },
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U
                    + (proven_any_vertical_full_record_path
                           ? record.vertical_low_work_ram_offset
                           : record.horizontal_low_work_ram_offset)),
                .value = proven_any_vertical_full_record_path
                    ? record.vertical_low
                    : record.horizontal_low,
                .master_cycle_offset =
                    proven_any_reverse_full_record_path
                        ? 606U
                        : (proven_any_vertical_full_record_path
                               ? 612U
                               : 573U),
            },
            {
                .instruction = {0x03U, 0x6A33U},
                .logical_address = static_cast<std::uint16_t>(
                    0x2000U + record.cursor_work_ram_offset),
                .value = record.cursor_after_delta,
                .master_cycle_offset =
                    proven_any_reverse_full_record_path ? 666U : 657U,
            },
        };
        return replacement;
    }

#endif
    // This branch depends only on the ROM selector and the indexed pass
    // state. It is therefore admitted semantically for every selector/slot,
    // rather than enumerating individual dynamic call ordinals.
    if (pass.collision_state != 0x81U
        || pass.collision_state_next != 0x80U
        || pass.pass_index != 0U
        || pass.returns_without_step == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x6988U;
    output.accumulator = pass.collision_state_next;
    output.y = pass.collision_state;
    output.status = 0x12U;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x6988U,
        },
        .output_registers = output,
        .body_master_cycles = 165U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x696BU},
            .logical_address = 0x202FU,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer & 0x00FFU),
            .master_cycle_offset = 39U,
        },
        {
            .instruction = {0x03U, 0x6970U},
            .logical_address = 0x2030U,
            .value = static_cast<std::uint8_t>(
                pass.table_pointer >> 8U),
            .master_cycle_offset = 66U,
        },
        {
            .instruction = {0x03U, 0x697CU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = 0x12U,
            .master_cycle_offset = 114U,
        },
        {
            .instruction = {0x03U, 0x697FU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + pass.collision_work_ram_offset),
            .value = pass.collision_state_next,
            .master_cycle_offset = 135U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    const auto doubled_selector = static_cast<std::uint8_t>(
        checkpoint.accumulator << 1U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    if ((checkpoint.accumulator & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    checkpoint.accumulator = doubled_selector;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6966U, 0x6967U, 6U, checkpoint);

    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x6967U, 0x6968U, 12U, checkpoint);

    checkpoint.accumulator = static_cast<std::uint8_t>(
        pass.table_pointer & 0x00FFU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6968U, 0x696BU, 27U, checkpoint);
    append_checkpoint(0x696BU, 0x696DU, 39U, checkpoint);

    checkpoint.accumulator = static_cast<std::uint8_t>(
        pass.table_pointer >> 8U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x696DU, 0x6970U, 54U, checkpoint);
    append_checkpoint(0x6970U, 0x6972U, 66U, checkpoint);

    checkpoint.y = pass.collision_state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0x6972U, 0x6975U, 81U, checkpoint);
    append_checkpoint(0x6975U, 0x6977U, 87U, checkpoint);

    checkpoint.accumulator = checkpoint.y;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6977U, 0x6978U, 93U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x7FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x6978U, 0x697AU, 99U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator ^ 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x697AU, 0x697CU, 105U, checkpoint);

    const auto pushed_status = static_cast<std::uint8_t>(
        checkpoint.status | 0x10U);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer - 1U);
    append_checkpoint(0x697CU, 0x697DU, 114U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator | 0x80U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x697DU, 0x697FU, 120U, checkpoint);
    append_checkpoint(0x697FU, 0x6982U, 135U, checkpoint);

    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 1U);
    checkpoint.status = pushed_status;
    append_checkpoint(0x6982U, 0x6983U, 147U, checkpoint);
    append_checkpoint(0x6983U, 0x6987U, 159U, checkpoint);
    append_checkpoint(0x6987U, 0x6988U, 165U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_response(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7FB1U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_collision_response response{};
    if (!dc_advance_collision_response(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &response)) {
        throw std::runtime_error(
            "Native 7FB1 replacement rejected the work-RAM snapshot.");
    }
    auto output = entry.registers;
    output.program_counter = 0x7FD1U;
    output.accumulator = response.countdown_reloaded != 0U
        ? response.countdown
        : response.response;
    // The response ADC cannot carry or overflow: response_base is a
    // multiple of four and phase contributes only its low two bits. DEC
    // supplies the final N/Z flags on the ordinary path; INC does so on
    // the reload path. All other status bits survive the routine.
    output.status =
        static_cast<std::uint8_t>(entry.registers.status & 0x3CU);
    const auto final_flag_value = response.countdown_reloaded != 0U
        ? response.phase
        : response.countdown;
    if (final_flag_value == 0U) {
        output.status =
            static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((final_flag_value & 0x80U) != 0U) {
        output.status =
            static_cast<std::uint8_t>(output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7FD1U,
        },
        .output_registers = output,
        .body_master_cycles =
            response.countdown_reloaded != 0U ? 162U : 126U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x03U, 0x7FB7U},
            .logical_address = 0x2026U,
            .value = response.response_base,
            .master_cycle_offset = 39U,
        },
        {
            .instruction = {0x03U, 0x7FC1U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.response_work_ram_offset),
            .value = response.response,
            .master_cycle_offset = 93U,
        },
        {
            .instruction = {0x03U, 0x7FC4U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.countdown_work_ram_offset),
            .value = response.countdown_reloaded != 0U
                ? 0U
                : response.countdown,
            .master_cycle_offset = 114U,
        },
    };
    if (response.countdown_reloaded != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7FCBU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.countdown_work_ram_offset),
            .value = response.countdown,
            .master_cycle_offset = 141U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x03U, 0x7FCEU},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + response.phase_work_ram_offset),
            .value = response.phase,
            .master_cycle_offset = 162U,
        });
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x03U, instruction_pc},
                .registers = registers,
            });
        };

    const auto original_phase = std::to_integer<std::uint8_t>(
        work_ram[response.phase_work_ram_offset]);
    const auto masked_event =
        static_cast<std::uint8_t>(response.event & 0xFEU);
    const auto phase_bits =
        static_cast<std::uint8_t>(original_phase & 0x03U);
    const auto decremented_countdown =
        response.countdown_reloaded != 0U ? 0U : response.countdown;

    auto checkpoint = entry.registers;
    checkpoint.accumulator = response.event;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FB1U, 0x7FB4U, 15U, checkpoint);

    checkpoint.accumulator = masked_event;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FB4U, 0x7FB6U, 21U, checkpoint);

    checkpoint.accumulator = response.response_base;
    checkpoint.status = set_nz(
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U),
        checkpoint.accumulator);
    if ((masked_event & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    append_checkpoint(0x7FB6U, 0x7FB7U, 27U, checkpoint);
    append_checkpoint(0x7FB7U, 0x7FB9U, 39U, checkpoint);

    checkpoint.accumulator = original_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FB9U, 0x7FBCU, 54U, checkpoint);

    checkpoint.accumulator = phase_bits;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0x7FBCU, 0x7FBEU, 60U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(0x7FBEU, 0x7FBFU, 66U, checkpoint);

    checkpoint.accumulator = response.response;
    checkpoint.status = set_nz(
        static_cast<std::uint8_t>(checkpoint.status & ~0x41U),
        checkpoint.accumulator);
    append_checkpoint(0x7FBFU, 0x7FC1U, 78U, checkpoint);
    append_checkpoint(0x7FC1U, 0x7FC4U, 93U, checkpoint);

    checkpoint.status = set_nz(
        checkpoint.status,
        static_cast<std::uint8_t>(decremented_countdown));
    append_checkpoint(0x7FC4U, 0x7FC7U, 114U, checkpoint);

    if (response.countdown_reloaded == 0U) {
        append_checkpoint(0x7FC7U, 0x7FD1U, 126U, output);
    } else {
        append_checkpoint(0x7FC7U, 0x7FC9U, 120U, checkpoint);

        checkpoint.accumulator = response.countdown;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0x7FC9U, 0x7FCBU, 126U, checkpoint);
        append_checkpoint(0x7FCBU, 0x7FCEU, 141U, checkpoint);
        append_checkpoint(0x7FCEU, 0x7FD1U, 162U, output);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_consumer_no_hit(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x03U,
        .logical_pc = 0x7F57U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    const auto slot = entry.registers.x;
    const auto collision = std::to_integer<std::uint8_t>(
        work_ram[0x1800U + slot]);
    if ((collision & 0x80U) != 0U) {
        return std::nullopt;
    }

    auto table_entry = entry;
    table_entry.routine = {
        .physical_bank = 0x03U,
        .logical_pc = 0x6966U,
    };
    table_entry.registers.program_counter = 0x6966U;
    table_entry.registers.accumulator =
        std::to_integer<std::uint8_t>(work_ram[0x1780U + slot]);
    table_entry.registers.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    table_entry.registers.status = static_cast<std::uint8_t>(
        entry.registers.status & static_cast<std::uint8_t>(~0x82U));
    if (table_entry.registers.accumulator == 0U) {
        table_entry.registers.status = static_cast<std::uint8_t>(
            table_entry.registers.status | 0x02U);
    } else if ((table_entry.registers.accumulator & 0x80U) != 0U) {
        table_entry.registers.status = static_cast<std::uint8_t>(
            table_entry.registers.status | 0x80U);
    }

    auto table = replace_collision_table_interpreter(
        table_entry,
        work_ram,
        rom);
    if (!table
        || table->return_instruction.physical_bank != 0x03U
        || (table->return_instruction.logical_pc != 0x6988U
            && table->return_instruction.logical_pc != 0x6A37U)) {
        return std::nullopt;
    }

    // The response helper observes the collision-table helper's committed
    // state. Compose against a private snapshot so full-record transactions
    // can feed their new event byte into 7FB1 without mutating live RAM.
    std::vector<std::byte> response_work_ram(
        work_ram.begin(),
        work_ram.end());
    for (const auto& write : table->work_ram_writes) {
        if (write.logical_address < 0x2000U) {
            return std::nullopt;
        }
        const auto offset = static_cast<std::size_t>(
            write.logical_address - 0x2000U);
        if (offset >= response_work_ram.size()) {
            return std::nullopt;
        }
        response_work_ram[offset] =
            static_cast<std::byte>(write.value);
    }

    auto response_entry = entry;
    response_entry.routine = {
        .physical_bank = 0x03U,
        .logical_pc = 0x7FB1U,
    };
    response_entry.registers = table->output_registers;
    response_entry.registers.program_counter = 0x7FB1U;
    response_entry.registers.stack_pointer = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 2U);
    auto response = replace_collision_response(
        response_entry,
        response_work_ram);
    if (!response
        || response->return_instruction.physical_bank != 0x03U
        || response->return_instruction.logical_pc != 0x7FD1U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0x7F80U;
    output.accumulator = collision;
    output.y = response->output_registers.y;
    output.status = static_cast<std::uint8_t>(
        response->output_registers.status
        & static_cast<std::uint8_t>(~0x82U));
    if (collision == 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);
    } else if ((collision & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    }

    constexpr std::uint32_t kFirstChildOffset = 36U;
    const auto second_jsr_offset = static_cast<std::uint32_t>(
        kFirstChildOffset + table->body_master_cycles + 21U);
    const auto response_child_offset = static_cast<std::uint32_t>(
        second_jsr_offset + 21U);
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x03U,
            .logical_pc = 0x7F80U,
        },
        .output_registers = output,
        .body_master_cycles = static_cast<std::uint32_t>(
            response_child_offset
            + response->body_master_cycles
            + 48U),
    };
    replacement.work_ram_writes.reserve(
        4U
        + table->work_ram_writes.size()
        + response->work_ram_writes.size());
    const auto outer_stack_high = entry.registers.stack_pointer;
    const auto outer_stack_low = static_cast<std::uint8_t>(
        entry.registers.stack_pointer - 1U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5AU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_high),
        .value = 0x7FU,
        .master_cycle_offset = 15U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5AU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_low),
        .value = 0x5CU,
        .master_cycle_offset = 15U,
    });
    for (auto write : table->work_ram_writes) {
        write.master_cycle_offset += kFirstChildOffset;
        replacement.work_ram_writes.push_back(write);
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5DU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_high),
        .value = 0x7FU,
        .master_cycle_offset = second_jsr_offset,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x03U, 0x7F5DU},
        .logical_address =
            static_cast<std::uint16_t>(0x2100U + outer_stack_low),
        .value = 0x5FU,
        .master_cycle_offset = second_jsr_offset,
    });
    for (auto write : response->work_ram_writes) {
        write.master_cycle_offset += response_child_offset;
        replacement.work_ram_writes.push_back(write);
    }

    if (!table->checkpoints.empty()
        && !response->checkpoints.empty()) {
        const auto set_nz =
            [](std::uint8_t status, const std::uint8_t value) {
                status = static_cast<std::uint8_t>(
                    status & static_cast<std::uint8_t>(~0xA2U));
                if (value == 0U) {
                    status =
                        static_cast<std::uint8_t>(status | 0x02U);
                } else if ((value & 0x80U) != 0U) {
                    status =
                        static_cast<std::uint8_t>(status | 0x80U);
                }
                return status;
            };
        const auto append_checkpoint =
            [&replacement](
                const std::uint16_t instruction_pc,
                const std::uint16_t resume_pc,
                const std::uint32_t interrupt_sample_master_cycle,
                const std::uint32_t master_cycle,
                auto registers) {
                registers.program_counter = resume_pc;
                replacement.checkpoints.push_back({
                    .interrupt_sample_master_cycle_offset =
                        interrupt_sample_master_cycle,
                    .master_cycle_offset = master_cycle,
                    .instruction = {0x03U, instruction_pc},
                    .registers = registers,
                });
            };

        auto checkpoint = entry.registers;
        checkpoint.accumulator =
            std::to_integer<std::uint8_t>(
                work_ram[0x1780U + slot]);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            0x7F57U,
            0x7F5AU,
            12U,
            15U,
            checkpoint);

        append_checkpoint(
            0x7F5AU,
            0x6966U,
            33U,
            kFirstChildOffset,
            table_entry.registers);

        for (auto child_checkpoint : table->checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset +=
                kFirstChildOffset;
            child_checkpoint.master_cycle_offset +=
                kFirstChildOffset;
            replacement.checkpoints.push_back(child_checkpoint);
        }

        auto after_table_rts = table->output_registers;
        after_table_rts.stack_pointer = entry.registers.stack_pointer;
        append_checkpoint(
            table->return_instruction.logical_pc,
            0x7F5DU,
            second_jsr_offset - 3U,
            second_jsr_offset,
            after_table_rts);

        append_checkpoint(
            0x7F5DU,
            0x7FB1U,
            response_child_offset - 3U,
            response_child_offset,
            response_entry.registers);

        for (auto child_checkpoint : response->checkpoints) {
            child_checkpoint.interrupt_sample_master_cycle_offset +=
                response_child_offset;
            child_checkpoint.master_cycle_offset +=
                response_child_offset;
            replacement.checkpoints.push_back(child_checkpoint);
        }

        const auto response_end = static_cast<std::uint32_t>(
            response_child_offset + response->body_master_cycles);
        auto after_response_rts = response->output_registers;
        after_response_rts.stack_pointer =
            entry.registers.stack_pointer;
        append_checkpoint(
            response->return_instruction.logical_pc,
            0x7F60U,
            response_end + 18U,
            response_end + 21U,
            after_response_rts);

        checkpoint = after_response_rts;
        checkpoint.accumulator = collision;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            0x7F60U,
            0x7F63U,
            response_end + 33U,
            response_end + 36U,
            checkpoint);
        append_checkpoint(
            0x7F63U,
            0x7F80U,
            response_end + 45U,
            response_end + 48U,
            output);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_geometry(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCCD5U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U
        || entry.registers.y >= 0x80U
        || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    const auto read_logical =
        [&entry, work_ram, rom](const std::uint16_t address)
            -> std::optional<std::uint8_t> {
            const auto segment = static_cast<std::uint8_t>(address >> 13U);
            const auto bank = entry.registers.mpr[segment];
            const auto bank_offset =
                static_cast<std::size_t>(address & 0x1FFFU);
            if (bank == 0xF8U) {
                return std::to_integer<std::uint8_t>(
                    work_ram[bank_offset]);
            }
            std::size_t mapped_bank = bank;
            if (rom.size() == 0x60000U && bank < 0x80U) {
                mapped_bank = bank < 0x40U
                    ? static_cast<std::size_t>(bank & 0x1FU)
                    : static_cast<std::size_t>(
                        (bank & 0x0FU) + 0x20U);
            }
            const auto rom_offset =
                mapped_bank * 0x2000U + bank_offset;
            if (rom_offset >= rom.size()) {
                return std::nullopt;
            }
            return std::to_integer<std::uint8_t>(rom[rom_offset]);
        };
    const auto pointer_at =
        [work_ram](const std::size_t low_offset) {
            return static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(work_ram[low_offset])
                | (static_cast<std::uint16_t>(
                       std::to_integer<std::uint8_t>(
                           work_ram[low_offset + 1U]))
                   << 8U));
        };
    const auto indexed_address =
        [](const std::uint16_t base, const std::uint8_t index) {
            return static_cast<std::uint16_t>(base + index);
        };
    const auto target = entry.registers.y;
    const auto directory_index = std::to_integer<std::uint8_t>(
        work_ram[0x1900U + target]);
    const auto phase_index = static_cast<std::uint8_t>(
        std::to_integer<std::uint8_t>(
            work_ram[0x1980U + target])
        << 1U);
    const std::array<std::uint16_t, 4> directory_bases{
        pointer_at(0x0096U),
        pointer_at(0x0098U),
        pointer_at(0x009AU),
        pointer_at(0x009CU),
    };
    std::array<std::uint8_t, 4> directory_bytes{};
    for (std::size_t index = 0; index < directory_bases.size(); ++index) {
        const auto value = read_logical(indexed_address(
            directory_bases[index], directory_index));
        if (!value) {
            return std::nullopt;
        }
        directory_bytes[index] = *value;
    }

    const std::array<std::uint16_t, 2> geometry_bases{
        static_cast<std::uint16_t>(
            directory_bytes[0]
            | (static_cast<std::uint16_t>(directory_bytes[1]) << 8U)),
        static_cast<std::uint16_t>(
            directory_bytes[2]
            | (static_cast<std::uint16_t>(directory_bytes[3]) << 8U)),
    };
    std::array<std::uint8_t, 4> geometry_bytes{};
    for (std::size_t base_index = 0;
         base_index < geometry_bases.size();
         ++base_index) {
        for (std::size_t byte_index = 0; byte_index < 2U; ++byte_index) {
            const auto index = static_cast<std::uint8_t>(
                phase_index + byte_index);
            const auto value = read_logical(indexed_address(
                geometry_bases[base_index], index));
            if (!value) {
                return std::nullopt;
            }
            const auto output_index = base_index + byte_index * 2U;
            geometry_bytes[output_index] = *value;
        }
    }

    const auto descriptor_base = static_cast<std::uint16_t>(
        geometry_bytes[0]
        | (static_cast<std::uint16_t>(geometry_bytes[2]) << 8U));
    std::array<std::uint8_t, 4> descriptor{};
    for (std::size_t index = 0; index < descriptor.size(); ++index) {
        const auto value = read_logical(indexed_address(
            descriptor_base, static_cast<std::uint8_t>(index)));
        if (!value) {
            return std::nullopt;
        }
        descriptor[index] = *value;
    }

    const dc_collision_geometry_tables tables{
        .first_directory_low = directory_bytes[0],
        .first_directory_high = directory_bytes[1],
        .second_directory_low = directory_bytes[2],
        .second_directory_high = directory_bytes[3],
        .first_geometry_low = geometry_bytes[0],
        .first_geometry_high = geometry_bytes[2],
        .second_geometry_low = geometry_bytes[1],
        .second_geometry_high = geometry_bytes[3],
        .horizontal_offset = descriptor[0],
        .vertical_offset = descriptor[1],
        .horizontal_limit = descriptor[2],
        .vertical_limit = descriptor[3],
    };
    dc_collision_geometry geometry{};
    if (!dc_prepare_collision_geometry(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            target,
            &tables,
            &geometry)
        || geometry.vertical.branch
            != DC_COLLISION_SCAN_VERTICAL_DEFER
        || geometry.position_rejected != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC3U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };

    const auto target_low = std::to_integer<std::uint8_t>(
        work_ram[0x1300U + target]);
    const auto target_high = std::to_integer<std::uint8_t>(
        work_ram[0x1380U + target]);
    const auto low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(target_low)
        + tables.vertical_offset);
    const bool low_carry = low_sum > 0xFFU;
    auto output = entry.registers;
    output.program_counter = 0xCD70U;
    output.accumulator = tables.vertical_limit;
    output.y = target;
    if (geometry.vertical_offset_negative != 0U) {
        output.status = adc_status(
            output.status,
            target_high,
            0xFFU,
            low_carry,
            geometry.collision_high);
    } else {
        output.status = adc_status(
            output.status,
            0U,
            target_high,
            low_carry,
            geometry.collision_high);
    }
    output.status = set_nz(output.status, entry.registers.x);
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x01U));

    const bool vertical_negative =
        geometry.vertical.difference_high != 0U;
    std::uint32_t cycle = vertical_negative ? 222U : 120U;
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCD70U,
        },
        .output_registers = output,
    };
    replacement.work_ram_writes.reserve(19U);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xCCDBU},
        .logical_address = 0x2042U,
        .value = geometry.vertical.difference_low,
        .master_cycle_offset = 45U,
    });
    if (vertical_negative) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF0U},
            .logical_address = 0x2043U,
            .value = geometry.vertical.difference_high,
            .master_cycle_offset = 102U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF6U},
            .logical_address = 0x2042U,
            .value = geometry.vertical.magnitude_low,
            .master_cycle_offset = 138U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCFBU},
            .logical_address = 0x2043U,
            .value = geometry.vertical.magnitude_high,
            .master_cycle_offset = 168U,
        });
    }
    const auto add_write =
        [&replacement](
            const std::uint16_t pc,
            const std::uint16_t address,
            const std::uint8_t value,
            const std::uint32_t write_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = write_cycle,
            });
        };

    cycle += 9U;
    add_write(
        0xCD01U,
        static_cast<std::uint16_t>(
            0x2100U + entry.registers.stack_pointer),
        entry.registers.x,
        cycle);
    cycle += 6U + 15U;
    for (std::size_t index = 0; index < 4U; ++index) {
        cycle += index == 0U ? 24U : 21U;
        cycle += 12U;
        add_write(
            static_cast<std::uint16_t>(0xCD08U + index * 4U),
            static_cast<std::uint16_t>(0x202DU + index),
            directory_bytes[index],
            cycle);
    }
    cycle += 15U + 6U + 6U;
    for (std::size_t index = 0; index < 4U; ++index) {
        if (index == 2U) {
            cycle += 6U;
        }
        cycle += 21U;
        cycle += 12U;
        static constexpr std::array<std::uint16_t, 4> kGeometryPcs{
            0xCD1DU, 0xCD21U, 0xCD26U, 0xCD2AU,
        };
        static constexpr std::array<std::uint16_t, 4> kGeometryAddresses{
            0x2072U, 0x2074U, 0x2073U, 0x2075U,
        };
        add_write(
            kGeometryPcs[index],
            kGeometryAddresses[index],
            geometry_bytes[index],
            cycle);
    }
    cycle += 6U + 15U + 12U + 12U;
    add_write(0xCD33U, 0x2068U, geometry.position_low, cycle);
    cycle += 15U + 12U + 6U + 6U + 12U;
    cycle += 15U;
    cycle += 12U;
    add_write(
        0xCD41U, 0x2068U, geometry.adjusted_position_low, cycle);

    cycle += 6U;
    cycle += 21U;
    if (geometry.vertical_offset_negative != 0U) {
        cycle += 12U + 6U + 15U + 12U;
        add_write(0xCD5AU, 0x2069U, geometry.collision_low, cycle);
        cycle += 15U + 6U + 12U;
        add_write(0xCD61U, 0x206AU, geometry.collision_high, cycle);
    } else {
        cycle += 6U + 6U + 15U + 12U;
        add_write(0xCD4CU, 0x2069U, geometry.collision_low, cycle);
        cycle += 6U + 15U + 12U;
        add_write(0xCD52U, 0x206AU, geometry.collision_high, cycle);
        cycle += 12U;
    }
    cycle += 6U;
    cycle += 21U;
    cycle += 12U;
    add_write(0xCD66U, 0x206BU, tables.horizontal_limit, cycle);
    cycle += 6U;
    cycle += 21U;
    cycle += 12U;
    add_write(0xCD6BU, 0x206CU, tables.vertical_limit, cycle);
    cycle += 6U + 15U + 6U;
    replacement.body_master_cycles = cycle;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_collision_scan_vertical_reject(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xCC1AU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_collision_scan_vertical_reject reject{};
    if (!dc_classify_collision_scan_vertical_reject(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.y,
            &reject)) {
        throw std::runtime_error(
            "Native CC1A replacement rejected the work-RAM snapshot.");
    }
    if (reject.branch == DC_COLLISION_SCAN_VERTICAL_DEFER
        || (entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0x82U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC3U));
            status = set_nz(status, result);
            if (static_cast<std::uint16_t>(left) >= subtrahend) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((left ^ result) & (left ^ right) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto result = static_cast<std::uint8_t>(left - right);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x83U));
            status = set_nz(status, result);
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [](
            RoutineReplacement& replacement,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    const bool negative =
        reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE
        || reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW;
    const bool compares_low =
        reject.branch == DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW
        || reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_LOW;
    const std::uint32_t body_master_cycles =
        reject.branch == DC_COLLISION_SCAN_VERTICAL_POSITIVE_PAGE
        ? 150U
        : reject.branch == DC_COLLISION_SCAN_VERTICAL_SAME_PAGE_LOW
        ? 180U
        : reject.branch == DC_COLLISION_SCAN_VERTICAL_NEGATIVE_PAGE
        ? 246U
        : 282U;

    auto output = entry.registers;
    output.program_counter = 0xCC40U;
    output.accumulator = negative
        ? (compares_low ? reject.magnitude_low : reject.magnitude_high)
        : (compares_low ? reject.difference_low : reject.difference_high);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xCC40U,
        },
        .output_registers = output,
        .body_master_cycles = body_master_cycles,
    };

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xCC1AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = 0xCCU,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x01U, 0xCC1AU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = 0x1CU,
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {0x01U, 0xCCDBU},
            .logical_address = 0x2042U,
            .value = reject.difference_low,
            .master_cycle_offset = 66U,
        },
    };
    if (negative) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF0U},
            .logical_address = 0x2043U,
            .value = reject.difference_high,
            .master_cycle_offset = 123U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCF6U},
            .logical_address = 0x2042U,
            .value = reject.magnitude_low,
            .master_cycle_offset = 159U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xCCFBU},
            .logical_address = 0x2043U,
            .value = reject.magnitude_high,
            .master_cycle_offset = 189U,
        });
    }

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(replacement, 0xCC1AU, 0xCCD5U, 21U, checkpoint);

    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status | 0x01U);
    append_checkpoint(replacement, 0xCCD5U, 0xCCD6U, 27U, checkpoint);

    checkpoint.accumulator = reject.source_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xCCD6U, 0xCCD8U, 39U, checkpoint);

    checkpoint.status = sbc_status(
        checkpoint.status,
        reject.source_low,
        reject.target_low,
        true,
        reject.difference_low);
    checkpoint.accumulator = reject.difference_low;
    append_checkpoint(replacement, 0xCCD8U, 0xCCDBU, 54U, checkpoint);
    append_checkpoint(replacement, 0xCCDBU, 0xCCDDU, 66U, checkpoint);

    checkpoint.accumulator = reject.source_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(replacement, 0xCCDDU, 0xCCDFU, 78U, checkpoint);

    checkpoint.status = sbc_status(
        checkpoint.status,
        reject.source_high,
        reject.target_high,
        reject.low_subtract_carry != 0U,
        reject.difference_high);
    checkpoint.accumulator = reject.difference_high;
    append_checkpoint(replacement, 0xCCDFU, 0xCCE2U, 93U, checkpoint);

    std::uint32_t cycle = 93U;
    if (reject.difference_high == 0U) {
        cycle += 12U;
        append_checkpoint(replacement, 0xCCE2U, 0xCCE8U, cycle, checkpoint);
    } else {
        cycle += 6U;
        append_checkpoint(replacement, 0xCCE2U, 0xCCE4U, cycle, checkpoint);
        cycle += negative ? 12U : 6U;
        append_checkpoint(
            replacement,
            0xCCE4U,
            negative ? 0xCCF0U : 0xCCE6U,
            cycle,
            checkpoint);
    }

    if (negative) {
        append_checkpoint(
            replacement, 0xCCF0U, 0xCCF2U, cycle += 12U, checkpoint);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status | 0x01U);
        append_checkpoint(
            replacement, 0xCCF2U, 0xCCF3U, cycle += 6U, checkpoint);
        checkpoint.accumulator = 0U;
        append_checkpoint(
            replacement, 0xCCF3U, 0xCCF4U, cycle += 6U, checkpoint);
        checkpoint.status = sbc_status(
            checkpoint.status,
            0U,
            reject.difference_low,
            true,
            reject.magnitude_low);
        checkpoint.accumulator = reject.magnitude_low;
        append_checkpoint(
            replacement, 0xCCF4U, 0xCCF6U, cycle += 12U, checkpoint);
        append_checkpoint(
            replacement, 0xCCF6U, 0xCCF8U, cycle += 12U, checkpoint);
        checkpoint.accumulator = 0U;
        append_checkpoint(
            replacement, 0xCCF8U, 0xCCF9U, cycle += 6U, checkpoint);
        checkpoint.status = sbc_status(
            checkpoint.status,
            0U,
            reject.difference_high,
            (reject.magnitude_low == 0U),
            reject.magnitude_high);
        checkpoint.accumulator = reject.magnitude_high;
        append_checkpoint(
            replacement, 0xCCF9U, 0xCCFBU, cycle += 12U, checkpoint);
        append_checkpoint(
            replacement, 0xCCFBU, 0xCCFDU, cycle += 12U, checkpoint);
        if (reject.magnitude_high != 0U) {
            append_checkpoint(
                replacement, 0xCCFDU, 0xCCE6U, cycle += 12U, checkpoint);
        } else {
            append_checkpoint(
                replacement, 0xCCFDU, 0xCCFFU, cycle += 6U, checkpoint);
            append_checkpoint(
                replacement, 0xCCFFU, 0xCCE8U, cycle += 12U, checkpoint);
        }
    }

    if (compares_low) {
        checkpoint.accumulator = negative
            ? reject.magnitude_low
            : reject.difference_low;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            replacement, 0xCCE8U, 0xCCEAU, cycle += 12U, checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status, checkpoint.accumulator, 0x80U);
        append_checkpoint(
            replacement, 0xCCEAU, 0xCCECU, cycle += 6U, checkpoint);
        append_checkpoint(
            replacement, 0xCCECU, 0xCCE6U, cycle += 12U, checkpoint);
    }

    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status | 0x01U);
    append_checkpoint(
        replacement, 0xCCE6U, 0xCCE7U, cycle += 6U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 2U);
    append_checkpoint(
        replacement, 0xCCE7U, 0xCC1DU, cycle += 21U, checkpoint);
    append_checkpoint(
        replacement, 0xCC1DU, 0xCC3FU, cycle += 12U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    append_checkpoint(
        replacement, 0xCC3FU, 0xCC40U, cycle += 6U, checkpoint);

    checkpoint.program_counter = 0xCC40U;
    replacement.output_registers = checkpoint;
    if (cycle != body_master_cycles) {
        throw std::runtime_error(
            "Native CC1A replacement composition diverged from its "
            "state model.");
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_input_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD466U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_flipper_input_gate gate{};
    if (!dc_evaluate_flipper_input_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate)) {
        throw std::runtime_error(
            "Native D466 replacement rejected the work-RAM snapshot.");
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [](
            RoutineReplacement& replacement,
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto output = entry.registers;
    if (gate.matched_input == 0U) {
        output.program_counter = 0xD46EU;
        output.accumulator = gate.matched_input;
        output.status = static_cast<std::uint8_t>(
            output.status & static_cast<std::uint8_t>(~0x82U));
        output.status = static_cast<std::uint8_t>(output.status | 0x02U);

        RoutineReplacement replacement{
            .return_instruction = {
                .physical_bank = 0x01U,
                .logical_pc = 0xD46EU,
            },
            .output_registers = output,
            .body_master_cycles = 36U,
        };
        auto checkpoint = entry.registers;
        checkpoint.accumulator = gate.global_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            replacement, 0xD466U, 0xD469U, 15U, checkpoint);
        checkpoint.accumulator = gate.matched_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            replacement, 0xD469U, 0xD46CU, 30U, checkpoint);
        append_checkpoint(
            replacement, 0xD46CU, 0xD46EU, 36U, output);
        return replacement;
    }

    dc_flipper_activation activation{};
    if (!dc_prepare_flipper_activation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &gate,
            &activation)) {
        throw std::runtime_error(
            "Native D466 activation rejected the work-RAM snapshot.");
    }
    if (activation.supported == 0U) {
        return std::nullopt;
    }

    output.program_counter = 0xC5E9U;
    output.accumulator = activation.dispatch_high;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x83U));
    output.status = static_cast<std::uint8_t>(output.status | 0x80U);
    if (activation.compare_carry != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x01U);
    }

    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);
    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5E9U,
        },
        .output_registers = output,
        .body_master_cycles = 282U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD471U},
            .logical_address = static_cast<std::uint16_t>(
                0x3E00U + entry.registers.x),
            .value = 0x01U,
            .master_cycle_offset = 63U,
        },
        {
            .instruction = {0x01U, 0xD474U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = 0x01U,
            .master_cycle_offset = 78U,
        },
        {
            .instruction = {0x01U, 0xD48BU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_high),
            .value = 0xD4U,
            .master_cycle_offset = 102U,
        },
        {
            .instruction = {0x01U, 0xD48BU},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + stack_low),
            .value = 0x8DU,
            .master_cycle_offset = 102U,
        },
        {
            .instruction = {0x01U, 0xC5CAU},
            .logical_address = 0x2026U,
            .value = 0x8DU,
            .master_cycle_offset = 147U,
        },
        {
            .instruction = {0x01U, 0xC5CDU},
            .logical_address = 0x2027U,
            .value = 0xD4U,
            .master_cycle_offset = 171U,
        },
        {
            .instruction = {0x01U, 0xC5D9U},
            .logical_address = 0x2026U,
            .value = activation.dispatch_low,
            .master_cycle_offset = 216U,
        },
        {
            .instruction = {0x01U, 0xC5E1U},
            .logical_address = static_cast<std::uint16_t>(
                0x3F00U + entry.registers.x),
            .value = activation.dispatch_low,
            .master_cycle_offset = 255U,
        },
        {
            .instruction = {0x01U, 0xC5E6U},
            .logical_address = static_cast<std::uint16_t>(
                0x3F80U + entry.registers.x),
            .value = activation.dispatch_high,
            .master_cycle_offset = 282U,
        },
    };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.global_input;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xD466U, 0xD469U, 15U, checkpoint);

    checkpoint.accumulator = gate.matched_input;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xD469U, 0xD46CU, 30U, checkpoint);
    append_checkpoint(
        replacement, 0xD46CU, 0xD46FU, 42U, checkpoint);

    checkpoint.accumulator = 0x01U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xD46FU, 0xD471U, 48U, checkpoint);
    append_checkpoint(
        replacement, 0xD471U, 0xD474U, 63U, checkpoint);
    append_checkpoint(
        replacement, 0xD474U, 0xD477U, 78U, checkpoint);

    const auto compare_result = static_cast<std::uint8_t>(
        entry.registers.x - activation.current_slot);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0xA3U));
    if (compare_result == 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x02U);
    } else if ((compare_result & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x80U);
    }
    if (activation.compare_carry != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    append_checkpoint(
        replacement, 0xD477U, 0xD479U, 90U, checkpoint);
    append_checkpoint(
        replacement, 0xD479U, 0xD48BU, 102U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 2U);
    append_checkpoint(
        replacement, 0xD48BU, 0xC5C9U, 123U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = 0x8DU;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5C9U, 0xC5CAU, 135U, checkpoint);
    append_checkpoint(
        replacement, 0xC5CAU, 0xC5CCU, 147U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = 0xD4U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5CCU, 0xC5CDU, 159U, checkpoint);
    append_checkpoint(
        replacement, 0xC5CDU, 0xC5CFU, 171U, checkpoint);

    checkpoint.accumulator = activation.coroutine_countdown;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5CFU, 0xC5D2U, 186U, checkpoint);
    append_checkpoint(
        replacement, 0xC5D2U, 0xC5D9U, 198U, checkpoint);

    checkpoint.status =
        set_nz(checkpoint.status, activation.dispatch_low);
    append_checkpoint(
        replacement, 0xC5D9U, 0xC5DBU, 216U, checkpoint);
    append_checkpoint(
        replacement, 0xC5DBU, 0xC5DFU, 228U, checkpoint);

    checkpoint.accumulator = activation.dispatch_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5DFU, 0xC5E1U, 240U, checkpoint);
    append_checkpoint(
        replacement, 0xC5E1U, 0xC5E4U, 255U, checkpoint);

    checkpoint.accumulator = activation.dispatch_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        replacement, 0xC5E4U, 0xC5E6U, 267U, checkpoint);
    append_checkpoint(
        replacement, 0xC5E6U, 0xC5E9U, 282U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_phase_step(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD500U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_flipper_phase_step step{};
    if (!dc_advance_flipper_phase(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &step)) {
        throw std::runtime_error(
            "Native D500 replacement rejected the work-RAM snapshot.");
    }
    if (step.supported == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xD524U;
    output.accumulator = step.phase;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x82U));
    if (step.decrements_phase == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status & static_cast<std::uint8_t>(~0x01U));
        if (step.compare_carry != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x01U);
        }
    }
    if (step.phase_next == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    } else if ((step.phase_next & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD524U,
        },
        .output_registers = output,
        .body_master_cycles =
            step.decrements_phase != 0U ? 126U : 138U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD500U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = 0U,
            .master_cycle_offset = 21U,
        },
        {
            .instruction = {0x01U, 0xD508U},
            .logical_address = static_cast<std::uint16_t>(
                0x3A00U + entry.registers.x),
            .value = step.reload,
            .master_cycle_offset = 57U,
        },
        {
            .instruction = {
                0x01U,
                static_cast<std::uint16_t>(
                    step.decrements_phase != 0U
                        ? 0xD521U
                        : 0xD517U),
            },
            .logical_address = static_cast<std::uint16_t>(
                0x3980U + entry.registers.x),
            .value = step.phase_next,
            .master_cycle_offset = 126U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.status = set_nz(checkpoint.status, 0U);
    append_checkpoint(0xD500U, 0xD503U, 21U, checkpoint);
    append_checkpoint(0xD503U, 0xD505U, 27U, checkpoint);

    checkpoint.accumulator = step.reload;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD505U, 0xD508U, 42U, checkpoint);
    append_checkpoint(0xD508U, 0xD50BU, 57U, checkpoint);

    checkpoint.accumulator = step.orientation;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD50BU, 0xD50EU, 72U, checkpoint);
    if (step.decrements_phase != 0U) {
        append_checkpoint(0xD50EU, 0xD51CU, 84U, checkpoint);

        checkpoint.accumulator = step.phase;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xD51CU, 0xD51FU, 99U, checkpoint);
        append_checkpoint(0xD51FU, 0xD521U, 105U, checkpoint);
        append_checkpoint(0xD521U, 0xD524U, 126U, output);
        return replacement;
    }
    append_checkpoint(0xD50EU, 0xD510U, 78U, checkpoint);

    checkpoint.accumulator = step.phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD510U, 0xD513U, 93U, checkpoint);

    const auto compare_result =
        static_cast<std::uint8_t>(step.phase - 3U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0xA3U));
    if (compare_result == 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x02U);
    } else if ((compare_result & 0x80U) != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x80U);
    }
    if (step.compare_carry != 0U) {
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    }
    append_checkpoint(0xD513U, 0xD515U, 99U, checkpoint);
    append_checkpoint(0xD515U, 0xD517U, 105U, checkpoint);
    append_checkpoint(0xD517U, 0xD51AU, 126U, output);
    append_checkpoint(0xD51AU, 0xD524U, 138U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_flipper_geometry_lookup(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xD081U,
    };
    constexpr std::size_t kGeometryTableOffset =
        0x01U * 0x2000U + (0xD2AEU & 0x1FFFU);
    constexpr std::size_t kGeometryTableSize = 0x80U;
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if (work_ram.size() < 0x2000U
        || kGeometryTableOffset + kGeometryTableSize > rom.size()) {
        throw std::runtime_error(
            "Native D081 replacement rejected its memory snapshots.");
    }

    dc_flipper_geometry geometry{};
    if (!dc_lookup_flipper_geometry(
            std::to_integer<std::uint8_t>(work_ram[0x0078U]),
            reinterpret_cast<const std::uint8_t*>(
                rom.data() + kGeometryTableOffset),
            kGeometryTableSize,
            &geometry)) {
        throw std::runtime_error(
            "Native D081 replacement rejected the geometry table.");
    }

    auto output = entry.registers;
    output.program_counter = 0xD09DU;
    output.accumulator = geometry.second_high;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0xA3U));
    if (entry.registers.y == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    } else if ((entry.registers.y & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xD09DU,
        },
        .output_registers = output,
        .body_master_cycles = 159U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xD085U},
            .logical_address = static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            .value = entry.registers.y,
            .master_cycle_offset = 27U,
        },
        {
            .instruction = {0x01U, 0xD08BU},
            .logical_address = 0x207FU,
            .value = geometry.first_low,
            .master_cycle_offset = 66U,
        },
        {
            .instruction = {0x01U, 0xD090U},
            .logical_address = 0x2080U,
            .value = geometry.first_high,
            .master_cycle_offset = 93U,
        },
        {
            .instruction = {0x01U, 0xD095U},
            .logical_address = 0x207DU,
            .value = geometry.second_low,
            .master_cycle_offset = 120U,
        },
        {
            .instruction = {0x01U, 0xD09AU},
            .logical_address = 0x207EU,
            .value = geometry.second_high,
            .master_cycle_offset = 147U,
        },
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = geometry.state;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD081U, 0xD083U, 12U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x1FU);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD083U, 0xD085U, 18U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer - 1U);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x20U);
    append_checkpoint(0xD085U, 0xD086U, 27U, checkpoint);

    checkpoint.accumulator = geometry.table_index;
    checkpoint.status = set_nz(
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U),
        checkpoint.accumulator);
    append_checkpoint(0xD086U, 0xD087U, 33U, checkpoint);

    checkpoint.y = checkpoint.accumulator;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xD087U, 0xD088U, 39U, checkpoint);

    checkpoint.accumulator = geometry.first_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD088U, 0xD08BU, 54U, checkpoint);
    append_checkpoint(0xD08BU, 0xD08DU, 66U, checkpoint);

    checkpoint.accumulator = geometry.first_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD08DU, 0xD090U, 81U, checkpoint);
    append_checkpoint(0xD090U, 0xD092U, 93U, checkpoint);

    checkpoint.accumulator = geometry.second_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD092U, 0xD095U, 108U, checkpoint);
    append_checkpoint(0xD095U, 0xD097U, 120U, checkpoint);

    checkpoint.accumulator = geometry.second_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xD097U, 0xD09AU, 135U, checkpoint);
    append_checkpoint(0xD09AU, 0xD09CU, 147U, checkpoint);
    append_checkpoint(0xD09CU, 0xD09DU, 159U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_coroutine_scheduler(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC5C9U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_coroutine_schedule schedule{};
    if (!dc_schedule_coroutine(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            entry.registers.stack_pointer,
            &schedule)) {
        throw std::runtime_error(
            "Native C5C9 replacement rejected the work-RAM snapshot.");
    }

    const auto countdown_offset = static_cast<std::uint16_t>(
        0x3D80U + entry.registers.x);
    const auto countdown_path_offset =
        schedule.decrements_countdown != 0U ? 21U : 0U;
    const auto carry_path_offset =
        schedule.dispatch_low_wrapped != 0U ? 12U : 0U;

    auto output = entry.registers;
    output.program_counter = 0xC5E9U;
    output.stack_pointer =
        static_cast<std::uint8_t>(output.stack_pointer + 2U);
    output.accumulator = schedule.installs_dispatch != 0U
        ? schedule.dispatch_high
        : schedule.countdown;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0xA2U));
    const auto flag_value = schedule.installs_dispatch != 0U
        ? schedule.dispatch_high
        : schedule.countdown_after;
    if (flag_value == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    } else if ((flag_value & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC5E9U,
        },
        .output_registers = output,
        .body_master_cycles = 102U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {0x01U, 0xC5CAU},
            .logical_address = 0x2026U,
            .value = schedule.stacked_return_low,
            .master_cycle_offset = 24U,
        },
        {
            .instruction = {0x01U, 0xC5CDU},
            .logical_address = 0x2027U,
            .value = schedule.stacked_return_high,
            .master_cycle_offset = 48U,
        },
    };
    if (schedule.decrements_countdown != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5D4U},
            .logical_address = countdown_offset,
            .value = schedule.countdown_after,
            .master_cycle_offset = 90U,
        });
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = schedule.stacked_return_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5C9U, 0xC5CAU, 12U, checkpoint);
    append_checkpoint(0xC5CAU, 0xC5CCU, 24U, checkpoint);

    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(checkpoint.stack_pointer + 1U);
    checkpoint.accumulator = schedule.stacked_return_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5CCU, 0xC5CDU, 36U, checkpoint);
    append_checkpoint(0xC5CDU, 0xC5CFU, 48U, checkpoint);

    checkpoint.accumulator = schedule.countdown;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC5CFU, 0xC5D2U, 63U, checkpoint);

    if (schedule.decrements_countdown != 0U) {
        append_checkpoint(0xC5D2U, 0xC5D4U, 69U, checkpoint);
        checkpoint.status =
            set_nz(checkpoint.status, schedule.countdown_after);
        append_checkpoint(0xC5D4U, 0xC5D7U, 90U, checkpoint);
        if (schedule.installs_dispatch == 0U) {
            append_checkpoint(0xC5D7U, 0xC5E9U, 102U, output);
            return replacement;
        }
        append_checkpoint(0xC5D7U, 0xC5D9U, 96U, checkpoint);
    } else {
        append_checkpoint(0xC5D2U, 0xC5D9U, 75U, checkpoint);
    }

    const auto increment_low_cycle =
        static_cast<std::uint32_t>(93U + countdown_path_offset);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5D9U},
        .logical_address = 0x2026U,
        .value = schedule.dispatch_low,
        .master_cycle_offset = increment_low_cycle,
    });
    if (schedule.dispatch_low_wrapped != 0U) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC5DDU},
            .logical_address = 0x2027U,
            .value = schedule.dispatch_high,
            .master_cycle_offset =
                static_cast<std::uint32_t>(
                    117U + countdown_path_offset),
        });
    }
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5E1U},
        .logical_address = static_cast<std::uint16_t>(
            0x3F00U + entry.registers.x),
        .value = schedule.dispatch_low,
        .master_cycle_offset =
            static_cast<std::uint32_t>(
                132U + countdown_path_offset + carry_path_offset),
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC5E6U},
        .logical_address = static_cast<std::uint16_t>(
            0x3F80U + entry.registers.x),
        .value = schedule.dispatch_high,
        .master_cycle_offset =
            static_cast<std::uint32_t>(
                159U + countdown_path_offset + carry_path_offset),
    });
    replacement.body_master_cycles =
        replacement.work_ram_writes.back().master_cycle_offset;

    checkpoint.status =
        set_nz(checkpoint.status, schedule.dispatch_low);
    append_checkpoint(
        0xC5D9U,
        0xC5DBU,
        increment_low_cycle,
        checkpoint);

    const auto branch_cycle = static_cast<std::uint32_t>(
        increment_low_cycle
        + (schedule.dispatch_low_wrapped != 0U ? 6U : 12U));
    if (schedule.dispatch_low_wrapped != 0U) {
        append_checkpoint(0xC5DBU, 0xC5DDU, branch_cycle, checkpoint);
        checkpoint.status =
            set_nz(checkpoint.status, schedule.dispatch_high);
        append_checkpoint(
            0xC5DDU,
            0xC5DFU,
            branch_cycle + 18U,
            checkpoint);
    } else {
        append_checkpoint(0xC5DBU, 0xC5DFU, branch_cycle, checkpoint);
    }

    const auto load_low_cycle = static_cast<std::uint32_t>(
        117U + countdown_path_offset + carry_path_offset);
    checkpoint.accumulator = schedule.dispatch_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC5DFU,
        0xC5E1U,
        load_low_cycle,
        checkpoint);
    append_checkpoint(
        0xC5E1U,
        0xC5E4U,
        load_low_cycle + 15U,
        checkpoint);

    checkpoint.accumulator = schedule.dispatch_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC5E4U,
        0xC5E6U,
        load_low_cycle + 27U,
        checkpoint);
    append_checkpoint(
        0xC5E6U,
        0xC5E9U,
        replacement.body_master_cycles,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_active_nudge(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const dc_nudge_input_gate& gate)
{
    const bool active_start =
        gate.branch == DC_NUDGE_GATE_ACTIVE_START;
    const bool active_finish =
        gate.branch == DC_NUDGE_GATE_ACTIVE_FINISH;
    if ((!active_start && !active_finish)
        || (entry.registers.status & 0x28U) != 0U
        || entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    const auto read = [&work_ram](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset]);
    };
    const auto original_counter = read(0x0CC5U);
    const auto counter_after =
        static_cast<std::uint8_t>(original_counter + 1U);
    const std::uint8_t vector_low =
        active_start ? 0U : read(0x0CC2U);
    const std::uint8_t vector_high =
        active_start ? 1U : read(0x0CC3U);
    const auto scratch_high = static_cast<std::uint8_t>(
        (vector_high >> 1U)
        | ((vector_high & 0x80U) != 0U ? 0x80U : 0U));
    const auto scratch_low = static_cast<std::uint8_t>(
        (vector_low >> 1U)
        | ((vector_high & 0x01U) != 0U ? 0x80U : 0U));
    const auto motion_low = read(0x1500U);
    const auto motion_high = read(0x1580U);
    const auto motion_low_sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(motion_low) + scratch_low);
    const bool motion_carry = motion_low_sum > 0xFFU;
    const auto motion_low_after =
        static_cast<std::uint8_t>(motion_low_sum & 0xFFU);
    const auto motion_high_after = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(motion_high)
        + static_cast<std::uint16_t>(scratch_high)
        + static_cast<std::uint16_t>(motion_carry));

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc =
                active_start ? 0xC43EU : 0xC456U,
        },
        .output_registers = entry.registers,
        .body_master_cycles =
            active_start ? 762U : 522U,
    };
    const auto append_write =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t logical_address,
            const std::uint8_t value,
            const std::uint32_t master_cycle) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, instruction_pc},
                .logical_address = logical_address,
                .value = value,
                .master_cycle_offset = master_cycle,
            });
        };

    append_write(0xC3DDU, 0x2CC6U, gate.timer_after, 96U);
    if (active_start) {
        append_write(0xC3FFU, 0x2CC5U, counter_after, 222U);
        append_write(0xC40EU, 0x2CC2U, 0x00U, 276U);
        append_write(0xC413U, 0x2CC3U, 0x01U, 297U);
        append_write(0xC419U, 0x0000U, 0x0CU, 327U);
        append_write(0xC41EU, 0x0002U, 0x0FU, 351U);
        append_write(0xC423U, 0x0003U, 0x04U, 375U);
        append_write(
            0xC427U,
            static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            0xC4U,
            381U);
        append_write(
            0xC427U,
            static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            0x29U,
            381U);
        append_write(0xC466U, 0x202FU, vector_low, 477U);
        append_write(0xC46BU, 0x2030U, vector_high, 504U);
        append_write(0xC46EU, 0x2030U, scratch_high, 528U);
        append_write(0xC470U, 0x202FU, scratch_low, 546U);
        append_write(0xC479U, 0x3500U, motion_low_after, 600U);
        append_write(0xC481U, 0x3580U, motion_high_after, 642U);
        append_write(0xC42CU, 0x2CC4U, 0x01U, 684U);
        append_write(0xC434U, 0x2CC2U, 0x00U, 726U);
        append_write(0xC43BU, 0x2CC3U, 0xFFU, 762U);
    } else {
        append_write(0xC442U, 0x0000U, 0x0CU, 171U);
        append_write(0xC447U, 0x0002U, 0x0FU, 195U);
        append_write(0xC44CU, 0x0003U, 0x02U, 219U);
        append_write(
            0xC450U,
            static_cast<std::uint16_t>(
                0x2100U + entry.registers.stack_pointer),
            0xC4U,
            225U);
        append_write(
            0xC450U,
            static_cast<std::uint16_t>(
                0x2100U
                + static_cast<std::uint8_t>(
                    entry.registers.stack_pointer - 1U)),
            0x52U,
            225U);
        append_write(0xC466U, 0x202FU, vector_low, 321U);
        append_write(0xC46BU, 0x2030U, vector_high, 348U);
        append_write(0xC46EU, 0x2030U, scratch_high, 372U);
        append_write(0xC470U, 0x202FU, scratch_low, 390U);
        append_write(0xC479U, 0x3500U, motion_low_after, 444U);
        append_write(0xC481U, 0x3580U, motion_high_after, 486U);
        append_write(0xC453U, 0x2CC4U, 0x00U, 522U);
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto compare_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(
                status,
                static_cast<std::uint8_t>(left - right));
            if (left >= right) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto adc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto sum = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(left)
                + static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto sbc_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right,
            const bool carry_in,
            const std::uint8_t result) {
            const auto subtrahend = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right)
                + static_cast<std::uint16_t>(carry_in ? 0U : 1U));
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xC1U));
            status = set_nz(status, result);
            if (static_cast<std::uint16_t>(left) >= subtrahend) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            if (((left ^ result) & (left ^ right) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x40U);
            }
            return status;
        };
    const auto shift_left_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto rotate_right_status =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t original,
            const std::uint8_t result) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x01U));
            status = set_nz(status, result);
            if ((original & 0x01U) != 0U) {
                status = static_cast<std::uint8_t>(status | 0x01U);
            }
            return status;
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CBU, 0xC3CDU, 12U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.session_flags & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CDU, 0xC3CFU, 18U, checkpoint);
    append_checkpoint(0xC3CFU, 0xC3D1U, 24U, checkpoint);
    checkpoint.accumulator = gate.tilt_lock;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D1U, 0xC3D4U, 39U, checkpoint);
    append_checkpoint(0xC3D4U, 0xC3D6U, 45U, checkpoint);
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D6U, 0xC3D8U, 57U, checkpoint);
    checkpoint.accumulator = gate.session_mask;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D8U, 0xC3DAU, 63U, checkpoint);
    append_checkpoint(0xC3DAU, 0xC3DDU, 75U, checkpoint);
    checkpoint.status = set_nz(checkpoint.status, gate.timer_after);
    append_checkpoint(0xC3DDU, 0xC3E0U, 96U, checkpoint);
    append_checkpoint(0xC3E0U, 0xC3EAU, 108U, checkpoint);
    checkpoint.accumulator = gate.nudge_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3EAU, 0xC3EDU, 123U, checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.nudge_phase - 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3EDU, 0xC3EEU, 129U, checkpoint);

    if (active_finish) {
        append_checkpoint(0xC3EEU, 0xC43FU, 141U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x04U);
        append_checkpoint(0xC43FU, 0xC440U, 147U, checkpoint);
        checkpoint.accumulator = 0x0CU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC440U, 0xC442U, 153U, checkpoint);
        append_checkpoint(0xC442U, 0xC445U, 171U, checkpoint);
        checkpoint.accumulator = 0x0FU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC445U, 0xC447U, 177U, checkpoint);
        append_checkpoint(0xC447U, 0xC44AU, 195U, checkpoint);
        checkpoint.accumulator = 0x02U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC44AU, 0xC44CU, 201U, checkpoint);
        append_checkpoint(0xC44CU, 0xC44FU, 219U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status & ~0x04U);
        append_checkpoint(0xC44FU, 0xC450U, 225U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 2U);
        append_checkpoint(0xC450U, 0xC457U, 246U, checkpoint);
    } else {
        append_checkpoint(0xC3EEU, 0xC3F0U, 135U, checkpoint);
        checkpoint.y = gate.active_player;
        checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
        append_checkpoint(0xC3F0U, 0xC3F3U, 150U, checkpoint);
        checkpoint.accumulator = gate.button_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3F3U, 0xC3F6U, 165U, checkpoint);
        checkpoint.accumulator = gate.matched_input;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3F6U, 0xC3F8U, 171U, checkpoint);
        append_checkpoint(0xC3F8U, 0xC3FBU, 183U, checkpoint);

        checkpoint.accumulator = read(0x0055U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3FBU, 0xC3FDU, 195U, checkpoint);
        append_checkpoint(0xC3FDU, 0xC3FFU, 201U, checkpoint);
        checkpoint.status = set_nz(checkpoint.status, counter_after);
        append_checkpoint(0xC3FFU, 0xC402U, 222U, checkpoint);
        checkpoint.accumulator = counter_after;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC402U, 0xC405U, 237U, checkpoint);
        checkpoint.status = compare_status(
            checkpoint.status,
            checkpoint.accumulator,
            0x14U);
        append_checkpoint(0xC405U, 0xC407U, 243U, checkpoint);
        append_checkpoint(0xC407U, 0xC40CU, 255U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC40CU, 0xC40EU, 261U, checkpoint);
        append_checkpoint(0xC40EU, 0xC411U, 276U, checkpoint);
        checkpoint.accumulator = 1U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC411U, 0xC413U, 282U, checkpoint);
        append_checkpoint(0xC413U, 0xC416U, 297U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x04U);
        append_checkpoint(0xC416U, 0xC417U, 303U, checkpoint);
        checkpoint.accumulator = 0x0CU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC417U, 0xC419U, 309U, checkpoint);
        append_checkpoint(0xC419U, 0xC41CU, 327U, checkpoint);
        checkpoint.accumulator = 0x0FU;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC41CU, 0xC41EU, 333U, checkpoint);
        append_checkpoint(0xC41EU, 0xC421U, 351U, checkpoint);
        checkpoint.accumulator = 0x04U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC421U, 0xC423U, 357U, checkpoint);
        append_checkpoint(0xC423U, 0xC426U, 375U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status & ~0x04U);
        append_checkpoint(0xC426U, 0xC427U, 381U, checkpoint);
        checkpoint.stack_pointer = static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 2U);
        append_checkpoint(0xC427U, 0xC457U, 402U, checkpoint);
    }

    const auto helper_offset = active_start ? 156U : 0U;
    checkpoint.accumulator = read(0x1000U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC457U,
        0xC45AU,
        261U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC45AU,
        0xC45CU,
        267U + helper_offset,
        checkpoint);
    checkpoint.accumulator = read(0x1C80U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC45CU,
        0xC45FU,
        282U + helper_offset,
        checkpoint);
    checkpoint.accumulator =
        static_cast<std::uint8_t>(checkpoint.accumulator & 0x01U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC45FU,
        0xC461U,
        288U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC461U,
        0xC463U,
        294U + helper_offset,
        checkpoint);
    checkpoint.accumulator = vector_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC463U,
        0xC466U,
        309U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC466U,
        0xC468U,
        321U + helper_offset,
        checkpoint);
    checkpoint.accumulator = vector_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC468U,
        0xC46BU,
        336U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC46BU,
        0xC46DU,
        348U + helper_offset,
        checkpoint);

    const auto shifted_high =
        static_cast<std::uint8_t>(vector_high << 1U);
    checkpoint.accumulator = shifted_high;
    checkpoint.status = shift_left_status(
        checkpoint.status,
        vector_high,
        checkpoint.accumulator);
    append_checkpoint(
        0xC46DU,
        0xC46EU,
        354U + helper_offset,
        checkpoint);
    checkpoint.status = rotate_right_status(
        checkpoint.status,
        vector_high,
        scratch_high);
    append_checkpoint(
        0xC46EU,
        0xC470U,
        372U + helper_offset,
        checkpoint);
    checkpoint.status = rotate_right_status(
        checkpoint.status,
        vector_low,
        scratch_low);
    append_checkpoint(
        0xC470U,
        0xC472U,
        390U + helper_offset,
        checkpoint);

    checkpoint.x = 0U;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.x);
    append_checkpoint(
        0xC472U,
        0xC473U,
        396U + helper_offset,
        checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status & ~0x01U);
    append_checkpoint(
        0xC473U,
        0xC474U,
        402U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_low;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC474U,
        0xC477U,
        417U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_low_after;
    checkpoint.status = adc_status(
        checkpoint.status,
        motion_low,
        scratch_low,
        false,
        checkpoint.accumulator);
    append_checkpoint(
        0xC477U,
        0xC479U,
        429U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC479U,
        0xC47CU,
        444U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_high;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC47CU,
        0xC47FU,
        459U + helper_offset,
        checkpoint);
    checkpoint.accumulator = motion_high_after;
    checkpoint.status = adc_status(
        checkpoint.status,
        motion_high,
        scratch_high,
        motion_carry,
        checkpoint.accumulator);
    append_checkpoint(
        0xC47FU,
        0xC481U,
        471U + helper_offset,
        checkpoint);
    append_checkpoint(
        0xC481U,
        0xC484U,
        486U + helper_offset,
        checkpoint);
    checkpoint.stack_pointer = static_cast<std::uint8_t>(
        checkpoint.stack_pointer + 2U);
    append_checkpoint(
        0xC484U,
        active_start ? 0xC42AU : 0xC453U,
        507U + helper_offset,
        checkpoint);

    if (active_finish) {
        append_checkpoint(0xC453U, 0xC456U, 522U, checkpoint);
    } else {
        checkpoint.accumulator = 1U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC42AU, 0xC42CU, 669U, checkpoint);
        append_checkpoint(0xC42CU, 0xC42FU, 684U, checkpoint);
        checkpoint.status =
            static_cast<std::uint8_t>(checkpoint.status | 0x01U);
        append_checkpoint(0xC42FU, 0xC430U, 690U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC430U, 0xC431U, 696U, checkpoint);
        auto subtraction = static_cast<std::uint8_t>(
            checkpoint.accumulator - 0U);
        checkpoint.status = sbc_status(
            checkpoint.status,
            checkpoint.accumulator,
            0U,
            true,
            subtraction);
        checkpoint.accumulator = subtraction;
        append_checkpoint(0xC431U, 0xC434U, 711U, checkpoint);
        append_checkpoint(0xC434U, 0xC437U, 726U, checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC437U, 0xC438U, 732U, checkpoint);
        subtraction = 0xFFU;
        checkpoint.status = sbc_status(
            checkpoint.status,
            checkpoint.accumulator,
            1U,
            true,
            subtraction);
        checkpoint.accumulator = subtraction;
        append_checkpoint(0xC438U, 0xC43BU, 747U, checkpoint);
        append_checkpoint(0xC43BU, 0xC43EU, 762U, checkpoint);
    }

    checkpoint.program_counter =
        replacement.return_instruction.logical_pc;
    replacement.output_registers = checkpoint;
    replacement.checkpoints.back().registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_nudge_input_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC3CBU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_nudge_input_gate gate{};
    if (!dc_evaluate_nudge_input_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native C3CB replacement rejected the work-RAM snapshot.");
    }
    if (gate.branch == DC_NUDGE_GATE_DEFER) {
        return std::nullopt;
    }
    if (const auto active = replace_active_nudge(
            entry,
            work_ram,
            gate)) {
        return active;
    }

    auto output = entry.registers;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x82U));
    const auto session_return =
        gate.branch == DC_NUDGE_GATE_SESSION_RETURN;
    const auto timer_reload_return =
        gate.branch == DC_NUDGE_GATE_TIMER_RELOAD_RETURN;
    if (gate.branch == DC_NUDGE_GATE_SESSION_RETURN) {
        output.program_counter = 0xC3DCU;
        output.accumulator = gate.session_mask;
        if ((output.accumulator & 0x80U) != 0U) {
            output.status = static_cast<std::uint8_t>(
                output.status | 0x80U);
        }
    } else {
        output.program_counter = 0xC3FAU;
        output.accumulator = gate.matched_input;
        output.y = gate.active_player;
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = session_return ? 0xC3DCU : 0xC3FAU,
        },
        .output_registers = output,
        .body_master_cycles =
            session_return ? 69U : (timer_reload_return ? 207U : 177U),
    };
    if (!session_return) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC3DDU},
            .logical_address = 0x2CC6U,
            .value = gate.timer_after,
            .master_cycle_offset = 96U,
        });
        if (timer_reload_return) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, 0xC3E4U},
                .logical_address = 0x2CC6U,
                .value = 0x78U,
                .master_cycle_offset = 123U,
            });
            replacement.work_ram_writes.push_back({
                .instruction = {0x01U, 0xC3E7U},
                .logical_address = 0x2CC5U,
                .value = 0x00U,
                .master_cycle_offset = 138U,
            });
        }
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CBU, 0xC3CDU, 12U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.session_flags & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3CDU, 0xC3CFU, 18U, checkpoint);
    append_checkpoint(0xC3CFU, 0xC3D1U, 24U, checkpoint);

    checkpoint.accumulator = gate.tilt_lock;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D1U, 0xC3D4U, 39U, checkpoint);
    append_checkpoint(0xC3D4U, 0xC3D6U, 45U, checkpoint);

    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D6U, 0xC3D8U, 57U, checkpoint);

    checkpoint.accumulator = gate.session_mask;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC3D8U, 0xC3DAU, 63U, checkpoint);
    if (session_return) {
        append_checkpoint(0xC3DAU, 0xC3DCU, 69U, output);
        return replacement;
    }
    append_checkpoint(0xC3DAU, 0xC3DDU, 75U, checkpoint);

    checkpoint.status = set_nz(checkpoint.status, gate.timer_after);
    append_checkpoint(0xC3DDU, 0xC3E0U, 96U, checkpoint);
    if (timer_reload_return) {
        append_checkpoint(0xC3E0U, 0xC3E2U, 102U, checkpoint);
        checkpoint.accumulator = 0x78U;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xC3E2U, 0xC3E4U, 108U, checkpoint);
        append_checkpoint(0xC3E4U, 0xC3E7U, 123U, checkpoint);
        append_checkpoint(0xC3E7U, 0xC3EAU, 138U, checkpoint);
    } else {
        append_checkpoint(0xC3E0U, 0xC3EAU, 108U, checkpoint);
    }

    const auto continuation_offset =
        timer_reload_return ? 30U : 0U;
    checkpoint.accumulator = gate.nudge_phase;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3EAU,
        0xC3EDU,
        123U + continuation_offset,
        checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.nudge_phase - 1U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3EDU,
        0xC3EEU,
        129U + continuation_offset,
        checkpoint);
    append_checkpoint(
        0xC3EEU,
        0xC3F0U,
        135U + continuation_offset,
        checkpoint);

    checkpoint.y = gate.active_player;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(
        0xC3F0U,
        0xC3F3U,
        150U + continuation_offset,
        checkpoint);

    checkpoint.accumulator = gate.button_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3F3U,
        0xC3F6U,
        165U + continuation_offset,
        checkpoint);

    checkpoint.accumulator = gate.matched_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        0xC3F6U,
        0xC3F8U,
        171U + continuation_offset,
        checkpoint);
    append_checkpoint(
        0xC3F8U,
        0xC3FAU,
        177U + continuation_offset,
        output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_session_input_gate(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC49EU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_session_input_gate gate{};
    if (!dc_evaluate_session_input_gate(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            &gate)) {
        throw std::runtime_error(
            "Native C49E replacement rejected the work-RAM snapshot.");
    }
    if (gate.returns_idle == 0U) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.program_counter = 0xC4C8U;
    output.accumulator = gate.run_input;
    output.y = gate.active_player;
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0x82U));
    output.status = static_cast<std::uint8_t>(output.status | 0x02U);

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC4C8U,
        },
        .output_registers = output,
        .body_master_cycles = 72U,
    };

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = gate.session_flags;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC49EU, 0xC4A0U, 12U, checkpoint);

    checkpoint.accumulator =
        static_cast<std::uint8_t>(gate.session_flags & 0x02U);
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC4A0U, 0xC4A2U, 18U, checkpoint);
    append_checkpoint(0xC4A2U, 0xC4A4U, 24U, checkpoint);

    checkpoint.y = gate.active_player;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xC4A4U, 0xC4A7U, 39U, checkpoint);

    checkpoint.accumulator = gate.button_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC4A7U, 0xC4AAU, 54U, checkpoint);

    checkpoint.accumulator = gate.run_input;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC4AAU, 0xC4ACU, 60U, checkpoint);
    append_checkpoint(0xC4ACU, 0xC4C8U, 72U, output);
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_camera_activation(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF4FCU,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_actor_camera_activation activation{};
    if (!dc_classify_actor_camera_activation(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &activation)) {
        throw std::runtime_error(
            "Native F4FC replacement rejected the work-RAM snapshot.");
    }

    const auto return_pc =
        activation.path == DC_ACTOR_CAMERA_BELOW
        ? 0xF523U
        : (activation.path == DC_ACTOR_CAMERA_ABOVE
            ? 0xF545U
            : 0xF54EU);
    const auto body_cycles =
        activation.path == DC_ACTOR_CAMERA_EMPTY
        ? 27U
        : (activation.path == DC_ACTOR_CAMERA_BELOW
            ? 189U
            : (activation.path == DC_ACTOR_CAMERA_ABOVE
                ? 327U
                : 333U));

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = static_cast<std::uint16_t>(return_pc),
        },
        .output_registers = entry.registers,
        .body_master_cycles = body_cycles,
    };
    replacement.work_ram_writes.reserve(
        activation.path == DC_ACTOR_CAMERA_EMPTY
        ? 0U
        : (activation.path == DC_ACTOR_CAMERA_BELOW ? 3U : 5U));
    if (activation.path != DC_ACTOR_CAMERA_EMPTY) {
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF506U},
            .logical_address = 0x2035U,
            .value = activation.lower_low,
            .master_cycle_offset = 57U,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x00U, 0xF50CU},
            .logical_address = 0x2036U,
            .value = activation.lower_high,
            .master_cycle_offset = 87U,
        });
        if (activation.path != DC_ACTOR_CAMERA_BELOW) {
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, 0xF529U},
                .logical_address = 0x2035U,
                .value = activation.upper_low,
                .master_cycle_offset = 195U,
            });
            replacement.work_ram_writes.push_back({
                .instruction = {0x00U, 0xF52EU},
                .logical_address = 0x2036U,
                .value = activation.upper_high,
                .master_cycle_offset = 225U,
            });
        }
        replacement.work_ram_writes.push_back({
            .instruction = {
                0x00U,
                activation.path == DC_ACTOR_CAMERA_BELOW
                    ? 0xF520U
                    : (activation.path == DC_ACTOR_CAMERA_ABOVE
                        ? 0xF542U
                        : 0xF54BU),
            },
            .logical_address = static_cast<std::uint16_t>(
                0x3080U + entry.registers.x),
            .value = activation.flags_after,
            .master_cycle_offset = body_cycles,
        });
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xA2U));
            if (value == 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x80U);
            }
            return status;
        };
    const auto sbc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto difference =
                static_cast<int>(left)
                - static_cast<int>(right)
                - ((status & 0x01U) != 0U ? 0 : 1);
            const auto result =
                static_cast<std::uint8_t>(difference);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x41U));
            status = set_nz(status, result);
            if (difference >= 0) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if ((((left ^ result) & (left ^ right)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return std::pair{result, status};
        };
    const auto adc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto sum =
                static_cast<unsigned int>(left)
                + static_cast<unsigned int>(right)
                + ((status & 0x01U) != 0U ? 1U : 0U);
            const auto result =
                static_cast<std::uint8_t>(sum);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x41U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return std::pair{result, status};
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.accumulator = activation.occupancy;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF4FCU, 0xF4FFU, 15U, checkpoint);
    if (activation.path == DC_ACTOR_CAMERA_EMPTY) {
        append_checkpoint(0xF4FFU, 0xF54EU, 27U, checkpoint);
        checkpoint.program_counter = 0xF54EU;
        replacement.output_registers = checkpoint;
        return replacement;
    }

    append_checkpoint(0xF4FFU, 0xF501U, 21U, checkpoint);
    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF501U, 0xF502U, 27U, checkpoint);

    checkpoint.accumulator = activation.anchor_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF502U, 0xF504U, 39U, checkpoint);
    auto arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        0x30U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF504U, 0xF506U, 45U, checkpoint);
    append_checkpoint(0xF506U, 0xF508U, 57U, checkpoint);

    checkpoint.accumulator = activation.anchor_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF508U, 0xF50AU, 69U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        0x00U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF50AU, 0xF50CU, 75U, checkpoint);
    append_checkpoint(0xF50CU, 0xF50EU, 87U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF50EU, 0xF50FU, 93U, checkpoint);
    checkpoint.accumulator = activation.position_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF50FU, 0xF512U, 108U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.lower_low);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF512U, 0xF514U, 120U, checkpoint);
    checkpoint.accumulator = activation.position_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF514U, 0xF517U, 135U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.lower_high);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF517U, 0xF519U, 147U, checkpoint);

    if (activation.path == DC_ACTOR_CAMERA_BELOW) {
        append_checkpoint(0xF519U, 0xF51BU, 153U, checkpoint);
        checkpoint.accumulator = activation.flags_before;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xF51BU, 0xF51EU, 168U, checkpoint);
        checkpoint.accumulator = activation.flags_after;
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(0xF51EU, 0xF520U, 174U, checkpoint);
        append_checkpoint(0xF520U, 0xF523U, 189U, checkpoint);
        checkpoint.program_counter = 0xF523U;
        replacement.output_registers = checkpoint;
        return replacement;
    }

    append_checkpoint(0xF519U, 0xF524U, 159U, checkpoint);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x01U));
    append_checkpoint(0xF524U, 0xF525U, 165U, checkpoint);
    checkpoint.accumulator = activation.anchor_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF525U, 0xF527U, 177U, checkpoint);
    arithmetic = adc(
        checkpoint.status,
        checkpoint.accumulator,
        0xF0U);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF527U, 0xF529U, 183U, checkpoint);
    append_checkpoint(0xF529U, 0xF52BU, 195U, checkpoint);
    checkpoint.accumulator = 0U;
    append_checkpoint(0xF52BU, 0xF52CU, 201U, checkpoint);
    arithmetic = adc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.anchor_high);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF52CU, 0xF52EU, 213U, checkpoint);
    append_checkpoint(0xF52EU, 0xF530U, 225U, checkpoint);

    checkpoint.status =
        static_cast<std::uint8_t>(checkpoint.status | 0x01U);
    append_checkpoint(0xF530U, 0xF531U, 231U, checkpoint);
    checkpoint.accumulator = activation.position_low;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF531U, 0xF534U, 246U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.upper_low);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF534U, 0xF536U, 258U, checkpoint);
    checkpoint.accumulator = activation.position_high;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xF536U, 0xF539U, 273U, checkpoint);
    arithmetic = sbc(
        checkpoint.status,
        checkpoint.accumulator,
        activation.upper_high);
    checkpoint.accumulator = arithmetic.first;
    checkpoint.status = arithmetic.second;
    append_checkpoint(0xF539U, 0xF53BU, 285U, checkpoint);

    const bool inside =
        activation.path == DC_ACTOR_CAMERA_INSIDE;
    append_checkpoint(
        0xF53BU,
        inside ? 0xF546U : 0xF53DU,
        inside ? 297U : 291U,
        checkpoint);
    checkpoint.accumulator = activation.flags_before;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        inside ? 0xF546U : 0xF53DU,
        inside ? 0xF549U : 0xF540U,
        inside ? 312U : 306U,
        checkpoint);
    checkpoint.accumulator = activation.flags_after;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(
        inside ? 0xF549U : 0xF540U,
        inside ? 0xF54BU : 0xF542U,
        inside ? 318U : 312U,
        checkpoint);
    append_checkpoint(
        inside ? 0xF54BU : 0xF542U,
        static_cast<std::uint16_t>(return_pc),
        body_cycles,
        checkpoint);
    checkpoint.program_counter =
        static_cast<std::uint16_t>(return_pc);
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_actor_slot_reset(
    const RoutineEntry& entry)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x01U,
        .logical_pc = 0xC574U,
    };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }

    dc_actor_slot_reset reset{};
    if (!dc_prepare_actor_slot_reset(
            entry.registers.x,
            &reset)) {
        return std::nullopt;
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x01U,
            .logical_pc = 0xC594U,
        },
        .output_registers = entry.registers,
        .body_master_cycles = 3627U,
    };
    replacement.work_ram_writes.reserve(
        3U + 3U * DC_ACTOR_SLOT_FIELD_COUNT);
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC574U},
        .logical_address = static_cast<std::uint16_t>(
            0x2100U + entry.registers.stack_pointer),
        .value = entry.registers.y,
        .master_cycle_offset = 9U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC57AU},
        .logical_address = 0x202DU,
        .value = 0U,
        .master_cycle_offset = 42U,
    });
    replacement.work_ram_writes.push_back({
        .instruction = {0x01U, 0xC57EU},
        .logical_address = 0x202EU,
        .value = 0x30U,
        .master_cycle_offset = 60U,
    });
    for (std::size_t index = 0U;
         index < DC_ACTOR_SLOT_FIELD_COUNT;
         ++index) {
        const auto iteration_offset =
            static_cast<std::uint32_t>(111U * index);
        const auto pointer_after = static_cast<std::uint16_t>(
            0x3000U + 0x80U * (index + 1U));
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC581U},
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + reset.work_ram_offsets[index]),
            .value = 0U,
            .master_cycle_offset = 87U + iteration_offset,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC588U},
            .logical_address = 0x202DU,
            .value = static_cast<std::uint8_t>(pointer_after),
            .master_cycle_offset = 123U + iteration_offset,
        });
        replacement.work_ram_writes.push_back({
            .instruction = {0x01U, 0xC58DU},
            .logical_address = 0x202EU,
            .value = static_cast<std::uint8_t>(
                pointer_after >> 8U),
            .master_cycle_offset = 153U + iteration_offset,
        });
    }

    const auto set_nz =
        [](std::uint8_t status, const std::uint8_t value) {
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0xA2U));
            if (value == 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x02U);
            } else if ((value & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x80U);
            }
            return status;
        };
    const auto adc =
        [&set_nz](
            std::uint8_t status,
            const std::uint8_t left,
            const std::uint8_t right) {
            const auto sum =
                static_cast<unsigned int>(left)
                + static_cast<unsigned int>(right)
                + ((status & 0x01U) != 0U ? 1U : 0U);
            const auto result =
                static_cast<std::uint8_t>(sum);
            status = static_cast<std::uint8_t>(
                status & static_cast<std::uint8_t>(~0x41U));
            status = set_nz(status, result);
            if (sum > 0xFFU) {
                status = static_cast<std::uint8_t>(
                    status | 0x01U);
            }
            if (((~(left ^ right) & (left ^ result)) & 0x80U) != 0U) {
                status = static_cast<std::uint8_t>(
                    status | 0x40U);
            }
            return std::pair{result, status};
        };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x01U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(
            checkpoint.stack_pointer - 1U);
    checkpoint.status = static_cast<std::uint8_t>(
        checkpoint.status & static_cast<std::uint8_t>(~0x20U));
    append_checkpoint(0xC574U, 0xC575U, 9U, checkpoint);
    checkpoint.y = DC_ACTOR_SLOT_FIELD_COUNT;
    checkpoint.status = set_nz(checkpoint.status, checkpoint.y);
    append_checkpoint(0xC575U, 0xC577U, 15U, checkpoint);
    std::swap(checkpoint.x, checkpoint.y);
    append_checkpoint(0xC577U, 0xC578U, 24U, checkpoint);
    checkpoint.accumulator = 0U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC578U, 0xC57AU, 30U, checkpoint);
    append_checkpoint(0xC57AU, 0xC57CU, 42U, checkpoint);
    checkpoint.accumulator = 0x30U;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.accumulator);
    append_checkpoint(0xC57CU, 0xC57EU, 48U, checkpoint);
    append_checkpoint(0xC57EU, 0xC580U, 60U, checkpoint);

    std::uint16_t pointer = 0x3000U;
    for (std::size_t index = 0U;
         index < DC_ACTOR_SLOT_FIELD_COUNT;
         ++index) {
        const auto iteration_offset =
            static_cast<std::uint32_t>(111U * index);
        checkpoint.accumulator = 0U;
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x20U));
        append_checkpoint(
            0xC580U,
            0xC581U,
            66U + iteration_offset,
            checkpoint);
        append_checkpoint(
            0xC581U,
            0xC583U,
            87U + iteration_offset,
            checkpoint);
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status
            & static_cast<std::uint8_t>(~0x21U));
        append_checkpoint(
            0xC583U,
            0xC584U,
            93U + iteration_offset,
            checkpoint);
        checkpoint.accumulator =
            static_cast<std::uint8_t>(pointer);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            0xC584U,
            0xC586U,
            105U + iteration_offset,
            checkpoint);
        auto arithmetic = adc(
            checkpoint.status,
            checkpoint.accumulator,
            0x80U);
        checkpoint.accumulator = arithmetic.first;
        checkpoint.status = arithmetic.second;
        append_checkpoint(
            0xC586U,
            0xC588U,
            111U + iteration_offset,
            checkpoint);
        append_checkpoint(
            0xC588U,
            0xC58AU,
            123U + iteration_offset,
            checkpoint);
        checkpoint.accumulator = 0U;
        checkpoint.status = static_cast<std::uint8_t>(
            checkpoint.status & static_cast<std::uint8_t>(~0x20U));
        append_checkpoint(
            0xC58AU,
            0xC58BU,
            129U + iteration_offset,
            checkpoint);
        arithmetic = adc(
            checkpoint.status,
            checkpoint.accumulator,
            static_cast<std::uint8_t>(pointer >> 8U));
        pointer = static_cast<std::uint16_t>(
            pointer + 0x80U);
        checkpoint.accumulator = arithmetic.first;
        checkpoint.status = arithmetic.second;
        append_checkpoint(
            0xC58BU,
            0xC58DU,
            141U + iteration_offset,
            checkpoint);
        append_checkpoint(
            0xC58DU,
            0xC58FU,
            153U + iteration_offset,
            checkpoint);
        checkpoint.x =
            static_cast<std::uint8_t>(checkpoint.x - 1U);
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.x);
        append_checkpoint(
            0xC58FU,
            0xC590U,
            159U + iteration_offset,
            checkpoint);
        const bool repeats =
            index + 1U < DC_ACTOR_SLOT_FIELD_COUNT;
        append_checkpoint(
            0xC590U,
            repeats ? 0xC580U : 0xC592U,
            repeats
                ? 171U + iteration_offset
                : 3606U,
            checkpoint);
    }

    std::swap(checkpoint.x, checkpoint.y);
    append_checkpoint(0xC592U, 0xC593U, 3615U, checkpoint);
    checkpoint.stack_pointer =
        static_cast<std::uint8_t>(
            checkpoint.stack_pointer + 1U);
    checkpoint.y = entry.registers.y;
    checkpoint.status =
        set_nz(checkpoint.status, checkpoint.y);
    checkpoint.program_counter = 0xC594U;
    append_checkpoint(0xC593U, 0xC594U, 3627U, checkpoint);
    replacement.output_registers = checkpoint;
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_cached_position_restore(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x00U,
        .logical_pc = 0xF241U,
    };
    constexpr std::array<std::uint16_t, DC_CACHED_POSITION_BYTE_COUNT>
        kStoreInstructionPcs{
            0xF243U,
            0xF248U,
            0xF24DU,
            0xF252U,
            0xF257U,
            0xF25CU,
        };
    if (entry.routine != kRoutine) {
        return std::nullopt;
    }

    dc_cached_position_restore restored{};
    if (!dc_restore_cached_position(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            entry.registers.x,
            &restored)) {
        throw std::runtime_error(
            "Native F241 replacement rejected the work-RAM snapshot.");
    }

    auto output = entry.registers;
    output.program_counter = 0xF25FU;
    output.accumulator =
        restored.values[DC_CACHED_POSITION_BYTE_COUNT - 1U];
    output.status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~(0x80U | 0x02U)));
    if (output.accumulator == 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x02U);
    }
    if ((output.accumulator & 0x80U) != 0U) {
        output.status = static_cast<std::uint8_t>(
            output.status | 0x80U);
    }

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = 0x00U,
            .logical_pc = 0xF25FU,
        },
        .output_registers = output,
        .body_master_cycles = 162U,
    };
    replacement.work_ram_writes.reserve(
        DC_CACHED_POSITION_BYTE_COUNT);
    for (std::size_t index = 0;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        replacement.work_ram_writes.push_back({
            .instruction = {
                .physical_bank = 0x00U,
                .logical_pc = kStoreInstructionPcs[index],
            },
            .logical_address = static_cast<std::uint16_t>(
                0x2000U + restored.work_ram_offsets[index]),
            .value = restored.values[index],
            .master_cycle_offset =
                static_cast<std::uint32_t>(27U * (index + 1U)),
        });
    }

    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(
            status & static_cast<std::uint8_t>(~0xA2U));
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        } else if ((value & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x80U);
        }
        return status;
    };
    const auto append_checkpoint =
        [&replacement](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t master_cycle,
            auto registers) {
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset =
                    master_cycle - 3U,
                .master_cycle_offset = master_cycle,
                .instruction = {0x00U, instruction_pc},
                .registers = registers,
            });
        };

    auto checkpoint = entry.registers;
    for (std::size_t index = 0;
         index < DC_CACHED_POSITION_BYTE_COUNT;
         ++index) {
        const auto load_pc = static_cast<std::uint16_t>(
            0xF241U + 5U * index);
        const auto store_pc = kStoreInstructionPcs[index];
        const auto load_cycle = static_cast<std::uint32_t>(
            12U + 27U * index);
        const auto store_cycle = static_cast<std::uint32_t>(
            27U * (index + 1U));

        checkpoint.accumulator = restored.values[index];
        checkpoint.status =
            set_nz(checkpoint.status, checkpoint.accumulator);
        append_checkpoint(
            load_pc,
            store_pc,
            load_cycle,
            checkpoint);
        append_checkpoint(
            store_pc,
            static_cast<std::uint16_t>(store_pc + 3U),
            store_cycle,
            index + 1U == DC_CACHED_POSITION_BYTE_COUNT
                ? output
                : checkpoint);
    }
    return replacement;
}

[[nodiscard]] std::optional<RoutineReplacement>
replace_native_gameplay_routine(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    std::optional<RoutineReplacement> replacement;
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF1B4U,
        }) {
        replacement = replace_position_integration(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD081U,
        }) {
        replacement =
            replace_flipper_geometry_lookup(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        }) {
        replacement = replace_coroutine_scheduler(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC3CBU,
        }) {
        replacement = replace_nudge_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC49EU,
        }) {
        replacement = replace_session_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD466U,
        }) {
        replacement = replace_flipper_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC1AU,
        }) {
        replacement =
            replace_collision_scan_vertical_reject(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCCD5U,
        }) {
        replacement = replace_collision_geometry(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD500U,
        }) {
        replacement = replace_flipper_phase_step(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD1A2U,
        }) {
        replacement = replace_motion_rotation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCFD8U,
        }) {
        replacement =
            replace_collision_motion_adjustment(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD09EU,
        }) {
        replacement = replace_flipper_projection(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD612U,
        }) {
        replacement =
            replace_flipper_radius_projection(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD62EU,
        }) {
        replacement =
            replace_flipper_angle_projection(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xDD09U,
        }) {
        replacement = replace_motion_damping(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF55CU,
        }) {
        replacement = replace_score_accumulator(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE8B3U,
        }) {
        replacement = replace_rng_update(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE837U,
        }) {
        replacement = replace_signed_word_negation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE905U,
        }) {
        replacement = replace_signed_division(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE624U,
        }) {
        replacement = replace_unsigned_multiply_round(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        }) {
        replacement = replace_signed_fixed_multiply(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6966U,
        }) {
        replacement =
            replace_collision_table_interpreter(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7FB1U,
        }) {
        replacement = replace_collision_response(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7F57U,
        }) {
        replacement =
            replace_collision_consumer_no_hit(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF4FCU,
        }) {
        replacement =
            replace_actor_camera_activation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC574U,
        }) {
        replacement = replace_actor_slot_reset(entry);
    } else {
        replacement = replace_cached_position_restore(entry, work_ram);
    }
    return replacement;
}

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
    SourceTexture backglass_source;
    StageTexture backglass_stage;
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
                    aspect_denominator);
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
                    : backglass_source.texture != nullptr
                        ? present_frame(
                            backglass.renderer,
                            backglass_source,
                            backglass_stage,
                            static_cast<std::uint32_t>(
                                backglass_source.width),
                            static_cast<std::uint32_t>(
                                backglass_source.height),
                            {0U, 0U, 0U})
                        : present_black(backglass))) {
            std::cerr
                << "Cabinet backglass renderer lost; disabling "
                   "backglass.\n";
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
