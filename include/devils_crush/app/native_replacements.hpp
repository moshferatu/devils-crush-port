// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace devils_crush::app {

[[nodiscard]] std::optional<emulation::RoutineReplacement>
replace_native_gameplay_routine(
    const emulation::RoutineEntry& entry,
    std::span<const std::byte> work_ram,
    std::span<const std::byte> rom);

} // namespace devils_crush::app
