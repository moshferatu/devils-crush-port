// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/emulation/pce_runtime.hpp"

#include "devils_crush/audio/music_voice_overlay.hpp"
#include "devils_crush/audio/pce_psg.hpp"
#include "devils_crush/cpu/huc6280_interrupt_boundary.hpp"
#include "devils_crush/cpu/huc6280_irq.hpp"
#include "devils_crush/cpu/huc6280_scheduler.hpp"
#include "devils_crush/input/pce_pad.hpp"
#include "devils_crush/video/pce_vce.hpp"
#include "devils_crush/video/pce_vdc.hpp"

#include <dc_beetle_execution_hook.h>
#include <libretro.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#ifndef DC_BEETLE_PCE_REVISION
#define DC_BEETLE_PCE_REVISION "unknown"
#endif

namespace devils_crush::emulation {
namespace {

constexpr std::size_t kMaximumFramebufferPixels = 2048U * 264U;
constexpr std::size_t kMaximumAudioSamples = 0x10000U * 2U;
constexpr std::size_t kDevilsCrushBackupRamSize = 2048U;
constexpr std::size_t kPceWorkRamSize = 8192U;
constexpr std::size_t kPceVramWordCount = DC_BEETLE_VRAM_WORD_COUNT;
constexpr std::size_t kPcePaletteEntryCount =
    DC_BEETLE_PALETTE_ENTRY_COUNT;
// Bank $45 maps to HuCard ROM bank $25. The programmer at 45:41A3 indexes
// 32-byte waveforms starting at logical $4E59 and cannot cross the bank.
constexpr std::size_t kMusicWaveformTableOffset = 0x25U * 0x2000U + 0x0E59U;
constexpr std::size_t kMusicWaveformTableEnd = 0x26U * 0x2000U;

// Device registers a replacement transcript may drive directly. Each range
// is a whole register file on the hardware page (physical bank FF) whose
// writes are ordinary stores with no wait state, so a transcript can price
// them like any other instruction:
//   $0000-$0003  VDC base ports (address/data), used by the video paths.
//   $0400-$0405  VCE register file. The palette upload at 00:EB30 points the
//                colour table at $0402/$0403 and pushes colours through
//                $0404/$0405; $0400 and $0401 are here to keep the range a
//                whole register file, and nothing writes them yet.
//   $0800-$0809  PSG register file, used by the timer-interrupt sound
//                driver, which streams DDA samples through $0806.
//   $0C00-$0C01  Timer counter and control; the sound driver stops the
//                timer through $0C01 once every channel falls idle.
//   $1000        The joypad port. Writes are the two latch lines, SEL and
//                CLR, which choose the nibble a read answers with and step
//                the multitap's cursor. The pad reader at 00:E195 drives
//                them. Scanning the ROM for absolute stores to $1000 finds
//                eight; four are its own and the other four are never
//                executed, three of them in banks the game never enters at
//                all. So a transcript that emits the same four writes
//                leaves the port in the state assembly would have left it.
//   $1402-$1403  Interrupt controller mask and acknowledge; the sound
//                driver acknowledges its own timer interrupt at $1403.
// Everything else on the hardware page stays with the core.
[[nodiscard]] constexpr bool is_replaceable_hardware_port(
    const std::uint16_t logical_address)
{
    return logical_address <= 0x0003U
        || (logical_address >= 0x0400U && logical_address <= 0x0405U)
        || (logical_address >= 0x0800U && logical_address <= 0x0809U)
        || (logical_address >= 0x0C00U && logical_address <= 0x0C01U)
        || logical_address == 0x1000U
        || (logical_address >= 0x1402U && logical_address <= 0x1403U);
}

// The controller as the native colour encoder reaches it. Every call goes
// through the fork's exported bridge, which dispatches to the native
// controller model when one is installed and to the emulator's own chip when
// none is -- so this one object serves both arms, and the two models can be
// installed in either order or one without the other.
//
// The alternative was for the host to hand the encoder a `PceVdcDevice` when
// the native controller happens to be installed. That would put the same
// decision in two places, and the two would have to agree about installation
// order as well as about the answer.
class BeetleVdcDevice final : public video::VdcDevice {
public:
    std::int32_t reset() noexcept override
    {
        return dc_beetle_vce_vdc_reset();
    }

    std::int32_t run(
        const std::int32_t clocks,
        std::uint16_t* const pixels,
        const bool skip) noexcept override
    {
        return dc_beetle_vce_vdc_run(clocks, pixels, skip ? 1 : 0);
    }

    std::int32_t hsync(const bool in_hblank) noexcept override
    {
        return dc_beetle_vce_vdc_hsync(in_hblank ? 1 : 0);
    }

    std::int32_t vsync(const bool in_vblank) noexcept override
    {
        return dc_beetle_vce_vdc_vsync(in_vblank ? 1 : 0);
    }

    void write(
        const std::uint32_t address,
        const std::uint8_t value) noexcept override
    {
        dc_beetle_vce_vdc_write(address, value);
    }

    std::uint8_t read(
        const std::uint32_t address,
        const bool peek) noexcept override
    {
        return dc_beetle_vce_vdc_read(address, peek ? 1 : 0);
    }

    [[nodiscard]] bool peek_irq() const noexcept override
    {
        return dc_beetle_vce_vdc_peek_irq() != 0;
    }

    [[nodiscard]] std::uint32_t get_register(
        const unsigned id) const noexcept override
    {
        return dc_beetle_vce_vdc_get_register(id);
    }

    void set_layer_enable_mask(const std::uint64_t mask) noexcept override
    {
        dc_beetle_vce_vdc_set_layer_enable_mask(mask);
    }

    void set_unlimited_sprites(const bool unlimited) noexcept override
    {
        dc_beetle_vce_vdc_set_unlimited_sprites(unlimited ? 1 : 0);
    }
};

struct CoreOption {
    std::string_view key;
    const char* value;
};

constexpr CoreOption kCoreOptions[] = {
    {"pce_palette", "RGB"},
    {"pce_aspect_ratio", "auto"},
    {"pce_scaling", "auto"},
    {"pce_hires_blend", "disabled"},
    {"pce_h_overscan", "auto"},
    {"pce_initial_scanline", "3"},
    {"pce_last_scanline", "242"},
    {"pce_psgrevision", "HuC6280A"},
    {"pce_resamp_quality", "0"},
    {"pce_show_advanced_input_settings", "disabled"},
    {"pce_mouse_sensitivity", "1.25"},
    {"pce_up_down_allowed", "disabled"},
    {"pce_disable_softreset", "disabled"},
    {"pce_multitap", "enabled"},
    {"pce_default_joypad_type_p1", "2 Buttons"},
    {"pce_default_joypad_type_p2", "2 Buttons"},
    {"pce_default_joypad_type_p3", "2 Buttons"},
    {"pce_default_joypad_type_p4", "2 Buttons"},
    {"pce_default_joypad_type_p5", "2 Buttons"},
    {"pce_Turbo_Toggling", "disabled"},
    {"pce_turbo_toggle_hotkey", "disabled"},
    {"pce_Turbo_Delay", "Fast"},
    {"pce_p0_turbo_I_enable", "disabled"},
    {"pce_p0_turbo_II_enable", "disabled"},
    {"pce_p1_turbo_I_enable", "disabled"},
    {"pce_p1_turbo_II_enable", "disabled"},
    {"pce_p2_turbo_I_enable", "disabled"},
    {"pce_p2_turbo_II_enable", "disabled"},
    {"pce_p3_turbo_I_enable", "disabled"},
    {"pce_p3_turbo_II_enable", "disabled"},
    {"pce_p4_turbo_I_enable", "disabled"},
    {"pce_p4_turbo_II_enable", "disabled"},
    {"pce_cdimagecache", "disabled"},
    {"pce_cdbios", "System Card 3"},
    {"pce_arcadecard", "enabled"},
    {"pce_cdspeed", "1"},
    {"pce_adpcmextraprec", "10-bit"},
    {"pce_adpcmvolume", "100"},
    {"pce_cddavolume", "100"},
    {"pce_cdpsgvolume", "100"},
    {"pce_nospritelimit", "disabled"},
    {"pce_ocmultiplier", "1"},
};

class BeetlePceRuntime;

std::atomic<BeetlePceRuntime*> active_runtime = nullptr;

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path)
{
    const auto encoded = path.u8string();
    return {
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size(),
    };
}

[[nodiscard]] constexpr std::uint32_t rgb565_to_argb8888(
    const std::uint16_t pixel) noexcept
{
    const auto red = static_cast<std::uint32_t>((pixel >> 11U) & 0x1FU);
    const auto green = static_cast<std::uint32_t>((pixel >> 5U) & 0x3FU);
    const auto blue = static_cast<std::uint32_t>(pixel & 0x1FU);

    const auto red_eight = (red << 3U) | (red >> 2U);
    const auto green_eight = (green << 2U) | (green >> 4U);
    const auto blue_eight = (blue << 3U) | (blue >> 2U);

    return 0xFF000000U | (red_eight << 16U) | (green_eight << 8U)
        | blue_eight;
}

[[nodiscard]] constexpr bool pressed(
    const std::uint16_t buttons,
    const Button button) noexcept
{
    return (buttons & static_cast<std::uint16_t>(button)) != 0U;
}

[[nodiscard]] constexpr bool routine_key_less(
    const RoutineKey left,
    const RoutineKey right) noexcept
{
    return left.physical_bank != right.physical_bank
        ? left.physical_bank < right.physical_bank
        : left.logical_pc < right.logical_pc;
}

class BeetlePceRuntime final : public PceRuntime {
    struct ActiveRoutineCall {
        RoutineCall call;
        std::uint8_t entry_stack_pointer = 0;
    };

    struct RoutineCallRegistration {
        RoutineCallObserver observer;
        std::vector<ActiveRoutineCall> active_calls;
    };

    // An `RTS` a transaction has declared it will run past, waiting for the
    // machine to arrive at the far side of it.
    struct PendingAbsorbedReturn {
        RoutineKey instruction;
        CpuRegisters registers;
        // The address and stack pointer the return lands on. Together they are
        // the signal that it has happened: no other boundary in the routine
        // reaches the caller's address with the caller's frame restored.
        std::uint16_t resume_logical_pc = 0;
        std::uint8_t resume_stack_pointer = 0;
        // Master cycles from the return instruction's boundary to the end of
        // the transaction, which is how far back the boundary sits from the one
        // that recognises it.
        std::uint32_t trailing_master_cycles = 0;
        // Where the return falls if the transaction runs to its end
        // uninterrupted. It is a prediction and a last resort: a transcript
        // suspended part-way through ends later than this by the whole of the
        // handler that suspended it, so both recognitions measure back from the
        // boundary they fire on and fall back to this only when the machine
        // gives them nothing to measure against.
        std::uint64_t absolute_master_cycle = 0;
        std::uint32_t master_cycle = 0;
        // Which transcript promised it. A checkpointed transcript can be given
        // up on part-way through, and then the return it declared never
        // happens; the core hands this token back so the promise can be taken
        // off the list instead of waiting for a landing that will not come.
        // Zero for a transcript that runs atomically, which cannot be
        // abandoned and so is never withdrawn.
        std::uint64_t transcript_token = 0;
    };

    // One abandonment site, as a key rather than a row: the transcript, where
    // the machine went on interpreting from, and why. Counted rather than
    // listed because the interesting ones repeat -- an address abandoned once
    // is noise and an address abandoned five thousand times is a routine to
    // work on.
    struct AbandonmentKey {
        RoutineKey routine;
        RoutineKey resume;
        AbandonmentReason reason = AbandonmentReason::registers;
        std::uint8_t mismatch = 0;

