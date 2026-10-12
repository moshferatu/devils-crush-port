// SPDX-License-Identifier: GPL-3.0-or-later
#include "host_files.hpp"

#include "trace_writers.hpp"

namespace devils_crush::app::host {

namespace {

constexpr std::uintmax_t kMaximumSaveStateFileSize =
    64U * 1024U * 1024U + 4096U;

} // namespace

void print_validation(const devils_crush::rom::ValidationResult& result)
{
    auto& output = result.supported() ? std::cout : std::cerr;
    output << result.message << '\n';
    if (result.size != 0U) {
        output << "Size:    " << result.size << " bytes\n";
    }
    if (!result.digests.md5.empty()) {
        output << "MD5:     " << result.digests.md5 << '\n';
    }
    if (!result.digests.sha256.empty()) {
        output << "SHA-256: " << result.digests.sha256 << '\n';
    }
}

[[nodiscard]] std::string path_to_utf8(
    const std::filesystem::path& path)
{
    const auto encoded = path.u8string();
    return {
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size(),
    };
}

[[nodiscard]] std::optional<std::vector<std::byte>> read_rom_bytes(
    const std::filesystem::path& path)
{
    std::vector<std::byte> bytes(
        static_cast<std::size_t>(
            devils_crush::rom::kSupportedRomSize));
    std::ifstream input(path, std::ios::binary);
    if (input) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!input
        || input.gcount()
            != static_cast<std::streamsize>(bytes.size())) {
        std::cerr << "Could not read the supported ROM bytes: "
                  << path.string() << '\n';
        return std::nullopt;
    }
    return bytes;
}

[[nodiscard]] bool load_backup_ram(
    PceRuntime& runtime,
    const std::filesystem::path& path)
{
    const auto expected_size = runtime.backup_ram_size();
    if (expected_size == 0U) {
        std::cerr << "The emulator did not expose backup RAM.\n";
        return false;
    }

    auto result =
        devils_crush::backup::load_exact(path, expected_size);
    if (result.status == devils_crush::backup::LoadStatus::not_found) {
        return true;
    }
    if (!result.loaded()) {
        std::cerr << "Could not load backup RAM: " << result.message
                  << " (" << path.string() << ")\n";
        return false;
    }
    if (!runtime.import_backup_ram(result.bytes)) {
        std::cerr << "The emulator rejected the backup RAM data.\n";
        return false;
    }

    std::cout << "Loaded backup RAM: " << path.string() << '\n';
    return true;
}

[[nodiscard]] bool save_backup_ram(
    const PceRuntime& runtime,
    const std::filesystem::path& path)
{
    const auto bytes = runtime.export_backup_ram();
    if (bytes.empty()) {
        std::cerr << "The emulator did not return backup RAM to save.\n";
        return false;
    }

    const auto result = devils_crush::backup::save_atomic(path, bytes);
    if (!result.saved) {
        std::cerr << "Could not save backup RAM: " << result.message
                  << " (" << path.string() << ")\n";
        return false;
    }

    std::cout << "Saved backup RAM: " << path.string() << '\n';
    return true;
}

// Publish a finished artifact over its own name, and say so when the replace
// had to wait for something else to let go of the file.
//
// Every recorder here writes to `<path>.tmp` and replaces `<path>` at the end,
// so the artifact a regression hashes is either the whole of the last run or
// the whole of the previous one, never half of either. The replace is the only
// step in a recording that another process can refuse: see
// `devils_crush::core::commit_replacement` for what does the refusing. The
// retried-success line exists because absorbing the wait silently would leave
// no way to tell a suite that never races from one that races and recovers.
[[nodiscard]] bool commit_artifact(
    const std::filesystem::path& temporary_path,
    const std::filesystem::path& path,
    const std::string_view label)
{
    const auto commit =
        devils_crush::core::commit_replacement(temporary_path, path);
    if (!commit.committed) {
        std::cerr << "Could not commit " << label << "; Windows error "
                  << commit.last_error << " after " << commit.retries
                  << " retries.\n";
        return false;
    }
    if (commit.retried()) {
        std::cout << "Committed " << label << " after " << commit.retries
                  << " retries; Windows error " << commit.last_error
                  << ".\n";
    }
    return true;
}

[[nodiscard]] bool save_quick_state(
    const PceRuntime& runtime,
    const std::filesystem::path& path)
{
    const auto state = runtime.serialize();
    const auto runtime_info = runtime.info();
    if (state.core_state.empty() || runtime_info.upstream_revision.empty()) {
        std::cerr << "The emulator did not return a savestate.\n";
        return false;
    }

    std::vector<std::byte> bytes;
    try {
        bytes = devils_crush::state::encode({
            .format_version =
                devils_crush::state::kSaveStateFormatVersion,
            .frame_number = state.frame_number,
            .rom_sha256 =
                std::string(devils_crush::rom::kSupportedSha256),
            .core_revision = runtime_info.upstream_revision,
            .core_state = state.core_state,
        });
    } catch (const std::exception& exception) {
        std::cerr << "Could not encode quick state: " << exception.what()
                  << '\n';
        return false;
    }

    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    if (error) {
        std::cerr << "Could not create save directory: " << error.message()
                  << '\n';
        return false;
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    {
        std::ofstream output(
            temporary_path,
            std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) {
            std::cerr << "Could not write quick-state file: "
                      << temporary_path.string() << '\n';
            return false;
        }
    }

    if (!commit_artifact(temporary_path, path, "quick-state file")) {
        return false;
    }

    std::cout << "Saved quick state at frame " << state.frame_number << ": "
              << path.string() << '\n';
    return true;
}

[[nodiscard]] bool load_quick_state(
    PceRuntime& runtime,
    const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        std::cerr << (error ? "Could not inspect quick-state file: "
                            + error.message()
                           : "No quick-state file exists.")
                  << '\n';
        return false;
    }
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size == 0U
        || file_size > kMaximumSaveStateFileSize) {
        std::cerr << "The quick-state file size is invalid.\n";
        return false;
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
    std::ifstream input(path, std::ios::binary);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input
        || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        std::cerr << "Could not read quick-state file: " << path.string()
                  << '\n';
        return false;
    }

    auto decoded = devils_crush::state::decode(bytes);
    if (!decoded.decoded) {
        std::cerr << "Could not restore quick state: " << decoded.message
                  << '\n';
        return false;
    }
    const auto runtime_info = runtime.info();
    if (decoded.container.rom_sha256
        != devils_crush::rom::kSupportedSha256) {
        std::cerr << "The quick state belongs to a different ROM.\n";
        return false;
    }
    if (decoded.container.core_revision
        != runtime_info.upstream_revision) {
        std::cerr << "The quick state belongs to a different core revision.\n";
        return false;
    }

    const auto frame_number = decoded.container.frame_number;
    if (!runtime.deserialize({
            .core_state = std::move(decoded.container.core_state),
            .frame_number = frame_number,
        })) {
        std::cerr << "The emulator rejected the quick-state payload.\n";
        return false;
    }

    std::cout << "Restored quick state at frame " << frame_number << ": "
              << path.string() << '\n';
    return true;
}

} // namespace devils_crush::app::host
