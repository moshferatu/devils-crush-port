// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace devils_crush::backup {

enum class LoadStatus {
    loaded,
    not_found,
    invalid_size,
    io_error,
};

struct LoadResult {
    LoadStatus status = LoadStatus::io_error;
    std::vector<std::byte> bytes;
    std::string message;

    [[nodiscard]] bool loaded() const noexcept
    {
        return status == LoadStatus::loaded;
    }
};

struct SaveResult {
    bool saved = false;
    std::string message;
};

[[nodiscard]] LoadResult load_exact(
    const std::filesystem::path& path,
    std::size_t expected_size);

[[nodiscard]] SaveResult save_atomic(
    const std::filesystem::path& path,
    const std::vector<std::byte>& bytes);

} // namespace devils_crush::backup
