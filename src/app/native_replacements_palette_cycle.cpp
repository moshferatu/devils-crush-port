// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// 09:60C7-09:612A, the palette-block colour rotator: take a block number in the
// accumulator, rotate three of that block's sixteen colours by one position,
// and mark the block for upload.
//
//   60C7  PHA                            keep the block number for the flag
//   60C8  STZ <$30 / ASL A / ROL <$30    ... five times: $30:A = block * 32
//   60D9  CLC / ADC #$B9 / STA <$2F      $2F/$30 = $27B9 + block * 32
//   60DE  LDA <$30 / ADC #$27 / STA <$30
//   60E4  LDY #$0A ... STA <$2E          keep colour 5 in <$2D/<$2E
//   60EF  LDY #$08 ... STA [$2F],Y       colour 5 <- colour 4
//   6101  LDY #$06 ... STA [$2F],Y       colour 4 <- colour 3
//   6113  LDY #$06 ... STA [$2F],Y       colour 3 <- the kept colour 5
//   611E  PLA / TAY
//   6120  LDA #$01 / STA $2CCC,Y         this block is dirty
//   6125  LDA #$FF / STA $2CCB           some block is dirty
//   612A  RTS
//
// **This is bank 09's first registered routine.** The bank had 1,493,322
// emulated master cycles and not one entry in the registry, and this run is
// 711,312 of them -- forty-eight per cent of the bank in fifty-nine
// instructions across a hundred bytes.
//
// Fifty-nine instructions, 996 executions each, and no branch anywhere: every
// row in the profile's `09:60C7`-`09:612A` range carries exactly 996. There is
// nothing to predict, so the transcript is the disassembly with the cycle
// counts filled in -- 708 master cycles including the return, which is what
// the profile's 711,312 comes to once the interrupts landing on it are taken
// out.
//
// **The addresses say what it is.** `$27B9` is the palette shadow, the
// 1,024-byte buffer `00:E950`'s `TII $27B9, $27BA, $03FF` clears and
// `00:EB46`'s whole-palette `TIA` uploads; thirty-two blocks of thirty-two
// bytes is thirty-two blocks of sixteen nine-bit colours, which is the VCE's
// entire palette RAM. `$2CCB` is the flag `00:EB4D` clears after that upload,
// and `$2CCC,Y` is its per-block half. So the three-way rotate of the pairs at
// offsets 6/7, 8/9 and 10/11 is colour cycling on colours three, four and five
// of one block, and the two stores at the end are how it asks to be seen.
//
// Every operand is in work RAM. There is no ROM read, no device access, no
// call and no branch, so the only thing that can stop the walk is the block
// number being out of range: `$27B9 + block * 32` leaves work RAM above block
// 195, and `read_ram` and `store` refuse the walk rather than reading through
// MPR2 into bank 08 the way the hardware would. The route only ever passes 2,
// 3 and 7.
//
// The return is absorbed. The `RTS` at 612A reads only the stack, so the same
// twenty-one master cycles are spent either way and the only question is which
// side of the seam they are counted on; the caller is the bank-call wrapper at
// 00:E252, reached with MPR3 already holding this bank, which is what
// admission at `09:60C7` states.
constexpr std::uint8_t kBank = 0x09U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;
constexpr std::uint16_t kStackBase = 0x2100U;

// The palette shadow, and the stride of one sixteen-colour block in it.
constexpr std::uint16_t kPaletteShadow = 0x27B9U;
constexpr std::uint16_t kBlockDirtyFlags = 0x2CCCU;
constexpr std::uint16_t kAnyBlockDirtyFlag = 0x2CCBU;

// Master cycles, three per CPU cycle. Every one of these is confirmed against
// the profile's own per-row totals for the range: 996 executions of `ASL A` is
// 6,048, of `ROL <$30` is 18,072, of `LDA [$2F],Y` is 21,060, and so on.
constexpr std::uint32_t kPush = 9U;
constexpr std::uint32_t kPull = 12U;
constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kZeroPageModify = 18U;
constexpr std::uint32_t kIndirectIndexed = 21U;
constexpr std::uint32_t kAbsolute = 15U;
constexpr std::uint32_t kReturn = 21U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status,
    const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    return static_cast<std::uint8_t>(
        result | static_cast<std::uint8_t>(value & kNegative));
}

}  // namespace

