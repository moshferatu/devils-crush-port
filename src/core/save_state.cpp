// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/save_state.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace devils_crush::state {
namespace {

constexpr std::array<std::byte, 8> kMagic{
    std::byte{'D'},
    std::byte{'C'},
    std::byte{'R'},
    std::byte{'S'},
    std::byte{'T'},
    std::byte{'A'},
    std::byte{'T'},
    std::byte{'E'},
};
constexpr std::size_t kRomHashLength = 64U;
constexpr std::size_t kCoreRevisionCapacity = 64U;
constexpr std::size_t kHeaderSize =
    kMagic.size() + 4U + 4U + 8U + 8U + 4U + 4U + kRomHashLength
    + kCoreRevisionCapacity;
constexpr std::size_t kMaximumCoreStateSize = 64U * 1024U * 1024U;

[[nodiscard]] constexpr bool is_lower_hex(const char character) noexcept
{
    return (character >= '0' && character <= '9')
        || (character >= 'a' && character <= 'f');
}

[[nodiscard]] bool valid_metadata(
    const SaveStateContainer& container) noexcept
{
    return container.format_version == kSaveStateFormatVersion
        && container.rom_sha256.size() == kRomHashLength
        && std::ranges::all_of(container.rom_sha256, is_lower_hex)
        && !container.core_revision.empty()
        && container.core_revision.size() <= kCoreRevisionCapacity
        && !container.core_state.empty()
        && container.core_state.size() <= kMaximumCoreStateSize;
}

void append_u32(
    std::vector<std::byte>& output,
    const std::uint32_t value)
{
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        output.push_back(
            static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void append_u64(
    std::vector<std::byte>& output,
    const std::uint64_t value)
{
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        output.push_back(
            static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

[[nodiscard]] bool read_u32(
    const std::span<const std::byte> bytes,
    std::size_t& offset,
    std::uint32_t& value) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        return false;
    }
    value = 0U;
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    }
    return true;
}

[[nodiscard]] bool read_u64(
    const std::span<const std::byte> bytes,
    std::size_t& offset,
    std::uint64_t& value) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < 8U) {
        return false;
    }
    value = 0U;
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
    }
    return true;
}

[[nodiscard]] std::uint32_t crc32(
    const std::span<const std::byte> bytes) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0U - static_cast<std::uint32_t>(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

[[nodiscard]] DecodeResult failure(std::string message)
{
    return {
        .decoded = false,
        .container = {},
        .message = std::move(message),
    };
}

} // namespace

std::vector<std::byte> encode(const SaveStateContainer& container)
{
    if (!valid_metadata(container)) {
        throw std::invalid_argument(
            "The save-state container has invalid metadata or payload.");
    }
    if (container.core_state.size()
        > std::numeric_limits<std::size_t>::max() - kHeaderSize) {
        throw std::invalid_argument("The save-state payload is too large.");
    }

    std::vector<std::byte> output;
    output.reserve(kHeaderSize + container.core_state.size());
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    append_u32(output, container.format_version);
    append_u32(output, static_cast<std::uint32_t>(kHeaderSize));
    append_u64(output, container.frame_number);
    append_u64(
        output,
        static_cast<std::uint64_t>(container.core_state.size()));
    append_u32(output, crc32(container.core_state));
    append_u32(
        output,
        static_cast<std::uint32_t>(container.core_revision.size()));
    for (const char character : container.rom_sha256) {
        output.push_back(static_cast<std::byte>(character));
    }
    for (std::size_t index = 0; index < kCoreRevisionCapacity; ++index) {
        output.push_back(index < container.core_revision.size()
                ? static_cast<std::byte>(container.core_revision[index])
                : std::byte{0});
    }
    output.insert(
        output.end(),
        container.core_state.begin(),
        container.core_state.end());
    return output;
}

DecodeResult decode(const std::span<const std::byte> bytes)
{
    if (bytes.size() < kHeaderSize) {
        return failure("The save-state file is truncated.");
    }
    if (!std::ranges::equal(kMagic, bytes.first(kMagic.size()))) {
        return failure("The save-state file has an invalid signature.");
    }

    std::size_t offset = kMagic.size();
    std::uint32_t version = 0U;
    std::uint32_t header_size = 0U;
    std::uint64_t frame_number = 0U;
    std::uint64_t payload_size = 0U;
    std::uint32_t payload_crc = 0U;
    std::uint32_t revision_length = 0U;
    if (!read_u32(bytes, offset, version)
        || !read_u32(bytes, offset, header_size)
        || !read_u64(bytes, offset, frame_number)
        || !read_u64(bytes, offset, payload_size)
        || !read_u32(bytes, offset, payload_crc)
        || !read_u32(bytes, offset, revision_length)) {
        return failure("The save-state header is truncated.");
    }
    if (version != kSaveStateFormatVersion) {
        return failure("The save-state version is not supported.");
    }
    if (header_size != kHeaderSize) {
        return failure("The save-state header size is not supported.");
    }
    if (payload_size == 0U || payload_size > kMaximumCoreStateSize
        || payload_size
            > static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max())
        || static_cast<std::size_t>(payload_size)
            != bytes.size() - kHeaderSize) {
        return failure("The save-state payload size is invalid.");
    }
    if (revision_length == 0U
        || revision_length > kCoreRevisionCapacity) {
        return failure("The save-state core revision is invalid.");
    }

    std::string rom_sha256;
    rom_sha256.reserve(kRomHashLength);
    for (std::size_t index = 0; index < kRomHashLength; ++index) {
        rom_sha256.push_back(static_cast<char>(bytes[offset++]));
    }
    std::string core_revision;
    core_revision.reserve(revision_length);
    for (std::uint32_t index = 0; index < revision_length; ++index) {
        core_revision.push_back(static_cast<char>(bytes[offset + index]));
    }
    for (std::size_t index = revision_length;
         index < kCoreRevisionCapacity;
         ++index) {
        if (bytes[offset + index] != std::byte{0}) {
            return failure(
                "The save-state core revision padding is invalid.");
        }
    }
    offset += kCoreRevisionCapacity;

    SaveStateContainer container{
        .format_version = version,
        .frame_number = frame_number,
        .rom_sha256 = std::move(rom_sha256),
        .core_revision = std::move(core_revision),
        .core_state =
            std::vector<std::byte>(bytes.begin() + offset, bytes.end()),
    };
    if (!valid_metadata(container)) {
        return failure("The save-state metadata is invalid.");
    }
    if (crc32(container.core_state) != payload_crc) {
        return failure("The save-state payload checksum does not match.");
    }

    return {
        .decoded = true,
        .container = std::move(container),
        .message = "Save-state container decoded.",
    };
}

} // namespace devils_crush::state
