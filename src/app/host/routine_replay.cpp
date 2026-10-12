// SPDX-License-Identifier: GPL-3.0-or-later
#include "routine_replay.hpp"

#include "command_line.hpp"
#include "host_files.hpp"
#include "recording_plans.hpp"

namespace devils_crush::app::host {

namespace {

constexpr std::uintmax_t kMaximumRoutineReplayCorpusFileSize =
    96U * 1024U * 1024U + 4096U;

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
    if (original.output_registers != replayed.output_registers) {
        const auto print_registers = [](
            const devils_crush::emulation::CpuRegisters& registers) {
            std::ostringstream output;
            output << std::uppercase << std::hex << std::setfill('0')
                   << "A=" << std::setw(2)
                   << static_cast<unsigned>(registers.accumulator)
                   << " X=" << std::setw(2)
                   << static_cast<unsigned>(registers.x)
                   << " Y=" << std::setw(2)
                   << static_cast<unsigned>(registers.y)
                   << " SP=" << std::setw(2)
                   << static_cast<unsigned>(registers.stack_pointer)
                   << " P=" << std::setw(2)
                   << static_cast<unsigned>(registers.status)
                   << " PC=" << std::setw(4)
                   << registers.program_counter;
            return output.str();
        };
        std::cerr << "  output registers: "
                  << print_registers(original.output_registers) << " vs "
                  << print_registers(replayed.output_registers) << '\n';
    }
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

[[nodiscard]] std::filesystem::path routine_replay_corpus_path(
    const std::filesystem::path& directory,
    const RoutineReplayRequest& request)
{
    std::ostringstream name;
    name << std::uppercase << std::hex << std::setfill('0')
         << std::setw(2)
         << static_cast<unsigned>(request.routine.physical_bank)
         << '-' << std::setw(4) << request.routine.logical_pc
         << std::dec << '-' << request.ordinal << ".dcrr";
    return directory / name.str();
}

[[nodiscard]] std::optional<
    devils_crush::emulation::RoutineReplayCorpusEntry>
read_routine_replay_corpus(const std::filesystem::path& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0U
        || size > kMaximumRoutineReplayCorpusFileSize) {
        std::cerr << "Routine-replay corpus file size is invalid: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        std::cerr << "Could not read routine-replay corpus: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    auto decoded =
        devils_crush::emulation::decode_routine_replay_corpus(bytes);
    if (!decoded.decoded) {
        std::cerr << "Could not decode routine-replay corpus "
                  << path.string() << ": " << decoded.message << '\n';
        return std::nullopt;
    }
    return std::move(decoded.entry);
}

struct DirectRoutineReplay {
    devils_crush::emulation::RoutineReplayCorpusEntry corpus;
    std::optional<RoutineEntry> actual_entry;
    std::optional<RoutineCall> actual_call;
    std::vector<std::byte> actual_post_call_work_ram;
    std::uint64_t replacement_hits = 0U;
};

} // namespace

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
            input_for_recording_plan_frame(plan, frame_number));
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

