// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstdint>
#include <optional>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// 01:C000-01:C0CB, bank 01's jump table: sixty-eight `JMP absolute` rows, three
// bytes each, that the rest of the cartridge reaches bank 01's routines --
// and, through them, bank 00's -- by. The bank-call wrapper at 00:E241 maps a
// request number onto a row and jumps into it, so a caller in any bank names
// the row rather than the routine, and the routine is free to move.
//
// Eighteen of the sixty-eight execute on the recorded route:
//
//   C012 -> $F08E   924   the shared motion step, reached by sixteen sites
//   C018 -> $C5C9  1508   the coroutine scheduler
//   C021 -> $C5EA    70
//   C024 -> $C5AB    44
//   C027 -> $C574   123
//   C02A -> $F54F    37
//   C03C -> $E93C   204
//   C042 -> $E1FD  1119
//   C05D -> $EBA5  1119
//   C060 -> $E241   996   the bank-call wrapper, reached through its own table
//   C063 -> $D6E2     7
//   C069 -> $D75E     3
//   C06C -> $F4FC    14
//   C06F -> $DAF8  2139   the busiest row on the route
//   C081 -> $E8B3    78
//   C087 -> $ECFD    58
//   C09F -> $D8CA     2
//   C0A2 -> $D8D2     1
//
// 8,446 executions and 101,352 master cycles at twelve each, which the profile
// prices at 102,288 -- the difference is interrupts taken on the instruction
// rather than the instruction itself.
//
// **A row is the shortest transcript this port can write, and the only one
// whose correctness is settled by the ROM alone.** `JMP absolute` reads two
// operand bytes, sets the program counter and charges ADDCYC(3) plus the
// LastCycle: no flag moves, no register moves, nothing is read from work RAM
// and nothing is written to it. The two operand bytes come through MPR6, and
// the transaction is admitted at `01:C0xx`, which *is* the statement that MPR6
// holds bank 01 -- so the bytes are bank 01's own $0012, $0018 and so on, and
// the target below each row is the disassembly's, not a guess about state.
//
// Each walk ends on its target, the same way a crossed `JSR` ends on its
// callee's first instruction, and for the same reason: the transcript did the
// transfer, and whatever owns the address on the far side picks the body up.
// Eleven of the eighteen targets are registered entries and continue natively:
// $F08E, $C5C9, $C5AB, $C574, $E93C, $E1FD, $EBA5, $E241, $F4FC, $DAF8 and
// $E8B3. The other seven -- $C5EA, $F54F, $D6E2, $D75E, $ECFD, $D8CA and
// $D8D2 -- hand back to the interpreter, which costs nothing: the twelve
// cycles were collected either way, and a row is not a claim about the routine
// it names.
//
// The single checkpoint is not decoration. The profile shows interrupts landing
// on these rows -- 01:C012's 924 executions cost 11,160 rather than 11,088 --
// and the checkpoint is where the core samples for one, at the instruction's
// LastCycle, exactly as the assembly does.
//
// The remaining fifty rows are handed back for want of a witness. Nothing about
// them is harder: each is the same twelve cycles and the same two bytes. But
// `--replace-hook` on an address the route never reaches reports `0 of 0`,
// which is not evidence, and this port admits a routine on measured calls
// rather than on an argument that the measurement would have been dull.
constexpr std::uint8_t kBank = 0x01U;

constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;

// Master cycles: three times the core's ADDCYC(3) plus the LastCycle it always
// charges.
constexpr std::uint32_t kJump = 12U;

// The row's target, or nothing if the address is not a row this walk owns.
[[nodiscard]] constexpr std::optional<std::uint16_t> target_of(
    const std::uint16_t logical_pc)
{
    switch (logical_pc) {
    case 0xC012U: return std::uint16_t{0xF08EU};
    case 0xC018U: return std::uint16_t{0xC5C9U};
    case 0xC021U: return std::uint16_t{0xC5EAU};
    case 0xC024U: return std::uint16_t{0xC5ABU};
    case 0xC027U: return std::uint16_t{0xC574U};
    case 0xC02AU: return std::uint16_t{0xF54FU};
    case 0xC03CU: return std::uint16_t{0xE93CU};
    case 0xC042U: return std::uint16_t{0xE1FDU};
    case 0xC05DU: return std::uint16_t{0xEBA5U};
    case 0xC060U: return std::uint16_t{0xE241U};
    case 0xC063U: return std::uint16_t{0xD6E2U};
    case 0xC069U: return std::uint16_t{0xD75EU};
    case 0xC06CU: return std::uint16_t{0xF4FCU};
    case 0xC06FU: return std::uint16_t{0xDAF8U};
    case 0xC081U: return std::uint16_t{0xE8B3U};
    case 0xC087U: return std::uint16_t{0xECFDU};
    case 0xC09FU: return std::uint16_t{0xD8CAU};
    case 0xC0A2U: return std::uint16_t{0xD8D2U};
    default: return std::nullopt;
    }
}

} // namespace

std::optional<RoutineReplacement> replace_bank_one_jump_table(
    const RoutineEntry& entry)
{
    if (entry.routine.physical_bank != kBank) {
        return std::nullopt;
    }
    const auto target = target_of(entry.routine.logical_pc);
    if (!target.has_value()) {
        return std::nullopt;
    }
    // Neither flag can reach a `JMP`: there is no arithmetic for decimal to
    // alter and no operand for the T flag to redirect. Both are refused anyway,
    // so that every walk in this port is admitted under the same conditions
    // rather than under a per-family argument about which ones happen not to
    // matter. The admitted counts are where that shows: eighteen rows at `N of
    // N` are eighteen statements that neither flag was ever set here.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    RoutineReplacement replacement;
    CpuRegisters registers = entry.registers;
    registers.program_counter = *target;

    // An instruction samples the interrupt one CPU cycle before its own last,
    // which is where the core's LastCycle sits.
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = kJump - 3U,
        .master_cycle_offset = kJump,
        .instruction = {kBank, entry.routine.logical_pc},
        .registers = registers,
    });

    // The bank the target logical address resolves through, which for these
    // rows is MPR6 -- bank 01, the bank the row itself is in -- or MPR7, which
    // this game holds at bank 00 for the whole route.
    replacement.return_instruction = RoutineKey{
        .physical_bank = registers.mpr[*target >> 13U],
        .logical_pc = *target,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = kJump;
    // Nothing is absorbed. A `JMP` pushes nothing and returns nowhere, so the
    // stack is exactly as it was found.
    return replacement;
}

} // namespace devils_crush::app::detail
