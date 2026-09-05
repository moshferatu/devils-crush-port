// SPDX-License-Identifier: GPL-3.0-or-later

// ROM-free boundary tests of the production transcript. The ROM regression
// separately compares the real emulator's complete timestamped bus stream.
#include "../src/app/native_replacements_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
using devils_crush::emulation::RoutineEntry;
using devils_crush::app::detail::replace_vram_upload;
using Ram = std::array<std::byte, 0x2000>;

void require(bool value, const char* label)
{
    if (!value) {
        throw std::runtime_error(label);
    }
}

RoutineEntry entry_at(std::uint16_t pc)
{
    RoutineEntry entry;
    entry.routine = {0U, pc};
    entry.registers.program_counter = pc;
    entry.registers.mpr = {0xFFU, 0xF8U, 2U, 3U, 0x10U, 0x45U, 1U, 0U};
    entry.registers.x = 0xA7U;
    entry.registers.stack_pointer = 0xF3U;
    return entry;
}

std::uint8_t mapped_byte(const RoutineEntry& entry, const Ram& ram,
    const std::vector<std::byte>& rom, std::uint16_t address)
{
    const auto bank = entry.registers.mpr[address >> 13U];
    if (bank == 0xF8U) {
        return std::to_integer<std::uint8_t>(ram[address & 0x1FFFU]);
    }
    const auto physical = bank < 0x40U ? bank % 32U : 32U + bank % 16U;
    return std::to_integer<std::uint8_t>(rom[physical * 8192U + address % 8192U]);
}

