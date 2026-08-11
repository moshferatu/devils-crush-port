// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "devils_crush/audio/pce_psg.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
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

    [[nodiscard]] bool operator==(const InputFrame&) const = default;
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

struct VideoMemorySnapshot {
    std::vector<std::uint16_t> vram_words;
    // The base HuC6270's internal post-DMA sprite attribute table. Four
    // words describe each of 64 hardware sprites.
    std::vector<std::uint16_t> sat_words;
    std::vector<std::uint16_t> palette_entries;
    std::uint16_t memory_width_register = 0;
    std::uint16_t background_x_scroll = 0;
    std::uint16_t background_y_scroll = 0;

    [[nodiscard]] int bat_width_tiles() const noexcept
    {
        constexpr std::array widths{32, 64, 128, 128};
        return widths[(memory_width_register >> 4U) & 0x03U];
    }

    [[nodiscard]] int bat_height_tiles() const noexcept
    {
        return (memory_width_register & 0x40U) != 0U ? 64 : 32;
    }
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

    [[nodiscard]] bool operator==(const CpuRegisters&) const = default;
};

struct RoutineEntry {
    RoutineKey routine;
    CpuRegisters registers;
    // The pad state the core is answering $1000 reads from for this frame.
    // It is machine state a replacement cannot otherwise see -- the joypad
    // is a device, not memory -- and the pad reader at 00:E195 is the one
    // routine whose result is a pure function of it. Handing it over here
    // rather than through the app keeps the value the transcript predicts
    // and the value the core would have returned the same object.
    InputFrame input;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
    std::uint64_t absolute_master_cycle = 0;
    // Atomic native bodies must finish strictly before the user/video event
    // or serviceable timer-IRQ boundary. A checkpointed transcript may cross
    // these horizons because the core samples and delivers interrupts at its
    // declared original instruction boundaries.
    std::uint32_t master_cycles_until_user_event =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t master_cycles_until_timer_interrupt =
        std::numeric_limits<std::uint32_t>::max();

    [[nodiscard]] bool operator==(const RoutineEntry&) const = default;
};

using ExecutionHookCallback = std::function<void(const RoutineEntry&)>;

struct ReplacementMemoryWrite {
    RoutineKey instruction;
    std::uint16_t logical_address = 0;
    std::uint8_t value = 0;
    std::uint32_t master_cycle_offset = 0;

    [[nodiscard]] bool operator==(
        const ReplacementMemoryWrite&) const = default;
};

struct ReplacementCheckpoint {
    std::uint32_t interrupt_sample_master_cycle_offset = 0;
    std::uint32_t master_cycle_offset = 0;
    RoutineKey instruction;
    CpuRegisters registers;

    [[nodiscard]] bool operator==(
        const ReplacementCheckpoint&) const = default;
};

struct RoutineReplacement {
    // The core advances to this routine's outer RTS after applying the
    // transaction. The RTS itself remains emulated, preserving stack and
    // caller-return behavior.
    RoutineKey return_instruction;
    CpuRegisters output_registers;
    std::uint32_t body_master_cycles = 0;
    // Legacy name: entries may target ordinary work RAM (physical bank F8)
    // or a bounded set of device registers on the PC Engine hardware page
    // (physical bank FF): the VDC base ports $0000-$0003, the PSG register
    // file $0800-$0809, the timer at $0C00-$0C01, and the interrupt
    // controller at $1402-$1403. Hardware-page writes are used only for
    // bounded, trace-locked device-register paths.
    std::vector<ReplacementMemoryWrite> work_ram_writes;
    // Optional instruction-boundary transcript. When present, the core
    // advances one checkpoint at a time so original interrupt handlers can
    // preempt and return between native segments.
    std::vector<ReplacementCheckpoint> checkpoints;
};

using ReplacementHookCallback = std::function<
    std::optional<RoutineReplacement>(
        const RoutineEntry&,
        std::span<const std::byte> work_ram)>;

struct ExecutionHook {
    RoutineKey routine;
    HookMode mode = HookMode::pass_through;
    ExecutionHookCallback callback;
    ReplacementHookCallback replacement_callback;
};

