// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace devils_crush::rom {

inline constexpr std::uintmax_t kSupportedRomSize = 393'216U;
inline constexpr std::string_view kSupportedMd5 =
    "92fbe89d4c87bf283dbfbeac69a64e6e";
inline constexpr std::string_view kSupportedSha256 =
    "9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9";

struct Digests {
    std::string md5;
    std::string sha256;
};

enum class ValidationCode {
    supported,
    file_not_found,
    read_error,
    wrong_size,
    unsupported_hash,
    hash_error,
};

struct ValidationResult {
    ValidationCode code = ValidationCode::read_error;
    std::filesystem::path path;
    std::uintmax_t size = 0;
    Digests digests;
    std::string message;

    [[nodiscard]] bool supported() const noexcept
    {
        return code == ValidationCode::supported;
    }
};

[[nodiscard]] Digests compute_digests(std::span<const std::byte> bytes);

[[nodiscard]] bool matches_supported_rom(
    std::uintmax_t size,
    const Digests& digests) noexcept;

[[nodiscard]] ValidationResult validate_file(const std::filesystem::path& path);

} // namespace devils_crush::rom
