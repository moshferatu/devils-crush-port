// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace devils_crush::emulation {

enum class Button : std::uint16_t {
    none = 0,
    one = 1U << 0U,
    two = 1U << 1U,
    select = 1U << 2U,
    run = 1U << 3U,
    up = 1U << 4U,
    right = 1U << 5U,
    down = 1U << 6U,
    left = 1U << 7U,
};

struct InputFrame {
    std::uint16_t player_one = 0;
    std::uint16_t player_two = 0;
};

struct FramebufferView {
    std::span<const std::uint32_t> pixels;
    int width = 0;
    int height = 0;
    int pitch_pixels = 0;
};

struct AudioView {
    std::span<const float> interleaved_stereo;
    int sample_rate = 0;
};

struct EmulatedFrame {
    // Views remain valid until the next non-const runtime operation or
    // runtime destruction.
    FramebufferView framebuffer;
    AudioView audio;
    std::uint64_t frame_number = 0;
};

struct RomLoadResult {
    bool loaded = false;
    std::string message;
};

struct RuntimeInfo {
    std::string name;
    std::string upstream_revision;
    bool core_available = false;
};

struct SaveState {
    std::vector<std::byte> core_state;
    std::uint64_t frame_number = 0;
};

struct RoutineKey {
    std::uint8_t physical_bank = 0;
    std::uint16_t logical_pc = 0;

    [[nodiscard]] bool operator==(const RoutineKey&) const = default;
};

enum class HookMode {
    pass_through,
    observe,
    replace,
};

struct CpuRegisters {
    std::uint16_t program_counter = 0;
    std::uint8_t accumulator = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t stack_pointer = 0;
    std::uint8_t status = 0;
    std::array<std::uint8_t, 8> mpr{};
};

struct RoutineEntry {
    RoutineKey routine;
    CpuRegisters registers;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
};

using ExecutionHookCallback = std::function<void(const RoutineEntry&)>;

struct ExecutionHook {
    RoutineKey routine;
    HookMode mode = HookMode::pass_through;
    ExecutionHookCallback callback;
};

class PceRuntime {
public:
    virtual ~PceRuntime() = default;

    // A real runtime is thread-confined. Create, call, and destroy it on the
    // same owning thread; libretro callbacks execute synchronously there.
    [[nodiscard]] virtual RuntimeInfo info() const = 0;
    [[nodiscard]] virtual RomLoadResult load_rom(
        const std::filesystem::path& path) = 0;
    virtual void reset() = 0;
    [[nodiscard]] virtual EmulatedFrame run_frame(
        const InputFrame& input) = 0;

    // Backup RAM is copied across this boundary; callers never receive a
    // mutable view into emulator-owned memory. These methods are available
    // only after a ROM has loaded successfully.
    [[nodiscard]] virtual std::size_t backup_ram_size() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::byte> export_backup_ram()
        const = 0;
    [[nodiscard]] virtual bool import_backup_ram(
        std::span<const std::byte> bytes) = 0;

    // Work RAM is exposed as a copy for deterministic recordings and later
    // typed game-state views. It is unavailable until a ROM is loaded.
    [[nodiscard]] virtual std::size_t work_ram_size() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::byte> export_work_ram() const = 0;

    // The first fork boundary supports one exact physical-bank/logical-PC
    // hook in pass-through or observe mode. It executes before opcode fetch
    // and cannot modify core state. Observe mode requires a callback. Replace
    // mode is reserved for a later bridge stage and is rejected for now.
    [[nodiscard]] virtual bool install_execution_hook(ExecutionHook hook) = 0;
    virtual void clear_execution_hook() noexcept = 0;
    [[nodiscard]] virtual bool has_execution_hook() const noexcept = 0;

    // Savestates are opaque Beetle payloads plus the host frame counter.
    // Persistent storage must additionally bind them to the ROM and core
    // revision before calling deserialize().
    [[nodiscard]] virtual SaveState serialize() const = 0;
    [[nodiscard]] virtual bool deserialize(const SaveState& state) = 0;
};

// Beetle PCE and libretro are single-instance APIs. A real runtime throws
// std::logic_error if a second instance is created before the first is
// destroyed. The dependency-free stub does not reserve emulator state.
[[nodiscard]] std::unique_ptr<PceRuntime> create_pce_runtime();

} // namespace devils_crush::emulation
