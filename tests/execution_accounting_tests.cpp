// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstdio>
#include <algorithm>
#include <string_view>
#include <stdexcept>

using namespace devils_crush::emulation;

namespace {
void require(bool value, const char* message)
{
    if (!value) { throw std::runtime_error(message); }
}
ExecutionAccounting read(PceRuntime& runtime)
{
    const auto value = runtime.execution_accounting();
    require(value.has_value(), "Accounting unavailable");
    std::uint64_t sum = 0;
    for (const auto cycles : value->cycles) { sum += cycles; }
    require(sum == value->elapsed_master_cycles, "Clock ledger does not reconcile");
    require(value->cycles[ExecutionAccounting::other] == 0, "Unidentified clock movement");
    return *value;
}
void frames(PceRuntime& runtime, int count)
{
    for (int i = 0; i < count; ++i) { (void)runtime.run_frame({}); }
}
void exercise(const char* rom, bool native_scheduler, bool native_entry = false)
{
    auto runtime = create_pce_runtime();
    require(!runtime->execution_accounting(), "Unloaded runtime has an active ledger");
    require(runtime->load_rom(rom).loaded, "ROM load failed");
    if (native_scheduler) {
        require(runtime->set_native_irq(true), "Native IRQ installation failed");
        require(runtime->set_native_scheduler(true), "Native scheduler installation failed");
        runtime->reset();
    }
    if (native_entry) {
        require(runtime->set_native_boundary(true), "Native boundary installation failed");
        runtime->reset();
    }
    require(read(*runtime).elapsed_master_cycles == 0, "New ledger is not empty");
    frames(*runtime, 90);
    const auto before = read(*runtime);
    if (native_entry) {
        require(before.native_interrupt_entries != 0 && before.interrupt_entries == 0,
                "Native IRQ entry retained core work");
        require(before.cycles[ExecutionAccounting::translated] != 0
                    && before.cycles[ExecutionAccounting::interrupt] == 0,
                "Native IRQ entry has incorrect ownership");
    } else {
        require(before.cycles[ExecutionAccounting::translated] == 0, "Unhooked work credited as translated");
    }
    require(before.opcode_dispatches[ExecutionAccounting::fallback] != 0, "Missing real opcode dispatches");
    const auto saved = runtime->serialize();
    frames(*runtime, 12);
    const auto after = read(*runtime);
    const auto expected_ram = runtime->export_work_ram();
    require(runtime->deserialize(saved), "State restore failed");
    require(read(*runtime).elapsed_master_cycles == 0, "State restore did not start a fresh ledger");
    frames(*runtime, 12);
    const auto replay = read(*runtime);
    require(replay.native_interrupt_entries == after.native_interrupt_entries - before.native_interrupt_entries,
            "Restored native entry count differs");
    require(replay.interrupt_entries == after.interrupt_entries - before.interrupt_entries,
            "Restored core entry count differs");
    require(replay.native_control_flows == after.native_control_flows - before.native_control_flows,
            "Restored native control-flow count differs");
    require(replay.native_control_flow_cycles == after.native_control_flow_cycles - before.native_control_flow_cycles,
            "Restored native control-flow cycles differ");
    require(replay.native_load_status_instructions == after.native_load_status_instructions - before.native_load_status_instructions,
            "Restored native load/status count differs");
    require(replay.native_load_status_cycles == after.native_load_status_cycles - before.native_load_status_cycles,
            "Restored native load/status cycles differ");
    require(replay.native_branches == after.native_branches - before.native_branches,
            "Restored native branches count differs");
    require(replay.native_branch_cycles == after.native_branch_cycles - before.native_branch_cycles,
            "Restored native branches cycles differ");
    require(replay.native_stores == after.native_stores - before.native_stores,
            "Restored native stores count differs");
    require(replay.native_store_cycles == after.native_store_cycles - before.native_store_cycles,
            "Restored native stores cycles differ");
    require(replay.native_loads == after.native_loads - before.native_loads,
            "Restored native loads count differs");
    require(replay.native_load_cycles == after.native_load_cycles - before.native_load_cycles,
            "Restored native loads cycles differ");
    require(replay.native_logical_instructions == after.native_logical_instructions - before.native_logical_instructions,
            "Restored native logical operations count differs");
    require(replay.native_logical_cycles == after.native_logical_cycles - before.native_logical_cycles,
            "Restored native logical operations cycles differ");
    require(replay.native_bit_branches == after.native_bit_branches - before.native_bit_branches,
            "Restored native bit-branch count differs");
    require(replay.native_bit_branch_cycles == after.native_bit_branch_cycles - before.native_bit_branch_cycles,
            "Restored native bit-branch cycles differ");
    require(replay.native_block_cycles == after.native_block_cycles - before.native_block_cycles,
            "Restored native transfer cycles differ");
    require(replay.native_block_resumes == after.native_block_resumes - before.native_block_resumes,
            "Restored native transfer resume count differs");
    for (std::size_t mode = 0; mode < replay.native_block_moves.size(); ++mode) {
        require(replay.native_block_moves[mode]
                    == after.native_block_moves[mode] - before.native_block_moves[mode],
                "Restored native transfer start count differs");
    }
    require(runtime->export_work_ram() == expected_ram, "State restore changed replay RAM");
    require(replay.elapsed_master_cycles == after.elapsed_master_cycles - before.elapsed_master_cycles,
            "Restored measurement window differs");
    for (std::size_t owner = 0; owner < replay.cycles.size(); ++owner) {
        require(replay.cycles[owner] == after.cycles[owner] - before.cycles[owner],
                "Restored clock ownership differs");
        require(replay.opcode_dispatches[owner]
                    == after.opcode_dispatches[owner] - before.opcode_dispatches[owner],
                "Restored opcode dispatch count differs");
    }
    runtime->reset();
    require(read(*runtime).elapsed_master_cycles == 0, "Reset did not start a fresh ledger");
    frames(*runtime, 4);
    require(read(*runtime).elapsed_master_cycles != 0, "Reset ledger did not advance");
}

void set_cpu_field(SaveState& state, std::string_view name,
                   std::uint32_t value, unsigned expected_size)
{
    // Pinned Mednafen tagged state format: 32-byte chunk name, LE size,
    // then byte-length name / LE size / data records. Only this test seed is
    // edited; the suspended states below come from real engine execution.
    std::array<std::byte, 32> tag{};
    tag[0] = std::byte{'C'}; tag[1] = std::byte{'P'}; tag[2] = std::byte{'U'};
    auto& bytes = state.core_state;
    auto found = std::search(bytes.begin(), bytes.end(), tag.begin(), tag.end());
    require(found != bytes.end(), "CPU state chunk missing");
    auto offset = static_cast<std::size_t>(found - bytes.begin()) + tag.size();
    const auto word = [&](std::size_t at) {
        require(at + 4 <= bytes.size(), "Truncated state size");
        std::uint32_t result = 0;
        for (unsigned i = 0; i < 4; ++i) {
            result |= std::to_integer<std::uint32_t>(bytes[at + i]) << (i * 8);
        }
        return result;
    };
    const auto end = offset + 4 + word(offset);
    require(end <= bytes.size(), "Truncated CPU chunk");
    offset += 4;
    while (offset < end) {
        const auto length = std::to_integer<unsigned>(bytes[offset++]);
        require(offset + length + 4 <= end, "Truncated state name");
        const std::string_view field(reinterpret_cast<const char*>(bytes.data() + offset), length);
        offset += length;
        const auto size = word(offset);
        offset += 4;
        require(offset + size <= end, "Truncated state field");
        if (field == name) {
            require(size == expected_size, "CPU state field size changed");
            for (unsigned i = 0; i < size; ++i) {
                bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 255);
            }
            return;
        }
        offset += size;
    }
    throw std::runtime_error("CPU state field missing");
}

