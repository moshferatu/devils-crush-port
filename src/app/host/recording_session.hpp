// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"
#include "interactive_session.hpp"

namespace devils_crush::app::host {

[[nodiscard]] int run_recording_session(
    PceRuntime& runtime,
    const CommandLine& options,
    const std::optional<InteractiveSessionReplay>& session_replay,
    const std::vector<std::byte>& replacement_rom);

} // namespace devils_crush::app::host