void check_transfer(const std::vector<std::byte>& rom, std::uint16_t count,
    std::uint16_t source, std::uint16_t entry_pc, std::uint8_t initial_y,
    std::uint8_t status)
{
    Ram ram{};
    for (std::size_t i = 0U; i < ram.size(); ++i) {
        ram[i] = static_cast<std::byte>((i * 13U + i / 256U) & 255U);
    }
    ram[0x2BU] = static_cast<std::byte>(count & 255U);
    ram[0x2CU] = static_cast<std::byte>(count >> 8U);
    ram[0x2DU] = std::byte{0x34};
    ram[0x2EU] = std::byte{0x12};
    ram[0x2FU] = static_cast<std::byte>(source & 255U);
    ram[0x30U] = static_cast<std::byte>(source >> 8U);
    auto entry = entry_at(entry_pc);
    entry.registers.y = initial_y;
    entry.registers.status = status;
    auto expected_y = entry_pc == 0xE365U ? std::uint8_t{0U} : initial_y;
    auto expected_source = source;
    auto remaining = count;
    // Zero is a 65,536-word transfer, not an empty one. Test its first chunk;
    // a whole address-space traversal would intentionally reach device reads.
    const auto total = count == 0U || count == 65535U ? 32U : unsigned(count);
    unsigned completed = 0U;
    while (completed < total) {
        const auto snapshot = ram;
        const auto replacement = replace_vram_upload(entry, ram, rom);
        require(replacement.has_value(), "supported transfer declined");
        require(ram == snapshot, "transcription mutated input RAM");
        require(replacement->checkpoints.size() <= 512U, "checkpoint bound");
        require(replacement->work_ram_writes.size() <= 104U, "write bound");
        std::uint32_t previous_cycle = 0U;
        for (const auto& point : replacement->checkpoints) {
            require(point.master_cycle_offset > previous_cycle, "nonmonotonic checkpoint");
            const auto pc = point.instruction.logical_pc;
            const bool port = pc == 0xE369U || pc == 0xE36EU || pc == 0xE373U
                || pc == 0xE37AU || pc == 0xE380U || pc == 0xE386U;
            require(point.interrupt_sample_master_cycle_offset
                == point.master_cycle_offset - (port ? 6U : 3U), "interrupt sample");
            if (pc == 0xE37DU) {
                require((point.registers.status & 0x82U) == 0U, "CLY changed N/Z");
            }
            previous_cycle = point.master_cycle_offset;
        }
        auto cycles = entry.routine.logical_pc == 0xE365U ? 138U : 0U;
        std::size_t write_index = 0U;
        const auto check_write = [&](std::uint16_t pc, std::uint16_t address,
                                     std::uint8_t value, std::uint32_t at_cycle) {
            require(write_index < replacement->work_ram_writes.size(), "missing write");
            const auto& write = replacement->work_ram_writes[write_index++];
            require(write.instruction.logical_pc == pc && write.instruction.physical_bank == 0U,
                "write instruction");
            require(write.logical_address == address && write.value == value, "write value/address");
            require(write.master_cycle_offset == at_cycle, "write timing");
            if (address >= 0x2000U) {
                ram[address - 0x2000U] = static_cast<std::byte>(value);
            }
        };
        if (entry.routine.logical_pc == 0xE365U) {
            check_write(0xE367U, 0x2006U, 0U, 18U);
            check_write(0xE369U, 0U, 0U, 36U);
            check_write(0xE36EU, 2U, 0x34U, 66U);
            check_write(0xE373U, 3U, 0x12U, 96U);
            check_write(0xE378U, 0x2006U, 2U, 114U);
            check_write(0xE37AU, 0U, 2U, 132U);
        }
        const auto words = std::min(32U, total - completed);
        for (unsigned i = 0U; i < words; ++i) {
            const auto low = mapped_byte(entry, ram, rom,
                static_cast<std::uint16_t>(expected_source + expected_y));
            expected_y = static_cast<std::uint8_t>(expected_y + 1U);
            const auto high = mapped_byte(entry, ram, rom,
                static_cast<std::uint16_t>(expected_source + expected_y));
            check_write(0xE380U, 2U, low, cycles + 39U);
            check_write(0xE386U, 3U, high, cycles + 84U);
            expected_y = static_cast<std::uint8_t>(expected_y + 1U);
            cycles += 102U;
            if (expected_y == 0U) {
                expected_source = static_cast<std::uint16_t>(expected_source + 256U);
                cycles += 12U;
                check_write(0xE38CU, 0x2030U,
                    static_cast<std::uint8_t>(expected_source >> 8U), cycles);
            }
            cycles += 24U;
            if ((remaining & 255U) == 0U) {
                cycles += 12U;
                check_write(0xE392U, 0x202CU,
                    static_cast<std::uint8_t>((remaining >> 8U) - 1U), cycles);
            }
            remaining = static_cast<std::uint16_t>(remaining - 1U);
            cycles += 18U;
            check_write(0xE394U, 0x202BU, static_cast<std::uint8_t>(remaining), cycles);
            cycles += remaining == 0U ? 30U : 36U;
        }
        require(write_index == replacement->work_ram_writes.size(), "unexpected write");
        require(cycles == replacement->body_master_cycles, "segment cycle count");
        require(replacement->output_registers.x == entry.registers.x
            && replacement->output_registers.stack_pointer == entry.registers.stack_pointer
            && replacement->output_registers.mpr == entry.registers.mpr, "preserved registers");
        const auto accumulator = static_cast<std::uint8_t>(remaining | (remaining >> 8U));
        const auto expected_status = static_cast<std::uint8_t>((status & ~0x82U)
            | (accumulator == 0U ? 2U : 0U) | (accumulator & 0x80U));
        require(replacement->output_registers.accumulator == accumulator
            && replacement->output_registers.y == expected_y
            && replacement->output_registers.status == expected_status, "output registers");
        require(replacement->return_instruction.logical_pc
            == (remaining == 0U ? 0xE39CU : 0xE37EU), "loop/return boundary");
        entry.routine = replacement->return_instruction;
        entry.registers = replacement->output_registers;
        completed += words;
    }
}
} // namespace

