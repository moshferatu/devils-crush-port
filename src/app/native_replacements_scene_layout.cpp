// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace devils_crush::app::detail {
namespace {

using emulation::CpuRegisters;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

// 02:4090-02:40A7, the control loop around the normal-table scene-layout
// record loader. The active full-layout mode points <$2F/$30 at the 88
// twelve-byte records beginning at 02:45DB. Each nonterminal head calls the
// loader at 02:40F6, whose return lands on the second entry here; that
// continuation advances the pointer and branches back to the head. The loader
// is itself split below around its shared actor reset and record-selected
// initializer, so every call remains an explicit composition boundary.
constexpr std::uint8_t kBank = 0x02U;
constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint32_t kImplied = 6U;
constexpr std::uint32_t kImmediate = 6U;
constexpr std::uint32_t kZeroPage = 12U;
constexpr std::uint32_t kIndirect = 21U;
constexpr std::uint32_t kAbsoluteStore = 15U;
constexpr std::uint32_t kBranchTaken = 12U;
constexpr std::uint32_t kBranchNotTaken = 6U;

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

void compare_accumulator(CpuRegisters& registers, const std::uint8_t operand)
{
    const auto difference = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(registers.accumulator) - operand);
    registers.status = static_cast<std::uint8_t>(
        set_nz(registers.status, static_cast<std::uint8_t>(difference))
        & static_cast<std::uint8_t>(~kCarry));
    if ((difference & 0x100U) == 0U) {
        registers.status = static_cast<std::uint8_t>(
            registers.status | kCarry);
    }
}

void add_with_carry(CpuRegisters& registers, const std::uint8_t operand)
{
    const auto left = registers.accumulator;
    const auto sum = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(left)
        + static_cast<std::uint16_t>(operand)
        + static_cast<std::uint16_t>(registers.status & kCarry));
    const auto result = static_cast<std::uint8_t>(sum);
    const bool signed_overflow =
        ((left ^ result) & (operand ^ result) & kNegative) != 0U;
    registers.status = static_cast<std::uint8_t>(
        registers.status
        & static_cast<std::uint8_t>(~(kCarry | kOverflow)));
    if (sum > 0xFFU) {
        registers.status = static_cast<std::uint8_t>(
            registers.status | kCarry);
    }
    if (signed_overflow) {
        registers.status = static_cast<std::uint8_t>(
            registers.status | kOverflow);
    }
    registers.accumulator = result;
    registers.status = set_nz(registers.status, result);
}

[[nodiscard]] std::optional<std::uint8_t> read_mapped_rom(
    const CpuRegisters& registers,
    const std::span<const std::byte> rom,
    const std::uint16_t address)
{
    const auto segment = static_cast<std::size_t>(address >> 13U);
    if (segment >= registers.mpr.size()) {
        return std::nullopt;
    }
    const auto rom_offset =
        static_cast<std::size_t>(registers.mpr[segment]) * 0x2000U
        + static_cast<std::size_t>(address & 0x1FFFU);
    if (rom_offset >= rom.size()) {
        return std::nullopt;
    }
    return std::to_integer<std::uint8_t>(rom[rom_offset]);
}

} // namespace

