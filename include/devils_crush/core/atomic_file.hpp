// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>

namespace devils_crush::core {

// The default time a commit is willing to spend waiting out another process.
// A scanner opens a file for the moment after it is closed and lets go of it
// in tens of milliseconds; five seconds is far past that and still far short
// of the shortest test in the suite, so a genuine permissions failure is
// reported inside the same run rather than hanging it.
inline constexpr int kDefaultCommitBudgetMilliseconds = 5000;

struct CommitResult {
    bool committed = false;
    // The last Win32 error any attempt reported, kept even once the commit
    // succeeds: on success it names what the process was waiting out, which is
    // the only record that anything was in the way. Zero when the first
    // attempt carried it.
    std::uint32_t last_error = 0U;
    // How many attempts were refused before this result. Nonzero with
    // `committed` set means the race below was met and absorbed, which is the
    // only way to tell that it happened at all.
    int retries = 0;

    [[nodiscard]] bool retried() const noexcept { return retries > 0; }
};

// True for the Win32 errors a replace raises because something outside this
// process is holding one of the two files open, and false for every error that
// says the replace itself was wrong.
[[nodiscard]] bool is_transient_commit_error(std::uint32_t error) noexcept;

// Replace `final_path` with `temporary_path` atomically, retrying while the
// refusal is one that another process can stop causing.
//
// The atomic replace is not a private operation between this process and its
// own file. On Windows a file is opened by the malware scanner and the search
// indexer in the moment after it is closed, and `MoveFileEx` is refused while
// any handle is open on either name -- with ERROR_SHARING_VIOLATION when the
// handle is on the temporary and ERROR_ACCESS_DENIED when it is on the
// destination. Nothing about the write was wrong when that happens, and the
// very next attempt usually succeeds, so the single attempt this replaced
// turned another process's housekeeping into a failed run. Under a 24-way
// parallel test suite that landed about once per run, on a different test every
// time, which is what made it read like nondeterminism in the thing under test
// rather than contention over a file handle.
//
// `budget_milliseconds` bounds the total wait; zero attempts the replace
// exactly once, which is what the previous behavior was.
[[nodiscard]] CommitResult commit_replacement(
    const std::filesystem::path& temporary_path,
    const std::filesystem::path& final_path,
    int budget_milliseconds = kDefaultCommitBudgetMilliseconds);

} // namespace devils_crush::core