        [[nodiscard]] bool operator<(const AbandonmentKey& other) const
        {
            return std::tuple{
                       routine.physical_bank,
                       routine.logical_pc,
                       resume.physical_bank,
                       resume.logical_pc,
                       reason,
                       mismatch}
            < std::tuple{
                       other.routine.physical_bank,
                       other.routine.logical_pc,
                       other.resume.physical_bank,
                       other.resume.logical_pc,
                       other.reason,
                       other.mismatch};
        }
    };

public:
    BeetlePceRuntime()
    {
        BeetlePceRuntime* expected = nullptr;
        if (!active_runtime.compare_exchange_strong(
                expected,
                this,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            throw std::logic_error(
                "Beetle PCE supports only one in-process runtime instance.");
        }
    }

    ~BeetlePceRuntime() override
    {
        assert_owner_thread();
        dc_beetle_clear_psg_model();
        music_voice_overlay_.reset();
        native_psg_.reset();
        dc_beetle_clear_vdc_model();
        native_vdc_.reset();
        dc_beetle_clear_vce_model();
        native_vce_.reset();
        dc_beetle_clear_irq_model();
        native_irq_.reset();
        dc_beetle_clear_scheduler_model();
        native_scheduler_.reset();
        dc_beetle_clear_boundary_model();
        native_boundary_.reset();
        dc_beetle_clear_idle_model();
        native_idle_skip_.reset();
        dc_beetle_clear_pad_model();
        native_pad_.reset();
        shutdown_core();
        BeetlePceRuntime* expected = this;
        (void)active_runtime.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    BeetlePceRuntime(const BeetlePceRuntime&) = delete;
    BeetlePceRuntime& operator=(const BeetlePceRuntime&) = delete;

    [[nodiscard]] RuntimeInfo info() const override
    {
        assert_owner_thread();
        retro_system_info core_info{};
        retro_get_system_info(&core_info);

        return {
            .name = core_info.library_name != nullptr
                ? core_info.library_name
                : "Beetle PCE",
            .upstream_revision = DC_BEETLE_PCE_REVISION,
            .core_available = true,
        };
    }

    [[nodiscard]] RomLoadResult load_rom(
        const std::filesystem::path& path) override
    {
        assert_owner_thread();

        std::error_code error;
        const auto absolute_path = std::filesystem::absolute(path, error);
        if (error) {
            return {
                .loaded = false,
                .message = "Could not resolve the ROM path: "
                    + error.message(),
            };
        }
        if (!std::filesystem::is_regular_file(absolute_path, error)) {
            return {
                .loaded = false,
                .message = error
                    ? "Could not inspect the ROM path: " + error.message()
                    : "The ROM path does not name a regular file.",
            };
        }

        if (retro_api_version() != RETRO_API_VERSION) {
            return {
                .loaded = false,
                .message = "The Beetle PCE libretro ABI is incompatible.",
            };
        }

        std::string prepared_rom_path;
        std::string prepared_system_directory;
        try {
            prepared_rom_path = path_to_utf8(absolute_path);
            prepared_system_directory =
                path_to_utf8(absolute_path.parent_path());
            framebuffer_pixels_.reserve(kMaximumFramebufferPixels);
            audio_samples_.reserve(kMaximumAudioSamples);
        } catch (const std::exception& exception) {
            return {
                .loaded = false,
                .message =
                    "Could not prepare the emulator: "
                    + std::string(exception.what()),
            };
        }

        shutdown_core();
        rom_path_utf8_ = std::move(prepared_rom_path);
        system_directory_utf8_ =
            std::move(prepared_system_directory);

        install_callbacks();
        retro_init();
        initialized_ = true;

        const retro_game_info game{
            .path = rom_path_utf8_.c_str(),
            .data = nullptr,
            .size = 0,
            .meta = nullptr,
        };
        if (!retro_load_game(&game)) {
            shutdown_core();
            return {
                .loaded = false,
                .message = "Beetle PCE rejected the ROM image.",
            };
        }

        loaded_ = true;
        dc_beetle_reset_execution_accounting();
        retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
        retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
        retro_get_system_av_info(&av_info_);

        framebuffer_pixels_.clear();
        audio_samples_.clear();
        framebuffer_width_ = 0;
        framebuffer_height_ = 0;
        frame_number_ = 0;
        callback_failed_ = false;

        return {
            .loaded = true,
            .message = "ROM loaded through the pinned Beetle PCE core.",
        };
    }

    void reset() override
    {
        assert_owner_thread();
        if (loaded_) {
            retro_reset();
            dc_beetle_reset_execution_accounting();
            frame_number_ = 0;
            clear_active_routine_calls();
        }
    }

    [[nodiscard]] EmulatedFrame run_frame(
        const InputFrame& input) override
    {
        assert_owner_thread();
        if (!loaded_) {
            return {};
        }

        current_input_ = input;
        // The host's input, handed to the port itself. This is the whole of
        // the ownership change: with a model installed the value below is what
        // the machine reads, and `current_input_` survives only as what the
        // emulator's own input path would have been given if it were still in
        // it. The model holds this until the machine's own frame latch, so
        // when a device sees it is still the machine's decision.
        if (native_pad_) {
            native_pad_->set_port_input(0, input.player_one);
            native_pad_->set_port_input(1, input.player_two);
        }
        audio_samples_.clear();
        callback_failed_ = false;

        retro_run();
        ++frame_number_;

        if (callback_failed_) {
            clear_execution_hooks();
            clear_routine_call_observers();
            clear_work_ram_write_observer();
            return {
                .frame_number = frame_number_,
            };
        }

        return {
            .framebuffer = {
                .pixels = framebuffer_pixels_,
                .width = framebuffer_width_,
                .height = framebuffer_height_,
                .pitch_pixels = framebuffer_width_,
            },
            .audio = {
                .interleaved_stereo = audio_samples_,
                .sample_rate =
                    static_cast<int>(av_info_.timing.sample_rate + 0.5),
            },
            .frame_number = frame_number_,
        };
    }

    [[nodiscard]] std::size_t backup_ram_size() const noexcept override
    {
        assert_owner_thread();
        return backup_ram_data() != nullptr
            ? kDevilsCrushBackupRamSize
            : 0U;
    }

    [[nodiscard]] std::vector<std::byte> export_backup_ram()
        const override
    {
        assert_owner_thread();
        const auto* data = backup_ram_data();
        if (data == nullptr) {
            return {};
        }

        std::vector<std::byte> copy(kDevilsCrushBackupRamSize);
        std::memcpy(copy.data(), data, copy.size());
        return copy;
    }

    [[nodiscard]] bool import_backup_ram(
        const std::span<const std::byte> bytes) override
    {
        assert_owner_thread();
        auto* data = backup_ram_data();
        if (data == nullptr
            || bytes.size() != kDevilsCrushBackupRamSize) {
            return false;
        }

        std::memcpy(data, bytes.data(), bytes.size());
        return true;
    }

    [[nodiscard]] std::size_t work_ram_size() const noexcept override
    {
        assert_owner_thread();
        return work_ram_data() != nullptr ? kPceWorkRamSize : 0U;
    }

    [[nodiscard]] std::vector<std::byte> export_work_ram() const override
    {
        assert_owner_thread();
        const auto* data = work_ram_data();
        if (data == nullptr) {
            return {};
        }

        std::vector<std::byte> copy(kPceWorkRamSize);
        std::memcpy(copy.data(), data, copy.size());
        return copy;
    }

    [[nodiscard]] bool import_work_ram(
        const std::span<const std::byte> bytes) override
    {
        assert_owner_thread();
        auto* const data = work_ram_data();
        if (data == nullptr || bytes.size() != kPceWorkRamSize) {
            return false;
        }

        std::memcpy(data, bytes.data(), bytes.size());
        return true;
    }

    [[nodiscard]] std::optional<VideoMemorySnapshot>
    export_video_memory() const override
    {
        assert_owner_thread();
        if (!loaded_) {
            return std::nullopt;
        }

        VideoMemorySnapshot result{
            .vram_words =
                std::vector<std::uint16_t>(kPceVramWordCount),
            .sat_words =
                std::vector<std::uint16_t>(DC_BEETLE_SAT_WORD_COUNT),
            .palette_entries =
                std::vector<std::uint16_t>(kPcePaletteEntryCount),
        };
        dc_beetle_video_snapshot snapshot{
            .vram_words = result.vram_words.data(),
            .vram_word_capacity = result.vram_words.size(),
            .sat_words = result.sat_words.data(),
            .sat_word_capacity = result.sat_words.size(),
            .palette_entries = result.palette_entries.data(),
            .palette_entry_capacity = result.palette_entries.size(),
        };
        if (dc_beetle_copy_video_snapshot(&snapshot) == 0) {
            return std::nullopt;
        }
        result.memory_width_register =
            snapshot.memory_width_register;
        result.background_x_scroll = snapshot.background_x_scroll;
        result.background_y_scroll = snapshot.background_y_scroll;
        return result;
    }

    [[nodiscard]] bool install_execution_hook(
        ExecutionHook hook) override
    {
        assert_owner_thread();
        if (!loaded_
            || (hook.mode == HookMode::observe && !hook.callback)
            || (hook.mode == HookMode::replace
                && !hook.replacement_callback)
            || (hook.mode != HookMode::replace
                && hook.replacement_callback)) {
            return false;
        }

        const auto position = std::lower_bound(
            execution_hooks_.begin(),
            execution_hooks_.end(),
            hook.routine,
            [](const ExecutionHook& existing, const RoutineKey routine) {
                if (existing.routine.physical_bank
                    != routine.physical_bank) {
                    return existing.routine.physical_bank
                        < routine.physical_bank;
                }
                return existing.routine.logical_pc < routine.logical_pc;
            });
        if (position != execution_hooks_.end()
            && position->routine == hook.routine) {
            return false;
        }

        try {
            execution_hooks_.insert(position, std::move(hook));
        } catch (...) {
            return false;
        }
        refresh_core_observation_hooks();
        return true;
    }

    [[nodiscard]] bool remove_execution_hook(
        const RoutineKey routine) noexcept override
    {
        assert_owner_thread();
        const auto position = find_execution_hook(routine);
        if (position == execution_hooks_.end()) {
            return false;
        }

        execution_hooks_.erase(position);
        refresh_core_observation_hooks();
        return true;
    }

    void clear_execution_hooks() noexcept override
    {
        assert_owner_thread();
        execution_hooks_.clear();
        refresh_core_observation_hooks();
    }

    [[nodiscard]] std::size_t execution_hook_count()
        const noexcept override
    {
        assert_owner_thread();
        return execution_hooks_.size();
    }

    [[nodiscard]] ReplacementNesting replacement_nesting()
        const noexcept override
    {
        assert_owner_thread();
        return {
            .nested = dc_beetle_replacement_nested_installs,
            .overflowed = dc_beetle_replacement_nest_overflows,
            .deepest = dc_beetle_replacement_max_depth,
            .abandoned = dc_beetle_replacement_abandonments,
        };
    }

    [[nodiscard]] bool install_routine_call_observer(
        RoutineCallObserver observer) override
    {
        assert_owner_thread();
        if (!loaded_ || !observer.callback) {
            return false;
        }

        const auto position = std::lower_bound(
            routine_call_observers_.begin(),
            routine_call_observers_.end(),
            observer.routine,
            [](const RoutineCallRegistration& existing,
               const RoutineKey routine) {
                return routine_key_less(
                    existing.observer.routine,
                    routine);
            });
        if (position != routine_call_observers_.end()
            && position->observer.routine == observer.routine) {
            return false;
        }

        try {
            routine_call_observers_.insert(
                position,
                RoutineCallRegistration{
                    .observer = std::move(observer),
                });
        } catch (...) {
            return false;
        }
        refresh_core_observation_hooks();
        return true;
    }

    [[nodiscard]] bool remove_routine_call_observer(
        const RoutineKey routine) noexcept override
    {
        assert_owner_thread();
        const auto position = find_routine_call_observer(routine);
        if (position == routine_call_observers_.end()) {
            return false;
        }

        routine_call_observers_.erase(position);
        refresh_core_observation_hooks();
        return true;
    }

    void clear_routine_call_observers() noexcept override
    {
        assert_owner_thread();
        routine_call_observers_.clear();
        refresh_core_observation_hooks();
    }

    [[nodiscard]] std::size_t routine_call_observer_count()
        const noexcept override
    {
        assert_owner_thread();
        return routine_call_observers_.size();
    }

    [[nodiscard]] bool set_work_ram_write_observer(
        WorkRamWriteCallback callback) override
    {
        assert_owner_thread();
        if (!loaded_ || !callback || work_ram_write_observer_) {
            return false;
        }

        work_ram_write_observer_ = std::move(callback);
        refresh_core_observation_hooks();
        return true;
    }

    void clear_work_ram_write_observer() noexcept override
    {
        assert_owner_thread();
        work_ram_write_observer_ = {};
        refresh_core_observation_hooks();
    }

    [[nodiscard]] bool has_work_ram_write_observer()
        const noexcept override
    {
        assert_owner_thread();
        return static_cast<bool>(work_ram_write_observer_);
    }

    [[nodiscard]] std::optional<ExecutionAccounting>
    execution_accounting() const override
    {
        assert_owner_thread();
        if (!loaded_) { return std::nullopt; }
        dc_beetle_execution_accounting counters{};
        dc_beetle_get_execution_accounting(&counters);
        ExecutionAccounting result{};
        static_assert(static_cast<unsigned>(ExecutionAccounting::owner_count)
                      == DC_EXEC_OWNER_COUNT);
        result.start_master_cycle = counters.start_master_cycle;
        result.elapsed_master_cycles = counters.elapsed_master_cycles;
        std::copy_n(counters.cycles, result.cycles.size(), result.cycles.begin());
        std::copy_n(counters.opcode_dispatches, result.opcode_dispatches.size(),
                    result.opcode_dispatches.begin());
        result.translated_installs = counters.translated_installs;
        result.retained_offers = counters.retained_offers;
        result.rejected_offers = counters.rejected_offers;
        result.interrupt_entries = counters.interrupt_entries;
        result.native_interrupt_entries = counters.native_interrupt_entries;
        return result;
    }

    [[nodiscard]] bool set_instruction_profiling(
        const bool enabled) override
    {
        assert_owner_thread();
        if (!loaded_) {
            return false;
        }
        if (!enabled) {
            instruction_profile_.reset();
            refresh_core_observation_hooks();
            return true;
        }
        if (!instruction_profile_) {
            instruction_profile_ = std::make_unique<InstructionProfile>();
        }
        refresh_core_observation_hooks();
        return true;
    }

    [[nodiscard]] bool instruction_profiling_enabled()
        const noexcept override
    {
        assert_owner_thread();
        return instruction_profile_ != nullptr;
    }

    // The model is registered through the fork's C boundary, which owns the
    // decision of whether the emulator's PSG or this one receives writes and
    // time. The emulator's PSG object stays alive either way: while the model
    // is installed it is dormant, and its only remaining job is to be the
    // shape the savestate format is written through.
    [[nodiscard]] bool set_native_psg(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_psg_model();
            music_voice_overlay_.reset();
            native_psg_.reset();
            return true;
        }
        if (native_psg_) {
            return true;
        }

        native_psg_ = std::make_unique<audio::PcePsg>(
            nullptr,
            nullptr,
            audio::PcePsg::Revision::HuC6280);
        native_psg_model_ = {
            .user_data = this,
            .configure = &psg_configure,
            .power = &psg_power,
            .write = &psg_write,
            .update = &psg_update,
            .reset_timestamp = &psg_reset_timestamp,
            .capture_state = &psg_capture_state,
            .apply_state = &psg_apply_state,
        };
        dc_beetle_set_psg_model(&native_psg_model_);
        return true;
    }

    [[nodiscard]] bool native_psg_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_psg_ != nullptr;
    }

    [[nodiscard]] std::optional<audio::PcePsg::Coverage> native_psg_coverage()
        const override
    {
        assert_owner_thread();
        if (native_psg_ == nullptr) {
            return std::nullopt;
        }
        return native_psg_->coverage();
    }

    [[nodiscard]] bool set_music_voice_overlay(
        const bool enabled,
        const std::span<const std::byte> rom) override
    {
        assert_owner_thread();
        if (!enabled) {
            music_voice_overlay_.reset();
            return true;
        }
        if (music_voice_overlay_) {
            return true;
        }
        if (loaded_ || !native_psg_
            || rom.size() < kMusicWaveformTableEnd) {
            return false;
        }

        music_voice_overlay_ =
            std::make_unique<audio::MusicVoiceOverlay>(rom.subspan(
                kMusicWaveformTableOffset,
                kMusicWaveformTableEnd - kMusicWaveformTableOffset));
        return true;
    }

    [[nodiscard]] bool music_voice_overlay_enabled()
        const noexcept override
    {
        assert_owner_thread();
        return music_voice_overlay_ != nullptr;
    }

    // The same seam for the video device, with one difference that matters:
    // the model reaches back into the machine. Its interrupt line and its
    // CPU stall are the colour encoder's to answer, so the model is given the
    // fork's two exported edges rather than anything of this runtime's -- a
    // wait-state hook that always agreed would hang the machine, because the
    // device stalls by asking until it is refused.
    [[nodiscard]] bool set_native_vdc(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_vdc_model();
            native_vdc_.reset();
            return true;
        }
        if (native_vdc_) {
            return true;
        }

        native_vdc_ = std::make_unique<video::PceVdc>();
        native_vdc_->set_irq_hook(&vdc_irq_hook, nullptr);
        native_vdc_->set_wait_state_hook(&vdc_wait_state_hook, nullptr);
        native_vdc_model_ = {
            .user_data = native_vdc_.get(),
            .configure = &vdc_configure,
            .reset = &vdc_reset,
            .run = &vdc_run,
            .hsync = &vdc_hsync,
            .vsync = &vdc_vsync,
            .write = &vdc_write,
            .read = &vdc_read,
            .peek_irq = &vdc_peek_irq,
            .get_register = &vdc_get_register,
            .peek_vram = &vdc_peek_vram,
            .peek_sat = &vdc_peek_sat,
            .set_layer_enable_mask = &vdc_set_layer_enable_mask,
            .set_unlimited_sprites = &vdc_set_unlimited_sprites,
            .capture_state = &vdc_capture_state,
            .apply_state = &vdc_apply_state,
        };
        dc_beetle_set_vdc_model(&native_vdc_model_);
        return true;
    }

