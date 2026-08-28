// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::native_runtime {

// Read-only logical-address view of the supported HuCard image. MPR banking is
// deliberately confined to this asset adapter; native gameplay only receives
// decoded tables. The two mirrored 64-bank windows follow the cartridge's
// observed mapping convention.
class MappedRomView {
public:
    static constexpr std::size_t kBankBytes = 0x2000U;
    static constexpr std::size_t kSupportedRomBytes = 0x60000U;

    MappedRomView(
        std::span<const std::byte> bytes,
        std::array<std::uint8_t, 8> mpr) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::optional<std::byte> byte_at(
        std::uint16_t logical_address) const noexcept;
    [[nodiscard]] std::optional<std::vector<std::byte>> range_at(
        std::uint16_t logical_address,
        std::size_t byte_count) const;

private:
    std::span<const std::byte> bytes_;
    std::array<std::uint8_t, 8> mpr_;
};

} // namespace devils_crush::native_runtime
