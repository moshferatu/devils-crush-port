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

    [[nodiscard]] std::size_t work_ram_size() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] std::vector<std::byte> export_work_ram() const override
    {
        return {};
    }

    [[nodiscard]] bool import_work_ram(
        const std::span<const std::byte>) override
    {
        return false;
    }

    [[nodiscard]] std::optional<VideoMemorySnapshot>
    export_video_memory() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] bool install_execution_hook(ExecutionHook) override
    {
        return false;
    }

    [[nodiscard]] bool remove_execution_hook(
        const RoutineKey) noexcept override
    {
        return false;
    }

    void clear_execution_hooks() noexcept override {}

    [[nodiscard]] std::size_t execution_hook_count()
        const noexcept override
    {
        return 0U;
    }

    [[nodiscard]] ReplacementNesting replacement_nesting()
        const noexcept override
    {
        return {};
    }

    [[nodiscard]] bool install_routine_call_observer(
        RoutineCallObserver) override
    {
        return false;
    }

    [[nodiscard]] bool remove_routine_call_observer(
        const RoutineKey) noexcept override
    {
        return false;
    }

    void clear_routine_call_observers() noexcept override {}

    [[nodiscard]] std::size_t routine_call_observer_count()
        const noexcept override
    {
        return 0U;
    }

    [[nodiscard]] bool set_work_ram_write_observer(
        WorkRamWriteCallback) override
    {
        return false;
    }

    void clear_work_ram_write_observer() noexcept override {}

    [[nodiscard]] bool has_work_ram_write_observer()
        const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool set_instruction_profiling(bool) override
    {
        return false;
    }

    [[nodiscard]] bool instruction_profiling_enabled()
        const noexcept override
    {
        return false;
    }

    [[nodiscard]] std::vector<InstructionProfileEntry>
    export_instruction_profile() const override
    {
        return {};
    }

    [[nodiscard]] bool set_native_psg(bool) override
    {
        return false;
    }

    [[nodiscard]] bool native_psg_enabled() const noexcept override
    {
        return false;
    }

    [[nodiscard]] std::optional<audio::PcePsg::Coverage> native_psg_coverage()
        const override
    {
        return std::nullopt;
    }

    [[nodiscard]] bool set_bus_write_observer(
        BusWriteCallback) override
    {
        return false;
    }

    void clear_bus_write_observer() noexcept override {}

    [[nodiscard]] bool has_bus_write_observer()
        const noexcept override
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