std::optional<RoutineReplacement> replace_selective_scene_layout(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kStart{0x02U, 0x40A8U};
    constexpr RoutineKey kAdvance{0x02U, 0x40BDU};
    if ((entry.routine != kStart && entry.routine != kAdvance)
        || work_ram.size() < 0x2000U || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }
    RoutineReplacement result;
    auto registers = entry.registers;
    auto low = std::to_integer<std::uint8_t>(work_ram[0x2FU]);
    auto high = std::to_integer<std::uint8_t>(work_ram[0x30U]);
    std::uint32_t cycle = 0U;
    const auto checkpoint = [&](std::uint16_t pc, std::uint16_t next,
                                std::uint32_t cycles) {
        cycle += cycles;
        registers.program_counter = next;
        result.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycle - 3U,
            .master_cycle_offset = cycle,
            .instruction = {kBank, pc}, .registers = registers});
    };
    const auto load = [&](std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto store = [&](std::uint16_t pc, std::uint16_t address) {
        result.work_ram_writes.push_back({
            .instruction = {kBank, pc}, .logical_address = address,
            .value = registers.accumulator, .master_cycle_offset = cycle + 12U});
        checkpoint(pc, static_cast<std::uint16_t>(pc + 2U), 12U);
    };
    const auto call = [&](std::uint16_t pc, std::uint16_t target) {
        // JSR pushes its last operand address before charging its cycles.
        const auto saved = static_cast<std::uint16_t>(pc + 2U);
        for (const auto value : {static_cast<std::uint8_t>(saved >> 8U),
                                static_cast<std::uint8_t>(saved)}) {
            result.work_ram_writes.push_back({
                .instruction = {kBank, pc},
                .logical_address = static_cast<std::uint16_t>(
                    0x2100U + registers.stack_pointer),
                .value = value, .master_cycle_offset = cycle});
            --registers.stack_pointer;
        }
        checkpoint(pc, target, 21U);
        result.return_instruction = {registers.mpr[target >> 13U], target};
        result.output_registers = registers;
        result.body_master_cycles = cycle;
    };
    bool advance = entry.routine == kAdvance;
    if (!advance) {
        load(0xDBU); checkpoint(0x40A8U, 0x40AAU, 6U);
        low = registers.accumulator; store(0x40AAU, 0x202FU);
        load(0x45U); checkpoint(0x40ACU, 0x40AEU, 6U);
        high = registers.accumulator; store(0x40AEU, 0x2030U);
    }
    // The real table has 88 records. A corrupt/unbounded list declines
    // atomically rather than overflowing the bridge's checkpoint channel.
    for (unsigned scanned = 0; scanned < 100U; ++scanned) {
        if (advance) {
            registers.status &= static_cast<std::uint8_t>(~kCarry);
            checkpoint(0x40BDU, 0x40BEU, 6U);
            load(low); checkpoint(0x40BEU, 0x40C0U, 12U);
            add_with_carry(registers, 12U); checkpoint(0x40C0U, 0x40C2U, 6U);
            low = registers.accumulator; store(0x40C2U, 0x202FU);
            registers.accumulator = 0U; // CLA preserves flags, including carry.
            checkpoint(0x40C4U, 0x40C5U, 6U);
            add_with_carry(registers, high); checkpoint(0x40C5U, 0x40C7U, 12U);
            high = registers.accumulator; store(0x40C7U, 0x2030U);
            checkpoint(0x40C9U, 0x40B0U, 12U);
        }
        const auto category = read_mapped_rom(registers, rom,
            static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8U)));
        if (!category) { return std::nullopt; }
        load(*category); checkpoint(0x40B0U, 0x40B2U, 21U);
        compare_accumulator(registers, 0xFFU);
        checkpoint(0x40B2U, 0x40B4U, 6U);
        const bool terminal = *category == 0xFFU;
        checkpoint(0x40B4U, terminal ? 0x40CBU : 0x40B6U, terminal ? 12U : 6U);
        if (terminal) { call(0x40CBU, 0xC08AU); return result; }
        compare_accumulator(registers, 1U); checkpoint(0x40B6U, 0x40B8U, 6U);
        const bool selected = *category == 1U;
        checkpoint(0x40B8U, selected ? 0x40BAU : 0x40BDU, selected ? 6U : 12U);
        if (selected) { call(0x40BAU, 0x40F6U); return result; }
        advance = true;
    }
    return std::nullopt;
}

