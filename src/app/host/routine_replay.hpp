// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"
#include "recording_plans.hpp"

namespace devils_crush::app::host {

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

int verify_routine_replays(
    PceRuntime& runtime,
    const RecordingPlan& plan,
    std::span<RoutineReplayCapture> captures,
    RoutineReplayCapture*& active_replay,
    bool& suppress_recording_observers);

[[nodiscard]] bool write_routine_replay_corpora(
    const std::filesystem::path& directory,
    const std::span<const RoutineReplayCapture> captures,
    const std::string_view core_revision);

[[nodiscard]] int run_routine_replay_corpus(
    PceRuntime& runtime,
    const CommandLine& options,
    const std::span<const std::byte> rom,
    const std::string_view core_revision);

} // namespace devils_crush::app::host