[[nodiscard]] bool write_routine_replay_corpora(
    const std::filesystem::path& directory,
    const std::span<const RoutineReplayCapture> captures,
    const std::string_view core_revision)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "Could not create routine-replay corpus directory: "
                  << error.message() << '\n';
        return false;
    }

    for (const auto& capture : captures) {
        if (!capture.original_call
            || capture.checkpoint.core_state.empty()
            || capture.pre_call_work_ram.empty()
            || capture.original_post_call_work_ram.empty()) {
            std::cerr << "Cannot persist incomplete routine replay "
                      << routine_replay_name(capture.request) << ".\n";
            return false;
        }

        std::vector<std::byte> bytes;
        try {
            bytes = devils_crush::emulation::encode_routine_replay_corpus({
                .format_version = devils_crush::emulation::
                    kRoutineReplayCorpusFormatVersion,
                .rom_sha256 =
                    std::string(devils_crush::rom::kSupportedSha256),
                .core_revision = std::string(core_revision),
                .ordinal = capture.request.ordinal,
                .checkpoint_entry = capture.checkpoint_entry,
                .checkpoint = capture.checkpoint,
                .pre_call_work_ram = capture.pre_call_work_ram,
                .expected_call = *capture.original_call,
                .post_call_work_ram =
                    capture.original_post_call_work_ram,
            });
        } catch (const std::exception& exception) {
            std::cerr << "Could not encode routine replay "
                      << routine_replay_name(capture.request) << ": "
                      << exception.what() << '\n';
            return false;
        }

        const auto path = routine_replay_corpus_path(
            directory, capture.request);
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
                std::cerr << "Could not write routine-replay corpus: "
                          << temporary_path.string() << '\n';
                return false;
            }
        }
        if (!commit_artifact(
                temporary_path, path, "routine-replay corpus")) {
            return false;
        }
        std::cout << "Routine replay corpus "
                  << routine_replay_name(capture.request)
                  << " persisted: " << path.string() << '\n';
    }
    std::cout << "Routine replay corpus complete: " << captures.size()
              << " authoritative assembly calls\n";
    return true;
}

