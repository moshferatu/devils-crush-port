// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/emulation/pce_runtime.hpp"

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
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
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

    [[nodiscard]] std::vector<InstructionProfileEntry>
    export_instruction_profile() const override
    {
        assert_owner_thread();
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
                if (counter.executions == 0U) {
                    continue;
                }
                entries.push_back({
                    .instruction =
                        {
                            .physical_bank = static_cast<std::uint8_t>(bank),
                            .logical_pc = static_cast<std::uint16_t>(pc),
                        },
                    .executions = counter.executions,
                    .master_cycles = counter.master_cycles,
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

    void install_callbacks()
    {
        retro_set_environment(&environment_callback);
        retro_set_video_refresh(&video_callback);
        retro_set_audio_sample(&audio_sample_callback);
        retro_set_audio_sample_batch(&audio_batch_callback);
        retro_set_input_poll(&input_poll_callback);
        retro_set_input_state(&input_state_callback);
    }

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
            dc_beetle_set_replacement_hook(
                &replacement_hook_callback,
                this);
        } else {
            dc_beetle_clear_replacement_hook();
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

    [[nodiscard]] static CpuRegisters cpu_registers(
        const dc_beetle_cpu_state& state) noexcept
    {
        return {
            .program_counter = state.logical_pc,
            .accumulator = state.accumulator,
            .x = state.x,
            .y = state.y,
            .stack_pointer = state.stack_pointer,
            .status = state.status,
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
            const auto resume_segment =
                result.return_instruction.logical_pc >> 13U;
            if (write_count > DC_BEETLE_MAX_REPLACEMENT_WRITES
                || checkpoint_count
                    > DC_BEETLE_MAX_REPLACEMENT_CHECKPOINTS
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

            core_result->resume_logical_pc =
                result.return_instruction.logical_pc;
            core_result->accumulator =
                result.output_registers.accumulator;
            core_result->x = result.output_registers.x;
            core_result->y = result.output_registers.y;
            core_result->stack_pointer =
                result.output_registers.stack_pointer;
            core_result->status = result.output_registers.status;
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
                    checkpoint.registers.status;
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
        }
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
    // Counters are indexed directly by physical bank and logical PC. A bank's
    // 1 MiB page is allocated on first execution, so only the handful of banks
    // the game actually runs from is resident.
    struct InstructionCounter {
        std::uint64_t executions = 0;
        std::uint64_t master_cycles = 0;
    };

    struct InstructionProfile {
        using BankCounters = std::array<InstructionCounter, 0x10000U>;

        std::array<std::unique_ptr<BankCounters>, 0x100U> banks{};
        // The previous instruction, whose cost is the span to the next entry.
        bool has_previous = false;
        std::uint8_t previous_bank = 0;
        std::uint16_t previous_pc = 0;
        std::uint64_t previous_absolute_master_cycle = 0;

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
                counter.master_cycles += absolute_master_cycle
                    - previous_absolute_master_cycle;
            }
            has_previous = true;
            previous_bank = bank;
            previous_pc = pc;
            previous_absolute_master_cycle = absolute_master_cycle;
        }
    };

    std::unique_ptr<InstructionProfile> instruction_profile_;
    std::vector<ExecutionHook> execution_hooks_;
    std::vector<RoutineCallRegistration> routine_call_observers_;
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