int main()
{
    try {
        std::vector<std::byte> rom(0x60000U);
        for (std::size_t i = 0U; i < rom.size(); ++i) {
            rom[i] = static_cast<std::byte>((i * 7U + i / 256U + i / 8192U) & 255U);
        }
        unsigned cases = 0U;
        for (const auto count : {0U, 1U, 2U, 31U, 32U, 33U, 64U, 127U,
                                128U, 129U, 255U, 256U, 257U, 65535U}) {
            for (const auto source : {0x9FFEU, 0xBFFEU, 0x28F0U}) {
                for (const auto status : {0U, 0xD7U, 0x10U, 0x87U}) {
                    for (const auto y : {0U, 0xFEU, 0xFFU}) {
                        check_transfer(rom, static_cast<std::uint16_t>(count),
                            static_cast<std::uint16_t>(source), 0xE37EU,
                            static_cast<std::uint8_t>(y), static_cast<std::uint8_t>(status));
                        ++cases;
                    }
                    check_transfer(rom, static_cast<std::uint16_t>(count),
                        static_cast<std::uint16_t>(source), 0xE365U, 0xFFU,
                        static_cast<std::uint8_t>(status));
                    ++cases;
                }
            }
        }
        Ram ram{};
        for (const auto count : {0U, 1U, 2U, 255U, 256U, 257U, 65535U}) {
            ram[0x2BU] = static_cast<std::byte>(count & 255U);
            ram[0x2CU] = static_cast<std::byte>(count >> 8U);
            ram[0x31U] = std::byte{0x5A};
            ram[0x32U] = std::byte{0xA5};
            auto entry = entry_at(0xE34CU);
            const auto result = replace_vram_upload(entry, ram, rom);
            require(result.has_value(), "address fill declined");
            require(result->work_ram_writes[0].value == 0x5AU
                && result->work_ram_writes[0].master_cycle_offset == 30U
                && result->work_ram_writes[1].value == 0xA5U
                && result->work_ram_writes[1].master_cycle_offset == 60U,
                "address-fill VDC latches");
            require(result->body_master_cycles == 138U
                + ((count & 255U) == 0U ? 12U : 0U) - (count == 1U ? 6U : 0U),
                "address-fill branch timing");
            require(result->return_instruction.logical_pc == (count == 1U ? 0xE364U : 0xE34CU),
                "address-fill closing branch");
            const auto remaining = static_cast<std::uint16_t>(count - 1U);
            require(result->output_registers.accumulator
                == static_cast<std::uint8_t>(remaining | (remaining >> 8U)), "address-fill count");
        }
        ram.fill(std::byte{0});
        ram[0x2BU] = std::byte{1};
        ram[0x30U] = std::byte{0x80};
        for (const auto pc : {0xE365U, 0xE37EU}) {
            for (const auto flag : {0x08U, 0x20U}) {
                auto entry = entry_at(static_cast<std::uint16_t>(pc));
                entry.registers.status = static_cast<std::uint8_t>(flag);
                require(!replace_vram_upload(entry, ram, rom), "unsupported flags accepted");
            }
            for (const auto segment : {0U, 1U}) {
                auto entry = entry_at(static_cast<std::uint16_t>(pc));
                entry.registers.mpr[segment] = 0U;
                require(!replace_vram_upload(entry, ram, rom), "unsupported mapping accepted");
            }
            auto entry = entry_at(static_cast<std::uint16_t>(pc));
            entry.registers.mpr[6U] = 0xFFU;
            ram[0x2FU] = std::byte{0xF0};
            ram[0x30U] = std::byte{0xBF};
            ram[0x2BU] = std::byte{8};
            require(replace_vram_upload(entry, ram, rom).has_value(), "ROM boundary declined early");
            ram[0x2BU] = std::byte{9};
            require(!replace_vram_upload(entry, ram, rom), "late device read accepted");
        }
        std::printf("VDC word-transfer boundary tests passed: %u cases plus guards.\n", cases);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VDC word-transfer test failed: %s\n", error.what());
        return 1;
    }
}
