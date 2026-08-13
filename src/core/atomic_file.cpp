// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/atomic_file.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>

namespace devils_crush::core {
namespace {

// The first delay, and the ceiling it doubles up to. The scanner's window is
// short enough that the first retry usually carries it, so the wait asks for as
// little as it can and only grows if the holder is slower than expected. What
// the first naps actually cost is the system timer's granularity rather than
// these figures; the ceiling is what governs a long wait, and a three-second
// hold measured 27 retries.
constexpr auto kInitialDelay = std::chrono::milliseconds(1);
constexpr auto kMaximumDelay = std::chrono::milliseconds(100);

} // namespace

bool is_transient_commit_error(const std::uint32_t error) noexcept
{
    switch (error) {
        // Another handle is open on the temporary file. This is the one the
        // suite actually failed on, because the temporary is what was written
        // and closed a moment earlier and so is what an on-close scan opens.
        case ERROR_SHARING_VIOLATION:
        // A handle open on the destination reports this instead -- measured,
        // not assumed. So does a destination whose permissions genuinely
        // forbid the replace, which is why a budget bounds the wait rather
        // than a retry count: that case still fails inside the same run.
        case ERROR_ACCESS_DENIED:
        // A byte range of the destination is locked.
        case ERROR_LOCK_VIOLATION:
        // The destination is mapped into some process's address space.
        case ERROR_USER_MAPPED_FILE:
            return true;
        default:
            return false;
    }
}

CommitResult commit_replacement(
    const std::filesystem::path& temporary_path,
    const std::filesystem::path& final_path,
    const int budget_milliseconds)
{
    CommitResult result;

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(std::max(budget_milliseconds, 0));
    auto delay = kInitialDelay;

    for (;;) {
        if (MoveFileExW(
                temporary_path.c_str(),
                final_path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            result.committed = true;
            return result;
        }

        result.last_error = static_cast<std::uint32_t>(GetLastError());
        if (!is_transient_commit_error(result.last_error)) {
            return result;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return result;
        }

        // Never sleep past the deadline: the caller asked for a bound on the
        // whole wait, not on each nap.
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now);
        std::this_thread::sleep_for(std::min(delay, remaining));
        delay = std::min(delay * 2, kMaximumDelay);
        ++result.retries;
    }
}

} // namespace devils_crush::core
