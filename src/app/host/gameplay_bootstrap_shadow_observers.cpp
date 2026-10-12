// SPDX-License-Identifier: GPL-3.0-or-later
#include "gameplay_bootstrap_shadow.hpp"

namespace devils_crush::app::host {

// Puts the shadow's observers on the live runtime. A deployed registry
// already shows it the routines it admits, through its entry observer, so
// those are only hooked here when there is none. False, having said why,
// when an observer cannot be installed.
[[nodiscard]] bool install_gameplay_bootstrap_shadow_observers(
    PceRuntime& runtime,
    NativeGameplayBootstrapShadow& shadow,
    const std::vector<std::byte>& rom,
    const bool registry_deployed)
{
    // C510 itself does not write either scheduler gate byte before its C52C
    // BSR.  When their two entry snapshots disagree, retain only the writer
    // of the current slot's `$3000`/`$3080` bytes; this isolates the boundary
    // that changes eligibility without taking ownership of the pass.
    if (!runtime.set_work_ram_write_observer(
            [&shadow](const ObservedMemoryWrite& write) {
                shadow.observe_main_actor_pass_gate_write(write);
            })) {
        std::cerr << "Could not observe C510 scheduler-gate writes for native "
                     "gameplay shadow.\n";
        return false;
    }
    // C539 is a direct composite call rather than an admitted registry entry,
    // so the registry's entry observer cannot see it. Separate observe-only
    // hooks at its two JSR sites establish whether the short pass was selected
    // at all; the C539 hook then supplies its preamble snapshot. None claims
    // dynamic handlers, the loop, or C606 filing.
    if (!runtime.install_execution_hook({
            .routine = {
                .physical_bank = 0x01U,
                .logical_pc = 0xC539U,
            },
            .mode = HookMode::observe,
            .callback = [&runtime, &shadow](const RoutineEntry& entry) {
                shadow.observe_short_actor_pass_entry(
                    entry, runtime.export_work_ram());
            },
        })) {
        std::cerr << "Could not observe C539 actor-pass entry for native "
                     "gameplay shadow.\n";
        return false;
    }
    constexpr std::array<RoutineKey, 2U> kShortActorPassCallSites{{
        {.physical_bank = 0x01U, .logical_pc = 0xC1D3U},
        {.physical_bank = 0x00U, .logical_pc = 0xEEC1U},
    }};
    for (const auto routine : kShortActorPassCallSites) {
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::observe,
                .callback = [&shadow](const RoutineEntry& entry) {
                    shadow.observe_short_actor_pass_call_site(entry);
                },
            })) {
            std::cerr << "Could not observe a C539 actor-pass call site for "
                         "native gameplay shadow.\n";
            return false;
        }
    }
    if (!registry_deployed
        && !runtime.install_execution_hook({
            .routine = {
                .physical_bank = 0x01U,
                .logical_pc = 0xCD74U,
            },
            .mode = HookMode::observe,
            .callback = [&runtime,
                         &shadow,
                         &rom](const RoutineEntry& entry) {
                shadow.observe_playfield_collision_source(
                    entry,
                    runtime.export_work_ram(),
                    rom);
            },
        })) {
        std::cerr << "Could not observe the playfield collision probe for "
                     "native gameplay shadow.\n";
        return false;
    }
    if (!registry_deployed) {
        constexpr std::array<RoutineKey, 14> kGameplayShadowEntries{{
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCB2AU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xDC95U,
            },
            {
                .physical_bank = 0x00U,
                .logical_pc = 0xF1B4U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xDD09U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCF33U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCDF8U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC52CU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC606U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xC510U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCBABU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCC1AU,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCC98U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCBC2U,
            },
            {
                .physical_bank = 0x01U,
                .logical_pc = 0xCBCFU,
            },
        }};
        for (const auto routine : kGameplayShadowEntries) {
            if (runtime.install_execution_hook({
                    .routine = routine,
                    .mode = HookMode::observe,
                    .callback = [&runtime,
                                 &shadow,
                                 &rom](
                                    const RoutineEntry& entry) {
                        shadow.observe_motion_entry(
                            entry,
                            runtime.export_work_ram(),
                            rom);
                    },
                })) {
                continue;
            }
            std::cerr << "Could not observe " << format_routine_key(routine)
                      << " for native gameplay shadow.\n";
            return false;
        }
    }
    constexpr std::array<RoutineKey, 7> kGameplayRoutineCompletions{{
        {
            .physical_bank = 0x00U,
            .logical_pc = 0xF241U,
        },
        {
            .physical_bank = 0x00U,
            .logical_pc = 0xF1B4U,
        },
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xDD09U,
        },
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xCD74U,
        },
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xC52CU,
        },
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xC606U,
        },
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xC510U,
        },
    }};
    for (const auto routine : kGameplayRoutineCompletions) {
        if (runtime.install_routine_call_observer({
                .routine = routine,
                .callback = [&runtime, &shadow](const RoutineCall& call) {
                    shadow.observe_gameplay_completion(
                        call, runtime.export_work_ram());
                },
            })) {
            continue;
        }
        std::cerr << "Could not observe completion of "
                  << format_routine_key(routine)
                  << " for native gameplay shadow.\n";
        return false;
    }
    // E9B6 returns through the core-owned JSR at E976.  Its routine-call
    // observer gives the shadow the precise pre-RTS register state; E994 is
    // the only caller-tail boundary not already owned by the registry.
    if (!runtime.install_routine_call_observer({
            .routine = {
                .physical_bank = 0x00U,
                .logical_pc = 0xE9B6U,
            },
            .callback = [&runtime, &shadow](const RoutineCall& call) {
                shadow.observe_palette_fade_block_completion(
                    call, runtime.export_work_ram());
            },
        })) {
        std::cerr << "Could not observe E9B6 palette-fade completion for "
                     "native gameplay shadow.\n";
        return false;
    }
    if (!runtime.install_execution_hook({
            .routine = {
                .physical_bank = 0x00U,
                .logical_pc = 0xE994U,
            },
            .mode = HookMode::observe,
            .callback = [&runtime, &shadow](const RoutineEntry& entry) {
                shadow.observe_palette_fade_terminal_entry(
                    entry, runtime.export_work_ram());
            },
        })) {
        std::cerr << "Could not observe E994 palette-fade tail for native "
                     "gameplay shadow.\n";
        return false;
    }
    return true;
}

} // namespace devils_crush::app::host
