// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

[[nodiscard]] int run_seed_check(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const bool uncull);

[[nodiscard]] int run_attribute_check(
    devils_crush::emulation::PceRuntime& runtime);

[[nodiscard]] int run_state_dump(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& state_path,
    const std::filesystem::path& output_directory);

[[nodiscard]] int run_palette_check(
    devils_crush::emulation::PceRuntime& runtime);

[[nodiscard]] int run_cabinet_motion_check(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& directory,
    const bool gpu);

[[nodiscard]] int run_cabinet_view_probe(
    devils_crush::emulation::PceRuntime& runtime,
    const std::filesystem::path& rom_path,
    const std::filesystem::path& output_directory);

} // namespace devils_crush::app::host
