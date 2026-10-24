// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/cabinet/bonus_ball_color.hpp"

#include <array>
#include <stdexcept>

namespace devils_crush::cabinet {
namespace {
constexpr std::size_t kSessionFlags = 0x53U;   // $53
constexpr std::size_t kBonusLevel = 0xCDU;     // $CD
constexpr std::size_t kMultiplier = 0xD38U;    // $2D38
constexpr std::size_t kBallAttribute = 0x1D00U; // $3D00, ball slot zero
constexpr unsigned kLostOrServing = 0x0AU;     // $53.1 | $53.3
// The palette table both copies read (`03:7D4A`, `05:6A0D`).
constexpr std::array<unsigned, 3> kBonusPalettes{0x0AU, 0x02U, 0x0EU};

// After `03:7CF0`/`03:7D2A` (bank 3) or `05:69B3`/`05:69ED` (bank 5), where
// both the bonus and silver paths have finished writing `$3D00`.
constexpr std::array<emulation::RoutineKey, 4> kPaletteChosen{{
    {3U, 0x7CF3U}, {3U, 0x7D2DU}, {5U, 0x69B6U}, {5U, 0x69F0U},
}};
}

bool keep_bonus_ball_color(std::span<std::byte> ram) noexcept
{
    if (ram.size() != 8192U) return false;
    const auto flags = std::to_integer<unsigned>(ram[kSessionFlags]);
    const auto level = std::to_integer<unsigned>(ram[kBonusLevel]);
    if ((flags & kLostOrServing) == 0U || level >= kBonusPalettes.size()
        || ram[kMultiplier] == std::byte{0}) return false;
    const auto attribute = std::to_integer<unsigned>(ram[kBallAttribute]);
    const auto colored = (attribute & 0xF0U) | kBonusPalettes[level];
    if (colored == attribute) return false;
    ram[kBallAttribute] = static_cast<std::byte>(colored);
    return true;
}

bool install_bonus_ball_color(emulation::PceRuntime& runtime)
{
    for (const auto key : kPaletteChosen) {
        if (!runtime.install_execution_hook({
                .routine = key, .mode = emulation::HookMode::observe,
                .callback = [&runtime](const emulation::RoutineEntry&) {
                    auto ram = runtime.export_work_ram();
                    if (keep_bonus_ball_color(ram) && !runtime.import_work_ram(ram)) {
                        throw std::runtime_error("Could not keep the bonus ball color.");
                    }
                },
            })) return false;
    }
    return true;
}
}
