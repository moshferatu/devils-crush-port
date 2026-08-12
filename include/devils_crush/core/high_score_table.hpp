// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::core {

// The cartridge keeps its high-score table in work RAM at `$2200`, as six
// parallel rows of seven bytes:
//
//     $2200  score, low byte     $2215  name, first character
//     $2207  score, middle byte  $221C  name, second character
//     $220E  score, high byte    $2223  name, third character
//
// Within a row, columns 0-4 are the five ranked entries and columns 5-6 are
// the two players' live values. The selection sort at `09:6B6F` runs X over
// 0-5 and Y over X+1-6, so a finished player's score walks into the ranked
// columns and the loser falls back out; that is the whole ranking mechanism.
//
// `01:C107` fills all 42 bytes from ROM at boot -- five entries of ten
// thousand stored units, which the panel shows as 1,000,000, all named `AAA`.
// Over the 3,600-frame census route the work-RAM write trace shows the only
// other writes anywhere in the block landing on columns 5 and 6, so the ranked
// columns are exactly the part of the block that outlives a session and the
// only part worth carrying between runs.
inline constexpr std::size_t kHighScoreRankCount = 5U;
inline constexpr std::size_t kHighScoreRowCount = 6U;
inline constexpr std::array<std::size_t, kHighScoreRowCount>
    kHighScoreRowOffsets{
        0x0200U,
        0x0207U,
        0x020EU,
        0x0215U,
        0x021CU,
        0x0223U};
inline constexpr std::size_t kHighScoreTableBytes =
    kHighScoreRankCount * kHighScoreRowCount;

// The ranked columns, row-major: `bytes[row * kHighScoreRankCount + rank]`.
struct HighScoreTable {
    std::array<std::uint8_t, kHighScoreTableBytes> bytes{};

    [[nodiscard]] friend bool operator==(
        const HighScoreTable&,
        const HighScoreTable&) = default;
};

// `"DCHS"`, a format byte, a rank count, then the ranked columns row by row.
// The count is stored rather than assumed so a file written by a build that
// ranked a different number of entries is refused instead of half-read.
inline constexpr std::size_t kHighScoreFileSize = 6U + kHighScoreTableBytes;

// The three-byte little-endian score in stored units. One stored unit is a
// hundred visible points, which is why the default table reads 1,000,000.
[[nodiscard]] std::uint32_t high_score_at(
    const HighScoreTable& table,
    std::size_t rank) noexcept;

[[nodiscard]] std::optional<HighScoreTable> read_high_score_table(
    std::span<const std::byte> work_ram);

[[nodiscard]] bool write_high_score_table(
    std::span<std::byte> work_ram,
    const HighScoreTable& table);

[[nodiscard]] std::vector<std::byte> encode_high_score_table(
    const HighScoreTable& table);

[[nodiscard]] std::optional<HighScoreTable> decode_high_score_table(
    std::span<const std::byte> bytes);

} // namespace devils_crush::core
