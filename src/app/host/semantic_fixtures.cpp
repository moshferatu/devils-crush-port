// SPDX-License-Identifier: GPL-3.0-or-later
#include "semantic_fixtures.hpp"

namespace devils_crush::app::host {

int run_unsigned_square_root_semantic_fixture()
{
    // This fixture is deliberately ROM-free.  It drives the public dispatcher
    // across every possible `$30:$2F` operand, varies every restored X value,
    // and applies the replacement's ordered writes before checking the leaf's
    // documented result.  The player-session replay separately proves the
    // timer-interleaved instruction checkpoints on all 665 natural calls.
    constexpr std::array<std::byte, 0U> empty_rom{};
    std::size_t verified = 0U;
    for (std::uint32_t operand = 0U; operand <= 0xFFFFU; ++operand) {
        std::vector<std::byte> work_ram(0x2000U);
        work_ram[0x002FU] = static_cast<std::byte>(operand & 0xFFU);
        work_ram[0x0030U] = static_cast<std::byte>(operand >> 8U);

        const auto x = static_cast<std::uint8_t>(operand >> 8U);
        const auto status = static_cast<std::uint8_t>(
            ((operand & 0x01U) != 0U ? 0x01U : 0U)
            | ((operand & 0x02U) != 0U ? 0x04U : 0U)
            | ((operand & 0x04U) != 0U ? 0x40U : 0U));
        const devils_crush::emulation::RoutineEntry entry{
            .routine = {.physical_bank = 0x00U, .logical_pc = 0xE85DU},
            .registers = {
                .program_counter = 0xE85DU,
                .accumulator = static_cast<std::uint8_t>(operand),
                .x = x,
                .y = static_cast<std::uint8_t>(operand >> 3U),
                .stack_pointer = 0xF8U,
                .status = status,
            },
        };
        const auto replacement =
            devils_crush::app::replace_native_gameplay_routine(
                entry, work_ram, empty_rom);
        if (!replacement || replacement->return_instruction
                != devils_crush::emulation::RoutineKey{0x00U, 0xE8A3U}
            || replacement->checkpoints.empty()
            || !replacement->absorbed_returns.empty()) {
            std::cerr << "Unsigned square-root fixture did not replace "
                      << operand << ".\n";
            return EXIT_FAILURE;
        }

        for (const auto& write : replacement->work_ram_writes) {
            if (write.logical_address < 0x2000U
                || write.logical_address >= 0x4000U) {
                std::cerr << "Unsigned square-root fixture emitted an "
                             "out-of-range write.\n";
                return EXIT_FAILURE;
            }
            work_ram[write.logical_address - 0x2000U] =
                static_cast<std::byte>(write.value);
        }

        std::uint16_t root = 0U;
        while (static_cast<std::uint32_t>(root + 1U)
                   * static_cast<std::uint32_t>(root + 1U)
               <= operand) {
            ++root;
        }
        const auto remainder = static_cast<std::uint16_t>(
            operand - static_cast<std::uint32_t>(root) * root);
        const auto four_times_root = static_cast<std::uint16_t>(4U * root);
        const auto& output = replacement->output_registers;
        const auto expected_nz = static_cast<std::uint8_t>(
            x == 0U ? 0x02U : (x & 0x80U) != 0U ? 0x80U : 0U);
        if (output.program_counter != 0xE8A3U || output.accumulator != root
            || output.x != x || output.y != entry.registers.y
            || output.stack_pointer != entry.registers.stack_pointer
            || (output.status & 0x82U) != expected_nz
            || std::to_integer<std::uint8_t>(work_ram[0x002FU]) != 0U
            || std::to_integer<std::uint8_t>(work_ram[0x0030U]) != 0U
            || std::to_integer<std::uint8_t>(work_ram[0x0042U]) != 0U
            || std::to_integer<std::uint8_t>(work_ram[0x0043U]) != root
            || std::to_integer<std::uint8_t>(work_ram[0x002BU])
                != (four_times_root & 0xFFU)
            || std::to_integer<std::uint8_t>(work_ram[0x002CU])
                != (four_times_root >> 8U)
            || std::to_integer<std::uint8_t>(work_ram[0x002DU])
                != (remainder & 0xFFU)
            || std::to_integer<std::uint8_t>(work_ram[0x002EU])
                != (remainder >> 8U)) {
            std::cerr << "Unsigned square-root fixture changed operand "
                      << operand << " incorrectly.\n";
            return EXIT_FAILURE;
        }
        ++verified;
    }

    std::vector<std::byte> guard_work_ram(0x2000U);
    for (const auto status : std::array<std::uint8_t, 2U>{0x08U, 0x20U}) {
        const devils_crush::emulation::RoutineEntry guarded_entry{
            .routine = {.physical_bank = 0x00U, .logical_pc = 0xE85DU},
            .registers = {.program_counter = 0xE85DU, .status = status},
        };
        if (devils_crush::app::replace_native_gameplay_routine(
                guarded_entry, guard_work_ram, empty_rom)) {
            std::cerr << "Unsigned square-root fixture accepted decimal or "
                         "T mode.\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "Unsigned square-root semantic fixture verified: "
              << verified << " exhaustive operands\n";
    return EXIT_SUCCESS;
}

int run_unsigned_word_multiply_partial_semantic_fixture()
{
    // This leaf is the eight-bit half of the cartridge's 16-bit multiply.
    // Cover every multiplier bit pattern, plus a grid of accumulator and
    // multiplicand words, through the public dispatcher.  The player-session
    // replay separately covers every timer-interleaved natural call.
    constexpr std::array<std::byte, 0U> empty_rom{};
    constexpr std::array<std::uint16_t, 4U> values{
        0x0000U,
        0x0001U,
        0x80FFU,
        0xFFFFU,
    };
    constexpr std::array<std::uint8_t, 4U> statuses{
        0x00U,
        0x01U,
        0x04U,
        0x41U,
    };
    std::size_t verified = 0U;
    for (const auto accumulator : values) {
        for (const auto multiplicand : values) {
            for (std::uint16_t multiplier_word = 0U;
                 multiplier_word <= 0xFFU;
                 ++multiplier_word) {
                const auto multiplier = static_cast<std::uint8_t>(
                    multiplier_word);
                const auto input_status = statuses[verified % statuses.size()];
                std::vector<std::byte> work_ram(0x2000U);
                work_ram[0x002DU] = static_cast<std::byte>(
                    multiplicand & 0xFFU);
                work_ram[0x002EU] = static_cast<std::byte>(
                    multiplicand >> 8U);
                work_ram[0x0030U] = static_cast<std::byte>(multiplier);
                work_ram[0x0042U] = static_cast<std::byte>(
                    accumulator & 0xFFU);
                work_ram[0x0043U] = static_cast<std::byte>(
                    accumulator >> 8U);
                const devils_crush::emulation::RoutineEntry entry{
                    .routine = {.physical_bank = 0x00U, .logical_pc = 0xE5BCU},
                    .registers = {
                        .program_counter = 0xE5BCU,
                        .accumulator = static_cast<std::uint8_t>(
                            accumulator >> 8U),
                        .x = 0xA5U,
                        .y = 0x5AU,
                        .stack_pointer = 0xF2U,
                        .status = input_status,
                    },
                };
                const auto replacement =
                    devils_crush::app::replace_native_gameplay_routine(
                        entry, work_ram, empty_rom);
                if (!replacement || replacement->return_instruction
                        != devils_crush::emulation::RoutineKey{0x00U, 0xE5EBU}
                    || replacement->checkpoints.empty()
                    || !replacement->absorbed_returns.empty()) {
                    std::cerr << "Unsigned-word-multiply partial fixture did "
                                 "not replace multiplier "
                              << multiplier_word << ".\n";
                    return EXIT_FAILURE;
                }

                for (std::size_t index = 0; index + 1U < replacement->checkpoints.size(); ++index) {
                    const auto& current = replacement->checkpoints[index];
                    const auto& next = replacement->checkpoints[index + 1U];
                    if (current.registers.program_counter != next.instruction.logical_pc) {
                        std::cerr << "Unsigned-word-multiply checkpoint skipped a ROM instruction.\n";
                        return EXIT_FAILURE;
                    }
                }
                std::uint16_t expected = accumulator;
                std::uint8_t shifting_multiplier = multiplier;
                std::uint8_t add_count = 0U;
                for (std::size_t bit = 0U; bit < 8U; ++bit) {
                    const bool add = (shifting_multiplier & 0x80U) != 0U;
                    expected = static_cast<std::uint16_t>(expected << 1U);
                    shifting_multiplier = static_cast<std::uint8_t>(
                        shifting_multiplier << 1U);
                    if (add) {
                        expected = static_cast<std::uint16_t>(
                            expected + multiplicand);
                        ++add_count;
                    }
                }
                const auto expected_writes = static_cast<std::size_t>(
                    24U + 2U * add_count);
                if (replacement->body_master_cycles
                        != static_cast<std::uint32_t>(600U + 72U * add_count)
                    || replacement->work_ram_writes.size() != expected_writes
                    || replacement->output_registers.program_counter != 0xE5EBU
                    || replacement->output_registers.x != 0U
                    || replacement->output_registers.y != entry.registers.y
                    || replacement->output_registers.stack_pointer
                        != entry.registers.stack_pointer
                    || (replacement->output_registers.status & 0x82U) != 0x02U) {
                    std::cerr << "Unsigned-word-multiply partial transcript "
                                 "changed accumulator "
                              << accumulator << ", multiplicand "
                              << multiplicand << ", multiplier "
                              << multiplier_word << ".\n";
                    return EXIT_FAILURE;
                }
                std::uint32_t previous_cycle = 0U;
                for (const auto& write : replacement->work_ram_writes) {
                    if (write.logical_address < 0x2000U
                        || write.logical_address >= 0x4000U
                        || write.master_cycle_offset < previous_cycle) {
                        std::cerr << "Unsigned-word-multiply partial fixture "
                                     "emitted an invalid write.\n";
                        return EXIT_FAILURE;
                    }
                    previous_cycle = write.master_cycle_offset;
                    work_ram[write.logical_address - 0x2000U] =
                        static_cast<std::byte>(write.value);
                }
                const auto result = static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(work_ram[0x0042U])
                    | (static_cast<std::uint16_t>(
                           std::to_integer<std::uint8_t>(work_ram[0x0043U]))
                       << 8U));
                if (result != expected
                    || std::to_integer<std::uint8_t>(work_ram[0x0030U]) != 0U) {
                    std::cerr << "Unsigned-word-multiply partial fixture "
                                 "changed state incorrectly.\n";
                    return EXIT_FAILURE;
                }
                ++verified;
            }
        }
    }

    std::vector<std::byte> guard_work_ram(0x2000U);
    for (const auto status : std::array<std::uint8_t, 2U>{0x08U, 0x20U}) {
        const devils_crush::emulation::RoutineEntry guarded_entry{
            .routine = {.physical_bank = 0x00U, .logical_pc = 0xE5BCU},
            .registers = {.program_counter = 0xE5BCU, .status = status},
        };
        if (devils_crush::app::replace_native_gameplay_routine(
                guarded_entry, guard_work_ram, empty_rom)) {
            std::cerr << "Unsigned-word-multiply partial fixture accepted "
                         "decimal or T mode.\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "Unsigned-word-multiply partial semantic fixture verified: "
              << verified << " operand cases\n";
    return EXIT_SUCCESS;
}

int run_signed_word_multiply_semantic_fixture()
{
    constexpr std::array<std::byte, 0U> empty_rom{};
    constexpr std::array<std::uint16_t, 8U> values{
        0x0000U, 0x0001U, 0x007FU, 0x0080U,
        0x7FFFU, 0x8000U, 0xFF80U, 0xFFFFU,
    };
    constexpr std::array<std::uint8_t, 4U> statuses{
        0x00U, 0x01U, 0x04U, 0x41U,
    };
    std::size_t verified = 0U;
    for (const auto multiplicand : values) {
        for (const auto multiplier : values) {
            std::vector<std::byte> work_ram(0x2000U);
            work_ram[0x002DU] = static_cast<std::byte>(multiplicand & 0xFFU);
            work_ram[0x002EU] = static_cast<std::byte>(multiplicand >> 8U);
            work_ram[0x002FU] = static_cast<std::byte>(multiplier & 0xFFU);
            work_ram[0x0030U] = static_cast<std::byte>(multiplier >> 8U);
            const devils_crush::emulation::RoutineEntry entry{
                .routine = {.physical_bank = 0x00U, .logical_pc = 0xE5ECU},
                .registers = {
                    .program_counter = 0xE5ECU,
                    .accumulator = 0xA5U,
                    .x = 0x5AU,
                    .y = 0xC3U,
                    .stack_pointer = 0xF2U,
                    .status = statuses[verified % statuses.size()],
                },
            };
            const auto replacement =
                devils_crush::app::replace_native_gameplay_routine(
                    entry, work_ram, empty_rom);
            if (!replacement || replacement->return_instruction
                    != devils_crush::emulation::RoutineKey{0x00U, 0xE60AU}
                || replacement->checkpoints.empty()
                || !replacement->absorbed_returns.empty()
                || replacement->output_registers.program_counter != 0xE60AU
                || replacement->output_registers.x != entry.registers.x
                || replacement->output_registers.y != entry.registers.y
                || replacement->output_registers.stack_pointer
                    != entry.registers.stack_pointer) {
                std::cerr << "Signed-word-multiply fixture did not replace "
                             "a valid input.\n";
                return EXIT_FAILURE;
            }
            std::uint32_t previous_cycle = 0U;
            for (const auto& write : replacement->work_ram_writes) {
                if (write.logical_address < 0x2000U
                    || write.logical_address >= 0x4000U
                    || write.master_cycle_offset < previous_cycle) {
                    std::cerr << "Signed-word-multiply fixture emitted an "
                                 "invalid write.\n";
                    return EXIT_FAILURE;
                }
                previous_cycle = write.master_cycle_offset;
                work_ram[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            const auto result = static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(work_ram[0x002FU])
                | (static_cast<std::uint16_t>(
                       std::to_integer<std::uint8_t>(work_ram[0x0030U]))
                   << 8U));
            const auto expected = static_cast<std::uint16_t>(
                static_cast<std::int32_t>(static_cast<std::int16_t>(multiplicand))
                * static_cast<std::int32_t>(static_cast<std::int16_t>(multiplier)));
            if (result != expected) {
                std::cerr << "Signed-word-multiply fixture changed "
                             "state incorrectly.\n";
                return EXIT_FAILURE;
            }
            ++verified;
        }
    }

    std::vector<std::byte> guard_work_ram(0x2000U);
    for (const auto status : std::array<std::uint8_t, 2U>{0x08U, 0x20U}) {
        const devils_crush::emulation::RoutineEntry guarded_entry{
            .routine = {.physical_bank = 0x00U, .logical_pc = 0xE5ECU},
            .registers = {.program_counter = 0xE5ECU, .status = status},
        };
        if (devils_crush::app::replace_native_gameplay_routine(
                guarded_entry, guard_work_ram, empty_rom)) {
            std::cerr << "Signed-word-multiply fixture accepted decimal or "
                         "T mode.\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "Signed-word-multiply semantic fixture verified: "
              << verified << " operand cases\n";
    return EXIT_SUCCESS;
}

int run_signed_word_normalizer_semantic_fixture()
{
    constexpr std::array<std::byte, 0U> empty_rom{};
    constexpr std::array<std::uint16_t, 2U> routines{0xE813U, 0xE825U};
    std::size_t verified = 0U;
    for (const auto routine : routines) {
        const auto low_offset = static_cast<std::size_t>(
            routine == 0xE813U ? 0x002FU : 0x002DU);
        for (std::uint32_t value = 0U; value <= 0xFFFFU; ++value) {
            std::vector<std::byte> work_ram(0x2000U);
            work_ram[low_offset] = static_cast<std::byte>(value & 0xFFU);
            work_ram[low_offset + 1U] = static_cast<std::byte>(value >> 8U);
            const devils_crush::emulation::RoutineEntry entry{
                .routine = {.physical_bank = 0x00U, .logical_pc = routine},
                .registers = {
                    .program_counter = routine,
                    .accumulator = 0xA5U,
                    .x = 0x5AU,
                    .y = 0xC3U,
                    .stack_pointer = 0xF2U,
                    .status = static_cast<std::uint8_t>(
                        (value & 0x03U) == 0U ? 0x00U
                        : (value & 0x03U) == 1U ? 0x01U
                        : (value & 0x03U) == 2U ? 0x40U : 0x41U),
                },
            };
            const auto replacement =
                devils_crush::app::replace_native_gameplay_routine(
                    entry, work_ram, empty_rom);
            const auto negative = (value & 0x8000U) != 0U;
            const auto expected = static_cast<std::uint16_t>(
                negative ? 0U - value : value);
            const auto rts_pc = static_cast<std::uint16_t>(routine + 17U);
            if (!replacement || replacement->return_instruction
                    != devils_crush::emulation::RoutineKey{0x00U, rts_pc}
                || replacement->output_registers.program_counter != rts_pc
                || replacement->output_registers.x != entry.registers.x
                || replacement->output_registers.y != entry.registers.y
                || replacement->output_registers.stack_pointer
                    != entry.registers.stack_pointer
                || replacement->output_registers.accumulator
                    != static_cast<std::uint8_t>(
                        negative ? 0x80U : (value >> 8U))
                || replacement->body_master_cycles
                    != (negative ? 90U : 24U)
                || replacement->checkpoints.size()
                    != (negative ? 10U : 2U)
                || replacement->work_ram_writes.size()
                    != (negative ? 2U : 0U)
                || !replacement->absorbed_returns.empty()) {
                std::cerr << "Signed-word-normalizer fixture did not replace "
                             "a valid input.\n";
                return EXIT_FAILURE;
            }
            for (const auto& write : replacement->work_ram_writes) {
                work_ram[write.logical_address - 0x2000U] =
                    static_cast<std::byte>(write.value);
            }
            const auto normalized = static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(work_ram[low_offset])
                | (static_cast<std::uint16_t>(
                       std::to_integer<std::uint8_t>(work_ram[low_offset + 1U]))
                   << 8U));
            if (normalized != expected) {
                std::cerr << "Signed-word-normalizer fixture changed state "
                             "incorrectly.\n";
                return EXIT_FAILURE;
            }
            ++verified;
        }
    }

    std::vector<std::byte> guard_work_ram(0x2000U);
    for (const auto routine : routines) {
        for (const auto status : std::array<std::uint8_t, 2U>{0x08U, 0x20U}) {
            const devils_crush::emulation::RoutineEntry guarded_entry{
                .routine = {.physical_bank = 0x00U, .logical_pc = routine},
                .registers = {.program_counter = routine, .status = status},
            };
            if (devils_crush::app::replace_native_gameplay_routine(
                    guarded_entry, guard_work_ram, empty_rom)) {
                std::cerr << "Signed-word-normalizer fixture accepted decimal "
                             "or T mode.\n";
                return EXIT_FAILURE;
            }
        }
    }
    std::cout << "Signed-word-normalizer semantic fixture verified: "
              << verified << " operand cases\n";
    return EXIT_SUCCESS;
}

int run_side_gate_deadline_check_semantic_fixture()
{
    constexpr std::array<std::byte, 0U> empty_rom{};
    constexpr std::array<std::uint8_t, 2U> input_statuses{0x00U, 0x42U};
    constexpr std::array<std::uint8_t, 3U> deadlines{0x00U, 0x01U, 0x02U};
    constexpr std::array<std::uint32_t, 5U> clocks{
        0x000000U, 0x001387U, 0x001388U, 0x001389U, 0xFFFFFFU,
    };
    const auto set_nz = [](std::uint8_t status, const std::uint8_t value) {
        status = static_cast<std::uint8_t>(status & ~0x82U);
        if (value == 0U) {
            status = static_cast<std::uint8_t>(status | 0x02U);
        }
        return static_cast<std::uint8_t>(status | (value & 0x80U));
    };
    const auto subtract = [&set_nz](
                              std::uint8_t status,
                              const std::uint8_t left,
                              const std::uint8_t right) {
        const auto difference = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(left) - right
            - ((status & 0x01U) != 0U ? 0U : 1U));
        const auto result = static_cast<std::uint8_t>(difference);
        status = static_cast<std::uint8_t>(status & ~0x41U);
        status = set_nz(status, result);
        if (difference < 0x100U) {
            status = static_cast<std::uint8_t>(status | 0x01U);
        }
        if (((left ^ right) & (left ^ result) & 0x80U) != 0U) {
            status = static_cast<std::uint8_t>(status | 0x40U);
        }
        return std::pair{result, status};
    };

    std::size_t verified = 0U;
    for (const auto input_status : input_statuses) {
        for (const auto deadline : deadlines) {
            for (const auto clock : clocks) {
                std::vector<std::byte> work_ram(0x2000U);
                constexpr std::uint8_t y = 0x07U;
                work_ram[0x0251U] = static_cast<std::byte>(y);
                work_ram[0x0D52U] = static_cast<std::byte>(deadline);
                work_ram[0x0205U + y] = static_cast<std::byte>(clock & 0xFFU);
                work_ram[0x020CU + y] = static_cast<std::byte>(
                    (clock >> 8U) & 0xFFU);
                work_ram[0x0213U + y] = static_cast<std::byte>(
                    (clock >> 16U) & 0xFFU);
                const devils_crush::emulation::RoutineEntry entry{
                    .routine = {.physical_bank = 0x02U, .logical_pc = 0x4FC4U},
                    .registers = {
                        .program_counter = 0x4FC4U,
                        .accumulator = 0xA5U,
                        .x = 0x5AU,
                        .y = 0xC3U,
                        .stack_pointer = 0xF2U,
                        .status = input_status,
                    },
                };
                const auto replacement =
                    devils_crush::app::replace_native_gameplay_routine(
                        entry, work_ram, empty_rom);
                if (!replacement || !replacement->work_ram_writes.empty()
                    || replacement->checkpoints.empty()
                    || !replacement->absorbed_returns.empty()) {
                    std::cerr << "Side-gate deadline fixture did not replace "
                                 "a valid input.\n";
                    return EXIT_FAILURE;
                }

                auto expected_status = input_status;
                auto expected_a = entry.registers.accumulator;
                auto expected_y = entry.registers.y;
                std::uint16_t expected_pc = 0x4FE1U;
                std::uint32_t expected_cycles = 12U;
                std::size_t expected_checkpoints = 1U;
                if ((input_status & 0x02U) != 0U) {
                    expected_a = deadline;
                    expected_status = set_nz(expected_status, expected_a);
                    expected_a = static_cast<std::uint8_t>(expected_a - 1U);
                    expected_status = set_nz(expected_status, expected_a);
                    expected_cycles = 39U;
                    expected_checkpoints = 4U;
                    if (expected_a == 0U) {
                        expected_y = y;
                        expected_status = set_nz(expected_status, expected_y);
                        expected_status = static_cast<std::uint8_t>(
                            expected_status | 0x01U);
                        constexpr std::array<std::uint8_t, 3U> kTargets{
                            0x88U, 0x13U, 0x00U,
                        };
                        std::size_t byte_index = 0U;
                        for (const auto byte : std::array<std::uint8_t, 3U>{
                                 static_cast<std::uint8_t>(clock & 0xFFU),
                                 static_cast<std::uint8_t>((clock >> 8U) & 0xFFU),
                                 static_cast<std::uint8_t>((clock >> 16U) & 0xFFU),
                             }) {
                            const auto [result, status] = subtract(
                                expected_status, byte, kTargets[byte_index]);
                            expected_a = result;
                            expected_status = status;
                            ++byte_index;
                        }
                        expected_pc = (expected_status & 0x01U) == 0U
                            ? 0x4FE2U
                            : 0x4FE1U;
                        expected_cycles = expected_pc == 0x4FE2U ? 129U : 123U;
                        expected_checkpoints = 13U;
                    }
                }
                if (replacement->return_instruction
                        != devils_crush::emulation::RoutineKey{0x02U, expected_pc}
                    || replacement->body_master_cycles != expected_cycles
                    || replacement->checkpoints.size() != expected_checkpoints
                    || replacement->output_registers.program_counter != expected_pc
                    || replacement->output_registers.accumulator != expected_a
                    || replacement->output_registers.x != entry.registers.x
                    || replacement->output_registers.y != expected_y
                    || replacement->output_registers.stack_pointer
                        != entry.registers.stack_pointer
                    || replacement->output_registers.status != expected_status) {
                    std::cerr << "Side-gate deadline fixture produced the "
                                 "wrong transcript: status "
                              << static_cast<unsigned int>(input_status)
                              << ", deadline "
                              << static_cast<unsigned int>(deadline)
                              << ", clock " << clock << ", expected PC "
                              << expected_pc << ", got "
                              << replacement->return_instruction.logical_pc
                              << ", expected cycles " << expected_cycles
                              << ", got "
                              << replacement->body_master_cycles
                              << ", expected status "
                              << static_cast<unsigned int>(expected_status)
                              << ", got "
                              << static_cast<unsigned int>(
                                     replacement->output_registers.status)
                              << ".\n";
                    return EXIT_FAILURE;
                }
                ++verified;
            }
        }
    }

    std::vector<std::byte> guard_work_ram(0x2000U);
    for (const auto status : std::array<std::uint8_t, 2U>{0x08U, 0x20U}) {
        const devils_crush::emulation::RoutineEntry entry{
            .routine = {.physical_bank = 0x02U, .logical_pc = 0x4FC4U},
            .registers = {.program_counter = 0x4FC4U, .status = status},
        };
        if (devils_crush::app::replace_native_gameplay_routine(
                entry, guard_work_ram, empty_rom)) {
            std::cerr << "Side-gate deadline fixture accepted decimal or T "
                         "mode.\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "Side-gate deadline semantic fixture verified: " << verified
              << " branch-and-clock cases\n";
    return EXIT_SUCCESS;
}

} // namespace devils_crush::app::host