std::optional<RoutineReplacement> replace_palette_colour_cycle(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{
        .physical_bank = kBank,
        .logical_pc = 0x60C7U,
    };
    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Decimal would change both halves of the pointer arithmetic at 60DA and
    // 60E0, and the memory-operand T flag would take the left-hand side of
    // each `ADC` out of zero page instead of the accumulator.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement;
    replacement.work_ram_writes.reserve(21U);
    replacement.checkpoints.reserve(59U);

    CpuRegisters registers = entry.registers;
    std::uint32_t cycle = 0U;
    bool overflow = false;

    const auto read_ram =
        [&ram, &overflow](const std::uint16_t address) -> std::uint8_t {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return 0U;
            }
            return ram[address - kWorkRamBase];
        };

    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };

    // Stores land on their instruction's final cycle, so a write shares the
    // offset and the instruction of the checkpoint that follows it.
    std::size_t committed_writes = 0U;
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };

    // An instruction samples the interrupt one CPU cycle before its own last,
    // which is where the core's LastCycle sits.
    const auto append_checkpoint =
        [&replacement, &registers, &cycle, &commit_pending_writes](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };

    const auto store =
        [&replacement, &ram, &overflow](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return;
            }
            ram[address - kWorkRamBase] = value;
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto load_y = [&registers](const std::uint8_t value) {
        registers.y = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto add_with_carry = [&registers](const std::uint8_t value) {
        const auto left = registers.accumulator;
        const auto carry_in = static_cast<std::uint16_t>(
            (registers.status & kCarry) != 0U ? 1U : 0U);
        const auto wide = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(left)
            + static_cast<std::uint16_t>(value) + carry_in);
        const auto result = static_cast<std::uint8_t>(wide);
        auto status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
        if (wide > 0xFFU) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        if ((static_cast<std::uint8_t>(~(left ^ value))
             & static_cast<std::uint8_t>(left ^ result) & kNegative)
            != 0U) {
            status = static_cast<std::uint8_t>(status | kOverflow);
        }
        registers.accumulator = result;
        registers.status = set_nz(status, result);
    };

    // ---- 60C7  keep the block number ------------------------------------
    // The push writes to $2100 + S and then moves the pointer, which is one of
    // the twenty-one writes this routine makes.
    store(
        0x60C7U,
        static_cast<std::uint16_t>(kStackBase + registers.stack_pointer),
        registers.accumulator);
    registers.stack_pointer =
        static_cast<std::uint8_t>(registers.stack_pointer - 1U);
    append_checkpoint(0x60C7U, 0x60C8U, kPush);

    // ---- 60C8  $30:A = block * 32 ---------------------------------------
    // Five `ASL A` / `ROL <$30` pairs shift the block number left through the
    // accumulator and into <$30, which `STZ` cleared first: the low byte comes
    // out in A and the high three bits in <$30.
    store(0x60C8U, zp(0x30U), 0x00U);
    append_checkpoint(0x60C8U, 0x60CAU, kZeroPage);

    constexpr std::uint16_t kShiftPairs[5][2] = {
        {0x60CAU, 0x60CBU},
        {0x60CDU, 0x60CEU},
        {0x60D0U, 0x60D1U},
        {0x60D3U, 0x60D4U},
        {0x60D6U, 0x60D7U},
    };
    for (const auto& pair : kShiftPairs) {
        // ASL A
        const auto shifted =
            static_cast<std::uint8_t>(registers.accumulator << 1U);
        auto status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if ((registers.accumulator & kNegative) != 0U) {
            status = static_cast<std::uint8_t>(status | kCarry);
        }
        registers.accumulator = shifted;
        registers.status = set_nz(status, shifted);
        append_checkpoint(
            pair[0], static_cast<std::uint16_t>(pair[1]), kImplied);

        // ROL <$30, a read-modify-write that reports one write.
        const auto before = read_ram(zp(0x30U));
        const auto carry_in = static_cast<std::uint8_t>(
            (registers.status & kCarry) != 0U ? 1U : 0U);
        const auto rolled =
            static_cast<std::uint8_t>((before << 1U) | carry_in);
        auto rol_status = static_cast<std::uint8_t>(registers.status & ~kCarry);
        if ((before & kNegative) != 0U) {
            rol_status = static_cast<std::uint8_t>(rol_status | kCarry);
        }
        registers.status = set_nz(rol_status, rolled);
        store(pair[1], zp(0x30U), rolled);
        // `ROL <$30` is two bytes, so the fifth pair resumes at 60D9 -- the
        // `CLC` below -- by the same arithmetic as the four before it.
        append_checkpoint(
            pair[1], static_cast<std::uint16_t>(pair[1] + 2U),
            kZeroPageModify);
        if (overflow) {
            return std::nullopt;
        }
    }

    // ---- 60D9  $2F/$30 = $27B9 + block * 32 ------------------------------
    registers.status = static_cast<std::uint8_t>(registers.status & ~kCarry);
    append_checkpoint(0x60D9U, 0x60DAU, kImplied);

    add_with_carry(static_cast<std::uint8_t>(kPaletteShadow & 0xFFU));
    append_checkpoint(0x60DAU, 0x60DCU, kImmediate);

    store(0x60DCU, zp(0x2FU), registers.accumulator);
    append_checkpoint(0x60DCU, 0x60DEU, kZeroPage);

    load_accumulator(read_ram(zp(0x30U)));
    append_checkpoint(0x60DEU, 0x60E0U, kZeroPage);

    add_with_carry(static_cast<std::uint8_t>(kPaletteShadow >> 8U));
    append_checkpoint(0x60E0U, 0x60E2U, kImmediate);

    store(0x60E2U, zp(0x30U), registers.accumulator);
    append_checkpoint(0x60E2U, 0x60E4U, kZeroPage);

    if (overflow) {
        return std::nullopt;
    }

    // The pointer the rest of the routine indexes through. It is read back out
    // of zero page rather than recomputed, because that is what `[$2F],Y`
    // does and the two `STA`s above are the only things that set it.
    const auto block = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(read_ram(zp(0x30U))) << 8U
        | static_cast<std::uint16_t>(read_ram(zp(0x2FU))));
    const auto slot = [block](const std::uint8_t offset) {
        return static_cast<std::uint16_t>(block + offset);
    };

    // ---- 60E4  keep colour 5 in <$2D/<$2E -------------------------------
    load_y(0x0AU);
    append_checkpoint(0x60E4U, 0x60E6U, kImmediate);

    load_accumulator(read_ram(slot(registers.y)));
    append_checkpoint(0x60E6U, 0x60E8U, kIndirectIndexed);

    store(0x60E8U, zp(0x2DU), registers.accumulator);
    append_checkpoint(0x60E8U, 0x60EAU, kZeroPage);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x60EAU, 0x60EBU, kImplied);

    load_accumulator(read_ram(slot(registers.y)));
    append_checkpoint(0x60EBU, 0x60EDU, kIndirectIndexed);

    store(0x60EDU, zp(0x2EU), registers.accumulator);
    append_checkpoint(0x60EDU, 0x60EFU, kZeroPage);

    if (overflow) {
        return std::nullopt;
    }

    // ---- 60EF  colour 5 <- colour 4 -------------------------------------
    // The pattern below repeats once at 6101 for colour 4 <- colour 3: read
    // the pair's low byte into <$2B, read its high byte into A, walk Y up two
    // slots and write both back one colour along. The high byte goes first
    // because it is the one still in the accumulator.
    load_y(0x08U);
    append_checkpoint(0x60EFU, 0x60F1U, kImmediate);

    load_accumulator(read_ram(slot(registers.y)));
    append_checkpoint(0x60F1U, 0x60F3U, kIndirectIndexed);

    store(0x60F3U, zp(0x2BU), registers.accumulator);
    append_checkpoint(0x60F3U, 0x60F5U, kZeroPage);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x60F5U, 0x60F6U, kImplied);

    load_accumulator(read_ram(slot(registers.y)));
    append_checkpoint(0x60F6U, 0x60F8U, kIndirectIndexed);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x60F8U, 0x60F9U, kImplied);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x60F9U, 0x60FAU, kImplied);

    store(0x60FAU, slot(registers.y), registers.accumulator);
    append_checkpoint(0x60FAU, 0x60FCU, kIndirectIndexed);

    load_y(static_cast<std::uint8_t>(registers.y - 1U));
    append_checkpoint(0x60FCU, 0x60FDU, kImplied);

    load_accumulator(read_ram(zp(0x2BU)));
    append_checkpoint(0x60FDU, 0x60FFU, kZeroPage);

    store(0x60FFU, slot(registers.y), registers.accumulator);
    append_checkpoint(0x60FFU, 0x6101U, kIndirectIndexed);

    if (overflow) {
        return std::nullopt;
    }

    // ---- 6101  colour 4 <- colour 3 -------------------------------------
    load_y(0x06U);
    append_checkpoint(0x6101U, 0x6103U, kImmediate);

    load_accumulator(read_ram(slot(registers.y)));
    append_checkpoint(0x6103U, 0x6105U, kIndirectIndexed);

    store(0x6105U, zp(0x2BU), registers.accumulator);
    append_checkpoint(0x6105U, 0x6107U, kZeroPage);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x6107U, 0x6108U, kImplied);

    load_accumulator(read_ram(slot(registers.y)));
    append_checkpoint(0x6108U, 0x610AU, kIndirectIndexed);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x610AU, 0x610BU, kImplied);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x610BU, 0x610CU, kImplied);

    store(0x610CU, slot(registers.y), registers.accumulator);
    append_checkpoint(0x610CU, 0x610EU, kIndirectIndexed);

    load_y(static_cast<std::uint8_t>(registers.y - 1U));
    append_checkpoint(0x610EU, 0x610FU, kImplied);

    load_accumulator(read_ram(zp(0x2BU)));
    append_checkpoint(0x610FU, 0x6111U, kZeroPage);

    store(0x6111U, slot(registers.y), registers.accumulator);
    append_checkpoint(0x6111U, 0x6113U, kIndirectIndexed);

    if (overflow) {
        return std::nullopt;
    }

    // ---- 6113  colour 3 <- the colour 5 kept at the top ------------------
    // This is what makes it a rotation rather than a shift: the pair saved
    // before anything moved comes back in at the front.
    load_y(0x06U);
    append_checkpoint(0x6113U, 0x6115U, kImmediate);

    load_accumulator(read_ram(zp(0x2DU)));
    append_checkpoint(0x6115U, 0x6117U, kZeroPage);

    store(0x6117U, slot(registers.y), registers.accumulator);
    append_checkpoint(0x6117U, 0x6119U, kIndirectIndexed);

    load_y(static_cast<std::uint8_t>(registers.y + 1U));
    append_checkpoint(0x6119U, 0x611AU, kImplied);

    load_accumulator(read_ram(zp(0x2EU)));
    append_checkpoint(0x611AU, 0x611CU, kZeroPage);

    store(0x611CU, slot(registers.y), registers.accumulator);
    append_checkpoint(0x611CU, 0x611EU, kIndirectIndexed);

    if (overflow) {
        return std::nullopt;
    }

    // ---- 611E  mark the block dirty --------------------------------------
    registers.stack_pointer =
        static_cast<std::uint8_t>(registers.stack_pointer + 1U);
    load_accumulator(read_ram(
        static_cast<std::uint16_t>(kStackBase + registers.stack_pointer)));
    append_checkpoint(0x611EU, 0x611FU, kPull);

    load_y(registers.accumulator);
    append_checkpoint(0x611FU, 0x6120U, kImplied);

    load_accumulator(0x01U);
    append_checkpoint(0x6120U, 0x6122U, kImmediate);

    store(
        0x6122U,
        static_cast<std::uint16_t>(kBlockDirtyFlags + registers.y),
        registers.accumulator);
    append_checkpoint(0x6122U, 0x6125U, kAbsolute);

    load_accumulator(0xFFU);
    append_checkpoint(0x6125U, 0x6127U, kImmediate);

    store(0x6127U, kAnyBlockDirtyFlag, registers.accumulator);
    append_checkpoint(0x6127U, 0x612AU, kAbsolute);

    if (overflow) {
        return std::nullopt;
    }

    // ---- 612A  the return, absorbed --------------------------------------
    // Ending past the `RTS` rather than in front of it. It reads only the
    // stack, so the same twenty-one master cycles are spent either way and
    // nothing new is asked of the core; what changes is which side of the seam
    // they are counted on. The declaration is what keeps the routine-call
    // observer able to close the call where the assembly closed it.
    const auto low = read_ram(static_cast<std::uint16_t>(
        kStackBase + static_cast<std::uint8_t>(registers.stack_pointer + 1U)));
    const auto high = read_ram(static_cast<std::uint16_t>(
        kStackBase + static_cast<std::uint8_t>(registers.stack_pointer + 2U)));
    if (overflow) {
        return std::nullopt;
    }
    const auto return_pc = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high) << 8U)
        + static_cast<std::uint16_t>(low) + 1U);

    auto at_return = registers;
    at_return.program_counter = 0x612AU;
    replacement.absorbed_returns.push_back({
        .instruction = {kBank, 0x612AU},
        .master_cycle_offset = cycle,
        .registers = at_return,
    });
    registers.stack_pointer =
        static_cast<std::uint8_t>(registers.stack_pointer + 2U);
    append_checkpoint(0x612AU, return_pc, kReturn);

    registers.program_counter = return_pc;
    replacement.return_instruction = {
        registers.mpr[return_pc >> 13U],
        return_pc,
    };
    replacement.output_registers = registers;
    replacement.body_master_cycles = cycle;
    return replacement;
}

}  // namespace devils_crush::app::detail