void suspended_transfers(const char* rom)
{
    auto runtime = create_pce_runtime();
    require(runtime->load_rom(rom).loaded, "ROM load failed");
    require(runtime->set_native_irq(true) && runtime->set_native_scheduler(true)
                && runtime->set_native_boundary(true), "Native models unavailable");
    runtime->reset();
    frames(*runtime, 1);
    auto ram_seed = runtime->export_work_ram();
    // TIN $4000,$3FFE,$0000 (65,536 bytes), then JMP $2207. Slow CPU
    // speed forces suspension across multiple frames. IRQs remain masked.
    const std::array<std::uint8_t, 10> program{0xd3, 0x00, 0x40, 0xfe, 0x3f, 0, 0,
                                               0x4c, 0x07, 0x22};
    for (std::size_t i = 0; i < program.size(); ++i) {
        ram_seed[0x200 + i] = static_cast<std::byte>(program[i]);
    }
    require(runtime->import_work_ram(ram_seed), "Transfer program import failed");
    auto seed = runtime->serialize();
    set_cpu_field(seed, "PC", 0x2200, 2);
    set_cpu_field(seed, "P", 0x04, 1);
    set_cpu_field(seed, "IFlagSample", 0x04, 4);
    set_cpu_field(seed, "speed", 0, 1);
    require(runtime->deserialize(seed), "Transfer seed restore failed");
    frames(*runtime, 1); // Leave the long transfer suspended.
    auto previous = read(*runtime);
    require(runtime->set_native_boundary(false), "Live core handoff failed");
    frames(*runtime, 1);
    auto current = read(*runtime);
    require(current.cycles[ExecutionAccounting::translated]
                == previous.cycles[ExecutionAccounting::translated],
            "Live core resume inherited native ownership");
    require(current.cycles[ExecutionAccounting::fallback]
                > previous.cycles[ExecutionAccounting::fallback],
            "Live core transfer resume did not execute");
    previous = current;
    require(runtime->set_native_boundary(true), "Live native handoff failed");
    frames(*runtime, 1);
    current = read(*runtime);
    require(current.native_block_resumes > previous.native_block_resumes
                && current.native_block_cycles > previous.native_block_cycles,
            "Live native transfer resume did not execute");
    require(runtime->deserialize(seed), "Transfer seed restart failed");
    unsigned checked = 0;
    for (unsigned frame = 0; frame < 16; ++frame) {
        const auto saved = runtime->serialize();
        const auto before = read(*runtime);
        const auto native_frame = runtime->run_frame({});
        const auto after = read(*runtime);
        // A resumed transfer at frame entry proves that the saved CPU was
        // suspended after a byte write, not merely between instructions.
        if (after.native_block_resumes == before.native_block_resumes) { continue; }
        const auto ram = runtime->export_work_ram();
        const std::vector<std::uint32_t> pixels(native_frame.framebuffer.pixels.begin(),
                                                 native_frame.framebuffer.pixels.end());
        require(runtime->set_native_boundary(false) && runtime->deserialize(saved),
                "Core suspended-state restore failed");
        const auto core_frame = runtime->run_frame({});
        require(std::equal(pixels.begin(), pixels.end(), core_frame.framebuffer.pixels.begin(),
                           core_frame.framebuffer.pixels.end()), "Resumed transfer changed pixels");
        require(runtime->export_work_ram() == ram, "Resumed transfer changed RAM");
        const auto core = read(*runtime);
        require(core.elapsed_master_cycles == after.elapsed_master_cycles - before.elapsed_master_cycles,
                "Resumed transfer changed frame timing");
        require(core.cycles[ExecutionAccounting::device]
                    == after.cycles[ExecutionAccounting::device] - before.cycles[ExecutionAccounting::device],
                "Resumed transfer changed device stalls");
        require(core.native_block_cycles == 0, "Original transfer resume was bypassed");
        // The next native frame resumes a state saved by the original engine.
        const auto continuation = runtime->serialize();
        require(runtime->set_native_boundary(true) && runtime->deserialize(continuation),
                "Native continuation restore failed");
        ++checked;
    }
    require(checked != 0, "No frame-spanning block transfer was tested");
    std::printf("%u suspended transfer states replay through both engines.\n", checked);
}
}

int main(int argc, char** argv)
{
    if (argc != 2) { return 2; }
    try {
        exercise(argv[1], false);
        exercise(argv[1], true);
        exercise(argv[1], true, true);
        suspended_transfers(argv[1]);
        std::puts("Core/native scheduler ledgers reconcile across reset and state restore.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
