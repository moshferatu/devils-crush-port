// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/core/atomic_file.hpp"

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

// `commit_budget_milliseconds` is handed to
// `devils_crush::core::commit_replacement`. Leave it at the default anywhere a
// pause is cheaper than a lost write, and pass zero on a caller that runs
// inside the frame loop and will come back to a pending write on its own: five
// seconds of waiting out a scanner is the right answer at shutdown and a
// visible stall during play.
[[nodiscard]] SaveResult save_atomic(
    const std::filesystem::path& path,
    const std::vector<std::byte>& bytes,
    int commit_budget_milliseconds =
        devils_crush::core::kDefaultCommitBudgetMilliseconds);

} // namespace devils_crush::backup
