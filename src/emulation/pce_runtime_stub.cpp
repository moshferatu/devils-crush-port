// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/emulation/pce_runtime.hpp"

#include <memory>

namespace devils_crush::emulation {
namespace {

class UnavailablePceRuntime final : public PceRuntime {
public:
    [[nodiscard]] RuntimeInfo info() const override
    {
        return {
            .name = "Beetle PCE disabled for this build",
            .upstream_revision = {},
            .core_available = false,
        };
    }

    [[nodiscard]] RomLoadResult load_rom(
        const std::filesystem::path&) override
    {
        return {
            .loaded = false,
            .message =
                "This dependency-free build was configured with "
                "DC_FETCH_BEETLE=OFF.",
        };
    }

    void reset() override {}

    [[nodiscard]] EmulatedFrame run_frame(const InputFrame&) override
    {
        return {};
    }

    [[nodiscard]] std::size_t backup_ram_size() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] std::vector<std::byte> export_backup_ram()
        const override
    {
        return {};
    }

    [[nodiscard]] bool import_backup_ram(
        const std::span<const std::byte>) override
    {
        return false;
    }

    [[nodiscard]] SaveState serialize() const override
    {
        return {};
    }

    [[nodiscard]] bool deserialize(const SaveState&) override
    {
        return false;
    }
};

} // namespace

std::unique_ptr<PceRuntime> create_pce_runtime()
{
    return std::make_unique<UnavailablePceRuntime>();
}

} // namespace devils_crush::emulation