struct ObservedMemoryWrite {
    RoutineKey instruction;
    std::uint16_t logical_address = 0;
    std::uint16_t work_ram_offset = 0;
    std::uint8_t value = 0;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
    std::uint64_t absolute_master_cycle = 0;

    [[nodiscard]] bool operator==(
        const ObservedMemoryWrite&) const = default;
};

struct ObservedBusWrite {
    RoutineKey instruction;
    std::uint16_t logical_address = 0;
    std::uint8_t physical_bank = 0;
    std::uint8_t value = 0;
    std::uint64_t frame_number = 0;
    std::uint32_t master_cycle = 0;
    std::uint64_t absolute_master_cycle = 0;

    [[nodiscard]] bool operator==(const ObservedBusWrite&) const = default;
};

using WorkRamWriteCallback =
    std::function<void(const ObservedMemoryWrite&)>;
using BusWriteCallback = std::function<void(const ObservedBusWrite&)>;

struct RoutineCall {
    RoutineEntry entry;
    RoutineKey return_instruction;
    CpuRegisters output_registers;
    std::uint16_t caller_pc = 0;
    std::uint16_t return_pc = 0;
    std::uint32_t return_master_cycle = 0;
    std::uint64_t return_absolute_master_cycle = 0;
    std::uint64_t body_master_cycles = 0;
    std::vector<ObservedMemoryWrite> work_ram_writes;

    [[nodiscard]] bool operator==(const RoutineCall&) const = default;
};

// One executed instruction address and the master cycles attributed to it.
// Cycles are the span between this instruction's entry and the next observed
// instruction's entry, so a natively replaced routine attributes its whole
// body to its entry address. Profile with replacements disabled to rank the
// code that is still emulated.
struct InstructionProfileEntry {
    RoutineKey instruction;
    std::uint64_t executions = 0;
    std::uint64_t master_cycles = 0;

    [[nodiscard]] bool operator==(
        const InstructionProfileEntry&) const = default;
};

using RoutineCallCallback = std::function<void(const RoutineCall&)>;

struct RoutineCallObserver {
    RoutineKey routine;
    RoutineCallCallback callback;
};

// How the runtime handled replacements offered while another replacement was
// still in progress. A transcript is suspended for as long as an interrupt
// asserted inside it is being serviced, and the handler is ordinary code that
// may have a replacement of its own: `nested` counts those. `overflowed`
// counts replacements the runtime refused because too many transcripts were
// already suspended; the original assembly runs instead, so the recording stays
// exact, but a caller that credits a native call when it offers one is then
// reporting a native share slightly larger than the one it measured.
struct ReplacementNesting {
    std::uint64_t nested = 0;
    std::uint64_t overflowed = 0;
    // High-water mark of simultaneously suspended transcripts.
    std::uint64_t deepest = 0;

    [[nodiscard]] bool operator==(const ReplacementNesting&) const = default;
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

    // Work RAM crosses this boundary as an exact-size copy. Import is
    // intentionally explicit so deterministic diagnostic recordings and
    // later typed game-state views cannot retain mutable emulator pointers.
    // Both operations are unavailable until a ROM is loaded.
    [[nodiscard]] virtual std::size_t work_ram_size() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::byte> export_work_ram() const = 0;
    [[nodiscard]] virtual bool import_work_ram(
        std::span<const std::byte> bytes) = 0;

    // A copied, frame-boundary view of the base PC Engine VDC and VCE state.
    // VRAM is represented in the HuC6270's 16-bit word-addressed form.
    [[nodiscard]] virtual std::optional<VideoMemorySnapshot>
    export_video_memory() const = 0;