    [[nodiscard]] bool native_vdc_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_vdc_ != nullptr;
    }

    [[nodiscard]] std::optional<video::PceVdc::Coverage> native_vdc_coverage()
        const override
    {
        assert_owner_thread();
        if (native_vdc_ == nullptr) {
            return std::nullopt;
        }
        return native_vdc_->coverage();
    }

    // The colour encoder. A wider seam than either device before it: this
    // model owns the schedule, so the emulator's processor stops where the
    // model says, and it draws, so the frontend's framebuffer crosses the
    // boundary every frame.
    //
    // Four hooks reach back into the machine and one reaches the frontend.
    // All five are the fork's exported edges rather than anything of this
    // runtime's, for the same reason the controller's two were: they run
    // into state a device model does not have, and inventing an answer here
    // would be inventing the machine.
    [[nodiscard]] bool set_native_vce(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_vce_model();
            native_vce_.reset();
            return true;
        }
        if (native_vce_) {
            return true;
        }

        native_vce_ = std::make_unique<video::PceVce>();
        native_vce_->set_vdc(&beetle_vdc_bridge_);
        native_vce_->set_irq_hook(&vce_irq_hook, nullptr);
        native_vce_->set_steal_cycles_hook(&vce_steal_cycles_hook, nullptr);
        native_vce_->set_exit_hook(&vce_exit_hook, nullptr);
        native_vce_->set_scanline_info_hook(&vce_scanline_info_hook, nullptr);
        native_vce_->set_cd_event_hook(&vce_cd_event_hook, nullptr);
        native_vce_model_ = {
            .user_data = native_vce_.get(),
            .reset = &vce_reset,
            .reset_timestamp = &vce_reset_timestamp,
            .start_frame = &vce_start_frame,
            .start_partial = &vce_start_partial,
            .finish_partial = &vce_finish_partial,
            .frame_done = &vce_frame_done,
            .sync = &vce_sync,
            .calc_next_event = &vce_calc_next_event,
            .last_timestamp = &vce_last_timestamp,
            .set_cd_event = &vce_set_cd_event,
            .write = &vce_write,
            .read = &vce_read,
            .write_vdc = &vce_write_vdc,
            .read_vdc = &vce_read_vdc,
            .write_vdc_st = &vce_write_vdc_st,
            .irq_change_check = &vce_irq_change_check,
            .wait_state = &vce_wait_state,
            .set_surface_palette = &vce_set_surface_palette,
            .set_disabled_layer_colour = &vce_set_disabled_layer_colour,
            .set_show_horizontal_overscan =
                &vce_set_show_horizontal_overscan,
            .set_layer_enable_mask = &vce_set_layer_enable_mask,
            .set_unlimited_sprites = &vce_set_unlimited_sprites,
            .report_scanline_info = &vce_report_scanline_info,
            .scanline = &vce_scanline,
            .peek_palette = &vce_peek_palette,
            .capture_state = &vce_capture_state,
            .apply_state = &vce_apply_state,
        };
        dc_beetle_set_vce_model(&native_vce_model_);
        return true;
    }

    [[nodiscard]] bool native_vce_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_vce_ != nullptr;
    }

    [[nodiscard]] std::optional<video::PceVce::Coverage> native_vce_coverage()
        const override
    {
        assert_owner_thread();
        if (native_vce_ == nullptr) {
            return std::nullopt;
        }
        return native_vce_->coverage();
    }

    // The timer and interrupt controller. The narrowest seam in the port and
    // the only one inside the processor: no framebuffer crosses it, no sound
    // buffer, no machine edge at all. The model answers and publishes a
    // level; the emulator's processor still decides which vector wins and
    // when the line is sampled, because those are instruction-boundary
    // decisions and this device has no notion of an instruction.
    [[nodiscard]] bool set_native_irq(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_irq_model();
            native_irq_.reset();
            return true;
        }
        if (native_irq_) {
            return true;
        }

        native_irq_ = std::make_unique<cpu::HuC6280Irq>();
        native_irq_model_ = {
            .user_data = native_irq_.get(),
            .init = &irq_init,
            .reset = &irq_reset,
            .reset_timestamp = &irq_reset_timestamp,
            .acknowledge_reset = &irq_acknowledge_reset,
            .begin_request = &irq_begin_request,
            .end_request = &irq_end_request,
            .sync = &irq_sync,
            .publish = &irq_publish,
            .read_timer = &irq_read_timer,
            .write_timer = &irq_write_timer,
            .read_status = &irq_read_status,
            .peek_status = &irq_peek_status,
            .write_status = &irq_write_status,
            .timer_horizon = &irq_timer_horizon,
            .capture_state = &irq_capture_state,
            .apply_state = &irq_apply_state,
        };
        dc_beetle_set_irq_model(&native_irq_model_);
        return true;
    }

    [[nodiscard]] bool native_irq_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_irq_ != nullptr;
    }

    [[nodiscard]] std::optional<cpu::HuC6280Irq::Coverage>
    native_irq_coverage() const override
    {
        assert_owner_thread();
        if (native_irq_ == nullptr) {
            return std::nullopt;
        }
        return native_irq_->coverage();
    }

    // The clock and event scheduler. The one seam whose install moves memory
    // rather than redirecting calls: the model's register block becomes the
    // block the processor reads its clock, its near horizon and its loop flag
    // out of. Everything that *changes* those is still a call.
    //
    // The four hooks below are what the scheduler stops for. Three of them
    // are the timer, and they are wired to the fork's own edges rather than
    // straight to `native_irq_`: the scheduler must stop for whichever timer
    // the machine has, and a native scheduler beside an emulated controller
    // is a combination the census arms actually run.
    [[nodiscard]] bool set_native_scheduler(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_scheduler_model();
            native_scheduler_.reset();
            return true;
        }
        if (native_scheduler_) {
            return true;
        }

        native_scheduler_ = std::make_unique<cpu::HuC6280Scheduler>();
        native_scheduler_->set_timer_sync_hook(&scheduler_sync_timer, nullptr);
        native_scheduler_->set_timer_horizon_hook(
            &scheduler_timer_horizon,
            nullptr);
        native_scheduler_->set_timer_rebase_hook(
            &scheduler_rebase_timer,
            nullptr);
        native_scheduler_->set_user_event_hook(
            &scheduler_user_event,
            nullptr);
        native_scheduler_model_ = {
            .user_data = native_scheduler_.get(),
            .registers = scheduler_registers(*native_scheduler_),
            .init = &scheduler_init,
            .advance = &scheduler_advance,
            .advance_master = &scheduler_advance_master,
            .recalculate = &scheduler_recalculate,
            .set_event = &scheduler_set_event,
            .sync_and_reset_timestamp = &scheduler_sync_and_reset_timestamp,
            .set_overclock = &scheduler_set_overclock,
            .set_speed = &scheduler_set_speed,
            .run = &scheduler_run,
            .exit = &scheduler_exit,
        };
        dc_beetle_set_scheduler_model(&native_scheduler_model_);
        return true;
    }

    [[nodiscard]] bool native_scheduler_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_scheduler_ != nullptr;
    }

    [[nodiscard]] std::optional<cpu::HuC6280Scheduler::Coverage>
    native_scheduler_coverage() const override
    {
        assert_owner_thread();
        if (native_scheduler_ == nullptr) {
            return std::nullopt;
        }
        return native_scheduler_->coverage();
    }

    // The instruction boundary. The one seam of the six that hands over no
    // storage at all: the model holds the two latched values, the processor
    // passes in the four inputs it already has, and nothing is mirrored back.
    [[nodiscard]] bool set_native_boundary(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_boundary_model();
            native_boundary_.reset();
            return true;
        }
        if (native_boundary_) {
            return true;
        }

        native_boundary_ =
            std::make_unique<cpu::HuC6280InterruptBoundary>();
        native_boundary_model_ = {
            .user_data = native_boundary_.get(),
            .reset = &boundary_reset,
            .sample = &boundary_sample,
            .select = &boundary_select,
            .vector = &boundary_vector,
            .acknowledge_reset = &boundary_acknowledge_reset,
            .capture_state = &boundary_capture_state,
            .apply_state = &boundary_apply_state,
        };
        dc_beetle_set_boundary_model(&native_boundary_model_);
        return true;
    }

    [[nodiscard]] bool native_boundary_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_boundary_ != nullptr;
    }

    [[nodiscard]] std::optional<cpu::HuC6280InterruptBoundary::Coverage>
    native_boundary_coverage() const override
    {
        assert_owner_thread();
        if (native_boundary_ == nullptr) {
            return std::nullopt;
        }
        return native_boundary_->coverage();
    }

    // The idle fast-forward. The one install with no timing rule attached to
    // it: nothing is handed over, so nothing is stranded by installing late
    // or handing it back early.
    [[nodiscard]] bool set_native_idle_skip(
        const std::optional<cpu::HuC6280IdleSkip::Loop> loop) override
    {
        assert_owner_thread();
        if (!loop) {
            dc_beetle_clear_idle_model();
            native_idle_skip_.reset();
            return true;
        }

        native_idle_skip_ = std::make_unique<cpu::HuC6280IdleSkip>(*loop);
        native_idle_skip_model_ = {
            .user_data = native_idle_skip_.get(),
            .watch_logical_pc = loop->logical_pc,
            .watch_physical_bank = loop->physical_bank,
            .iteration_cycles = &idle_iteration_cycles,
            .advanced = &idle_advanced,
        };
        dc_beetle_set_idle_model(&native_idle_skip_model_);
        return true;
    }

    [[nodiscard]] bool native_idle_skip_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_idle_skip_ != nullptr;
    }

    [[nodiscard]] std::optional<cpu::HuC6280IdleSkip::Coverage>
    native_idle_skip_coverage() const override
    {
        assert_owner_thread();
        if (native_idle_skip_ == nullptr) {
            return std::nullopt;
        }
        return native_idle_skip_->coverage();
    }

    // The controller port. The eighth seam and the one that hands over the
    // least work: the strobe protocol the game drives it with has been native
    // since phase 1, inside the cartridge's own pad reader. What moves is the
    // input itself, which now arrives from this host rather than through the
    // emulator's frontend buffer and its five device objects.
    [[nodiscard]] bool set_native_pad(const bool enabled) override
    {
        assert_owner_thread();
        if (!enabled) {
            dc_beetle_clear_pad_model();
            native_pad_.reset();
            return true;
        }
        if (native_pad_) {
            return true;
        }

        native_pad_ = std::make_unique<input::PcePad>();
        // The host's half of the configuration. The machine's half arrives
        // through the fork, which reports what the emulator resolved from its
        // own settings; this is the one policy the machine has no opinion
        // about, and it is set to match the core option this runtime asks for.
        native_pad_->set_frontend_policy({
            .allow_opposing_directions = false,
        });
        native_pad_->set_port_input(0, current_input_.player_one);
        native_pad_->set_port_input(1, current_input_.player_two);
        native_pad_model_ = {
            .user_data = native_pad_.get(),
            .configure = &pad_configure,
            .configure_port = &pad_configure_port,
            .power = &pad_power,
            .frame = &pad_frame,
            .transform_input = &pad_transform_input,
            .adjust_timestamp = &pad_adjust_timestamp,
            .read = &pad_read,
            .write = &pad_write,
            .capture_state = &pad_capture_state,
            .apply_state = &pad_apply_state,
        };
        dc_beetle_set_pad_model(&native_pad_model_);
        return true;
    }

    [[nodiscard]] bool native_pad_enabled() const noexcept override
    {
        assert_owner_thread();
        return native_pad_ != nullptr;
    }

    [[nodiscard]] std::optional<input::PcePad::Coverage>
    native_pad_coverage() const override
    {
        assert_owner_thread();
        if (native_pad_ == nullptr) {
            return std::nullopt;
        }
        return native_pad_->coverage();
    }

    [[nodiscard]] std::vector<InstructionProfileEntry>
    export_instruction_profile() const override
    {
        assert_owner_thread();
        return export_instruction_profile(false);
    }

    [[nodiscard]] std::vector<InstructionProfileEntry>
    export_interpreted_instruction_profile() const override
    {
        assert_owner_thread();
        return export_instruction_profile(true);
    }

    [[nodiscard]] std::vector<AbandonmentSite>
    export_abandonment_census() const override
    {
        assert_owner_thread();
        std::vector<AbandonmentSite> sites;
        sites.reserve(abandonment_census_.size());
        for (const auto& [key, count] : abandonment_census_) {
            sites.push_back({
                .routine = key.routine,
                .resume = key.resume,
                .reason = key.reason,
                .mismatch = key.mismatch,
                .count = count,
            });
        }
        std::ranges::sort(
            sites,
            [](const AbandonmentSite& left, const AbandonmentSite& right) {
                if (left.count != right.count) {
                    return left.count > right.count;
                }
                return std::tuple{
                           left.routine.physical_bank,
                           left.routine.logical_pc,
                           left.resume.physical_bank,
                           left.resume.logical_pc,
                           left.reason,
                           left.mismatch}
                < std::tuple{
                           right.routine.physical_bank,
                           right.routine.logical_pc,
                           right.resume.physical_bank,
                           right.resume.logical_pc,
                           right.reason,
                           right.mismatch};
            });
        return sites;
    }

    [[nodiscard]] bool set_bus_write_observer(
        BusWriteCallback callback) override
    {
        assert_owner_thread();
        if (!loaded_ || !callback || bus_write_observer_) {
            return false;
        }
        bus_write_observer_ = std::move(callback);
        refresh_core_observation_hooks();
        return true;
    }

    void clear_bus_write_observer() noexcept override
    {
        assert_owner_thread();
        bus_write_observer_ = {};
        refresh_core_observation_hooks();
    }

    [[nodiscard]] bool has_bus_write_observer()
        const noexcept override
    {
        assert_owner_thread();
        return static_cast<bool>(bus_write_observer_);
    }

    [[nodiscard]] SaveState serialize() const override
    {
        assert_owner_thread();
        if (!loaded_) {
            return {};
        }

        const auto size = retro_serialize_size();
        if (size == 0U || size > 64U * 1024U * 1024U) {
            return {};
        }

        SaveState state{
            .core_state = std::vector<std::byte>(size),
            .frame_number = frame_number_,
        };
        if (!retro_serialize(state.core_state.data(), state.core_state.size())) {
            return {};
        }
        return state;
    }

    [[nodiscard]] bool deserialize(const SaveState& state) override
    {
        assert_owner_thread();
        if (!loaded_ || state.core_state.empty()
            || state.core_state.size() != retro_serialize_size()
            || !retro_unserialize(
                state.core_state.data(),
                state.core_state.size())) {
            return false;
        }

        frame_number_ = state.frame_number;
        dc_beetle_reset_execution_accounting();
        framebuffer_pixels_.clear();
        audio_samples_.clear();
        framebuffer_width_ = 0;
        framebuffer_height_ = 0;
        current_input_ = {};
        callback_failed_ = false;
        clear_active_routine_calls();
        return true;
    }

