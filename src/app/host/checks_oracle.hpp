// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

[[nodiscard]] int run_high_score_check(
    devils_crush::emulation::PceRuntime& runtime);

[[nodiscard]] int run_freeze_census(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path);

[[nodiscard]] int run_cabinet_oracle(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path);

[[nodiscard]] int dump_cabinet_route(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& directory);

} // namespace devils_crush::app::host