    // Hooks are keyed by exact physical-bank/logical-PC pairs and execute
    // after opcode fetch but before instruction execution. Each key may be
    // registered once. Pass-through and observe hooks cannot modify core
    // state; observe mode requires a callback. Replace mode requires a
    // transactional callback. Returning no replacement continues in the
    // original assembly. A replacement supplies final registers, ordered
    // writes with entry-relative timing, the body-cycle budget, and the
    // outer RTS at which emulation resumes.
    //
    // Replacement callbacks receive an isolated work-RAM snapshot plus the
    // entry's conservative master-cycle horizons to the next user/video
    // event and serviceable timer interrupt. Broad semantic schedulers
    // decline transactions that reach either horizon; exact restored-
    // assembly fixtures may separately admit a proven no-interrupt call.
    // Hardware events advance at declared write and body-cycle boundaries.
    [[nodiscard]] virtual bool install_execution_hook(ExecutionHook hook) = 0;
    [[nodiscard]] virtual bool remove_execution_hook(
        RoutineKey routine) noexcept = 0;
    virtual void clear_execution_hooks() noexcept = 0;
    [[nodiscard]] virtual std::size_t execution_hook_count()
        const noexcept = 0;

    // Counted from the start of the process, across every hook installation.
    [[nodiscard]] virtual ReplacementNesting replacement_nesting()
        const noexcept = 0;

    // A call observer begins at one exact routine entry and completes before
    // its outer RTS executes. It captures pre-return registers, the return
    // target, body master cycles (excluding RTS), and ordered writes to the
    // 8 KiB PCE work-RAM region, including writes made by nested callees.
    // Observation is read-only and callbacks run synchronously on the owner
    // thread.
    [[nodiscard]] virtual bool install_routine_call_observer(
        RoutineCallObserver observer) = 0;
    [[nodiscard]] virtual bool remove_routine_call_observer(
        RoutineKey routine) noexcept = 0;
    virtual void clear_routine_call_observers() noexcept = 0;
    [[nodiscard]] virtual std::size_t routine_call_observer_count()
        const noexcept = 0;

    // An optional whole-ROM instruction cycle profile. Unlike execution hooks
    // it is not keyed, so it ranks code nobody has thought to observe yet.
    // Enabling it is only available after a ROM has loaded; it counts every
    // executed instruction and costs roughly the same as one observation hook.
    [[nodiscard]] virtual bool set_instruction_profiling(bool enabled) = 0;
    [[nodiscard]] virtual bool instruction_profiling_enabled()
        const noexcept = 0;
    // Entries are returned in descending master-cycle order. Addresses that
    // never executed are omitted.
    [[nodiscard]] virtual std::vector<InstructionProfileEntry>
    export_instruction_profile() const = 0;

    // One optional global observer receives every write to the physical
    // 8 KiB PCE work-RAM bank. It is intended for selective state-writer
    // censuses; callers should filter logical addresses in the callback.
    [[nodiscard]] virtual bool set_work_ram_write_observer(
        WorkRamWriteCallback callback) = 0;
    virtual void clear_work_ram_write_observer() noexcept = 0;
    [[nodiscard]] virtual bool has_work_ram_write_observer()
        const noexcept = 0;

    // One optional read-only observer receives every CPU bus write. It is
    // intended for bounded hardware-port mapping; callbacks must filter
    // aggressively and cannot modify the emulated bus.
    [[nodiscard]] virtual bool set_bus_write_observer(
        BusWriteCallback callback) = 0;
    virtual void clear_bus_write_observer() noexcept = 0;
    [[nodiscard]] virtual bool has_bus_write_observer()
        const noexcept = 0;

    // Replaces the emulator's programmable sound generator with the native
    // device model in `devils_crush::audio`. The model computes the same
    // band-limited deltas from the same register writes and the same clock,
    // so the audio a frame yields does not change -- what changes is who
    // owns the device. The emulator retains only sample-rate conversion,
    // which is a property of the host rather than of the PC Engine.
    //
    // May be set before or after a ROM loads, but not mid-frame.
    [[nodiscard]] virtual bool set_native_psg(bool enabled) = 0;
    [[nodiscard]] virtual bool native_psg_enabled() const noexcept = 0;

    // Which of the model's output paths the run just executed, or nothing
    // when the emulator owned the device. A route exercises only what the
    // game asks the chip for, and this is how much of it that was.
    [[nodiscard]] virtual std::optional<audio::PcePsg::Coverage>
    native_psg_coverage() const = 0;

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
