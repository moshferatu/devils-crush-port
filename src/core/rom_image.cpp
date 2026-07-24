// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/core/rom_image.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace devils_crush::rom {
namespace {

[[nodiscard]] bool nt_failed(const NTSTATUS status) noexcept
{
    return status < 0;
}

[[nodiscard]] std::runtime_error bcrypt_error(
    const char* operation,
    const NTSTATUS status)
{
    std::ostringstream message;
    message << operation << " failed with NTSTATUS 0x" << std::hex
            << static_cast<unsigned long>(status);
    return std::runtime_error(message.str());
}

[[nodiscard]] std::vector<UCHAR> hash_bytes(
    const wchar_t* algorithm_name,
    const std::span<const std::byte> bytes)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;

    auto status =
        BCryptOpenAlgorithmProvider(&algorithm, algorithm_name, nullptr, 0);
    if (nt_failed(status)) {
        throw bcrypt_error("BCryptOpenAlgorithmProvider", status);
    }

    ULONG object_size = 0;
    ULONG bytes_written = 0;
    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size),
        static_cast<ULONG>(sizeof(object_size)),
        &bytes_written,
        0);
    if (nt_failed(status)) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw bcrypt_error("BCryptGetProperty(BCRYPT_OBJECT_LENGTH)", status);
    }

    ULONG digest_size = 0;
    status = BCryptGetProperty(
        algorithm,
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&digest_size),
        static_cast<ULONG>(sizeof(digest_size)),
        &bytes_written,
        0);
    if (nt_failed(status)) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw bcrypt_error("BCryptGetProperty(BCRYPT_HASH_LENGTH)", status);
    }

    std::vector<UCHAR> hash_object(object_size);
    std::vector<UCHAR> digest(digest_size);

    status = BCryptCreateHash(
        algorithm,
        &hash,
        hash_object.data(),
        object_size,
        nullptr,
        0,
        0);
    if (nt_failed(status)) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw bcrypt_error("BCryptCreateHash", status);
    }

    std::size_t offset = 0;
    constexpr auto kMaxChunk =
        static_cast<std::size_t>(std::numeric_limits<ULONG>::max());
    while (offset < bytes.size()) {
        const auto chunk_size = std::min(bytes.size() - offset, kMaxChunk);
        status = BCryptHashData(
            hash,
            reinterpret_cast<PUCHAR>(
                const_cast<std::byte*>(bytes.data() + offset)),
            static_cast<ULONG>(chunk_size),
            0);
        if (nt_failed(status)) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            throw bcrypt_error("BCryptHashData", status);
        }
        offset += chunk_size;
    }

    status = BCryptFinishHash(hash, digest.data(), digest_size, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (nt_failed(status)) {
        throw bcrypt_error("BCryptFinishHash", status);
    }

    return digest;
}

[[nodiscard]] std::string to_lower_hex(const std::span<const UCHAR> bytes)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : bytes) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

[[nodiscard]] ValidationResult failure(
    const ValidationCode code,
    const std::filesystem::path& path,
    std::string message)
{
    ValidationResult result;
    result.code = code;
    result.path = path;
    result.message = std::move(message);
    return result;
}

} // namespace

Digests compute_digests(const std::span<const std::byte> bytes)
{
    const auto md5 = hash_bytes(BCRYPT_MD5_ALGORITHM, bytes);
    const auto sha256 = hash_bytes(BCRYPT_SHA256_ALGORITHM, bytes);
    return {
        .md5 = to_lower_hex(md5),
        .sha256 = to_lower_hex(sha256),
    };
}

bool matches_supported_rom(
    const std::uintmax_t size,
    const Digests& digests) noexcept
{
    return size == kSupportedRomSize && digests.md5 == kSupportedMd5
        && digests.sha256 == kSupportedSha256;
}

ValidationResult validate_file(const std::filesystem::path& path)
{
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error || !std::filesystem::exists(status)) {
        return failure(
            ValidationCode::file_not_found,
            path,
            "ROM file was not found: " + path.string());
    }
    if (!std::filesystem::is_regular_file(status)) {
        return failure(
            ValidationCode::read_error,
            path,
            "ROM path is not a regular file: " + path.string());
    }

    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return failure(
            ValidationCode::read_error,
            path,
            "Could not determine ROM size: " + error.message());
    }

    if (size != kSupportedRomSize) {
        std::ostringstream message;
        message << "Unsupported ROM size: " << size << " bytes; expected "
                << kSupportedRomSize << " bytes.";
        if (size == kSupportedRomSize + 512U) {
            message
                << " This looks like the supported ROM with a 512-byte copier "
                   "header; remove the header and try again.";
        }

        auto result =
            failure(ValidationCode::wrong_size, path, message.str());
        result.size = size;
        return result;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        auto result = failure(
            ValidationCode::read_error,
            path,
            "Could not open ROM for reading: " + path.string());
        result.size = size;
        return result;
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input
        || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        auto result = failure(
            ValidationCode::read_error,
            path,
            "ROM could not be read completely.");
        result.size = size;
        return result;
    }

    Digests digests;
    try {
        digests = compute_digests(bytes);
    } catch (const std::exception& exception) {
        auto result = failure(
            ValidationCode::hash_error,
            path,
            "Could not hash ROM: " + std::string(exception.what()));
        result.size = size;
        return result;
    }

    if (!matches_supported_rom(size, digests)) {
        auto result = failure(
            ValidationCode::unsupported_hash,
            path,
            "ROM contents do not match the supported USA image.");
        result.size = size;
        result.digests = std::move(digests);
        return result;
    }

    ValidationResult result;
    result.code = ValidationCode::supported;
    result.path = path;
    result.size = size;
    result.digests = std::move(digests);
    result.message = "Supported Devil's Crush USA ROM verified.";
    return result;
}

} // namespace devils_crush::rom
