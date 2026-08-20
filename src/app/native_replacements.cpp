// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/app/native_replacements.hpp"
#include "native_replacements_internal.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace devils_crush::app {

using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

namespace {
[[nodiscard]] std::optional<RoutineReplacement>
replace_native_gameplay_routine_impl(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x07U,
            .logical_pc = 0x4003U,
        }) {
        return detail::replace_bank7_mode_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7B1FU,
        }) {
        return detail::replace_fixed_scheduler_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xDD6EU,
        }) {
        return detail::replace_ball_drain_threshold(entry, work_ram);
    }
    // The `else if` chain below is at the compiler's nesting limit, so new
    // routines are matched here, where each arm returns instead of nesting.
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xDC95U,
        }) {
        return detail::replace_motion_substep_split(entry, work_ram);
    }
    // The loop that spends what the split above hands it. Eleven entry points:
    // the head the coroutine scheduler resumes at, and the ten addresses this
    // family's eight crossed calls and one handed-back `BBR` arrive at. They
    // are written out one key at a time rather than folded into a predicate
    // because the admitted-registry generator reads this chain to check that
    // every routine it admits can actually be reached.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC721U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC72EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC742U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC74CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC759U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC75CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC75EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC761U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC768U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC76BU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC76EU,
            }) {
        return detail::replace_ball_substep_loop(entry, work_ram);
    }
    // The shared motion step. Eleven entry points: the head, the four leaves
    // its own calls cross into, the four addresses those calls come back to,
    // and the two the comparison helper at 00:E93C returns to. Written out one
    // key at a time for the same reason as the block above.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF08EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF09AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF0A4U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF0AEU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF0B8U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF0C0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF0FEU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF104U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF142U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF148U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF17EU,
            }) {
        return detail::replace_actor_motion_step(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC4DEU,
        }) {
        return detail::replace_actor_scan_preamble(entry, work_ram);
    }
    // Bank 09's palette-block colour rotator, and bank 09's first entry of any
    // kind. Straight-line work RAM: rotate three colours of one shadow block
    // and flag it for the whole-palette upload.
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x09U,
            .logical_pc = 0x60C7U,
        }) {
        return detail::replace_palette_colour_cycle(entry, work_ram);
    }
    // The score panel's two-by-two glyph writer. It reads four tile bytes from
    // the bank-09 ROM table, drives the VDC ports, and moves the panel cursor
    // two columns to the left.
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x09U,
            .logical_pc = 0x72FDU,
        }) {
        return detail::replace_score_panel_glyph(entry, work_ram, rom);
    }
    // The score panel's redraw poll. Four work RAM gates and a dirty bit; its
    // glyph-writing leaf is registered separately above.
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x09U,
            .logical_pc = 0x7237U,
        }) {
        return detail::replace_score_panel_poll(entry, work_ram);
    }
    // The main session loop. Ten entry points, all of them the address a call
    // returns to: the loop is a chain of crossings, and each walk ends on the
    // next `JSR` it makes rather than on a return.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC157U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC15AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC15DU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC166U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC169U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC16CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC16FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC17AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC185U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC190U,
            }) {
        return detail::replace_main_session_loop(entry, work_ram);
    }
    // Bank 01's jump table. Eighteen rows, one `JMP absolute` each: the walk
    // charges twelve master cycles, transfers, and ends on the row's target.
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC012U,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC018U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC021U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC024U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC027U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC02AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC03CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC042U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC05DU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC060U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC063U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC069U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC06CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC06FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC081U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC087U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC09FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC0A2U,
            }) {
        return detail::replace_bank_one_jump_table(entry);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC510U,
        }) {
        return detail::replace_actor_scan_run(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC52CU,
        }) {
        return detail::replace_actor_dispatch_vector(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC523U,
        }) {
        return detail::replace_actor_scan_advance(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x08U,
            .logical_pc = 0x40C3U,
        }) {
        return detail::replace_sat_tail_clear(entry, work_ram);
    }
    // The timer-interrupt sound driver. The interrupt vector is the only way
    // into the handler, so FC00 is its one real entry point. FC82 and FD76
    // are the instruction after each epilogue's SEI: the walk stops in front
    // of that SEI and is re-entered here, because the rerun request the
    // epilogue then reads can be written by a handler that preempted the
    // body, and only a fresh snapshot sees it.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xFC00U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xFC82U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xFD76U,
            }) {
        return detail::replace_sound_timer_driver(entry, work_ram, rom);
    }
    // One shared walk, sixteen entry points: the bank-45 music sequencer's
    // sample/envelope group, its PSG register programmer, its portamento
    // stepper and its command interpreter. Each is a subroutine the
    // per-frame voice walk calls; the walk inlines any nested call and stops
    // in front of the RTS. Three of the sixteen are not calls at all -- `4044`
    // and `4143` are loop heads, where a chunked transcript resumes, and
    // `4169` is where the channel loop picks up after the one instruction in
    // the bank a checkpoint cannot describe, the SEI at `4168`.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4015U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4044U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x409CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x413BU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4142U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4143U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4169U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4154U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x47E0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x482EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x485EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x49E4U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4211U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4228U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x457AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x4883U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x48B4U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x48F6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x49A2U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x49B6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x45U,
                .logical_pc = 0x49F9U,
            }) {
        return detail::replace_music_sequencer(entry, work_ram, rom);
    }
    // One shared walk, six entry points: the leaves that maintain the palette
    // shadow buffer at $27B9, and the upload that drains it to the VCE. None
    // of them calls anything, so each transcript is a single straight walk to
    // its own RTS -- except the fade-to-black stepper, which is too long for
    // one transcript and chunks at 00:EAB0, the head of its colour loop.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE93CU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE9B6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xEAA7U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xEAB0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xEB30U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xEB4DU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xEBA5U,
            }) {
        return detail::replace_palette_shadow(entry, work_ram, rom);
    }
    // The same job in bank 08, for the block the setup screens flash. Two
    // entry points, because the walk crosses the resolver call that turns its
    // asset reference into a source pointer.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x5977U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x5982U,
            }) {
        return detail::replace_setup_screen_palette_stager(
            entry,
            work_ram,
            rom);
    }
    // One shared walk, ten entry points: the bank-switching call wrappers
    // every cross-bank call in the game goes through. Each transcript makes the
    // call it exists to make and ends on the callee, or ends in front of its
    // own RTS.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE1FDU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE200U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE20DU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE219U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE241U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE255U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE262U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xECC6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xECE2U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xECF2U,
            }) {
        return detail::replace_bank_call_wrappers(entry, work_ram, rom);
    }
    // One shared walk, eleven entry points: the VDC interrupt handler. Each
    // transcript stops in front of the call it makes, the PLP that restores the
    // interrupted status byte, the arm the dispatcher hands off to, or -- for
    // the dispatcher's head -- the one instruction here that cannot be
    // transcribed at all, the status read at 00:E05C whose side effect is the
    // interrupt acknowledge. Two of the eleven are the RTIs past those PLPs,
    // entries of one instruction rather than tails of the walks that reach
    // them, because the PLP between has to be interpreted.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE058U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE05FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE0EBU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE114U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE117U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE123U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE135U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE136U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE179U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE183U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE194U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE195U,
            }) {
        return detail::replace_vblank_handler(entry, work_ram, rom);
    }

    // One shared walk, six entry points: the graphics driver's port-streaming
    // loops. Each transcript stops in front of an RTS or in front of the
    // backward branch that closes a loop, which is where a chunk hands the
    // stream back to the core.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE3C0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE3D7U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE3FCU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE403U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE41FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE439U,
            }) {
        return detail::replace_vram_upload(entry, work_ram, rom);
    }

    // One shared walk, eight entry points: the tilemap row writer at 00:F3E7
    // and the scroll driver that calls it. Each transcript stops in front of an
    // RTS, in front of one of the driver's six calls, or at 00:F4F2, where the
    // driver falls into the camera sweep.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF261U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF2ABU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF301U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF330U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF35FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF3B5U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF3E4U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF3E7U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF426U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF42AU,
            }) {
        return detail::replace_tilemap_upload(entry, work_ram, rom);
    }

    // One shared walk, twenty entry points: the frame around the collision
    // scanner. Four are routine heads -- the ball/table gate at 01:CA84, the
    // sweep setup at 01:CB2A, the single-slot probe at 01:CBAB and the contact
    // dispatcher at 01:CBCF -- one is the playfield-tile probe at 01:CD74, and
    // the rest are addresses a call this family makes returns to, or arms of
    // the BBR at 01:CB7E and the BBS at 01:CB71, which the core resolves
    // itself. 01:CB76 is one of those arms and one instruction long.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCA84U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCAA1U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCAB6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCAD3U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCAE0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCAEDU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB04U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB11U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB2AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB74U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB76U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB79U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB81U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCB8AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCBABU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCBB6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCBC2U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCBCAU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCBCFU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xCD74U,
            }) {
        return detail::replace_collision_tail(entry, work_ram, rom);
    }
    // The Sorcerer tick: the deployment delay at 02:586F, the arrival state at
    // 02:5886 and orbit at 02:58C2, plus the two addresses the core arrives at
    // after this family's calls to the collision-table interpreter at
    // 03:6966.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x586FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5886U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5893U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x58C2U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x58DDU,
            }) {
        return detail::replace_sorcerer_tick(entry, work_ram, rom);
    }

    // Three bank-01 arithmetic leaves: the path-length accumulator, the
    // range-squared guard and the direction-octant classifier.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC8F3U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC969U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xDAF8U,
            }) {
        return detail::replace_actor_geometry_leaf(entry, work_ram, rom);
    }

    // One shared walk, four entry points: the camera sweep's initializer and
    // the three addresses `00:F4FC` returns through.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF4F2U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF523U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF545U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xF54EU,
            }) {
        return detail::replace_actor_camera_sweep(entry, work_ram);
    }
    // One shared walk, five entry points: the SAT submission driver's head and
    // the four addresses a `JSR $40DF` returns to.
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x4036U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x4063U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x4075U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x4088U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x08U,
                .logical_pc = 0x4097U,
            }) {
        return detail::replace_sat_submission_step(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC606U,
        }) {
        return detail::replace_actor_queue_registration(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC51EU,
        }) {
        return detail::replace_actor_dispatch_call(entry);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC520U,
        }) {
        return detail::replace_actor_queue_call(entry);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC654U,
        }) {
        return detail::replace_actor_queue_return(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCB48U,
        }) {
        return detail::replace_collision_scan_run(entry, work_ram, rom);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCB61U,
        }) {
        return detail::replace_collision_scan_candidate_branch(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCB6BU,
        }) {
        return detail::replace_collision_scan_advance(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x08U,
            .logical_pc = 0x40DFU,
        }) {
        return detail::replace_sprite_emit_run(entry, work_ram, rom);
    }
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xC80AU,
            }
        || entry.routine
               == RoutineKey{
                   .physical_bank = 0x01U,
                   .logical_pc = 0xC7F4U,
               }) {
        return detail::replace_remaining_ball_presentation(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC7EFU,
        }) {
        return detail::replace_remaining_ball_alternate_scheduler(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC7F9U,
        }) {
        return detail::replace_zero_ball_countdown_tick(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC7FCU,
        }) {
        return detail::replace_zero_ball_expiry_scheduler(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC804U,
        }) {
        return detail::replace_zero_ball_presentation_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC812U,
        }) {
        return detail::replace_zero_ball_state_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC7EAU,
        }) {
        return detail::replace_remaining_ball_transition_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC7E5U,
        }) {
        return detail::replace_remaining_ball_decision(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5159U,
        }) {
        return detail::replace_devil_shooter_release_delay_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x518DU,
        }) {
        return detail::replace_devil_shooter_charge_after_audio(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5197U,
        }) {
        return detail::replace_devil_shooter_charge_tail(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5475U,
        }) {
        return detail::replace_devil_shooter_appearance_submission_prefix(
            entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5481U,
        }) {
        return detail::replace_devil_shooter_appearance_submission_tail(
            entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x520BU,
        }) {
        return detail::replace_devil_shooter_launch_continuation(
            entry, work_ram);
    }
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5208U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x52C3U,
            }) {
        return detail::replace_devil_shooter_charge_retraction_calls(
            entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5223U,
        }) {
        return detail::replace_devil_shooter_charge_entry_controller(
            entry, work_ram);
    }
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x4090U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x4099U,
            }) {
        return detail::replace_scene_layout_record_loop(entry, work_ram, rom);
    }
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x40F6U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x40FEU,
            }) {
        return detail::replace_scene_layout_record_loader(entry, work_ram, rom);
    }
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x4096U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x40FBU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x40A7U,
            }) {
        return detail::replace_scene_layout_record_calls(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5249U,
        }) {
        return detail::replace_devil_shooter_charge_controller(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x524CU,
        }) {
        return detail::replace_devil_shooter_charge_controller_tail(
            entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x525FU,
        }) {
        return detail::replace_devil_shooter_post_launch_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5EECU,
        }) {
        return detail::replace_giant_skeleton_controller(
            entry, work_ram, rom);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x52E3U,
        }) {
        return detail::replace_devil_shooter_charge_position(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x533AU,
        }) {
        return detail::replace_devil_shooter_charge_return(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x533BU,
        }) {
        return detail::replace_devil_shooter_charge_retraction(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x52C6U,
        }) {
        return detail::replace_devil_shooter_catch_retraction_tail(
            entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7B6DU,
        }) {
        return detail::replace_fixed_marker_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7CA7U,
        }) {
        return detail::replace_fixed_response_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x42EBU,
        }) {
        return detail::replace_dragon_egg_controller_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x66A3U,
        }) {
        return detail::replace_three_blobs_first_movement_prefix(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x66DEU,
        }) {
        return detail::replace_three_blobs_direct_continuation(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x676BU,
        }) {
        return detail::replace_three_blobs_shared_motion_idle(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x67C0U,
        }) {
        return detail::replace_three_blobs_collision_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x68A1U,
        }) {
        return detail::replace_gray_blob_first_live_prefix(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x57ADU,
        }) {
        return detail::replace_sorcerer_live_controller_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x57F8U,
        }) {
        return detail::replace_sorcerer_staging_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5808U,
        }) {
        return detail::replace_sorcerer_deployment_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5F22U,
        }) {
        return detail::replace_tower_insect_source_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x60B3U,
        }) {
        return detail::replace_tower_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x789AU,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x78A4U,
            }) {
        return detail::replace_bumper_initializers(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x79DEU,
        }) {
        return detail::replace_bumper_child_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7D4DU,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7DE9U,
            }) {
        return detail::replace_small_soldier_source_initializers(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7D6FU,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7E0DU,
            }) {
        return detail::replace_small_soldier_delayed_spawn_gates(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7428U,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x742BU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x742EU,
            }) {
        return detail::replace_giant_face_controller_initializers(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x766CU,
        }) {
        return detail::replace_giant_face_animator_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x76F7U,
        }) {
        return detail::replace_giant_face_crown_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7718U,
        }) {
        return detail::replace_giant_face_mouth_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x458AU,
        }) {
        return detail::replace_little_skull_gray_initializer(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x45A4U,
        }) {
        return detail::replace_little_skull_gray_release(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4515U,
        }) {
        return detail::replace_little_skull_live_continuation(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x61ABU,
        }) {
        return detail::replace_undead_head_selector_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x61A3U,
        }) {
        return detail::replace_undead_head_waiting_parent(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5722U,
        }) {
        return detail::replace_big_star_pending_helper(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5738U,
        }) {
        return detail::replace_big_star_pending_right_helper(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5F4AU,
        }) {
        return detail::replace_tower_insect_source_countdown_wait(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5F5BU,
        }) {
        return detail::replace_tower_insect_source_countdown_return(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5CCDU,
        }) {
        return detail::replace_side_worm_selector_gate(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5CC5U,
        }) {
        return detail::replace_side_worm_waiting_parent(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5D4AU,
        }) {
        return detail::replace_side_worm_active_timer_initializer(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4DF7U,
        }) {
        return detail::replace_giant_skeleton_eyeball_opening_wait(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4E24U,
        }) {
        return detail::replace_giant_skeleton_eyeball_steady_idle(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4F3DU,
        }) {
        return detail::replace_giant_skeleton_eyeball_final_wait(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x76C5U,
        }) {
        return detail::replace_giant_face_animator_active_wait(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4C33U,
        }) {
        return detail::replace_side_star_bonus_entrance_animation(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x41A0U,
        }) {
        return detail::replace_knight_terminal_release_helper_prefix(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x41A8U,
        }) {
        return detail::replace_knight_terminal_release_helper_setup_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x41ADU,
        }) {
        return detail::replace_knight_terminal_release_helper_table_setup_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x41C2U,
        }) {
        return detail::replace_knight_terminal_release_helper_call_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x41F5U,
        }) {
        return detail::replace_knight_terminal_release_helper_pointer_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x420BU,
        }) {
        return detail::replace_knight_terminal_release_helper_shared_call_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x420EU,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_mode_branch_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4210U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_pointer_low_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4212U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_pointer_low_store_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4215U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_pointer_high_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4217U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_pointer_high_store_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x421AU,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_scratch_clear_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x421CU,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_mode_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x421EU,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_mode_signed_branch_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4222U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_position_carry_clear_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4223U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_position_base_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4226U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_position_mode_add_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4228U,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_position_store_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x422BU,
        }) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x422EU}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_add_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4230U}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4233U}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_scratch_clear_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4235U}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_mode_load_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4237U}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_mode_signed_branch_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x423BU}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_carry_clear_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x423CU}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_base_load_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x423FU}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_scratch_add_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4241U}) {
        return detail::replace_knight_terminal_release_helper_post_return_position_high_actor_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4244U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_load_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4247U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_scratch_add_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4249U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x424CU}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_clear_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x424DU}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_zero_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4250U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_second_zero_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4253U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_attribute_load_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4256U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_attribute_mask_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4258U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_attribute_mode_merge_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x425AU}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_attribute_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x425DU}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_constant_load_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x425FU}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_constant_store_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4262U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_second_constant_load_prefix(
            entry, work_ram);
    }
    if (entry.routine == RoutineKey{.physical_bank = 0x02U, .logical_pc = 0x4264U}) {
        return detail::replace_knight_terminal_release_helper_post_return_actor_field_second_constant_store_prefix(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5ABU,
        }) {
        return detail::replace_actor_slot_allocator(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5ADU,
        }) {
        return detail::replace_knight_terminal_release_helper_shared_scan_read_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5B0U,
        }) {
        return detail::replace_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5B2U,
        }) {
        return detail::replace_knight_terminal_release_helper_shared_scan_increment_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5B3U,
        }) {
        return detail::replace_knight_terminal_release_helper_shared_scan_range_compare_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5B5U,
        }) {
        return detail::replace_knight_terminal_release_helper_shared_scan_loop_branch_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5B9U,
        }) {
        return detail::replace_knight_terminal_release_helper_allocator_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5BBU,
        }) {
        return detail::replace_knight_terminal_release_helper_allocator_store_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5BEU,
        }) {
        return detail::replace_knight_terminal_release_helper_allocator_mode_load_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C0U,
        }) {
        return detail::replace_knight_terminal_release_helper_allocator_mode_branch_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C7U,
        }) {
        return detail::replace_knight_terminal_release_helper_allocator_return_carry_clear_prefix(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x427CU,
        }) {
        return detail::replace_impact_burst_allocator_success_return(
            entry,
            work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x42A3U,
        }) {
        return detail::replace_impact_burst_audio_command_call(entry);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x42B0U,
        }) {
        return detail::replace_impact_burst_lifetime(entry, work_ram, rom);
    }
    if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5FDBU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5FE6U,
            }) {
        return detail::replace_tower_insect_initial_controller(
            entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6016U,
        }) {
        return detail::replace_tower_insect_collision_call(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5FFBU,
        }) {
        return detail::replace_tower_insect_collision_call(entry, work_ram);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x601CU,
        }) {
        return detail::replace_tower_insect_animation_call(entry);
    }
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x601EU,
        }) {
        return detail::replace_tower_insect_hit_gate(entry, work_ram);
    }
    std::optional<RoutineReplacement> replacement;
    if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF1B4U,
        }) {
        replacement =
            detail::replace_position_integration(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD081U,
        }) {
        replacement = detail::replace_flipper_geometry_lookup(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC5C9U,
        }) {
        replacement =
            detail::replace_coroutine_scheduler_entry(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC3CBU,
        }) {
        replacement =
            detail::replace_nudge_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC49EU,
        }) {
        replacement =
            detail::replace_session_input_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD466U,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x01U,
                .logical_pc = 0xD4B3U,
            }) {
        replacement = entry.routine.logical_pc == 0xD466U
            ? detail::replace_flipper_input_gate(entry, work_ram)
            : detail::replace_flipper_release_arm(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC1AU,
        }) {
        replacement = detail::replace_collision_scan_vertical_reject_entry(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCC98U,
        }) {
        replacement = detail::replace_collision_mask_lookup_entry(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCCD5U,
        }) {
        replacement =
            detail::replace_collision_geometry(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD500U,
        }) {
        replacement =
            detail::replace_flipper_phase_step(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD1A2U,
        }) {
        replacement = detail::replace_motion_rotation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xCFD8U,
        }) {
        replacement = detail::replace_collision_motion_adjustment(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD09EU,
        }) {
        replacement =
            detail::replace_flipper_projection(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD612U,
        }) {
        replacement = detail::replace_flipper_radius_projection(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xD62EU,
        }) {
        replacement = detail::replace_flipper_angle_projection(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xDD09U,
        }) {
        replacement =
            detail::replace_motion_damping(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF55CU,
        }) {
        replacement =
            detail::replace_score_accumulator(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4DC8U,
        }) {
        replacement = detail::replace_giant_skeleton_eyeball_initializer(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4DD9U,
        }) {
        replacement = detail::replace_giant_skeleton_eyeball_gate(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6876U,
        }) {
        replacement = detail::replace_gray_blob_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4F9FU,
        }) {
        replacement = detail::replace_mercy_star_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x50EEU,
        }) {
        replacement = detail::replace_devil_shooter_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x44E4U,
        }) {
        replacement = detail::replace_little_skull_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4485U,
        }) {
        replacement = detail::replace_little_skull_pair_initializer(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4455U,
        }) {
        replacement = detail::replace_little_skull_hidden_initializer(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x565CU,
        }) {
        replacement = detail::replace_big_star_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x433FU,
        }) {
        replacement = detail::replace_dragon_egg_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x61A0U,
        }) {
        replacement = detail::replace_undead_head_initializer(entry, work_ram);
    } else if (entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x5C4BU,
                   }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5C7CU,
            }) {
        replacement = detail::replace_side_worm_initializers(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A31U,
        }) {
        replacement = detail::replace_dragon_head_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5EE4U,
        }) {
        replacement = detail::replace_giant_skeleton_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x664AU,
        }) {
        replacement = detail::replace_three_blobs_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x65BCU,
        }) {
        replacement = detail::replace_three_blobs_controller_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6319U,
        }) {
        replacement = detail::replace_walled_star_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4A44U,
        }) {
        replacement = detail::replace_walled_star_access_initializer(entry, work_ram);
    } else if (entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x4A54U,
                   }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x4A4CU,
            }) {
        replacement = detail::replace_side_star_initializers(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4FC1U,
        }) {
        replacement = detail::replace_mercy_star_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x42F6U,
        }) {
        replacement =
            detail::replace_dragon_egg_controller(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4345U,
        }) {
        replacement =
            detail::replace_dragon_egg_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4357U,
        }) {
        replacement =
            detail::replace_dragon_egg_hit_continuation(entry, work_ram);
    } else if (entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x4371U,
                   }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x4387U,
            }) {
        replacement = entry.routine.logical_pc == 0x4371U
            ? detail::replace_dragon_egg_post_allocation(entry, work_ram)
            : detail::replace_dragon_egg_reset_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x445BU,
        }) {
        replacement =
            detail::replace_big_star_feature_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x570AU,
        }) {
        replacement =
            detail::replace_big_star_pending_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5674U,
        }) {
        replacement = detail::replace_big_star_rotation_wait(entry, work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x53DDU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5456U,
            }) {
        replacement = detail::replace_devil_shooter_companion_dormant(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5397U,
        }) {
        replacement = detail::replace_devil_shooter_companion_position(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5408U,
        }) {
        replacement = detail::replace_devil_shooter_companion_appearance(
            entry,
            work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x53C8U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5441U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x53E4U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x545DU,
            }) {
        replacement = (entry.routine.logical_pc == 0x53C8U
                        || entry.routine.logical_pc == 0x5441U)
            ? detail::replace_devil_shooter_companion_active_controller(
                  entry,
                  work_ram)
            : detail::replace_devil_shooter_companion_active_timer(
                  entry,
                  work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5116U,
        }) {
        replacement =
            detail::replace_devil_shooter_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x52B8U,
        }) {
        replacement =
            detail::replace_devil_shooter_catch_continuation(
                entry,
                work_ram);
    } else if (entry.routine.physical_bank == 0x02U
               && (entry.routine.logical_pc == 0x54B9U
                   || entry.routine.logical_pc == 0x54D0U
                   || entry.routine.logical_pc == 0x5500U)) {
        replacement = entry.routine.logical_pc == 0x54B9U
            ? detail::replace_skeleton_insect_left_gate(entry, work_ram)
            : entry.routine.logical_pc == 0x54D0U
            ? detail::replace_skeleton_insect_startup_countdown(
                  entry,
                  work_ram)
            : detail::replace_skeleton_insect_left_post_trigger(
                  entry,
                  work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5551U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5568U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x5590U,
            }) {
        replacement = entry.routine.logical_pc == 0x5551U
            ? detail::replace_skeleton_insect_right_gate(entry, work_ram)
            : entry.routine.logical_pc == 0x5568U
            ? detail::replace_skeleton_insect_right_startup_countdown(
                  entry,
                  work_ram)
            : detail::replace_skeleton_insect_right_post_trigger(
                  entry,
                  work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x55F9U,
        }) {
        replacement =
            detail::replace_skeleton_insect_center_gate(entry, work_ram);
    } else if (entry.routine.physical_bank == 0x02U
               && (entry.routine.logical_pc == 0x5604U
                   || entry.routine.logical_pc == 0x561AU
                   || entry.routine.logical_pc == 0x5625U
                   || entry.routine.logical_pc == 0x5630U)) {
        replacement = entry.routine.logical_pc == 0x5604U
            ? detail::replace_skeleton_insect_center_continuation(
                  entry,
                  work_ram)
            : entry.routine.logical_pc == 0x561AU
            ? detail::replace_skeleton_insect_center_toggle(entry, work_ram)
            : entry.routine.logical_pc == 0x5625U
            ? detail::replace_skeleton_insect_center_post_delay_handoff(
                  entry,
                  work_ram)
            : detail::replace_skeleton_insect_center_opening_gate(
                  entry,
                  work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A4BU,
        }) {
        replacement =
            detail::replace_dragon_head_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A6EU,
        }) {
        replacement =
            detail::replace_dragon_head_hit_continuation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5F5FU,
        }) {
        replacement = detail::replace_tower_insect_source_gate(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x63E1U,
        }) {
        replacement = detail::replace_devils_coffin_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x63F1U,
        }) {
        replacement =
            detail::replace_devils_coffin_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x640AU,
        }) {
        replacement =
            detail::replace_devils_coffin_opening(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6445U,
        }) {
        replacement =
            detail::replace_devils_coffin_closing(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6294U,
        }) {
        replacement =
            detail::replace_giant_door_initializer(entry, work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x62B8U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x62CBU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x62D4U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x62E2U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x62F0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x62FEU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x6308U,
            }) {
        replacement = entry.routine.logical_pc == 0x62B8U
            ? detail::replace_giant_door_hit_gate(entry, work_ram)
            : entry.routine.logical_pc == 0x62D4U
            ? detail::replace_giant_door_completion_score_prefix(entry, work_ram)
            : entry.routine.logical_pc == 0x62E2U
            ? detail::replace_giant_door_completion_scheduler_prefix(
                  entry, work_ram)
            : entry.routine.logical_pc == 0x62F0U
            ? detail::replace_giant_door_completion_request_prefix(
                  entry, work_ram)
            : entry.routine.logical_pc == 0x62FEU
            ? detail::replace_giant_door_completion_wait_gate(entry, work_ram)
            : entry.routine.logical_pc == 0x6308U
            ? detail::replace_giant_door_completion_channel_advance(
                  entry, work_ram)
            : detail::replace_giant_door_hit_continuation(
                  entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x74D8U,
        }) {
        replacement =
            detail::replace_giant_face_request_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x767EU,
        }) {
        replacement =
            detail::replace_giant_face_animator_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x76FAU,
        }) {
        replacement = detail::replace_giant_face_crown_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7726U,
        }) {
        replacement = detail::replace_giant_face_mouth_gate(entry, work_ram);
    } else if (entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x4C91U,
                   }
        || entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x4CFBU,
                   }
        || entry.routine
               == RoutineKey{
                   .physical_bank = 0x02U,
                   .logical_pc = 0x4D0BU,
               }) {
        replacement = entry.routine.logical_pc == 0x4CFBU
            ? detail::replace_giant_face_helmet_hit_continuation(
                entry,
                work_ram)
            : entry.routine.logical_pc == 0x4D0BU
            ? detail::replace_giant_face_helmet_followup_timer(entry, work_ram)
            : detail::replace_giant_face_helmet_controller(entry, work_ram);
    } else if (entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x4A5CU,
                   }
        || entry.routine
                   == RoutineKey{
                       .physical_bank = 0x02U,
                       .logical_pc = 0x4A64U,
                   }) {
        replacement = detail::replace_giant_face_helmet_outer_initializers(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4C78U,
        }) {
        replacement = detail::replace_giant_face_helmet_initializer(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4A93U,
        }) {
        replacement = detail::replace_giant_face_helmet_controller(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4B3FU,
        }) {
        replacement = detail::replace_giant_face_helmet_controller(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4BD5U,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x02U,
                .logical_pc = 0x4C0AU,
            }) {
        replacement = detail::replace_giant_face_helmet_controller(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4AD2U,
        }) {
        replacement = detail::replace_four_star_wall_pocket_post_audio_score(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4AE5U,
        }) {
        replacement = detail::replace_four_star_wall_pocket_post_appearance(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x4AEBU,
        }) {
        replacement = detail::replace_four_star_wall_pocket_animation(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5972U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_initializer(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5975U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_live_setup(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5991U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_countdown(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x59A5U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_opening(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x59C7U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_opening_followup(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x59E0U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_opening_expiry(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A08U,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_post_opening_schedule(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A0BU,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_post_opening_state(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5A1BU,
        }) {
        replacement = detail::replace_upper_wall_pocket_hand_post_opening_countdown(
            entry,
            work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x6339U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x638FU,
            }) {
        replacement = entry.routine.logical_pc == 0x6339U
            ? detail::replace_walled_star_hit_gate(entry, work_ram)
            : detail::replace_walled_star_post_destruction_idle(
                  entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
                       .logical_pc = 0x60C3U,
                   }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x60F9U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x6136U,
            }) {
        replacement = entry.routine.logical_pc == 0x60F9U
            ? detail::replace_tower_bonus_terminal(entry, work_ram)
            : entry.routine.logical_pc == 0x6136U
            ? detail::replace_tower_bonus_continuation(entry, work_ram)
            : detail::replace_tower_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x73D8U,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x73FEU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x740AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7420U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7418U,
            }) {
        replacement = entry.routine.logical_pc == 0x73D8U
            ? detail::replace_knight_hit_gate(entry, work_ram)
            : entry.routine.logical_pc == 0x73FEU
            ? detail::replace_knight_terminal_score_prefix(entry, work_ram)
            : entry.routine.logical_pc == 0x740AU
            ? detail::replace_knight_terminal_acknowledgement_prefix(entry, work_ram)
            : entry.routine.logical_pc == 0x7420U
            ? detail::replace_knight_terminal_scheduler(entry, work_ram)
            : detail::replace_knight_terminal_release_prefix(entry, work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x734EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7353U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x735AU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7361U,
            }) {
        replacement = detail::replace_knight_initializers(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x64FAU,
        }) {
        replacement = detail::replace_side_spider_initializer(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x646BU,
        }) {
        replacement = detail::replace_front_spider_initializer(entry, work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7378U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x73A6U,
            }) {
        replacement = entry.routine.logical_pc == 0x7378U
            ? detail::replace_knight_waiting_parent(entry, work_ram)
            : detail::replace_knight_movement_parent(entry, work_ram, rom);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x6490U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x651FU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x65A4U,
            }) {
        replacement = entry.routine.logical_pc == 0x65A4U
            ? detail::replace_spider_appearance(entry, work_ram)
            : detail::replace_spider_controller_parent(entry, work_ram, rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6054U,
        }) {
        replacement = detail::replace_tower_insect_animation(entry, work_ram);
    } else if (entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7D85U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7E23U,
            }) {
        replacement = detail::replace_small_soldier_source_gate(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x57E9U,
        }) {
        replacement = detail::replace_sorcerer_color_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x60E0U,
        }) {
        replacement =
            detail::replace_tower_hit_continuation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x611CU,
        }) {
        replacement = detail::replace_tower_post_dispatch(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x60A6U,
        }) {
        replacement =
            detail::replace_tower_guard_selector(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x78C7U,
        }) {
        replacement =
            detail::replace_bumper_hit_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x78F5U,
        }) {
        replacement =
            detail::replace_bumper_primary_progress(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7912U,
        }) {
        replacement =
            detail::replace_bumper_secondary_progress(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7CAFU,
        }) {
        replacement =
            detail::replace_fixed_response_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7B27U,
        }) {
        replacement =
            detail::replace_fixed_scheduler_gate(entry, work_ram);
    } else if (
        entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7B4EU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7B5DU,
            }) {
        replacement =
            detail::replace_fixed_scheduler_submission(entry);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7B7AU,
        }) {
        replacement =
            detail::replace_fixed_marker_gate(entry, work_ram);
    } else if (
        entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BBBU,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BCDU,
            }) {
        replacement =
            detail::replace_fixed_marker_submission(entry);
    } else if (
        entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BE0U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BE8U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7BECU,
            }) {
        replacement =
            detail::replace_fixed_marker_helper_segment(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x02U,
            .logical_pc = 0x5955U,
        }) {
        replacement = detail::replace_scaled_word_add(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x65C2U,
        }) {
        replacement =
            detail::replace_three_blobs_controller(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x67B3U,
        }) {
        replacement =
            detail::replace_three_blobs_appearance_gate(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE8B3U,
        }) {
        replacement = detail::replace_rng_update(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE837U,
        }) {
        replacement =
            detail::replace_signed_word_negation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE905U,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE7D7U,
            }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x00U,
                .logical_pc = 0xE6DCU,
            }) {
        if (entry.routine.logical_pc == 0xE905U) {
            replacement = detail::replace_signed_division(entry, work_ram);
        } else if (entry.routine.logical_pc == 0xE7D7U) {
            replacement = detail::replace_unsigned_24_division(
                entry,
                work_ram);
        } else {
            replacement = detail::replace_unsigned_byte_division(
                entry,
                work_ram);
        }
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE569U,
        }) {
        replacement = detail::replace_unsigned_byte_multiply(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE624U,
        }) {
        replacement = detail::replace_unsigned_multiply_round(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xE65CU,
        }) {
        replacement = detail::replace_signed_fixed_multiply(
            entry,
            work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x6966U,
        }) {
        replacement = detail::replace_collision_table_entry(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7FB1U,
        }) {
        replacement =
            detail::replace_collision_response_entry(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x03U,
            .logical_pc = 0x7F1AU,
        }
        || entry.routine
            == RoutineKey{
                .physical_bank = 0x03U,
                .logical_pc = 0x7F57U,
            }) {
        replacement = detail::replace_collision_consumer_no_hit(
            entry,
            work_ram,
            rom);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x00U,
            .logical_pc = 0xF4FCU,
        }) {
        replacement =
            detail::replace_actor_camera_activation(entry, work_ram);
    } else if (entry.routine
        == RoutineKey{
            .physical_bank = 0x01U,
            .logical_pc = 0xC574U,
        }) {
        replacement = detail::replace_actor_slot_reset(entry, work_ram);
    } else {
        replacement = detail::replace_cached_position_restore(
            entry,
            work_ram);
    }
    return replacement;
}


} // namespace

std::optional<RoutineReplacement> replace_native_gameplay_routine(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    return replace_native_gameplay_routine_impl(entry, work_ram, rom);
}

} // namespace devils_crush::app
