// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/emulation/pce_runtime.hpp"

#include <dc_beetle_execution_hook.h>
#include <libretro.h>

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

class BeetlePceRuntime final : public PceRuntime {
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

    [[nodiscard]] bool install_execution_hook(
        ExecutionHook hook) override
    {
        assert_owner_thread();
        if (!loaded_ || hook.mode == HookMode::replace
            || (hook.mode == HookMode::observe && !hook.callback)) {
            return false;
        }

        execution_hook_ = std::move(hook);
        execution_hook_installed_ = true;
        dc_beetle_set_instruction_hook(
            &instruction_hook_callback,
            this);
        return true;
    }

    void clear_execution_hook() noexcept override
    {
        assert_owner_thread();
        dc_beetle_clear_instruction_hook();
        execution_hook_ = {};
        execution_hook_installed_ = false;
    }

    [[nodiscard]] bool has_execution_hook() const noexcept override
    {
        assert_owner_thread();
        return execution_hook_installed_;
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
        clear_execution_hook();
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

    static void instruction_hook_callback(
        const dc_beetle_cpu_state* const state,
        void* const user_data) noexcept
    {
        auto* const runtime =
            static_cast<BeetlePceRuntime*>(user_data);
        if (runtime == nullptr || state == nullptr
            || !runtime->execution_hook_installed_) {
            return;
        }

        const RoutineKey routine{
            .physical_bank = state->physical_bank,
            .logical_pc = state->logical_pc,
        };
        if (!(routine == runtime->execution_hook_.routine)) {
            return;
        }

        const auto& callback = runtime->execution_hook_.callback;
        if (!callback) {
            return;
        }

        try {
            callback({
                .routine = routine,
                .registers = {
                    .program_counter = state->logical_pc,
                    .accumulator = state->accumulator,
                    .x = state->x,
                    .y = state->y,
                    .stack_pointer = state->stack_pointer,
                    .status = state->status,
                    .mpr = {
                        state->mpr[0],
                        state->mpr[1],
                        state->mpr[2],
                        state->mpr[3],
                        state->mpr[4],
                        state->mpr[5],
                        state->mpr[6],
                        state->mpr[7],
                    },
                },
                .frame_number = runtime->frame_number_ + 1U,
                .master_cycle = state->master_cycle,
            });
        } catch (...) {
            runtime->callback_failed_ = true;
            dc_beetle_clear_instruction_hook();
            runtime->execution_hook_installed_ = false;
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

    [[nodiscard]] const std::byte* work_ram_data() const noexcept
    {
        if (!loaded_
            || retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) == nullptr
            || retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)
                < kPceWorkRamSize) {
            return nullptr;
        }
        return static_cast<const std::byte*>(
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
    ExecutionHook execution_hook_;
    bool execution_hook_installed_ = false;
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
