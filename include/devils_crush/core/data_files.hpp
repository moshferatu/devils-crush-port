// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace devils_crush::core {

// The files the game keeps between runs, all in the working directory: the
// folder the game is started from, which the launcher makes its own.
inline constexpr const char* kConfigFileName = "devils-crush.ini";
inline constexpr const char* kBackupRamFileName = "devils-crush.sav";
inline constexpr const char* kQuickStateFileName = "devils-crush.state";
inline constexpr const char* kHighScoreFileName = "devils-crush.hiscore";
inline constexpr const char* kGamepadMappingsFileName = "gamecontrollerdb.txt";

// The subfolder earlier builds kept them in, where the settings were
// `cabinet.ini`.
inline constexpr const char* kLegacyDataFolderName = "portable-data";

struct DataMigration {
    // What was moved, as "old -> new", and what could not be.
    std::vector<std::string> moved;
    std::vector<std::string> problems;
};

// Moves the files an earlier build left in `portable-data` into `base`, once.
//
// A file already at the new name wins and the old one is left where it is:
// the new one was written by a build that has already moved, so it is the
// newer. The folder is removed only if that leaves it empty, so anything
// else a player put there stays. Called by the game and the launcher before
// either reads anything, so whichever starts first does it.
[[nodiscard]] DataMigration migrate_legacy_data_folder(
    const std::filesystem::path& base);

} // namespace devils_crush::core
