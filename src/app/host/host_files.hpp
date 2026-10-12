// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"

namespace devils_crush::app::host {

void print_validation(const devils_crush::rom::ValidationResult& result);

[[nodiscard]] std::string path_to_utf8(
    const std::filesystem::path& path);

[[nodiscard]] std::optional<std::vector<std::byte>> read_rom_bytes(
    const std::filesystem::path& path);

[[nodiscard]] bool load_backup_ram(
    PceRuntime& runtime,
    const std::filesystem::path& path);

[[nodiscard]] bool save_backup_ram(
    const PceRuntime& runtime,
    const std::filesystem::path& path);

[[nodiscard]] bool commit_artifact(
    const std::filesystem::path& temporary_path,
    const std::filesystem::path& path,
    const std::string_view label);

[[nodiscard]] bool save_quick_state(
    const PceRuntime& runtime,
    const std::filesystem::path& path);

[[nodiscard]] bool load_quick_state(
    PceRuntime& runtime,
    const std::filesystem::path& path);

} // namespace devils_crush::app::host