std::optional<RoutineReplacement> replace_scene_layout_record_loop(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kHead{.physical_bank = kBank, .logical_pc = 0x4090U};
    constexpr RoutineKey kAdvance{
        .physical_bank = kBank,
        .logical_pc = 0x4099U,
    };
    if ((entry.routine != kHead && entry.routine != kAdvance)
        || work_ram.size() < 0x2000U
        || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    RoutineReplacement replacement;
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    const auto append_checkpoint =
        [&replacement, &registers, &cycle](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };
    const auto finish = [&replacement, &registers, &cycle](
                            const std::uint16_t resume_pc) {
        registers.program_counter = resume_pc;
        replacement.return_instruction = {kBank, resume_pc};
        replacement.output_registers = registers;
        replacement.body_master_cycles = cycle;
    };

    if (entry.routine == kHead) {
        const auto pointer = static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(work_ram[0x002FU])
            | (static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(work_ram[0x0030U]))
               << 8U));
        const auto category = read_mapped_rom(registers, rom, pointer);
        if (!category) {
            return std::nullopt;
        }

        registers.accumulator = *category;
        registers.status = set_nz(registers.status, registers.accumulator);
        append_checkpoint(0x4090U, 0x4092U, kIndirect);

        compare_accumulator(registers, 0xFFU);
        append_checkpoint(0x4092U, 0x4094U, kImmediate);

        const bool terminal = (registers.status & kZero) != 0U;
        const auto resume_pc = static_cast<std::uint16_t>(
            terminal ? 0x40A7U : 0x4096U);
        append_checkpoint(
            0x4094U,
            resume_pc,
            terminal ? kBranchTaken : kBranchNotTaken);
        finish(resume_pc);
        return replacement;
    }

    registers.status = static_cast<std::uint8_t>(
        registers.status & static_cast<std::uint8_t>(~kCarry));
    append_checkpoint(0x4099U, 0x409AU, kImplied);  // CLC

    registers.accumulator = std::to_integer<std::uint8_t>(work_ram[0x002FU]);
    registers.status = set_nz(registers.status, registers.accumulator);
    append_checkpoint(0x409AU, 0x409CU, kZeroPage);  // LDA <$2F

    add_with_carry(registers, 0x0CU);
    append_checkpoint(0x409CU, 0x409EU, kImmediate);  // ADC #$0C

    replacement.work_ram_writes.push_back({
        .instruction = {kBank, 0x409EU},
        .logical_address = static_cast<std::uint16_t>(kWorkRamBase + 0x2FU),
        .value = registers.accumulator,
        .master_cycle_offset = cycle + kZeroPage,
    });
    append_checkpoint(0x409EU, 0x40A0U, kZeroPage);  // STA <$2F

    registers.accumulator = 0U;  // CLA changes no flags.
    append_checkpoint(0x40A0U, 0x40A1U, kImplied);

    add_with_carry(
        registers, std::to_integer<std::uint8_t>(work_ram[0x0030U]));
    append_checkpoint(0x40A1U, 0x40A3U, kZeroPage);  // ADC <$30

    replacement.work_ram_writes.push_back({
        .instruction = {kBank, 0x40A3U},
        .logical_address = static_cast<std::uint16_t>(kWorkRamBase + 0x30U),
        .value = registers.accumulator,
        .master_cycle_offset = cycle + kZeroPage,
    });
    append_checkpoint(0x40A3U, 0x40A5U, kZeroPage);  // STA <$30
    append_checkpoint(0x40A5U, 0x4090U, kBranchTaken);  // BRA $4090
    finish(0x4090U);
    return replacement;
}

