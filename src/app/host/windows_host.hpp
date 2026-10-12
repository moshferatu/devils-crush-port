// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

int run_windows_host(
    PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const bool cabinet_mode,
    const std::uint8_t bonus_stage,
    const int frame_limit = 0,
    const CommandLine* native_registry = nullptr,
    const CabinetLayers layers = CabinetLayers::all,
    const bool overlay_stats = false,
    const std::filesystem::path& capture_session_directory = {},
    const bool native_gameplay_bootstrap_shadow = false,
    const double nudge_multiplier = 1.0,
    const bool presentation_check = false,
    const bool drag_ball = false);

} // namespace devils_crush::app::host
