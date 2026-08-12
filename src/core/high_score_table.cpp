// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/high_score_table.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::core {
namespace {

constexpr std::size_t kWorkRamSize = 0x2000U;
constexpr std::array<std::byte, 4> kMagic{
    std::byte{'D'},
    std::byte{'C'},
    std::byte{'H'},
    std::byte{'S'}};
constexpr std::byte kFormatVersion{1U};

} // namespace

std::uint32_t high_score_at(
    const HighScoreTable& table,
    const std::size_t rank) noexcept
{
    if (rank >= kHighScoreRankCount) {
        return 0U;
    }
    return static_cast<std::uint32_t>(table.bytes[rank])
        | static_cast<std::uint32_t>(table.bytes[kHighScoreRankCount + rank])
            << 8U
        | static_cast<std::uint32_t>(
              table.bytes[2U * kHighScoreRankCount + rank])
            << 16U;
}

std::optional<HighScoreTable> read_high_score_table(
    const std::span<const std::byte> work_ram)
{
    if (work_ram.size() < kWorkRamSize) {
        return std::nullopt;
    }
    HighScoreTable table;
    for (std::size_t row = 0; row < kHighScoreRowCount; ++row) {
        for (std::size_t rank = 0; rank < kHighScoreRankCount; ++rank) {
            table.bytes[row * kHighScoreRankCount + rank] =
                std::to_integer<std::uint8_t>(
                    work_ram[kHighScoreRowOffsets[row] + rank]);
        }
    }
    return table;
}

bool write_high_score_table(
    const std::span<std::byte> work_ram,
    const HighScoreTable& table)
{
    if (work_ram.size() < kWorkRamSize) {
        return false;
    }
    for (std::size_t row = 0; row < kHighScoreRowCount; ++row) {
        for (std::size_t rank = 0; rank < kHighScoreRankCount; ++rank) {
            work_ram[kHighScoreRowOffsets[row] + rank] =
                static_cast<std::byte>(
                    table.bytes[row * kHighScoreRankCount + rank]);
        }
    }
    return true;
}

std::vector<std::byte> encode_high_score_table(const HighScoreTable& table)
{
    std::vector<std::byte> bytes;
    bytes.reserve(kHighScoreFileSize);
    bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
    bytes.push_back(kFormatVersion);
    bytes.push_back(static_cast<std::byte>(kHighScoreRankCount));
    for (const auto value : table.bytes) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

std::optional<HighScoreTable> decode_high_score_table(
    const std::span<const std::byte> bytes)
{
    if (bytes.size() != kHighScoreFileSize
        || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())
        || bytes[4] != kFormatVersion
        || bytes[5] != static_cast<std::byte>(kHighScoreRankCount)) {
        return std::nullopt;
    }
    // Nothing beyond the header is checked. The ranked columns are ordinary
    // bytes to the cartridge -- it re-sorts the whole table at the next game
    // over and clamps scores in the accumulator -- so a stricter reader would
    // only be able to refuse a table the game itself would have accepted.
    HighScoreTable table;
    for (std::size_t index = 0; index < kHighScoreTableBytes; ++index) {
        table.bytes[index] =
            std::to_integer<std::uint8_t>(bytes[6U + index]);
    }
    return table;
}

} // namespace devils_crush::core