[[nodiscard]] int run_routine_replay_corpus(
    PceRuntime& runtime,
    const CommandLine& options,
    const std::span<const std::byte> rom,
    const std::string_view core_revision)
{
    std::vector<DirectRoutineReplay> replays;
    replays.reserve(options.routine_replay_corpus_paths.size());
    for (const auto& path : options.routine_replay_corpus_paths) {
        auto corpus = read_routine_replay_corpus(path);
        if (!corpus) {
            return EXIT_FAILURE;
        }
        if (corpus->rom_sha256 != devils_crush::rom::kSupportedSha256) {
            std::cerr << "Routine-replay corpus targets a different ROM: "
                      << path.string() << '\n';
            return EXIT_FAILURE;
        }
        if (corpus->core_revision != core_revision) {
            std::cerr << "Routine-replay corpus targets core revision "
                      << corpus->core_revision << ", not " << core_revision
                      << ": " << path.string() << '\n';
            return EXIT_FAILURE;
        }
        replays.push_back({.corpus = std::move(*corpus)});
    }
    for (const auto& replacement : options.replace_hooks) {
        if (std::none_of(
                replays.begin(),
                replays.end(),
                [&replacement](const DirectRoutineReplay& replay) {
                    return replay.corpus.checkpoint_entry.routine
                        == replacement.routine;
                })) {
            std::cerr << "Replacement target "
                      << format_routine_key(replacement.routine)
                      << " has no supplied corpus entry.\n";
            return EXIT_FAILURE;
        }
    }

    DirectRoutineReplay* active = nullptr;
    std::set<std::pair<std::uint8_t, std::uint16_t>> installed;
    for (const auto& replay : replays) {
        const auto routine = replay.corpus.checkpoint_entry.routine;
        if (!installed.emplace(
                routine.physical_bank, routine.logical_pc).second) {
            continue;
        }
        const auto replacement = std::find_if(
            options.replace_hooks.begin(),
            options.replace_hooks.end(),
            [routine](const RoutineReplayRequest& request) {
                return request.routine == routine;
            });
        const bool replaces = replacement != options.replace_hooks.end();
        devils_crush::emulation::ExecutionHook hook{
            .routine = routine,
            .mode = replaces ? HookMode::replace : HookMode::observe,
            .callback = [&active, routine](const RoutineEntry& entry) {
                if (active != nullptr
                    && active->corpus.checkpoint_entry.routine == routine
                    && !active->actual_entry) {
                    active->actual_entry = entry;
                }
            },
        };
        if (replaces) {
            hook.replacement_callback =
                [&active, routine, rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    if (active == nullptr
                        || active->corpus.checkpoint_entry.routine
                            != routine
                        // A corpus checkpoint resumes at the requested entry.
                        // The rest of its frame may reach the same routine
                        // again, but direct replay validates exactly the one
                        // captured call rather than replacing later calls.
                        || active->replacement_hits != 0U) {
                        return std::optional<RoutineReplacement>{};
                    }
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, rom);
                    if (result && result->checkpoints.empty()
                        && result->body_master_cycles
                            >= std::min(
                                entry.master_cycles_until_user_event,
                                entry.master_cycles_until_timer_interrupt)) {
                        result.reset();
                    }
                    if (result) {
                        ++active->replacement_hits;
                    }
                    return result;
                };
        }
        if (!runtime.install_execution_hook(std::move(hook))) {
            std::cerr << "Could not install direct corpus execution hook "
                      << format_routine_key(routine) << ".\n";
            return EXIT_FAILURE;
        }
        if (!runtime.install_routine_call_observer({
                .routine = routine,
                .callback = [&runtime, &active, routine](
                    const RoutineCall& call) {
                    if (active != nullptr
                        && active->corpus.checkpoint_entry.routine == routine
                        && !active->actual_call) {
                        active->actual_call = call;
                        active->actual_post_call_work_ram =
                            runtime.export_work_ram();
                    }
                },
            })) {
            std::cerr << "Could not install direct corpus call observer "
                      << format_routine_key(routine) << ".\n";
            return EXIT_FAILURE;
        }
    }

    for (auto& replay : replays) {
        active = &replay;
        const auto& expected = replay.corpus.expected_call;
        const auto name = format_routine_key(expected.entry.routine)
            + '@' + std::to_string(replay.corpus.ordinal);
        if (!runtime.deserialize(replay.corpus.checkpoint)) {
            std::cerr << "Direct routine replay " << name
                      << " could not restore its corpus state.\n";
            return EXIT_FAILURE;
        }
        if (runtime.export_work_ram()
            != replay.corpus.pre_call_work_ram) {
            std::cerr << "Direct routine replay " << name
                      << " restored different pre-call work RAM.\n";
            return EXIT_FAILURE;
        }
        const auto frame = runtime.run_frame(expected.entry.input);
        if (frame.frame_number != expected.entry.frame_number
            || !replay.actual_entry || !replay.actual_call
            || replay.actual_post_call_work_ram.empty()) {
            std::cerr << "Direct routine replay " << name
                      << " did not complete in its restored frame.\n";
            return EXIT_FAILURE;
        }
        const bool replacement_requested = std::any_of(
            options.replace_hooks.begin(),
            options.replace_hooks.end(),
            [&expected](const RoutineReplayRequest& request) {
                return request.routine == expected.entry.routine;
            });
        if (replacement_requested && replay.replacement_hits != 1U) {
            std::cerr << "Direct routine replay " << name
                      << " expected one native replacement, observed "
                      << replay.replacement_hits << ".\n";
            return EXIT_FAILURE;
        }
        if (!routine_calls_match_after_clock_normalization(
                expected, *replay.actual_call)) {
            std::cerr << "Direct routine replay " << name
                      << " changed registers, return state, relative "
                         "cycles, or ordered work-RAM writes.\n";
            report_routine_call_difference(expected, *replay.actual_call);
            return EXIT_FAILURE;
        }
        if (replay.actual_post_call_work_ram
            != replay.corpus.post_call_work_ram) {
            std::cerr << "Direct routine replay " << name
                      << " changed post-call work RAM.\n";
            return EXIT_FAILURE;
        }
        std::cout << "Direct routine replay " << name << " verified: frame "
                  << expected.entry.frame_number << ", "
                  << expected.body_master_cycles << " body cycles, "
                  << expected.work_ram_writes.size() << " ordered writes, "
                  << (replacement_requested ? "native" : "assembly")
                  << '\n';
        active = nullptr;
    }

    std::cout << "Direct routine replay corpus complete: " << replays.size()
              << " calls without route startup\n";
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
