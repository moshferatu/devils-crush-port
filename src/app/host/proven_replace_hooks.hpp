// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "command_line.hpp"

namespace devils_crush::app::host {

// Whether `--replace-hook` may deploy this request: the routine, at that call
// or at all of them, is one a replacement has been proven for.
[[nodiscard]] bool is_proven_replace_hook(const RoutineReplayRequest& request);

// What `--replace-hook` says of a request that is not.
[[nodiscard]] std::string unproven_replace_hook_message();

} // namespace devils_crush::app::host
