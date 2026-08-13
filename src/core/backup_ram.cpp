// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/backup_ram.hpp"

#include "devils_crush/core/atomic_file.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace devils_crush::backup {

LoadResult load_exact(
    const std::filesystem::path& path,
    const std::size_t expected_size)
{
    if (expected_size == 0U
        || expected_size
            > static_cast<std::size_t>(
                std::numeric_limits<std::streamsize>::max())) {
        return {
            .status = LoadStatus::invalid_size,
            .bytes = {},
            .message = "The expected backup-RAM size is invalid.",
        };
    }

    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {
            .status =
                error ? LoadStatus::io_error : LoadStatus::not_found,
            .bytes = {},
            .message = error
                ? "Could not inspect backup RAM: " + error.message()
                : "No backup-RAM file exists.",
        };
    }

    const auto file_size = std::filesystem::file_size(path, error);
    if (error) {
        return {
            .status = LoadStatus::io_error,
            .bytes = {},
            .message =
                "Could not inspect backup RAM: " + error.message(),
        };
    }
    if (file_size != expected_size) {
        return {
            .status = LoadStatus::invalid_size,
            .bytes = {},
            .message = "The backup-RAM file has an unexpected size.",
        };
    }

    std::vector<std::byte> bytes(expected_size);
    std::ifstream input(path, std::ios::binary);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input
        || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        return {
            .status = LoadStatus::io_error,
            .bytes = {},
            .message = "Could not read the complete backup-RAM file.",
        };
    }

    return {
        .status = LoadStatus::loaded,
        .bytes = std::move(bytes),
        .message = "Backup RAM loaded.",
    };
}

SaveResult save_atomic(
    const std::filesystem::path& path,
    const std::vector<std::byte>& bytes,
    const int commit_budget_milliseconds)
{
    if (bytes.empty()
        || bytes.size()
            > static_cast<std::size_t>(
                std::numeric_limits<std::streamsize>::max())) {
        return {
            .saved = false,
            .message = "The backup-RAM payload size is invalid.",
        };
    }

    std::error_code error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return {
                .saved = false,
                .message =
                    "Could not create the backup-RAM directory: "
                    + error.message(),
            };
        }
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    {
        std::ofstream output(
            temporary_path,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) {
            return {
                .saved = false,
                .message = "Could not write the temporary backup-RAM file.",
            };
        }
    }

    const auto commit = devils_crush::core::commit_replacement(
        temporary_path,
        path,
        commit_budget_milliseconds);
    if (!commit.committed) {
        return {
            .saved = false,
            .message =
                "Could not atomically commit backup RAM; Windows error "
                + std::to_string(commit.last_error) + " after "
                + std::to_string(commit.retries) + " retries.",
        };
    }

    return {
        .saved = true,
        .message = "Backup RAM saved.",
    };
}

} // namespace devils_crush::backup
