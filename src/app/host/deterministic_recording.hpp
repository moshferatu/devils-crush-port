// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"
#include "recording_plans.hpp"

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
    const std::filesystem::path& table_vram_census_path = {},
    const std::span<const std::byte> rom = {},
    const std::filesystem::path& initial_state_path = {},
    const std::function<void()>& failure_callback = {});

int run_backup_ram_reload_test(
    PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& backup_path);

int run_smoke_test(PceRuntime& runtime);

} // namespace devils_crush::app::host