std::optional<RoutineReplacement> replace_scene_layout_record_loader(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    constexpr RoutineKey kSelectSlot{
        .physical_bank = kBank,
        .logical_pc = 0x40F6U,
    };
    constexpr RoutineKey kCopyRecord{
        .physical_bank = kBank,
        .logical_pc = 0x40FEU,
    };
    if ((entry.routine != kSelectSlot && entry.routine != kCopyRecord)
        || work_ram.size() < 0x2000U
        || entry.registers.mpr[1] != 0xF8U
        || (entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }

    const auto record_pointer = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(work_ram[0x002FU])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(work_ram[0x0030U]))
           << 8U));
    RoutineReplacement replacement;
    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    const auto append_checkpoint =
        [&replacement, &registers, &cycle](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
        };
    const auto finish =
        [&replacement, &registers, &cycle](
            const std::uint8_t bank,
            const std::uint16_t resume_pc) {
            registers.program_counter = resume_pc;
            replacement.return_instruction = {bank, resume_pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
        };

    if (entry.routine == kSelectSlot) {
        registers.y = 0x01U;
        registers.status = set_nz(registers.status, registers.y);
        append_checkpoint(0x40F6U, 0x40F8U, kImmediate);  // LDY #$01

        const auto slot_address = static_cast<std::uint16_t>(
            record_pointer + registers.y);
        const auto slot = read_mapped_rom(registers, rom, slot_address);
        if (!slot) {
            return std::nullopt;
        }
        registers.accumulator = *slot;
        registers.status = set_nz(registers.status, registers.accumulator);
        append_checkpoint(0x40F8U, 0x40FAU, kIndirect);  // LDA [$2F],Y

        registers.x = registers.accumulator;
        registers.status = set_nz(registers.status, registers.x);
        append_checkpoint(0x40FAU, 0x40FBU, kImplied);  // TAX
        finish(kBank, 0x40FBU);  // Leave JSR $C027 to the core.
        return replacement;
    }

    struct RecordStore {
        std::uint16_t load_pc;
        std::uint16_t store_pc;
        std::uint16_t destination_base;
    };
    constexpr RecordStore kStores[] = {
        {0x40FFU, 0x4101U, 0x2CBEU},
        {0x4105U, 0x4107U, 0x2CBFU},
        {0x410BU, 0x410DU, 0x3180U},
        {0x4111U, 0x4113U, 0x3200U},
        {0x4117U, 0x4119U, 0x3300U},
        {0x411DU, 0x411FU, 0x3380U},
        {0x4123U, 0x4125U, 0x3900U},
        {0x4129U, 0x412BU, 0x3080U},
        {0x412FU, 0x4131U, 0x3000U},
    };
    constexpr std::uint16_t kIncrementPcs[] = {
        0x40FEU,
        0x4104U,
        0x410AU,
        0x4110U,
        0x4116U,
        0x411CU,
        0x4122U,
        0x4128U,
        0x412EU,
    };
    std::uint8_t initializer_low = 0U;
    std::uint8_t initializer_high = 0U;
    for (std::size_t index = 0U; index < 9U; ++index) {
        registers.y = static_cast<std::uint8_t>(registers.y + 1U);
        registers.status = set_nz(registers.status, registers.y);
        append_checkpoint(
            kIncrementPcs[index], kStores[index].load_pc, kImplied);  // INY

        const auto source_address = static_cast<std::uint16_t>(
            record_pointer + registers.y);
        const auto value = read_mapped_rom(registers, rom, source_address);
        if (!value) {
            return std::nullopt;
        }
        if (index == 0U) {
            initializer_low = *value;
        } else if (index == 1U) {
            initializer_high = *value;
        }
        registers.accumulator = *value;
        registers.status = set_nz(registers.status, registers.accumulator);
        append_checkpoint(
            kStores[index].load_pc,
            kStores[index].store_pc,
            kIndirect);  // LDA [$2F],Y

        const auto destination = static_cast<std::uint16_t>(
            kStores[index].destination_base
            + (index >= 2U ? registers.x : 0U));
        replacement.work_ram_writes.push_back({
            .instruction = {kBank, kStores[index].store_pc},
            .logical_address = destination,
            .value = registers.accumulator,
            .master_cycle_offset = cycle + kAbsoluteStore,
        });
        append_checkpoint(
            kStores[index].store_pc,
            static_cast<std::uint16_t>(kStores[index].store_pc + 3U),
            kAbsoluteStore);
    }

    registers.y = static_cast<std::uint8_t>(registers.y + 1U);
    registers.status = set_nz(registers.status, registers.y);
    append_checkpoint(0x4134U, 0x4135U, kImplied);  // INY

    const auto argument_address = static_cast<std::uint16_t>(
        record_pointer + registers.y);
    const auto argument = read_mapped_rom(registers, rom, argument_address);
    if (!argument) {
        return std::nullopt;
    }
    registers.accumulator = *argument;
    registers.status = set_nz(registers.status, registers.accumulator);
    append_checkpoint(0x4135U, 0x4137U, kIndirect);  // LDA [$2F],Y

    // The scratch pair has just been overwritten by this transcript. Resolve
    // the indirect jump from those record bytes, not the immutable input view.
    const auto target = static_cast<std::uint16_t>(
        initializer_low
        | (static_cast<std::uint16_t>(initializer_high) << 8U));
    const auto target_segment = static_cast<std::size_t>(target >> 13U);
    if (target_segment >= registers.mpr.size()) {
        return std::nullopt;
    }
    append_checkpoint(0x4137U, target, kIndirect);  // JMP ($2CBE)
    finish(registers.mpr[target_segment], target);
    return replacement;
}

