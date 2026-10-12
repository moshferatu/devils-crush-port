// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_registry_install.hpp"

#include "command_line.hpp"

namespace devils_crush::app::host {

[[nodiscard]] bool install_native_registry(
    PceRuntime& runtime,
    const CommandLine& options,
    const std::vector<std::byte>& replacement_rom,
    const std::span<const RoutineKey> claimed_routines,
    NativeRegistryDeployment& deployment,
    const NativeRegistryEntryObserver& entry_observer)
{
    const auto admitted = devils_crush::app::admitted_native_routines();
    deployment.rows.clear();
    deployment.rows.reserve(admitted.size());
    for (const auto& routine : admitted) {
        deployment.rows.push_back({
            .routine = routine.routine,
            .evidence = routine.evidence,
        });
    }

    for (std::size_t index = 0U; index < deployment.rows.size(); ++index) {
        if (std::find(
                kWithheldRoutines.begin(),
                kWithheldRoutines.end(),
                deployment.rows[index].routine)
            != kWithheldRoutines.end()) {
            deployment.rows[index].skipped = true;
            continue;
        }
        if (options.registry_slice_set
            && (index < options.registry_slice_first
                || index >= options.registry_slice_first
                    + options.registry_slice_count)) {
            deployment.rows[index].skipped = true;
            continue;
        }
        if (std::find(
                options.registry_excludes.begin(),
                options.registry_excludes.end(),
                deployment.rows[index].routine)
            != options.registry_excludes.end()) {
            deployment.rows[index].skipped = true;
            continue;
        }
        // An explicit --replace-hook or --observe-hook owns its key; each key
        // may be registered only once.
        if (std::find(
                claimed_routines.begin(),
                claimed_routines.end(),
                deployment.rows[index].routine)
            != claimed_routines.end()) {
            deployment.rows[index].skipped = true;
            continue;
        }
        if (runtime.install_execution_hook({
                .routine = deployment.rows[index].routine,
                .mode = HookMode::replace,
                .replacement_callback =
                    [&deployment, &replacement_rom, entry_observer, index,
                     nudge_multiplier = options.nudge_multiplier](
                        const RoutineEntry& entry,
                        const std::span<const std::byte> work_ram) {
                        auto& row = deployment.rows[index];
                        const bool three_blobs_timer_decrement =
                            entry.routine
                            == RoutineKey{
                                .physical_bank = 0x03U,
                                .logical_pc = 0x66A3U,
                            };
                        ++row.entries;
                        if (entry.routine == RoutineKey{0x03U, 0x68BFU}) {
                            ++deployment.core_gray_blob_live_wait_entries;
                        }
                        deployment.span.observe(entry.absolute_master_cycle);
                        if (entry_observer) {
                            entry_observer(entry, work_ram);
                        }
                        auto result = replace_native_gameplay_routine(
                            entry,
                            work_ram,
                            replacement_rom);
                        // An active C3CB body absorbs the C484 boundary. Let
                        // instruction-level execution reach the force hook
                        // when customized; idle nudge bodies remain batched.
                        if (nudge_multiplier != 1.0 && result
                            && entry.routine == RoutineKey{0x01U, 0xC3CBU}
                            && std::ranges::any_of(result->work_ram_writes,
                                [](const auto& write) {
                                    return write.logical_address == 0x3500U;
                                })) {
                            result.reset();
                        }
                        // The same interrupt-horizon valve the proven @all
                        // routes use: an atomic body may not span a user or
                        // video event or a serviceable timer interrupt.
                        if (result && result->checkpoints.empty()
                            && result->body_master_cycles
                                >= std::min(
                                    entry.master_cycles_until_user_event,
                                    entry
                                        .master_cycles_until_timer_interrupt)) {
                            result.reset();
                            ++row.declined_at_horizon;
                        }
                        if (result) {
                            ++row.accepted;
                            row.native_master_cycles +=
                                result->body_master_cycles;
                            if (three_blobs_timer_decrement) {
                                ++deployment.core_three_blobs_timer_decrement_instructions;
                            }
                        }
                        return result;
                    },
            })) {
            continue;
        }
        std::cerr << "Could not install registry hook "
                  << format_routine_key(deployment.rows[index].routine)
                  << ".\n";
        return false;
    }
    // The full VCE palette upload is a single TIA opcode at 00:EB46, reached
    // from the admitted 00:EB30 transaction. It is not a separately admitted
    // game routine, but it still needs a hook at the instruction boundary so
    // the compact core block-transfer path can take it over. Keep it outside
    // the census rows: the existing EB30 owner remains the reported routine.
    constexpr RoutineKey kPaletteTiaInstruction{
        .physical_bank = 0x00U,
        .logical_pc = 0xEB46U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kPaletteTiaInstruction)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kPaletteTiaInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_tia_transfers;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native palette TIA hook.\n";
        return false;
    }
    // Like TIA, BBR0 is an indivisible core opcode rather than a game-routine
    // transcript: its zero-page read and branch each sample the event horizon.
    // Its two tilemap arms are already admitted at F426/F42A.
    constexpr RoutineKey kTilemapBbr0Instruction{
        .physical_bank = 0x00U,
        .logical_pc = 0xF423U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kTilemapBbr0Instruction)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kTilemapBbr0Instruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_bbr0_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native tilemap BBR0 hook.\n";
        return false;
    }
    // The substep loops contain two BBR3 <$77 instructions.  Their native
    // continuations already begin at each possible arm, while the core keeps
    // the original read/branch event samples.
    constexpr std::array<RoutineKey, 2U> kSubstepBbr3Instructions{{
        {.physical_bank = 0x01U, .logical_pc = 0xC6E3U},
        {.physical_bank = 0x01U, .logical_pc = 0xC756U},
    }};
    for (const auto routine : kSubstepBbr3Instructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
            .routine = routine,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_bbr3_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
            std::cerr << "Could not install native substep BBR3 hook.\n";
            return false;
        }
    }
    // The collision sweep uses the same BBR3 opcode and has native arms at
    // CB81 and CB8A. It stays in the core for the original event samples.
    constexpr RoutineKey kCollisionTailBbr3Instruction{
        .physical_bank = 0x01U,
        .logical_pc = 0xCB7EU,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kCollisionTailBbr3Instruction)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kCollisionTailBbr3Instruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_bbr3_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native collision BBR3 hook.\n";
        return false;
    }
    // The preceding collision-sweep BBS2 has native arms at CB74 and CB76;
    // its core path retains the zero-page and branch event samples.
    constexpr RoutineKey kCollisionTailBbs2Instruction{
        .physical_bank = 0x01U,
        .logical_pc = 0xCB71U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kCollisionTailBbs2Instruction)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kCollisionTailBbs2Instruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_bbs2_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native collision BBS2 hook.\n";
        return false;
    }
    // D4B2 is an RTS embedded in the active-flipper owner. Its target comes
    // from a live stack frame, so the core alone executes the return.
    constexpr RoutineKey kFlipperOwnerRtsInstruction{
        .physical_bank = 0x01U,
        .logical_pc = 0xD4B2U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kFlipperOwnerRtsInstruction)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kFlipperOwnerRtsInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native flipper-owner RTS hook.\n";
        return false;
    }
    // 02:5933 is the upper-table effect's shared live-stack return.  As with
    // D4B2, retain the core's exact return sequence instead of inventing a
    // transcript target from the current stack page.
    constexpr RoutineKey kUpperTableReturnInstruction{
        .physical_bank = 0x02U,
        .logical_pc = 0x5933U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kUpperTableReturnInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kUpperTableReturnInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native upper-table RTS hook.\n";
        return false;
    }
    // These helper returns are reached from several live callers.  Keep the
    // HuC6280's stack-pop sequence exact while moving their instruction
    // boundary out of the interpreter. 44A3 is an upper-table helper return,
    // 5CD5 is the Side-Worm selector return, while C9DE is the active
    // range-helper's caller-owned return; each deliberately remains a core
    // RTS instruction rather than a synthetic continuation of its caller.
    constexpr std::array<RoutineKey, 13U> kGameplayHelperRtsInstructions{{
        // Synthetic vector dispatch: execute the live RTS, never absorb it
        // as the completion of the enclosing collision routine.
        {.physical_bank = 0x01U, .logical_pc = 0xCBF9U},
        {.physical_bank = 0x01U, .logical_pc = 0xC9DEU},
        {.physical_bank = 0x01U, .logical_pc = 0xC70AU},
        {.physical_bank = 0x01U, .logical_pc = 0xC3DCU},
        {.physical_bank = 0x02U, .logical_pc = 0x44A3U},
        {.physical_bank = 0x02U, .logical_pc = 0x453BU},
        {.physical_bank = 0x02U, .logical_pc = 0x5CD5U},
        {.physical_bank = 0x03U, .logical_pc = 0x6314U},
        {.physical_bank = 0x03U, .logical_pc = 0x6905U},
        {.physical_bank = 0x03U, .logical_pc = 0x73CCU},
        {.physical_bank = 0x03U, .logical_pc = 0x7423U},
        {.physical_bank = 0x03U, .logical_pc = 0x73FDU},
        {.physical_bank = 0x03U, .logical_pc = 0x76A8U},
    }};
    for (const auto routine : kGameplayHelperRtsInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_rts_instructions;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native gameplay-helper RTS hook.\\n";
            return false;
        }
    }
    // The VDC status read is the interrupt acknowledge itself. It cannot be
    // transcribed, but the core can retain the original device read exactly.
    constexpr RoutineKey kVdcStatusReadInstruction{
        .physical_bank = 0x00U,
        .logical_pc = 0xE05CU,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kVdcStatusReadInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kVdcStatusReadInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_vdc_status_reads;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native VDC-status read hook.\n";
        return false;
    }
    // The sprite attribute updater reads back VDC data at these two adjacent
    // sites.  The selected register and read buffer are live device state, so
    // both LDA instructions remain exact core boundaries until the enclosing
    // transaction can be transcribed around them.
    constexpr std::array<RoutineKey, 2U> kVdcDataReadInstructions{{
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xDA06U,
        },
        {
            .physical_bank = 0x01U,
            .logical_pc = 0xDA0BU,
        },
    }};
    for (const auto routine : kVdcDataReadInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
                != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_vdc_data_reads;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native VDC-data read hook.\n";
            return false;
        }
    }
    // D950 is the deterministic address/setup half of the sprite-attribute
    // transaction. It stops at DA06 so the selected-register VDC read remains
    // the exact core instruction installed above.
    constexpr RoutineKey kVdcSpriteAttributePrefix{
        .physical_bank = 0x01U,
        .logical_pc = 0xD950U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(),
            kVdcSpriteAttributePrefix)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kVdcSpriteAttributePrefix,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_vdc_sprite_attribute_prefix_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native VDC sprite-attribute prefix hook.\n";
        return false;
    }
    // F605 prepares one actor's MPR-backed graphics record. It makes the
    // original resolver and word-stream-decoder calls, so its two return
    // continuations are separately installed beside the routine head.
    constexpr std::array<RoutineKey, 3U> kActorScriptRecordLoaderEntries{{
        {.physical_bank = 0x00U, .logical_pc = 0xF605U},
        {.physical_bank = 0x00U, .logical_pc = 0xF660U},
        {.physical_bank = 0x00U, .logical_pc = 0xF664U},
    }};
    for (const auto routine : kActorScriptRecordLoaderEntries) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.auxiliary_transcript_entries;
                    ++deployment.auxiliary_actor_script_record_loader_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.auxiliary_transcript_accepted;
                        deployment.auxiliary_transcript_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native actor-script loader hook.\n";
            return false;
        }
    }
    // 02:51AF is now registry-admitted, including expiry. Do not install a
    // second auxiliary hook for the same address.
    // E34C, including its closing BNE, has graduated from an auxiliary
    // VDC-address capability to a fully admitted routine above. Do not install
    // a second hook here; its cycles and calls now belong to the registry row.
    // PLP samples the interrupt mask before it restores P from the stack.
    // Keep both VBlank epilogues in the core so that ordering remains exact.
    constexpr std::array<std::uint16_t, 2U> kVblankPlpInstructions{
        0xE134U,
        0xE193U,
    };
    for (const auto plp_pc : kVblankPlpInstructions) {
        const RoutineKey plp_instruction{
            .physical_bank = 0x00U,
            .logical_pc = plp_pc,
        };
        if (std::find(
                claimed_routines.begin(), claimed_routines.end(), plp_instruction)
                == claimed_routines.end()
            && !runtime.install_execution_hook({
                .routine = plp_instruction,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_plp_instructions;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native VBlank PLP hook.\n";
            return false;
        }
    }
    // The music driver reaches this SEI with interrupts enabled. The original
    // opcode samples its pending IRQ before it raises I, so retain it in core.
    constexpr std::array<RoutineKey, 2U> kMusicSeiInstructions{{
        {.physical_bank = 0x45U, .logical_pc = 0x4168U},
        {.physical_bank = 0x45U, .logical_pc = 0x4B5DU},
    }};
    for (const auto routine : kMusicSeiInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
            .routine = routine,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_sei_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
            })) {
            std::cerr << "Could not install native music SEI hook.\n";
            return false;
        }
    }
    constexpr std::array<RoutineKey, 11U> kExactJsrInstructions{{
        {.physical_bank = 0x45U, .logical_pc = 0x408FU},
        {.physical_bank = 0x45U, .logical_pc = 0x4099U},
        {.physical_bank = 0x45U, .logical_pc = 0x4138U},
        {.physical_bank = 0x03U, .logical_pc = 0x7696U},
        {.physical_bank = 0x01U, .logical_pc = 0xCB5EU},
        {.physical_bank = 0x00U, .logical_pc = 0xE976U},
        {.physical_bank = 0x00U, .logical_pc = 0xED4DU},
        {.physical_bank = 0x01U, .logical_pc = 0xD4A4U},
        {.physical_bank = 0x01U, .logical_pc = 0xC603U},
        {.physical_bank = 0x01U, .logical_pc = 0xC7CFU},
        // The collision scanner's $CC04 call opens the bounded $CCD5
        // comparison walk.  Its callee remains independently scheduled;
        // retain only the exact stack-producing JSR here.
        {.physical_bank = 0x01U, .logical_pc = 0xCC04U},
    }};
    for (const auto routine : kExactJsrInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_jsr_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
            })) {
            std::cerr << "Could not install native exact JSR hook.\n";
            return false;
        }
    }
    constexpr RoutineKey kMusicPsgReturnInstruction{
        .physical_bank = 0x45U,
        .logical_pc = 0x4210U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kMusicPsgReturnInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kMusicPsgReturnInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native music RTS hook.\n";
        return false;
    }
    constexpr RoutineKey kSharedActorReturnInstruction{
        .physical_bank = 0x03U,
        .logical_pc = 0x7525U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kSharedActorReturnInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kSharedActorReturnInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native shared actor RTS hook.\n";
        return false;
    }
    constexpr RoutineKey kCollisionScannerReturnInstruction{
        .physical_bank = 0x01U,
        .logical_pc = 0xCCD4U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kCollisionScannerReturnInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kCollisionScannerReturnInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native collision-scanner RTS hook.\n";
        return false;
    }
    // The sibling hit return is reached by the collision-mask lookup with
    // carry already set.  Like CCD4, only the core can consume its caller's
    // live stack frame without inventing a continuation.
    constexpr RoutineKey kCollisionScannerHitReturnInstruction{
        .physical_bank = 0x01U,
        .logical_pc = 0xCCD2U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kCollisionScannerHitReturnInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kCollisionScannerHitReturnInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native collision hit-return RTS hook.\n";
        return false;
    }
    constexpr RoutineKey kKnightNoHitReturnInstruction{
        .physical_bank = 0x03U,
        .logical_pc = 0x73D7U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kKnightNoHitReturnInstruction)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kKnightNoHitReturnInstruction,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native Knight no-hit RTS hook.\n";
        return false;
    }
    constexpr std::array<RoutineKey, 4U> kFlipperOwnerInstructions{{
        {.physical_bank = 0x01U, .logical_pc = 0xD4A7U},
        {.physical_bank = 0x01U, .logical_pc = 0xD4AAU},
        {.physical_bank = 0x01U, .logical_pc = 0xD4ADU},
        {.physical_bank = 0x01U, .logical_pc = 0xD4B0U},
    }};
    for (const auto routine : kFlipperOwnerInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_flipper_owner_instructions;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native flipper-owner hook.\n";
            return false;
        }
    }
    constexpr std::array<RoutineKey, 3U> kActorSchedulerInstructions{{
        {.physical_bank = 0x03U, .logical_pc = 0x751EU},
        {.physical_bank = 0x03U, .logical_pc = 0x7521U},
        {.physical_bank = 0x03U, .logical_pc = 0x7523U},
    }};
    for (const auto routine : kActorSchedulerInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_actor_scheduler_instructions;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native actor-scheduler hook.\n";
            return false;
        }
    }
    constexpr std::array<RoutineKey, 16U> kMusicJumpInstructions{{
        {.physical_bank = 0x45U, .logical_pc = 0x4000U},
        {.physical_bank = 0x45U, .logical_pc = 0x4003U},
        {.physical_bank = 0x45U, .logical_pc = 0x4006U},
        {.physical_bank = 0x45U, .logical_pc = 0x4009U},
        {.physical_bank = 0x45U, .logical_pc = 0x40FFU},
        {.physical_bank = 0x08U, .logical_pc = 0x4000U},
        {.physical_bank = 0x08U, .logical_pc = 0x4003U},
        {.physical_bank = 0x08U, .logical_pc = 0x4006U},
        {.physical_bank = 0x08U, .logical_pc = 0x4009U},
        {.physical_bank = 0x08U, .logical_pc = 0x400CU},
        {.physical_bank = 0x08U, .logical_pc = 0x400FU},
        {.physical_bank = 0x08U, .logical_pc = 0x4012U},
        {.physical_bank = 0x08U, .logical_pc = 0x4015U},
        {.physical_bank = 0x08U, .logical_pc = 0x4018U},
        {.physical_bank = 0x08U, .logical_pc = 0x401BU},
        {.physical_bank = 0x08U, .logical_pc = 0x4030U},
    }};
    for (const auto routine : kMusicJumpInstructions) {
        // An admitted export is already installed above as its own registry
        // row.  Do not claim it again as a generic core-JMP capability: the
        // runtime permits one hook per address, and a registry slice or
        // explicit exclusion must still leave the core capability available.
        const bool installed_by_registry = std::ranges::any_of(
            deployment.rows,
            [routine](const RegistryCensusRow& row) {
                return row.routine == routine && !row.skipped;
            });
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
                != claimed_routines.end()
            || installed_by_registry) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_jmp_instructions;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native music JMP hook.\n";
            return false;
        }
    }
    // Stable sequencer continuations reached after an inner native call or a
    // short core interval. Register them directly so the existing bank-45
    // model resumes the voice walk instead of falling back to interpretation.
    constexpr std::array<RoutineKey, 6U> kMusicSequencerContinuations{{
        {.physical_bank = 0x45U, .logical_pc = 0x40ADU},
        {.physical_bank = 0x45U, .logical_pc = 0x40EEU},
        {.physical_bank = 0x45U, .logical_pc = 0x40F6U},
        {.physical_bank = 0x45U, .logical_pc = 0x402CU},
        {.physical_bank = 0x45U, .logical_pc = 0x4B5EU},
    }};
    for (const auto routine : kMusicSequencerContinuations) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native music continuation hook.\n";
            return false;
        }
    }
    // The active Giant-Face animator returns through 76CA and immediately
    // runs this five-opcode phase dispatcher. Each byte stays core-owned so
    // its work-RAM reads, flags, branches, and event samples remain exact.
    constexpr std::array<RoutineKey, 5U> kGiantFaceWaitDispatchInstructions{{
        {.physical_bank = 0x03U, .logical_pc = 0x7699U},
        {.physical_bank = 0x03U, .logical_pc = 0x769CU},
        {.physical_bank = 0x03U, .logical_pc = 0x76A1U},
        {.physical_bank = 0x03U, .logical_pc = 0x76A4U},
        {.physical_bank = 0x03U, .logical_pc = 0x76A6U},
    }};
    for (const auto routine : kGiantFaceWaitDispatchInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_giant_face_wait_dispatch_instructions;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native Giant-Face wait-dispatch hook.\n";
            return false;
        }
    }
    // The final four-tick countdown is shared by the Giant-Face helmet and
    // its sibling actor routes.  Its replacement carries every original
    // interrupt checkpoint, but is not an admitted routine row, so install
    // its precise tail as a visible core capability.
    constexpr RoutineKey kSharedFourTickTimerTail{
        .physical_bank = 0x02U,
        .logical_pc = 0x4CDAU,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kSharedFourTickTimerTail)
        == claimed_routines.end()) {
        if (!runtime.install_execution_hook({
                .routine = kSharedFourTickTimerTail,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    ++deployment.core_four_tick_timer_tail_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native four-tick timer tail hook.\n";
            return false;
        }
    }
    // This is the music command decoder's common duration-dispatch seam. It
    // is already covered by the sequencer's guarded transcript, but is an
    // internal branch target rather than an admitted routine row.
    constexpr RoutineKey kMusicDurationTail{
        .physical_bank = 0x45U,
        .logical_pc = 0x4667U,
    };
    if (std::find(claimed_routines.begin(), claimed_routines.end(), kMusicDurationTail)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kMusicDurationTail,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_music_duration_tail_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native music duration-tail hook.\n";
        return false;
    }
    // The duration decoder's matching RTS uses the caller's live frame.  Its
    // target is opaque at the 4667 internal seam, so retain the core's exact
    // stack-pop and final event sample instead of synthesizing a continuation.
    constexpr RoutineKey kMusicDurationReturn{
        .physical_bank = 0x45U,
        .logical_pc = 0x46D6U,
    };
    if (std::find(claimed_routines.begin(), claimed_routines.end(), kMusicDurationReturn)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kMusicDurationReturn,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_music_duration_return_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    ++deployment.core_rts_instructions;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native music duration-return hook.\n";
        return false;
    }
    // The common Mercy-Star post-callback tail only chooses the shared
    // four-tick timer. Its active-table and negative-activity branches remain
    // interpreted, while this bounded transfer is visible separately.
    constexpr RoutineKey kMercyStarNotLastTail{
        .physical_bank = 0x02U,
        .logical_pc = 0x5003U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kMercyStarNotLastTail)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kMercyStarNotLastTail,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_mercy_star_not_last_tail_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native Mercy-Star tail hook.\n";
        return false;
    }
    // 02:592B's idle arm is only a zero load and a taken branch into the
    // already core-owned shared RTS. Its nonzero jump remains interpreted.
    constexpr RoutineKey kUpperTableIdleGate{
        .physical_bank = 0x02U,
        .logical_pc = 0x592BU,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kUpperTableIdleGate)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kUpperTableIdleGate,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_upper_table_idle_gate_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native upper-table idle gate hook.\n";
        return false;
    }
    // 02:449B's mapped-parent gate returns immediately when the parent is
    // already active. Its zero arm enters the original setup path.
    constexpr RoutineKey kUpperTableParentIdleGate{
        .physical_bank = 0x02U,
        .logical_pc = 0x449BU,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kUpperTableParentIdleGate)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kUpperTableParentIdleGate,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_upper_table_parent_idle_gate_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native upper-table parent idle gate hook.\n";
        return false;
    }
    // 02:452E's upper-table actor wait arm reads a zero gate and an active
    // actor before returning through the shared RTS at 453B. Its two other
    // branches retain their ROM-owned active/reactivation behavior.
    constexpr RoutineKey kUpperTableActorIdleGate{
        .physical_bank = 0x02U,
        .logical_pc = 0x452EU,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kUpperTableActorIdleGate)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kUpperTableActorIdleGate,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_upper_table_actor_idle_gate_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native upper-table actor idle gate hook.\n";
        return false;
    }
    // 05:7356's stage-six hazard normally decrements a live timer and returns.
    // Its zero arm installs a new state through C018/C5C9, so that transition
    // remains assembly-owned while the common steady arm is native.
    constexpr RoutineKey kBonusHazardTimer{
        .physical_bank = 0x05U,
        .logical_pc = 0x7356U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kBonusHazardTimer)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kBonusHazardTimer,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_bonus_hazard_timer_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native bonus-hazard timer hook.\n";
        return false;
    }
    // 01:DE9C emits a bounded pair of VDC words per row. It remains an
    // auxiliary capability until live play has covered both terminal arms.
    constexpr RoutineKey kBank1VdcRowUpload{
        .physical_bank = 0x01U,
        .logical_pc = 0xDE9CU,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kBank1VdcRowUpload)
        == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kBank1VdcRowUpload,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_bank1_vdc_row_upload_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native bank-01 VDC row-upload hook.\n";
        return false;
    }
    // This post-collision timer tail is shared by several bank-02 actors.
    // The set-carry contact arm only transfers to the already-native scheduler;
    // its active continuation remains assembly-owned.
    constexpr RoutineKey kSharedCollisionFreeTimerTail{
        .physical_bank = 0x02U,
        .logical_pc = 0x4424U,
    };
    if (std::find(
            claimed_routines.begin(),
            claimed_routines.end(),
            kSharedCollisionFreeTimerTail)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kSharedCollisionFreeTimerTail,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.core_capability_entries;
                ++deployment.core_shared_collision_free_timer_tail_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.core_capability_accepted;
                    deployment.core_capability_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native shared collision-free timer tail hook.\n";
        return false;
    }
    // These fixed descriptors deliberately hand the original overlapping
    // clears back to the core's resumable TII opcode path. None serializes
    // thousands of writes through the ordinary replacement channel.
    constexpr std::array<RoutineKey, 4U> kCoreTiiInstructions{{
        {.physical_bank = 0x01U, .logical_pc = 0xC100U},
        {.physical_bank = 0x08U, .logical_pc = 0x585AU},
        {.physical_bank = 0x00U, .logical_pc = 0xE950U},
        {.physical_bank = 0x08U, .logical_pc = 0x559FU},
    }};
    for (const auto routine : kCoreTiiInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.core_capability_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.core_capability_accepted;
                        ++deployment.core_tii_transfers;
                        deployment.core_capability_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native core TII hook.\n";
            return false;
        }
    }
    // E979 is the bounded caller continuation after E9B6's palette-block
    // interpolation.  It returns to the already core-owned JSR at E976 on
    // every non-terminal pass, so this remains an auxiliary transcript until
    // live play has exercised both its loop and E994 terminal arms broadly.
    constexpr RoutineKey kPaletteFadeCallerTail{
        .physical_bank = 0x00U,
        .logical_pc = 0xE979U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(),
            kPaletteFadeCallerTail)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kPaletteFadeCallerTail,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_palette_fade_tail_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native palette-fade tail hook.\n";
        return false;
    }
    // The coroutine scheduler ends at a shared RTS whose stack frame depends
    // on whichever actor scheduled it.  Let the core consume that live frame
    // through the native RTS primitive rather than modelling a scheduler
    // return boundary here.
    constexpr RoutineKey kCoroutineSchedulerReturn{
        .physical_bank = 0x01U,
        .logical_pc = 0xC5E9U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(),
            kCoroutineSchedulerReturn)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kCoroutineSchedulerReturn,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_coroutine_scheduler_rts_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native coroutine-scheduler RTS hook.\n";
        return false;
    }
    // This per-voice load has no stack or device side effects, so it remains
    // a safe independent seam when the enclosing music transcript yields at
    // an interrupt horizon.
    constexpr RoutineKey kMusicVoiceStateLoad{
        .physical_bank = 0x45U,
        .logical_pc = 0x4092U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(),
            kMusicVoiceStateLoad)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kMusicVoiceStateLoad,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_music_voice_state_load_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native music voice-state load hook.\n";
        return false;
    }
    // The knight's relative call shares a live frame with the surrounding
    // bank-03 path. Keep its push, relative target, and interrupt boundary
    // in the core before considering the larger caller as a native domain.
    constexpr RoutineKey kKnightSourceSlotLoad{
        .physical_bank = 0x03U,
        .logical_pc = 0x73D0U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(),
            kKnightSourceSlotLoad)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kKnightSourceSlotLoad,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_knight_source_slot_load_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native knight source-slot load hook.\n";
        return false;
    }
    constexpr RoutineKey kKnightBsr{
        .physical_bank = 0x03U,
        .logical_pc = 0x73D3U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kKnightBsr)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kKnightBsr,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_knight_bsr_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native knight BSR hook.\n";
        return false;
    }
    // The BSR returns into a carry-sensitive branch. Its taken and
    // fall-through timing belong to the core with the live carry result.
    constexpr RoutineKey kKnightBcs{
        .physical_bank = 0x03U,
        .logical_pc = 0x73D5U,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(), kKnightBcs)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kKnightBcs,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_knight_bcs_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native knight BCS hook.\n";
        return false;
    }
    // The Knight movement helper owns its state transitions, but its literal
    // call/return boundaries can remain exact core instructions. Keep the
    // four seams visible independently until live play covers their callers.
    constexpr std::array<RoutineKey, 4U> kKnightMovementInstructionSeams{{
        {.physical_bank = 0x03U, .logical_pc = 0x7A74U},
        {.physical_bank = 0x03U, .logical_pc = 0x7A7CU},
        {.physical_bank = 0x03U, .logical_pc = 0x7A8EU},
        {.physical_bank = 0x03U, .logical_pc = 0x7AF2U},
    }};
    for (const auto routine : kKnightMovementInstructionSeams) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.auxiliary_transcript_entries;
                    if (entry.routine.logical_pc == 0x7A74U
                        || entry.routine.logical_pc == 0x7A7CU) {
                        ++deployment.auxiliary_knight_movement_jsr_entries;
                    } else {
                        ++deployment.auxiliary_knight_movement_rts_entries;
                    }
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.auxiliary_transcript_accepted;
                        deployment.auxiliary_transcript_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native knight movement seam hook.\n";
            return false;
        }
    }
    // The fixed scheduler repeats three calls to the shared $C060 helper,
    // then closes through a caller-owned RTS. Keep those literal stack seams
    // native while the scheduler's setup and marker writes stay interpreted.
    constexpr std::array<RoutineKey, 4U> kFixedSchedulerInstructionSeams{{
        {.physical_bank = 0x03U, .logical_pc = 0x7B4BU},
        {.physical_bank = 0x03U, .logical_pc = 0x7B5AU},
        {.physical_bank = 0x03U, .logical_pc = 0x7B69U},
        {.physical_bank = 0x03U, .logical_pc = 0x7B6CU},
    }};
    for (const auto routine : kFixedSchedulerInstructionSeams) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.auxiliary_transcript_entries;
                    if (entry.routine.logical_pc == 0x7B6CU) {
                        ++deployment.auxiliary_fixed_scheduler_rts_entries;
                    } else {
                        ++deployment.auxiliary_fixed_scheduler_jsr_entries;
                    }
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.auxiliary_transcript_accepted;
                        deployment.auxiliary_transcript_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native fixed-scheduler seam hook.\n";
            return false;
        }
    }
    // This independent knight counter tail returns through a live frame after
    // its per-actor counter check. Keep the core responsible for the pop.
    constexpr RoutineKey kKnightCounterReturn{
        .physical_bank = 0x03U,
        .logical_pc = 0x76CAU,
    };
    if (std::find(
            claimed_routines.begin(), claimed_routines.end(),
            kKnightCounterReturn)
            == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kKnightCounterReturn,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment, &replacement_rom](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_knight_counter_rts_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(
                    entry, work_ram, replacement_rom);
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native knight counter RTS hook.\n";
        return false;
    }
    // Bank 09's compact actor scan reuses the admitted bank-01 queue filer,
    // but its two resumable instruction boundaries are not routine entries.
    // Keep it isolated from the registry while live play validates its exact
    // C654 -> 60AB continuation.
    constexpr std::array<RoutineKey, 2U> kBank9ActorScanInstructions{{
        {.physical_bank = 0x09U, .logical_pc = 0x609BU},
        {.physical_bank = 0x09U, .logical_pc = 0x60ABU},
    }};
    for (const auto routine : kBank9ActorScanInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.auxiliary_transcript_entries;
                    ++deployment.auxiliary_bank9_actor_scan_entries;
                    if (entry.routine.logical_pc == 0x609BU) {
                        ++deployment.auxiliary_bank9_actor_scan_head_entries;
                    } else {
                        ++deployment.auxiliary_bank9_actor_scan_advance_entries;
                    }
                    if ((entry.registers.status & 0x08U) != 0U) {
                        ++deployment.auxiliary_bank9_actor_scan_decimal_entries;
                    }
                    if ((entry.registers.status & 0x20U) != 0U) {
                        ++deployment.auxiliary_bank9_actor_scan_transfer_entries;
                    }
                    if (entry.registers.mpr[0xC606U >> 13U] != 0x01U) {
                        ++deployment.auxiliary_bank9_actor_scan_queue_bank_mismatch_entries;
                    }
                    if (work_ram.size() >= 0x4EU
                        && std::to_integer<std::uint8_t>(work_ram[0x4DU])
                            >= 0x80U) {
                        ++deployment.auxiliary_bank9_actor_scan_wrapped_counter_entries;
                    }
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.auxiliary_transcript_accepted;
                        deployment.auxiliary_transcript_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native bank-09 actor-scan hook.\n";
            return false;
        }
    }
    // The upper-table scripted actors begin with an exact immediate-store
    // prefix, then execute the bank-01 common-initializer JSR in the core.
    // Keep those one-shot prefixes outside the routine registry while their
    // longer scripts remain core-owned.
    constexpr std::array<RoutineKey, 2U> kBank9ScriptedActorInitializers{{
        {.physical_bank = 0x09U, .logical_pc = 0x6FB1U},
        {.physical_bank = 0x09U, .logical_pc = 0x70EEU},
    }};
    for (const auto routine : kBank9ScriptedActorInitializers) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.auxiliary_transcript_entries;
                    ++deployment.auxiliary_bank9_scripted_actor_initializer_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(entry, work_ram, {});
                    if (result) {
                        ++deployment.auxiliary_transcript_accepted;
                        deployment.auxiliary_transcript_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native bank-09 scripted actor initializer hook.\n";
            return false;
        }
    }
    // The first scripted actor resumes after F08E at 09:7025. Its bounded
    // countdown leaves the script's RTS or the common-initializer JSR in the
    // core, preserving both existing call frames.
    constexpr std::array<RoutineKey, 2U> kBank9UpperTableCountdowns{{
        {.physical_bank = 0x09U, .logical_pc = 0x7025U},
        {.physical_bank = 0x09U, .logical_pc = 0x715DU},
    }};
    for (const auto routine : kBank9UpperTableCountdowns) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
                != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
            .routine = routine,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_bank9_upper_table_countdown_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(entry, work_ram, {});
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
            std::cerr << "Could not install native bank-09 upper-table countdown hook.\n";
            return false;
        }
    }
    constexpr std::array<RoutineKey, 2U> kBank9UpperTableParentDecays{{
        {.physical_bank = 0x09U, .logical_pc = 0x7043U},
        {.physical_bank = 0x09U, .logical_pc = 0x717BU},
    }};
    for (const auto routine : kBank9UpperTableParentDecays) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
                != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
            .routine = routine,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_bank9_upper_table_parent_decay_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(entry, work_ram, {});
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
            std::cerr << "Could not install native bank-09 upper-table parent-decay hook.\n";
            return false;
        }
    }
    // Both parent actors reset their animation counters and enter the shared
    // scheduler through a conventional JSR frame.  The replacement owns only
    // that prefix; the scheduler remains the established native boundary.
    constexpr std::array<RoutineKey, 2U> kBank9UpperTableParentInitializers{{
        {.physical_bank = 0x09U, .logical_pc = 0x704FU},
        {.physical_bank = 0x09U, .logical_pc = 0x7187U},
    }};
    for (const auto routine : kBank9UpperTableParentInitializers) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
                != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
            .routine = routine,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_bank9_upper_table_parent_initializer_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(entry, work_ram, {});
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
            std::cerr << "Could not install native bank-09 upper-table parent initializer hook.\n";
            return false;
        }
    }
    constexpr RoutineKey kBank2ActorCountdownReturn{
        .physical_bank = 0x02U,
        .logical_pc = 0x4F6CU,
    };
    if (std::find(claimed_routines.begin(), claimed_routines.end(),
                  kBank2ActorCountdownReturn) == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kBank2ActorCountdownReturn,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_bank2_actor_countdown_return_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(entry, work_ram, {});
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native bank-02 actor countdown-return hook.\n";
        return false;
    }
    constexpr RoutineKey kBank3ActorIdleBranch{
        .physical_bank = 0x03U,
        .logical_pc = 0x6001U,
    };
    if (std::find(claimed_routines.begin(), claimed_routines.end(),
                  kBank3ActorIdleBranch) == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kBank3ActorIdleBranch,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_bank3_actor_idle_branch_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(entry, work_ram, {});
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native bank-03 actor idle-branch hook.\n";
        return false;
    }
    constexpr RoutineKey kActorQueueTerminal{
        .physical_bank = 0x01U,
        .logical_pc = 0xC63EU,
    };
    if (std::find(claimed_routines.begin(), claimed_routines.end(),
                  kActorQueueTerminal) == claimed_routines.end()
        && !runtime.install_execution_hook({
            .routine = kActorQueueTerminal,
            .mode = HookMode::replace,
            .replacement_callback = [&deployment](
                const RoutineEntry& entry,
                const std::span<const std::byte> work_ram) {
                ++deployment.auxiliary_transcript_entries;
                ++deployment.auxiliary_actor_queue_terminal_entries;
                deployment.span.observe(entry.absolute_master_cycle);
                auto result = replace_native_gameplay_routine(entry, work_ram, {});
                if (result) {
                    ++deployment.auxiliary_transcript_accepted;
                    deployment.auxiliary_transcript_master_cycles +=
                        result->body_master_cycles;
                }
                return result;
            },
        })) {
        std::cerr << "Could not install native actor queue-terminal hook.\n";
        return false;
    }
    // The longer bonus route maps a sibling scan at bank 01:C559. Its clear
    // run, dispatch call, queue call, and post-queue continuation are all
    // instruction boundaries rather than admitted rows, so keep their
    // deployment explicitly visible while live play validates the route.
    constexpr std::array<RoutineKey, 4U> kBonusActorScanInstructions{{
        {.physical_bank = 0x01U, .logical_pc = 0xC559U},
        {.physical_bank = 0x01U, .logical_pc = 0xC566U},
        {.physical_bank = 0x01U, .logical_pc = 0xC568U},
        {.physical_bank = 0x01U, .logical_pc = 0xC56BU},
    }};
    for (const auto routine : kBonusActorScanInstructions) {
        if (std::find(claimed_routines.begin(), claimed_routines.end(), routine)
            != claimed_routines.end()) {
            continue;
        }
        if (!runtime.install_execution_hook({
                .routine = routine,
                .mode = HookMode::replace,
                .replacement_callback = [&deployment, &replacement_rom](
                    const RoutineEntry& entry,
                    const std::span<const std::byte> work_ram) {
                    ++deployment.auxiliary_transcript_entries;
                    ++deployment.auxiliary_bonus_actor_scan_entries;
                    deployment.span.observe(entry.absolute_master_cycle);
                    auto result = replace_native_gameplay_routine(
                        entry, work_ram, replacement_rom);
                    if (result) {
                        ++deployment.auxiliary_transcript_accepted;
                        deployment.auxiliary_transcript_master_cycles +=
                            result->body_master_cycles;
                    }
                    return result;
                },
            })) {
            std::cerr << "Could not install native bonus actor-scan hook.\n";
            return false;
        }
    }
    return true;
}

} // namespace devils_crush::app::host
