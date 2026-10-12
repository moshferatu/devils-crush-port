// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

struct InteractiveCaptureFrame {
    InputFrame input;
    devils_crush::input::PlungerSample plunger;
    // The host only modifies the shooter after the cartridge accepts the
    // draw. Keeping this result distinguishes a raw axis position from a
    // gameplay-visible plunger movement during replay.
    bool plunger_engaged = false;
    std::string work_ram_sha256;
};

// A captured pad stream becomes a regular deterministic recording plan. This
// accepts the original three-column v1 artifact as well as the later
// plunger-aware form, but refuses a session in which the physical rod moved:
// replaying its button bit without its pre-frame shooter write would be a
// misleading approximation.
struct InteractiveSessionReplay {
    std::vector<InputFrame> inputs;
    std::vector<std::string> work_ram_sha256s;
    std::filesystem::path initial_state_path;
    std::filesystem::path initial_high_score_path;
    bool cabinet_host = false;
    double nudge_multiplier = 1.0;
};

[[nodiscard]] std::optional<InteractiveSessionReplay>
read_interactive_session_replay(const std::filesystem::path& path);

[[nodiscard]] bool write_interactive_input_capture(
    const std::filesystem::path& directory,
    const std::span<const InteractiveCaptureFrame> inputs,
    const std::string_view core_revision,
    const bool cabinet_mode,
    const double nudge_multiplier);

} // namespace devils_crush::app::host
