// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once


#include "devils_crush/core/backup_ram.hpp"
#include "devils_crush/app/native_registry.hpp"
#include "devils_crush/app/native_replacements.hpp"
#include "devils_crush/cabinet/cabinet_state.hpp"
#include "devils_crush/cabinet/display_config.hpp"
#include "devils_crush/cabinet/dmd_layout.hpp"
#include "devils_crush/cabinet/dmd_panel.hpp"
#include "devils_crush/cabinet/dragon_heads.hpp"
#include "devils_crush/cabinet/music_transition.hpp"
#include "devils_crush/core/atomic_file.hpp"
#include "devils_crush/core/data_files.hpp"
#include "devils_crush/core/high_score_table.hpp"
#include "devils_crush/core/rom_image.hpp"
#include "devils_crush/core/save_state.hpp"
#include "devils_crush/emulation/pce_runtime.hpp"
#include "devils_crush/emulation/playfield_collision_adapter.hpp"
#include "devils_crush/emulation/routine_replay_corpus.hpp"
#include "devils_crush/input/ball_drag.hpp"
#include "devils_crush/input/plunger.hpp"
#include "devils_crush/native_runtime/actor_collision_scan.hpp"
#include "devils_crush/native_runtime/actor_scheduler.hpp"
#include "devils_crush/native_runtime/collision_map_asset.hpp"
#include "devils_crush/native_runtime/ball_frame_schedule.hpp"
#include "devils_crush/native_runtime/bonus_stage_launch.hpp"
#include "devils_crush/native_runtime/gameplay_bootstrap.hpp"
#include "devils_crush/native_runtime/mapped_rom.hpp"
#include "devils_crush/native_runtime/native_gameplay_runtime.hpp"
#include "devils_crush/render/pce_background.hpp"
#include "devils_crush/render/pce_sprite_overlay.hpp"
#include "devils_crush/render/table_palette_cache.hpp"
#include "devils_crush/render/sharp_pixel_scaler.hpp"
#include "devils_crush/render/ending_head.hpp"
#include "devils_crush/render/title_logo.hpp"
#include "devils_crush/render/viewport_motion.hpp"

#include <SDL3/SDL.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace devils_crush::app::host {

constexpr int kSmokeTestFrames = 10;

inline const std::filesystem::path kBackupRamPath =
    std::filesystem::path(devils_crush::core::kBackupRamFileName);
inline const std::filesystem::path kQuickStatePath =
    std::filesystem::path(devils_crush::core::kQuickStateFileName);

inline const std::filesystem::path kCabinetConfigPath =
    std::filesystem::path(devils_crush::core::kConfigFileName);

inline const std::filesystem::path kHighScorePath =
    std::filesystem::path(devils_crush::core::kHighScoreFileName);

using devils_crush::emulation::AudioView;
using devils_crush::emulation::Button;
using devils_crush::emulation::EmulatedFrame;
using devils_crush::emulation::FramebufferView;
using devils_crush::emulation::HookMode;
using devils_crush::emulation::InputFrame;
using devils_crush::emulation::ObservedBusWrite;
using devils_crush::emulation::ObservedMemoryWrite;
using devils_crush::emulation::PceRuntime;
using devils_crush::emulation::ReplacementNesting;
using devils_crush::emulation::RoutineCall;
using devils_crush::emulation::RoutineEntry;
using devils_crush::emulation::RoutineKey;
using devils_crush::emulation::RoutineReplacement;
using devils_crush::app::replace_native_gameplay_routine;

} // namespace devils_crush::app::host
