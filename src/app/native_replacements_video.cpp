// SPDX-License-Identifier: GPL-3.0-or-later

#include "native_replacements_internal.hpp"
#include "devils_crush/game_logic/sprite.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace devils_crush::app::detail {

using emulation::ReplacementMemoryWrite;
using emulation::RoutineEntry;
using emulation::RoutineKey;
using emulation::RoutineReplacement;

[[nodiscard]] std::optional<RoutineReplacement>
replace_sprite_emit_run(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    // The satellite-attribute-table emitter. One call walks an actor's sprite
    // descriptor for the current animation frame and pushes four 16-bit words
    // per entry — vertical, horizontal, pattern, attribute — through the VDC
    // data ports, advancing the table cursor in $4E as it goes.
    //
    // NOT ADMISSIBLE. This adapter has no @all regression and must not gain
    // one in its present shape, for two measured reasons.
    //
    // First, it cannot pay. The mean body is 1,328 master cycles against a
    // ~5,068-cycle timer period, but the routine runs inside the video window
    // where the user-event horizon is far shorter, so the valve admits 535 of
    // 73,818 calls. That converts 7.77% of all master cycles into 0.056%.
    //
    // Second, it is not yet faithful. Every admitted call runs 24 master
    // cycles longer than this transaction declares, which shifts later
    // interrupt arrivals and moves the state census off the assembly
    // baseline. The declared duration is not the error: the observed call
    // durations are exactly this model's set, and every internal write offset
    // matches assembly across all 73,818 calls. The 24 is charged somewhere
    // the work-RAM write trace cannot see, and the write observer does not
    // capture hardware-page writes, so the VDC store path is the open
    // suspect and is unresolved.
    //
    // Both problems point at the same fix: a checkpointed transcript, which
    // would let interrupts preempt between entries and price the VDC stores
    // at their real boundaries. The classifier below is verified independently
    // and is the foundation for that work.
    constexpr RoutineKey kRoutine{
        .physical_bank = 0x08U,
        .logical_pc = 0x40DFU,
    };
    constexpr std::uint16_t kReturnPc = 0x41ABU;
    constexpr std::uint16_t kVdcDataLow = 0x0002U;
    constexpr std::uint16_t kVdcDataHigh = 0x0003U;
    constexpr std::uint16_t kCountAddress = 0x202CU;
    constexpr std::uint16_t kDescriptorLowAddress = 0x202DU;
    constexpr std::uint16_t kDescriptorHighAddress = 0x202EU;
    constexpr std::uint16_t kEntryLowAddress = 0x202FU;
    constexpr std::uint16_t kEntryHighAddress = 0x2030U;
    constexpr std::uint16_t kScratchLowAddress = 0x2035U;
    constexpr std::uint16_t kScratchHighAddress = 0x2036U;
    constexpr std::uint16_t kCursorAddress = 0x204EU;

    // Master cycles. The prologue resolves both pointers and the entry count;
    // each entry then walks four fixed blocks whose branch shapes the
    // classifier reports. A VDC port write costs six CPU cycles where an
    // ordinary absolute write costs five.
    constexpr std::uint32_t kPrologueCycles = 249U;
    constexpr std::uint32_t kVerticalPositive = 105U;
    constexpr std::uint32_t kVerticalNegative = 99U;
    constexpr std::uint32_t kCameraBlock = 162U;
    constexpr std::uint32_t kHorizontalPositive = 117U;
    constexpr std::uint32_t kHorizontalNegative = 111U;
    constexpr std::uint32_t kPatternBlock = 126U;
    constexpr std::uint32_t kAttributeLowMerged = 93U;
    constexpr std::uint32_t kAttributeLowPlain = 72U;
    constexpr std::uint32_t kAttributeHighBlock = 78U;
    constexpr std::uint32_t kTailContinue = 60U;
    constexpr std::uint32_t kTailLast = 54U;

    // Write offsets, each measured to the end of the storing instruction.
    constexpr std::uint32_t kDescriptorLowWrite = 42U;
    constexpr std::uint32_t kDescriptorHighWrite = 69U;
    constexpr std::uint32_t kEntryLowWrite = 129U;
    constexpr std::uint32_t kEntryHighWrite = 168U;
    constexpr std::uint32_t kCountWrite = 207U;
    constexpr std::uint32_t kWidenLowPositive = 60U;
    constexpr std::uint32_t kWidenHighPositive = 93U;
    constexpr std::uint32_t kWidenLowNegative = 66U;
    constexpr std::uint32_t kWidenHighNegative = 99U;
    constexpr std::uint32_t kCameraLowWrite = 42U;
    constexpr std::uint32_t kCameraHighWrite = 78U;
    constexpr std::uint32_t kVerticalLowPort = 120U;
    constexpr std::uint32_t kVerticalHighPort = 156U;
    constexpr std::uint32_t kHorizontalLowPortPositive = 66U;
    constexpr std::uint32_t kHorizontalHighPortPositive = 105U;
    constexpr std::uint32_t kHorizontalLowPortNegative = 72U;
    constexpr std::uint32_t kHorizontalHighPortNegative = 111U;
    constexpr std::uint32_t kPatternLowPort = 69U;
    constexpr std::uint32_t kPatternHighPort = 120U;
    constexpr std::uint32_t kCursorWrite = 24U;
    constexpr std::uint32_t kCountDecrementWrite = 42U;

    // Five prologue writes plus fourteen per entry must clear the core's
    // 512-write transaction cap.
    constexpr std::uint8_t kMaximumEntries = 36U;
    constexpr std::size_t kPrologueWrites = 5U;
    constexpr std::size_t kWritesPerEntry = 14U;

    if (entry.routine != kRoutine || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // Decimal mode would change every ADC in the routine.
    if ((entry.registers.status & 0x08U) != 0U) {
        return std::nullopt;
    }
    // The VDC data ports are only addressable while the hardware page is
    // mapped at segment 0, which is also what the core's write validator
    // requires before it will accept them.
    if (entry.registers.mpr[0] != 0xFFU) {
        return std::nullopt;
    }

    // The routine's own bank supplies every descriptor and pointer it follows.
    const auto sprite_bank = entry.registers.mpr[2];
    std::size_t mapped_bank = sprite_bank;
    if (rom.size() == 0x60000U && sprite_bank < 0x80U) {
        mapped_bank = sprite_bank < 0x40U
            ? static_cast<std::size_t>(sprite_bank & 0x1FU)
            : static_cast<std::size_t>((sprite_bank & 0x0FU) + 0x20U);
    }
    const auto rom_offset = mapped_bank * 0x2000U;
    if (rom_offset + DC_SPRITE_EMIT_ROM_SIZE > rom.size()) {
        return std::nullopt;
    }
    const auto sprite_rom = rom.subspan(rom_offset, DC_SPRITE_EMIT_ROM_SIZE);

    dc_sprite_emit_run run{};
    if (!dc_classify_sprite_emit_run(
            reinterpret_cast<const std::uint8_t*>(work_ram.data()),
            work_ram.size(),
            reinterpret_cast<const std::uint8_t*>(sprite_rom.data()),
            sprite_rom.size(),
            entry.registers.x,
            &run)) {
        return std::nullopt;
    }
    if (run.count > kMaximumEntries) {
        return std::nullopt;
    }

    // Price the whole call before committing to it: this body is atomic, so
    // it may not span a user event or a serviceable timer interrupt.
    std::uint32_t body = kPrologueCycles;
    for (std::uint8_t index = 0U; index < run.count; ++index) {
        const auto& emitted = run.entries[index];
        body += emitted.vertical_negative ? kVerticalNegative
                                          : kVerticalPositive;
        body += kCameraBlock;
        body += emitted.horizontal_negative ? kHorizontalNegative
                                            : kHorizontalPositive;
        body += kPatternBlock;
        body += emitted.attribute_low_merged ? kAttributeLowMerged
                                             : kAttributeLowPlain;
        body += kAttributeHighBlock;
        body += index + 1U == run.count ? kTailLast : kTailContinue;
    }

    const auto horizon = std::min(
        entry.master_cycles_until_user_event,
        entry.master_cycles_until_timer_interrupt);
    if (body >= horizon) {
        return std::nullopt;
    }

    std::vector<ReplacementMemoryWrite> writes;
    writes.reserve(
        kPrologueWrites
        + static_cast<std::size_t>(run.count) * kWritesPerEntry);

    writes.push_back({
        .instruction = {0x08U, 0x40E5U},
        .logical_address = kDescriptorLowAddress,
        .value = static_cast<std::uint8_t>(run.descriptor_pointer),
        .master_cycle_offset = kDescriptorLowWrite,
    });
    writes.push_back({
        .instruction = {0x08U, 0x40EAU},
        .logical_address = kDescriptorHighAddress,
        .value = static_cast<std::uint8_t>(run.descriptor_pointer >> 8U),
        .master_cycle_offset = kDescriptorHighWrite,
    });
    writes.push_back({
        .instruction = {0x08U, 0x40F3U},
        .logical_address = kEntryLowAddress,
        .value = static_cast<std::uint8_t>(run.entry_pointer),
        .master_cycle_offset = kEntryLowWrite,
    });
    writes.push_back({
        .instruction = {0x08U, 0x40F8U},
        .logical_address = kEntryHighAddress,
        .value = static_cast<std::uint8_t>(run.entry_pointer >> 8U),
        .master_cycle_offset = kEntryHighWrite,
    });
    writes.push_back({
        .instruction = {0x08U, 0x40FDU},
        .logical_address = kCountAddress,
        .value = run.count,
        .master_cycle_offset = kCountWrite,
    });

    std::uint32_t cycle = kPrologueCycles;
    auto cursor = static_cast<std::uint8_t>(run.cursor - run.count);
    for (std::uint8_t index = 0U; index < run.count; ++index) {
        const auto& emitted = run.entries[index];
        const bool vertical_negative = emitted.vertical_negative != 0U;
        const bool horizontal_negative = emitted.horizontal_negative != 0U;
        const bool merged = emitted.attribute_low_merged != 0U;

        // The signed vertical offset widens into $35:$36 first.
        writes.push_back({
            .instruction = {0x08U, vertical_negative ? 0x411FU : 0x4110U},
            .logical_address = kScratchLowAddress,
            .value = emitted.widen_low,
            .master_cycle_offset = cycle
                + (vertical_negative ? kWidenLowNegative
                                     : kWidenLowPositive),
        });
        writes.push_back({
            .instruction = {0x08U, vertical_negative ? 0x4126U : 0x4116U},
            .logical_address = kScratchHighAddress,
            .value = emitted.widen_high,
            .master_cycle_offset = cycle
                + (vertical_negative ? kWidenHighNegative
                                     : kWidenHighPositive),
        });
        cycle += vertical_negative ? kVerticalNegative : kVerticalPositive;

        // The camera origin subtracts back out through the same pair.
        writes.push_back({
            .instruction = {0x08U, 0x412DU},
            .logical_address = kScratchLowAddress,
            .value = emitted.camera_low,
            .master_cycle_offset = cycle + kCameraLowWrite,
        });
        writes.push_back({
            .instruction = {0x08U, 0x4133U},
            .logical_address = kScratchHighAddress,
            .value = emitted.camera_high,
            .master_cycle_offset = cycle + kCameraHighWrite,
        });
        writes.push_back({
            .instruction = {0x08U, 0x413AU},
            .logical_address = kVdcDataLow,
            .value = emitted.vertical_low,
            .master_cycle_offset = cycle + kVerticalLowPort,
        });
        writes.push_back({
            .instruction = {0x08U, 0x4141U},
            .logical_address = kVdcDataHigh,
            .value = emitted.vertical_high,
            .master_cycle_offset = cycle + kVerticalHighPort,
        });
        cycle += kCameraBlock;

        writes.push_back({
            .instruction = {0x08U, horizontal_negative ? 0x415DU : 0x414DU},
            .logical_address = kVdcDataLow,
            .value = emitted.horizontal_low,
            .master_cycle_offset = cycle
                + (horizontal_negative ? kHorizontalLowPortNegative
                                       : kHorizontalLowPortPositive),
        });
        writes.push_back({
            .instruction = {0x08U, horizontal_negative ? 0x4165U : 0x4154U},
            .logical_address = kVdcDataHigh,
            .value = emitted.horizontal_high,
            .master_cycle_offset = cycle
                + (horizontal_negative ? kHorizontalHighPortNegative
                                       : kHorizontalHighPortPositive),
        });
        cycle += horizontal_negative ? kHorizontalNegative
                                     : kHorizontalPositive;

        writes.push_back({
            .instruction = {0x08U, 0x416FU},
            .logical_address = kVdcDataLow,
            .value = emitted.pattern_low,
            .master_cycle_offset = cycle + kPatternLowPort,
        });
        writes.push_back({
            .instruction = {0x08U, 0x4176U},
            .logical_address = kVdcDataHigh,
            .value = emitted.pattern_high,
            .master_cycle_offset = cycle + kPatternHighPort,
        });
        cycle += kPatternBlock;

        const auto attribute_low_cost =
            merged ? kAttributeLowMerged : kAttributeLowPlain;
        writes.push_back({
            .instruction = {0x08U, 0x418AU},
            .logical_address = kVdcDataLow,
            .value = emitted.attribute_low,
            .master_cycle_offset = cycle + attribute_low_cost,
        });
        cycle += attribute_low_cost;

        writes.push_back({
            .instruction = {0x08U, 0x419EU},
            .logical_address = kVdcDataHigh,
            .value = emitted.attribute_high,
            .master_cycle_offset = cycle + kAttributeHighBlock,
        });
        cycle += kAttributeHighBlock;

        ++cursor;
        writes.push_back({
            .instruction = {0x08U, 0x41A2U},
            .logical_address = kCursorAddress,
            .value = cursor,
            .master_cycle_offset = cycle + kCursorWrite,
        });
        writes.push_back({
            .instruction = {0x08U, 0x41A4U},
            .logical_address = kCountAddress,
            .value = static_cast<std::uint8_t>(run.count - index - 1U),
            .master_cycle_offset = cycle + kCountDecrementWrite,
        });
        cycle += index + 1U == run.count ? kTailLast : kTailContinue;
    }

    if (cycle != body) {
        return std::nullopt;
    }

    auto output = entry.registers;
    output.accumulator = run.accumulator;
    output.y = run.final_y;
    // DEC $2C reached zero, so Z is set and N clear. Carry and overflow are
    // whatever the last pattern high-byte ADC left standing.
    auto status = static_cast<std::uint8_t>(
        output.status & static_cast<std::uint8_t>(~0xC3U));
    status = static_cast<std::uint8_t>(status | 0x02U);
    if (run.carry != 0U) {
        status = static_cast<std::uint8_t>(status | 0x01U);
    }
    if (run.overflow != 0U) {
        status = static_cast<std::uint8_t>(status | 0x40U);
    }
    output.status = status;
    output.program_counter = kReturnPc;

    return RoutineReplacement{
        .return_instruction =
            {
                .physical_bank = kRoutine.physical_bank,
                .logical_pc = kReturnPc,
            },
        .output_registers = output,
        .body_master_cycles = body,
        .work_ram_writes = std::move(writes),
    };
}

} // namespace devils_crush::app::detail