private:
    void assert_owner_thread() const noexcept
    {
        assert(owner_thread_ == std::this_thread::get_id());
    }

    // The fork's boundary is C, so the model reaches it as free functions
    // carrying the instance in `user_data`.
    static BeetlePceRuntime& psg_runtime(void* const user_data) noexcept
    {
        return *static_cast<BeetlePceRuntime*>(user_data);
    }

    static audio::PcePsg& psg_model(void* const user_data) noexcept
    {
        return *psg_runtime(user_data).native_psg_;
    }

    static void psg_configure(
        void* const user_data,
        std::int32_t* const left,
        std::int32_t* const right,
        const int revision)
    {
        const auto model_revision =
            revision == DC_BEETLE_PSG_REVISION_HUC6280A
            ? audio::PcePsg::Revision::HuC6280A
            : audio::PcePsg::Revision::HuC6280;
        auto& runtime = psg_runtime(user_data);
        runtime.native_psg_->configure(left, right, model_revision);
        if (runtime.music_voice_overlay_) {
            runtime.music_voice_overlay_->configure(
                left, right, model_revision);
        }
    }

    static void psg_power(
        void* const user_data,
        const std::int32_t timestamp)
    {
        auto& runtime = psg_runtime(user_data);
        runtime.native_psg_->power(timestamp);
        if (runtime.music_voice_overlay_) {
            runtime.music_voice_overlay_->power(timestamp);
        }
    }

    static void psg_write(
        void* const user_data,
        const std::int32_t timestamp,
        const std::uint8_t address,
        const std::uint8_t value)
    {
        auto& runtime = psg_runtime(user_data);
        runtime.native_psg_->write(timestamp, address, value);
        if (runtime.music_voice_overlay_) {
            const auto* const work_ram = runtime.work_ram_data();
            runtime.music_voice_overlay_->observe_primary_write(
                timestamp,
                address,
                value,
                work_ram != nullptr
                    ? std::span<const std::byte>{work_ram, kPceWorkRamSize}
                    : std::span<const std::byte>{});
        }
    }

    static void psg_update(
        void* const user_data,
        const std::int32_t timestamp)
    {
        auto& runtime = psg_runtime(user_data);
        runtime.native_psg_->update(timestamp);
        if (runtime.music_voice_overlay_) {
            runtime.music_voice_overlay_->update(timestamp);
        }
    }

    static void psg_reset_timestamp(
        void* const user_data,
        const std::int32_t base)
    {
        auto& runtime = psg_runtime(user_data);
        runtime.native_psg_->reset_timestamp(base);
        if (runtime.music_voice_overlay_) {
            runtime.music_voice_overlay_->reset_timestamp(base);
        }
    }

    static void psg_capture_state(
        void* const user_data,
        dc_beetle_psg_state* const state)
    {
        const auto captured = psg_model(user_data).capture_state();

        for (std::size_t index = 0U;
             index < captured.channels.size();
             ++index) {
            const auto& source = captured.channels[index];
            auto& target = state->channels[index];

            target.counter = source.counter;
            target.frequency = source.frequency;
            target.control = source.control;
            target.balance = source.balance;
            std::memcpy(
                target.waveform,
                source.waveform.data(),
                source.waveform.size());
            target.waveform_index = source.waveform_index;
            target.dda = source.dda;
            target.noisectrl = source.noisectrl;
            target.noisecount = source.noisecount;
            target.lfsr = source.lfsr;
            target.vl[0] = source.vl[0];
            target.vl[1] = source.vl[1];
        }

        state->select = captured.select;
        state->global_balance = captured.global_balance;
        state->lfo_frequency = captured.lfo_frequency;
        state->lfo_control = captured.lfo_control;
        state->volume_update_counter = captured.volume_update_counter;
        state->volume_update_which = captured.volume_update_which;
        state->volume_update_latch = captured.volume_update_latch;
        state->volume_update_pending =
            captured.volume_update_pending ? 1U : 0U;
    }

    static void psg_apply_state(
        void* const user_data,
        const dc_beetle_psg_state* const state)
    {
        audio::PcePsg::State restored;

        for (std::size_t index = 0U;
             index < restored.channels.size();
             ++index) {
            const auto& source = state->channels[index];
            auto& target = restored.channels[index];

            target.counter = source.counter;
            target.frequency = source.frequency;
            target.control = source.control;
            target.balance = source.balance;
            std::memcpy(
                target.waveform.data(),
                source.waveform,
                target.waveform.size());
            target.waveform_index = source.waveform_index;
            target.dda = source.dda;
            target.noisectrl = source.noisectrl;
            target.noisecount = source.noisecount;
            target.lfsr = source.lfsr;
            target.vl[0] = source.vl[0];
            target.vl[1] = source.vl[1];
        }

        restored.select = state->select;
        restored.global_balance = state->global_balance;
        restored.lfo_frequency = state->lfo_frequency;
        restored.lfo_control = state->lfo_control;
        restored.volume_update_counter = state->volume_update_counter;
        restored.volume_update_which = state->volume_update_which;
        restored.volume_update_latch = state->volume_update_latch;
        restored.volume_update_pending = state->volume_update_pending != 0U;

        psg_model(user_data).apply_state(restored);
        auto& runtime = psg_runtime(user_data);
        if (runtime.music_voice_overlay_) {
            runtime.music_voice_overlay_->invalidate(0);
        }
    }

    static video::PceVdc& vdc_model(void* const user_data) noexcept
    {
        return *static_cast<video::PceVdc*>(user_data);
    }

    // The two edges back into the machine. Neither carries this runtime's
    // identity, because neither is answered here: the fork routes them to the
    // colour encoder, which is where the CPU's interrupt level and the
    // per-frame budget of stolen cycles live.
    static void vdc_irq_hook(void*, const bool raised)
    {
        dc_beetle_vdc_irq_change(raised ? 1 : 0);
    }

    static bool vdc_wait_state_hook(void*, const std::int32_t cycles)
    {
        return dc_beetle_vdc_wait_state(cycles) != 0;
    }

    static void vdc_configure(
        void* const user_data,
        const std::uint32_t vram_size)
    {
        vdc_model(user_data).set_vram_size(vram_size);
    }

    static std::int32_t vdc_reset(void* const user_data)
    {
        return vdc_model(user_data).reset();
    }

    static std::int32_t vdc_run(
        void* const user_data,
        const std::int32_t clocks,
        std::uint16_t* const pixels,
        const int skip)
    {
        return vdc_model(user_data).run(clocks, pixels, skip != 0);
    }

    static std::int32_t vdc_hsync(void* const user_data, const int in_hblank)
    {
        return vdc_model(user_data).hsync(in_hblank != 0);
    }

    static std::int32_t vdc_vsync(void* const user_data, const int in_vblank)
    {
        return vdc_model(user_data).vsync(in_vblank != 0);
    }

    // `next_event` is left alone, because the emulator's own device leaves it
    // alone: it is a parameter of `VDC::Read` and `VDC::Write` that neither
    // body reads or assigns. Writing a horizon here would be a difference
    // from the reference, not a fidelity to it.
    static void vdc_write(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t value,
        std::int32_t*)
    {
        vdc_model(user_data).write(address, value);
    }

    static std::uint8_t vdc_read(
        void* const user_data,
        const std::uint32_t address,
        std::int32_t*,
        const int peek)
    {
        return vdc_model(user_data).read(address, peek != 0);
    }

    static int vdc_peek_irq(void* const user_data)
    {
        return vdc_model(user_data).peek_irq() ? 1 : 0;
    }

    static std::uint32_t vdc_get_register(
        void* const user_data,
        const unsigned id)
    {
        return vdc_model(user_data).get_register(
            static_cast<video::PceVdc::Register>(id));
    }

    static std::uint16_t vdc_peek_vram(
        void* const user_data,
        const std::uint16_t address)
    {
        return vdc_model(user_data).peek_vram(address);
    }

    static std::uint16_t vdc_peek_sat(
        void* const user_data,
        const std::uint8_t address)
    {
        return vdc_model(user_data).peek_sat(address);
    }

    static void vdc_set_layer_enable_mask(
        void* const user_data,
        const std::uint64_t mask)
    {
        vdc_model(user_data).set_layer_enable_mask(mask);
    }

    static void vdc_set_unlimited_sprites(
        void* const user_data,
        const int unlimited)
    {
        vdc_model(user_data).set_unlimited_sprites(unlimited != 0);
    }

    static void vdc_capture_state(
        void* const user_data,
        dc_beetle_vdc_state* const state)
    {
        video::PceVdc::State captured;
        vdc_model(user_data).capture_state(captured);

        std::memcpy(
            state->vram,
            captured.vram.data(),
            sizeof(state->vram));
        std::memcpy(state->sat, captured.sat.data(), sizeof(state->sat));

        for (std::size_t index = 0U;
             index < captured.sprite_list.size();
             ++index) {
            const auto& source = captured.sprite_list[index];
            auto& target = state->sprite_list[index];

            target.x = source.x;
            target.flags = source.flags;
            target.palette_index = source.palette_index;
            for (std::size_t plane = 0U; plane < 4U; ++plane) {
                target.pattern_data[plane] = source.pattern_data[plane];
            }
        }

        state->select = captured.select;
        state->mawr = captured.mawr;
        state->marr = captured.marr;
        state->cr = captured.cr;
        state->cr_cache = captured.cr_cache;
        state->rcr = captured.rcr;
        state->bxr = captured.bxr;
        state->byr = captured.byr;
        state->mwr = captured.mwr;
        state->hsr = captured.hsr;
        state->hdr = captured.hdr;
        state->vsr = captured.vsr;
        state->vdr = captured.vdr;
        state->vcr = captured.vcr;
        state->dcr = captured.dcr;
        state->sour = captured.sour;
        state->desr = captured.desr;
        state->lenr = captured.lenr;
        state->dvssr = captured.dvssr;
        state->status = captured.status;

        state->pending_read = captured.pending_read ? 1U : 0U;
        state->pending_read_addr = captured.pending_read_addr;
        state->read_buffer = captured.read_buffer;
        state->write_latch = captured.write_latch;
        state->pending_write = captured.pending_write ? 1U : 0U;
        state->pending_write_addr = captured.pending_write_addr;
        state->pending_write_latch = captured.pending_write_latch;

        state->sat_dma_counter = captured.sat_dma_counter;
        state->vdma_cycle_counter = captured.vdma_cycle_counter;
        state->dma_read_buffer = captured.dma_read_buffer;
        state->dma_read_write = captured.dma_read_write ? 1U : 0U;
        state->dma_running = captured.dma_running ? 1U : 0U;
        state->dma_pending = captured.dma_pending ? 1U : 0U;
        state->satb_pending = captured.satb_pending ? 1U : 0U;
        state->burst_mode = captured.burst_mode ? 1U : 0U;

        state->rcr_count = captured.rcr_count;
        state->bg_y_offset = captured.bg_y_offset;
        state->bg_x_offset = captured.bg_x_offset;
        state->bg_y_moo = captured.bg_y_moo;

        state->hsw_cache = captured.hsw_cache;
        state->hds_cache = captured.hds_cache;
        state->hdw_cache = captured.hdw_cache;
        state->hde_cache = captured.hde_cache;
        state->vds_cache = captured.vds_cache;
        state->vsw_cache = captured.vsw_cache;
        state->vdw_cache = captured.vdw_cache;
        state->vcr_cache = captured.vcr_cache;
        state->mwr_cache = captured.mwr_cache;

        state->need_rcr_inc = captured.need_rcr_inc ? 1U : 0U;
        state->need_vblank_irq_test =
            captured.need_vblank_irq_test ? 1U : 0U;
        state->need_sat_dma_test = captured.need_sat_dma_test ? 1U : 0U;
        state->need_bg_y_inc = captured.need_bg_y_inc ? 1U : 0U;

        state->h_phase = captured.h_phase;
        state->v_phase = captured.v_phase;
        state->h_phase_counter = captured.h_phase_counter;
        state->v_phase_counter = captured.v_phase_counter;

        state->sprite_cg_fetch_counter = captured.sprite_cg_fetch_counter;
        state->mystery_counter = captured.mystery_counter;
        state->mystery_phase = captured.mystery_phase ? 1U : 0U;
        state->active_sprites = captured.active_sprites;

        state->in_exhsync = captured.in_exhsync ? 1U : 0U;
        state->in_exvsync = captured.in_exvsync ? 1U : 0U;
    }

    static void vdc_apply_state(
        void* const user_data,
        const dc_beetle_vdc_state* const state)
    {
        auto restored = std::make_unique<video::PceVdc::State>();

        std::memcpy(
            restored->vram.data(),
            state->vram,
            sizeof(state->vram));
        std::memcpy(restored->sat.data(), state->sat, sizeof(state->sat));

        for (std::size_t index = 0U;
             index < restored->sprite_list.size();
             ++index) {
            const auto& source = state->sprite_list[index];
            auto& target = restored->sprite_list[index];

            target.x = source.x;
            target.flags = source.flags;
            target.palette_index = source.palette_index;
            for (std::size_t plane = 0U; plane < 4U; ++plane) {
                target.pattern_data[plane] = source.pattern_data[plane];
            }
        }

        restored->select = state->select;
        restored->mawr = state->mawr;
        restored->marr = state->marr;
        restored->cr = state->cr;
        restored->cr_cache = state->cr_cache;
        restored->rcr = state->rcr;
        restored->bxr = state->bxr;
        restored->byr = state->byr;
        restored->mwr = state->mwr;
        restored->hsr = state->hsr;
        restored->hdr = state->hdr;
        restored->vsr = state->vsr;
        restored->vdr = state->vdr;
        restored->vcr = state->vcr;
        restored->dcr = state->dcr;
        restored->sour = state->sour;
        restored->desr = state->desr;
        restored->lenr = state->lenr;
        restored->dvssr = state->dvssr;
        restored->status = state->status;

        restored->pending_read = state->pending_read != 0U;
        restored->pending_read_addr = state->pending_read_addr;
        restored->read_buffer = state->read_buffer;
        restored->write_latch = state->write_latch;
        restored->pending_write = state->pending_write != 0U;
        restored->pending_write_addr = state->pending_write_addr;
        restored->pending_write_latch = state->pending_write_latch;

        restored->sat_dma_counter = state->sat_dma_counter;
        restored->vdma_cycle_counter = state->vdma_cycle_counter;
        restored->dma_read_buffer = state->dma_read_buffer;
        restored->dma_read_write = state->dma_read_write != 0U;
        restored->dma_running = state->dma_running != 0U;
        restored->dma_pending = state->dma_pending != 0U;
        restored->satb_pending = state->satb_pending != 0U;
        restored->burst_mode = state->burst_mode != 0U;

        restored->rcr_count = state->rcr_count;
        restored->bg_y_offset = state->bg_y_offset;
        restored->bg_x_offset = state->bg_x_offset;
        restored->bg_y_moo = state->bg_y_moo;

        restored->hsw_cache = state->hsw_cache;
        restored->hds_cache = state->hds_cache;
        restored->hdw_cache = state->hdw_cache;
        restored->hde_cache = state->hde_cache;
        restored->vds_cache = state->vds_cache;
        restored->vsw_cache = state->vsw_cache;
        restored->vdw_cache = state->vdw_cache;
        restored->vcr_cache = state->vcr_cache;
        restored->mwr_cache = state->mwr_cache;

        restored->need_rcr_inc = state->need_rcr_inc != 0U;
        restored->need_vblank_irq_test = state->need_vblank_irq_test != 0U;
        restored->need_sat_dma_test = state->need_sat_dma_test != 0U;
        restored->need_bg_y_inc = state->need_bg_y_inc != 0U;

        restored->h_phase = state->h_phase;
        restored->v_phase = state->v_phase;
        restored->h_phase_counter = state->h_phase_counter;
        restored->v_phase_counter = state->v_phase_counter;

        restored->sprite_cg_fetch_counter = state->sprite_cg_fetch_counter;
        restored->mystery_counter = state->mystery_counter;
        restored->mystery_phase = state->mystery_phase != 0U;
        restored->active_sprites = state->active_sprites;

        restored->in_exhsync = state->in_exhsync != 0U;
        restored->in_exvsync = state->in_exvsync != 0U;

        vdc_model(user_data).apply_state(*restored);
    }

    static video::PceVce& vce_model(void* const user_data) noexcept
    {
        return *static_cast<video::PceVce*>(user_data);
    }

    // The four machine edges and the frontend one. None carries a context:
    // the fork's exported functions reach the single machine this process
    // has, which is the same reason the controller's two carry none.
    static void vce_irq_hook(void*, const bool raised)
    {
        dc_beetle_vce_irq(raised ? 1 : 0);
    }

    static void vce_steal_cycles_hook(void*, const std::int32_t cycles)
    {
        dc_beetle_vce_steal_cycles(cycles);
    }

    static void vce_exit_hook(void*)
    {
        dc_beetle_vce_exit();
    }

    static std::int32_t vce_cd_event_hook(
        void*,
        const std::uint32_t timestamp)
    {
        return dc_beetle_vce_cd_event(timestamp);
    }

    static void vce_scanline_info_hook(
        void*,
        const video::PceVce::ScanlineInfo& info)
    {
        const dc_beetle_vce_scanline_info reported{
            .pulse = info.pulse,
            .start = info.start,
            .width = info.width,
            .end = info.end,
            .rate = info.rate,
            .line = info.line,
            .restart = info.restart ? 1 : 0,
        };
        dc_beetle_vce_report_scanline_info(&reported);
    }

    static void vce_reset(void* const user_data)
    {
        vce_model(user_data).reset();
    }

    static void vce_reset_timestamp(
        void* const user_data,
        const std::int32_t base)
    {
        vce_model(user_data).reset_timestamp(base);
    }

    // The framebuffer crosses here. `pixels` is the emulator's `bpp_t*`,
    // which this build makes 16 bits wide, and the fork asserts that at the
    // call site rather than trusting it.
    static void vce_start_frame(
        void* const user_data,
        std::uint16_t* const framebuffer,
        const std::int32_t pitch,
        std::int32_t* const line_widths,
        const int skip,
        const int awesome_mode,
        const int scanline_start,
        const int scanline_end)
    {
        vce_model(user_data).start_frame(
            framebuffer,
            pitch,
            line_widths,
            skip != 0,
            awesome_mode != 0,
            scanline_start,
            scanline_end);
    }

    static void vce_start_partial(void* const user_data)
    {
        vce_model(user_data).start_partial();
    }

    static void vce_finish_partial(void* const user_data)
    {
        vce_model(user_data).finish_partial();
    }

    static int vce_frame_done(void* const user_data)
    {
        return vce_model(user_data).frame_done() ? 1 : 0;
    }

    static std::int32_t vce_sync(
        void* const user_data,
        const std::int32_t timestamp)
    {
        return vce_model(user_data).sync(timestamp);
    }

    static std::int32_t vce_calc_next_event(void* const user_data)
    {
        return vce_model(user_data).calc_next_event();
    }

    static std::int32_t vce_last_timestamp(void* const user_data)
    {
        return vce_model(user_data).last_timestamp();
    }

    static void vce_set_cd_event(
        void* const user_data,
        const std::int32_t cycles,
        const std::int32_t time_behind)
    {
        vce_model(user_data).set_cd_event(cycles, time_behind);
    }

    static void vce_write(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t value)
    {
        vce_model(user_data).write(address, value);
    }

    static std::uint8_t vce_read(
        void* const user_data,
        const std::uint32_t address)
    {
        return vce_model(user_data).read(address);
    }

    static void vce_write_vdc(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t value)
    {
        vce_model(user_data).write_vdc(address, value);
    }

    static std::uint8_t vce_read_vdc(
        void* const user_data,
        const std::uint32_t address)
    {
        return vce_model(user_data).read_vdc(address);
    }

    static void vce_write_vdc_st(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t value)
    {
        vce_model(user_data).write_vdc_st(address, value);
    }

    static void vce_irq_change_check(void* const user_data)
    {
        vce_model(user_data).irq_change_check();
    }

    static int vce_wait_state(
        void* const user_data,
        const std::int32_t vdc_cycles)
    {
        return vce_model(user_data).wait_state(vdc_cycles) ? 1 : 0;
    }

    static void vce_set_surface_palette(
        void* const user_data,
        const std::uint32_t* const normal,
        const std::uint32_t* const grey)
    {
        vce_model(user_data).set_surface_palette(normal, grey);
    }

    static void vce_set_disabled_layer_colour(
        void* const user_data,
        const std::uint32_t colour)
    {
        vce_model(user_data).set_disabled_layer_colour(colour);
    }

    static void vce_set_show_horizontal_overscan(
        void* const user_data,
        const int show)
    {
        vce_model(user_data).set_show_horizontal_overscan(show != 0);
    }

    static void vce_set_layer_enable_mask(
        void* const user_data,
        const std::uint64_t mask)
    {
        vce_model(user_data).set_layer_enable_mask(mask);
    }

    static void vce_set_unlimited_sprites(
        void* const user_data,
        const int unlimited)
    {
        vce_model(user_data).set_unlimited_sprites(unlimited != 0);
    }

    static void vce_report_scanline_info(void* const user_data)
    {
        vce_model(user_data).report_scanline_info();
    }

    static std::int32_t vce_scanline(void* const user_data)
    {
        return vce_model(user_data).scanline();
    }

    static std::uint16_t vce_peek_palette(
        void* const user_data,
        const std::uint16_t address)
    {
        return vce_model(user_data).peek_palette(address);
    }

    static void vce_capture_state(
        void* const user_data,
        dc_beetle_vce_state* const state)
    {
        video::PceVce::State captured;
        vce_model(user_data).capture_state(captured);

        for (std::size_t entry = 0U;
             entry < captured.color_table.size();
             ++entry) {
            state->color_table[entry] = captured.color_table[entry];
        }

        state->ws_counter = captured.ws_counter;
        state->clock_divider = captured.clock_divider;
        state->child_event[0] = captured.child_event[0];
        state->child_event[1] = captured.child_event[1];
        state->scanline = captured.scanline;
        state->pixel_offset = captured.pixel_offset;
        state->hblank_counter = captured.hblank_counter;
        state->vblank_counter = captured.vblank_counter;
        state->ctaddress = captured.ctaddress;
        state->cr = captured.cr;
        state->hblank = captured.hblank ? 1U : 0U;
        state->vblank = captured.vblank ? 1U : 0U;
        state->need_sl_reset = captured.need_sl_reset ? 1U : 0U;
    }

    static void vce_apply_state(
        void* const user_data,
        const dc_beetle_vce_state* const state)
    {
        video::PceVce::State restored;

        for (std::size_t entry = 0U;
             entry < restored.color_table.size();
             ++entry) {
            restored.color_table[entry] = state->color_table[entry];
        }

        restored.ws_counter = state->ws_counter;
        restored.clock_divider = state->clock_divider;
        restored.child_event[0] = state->child_event[0];
        restored.child_event[1] = state->child_event[1];
        restored.scanline = state->scanline;
        restored.pixel_offset = state->pixel_offset;
        restored.hblank_counter = state->hblank_counter;
        restored.vblank_counter = state->vblank_counter;
        restored.ctaddress = state->ctaddress;
        restored.cr = state->cr;
        restored.hblank = state->hblank != 0U;
        restored.vblank = state->vblank != 0U;
        restored.need_sl_reset = state->need_sl_reset != 0U;

        vce_model(user_data).apply_state(restored);
    }

    static cpu::HuC6280Irq& irq_model(void* const user_data) noexcept
    {
        return *static_cast<cpu::HuC6280Irq*>(user_data);
    }

    // No machine edges at all. This is the first device model in the port
    // that reaches nothing outside itself: the processor asks it questions
    // and reads its answers back, and the interrupt it raises is a level the
    // processor samples rather than a call the device makes.
    static void irq_init(void* const user_data)
    {
        irq_model(user_data).init();
    }

    static void irq_reset(void* const user_data)
    {
        irq_model(user_data).reset();
    }

    static void irq_reset_timestamp(
        void* const user_data,
        const std::int32_t base)
    {
        irq_model(user_data).reset_timestamp(base);
    }

    static void irq_acknowledge_reset(void* const user_data)
    {
        irq_model(user_data).acknowledge_reset();
    }

    static void irq_begin_request(
        void* const user_data,
        const std::uint32_t lines)
    {
        irq_model(user_data).begin_request(lines);
    }

    static void irq_end_request(
        void* const user_data,
        const std::uint32_t lines)
    {
        irq_model(user_data).end_request(lines);
    }

    static void irq_sync(
        void* const user_data,
        const std::int32_t timestamp)
    {
        irq_model(user_data).sync(timestamp);
    }

    // The mirror refresh, and by a long way the most frequently called of
    // these: every dispatch that can move the device ends here.
    static void irq_publish(
        void* const user_data,
        dc_beetle_irq_level* const level)
    {
        const cpu::HuC6280Irq& device = irq_model(user_data);

        level->next_event = device.next_event();
        level->requests = device.requests();
        level->mask = device.mask();
    }

    static std::uint8_t irq_read_timer(
        void* const user_data,
        const std::uint8_t io_data_buffer)
    {
        return irq_model(user_data).read_timer(io_data_buffer);
    }

    static void irq_write_timer(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t value)
    {
        irq_model(user_data).write_timer(address, value);
    }

    static std::uint8_t irq_read_status(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t io_data_buffer)
    {
        return irq_model(user_data).read_status(address, io_data_buffer);
    }

    static std::uint8_t irq_peek_status(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t io_data_buffer)
    {
        return irq_model(user_data).peek_status(address, io_data_buffer);
    }

    static void irq_write_status(
        void* const user_data,
        const std::uint32_t address,
        const std::uint8_t value)
    {
        irq_model(user_data).write_status(address, value);
    }

    static std::uint32_t irq_timer_horizon(
        void* const user_data,
        const std::int32_t timestamp,
        const int i_flag)
    {
        return irq_model(user_data).master_cycles_until_timer_interrupt(
            timestamp,
            i_flag != 0);
    }

    static void irq_capture_state(
        void* const user_data,
        dc_beetle_irq_state* const state)
    {
        cpu::HuC6280Irq::State captured;
        irq_model(user_data).capture_state(captured);

        state->irq_low = captured.irq_low;
        state->irq_mask = captured.irq_mask;
        state->timer_inreload = captured.timer_inreload ? 1U : 0U;
        state->timer_status = captured.timer_status;
        state->timer_value = captured.timer_value;
        state->timer_load = captured.timer_load;
        state->timer_div = captured.timer_div;
    }

    static void irq_apply_state(
        void* const user_data,
        const dc_beetle_irq_state* const state)
    {
        cpu::HuC6280Irq::State restored;

        restored.irq_low = state->irq_low;
        restored.irq_mask = state->irq_mask;
        restored.timer_inreload = state->timer_inreload != 0U;
        restored.timer_status = state->timer_status;
        restored.timer_value = state->timer_value;
        restored.timer_load = state->timer_load;
        restored.timer_div = state->timer_div;

        irq_model(user_data).apply_state(restored);
    }

    static cpu::HuC6280Scheduler& scheduler_model(void* const user_data)
        noexcept
    {
        return *static_cast<cpu::HuC6280Scheduler*>(user_data);
    }

    // The one place in the port where a native model's storage is handed to
    // the emulator rather than copied into it, so the two declarations of the
    // block have to agree field for field. They are declared apart on purpose:
    // the model is meant to outlive the emulator, and a model that included
    // the fork's header to name its own state would not.
    [[nodiscard]] static dc_beetle_scheduler_registers* scheduler_registers(
        cpu::HuC6280Scheduler& model) noexcept
    {
        using Native = cpu::HuC6280Scheduler::Registers;
        using Fork = dc_beetle_scheduler_registers;

        static_assert(sizeof(Native) == sizeof(Fork));
        static_assert(alignof(Native) == alignof(Fork));
        static_assert(offsetof(Native, timestamp)
            == offsetof(Fork, timestamp));
        static_assert(offsetof(Native, timestamp_fraction)
            == offsetof(Fork, timestamp_fraction));
        static_assert(offsetof(Native, overclock_rate)
            == offsetof(Fork, overclock_rate));
        static_assert(offsetof(Native, next_event)
            == offsetof(Fork, next_event));
        static_assert(offsetof(Native, next_user_event)
            == offsetof(Fork, next_user_event));
        static_assert(offsetof(Native, run_flag) == offsetof(Fork, run_flag));
        static_assert(offsetof(Native, speed) == offsetof(Fork, speed));
        static_assert(offsetof(Native, speed_shift)
            == offsetof(Fork, speed_shift));

        return reinterpret_cast<Fork*>(&model.registers());
    }

    // The three edges the scheduler stops for the timer at, and the one it
    // stops for the machine's event handler at. All four reach the fork's own
    // dispatch rather than this runtime's native controller, so that a native
    // scheduler stops for whichever timer the machine actually has.
    static void scheduler_sync_timer(void*, std::int32_t)
    {
        dc_beetle_scheduler_sync_timer();
    }

    static std::int32_t scheduler_timer_horizon(void*)
    {
        return dc_beetle_scheduler_timer_horizon();
    }

    static void scheduler_rebase_timer(void*, const std::int32_t base)
    {
        dc_beetle_scheduler_rebase_timer(base);
    }

    static std::int32_t scheduler_user_event(
        void*,
        const std::int32_t timestamp)
    {
        return dc_beetle_scheduler_user_event(timestamp);
    }

    static void scheduler_init(void* const user_data)
    {
        scheduler_model(user_data).init();
    }

    static void scheduler_advance(
        void* const user_data,
        const std::int32_t cycles)
    {
        scheduler_model(user_data).advance(cycles);
    }

    static void scheduler_advance_master(
        void* const user_data,
        const std::int32_t master_clocks)
    {
        scheduler_model(user_data).advance_master(master_clocks);
    }

    static void scheduler_recalculate(void* const user_data)
    {
        scheduler_model(user_data).recalculate();
    }

    static void scheduler_set_event(
        void* const user_data,
        const std::int32_t cycles)
    {
        scheduler_model(user_data).set_event(cycles);
    }

    static void scheduler_sync_and_reset_timestamp(
        void* const user_data,
        const std::int32_t base)
    {
        scheduler_model(user_data).sync_and_reset_timestamp(base);
    }

    static void scheduler_set_overclock(
        void* const user_data,
        const std::uint32_t rate)
    {
        scheduler_model(user_data).set_overclock(rate);
    }

    static void scheduler_set_speed(
        void* const user_data,
        const std::uint8_t speed)
    {
        scheduler_model(user_data).set_speed(speed);
    }

    static void scheduler_run(void* const user_data, const int step_mode)
    {
        scheduler_model(user_data).run(step_mode != 0);
    }

    static void scheduler_exit(void* const user_data)
    {
        scheduler_model(user_data).exit();
    }

    static cpu::HuC6280InterruptBoundary& boundary_model(
        void* const user_data) noexcept
    {
        return *static_cast<cpu::HuC6280InterruptBoundary*>(user_data);
    }

    // The lines as the fork holds them, which is also how the model names
    // them. Two values rather than a struct across the seam, because the
    // processor holds them in two registers and building a struct to take
    // them apart again is the one cost this dispatch cannot afford.
    [[nodiscard]] static cpu::HuC6280InterruptBoundary::Lines boundary_lines(
        const std::uint32_t requests,
        const std::uint8_t mask) noexcept
    {
        return cpu::HuC6280InterruptBoundary::Lines{requests, mask};
    }

    static void boundary_reset(void* const user_data)
    {
        boundary_model(user_data).reset();
    }

    static void boundary_sample(
        void* const user_data,
        const std::uint32_t requests,
        const std::uint8_t mask,
        const int i_flag_set)
    {
        boundary_model(user_data).sample(
            boundary_lines(requests, mask),
            i_flag_set != 0);
    }

    static int boundary_select(
        void* const user_data,
        const std::uint32_t requests,
        const std::uint8_t mask)
    {
        return static_cast<int>(
            boundary_model(user_data).select(boundary_lines(requests, mask)));
    }

    static std::uint16_t boundary_vector(void*, const int decision)
    {
        using Decision = cpu::HuC6280InterruptBoundary::Decision;
        return cpu::HuC6280InterruptBoundary::vector(
            static_cast<Decision>(decision));
    }

    static void boundary_acknowledge_reset(void* const user_data)
    {
        boundary_model(user_data).acknowledge_reset();
    }

    static void boundary_capture_state(
        void* const user_data,
        dc_beetle_boundary_state* const state)
    {
        cpu::HuC6280InterruptBoundary::State captured;
        boundary_model(user_data).capture_state(captured);

        state->i_flag_sample = captured.i_flag_sample;
        state->irq_sample = captured.irq_sample;
    }

    static cpu::HuC6280IdleSkip& idle_model(void* const user_data) noexcept
    {
        return *static_cast<cpu::HuC6280IdleSkip*>(user_data);
    }

    // The two views the machine offers, as spans. Both are direct pointers
    // into memory the processor has already mapped, so neither costs a bus
    // access; a null one is a logical page with no directly readable memory
    // behind it, which the model refuses like any other unrecognised state.
    static std::int32_t idle_iteration_cycles(
        void* const user_data,
        const dc_beetle_idle_state* const state)
    {
        const auto view = [](const std::uint8_t* const data,
                             const std::size_t size) {
            return data == nullptr
                ? std::span<const std::uint8_t>{}
                : std::span<const std::uint8_t>{data, size};
        };

        return idle_model(user_data).iteration_cycles(
            state->accumulator,
            view(state->code, state->code_bytes),
            // The zero page is 256 bytes at logical $2000, and the model
            // only ever indexes it with the one byte a zero-page operand
            // holds, so 256 is both the most it can reach and the least the
            // machine can have mapped there.
            view(state->zero_page, 0x100U));
    }

    static void idle_advanced(
        void* const user_data,
        const std::int32_t iterations,
        const std::int32_t master_skipped,
        const std::int32_t master_per_iteration)
    {
        idle_model(user_data).advanced(
            iterations, master_skipped, master_per_iteration);
    }

    static void boundary_apply_state(
        void* const user_data,
        const dc_beetle_boundary_state* const state)
    {
        cpu::HuC6280InterruptBoundary::State restored;

        restored.i_flag_sample = state->i_flag_sample;
        restored.irq_sample = state->irq_sample;

        boundary_model(user_data).apply_state(restored);
    }

    static input::PcePad& pad_model(void* const user_data) noexcept
    {
        return *static_cast<input::PcePad*>(user_data);
    }

    static void pad_configure(
        void* const user_data,
        const int multitap_enabled,
        const int disable_soft_reset,
        const int cd_attached)
    {
        pad_model(user_data).configure(
            multitap_enabled != 0,
            disable_soft_reset != 0,
            cd_attached != 0);
    }

    static void pad_configure_port(
        void* const user_data,
        const unsigned port,
        const int device_type)
    {
        const auto device = [device_type]() {
            switch (device_type) {
            case DC_BEETLE_PAD_DEVICE_GAMEPAD:
                return input::PcePad::Device::gamepad;
            case DC_BEETLE_PAD_DEVICE_MOUSE:
                return input::PcePad::Device::mouse;
            case DC_BEETLE_PAD_DEVICE_TSUSHINKB:
                return input::PcePad::Device::keyboard;
            default:
                return input::PcePad::Device::none;
            }
        }();

        pad_model(user_data).configure_port(port, device);
    }

    static void pad_power(void* const user_data, const std::int32_t timestamp)
    {
        pad_model(user_data).power(timestamp);
    }

    static void pad_frame(void* const user_data, const int start_frame)
    {
        pad_model(user_data).frame(start_frame != 0);
    }

    static void pad_transform_input(void* const user_data)
    {
        pad_model(user_data).transform_input();
    }

    static void pad_adjust_timestamp(
        void* const user_data,
        const std::int32_t delta)
    {
        pad_model(user_data).adjust_timestamp(delta);
    }

    static std::uint8_t pad_read(
        void* const user_data,
        const std::int32_t timestamp,
        const std::uint32_t address)
    {
        return pad_model(user_data).read(timestamp, address);
    }

    static void pad_write(
        void* const user_data,
        const std::int32_t timestamp,
        const std::uint32_t address,
        const std::uint8_t value)
    {
        pad_model(user_data).write(timestamp, address, value);
    }

    static void pad_capture_state(
        void* const user_data,
        dc_beetle_pad_state* const state)
    {
        input::PcePad::State captured;
        pad_model(user_data).capture_state(captured);

        state->select = captured.select ? 1U : 0U;
        state->clear = captured.clear ? 1U : 0U;
        state->read_index = captured.read_index;
        for (std::size_t port = 0; port < input::PcePad::kPortCount; port++) {
            state->ports[port].buttons = captured.ports[port].buttons;
            state->ports[port].select =
                captured.ports[port].select ? 1U : 0U;
            state->ports[port].clear = captured.ports[port].clear ? 1U : 0U;
            state->ports[port].six_button_half =
                captured.ports[port].six_button_half ? 1U : 0U;
        }
    }

    static void pad_apply_state(
        void* const user_data,
        const dc_beetle_pad_state* const state)
    {
        input::PcePad::State restored;

        restored.select = state->select != 0U;
        restored.clear = state->clear != 0U;
        restored.read_index = state->read_index;
        for (std::size_t port = 0; port < input::PcePad::kPortCount; port++) {
            restored.ports[port].buttons = state->ports[port].buttons;
            restored.ports[port].select = state->ports[port].select != 0U;
            restored.ports[port].clear = state->ports[port].clear != 0U;
            restored.ports[port].six_button_half =
                state->ports[port].six_button_half != 0U;
        }

        pad_model(user_data).apply_state(restored);
    }

    void install_callbacks()
    {
        retro_set_environment(&environment_callback);
        retro_set_video_refresh(&video_callback);
        retro_set_audio_sample(&audio_sample_callback);
        retro_set_audio_sample_batch(&audio_batch_callback);
        retro_set_input_poll(&input_poll_callback);
        retro_set_input_state(&input_state_callback);
    }

    // Deliberately leaves the native device models -- sound and video --
    // installed. Loading a ROM shuts the core down first, so tearing them down
    // here would uninstall them between the caller's request and the load that
    // needs them. The device choice belongs to the host for as long as the
    // runtime lives.
    void shutdown_core() noexcept
    {
        clear_execution_hooks();
        clear_routine_call_observers();
        clear_work_ram_write_observer();
        if (loaded_) {
            retro_unload_game();
            loaded_ = false;
        }
        if (initialized_) {
            retro_deinit();
            initialized_ = false;
        }

        framebuffer_pixels_.clear();
        audio_samples_.clear();
        framebuffer_width_ = 0;
        framebuffer_height_ = 0;
        frame_number_ = 0;
        current_input_ = {};
        av_info_ = {};
    }

    [[nodiscard]] static BeetlePceRuntime* active() noexcept
    {
        return active_runtime.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::vector<ExecutionHook>::iterator find_execution_hook(
        const RoutineKey routine) noexcept
    {
        const auto position = std::lower_bound(
            execution_hooks_.begin(),
            execution_hooks_.end(),
            routine,
            [](const ExecutionHook& existing, const RoutineKey key) {
                if (existing.routine.physical_bank != key.physical_bank) {
                    return existing.routine.physical_bank
                        < key.physical_bank;
                }
                return existing.routine.logical_pc < key.logical_pc;
            });
        if (position == execution_hooks_.end()
            || !(position->routine == routine)) {
            return execution_hooks_.end();
        }
        return position;
    }

    [[nodiscard]] std::vector<RoutineCallRegistration>::iterator
    find_routine_call_observer(const RoutineKey routine) noexcept
    {
        const auto position = std::lower_bound(
            routine_call_observers_.begin(),
            routine_call_observers_.end(),
            routine,
            [](const RoutineCallRegistration& existing,
               const RoutineKey key) {
                return routine_key_less(
                    existing.observer.routine,
                    key);
            });
        if (position == routine_call_observers_.end()
            || !(position->observer.routine == routine)) {
            return routine_call_observers_.end();
        }
        return position;
    }

    void clear_active_routine_calls() noexcept
    {
        for (auto& registration : routine_call_observers_) {
            registration.active_calls.clear();
        }
    }

    void refresh_core_observation_hooks() noexcept
    {
        if (execution_hooks_.empty()
            && routine_call_observers_.empty()
            && !instruction_profile_) {
            dc_beetle_clear_instruction_hook();
        } else {
            dc_beetle_set_instruction_hook(
                &instruction_hook_callback,
                this);
        }

        const bool has_replacement = std::any_of(
            execution_hooks_.begin(),
            execution_hooks_.end(),
            [](const ExecutionHook& hook) {
                return hook.mode == HookMode::replace;
            });
        if (has_replacement) {
            dc_beetle_active_translated_hook = &translated_hook_callback;
            dc_beetle_active_translated_hook_user_data = this;
            dc_beetle_set_replacement_hook(
                &replacement_hook_callback,
                this);
            dc_beetle_set_replacement_abandoned_hook(
                &replacement_abandoned_hook_callback,
                this);
        } else {
            dc_beetle_clear_replacement_hook();
            dc_beetle_active_translated_hook = nullptr;
            dc_beetle_active_translated_hook_user_data = nullptr;
            dc_beetle_clear_replacement_abandoned_hook();
        }

        if (routine_call_observers_.empty()
            && !work_ram_write_observer_
            && !bus_write_observer_) {
            dc_beetle_clear_memory_write_hook();
        } else {
            dc_beetle_set_memory_write_hook(
                &memory_write_hook_callback,
                this);
        }
    }

    // The status register as anything outside the processor is entitled to see
    // it, which is every flag except B.
    //
    // B is not state on this processor. `PHP` pushes `P | B_FLAG` and an
    // interrupt pushes `P & ~B_FLAG`, so both writers force the bit and
    // neither consults it; the only other reader of `P` in the core caches
    // `I`. What the live register happens to hold is therefore a residue of
    // whichever of those two things last ran -- a `PLP` after a `PHP` leaves it
    // set, the next interrupt leaves it clear -- and no execution can depend on
    // which.
    //
    // It has to be dropped rather than reported faithfully because a transcript
    // cannot model it. A transcript models `PHP` and `PLP` exactly and so
    // carries the bit forward, but it cannot know when an interrupt will clear
    // it, so a replaced route and an emulated one disagree about it and about
    // nothing else. Dropping it on the way out is what makes those two
    // comparable; the alternative would be to report a difference that no
    // instruction could ever observe.
    [[nodiscard]] static constexpr std::uint8_t observable_status(
        const std::uint8_t status) noexcept
    {
        return static_cast<std::uint8_t>(status & 0xEFU);
    }

    // The interrupt-disable mask in the status byte, used only to check that a
    // checkpoint claiming to have sampled it clear is one that set it.
    static constexpr std::uint8_t kInterruptDisableFlag = 0x04U;

    [[nodiscard]] static CpuRegisters cpu_registers(
        const dc_beetle_cpu_state& state) noexcept
    {
        return {
            .program_counter = state.logical_pc,
            .accumulator = state.accumulator,
            .x = state.x,
            .y = state.y,
            .stack_pointer = state.stack_pointer,
            .status = observable_status(state.status),
            .mpr = {
                state.mpr[0],
                state.mpr[1],
                state.mpr[2],
                state.mpr[3],
                state.mpr[4],
                state.mpr[5],
                state.mpr[6],
                state.mpr[7],
            },
        };
    }

    [[nodiscard]] RoutineEntry routine_entry(
        const dc_beetle_cpu_state& state) const noexcept
    {
        return {
            .routine = {
                .physical_bank = state.physical_bank,
                .logical_pc = state.logical_pc,
            },
            .registers = cpu_registers(state),
            .input = current_input_,
            .frame_number = frame_number_ + 1U,
            .master_cycle = state.master_cycle,
            .absolute_master_cycle = state.absolute_master_cycle,
            .master_cycles_until_user_event =
                state.master_cycles_until_user_event,
            .master_cycles_until_timer_interrupt =
                state.master_cycles_until_timer_interrupt,
        };
    }

    static void instruction_hook_callback(
        const dc_beetle_cpu_state* const state,
        void* const user_data) noexcept
    {
        auto* const runtime =
            static_cast<BeetlePceRuntime*>(user_data);
        if (runtime == nullptr || state == nullptr) {
            return;
        }
        if (runtime->instruction_profile_) {
            runtime->instruction_profile_->sample(
                state->physical_bank,
                state->logical_pc,
                state->absolute_master_cycle);
        }
        if (runtime->execution_hooks_.empty()
            && runtime->routine_call_observers_.empty()) {
            return;
        }

        const RoutineKey routine{
            .physical_bank = state->physical_bank,
            .logical_pc = state->logical_pc,
        };

        try {
            // Returns a transcript ended past rather than in front of are
            // closed here, at the first boundary after them, because that is
            // the first moment their writes have been delivered.
            if (!runtime->pending_absorbed_returns_.empty()) {
                runtime->complete_absorbed_calls(*state);
            }

            std::vector<std::pair<RoutineCallCallback, RoutineCall>>
                completed_calls;

            if (state->opcode == 0x60U) {
                const auto* const work_ram = runtime->work_ram_data();
                for (auto& registration
                     : runtime->routine_call_observers_) {
                    auto& active_calls = registration.active_calls;
                    for (auto active = active_calls.begin();
                         active != active_calls.end();) {
                        if (active->entry_stack_pointer
                            != state->stack_pointer) {
                            ++active;
                            continue;
                        }

                        auto& call = active->call;
                        call.return_instruction = routine;
                        call.output_registers = cpu_registers(*state);
                        call.return_master_cycle = state->master_cycle;
                        call.return_absolute_master_cycle =
                            state->absolute_master_cycle;
                        call.body_master_cycles =
                            state->absolute_master_cycle
                            - call.entry.absolute_master_cycle;

                        if (work_ram != nullptr
                            && state->mpr[1] == 0xF8U) {
                            const auto low_offset =
                                static_cast<std::uint16_t>(
                                    0x100U
                                    | static_cast<std::uint8_t>(
                                        state->stack_pointer + 1U));
                            const auto high_offset =
                                static_cast<std::uint16_t>(
                                    0x100U
                                    | static_cast<std::uint8_t>(
                                        state->stack_pointer + 2U));
                            const auto stacked_return =
                                static_cast<std::uint16_t>(
                                    std::to_integer<std::uint8_t>(
                                        work_ram[low_offset])
                                    | (static_cast<std::uint16_t>(
                                           std::to_integer<std::uint8_t>(
                                               work_ram[high_offset]))
                                       << 8U));
                            call.return_pc =
                                static_cast<std::uint16_t>(
                                    stacked_return + 1U);
                            call.caller_pc =
                                static_cast<std::uint16_t>(
                                    call.return_pc - 3U);
                        }

                        completed_calls.emplace_back(
                            registration.observer.callback,
                            std::move(call));
                        active = active_calls.erase(active);
                    }
                }
            }

            const auto observer =
                runtime->find_routine_call_observer(routine);
            if (observer
                != runtime->routine_call_observers_.end()) {
                observer->active_calls.push_back({
                    .call = {
                        .entry = runtime->routine_entry(*state),
                    },
                    .entry_stack_pointer = state->stack_pointer,
                });
            }

            const auto hook = runtime->find_execution_hook(routine);
            ExecutionHookCallback execution_callback;
            RoutineEntry entry;
            if (hook != runtime->execution_hooks_.end()) {
                execution_callback = hook->callback;
                entry = runtime->routine_entry(*state);
            }
            if (execution_callback) {
                execution_callback(entry);
            }

            for (const auto& [callback, call] : completed_calls) {
                if (callback) {
                    callback(call);
                }
            }
        } catch (...) {
            runtime->callback_failed_ = true;
            dc_beetle_clear_instruction_hook();
            dc_beetle_clear_memory_write_hook();
        }
    }

    // Remember the return a transaction is about to absorb, so the call it ends
    // can be closed when the machine arrives on the far side of it.
    //
    // The ordinary completion path is an emulated `RTS` boundary in
    // `instruction_hook_callback`, matched by stack pointer. A transcript that
    // ends past its own return deletes that boundary, so without this the call
    // stays open and is later closed by an ancestor's `RTS` at the same stack
    // pointer -- reported with that ancestor's return address, its cycle, and
    // every write made in between.
    //
    // The call this will close was opened moments ago by the instruction hook
    // at this same boundary: the core runs that hook first and the replacement
    // hook second, so the frame is already registered.
    void stage_absorbed_returns(
        const dc_beetle_cpu_state& state,
        const RoutineReplacement& result,
        const std::uint64_t transcript_token)
    {
        if (result.absorbed_returns.empty()
            || routine_call_observers_.empty()) {
            return;
        }
        for (const auto& absorbed : result.absorbed_returns) {
            // The instruction observer has already completed older calls on
            // this entry boundary, before opening any observer rooted at the
            // RTS itself. Replaying that same boundary would incorrectly
            // close the newly opened call with a zero-length body.
            if (absorbed.master_cycle_offset == 0U
                && absorbed.instruction == routine_entry(state).routine) {
                continue;
            }
            // The registers this call will be reported closing on, so they are
            // normalised exactly as an observed boundary's would be. A
            // transcript's own B would otherwise be the one column on which a
            // replaced route and an emulated one disagree.
            auto registers = absorbed.registers;
            registers.status = observable_status(registers.status);
            pending_absorbed_returns_.push_back({
                .instruction = absorbed.instruction,
                .registers = registers,
                .resume_logical_pc =
                    result.return_instruction.logical_pc,
                .resume_stack_pointer =
                    result.output_registers.stack_pointer,
                .trailing_master_cycles = result.body_master_cycles
                    - absorbed.master_cycle_offset,
                .absolute_master_cycle = state.absolute_master_cycle
                    + absorbed.master_cycle_offset,
                .master_cycle = state.master_cycle
                    + absorbed.master_cycle_offset,
                .transcript_token = transcript_token,
            });
        }
    }

    // Take back a promise the transcript that made it never kept.
    //
    // The core abandons a checkpointed transcript when the machine does not
    // come back to a resume point in the state the transcript declared -- an
    // interrupt handler that returns a register changed, or that leaves through
    // a different path entirely. The original assembly then runs from the last
    // checkpoint applied, and executes the very `RTS` this transaction said it
    // would run past; the ordinary observer closes the call on it. So the entry
    // left here is not merely undeliverable, it has a live rival, and leaving
    // it standing is the fault this exists to prevent: the next arrival at that
    // address and stack pointer is a different call, and it would be closed
    // with an abandoned transcript's registers.
    //
    // Harmless without a re-entry, which is why every family shipped before
    // this could absorb without it. `01:C510` is the one that is both long
    // enough to be cut short and re-entered at the same depth.
    void withdraw_absorbed_returns(
        const std::uint64_t transcript_token) noexcept
    {
        std::erase_if(
            pending_absorbed_returns_,
            [transcript_token](const PendingAbsorbedReturn& pending) {
                return pending.transcript_token == transcript_token;
            });
    }

    // Close the call whose absorbed return the machine has just landed on the
    // far side of.
    //
    // Waiting for a boundary rather than closing inside the replacement hook is
    // what makes the row match the emulated one. A call carries the writes made
    // inside it and a transaction's writes are delivered by the core after the
    // hook returns, so closing early would report the routine as having written
    // nothing.
    //
    // Waiting for *this* boundary rather than simply the next one is what makes
    // preemption come out right. An interrupt that lands on a checkpoint
    // suspends the transcript rather than cancelling it: the handler runs, and
    // the remaining checkpoints are applied when it returns. So the return
    // happens, but hundreds of cycles after the transaction began, and the next
    // boundary after the hook is the handler's first instruction. Recognising
    // the return by where it lands instead of when it was due keeps both
    // endings right, and keeps the reported cycle the real one -- measured back
    // from here rather than forward from a prediction.
    void complete_absorbed_calls(const dc_beetle_cpu_state& state)
    {
        // An interrupt delivered at the end of a transcript takes the resume
        // address off it and goes to the vector, so the machine never stands on
        // that address and the plain test above would not fire until the
        // handler returned -- charging the call the whole handler. It is
        // recognisable from the frame the interrupt has just pushed: three
        // bytes below the returned-to stack pointer, carrying the address the
        // return produced.
        const auto interrupt_took_the_return =
            [&state, this](const PendingAbsorbedReturn& pending) {
                const auto* const work_ram = work_ram_data();
                if (work_ram == nullptr || state.mpr[1] != 0xF8U
                    || state.stack_pointer
                        != static_cast<std::uint8_t>(
                            pending.resume_stack_pointer - 3U)) {
                    return false;
                }
                const auto low_offset = static_cast<std::uint16_t>(
                    0x100U
                    | static_cast<std::uint8_t>(
                        pending.resume_stack_pointer - 1U));
                const auto high_offset = static_cast<std::uint16_t>(
                    0x100U | pending.resume_stack_pointer);
                const auto stacked = static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(work_ram[low_offset])
                    | (static_cast<std::uint16_t>(
                           std::to_integer<std::uint8_t>(
                               work_ram[high_offset]))
                       << 8U));
                return stacked == pending.resume_logical_pc;
            };

        bool measured_from_here = true;
        auto arrived = std::find_if(
            pending_absorbed_returns_.begin(),
            pending_absorbed_returns_.end(),
            [&state](const PendingAbsorbedReturn& pending) {
                return pending.resume_logical_pc == state.logical_pc
                    && pending.resume_stack_pointer
                        == state.stack_pointer;
            });
        if (arrived == pending_absorbed_returns_.end()) {
            arrived = std::find_if(
                pending_absorbed_returns_.begin(),
                pending_absorbed_returns_.end(),
                interrupt_took_the_return);
            measured_from_here = false;
        }
        if (arrived == pending_absorbed_returns_.end()) {
            return;
        }
        const auto absorbed = *arrived;
        pending_absorbed_returns_.erase(arrived);

        // Where the deleted boundary was. The emulated observer reported the
        // `RTS` instruction's own cycle, which is this one less the cost of the
        // return.
        //
        // When the interrupt took the return instead, this boundary is past the
        // vector sequence, so the sequence has to be subtracted as well. It is
        // eight CPU cycles, and its third and fourth push the address the
        // return produced -- consecutive cycles of the same sequence, so the
        // gap between those two writes is what one CPU cycle costs at whatever
        // speed the machine is running. Measuring it rather than assuming the
        // high speed keeps this arithmetic true of a transcript written at
        // either.
        //
        // The transcript's own prediction is the fallback, and it is only a
        // fallback: it is made when the transaction is installed and takes no
        // account of a transcript being suspended part-way through. An
        // interrupt inside the routine inserts the whole handler into the body,
        // and then a second interrupt at the end recognises the return here --
        // 08:41AB does exactly this on 58 of its 73,818 calls, where the
        // prediction is short by the handler it never saw.
        auto return_absolute_master_cycle =
            state.absolute_master_cycle - absorbed.trailing_master_cycles;
        if (!measured_from_here) {
            const auto pushed_high = stack_page_write_cycle_
                [absorbed.resume_stack_pointer];
            const auto pushed_low = stack_page_write_cycle_
                [static_cast<std::uint8_t>(
                    absorbed.resume_stack_pointer - 1U)];
            if (pushed_low > pushed_high) {
                return_absolute_master_cycle -= 8U * (pushed_low - pushed_high);
            } else {
                return_absolute_master_cycle = absorbed.absolute_master_cycle;
            }
        }

        // The same instant on the emulator's own clock, which is not the same
        // arithmetic. `master_cycle` is a slice timestamp and restarts every
        // time the core is re-entered, so counting back from this boundary is
        // only right while both ends are inside one slice. A return that lands
        // in the last cycles of a slice is recognised at the first boundary of
        // the next, and subtracting there measures against the wrong origin --
        // 03:7F80 does this twice on the census route, reporting 9 where the
        // emulated observer reported 176,064. So the slice origin is recovered
        // from the absolute clock, which never restarts, and the transcript's
        // own reckoning is used only when the return is behind that origin
        // entirely, where no arithmetic on this slice's clock can reach it.
        const auto slice_start_absolute_master_cycle =
            state.absolute_master_cycle - state.master_cycle;
        const auto return_master_cycle =
            return_absolute_master_cycle >= slice_start_absolute_master_cycle
                ? static_cast<std::uint32_t>(
                      return_absolute_master_cycle
                      - slice_start_absolute_master_cycle)
                : static_cast<std::uint32_t>(
                      absorbed.master_cycle
                      + (return_absolute_master_cycle
                         - absorbed.absolute_master_cycle));

        std::vector<std::pair<RoutineCallCallback, RoutineCall>>
            completed_calls;
        for (auto& registration : routine_call_observers_) {
            auto& active_calls = registration.active_calls;
            for (auto active = active_calls.begin();
                 active != active_calls.end();) {
                if (active->entry_stack_pointer
                    != absorbed.registers.stack_pointer) {
                    ++active;
                    continue;
                }

                auto& call = active->call;
                call.return_instruction = absorbed.instruction;
                call.output_registers = absorbed.registers;
                call.return_master_cycle = return_master_cycle;
                call.return_absolute_master_cycle =
                    return_absolute_master_cycle;
                call.body_master_cycles =
                    return_absolute_master_cycle
                    - call.entry.absolute_master_cycle;
                // The transcript resumed where the `RTS` would have landed, so
                // the return address needs no second reading off the stack --
                // which is just as well, because by now an interrupt may have
                // pushed its own over it.
                call.return_pc = absorbed.resume_logical_pc;
                call.caller_pc =
                    static_cast<std::uint16_t>(call.return_pc - 3U);
                // A call's writes are the ones it made, and recognising the
                // return after the fact means a few can arrive in between --
                // when an interrupt takes the return, the three bytes of its
                // own frame. The emulated observer closed at the `RTS` and
                // never saw them.
                std::erase_if(
                    call.work_ram_writes,
                    [return_absolute_master_cycle](
                        const ObservedMemoryWrite& write) {
                        return write.absolute_master_cycle
                            > return_absolute_master_cycle;
                    });

                completed_calls.emplace_back(
                    registration.observer.callback,
                    std::move(call));
                active = active_calls.erase(active);
            }
        }
        for (const auto& [callback, call] : completed_calls) {
            if (callback) {
                callback(call);
            }
        }
    }

    static void translated_hook_callback(
        const dc_beetle_cpu_state* const state, void* const user_data) noexcept
    {
        auto* const runtime = static_cast<BeetlePceRuntime*>(user_data);
        if (runtime->instruction_profile_) {
            runtime->instruction_profile_->mark_previous_native(
                state->physical_bank, state->logical_pc,
                state->absolute_master_cycle);
        }
    }

    static void replacement_hook_callback(
        const dc_beetle_cpu_state* const state,
        dc_beetle_replacement_result* const core_result,
        void* const user_data) noexcept
    {
        auto* const runtime =
            static_cast<BeetlePceRuntime*>(user_data);
        if (runtime == nullptr || state == nullptr
            || core_result == nullptr) {
            return;
        }

        const RoutineKey routine{
            .physical_bank = state->physical_bank,
            .logical_pc = state->logical_pc,
        };
        const auto hook = runtime->find_execution_hook(routine);
        if (hook == runtime->execution_hooks_.end()
            || hook->mode != HookMode::replace
            || !hook->replacement_callback) {
            return;
        }

        try {
            const auto* const work_ram = runtime->work_ram_data();
            if (work_ram == nullptr) {
                throw std::runtime_error(
                    "Replacement hook could not snapshot work RAM.");
            }
            std::vector<std::byte> snapshot(kPceWorkRamSize);
            std::memcpy(
                snapshot.data(),
                work_ram,
                snapshot.size());

            const auto replacement = hook->replacement_callback(
                runtime->routine_entry(*state),
                snapshot);
            if (!replacement) {
                return;
            }

            const auto& result = *replacement;
            const auto write_count = result.work_ram_writes.size();
            const auto checkpoint_count = result.checkpoints.size();
            const auto native_tia_transfer = result.native_tia_transfer;
            const auto native_bbr0_instruction =
                result.native_bbr0_instruction;
            const auto native_bbr3_instruction =
                result.native_bbr3_instruction;
            const auto native_bbs2_instruction =
                result.native_bbs2_instruction;
            const auto native_rts_instruction = result.native_rts_instruction;
            const auto native_vdc_status_read_instruction =
                result.native_vdc_status_read_instruction;
            const auto native_vdc_data_read_instruction =
                result.native_vdc_data_read_instruction;
            const auto native_plp_instruction = result.native_plp_instruction;
            const auto native_sei_instruction = result.native_sei_instruction;
            const auto native_jsr_instruction = result.native_jsr_instruction;
            const auto native_bsr_instruction = result.native_bsr_instruction;
            const auto native_bcs_instruction = result.native_bcs_instruction;
            const auto native_giant_face_wait_dispatch_instruction =
                result.native_giant_face_wait_dispatch_instruction;
            const auto native_three_blobs_timer_decrement_instruction =
                result.native_three_blobs_timer_decrement_instruction;
            const auto native_jmp_instruction = result.native_jmp_instruction;
            const auto native_flipper_owner_instruction =
                result.native_flipper_owner_instruction;
            const auto native_actor_scheduler_instruction =
                result.native_actor_scheduler_instruction;
            const auto native_tii_transfer = result.native_tii_transfer;
            const auto native_capability_count =
                static_cast<unsigned>(native_tia_transfer.has_value())
                + static_cast<unsigned>(native_bbr0_instruction.has_value())
                + static_cast<unsigned>(native_bbr3_instruction.has_value())
                + static_cast<unsigned>(native_bbs2_instruction.has_value())
                + static_cast<unsigned>(native_rts_instruction.has_value())
                + static_cast<unsigned>(
                    native_vdc_status_read_instruction.has_value())
                + static_cast<unsigned>(
                    native_vdc_data_read_instruction.has_value())
                + static_cast<unsigned>(native_plp_instruction.has_value())
                + static_cast<unsigned>(native_sei_instruction.has_value())
                + static_cast<unsigned>(native_jsr_instruction.has_value())
                + static_cast<unsigned>(native_bsr_instruction.has_value())
                + static_cast<unsigned>(native_bcs_instruction.has_value())
                + static_cast<unsigned>(
                    native_giant_face_wait_dispatch_instruction.has_value())
                + static_cast<unsigned>(
                    native_three_blobs_timer_decrement_instruction.has_value())
                + static_cast<unsigned>(native_jmp_instruction.has_value())
                + static_cast<unsigned>(
                    native_flipper_owner_instruction.has_value())
                + static_cast<unsigned>(
                    native_actor_scheduler_instruction.has_value())
                + static_cast<unsigned>(native_tii_transfer.has_value());
            const auto resume_segment =
                result.return_instruction.logical_pc >> 13U;
            if (write_count > DC_BEETLE_MAX_REPLACEMENT_WRITES
                || checkpoint_count
                    > DC_BEETLE_MAX_REPLACEMENT_CHECKPOINTS
                || (native_tia_transfer
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 21555U))
                || (native_bbr0_instruction
                    && (write_count != 0U || checkpoint_count != 0U))
                || (native_bbr3_instruction
                    && (write_count != 0U || checkpoint_count != 0U))
                || (native_bbs2_instruction
                    && (write_count != 0U || checkpoint_count != 0U))
                || (native_rts_instruction
                    && (write_count != 0U || checkpoint_count != 0U))
                || (native_vdc_status_read_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 18U))
                || (native_vdc_data_read_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 12U))
                || (native_plp_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 12U))
                || (native_sei_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 6U))
                || (native_jsr_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 21U))
                || (native_bsr_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 24U))
                || (native_bcs_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || (result.body_master_cycles != 6U
                            && result.body_master_cycles != 12U)))
                || (native_giant_face_wait_dispatch_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || (result.body_master_cycles != 6U
                            && result.body_master_cycles != 12U
                            && result.body_master_cycles != 15U)))
                || (native_three_blobs_timer_decrement_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 21U))
                || (native_jmp_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || result.body_master_cycles != 12U))
                || (native_flipper_owner_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || (result.body_master_cycles != 15U
                            && result.body_master_cycles != 6U
                            && result.body_master_cycles != 12U)))
                || (native_actor_scheduler_instruction
                    && (write_count != 0U || checkpoint_count != 0U
                        || (result.body_master_cycles != 15U
                            && result.body_master_cycles != 6U
                            && result.body_master_cycles != 12U)))
                || (native_tii_transfer
                    && (write_count != 0U || checkpoint_count != 0U
                        || (result.body_master_cycles != 147489U
                            && result.body_master_cycles != 73785U
                            && result.body_master_cycles != 18465U)))
                || (native_tia_transfer && native_bbr0_instruction)
                || (native_tia_transfer && native_bbr3_instruction)
                || (native_tia_transfer && native_bbs2_instruction)
                || (native_tia_transfer && native_rts_instruction)
                || (native_tia_transfer && native_vdc_status_read_instruction)
                || (native_tia_transfer && native_plp_instruction)
                || (native_tia_transfer && native_sei_instruction)
                || (native_tia_transfer && native_jsr_instruction)
                || (native_tia_transfer && native_tii_transfer)
                || (native_bbr0_instruction && native_tii_transfer)
                || (native_bbr0_instruction && native_vdc_status_read_instruction)
                || (native_bbr0_instruction && native_plp_instruction)
                || (native_bbr0_instruction && native_sei_instruction)
                || (native_bbr0_instruction && native_jsr_instruction)
                || (native_bbr0_instruction && native_bbr3_instruction)
                || (native_bbr0_instruction && native_bbs2_instruction)
                || (native_bbr0_instruction && native_rts_instruction)
                || (native_bbr3_instruction && native_tii_transfer)
                || (native_bbr3_instruction && native_vdc_status_read_instruction)
                || (native_bbr3_instruction && native_plp_instruction)
                || (native_bbr3_instruction && native_sei_instruction)
                || (native_bbr3_instruction && native_jsr_instruction)
                || (native_bbr3_instruction && native_bbs2_instruction)
                || (native_bbr3_instruction && native_rts_instruction)
                || (native_bbs2_instruction && native_tii_transfer)
                || (native_bbs2_instruction && native_vdc_status_read_instruction)
                || (native_bbs2_instruction && native_plp_instruction)
                || (native_bbs2_instruction && native_sei_instruction)
                || (native_bbs2_instruction && native_jsr_instruction)
                || (native_bbs2_instruction && native_rts_instruction)
                || (native_rts_instruction && native_tii_transfer)
                || (native_rts_instruction && native_vdc_status_read_instruction)
                || (native_rts_instruction && native_plp_instruction)
                || (native_rts_instruction && native_sei_instruction)
                || (native_rts_instruction && native_jsr_instruction)
                || (native_vdc_status_read_instruction && native_plp_instruction)
                || (native_vdc_status_read_instruction && native_tii_transfer)
                || (native_plp_instruction && native_tii_transfer)
                || (native_plp_instruction && native_sei_instruction)
                || (native_plp_instruction && native_jsr_instruction)
                || (native_sei_instruction && native_tii_transfer)
                || (native_sei_instruction && native_jsr_instruction)
                || (native_jsr_instruction && native_tii_transfer)
                || (native_vdc_status_read_instruction && native_sei_instruction)
                || (native_vdc_status_read_instruction && native_jsr_instruction)
                || native_capability_count > 1U
                || result.output_registers.program_counter
                    != result.return_instruction.logical_pc
                || result.output_registers.mpr[resume_segment]
                    != result.return_instruction.physical_bank) {
                throw std::runtime_error(
                    "Replacement hook returned an invalid transaction.");
            }

            std::uint32_t previous_cycle = 0;
            for (const auto& write : result.work_ram_writes) {
                const auto segment = write.logical_address >> 13U;
                const auto physical_bank = state->mpr[segment];
                const bool bounded_hardware_port =
                    physical_bank == 0xFFU
                    && is_replaceable_hardware_port(
                        write.logical_address);
                if (write.master_cycle_offset < previous_cycle
                    || write.master_cycle_offset
                        > result.body_master_cycles
                    || (physical_bank != 0xF8U
                        && !bounded_hardware_port)) {
                    throw std::runtime_error(
                        "Replacement hook returned an invalid write.");
                }
                previous_cycle = write.master_cycle_offset;
            }

            // Replacement commits now flow through the same memory-write
            // callback as interpreted stores.  Mirroring this list here as
            // well duplicates every native work-RAM write in routine and
            // range traces while leaving the final machine state unchanged.

            previous_cycle = 0;
            for (const auto& checkpoint : result.checkpoints) {
                if (checkpoint.interrupt_sample_master_cycle_offset
                        < previous_cycle
                    || checkpoint.interrupt_sample_master_cycle_offset
                        > checkpoint.master_cycle_offset
                    || checkpoint.master_cycle_offset
                        > result.body_master_cycles) {
                    throw std::runtime_error(
                        "Replacement hook returned an invalid checkpoint.");
                }
                // A boundary can only claim to have sampled the mask clear if
                // it retired with the mask set, because the claim exists to
                // describe the one instruction that closes a window it was
                // still standing inside. Declared anywhere else it would open
                // a window the machine had shut, so it is refused here rather
                // than quietly handed to the core.
                if (checkpoint.samples_interrupt_flag_clear
                    && (checkpoint.registers.status
                        & kInterruptDisableFlag) == 0U) {
                    throw std::runtime_error(
                        "Replacement hook returned a checkpoint that samples "
                        "the interrupt mask clear without setting it.");
                }
                previous_cycle = checkpoint.master_cycle_offset;
            }
            if (!result.checkpoints.empty()
                && (previous_cycle != result.body_master_cycles
                    || result.checkpoints.back().registers
                        != result.output_registers)) {
                throw std::runtime_error(
                    "Replacement hook returned an incomplete checkpoint "
                    "transcript.");
            }

            // A declared return has to be the one this transcript ends on. That
            // is the only shape the observer can recognise, because the signal
            // it waits for is the transaction's own resume: an interior return,
            // absorbed on behalf of a routine this one composed, lands at an
            // address the machine passes through without ever stopping on.
            for (const auto& absorbed : result.absorbed_returns) {
                if (result.absorbed_returns.size() > 1U
                    || absorbed.master_cycle_offset
                        >= result.body_master_cycles
                    || static_cast<std::uint8_t>(
                           absorbed.registers.stack_pointer + 2U)
                        != result.output_registers.stack_pointer) {
                    throw std::runtime_error(
                        "Replacement hook returned an invalid absorbed "
                        "return.");
                }
            }
            // Named only if it can be abandoned. An atomic transcript is
            // applied whole inside the core's own loop iteration, so there is
            // no moment at which it can be given up on and nothing for the
            // core to hand back; zero says so.
            //
            // The name carries the routine it was issued for as well as a
            // serial. The core hands the token back and nothing else, so
            // without this the host would have to keep every open transcript in
            // a map to answer "which one" -- and it is never told which ones
            // closed, because completion reaches no hook. A name that says what
            // it names needs no such map. The serial keeps it unique, which is
            // what the withdrawal below matches on.
            const std::uint64_t transcript_token = checkpoint_count > 0U
                ? ((++runtime->next_transcript_token_ << 24U)
                   | (static_cast<std::uint64_t>(routine.physical_bank) << 16U)
                   | static_cast<std::uint64_t>(routine.logical_pc))
                : 0U;
            runtime->stage_absorbed_returns(
                *state,
                result,
                transcript_token);
            core_result->transcript_token = transcript_token;

            if (native_tia_transfer) {
                core_result->native_tia_transfer = 1U;
                core_result->native_tia_source = native_tia_transfer->source;
                core_result->native_tia_destination =
                    native_tia_transfer->destination;
                core_result->native_tia_length = native_tia_transfer->length;
            }
            if (native_bbr0_instruction) {
                core_result->native_bbr0_instruction = 1U;
            }
            if (native_bbr3_instruction) {
                core_result->native_bbr3_instruction = 1U;
            }
            if (native_bbs2_instruction) {
                core_result->native_bbs2_instruction = 1U;
            }
            if (native_rts_instruction) {
                core_result->native_rts_instruction = 1U;
            }
            if (native_vdc_status_read_instruction) {
                core_result->native_vdc_status_read_instruction = 1U;
            }
            if (native_vdc_data_read_instruction) {
                core_result->native_vdc_data_read_instruction = 1U;
            }
            if (native_plp_instruction) {
                core_result->native_plp_instruction = 1U;
            }
            if (native_sei_instruction) {
                core_result->native_sei_instruction = 1U;
            }
            if (native_jsr_instruction) {
                core_result->native_jsr_instruction = 1U;
            }
            if (native_bsr_instruction) {
                core_result->native_bsr_instruction = 1U;
            }
            if (native_bcs_instruction) {
                core_result->native_bcs_instruction = 1U;
            }
            if (native_giant_face_wait_dispatch_instruction) {
                core_result->native_giant_face_wait_dispatch_instruction = 1U;
            }
            if (native_three_blobs_timer_decrement_instruction) {
                core_result->native_three_blobs_timer_decrement_instruction = 1U;
            }
            if (native_jmp_instruction) {
                core_result->native_jmp_instruction = 1U;
            }
            if (native_flipper_owner_instruction) {
                core_result->native_flipper_owner_instruction = 1U;
            }
            if (native_actor_scheduler_instruction) {
                core_result->native_actor_scheduler_instruction = 1U;
            }
            if (native_tii_transfer) {
                core_result->native_tii_transfer = 1U;
                core_result->native_tii_source = native_tii_transfer->source;
                core_result->native_tii_destination = native_tii_transfer->destination;
                core_result->native_tii_length = native_tii_transfer->length;
            }

            core_result->resume_logical_pc =
                result.return_instruction.logical_pc;
            core_result->accumulator =
                result.output_registers.accumulator;
            core_result->x = result.output_registers.x;
            core_result->y = result.output_registers.y;
            core_result->stack_pointer =
                result.output_registers.stack_pointer;
            // Normalised on the way in as well as on the way out. A transcript
            // models `PLP` faithfully and so carries a B the machine may not
            // have; handing that to the core would write a modelling artifact
            // into the register the emulator saves.
            core_result->status =
                observable_status(result.output_registers.status);
            std::copy(
                result.output_registers.mpr.begin(),
                result.output_registers.mpr.end(),
                core_result->mpr);
            core_result->body_master_cycles =
                result.body_master_cycles;
            core_result->write_count =
                static_cast<std::uint32_t>(write_count);
            for (std::size_t index = 0; index < write_count; ++index) {
                const auto& write = result.work_ram_writes[index];
                runtime->replacement_writes_[index] = {
                    .logical_address = write.logical_address,
                    .value = write.value,
                    .instruction_logical_pc =
                        write.instruction.logical_pc,
                    .instruction_physical_bank =
                        write.instruction.physical_bank,
                    .master_cycle_offset =
                        write.master_cycle_offset,
                };
            }
            core_result->writes =
                runtime->replacement_writes_.data();
            core_result->checkpoint_count =
                static_cast<std::uint32_t>(checkpoint_count);
            for (std::size_t index = 0;
                 index < checkpoint_count;
                 ++index) {
                const auto& checkpoint = result.checkpoints[index];
                auto& core_checkpoint =
                    runtime->replacement_checkpoints_[index];
                core_checkpoint.interrupt_sample_master_cycle_offset =
                    checkpoint.interrupt_sample_master_cycle_offset;
                core_checkpoint.master_cycle_offset =
                    checkpoint.master_cycle_offset;
                core_checkpoint.instruction_logical_pc =
                    checkpoint.instruction.logical_pc;
                core_checkpoint.instruction_physical_bank =
                    checkpoint.instruction.physical_bank;
                core_checkpoint.resume_logical_pc =
                    checkpoint.registers.program_counter;
                core_checkpoint.accumulator =
                    checkpoint.registers.accumulator;
                core_checkpoint.x = checkpoint.registers.x;
                core_checkpoint.y = checkpoint.registers.y;
                core_checkpoint.stack_pointer =
                    checkpoint.registers.stack_pointer;
                core_checkpoint.status =
                    observable_status(checkpoint.registers.status);
                core_checkpoint.samples_interrupt_flag_clear =
                    checkpoint.samples_interrupt_flag_clear ? 1U : 0U;
                std::copy(
                    checkpoint.registers.mpr.begin(),
                    checkpoint.registers.mpr.end(),
                    core_checkpoint.mpr);
            }
            core_result->checkpoints =
                runtime->replacement_checkpoints_.data();

            core_result->handled = 1U;
        } catch (...) {
            runtime->callback_failed_ = true;
            dc_beetle_clear_instruction_hook();
            dc_beetle_clear_memory_write_hook();
            dc_beetle_clear_replacement_hook();
            dc_beetle_active_translated_hook = nullptr;
            dc_beetle_active_translated_hook_user_data = nullptr;
            dc_beetle_clear_replacement_abandoned_hook();
        }
    }

    static void replacement_abandoned_hook_callback(
        const std::uint64_t transcript_token,
        const std::uint16_t resume_logical_pc,
        const std::uint8_t resume_physical_bank,
        const std::uint8_t reason,
        const std::uint8_t detail,
        void* const user_data) noexcept
    {
        auto* const runtime =
            static_cast<BeetlePceRuntime*>(user_data);
        if (runtime == nullptr || transcript_token == 0U) {
            return;
        }
        // The token names the routine it was issued for in its low
        // twenty-four bits, so the transcript identifies itself and no record
        // of open transcripts has to be kept. Nothing tells this runtime when
        // one completes, and completion is the common ending.
        runtime->abandonment_census_[{
            .routine =
                {
                    .physical_bank = static_cast<std::uint8_t>(
                        (transcript_token >> 16U) & 0xFFU),
                    .logical_pc = static_cast<std::uint16_t>(
                        transcript_token & 0xFFFFU),
                },
            .resume =
                {
                    .physical_bank = resume_physical_bank,
                    .logical_pc = resume_logical_pc,
                },
            .reason = static_cast<AbandonmentReason>(reason),
            .mismatch = detail,
        }] += 1U;
        if (runtime->pending_absorbed_returns_.empty()) {
            return;
        }
        runtime->withdraw_absorbed_returns(transcript_token);
    }

    static void memory_write_hook_callback(
        const dc_beetle_memory_write* const write,
        void* const user_data) noexcept
    {
        auto* const runtime =
            static_cast<BeetlePceRuntime*>(user_data);
        if (runtime == nullptr || write == nullptr
            || (runtime->routine_call_observers_.empty()
                && !runtime->work_ram_write_observer_
                && !runtime->bus_write_observer_)) {
            return;
        }

        try {
            if (runtime->bus_write_observer_) {
                runtime->bus_write_observer_({
                    .instruction = {
                        .physical_bank =
                            write->instruction_physical_bank,
                        .logical_pc =
                            write->instruction_logical_pc,
                    },
                    .logical_address = write->logical_address,
                    .physical_bank = write->physical_bank,
                    .value = write->value,
                    .frame_number = runtime->frame_number_ + 1U,
                    .master_cycle = write->master_cycle,
                    .absolute_master_cycle =
                        write->absolute_master_cycle,
                });
            }
            if (write->physical_bank != 0xF8U) {
                return;
            }
            // When each byte of the stack page was last written. An absorbed
            // return that an interrupt takes rather than lands on is recognised
            // from the frame that interrupt pushed, and these are the only
            // record of when the push happened.
            if ((write->logical_address & 0xFF00U) == 0x2100U) {
                runtime->stack_page_write_cycle_
                    [write->logical_address & 0xFFU] =
                        write->absolute_master_cycle;
            }
            const ObservedMemoryWrite observed{
                .instruction = {
                    .physical_bank =
                        write->instruction_physical_bank,
                    .logical_pc = write->instruction_logical_pc,
                },
                .logical_address = write->logical_address,
                .work_ram_offset = static_cast<std::uint16_t>(
                    write->logical_address & 0x1FFFU),
                .value = write->value,
                .frame_number = runtime->frame_number_ + 1U,
                .master_cycle = write->master_cycle,
                .absolute_master_cycle =
                    write->absolute_master_cycle,
            };
            for (auto& registration
                 : runtime->routine_call_observers_) {
                for (auto& active : registration.active_calls) {
                    active.call.work_ram_writes.push_back(observed);
                }
            }
            if (runtime->work_ram_write_observer_) {
                runtime->work_ram_write_observer_(observed);
            }
        } catch (...) {
            runtime->callback_failed_ = true;
            dc_beetle_clear_instruction_hook();
            dc_beetle_clear_memory_write_hook();
        }
    }

    [[nodiscard]] std::byte* backup_ram_data() const noexcept
    {
        if (!loaded_
            || retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)
                != kDevilsCrushBackupRamSize) {
            return nullptr;
        }
        return static_cast<std::byte*>(
            retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
    }

    [[nodiscard]] std::byte* work_ram_data() const noexcept
    {
        if (!loaded_
            || retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == nullptr
            || retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)
                < kPceWorkRamSize) {
            return nullptr;
        }
        return static_cast<std::byte*>(
            retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    }

    [[nodiscard]] static const char* option_value(
        const char* key) noexcept
    {
        if (key == nullptr) {
            return nullptr;
        }
        for (const auto& option : kCoreOptions) {
            if (option.key == key) {
                return option.value;
            }
        }
        return nullptr;
    }

    static bool RETRO_CALLCONV environment_callback(
        const unsigned command,
        void* data)
    {
        auto* runtime = active();
        if (runtime == nullptr) {
            return false;
        }

        switch (command) {
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            if (data == nullptr) {
                return false;
            }
            *static_cast<const char**>(data) =
                runtime->system_directory_utf8_.c_str();
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return data != nullptr
                && *static_cast<const retro_pixel_format*>(data)
                == RETRO_PIXEL_FORMAT_RGB565;

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            if (data == nullptr) {
                return false;
            }
            static_cast<retro_log_callback*>(data)->log = &log_callback;
            return true;

        case RETRO_ENVIRONMENT_GET_VARIABLE:
            if (data == nullptr) {
                return false;
            }
            {
                auto* variable = static_cast<retro_variable*>(data);
                variable->value = option_value(variable->key);
                return variable->value != nullptr;
            }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            if (data == nullptr) {
                return false;
            }
            *static_cast<bool*>(data) = false;
            return true;

        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            if (data == nullptr) {
                return false;
            }
            *static_cast<int*>(data) = 3;
            return true;

        case RETRO_ENVIRONMENT_SET_GEOMETRY:
            if (data == nullptr) {
                return false;
            }
            runtime->av_info_.geometry =
                *static_cast<const retro_game_geometry*>(data);
            return true;

        default:
            return false;
        }
    }

    static void RETRO_CALLCONV log_callback(
        retro_log_level,
        const char*,
        ...)
    {
    }

    static void RETRO_CALLCONV video_callback(
        const void* data,
        const unsigned width,
        const unsigned height,
        const std::size_t pitch)
    {
        auto* runtime = active();
        if (runtime == nullptr || data == nullptr) {
            return;
        }

        if (width == 0U || height == 0U || width > 2048U
            || height > 264U
            || pitch < static_cast<std::size_t>(width) * 2U) {
            runtime->callback_failed_ = true;
            return;
        }

        const auto pixel_count =
            static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height);
        if (pixel_count > runtime->framebuffer_pixels_.capacity()) {
            runtime->callback_failed_ = true;
            return;
        }

        try {
            runtime->framebuffer_pixels_.resize(pixel_count);
        } catch (...) {
            runtime->callback_failed_ = true;
            return;
        }

        const auto* source_bytes = static_cast<const std::byte*>(data);
        for (unsigned y = 0; y < height; ++y) {
            const auto* source = reinterpret_cast<const std::uint16_t*>(
                source_bytes + static_cast<std::size_t>(y) * pitch);
            auto* destination =
                runtime->framebuffer_pixels_.data()
                + static_cast<std::size_t>(y) * width;

            for (unsigned x = 0; x < width; ++x) {
                destination[x] = rgb565_to_argb8888(source[x]);
            }
        }

        runtime->framebuffer_width_ = static_cast<int>(width);
        runtime->framebuffer_height_ = static_cast<int>(height);
    }

    static void RETRO_CALLCONV audio_sample_callback(
        const std::int16_t left,
        const std::int16_t right)
    {
        auto* runtime = active();
        if (runtime == nullptr) {
            return;
        }

        const auto old_size = runtime->audio_samples_.size();
        if (old_size + 2U > runtime->audio_samples_.capacity()) {
            runtime->callback_failed_ = true;
            return;
        }

        try {
            runtime->audio_samples_.resize(old_size + 2U);
        } catch (...) {
            runtime->callback_failed_ = true;
            return;
        }

        runtime->audio_samples_[old_size] =
            static_cast<float>(left) / 32768.0F;
        runtime->audio_samples_[old_size + 1U] =
            static_cast<float>(right) / 32768.0F;
    }

    static std::size_t RETRO_CALLCONV audio_batch_callback(
        const std::int16_t* data,
        const std::size_t frames)
    {
        auto* runtime = active();
        if (runtime == nullptr || (data == nullptr && frames != 0U)) {
            return 0;
        }
        if (frames == 0U) {
            return 0;
        }

        constexpr std::size_t channels = 2U;
        if (frames > std::numeric_limits<std::size_t>::max() / channels) {
            runtime->callback_failed_ = true;
            return frames;
        }

        const auto sample_count = frames * channels;
        const auto old_size = runtime->audio_samples_.size();
        if (sample_count > runtime->audio_samples_.capacity() - old_size) {
            runtime->callback_failed_ = true;
            return frames;
        }

        try {
            runtime->audio_samples_.resize(old_size + sample_count);
        } catch (...) {
            runtime->callback_failed_ = true;
            return frames;
        }

        for (std::size_t index = 0; index < sample_count; ++index) {
            runtime->audio_samples_[old_size + index] =
                static_cast<float>(data[index]) / 32768.0F;
        }
        return frames;
    }

    static void RETRO_CALLCONV input_poll_callback() {}

    static std::int16_t RETRO_CALLCONV input_state_callback(
        const unsigned port,
        const unsigned device,
        const unsigned index,
        const unsigned id)
    {
        const auto* runtime = active();
        if (runtime == nullptr || device != RETRO_DEVICE_JOYPAD
            || index != 0U) {
            return 0;
        }

        // With the port model installed the machine reads the host's input
        // from the model directly, and this callback is the emulator's own
        // input path -- which is exactly what an ownership change has to make
        // dead rather than merely bypass. Answering nothing here is what turns
        // "both arms produce the same census" into evidence: a run that had
        // silently fallen back to the emulator's chain would be a run with
        // nothing pressed, and a route that presses buttons would not survive
        // it.
        if (runtime->native_pad_) {
            return 0;
        }

        std::uint16_t buttons = 0;
        if (port == 0U) {
            buttons = runtime->current_input_.player_one;
        } else if (port == 1U) {
            buttons = runtime->current_input_.player_two;
        } else {
            return 0;
        }

        Button button = Button::none;
        switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_A:
            button = Button::one;
            break;
        case RETRO_DEVICE_ID_JOYPAD_B:
            button = Button::two;
            break;
        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            button = Button::select;
            break;
        case RETRO_DEVICE_ID_JOYPAD_START:
            button = Button::run;
            break;
        case RETRO_DEVICE_ID_JOYPAD_UP:
            button = Button::up;
            break;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            button = Button::right;
            break;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            button = Button::down;
            break;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            button = Button::left;
            break;
        default:
            return 0;
        }

        return pressed(buttons, button) ? 1 : 0;
    }

    bool initialized_ = false;
    bool loaded_ = false;
    bool callback_failed_ = false;
    std::string rom_path_utf8_;
    std::string system_directory_utf8_;
    InputFrame current_input_;
    retro_system_av_info av_info_{};
    std::vector<std::uint32_t> framebuffer_pixels_;
    std::vector<float> audio_samples_;
    // The native sound device, and the C vtable the fork holds a pointer to.
    // Both outlive every frame the model is installed for.
    std::unique_ptr<audio::PcePsg> native_psg_;
    dc_beetle_psg_model native_psg_model_{};
    std::unique_ptr<audio::MusicVoiceOverlay> music_voice_overlay_;
    std::unique_ptr<video::PceVdc> native_vdc_;
    dc_beetle_vdc_model native_vdc_model_{};
    std::unique_ptr<video::PceVce> native_vce_;
    dc_beetle_vce_model native_vce_model_{};
    std::unique_ptr<cpu::HuC6280Irq> native_irq_;
    dc_beetle_irq_model native_irq_model_{};
    std::unique_ptr<cpu::HuC6280Scheduler> native_scheduler_;
    dc_beetle_scheduler_model native_scheduler_model_{};
    std::unique_ptr<cpu::HuC6280InterruptBoundary> native_boundary_;
    dc_beetle_boundary_model native_boundary_model_{};
    std::unique_ptr<cpu::HuC6280IdleSkip> native_idle_skip_;
    dc_beetle_idle_model native_idle_skip_model_{};
    std::unique_ptr<input::PcePad> native_pad_;
    dc_beetle_pad_model native_pad_model_{};
    // The controller the encoder clocks, whichever one that turns out to be.
    // Stateless, so it outlives every install and uninstall of either model.
    BeetleVdcDevice beetle_vdc_bridge_;
    // Counters are indexed directly by physical bank and logical PC. A bank's
    // 1 MiB page is allocated on first execution, so only the handful of banks
    // the game actually runs from is resident.
    struct InstructionCounter {
        std::uint64_t executions = 0;
        std::uint64_t master_cycles = 0;
        std::uint64_t interpreted_executions = 0;
        std::uint64_t interpreted_master_cycles = 0;
    };

    struct InstructionProfile {
        using BankCounters = std::array<InstructionCounter, 0x10000U>;

        std::array<std::unique_ptr<BankCounters>, 0x100U> banks{};
        // The previous instruction, whose cost is the span to the next entry.
        bool has_previous = false;
        std::uint8_t previous_bank = 0;
        std::uint16_t previous_pc = 0;
        std::uint64_t previous_absolute_master_cycle = 0;
        bool previous_was_native = false;

        void sample(
            const std::uint8_t bank,
            const std::uint16_t pc,
            const std::uint64_t absolute_master_cycle)
        {
            if (has_previous
                && absolute_master_cycle
                    >= previous_absolute_master_cycle) {
                auto& page = banks[previous_bank];
                if (!page) {
                    page = std::make_unique<BankCounters>();
                }
                auto& counter = (*page)[previous_pc];
                ++counter.executions;
                const auto cycles = absolute_master_cycle
                    - previous_absolute_master_cycle;
                counter.master_cycles += cycles;
                if (!previous_was_native) {
                    ++counter.interpreted_executions;
                    counter.interpreted_master_cycles += cycles;
                }
            }
            has_previous = true;
            previous_bank = bank;
            previous_pc = pc;
            previous_absolute_master_cycle = absolute_master_cycle;
            previous_was_native = false;
        }

        void mark_previous_native(
            const std::uint8_t bank,
            const std::uint16_t pc,
            const std::uint64_t absolute_master_cycle) noexcept
        {
            // The core enters the instruction hook before asking whether this
            // exact boundary has a replacement. A successful transaction owns
            // the interval which starts here; a declined transaction leaves it
            // interpreted. Do not let a delayed or mismatched callback relabel
            // a newer instruction.
            if (has_previous && previous_bank == bank && previous_pc == pc
                && previous_absolute_master_cycle == absolute_master_cycle) {
                previous_was_native = true;
            }
        }
    };

    [[nodiscard]] std::vector<InstructionProfileEntry>
    export_instruction_profile(const bool interpreted_only) const
    {
        std::vector<InstructionProfileEntry> entries;
        if (!instruction_profile_) {
            return entries;
        }

        for (std::size_t bank = 0U;
             bank < instruction_profile_->banks.size();
             ++bank) {
            const auto& counters = instruction_profile_->banks[bank];
            if (!counters) {
                continue;
            }
            for (std::size_t pc = 0U; pc < counters->size(); ++pc) {
                const auto& counter = (*counters)[pc];
                const auto executions = interpreted_only
                    ? counter.interpreted_executions
                    : counter.executions;
                if (executions == 0U) {
                    continue;
                }
                entries.push_back({
                    .instruction =
                        {
                            .physical_bank = static_cast<std::uint8_t>(bank),
                            .logical_pc = static_cast<std::uint16_t>(pc),
                        },
                    .executions = executions,
                    .master_cycles = interpreted_only
                        ? counter.interpreted_master_cycles
                        : counter.master_cycles,
                });
            }
        }

        std::ranges::sort(
            entries,
            [](const InstructionProfileEntry& left,
               const InstructionProfileEntry& right) {
                if (left.master_cycles != right.master_cycles) {
                    return left.master_cycles > right.master_cycles;
                }
                return std::pair{
                           left.instruction.physical_bank,
                           left.instruction.logical_pc}
                < std::pair{
                           right.instruction.physical_bank,
                           right.instruction.logical_pc};
            });
        return entries;
    }

    std::unique_ptr<InstructionProfile> instruction_profile_;
    std::vector<ExecutionHook> execution_hooks_;
    std::vector<RoutineCallRegistration> routine_call_observers_;
    std::vector<PendingAbsorbedReturn> pending_absorbed_returns_;
    // Names the transcripts this runtime hands the core, so that one it gives
    // up on can be named back. Monotonic and never reused, so a token that
    // arrives late cannot match a promise made since. The low twenty-four bits
    // of a token are the routine it was issued for; this is the serial above
    // them, and the shift is what keeps a routine's tokens distinct.
    std::uint64_t next_transcript_token_ = 0;
    std::map<AbandonmentKey, std::uint64_t> abandonment_census_;
    std::array<std::uint64_t, 0x100U> stack_page_write_cycle_{};
    WorkRamWriteCallback work_ram_write_observer_;
    BusWriteCallback bus_write_observer_;
    std::array<
        dc_beetle_replacement_write,
        DC_BEETLE_MAX_REPLACEMENT_WRITES>
        replacement_writes_{};
    std::array<
        dc_beetle_replacement_checkpoint,
        DC_BEETLE_MAX_REPLACEMENT_CHECKPOINTS>
        replacement_checkpoints_{};
    int framebuffer_width_ = 0;
    int framebuffer_height_ = 0;
    std::uint64_t frame_number_ = 0;
    const std::thread::id owner_thread_ = std::this_thread::get_id();
};

} // namespace

std::unique_ptr<PceRuntime> create_pce_runtime()
{
    return std::make_unique<BeetlePceRuntime>();
}

} // namespace devils_crush::emulation
