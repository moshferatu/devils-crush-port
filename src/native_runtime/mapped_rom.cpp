// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/native_runtime/mapped_rom.hpp"

namespace devils_crush::native_runtime {

MappedRomView::MappedRomView(
    const std::span<const std::byte> bytes,
    const std::array<std::uint8_t, 8> mpr) noexcept
    : bytes_(bytes)
    , mpr_(mpr)
{
}

bool MappedRomView::valid() const noexcept
{
    return bytes_.size() == kSupportedRomBytes;
}

std::optional<std::byte> MappedRomView::byte_at(
    const std::uint16_t logical_address) const noexcept
{
    if (!valid()) {
        return std::nullopt;
    }

    const auto logical_page = static_cast<std::size_t>(logical_address >> 13U);
    const auto mapped_bank = mpr_[logical_page];
    if (mapped_bank >= 0x80U) {
        return std::nullopt;
    }
    const auto physical_bank = mapped_bank < 0x40U
        ? static_cast<std::size_t>(mapped_bank & 0x1FU)
        : static_cast<std::size_t>((mapped_bank & 0x0FU) + 0x20U);
    const auto offset = physical_bank * kBankBytes
        + static_cast<std::size_t>(logical_address & (kBankBytes - 1U));
    return offset < bytes_.size() ? std::optional<std::byte>{bytes_[offset]}
                                  : std::nullopt;
}

std::optional<std::vector<std::byte>> MappedRomView::range_at(
    const std::uint16_t logical_address,
    const std::size_t byte_count) const
{
    const auto remaining_logical_bytes = static_cast<std::size_t>(
        0x10000U - static_cast<std::uint32_t>(logical_address));
    if (byte_count > remaining_logical_bytes) {
        return std::nullopt;
    }

    std::vector<std::byte> result;
    result.reserve(byte_count);
    for (std::size_t offset = 0U; offset < byte_count; ++offset) {
        const auto value = byte_at(static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(logical_address) + offset));
        if (!value) {
            return std::nullopt;
        }
        result.push_back(*value);
    }
    return result;
}

} // namespace devils_crush::native_runtime
