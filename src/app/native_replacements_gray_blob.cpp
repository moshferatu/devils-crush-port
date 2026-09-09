// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_replacements_internal.hpp"
namespace devils_crush::app::detail {
using emulation::RoutineKey; using emulation::RoutineEntry; using emulation::RoutineReplacement;
[[nodiscard]] std::optional<RoutineReplacement>
replace_gray_blob_live_wait_prefix(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram)
{
    constexpr RoutineKey kRoutine{.physical_bank = 0x03U, .logical_pc = 0x68BFU};
    if (entry.routine != kRoutine || entry.registers.program_counter != 0x68BFU
        || entry.registers.mpr[1] != 0xF8U || entry.registers.mpr[3] != 0x03U
        || (entry.registers.status & 0x20U) != 0U || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }

    // The ROM's non-expiring movement branch lands directly on RTS at 6905.
    // Phase countdown is a separate post-movement path at 68FD, not 6905.
    // Execute appearance expiry and collision selection too, then hand off
    // movement expiry at 68DE or collision response at 6906.
    auto r = entry.registers;
    RoutineReplacement result{};
    std::uint32_t cycles = 0U;
    const auto nz = [&r](const std::uint8_t value) {
        r.status = static_cast<std::uint8_t>((r.status & 0x7DU)
            | (value == 0U ? 0x02U : value & 0x80U));
    };
    const auto read = [&](const std::size_t offset) {
        return std::to_integer<std::uint8_t>(work_ram[offset + r.x]);
    };
    const auto step = [&](const std::uint16_t pc, const std::uint16_t next,
                          const std::uint32_t cost) {
        cycles += cost;
        r.program_counter = next;
        result.checkpoints.push_back({
            .interrupt_sample_master_cycle_offset = cycles - 3U,
            .master_cycle_offset = cycles,
            .instruction = {0x03U, pc},
            .registers = r,
        });
    };
    const auto write = [&](const std::uint16_t pc, const std::uint16_t base,
                           const std::uint8_t value, const std::uint32_t cost) {
        result.work_ram_writes.push_back({
            .instruction = {0x03U, pc},
            .logical_address = static_cast<std::uint16_t>(base + r.x),
            .value = value,
            .master_cycle_offset = cycles + cost,
        });
    };
    const auto appearance = static_cast<std::uint8_t>(read(0x1A00U) - 1U);
    nz(appearance);
    write(0x68BFU, 0x3A00U, appearance, 21U);
    step(0x68BFU, 0x68C2U, 21U);
    step(0x68C2U, appearance != 0U ? 0x68D1U : 0x68C4U, appearance != 0U ? 12U : 6U);
    if (appearance == 0U) {
        r.accumulator = 4U; nz(r.accumulator);
        step(0x68C4U, 0x68C6U, 6U);
        write(0x68C6U, 0x3A00U, r.accumulator, 15U);
        step(0x68C6U, 0x68C9U, 15U);
        r.accumulator = read(0x1980U); nz(r.accumulator);
        step(0x68C9U, 0x68CCU, 15U);
        r.accumulator ^= 1U; nz(r.accumulator);
        step(0x68CCU, 0x68CEU, 6U);
        write(0x68CEU, 0x3980U, r.accumulator, 15U);
        step(0x68CEU, 0x68D1U, 15U);
    }
    r.accumulator = read(0x1800U); nz(r.accumulator);
    step(0x68D1U, 0x68D4U, 15U);
    const bool collision = (r.accumulator & 0x80U) != 0U;
    step(0x68D4U, collision ? 0x68D6U : 0x68D9U, collision ? 6U : 12U);
    if (collision) {
        step(0x68D6U, 0x6906U, 12U);
    } else {
        const auto movement = static_cast<std::uint8_t>(read(0x1B80U) - 1U);
        nz(movement);
        write(0x68D9U, 0x3B80U, movement, 21U);
        step(0x68D9U, 0x68DCU, 21U);
        step(0x68DCU, movement != 0U ? 0x6905U : 0x68DEU, movement != 0U ? 12U : 6U);
    }
    result.return_instruction = {0x03U, r.program_counter};
    result.output_registers = r;
    result.body_master_cycles = cycles;
    if (r.program_counter == 0x6905U) {
        absorb_return(result, work_ram, r.stack_pointer, {0x03U, 0x6905U}, cycles, r);
    }
    return result;
}
}