std::optional<RoutineReplacement> replace_scene_layout_record_calls(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kCallLoader{
        .physical_bank = kBank,
        .logical_pc = 0x4096U,
    };
    constexpr RoutineKey kCallReset{
        .physical_bank = kBank,
        .logical_pc = 0x40FBU,
    };
    constexpr RoutineKey kReturn{
        .physical_bank = kBank,
        .logical_pc = 0x40A7U,
    };
    if ((entry.routine != kCallLoader
            && entry.routine != kCallReset
            && entry.routine != kReturn)
        || work_ram.size() < 0x2000U
        || entry.registers.mpr[1] != 0xF8U) {
        return std::nullopt;
    }

    if (entry.routine == kReturn) {
        if (!absorbed_return_fits_atomically(entry, 0U)) {
            return std::nullopt;
        }
        RoutineReplacement replacement{
            .return_instruction = kReturn,
            .output_registers = entry.registers,
            .body_master_cycles = 0U,
        };
        absorb_return(
            replacement,
            work_ram,
            entry.registers.stack_pointer,
            kReturn,
            0U,
            entry.registers);
        return replacement;
    }

    // A HuC6280 JSR writes the address of its last operand byte, high byte
    // first, before charging any of its seven CPU cycles. Stop at the child
    // entry so its independently registered replacement remains observable
    // and interruptible rather than being folded into this tiny wrapper.
    const auto call_pc = entry.routine.logical_pc;
    const auto target_pc = static_cast<std::uint16_t>(
        entry.routine == kCallLoader ? 0x40F6U : 0xC027U);
    const auto pushed = static_cast<std::uint16_t>(call_pc + 2U);
    const auto stack_high = entry.registers.stack_pointer;
    const auto stack_low = static_cast<std::uint8_t>(stack_high - 1U);

    auto output = entry.registers;
    output.stack_pointer = static_cast<std::uint8_t>(
        output.stack_pointer - 2U);
    output.program_counter = target_pc;

    RoutineReplacement replacement{
        .return_instruction = {
            .physical_bank = output.mpr[target_pc >> 13U],
            .logical_pc = target_pc,
        },
        .output_registers = output,
        .body_master_cycles = 21U,
    };
    replacement.work_ram_writes = {
        {
            .instruction = {kBank, call_pc},
            .logical_address = static_cast<std::uint16_t>(
                kWorkRamBase + 0x100U + stack_high),
            .value = static_cast<std::uint8_t>(pushed >> 8U),
            .master_cycle_offset = 0U,
        },
        {
            .instruction = {kBank, call_pc},
            .logical_address = static_cast<std::uint16_t>(
                kWorkRamBase + 0x100U + stack_low),
            .value = static_cast<std::uint8_t>(pushed & 0x00FFU),
            .master_cycle_offset = 0U,
        },
    };
    replacement.checkpoints.push_back({
        .interrupt_sample_master_cycle_offset = 18U,
        .master_cycle_offset = 21U,
        .instruction = {kBank, call_pc},
        .registers = output,
    });
    return replacement;
}

} // namespace devils_crush::app::detail
