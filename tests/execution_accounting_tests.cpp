// SPDX-License-Identifier: GPL-3.0-or-later
#include "devils_crush/emulation/pce_runtime.hpp"

#include <cstdio>
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
}

int main(int argc, char** argv)
{
    if (argc != 2) { return 2; }
    try {
        exercise(argv[1], false);
        exercise(argv[1], true);
        exercise(argv[1], true, true);
        std::puts("Core/native scheduler ledgers reconcile across reset and state restore.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
