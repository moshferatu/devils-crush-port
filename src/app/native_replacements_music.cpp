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

// Bank 45 is the music sequencer. It is a separate component from the timer
// sound driver at 00:FC00: the driver streams DDA samples, the sequencer
// decides what they are, programming the PSG register file at $0801-$0809 and
// maintaining the per-voice records in $2E00-$2FFF.
//
// Unlike the driver it is not flat. Its per-frame tick at 45:4015 walks the
// voices and calls sixteen subroutines, so the port was incremental: each
// entry below is a subroutine registered on its own, and the walk inlines any
// nested call it makes.
//
// The tick is now here too, and with it the bank's top level. It was recorded
// as unportable for four slices, on two grounds that both turned out to be
// wrong -- see the block at case 0x4015U below. In short: the body that has to
// fit the timer period is a transcript's, and the tick's body is a loop that
// chunks at 4044 like any other; and the voice loop never reads the driver's
// channel bank bytes, because the bank's only JSR $4142 is at 4138, past the
// loop's closing jump. Since every routine the loop calls is already a case in
// this same switch, composing them is ordinary control flow rather than the
// spliced child transcripts the Sorcerer tick needs.
//
// The earlier slices ported the sample/envelope group -- the routines the
// voice walk calls once per voice per frame, which is where the bank's time
// actually goes:
//
//   45:48B4  sequence-data fetch, the shared command decoder
//   45:4883  pitch/period update           (inlines 48B4)
//   45:48F6  sweep accumulator update      (inlines 48B4)
//   45:4936  pitch-slide stepper
//   45:49A2  vibrato accumulator update
//   45:49B6  voice-latch clear
//   45:49F9  voice commit, which programs $0800/$0804
//   45:4BCC  command descriptor loader      (inlines 4C42)
//   45:4211  the two pending-event gates
//   45:4142  the PSG register programmer, which pushes six voices out to
//            $0800-$0809 and uploads a 32-byte waveform when one changes
//   45:4228  the portamento/sweep stepper, which shares its note-to-period
//            tail at $4705-$4882 with the command interpreter at 45:457A
//
// Together they are 31.7M of the bank's 46.1M master cycles on the census
// route, and the tick's own instructions are a further 13.8M.
//
//   45:4015  the per-frame tick, entered once a frame from outside the bank
//   45:4044  the head of its voice loop, and the chunk head
//   45:409C  where it resumes after 45:4936 when the tick hands the call back
//   45:413B  where it resumes after 45:4142
//
// Five more are not subroutines at all. Each of the bank's SEIs is emulated
// rather than transcribed -- see `closes_interrupt_window` -- and these are
// where the body picks back up on the far side of one:
//
//   45:4154  the channel-release arm       45:47E0  the sample trigger
//   45:4169  each channel of 45:4142       45:485E  its bank-map arm
//   45:49E4  the voice commit
//
// 45:469B is a different internal boundary. The command interpreter reaches
// it after resolving the optional note-duration byte. Most 457A calls are
// deliberately handed back before this point, so admitting the continuation
// lets the already-modelled note commit and real 46D6 return run natively.
// It is not a subroutine and therefore must be proved by the enclosing music
// family trace rather than by a standalone routine observer.
constexpr std::uint8_t kBank = 0x45U;

constexpr std::uint8_t kCarry = 0x01U;
constexpr std::uint8_t kZero = 0x02U;
constexpr std::uint8_t kInterruptDisable = 0x04U;
constexpr std::uint8_t kDecimal = 0x08U;
constexpr std::uint8_t kMemory = 0x20U;
constexpr std::uint8_t kOverflow = 0x40U;
constexpr std::uint8_t kNegative = 0x80U;

// Work-RAM window. Zero page is $2000-$20FF and the sequencer's voice records
// occupy $2E00-$2FFF; the walk keeps a private copy of the whole 8KB so it can
// read back its own stores.
constexpr std::uint16_t kWorkRamBase = 0x2000U;
constexpr std::uint16_t kWorkRamEnd = 0x4000U;

// PSG ports these routines drive directly.
constexpr std::uint16_t kPsgChannelSelect = 0x0800U;
constexpr std::uint16_t kPsgFrequencyLow = 0x0802U;
constexpr std::uint16_t kPsgFrequencyHigh = 0x0803U;
constexpr std::uint16_t kPsgAmplitude = 0x0804U;
constexpr std::uint16_t kPsgBalance = 0x0805U;
constexpr std::uint16_t kPsgWaveform = 0x0806U;
constexpr std::uint16_t kPsgNoise = 0x0807U;
constexpr std::uint16_t kPsgLfoFrequency = 0x0808U;
constexpr std::uint16_t kPsgLfoControl = 0x0809U;
constexpr std::uint16_t kTimerControl = 0x0C01U;

// The timer sound driver's per-channel bank bytes, `$F5,X` for X = 0, 2, 4.
// These are the only bytes in this bank's working set that another thread of
// control writes: the driver sets one when it maps a channel's data bank and
// clears it when the channel falls idle. A transcript is computed once from
// one snapshot, so a walk that reads them is only sound if no timer interrupt
// can be delivered while it runs -- see `reads_driver_state` below.
constexpr std::uint16_t kDriverChannelBankFirst = 0x20F5U;
constexpr std::uint16_t kDriverChannelBankLast = 0x20FAU;

// Where a 45:4015 chunk stops.
//
// The two channel budgets are the lesser reason. The walk below refuses itself
// past 1,800 checkpoints and 480 writes, inside the core's 2,048 and 512, and
// these leave a whole voice iteration of headroom under both.
//
// The reason that actually decides the tick is the interrupt horizon. Every
// voice iteration commits its amplitude through 49D2, which reads the timer
// driver's channel bank byte at $20F5,Y, so `reads_driver_state` is set on
// essentially every transcript that runs the loop even once -- the guard the
// bank has carried since 4142 was ported. A whole call is about 12,667 master
// cycles against a timer period of 5,068, so an unchunked tick transcript is
// refused by that guard on every call, which is exactly the arithmetic the
// docs recorded as proof the routine could not be ported at all. It is proof
// about one transcript. Cutting the walk so that each chunk lands inside the
// horizon it was handed is the whole slice: the walk stops at the loop's
// closing jump once one more iteration would not fit before the next
// serviceable timer interrupt, the core takes the jump and the interrupt, and
// the next chunk starts against a fresh horizon.
//
// One iteration measured 1,137 composed master cycles on this route; the
// margin here is rounded up from that, because the stop has to be decided
// before the iteration runs rather than after.
constexpr std::size_t kChunkCheckpoints = 1200U;
constexpr std::size_t kChunkWrites = 340U;
constexpr std::uint32_t kIterationMasterCycles = 1600U;

// Where a 45:4142 chunk stops.
//
// The same guard refuses this routine for the same reason, and it was left
// refusing half its arrivals on the grounds that a whole call averages 2,055
// master cycles against the same 5,068-cycle timer period. That is again an
// argument about one transcript. 4142 is a loop too -- 4143 is its head, 41F7
// closes it with a JMP $4143, and one channel is 370 composed master cycles on
// average -- so it chunks exactly as the tick does.
//
// The margin has to cover the worst iteration rather than the average, because
// the stop is decided before the iteration runs, and it is worth being exact
// about which one. Iterations here come in two sizes. A plain one is 390 master
// cycles; one whose voice changed waveform runs 41A3 through 41E1 as well,
// uploading 32 bytes through $0806 in an eight-pass loop, and only 186 of the
// route's 19,062 iterations take it.
//
// The longest path from 4143 back to 41F7 is 2,190: the channel-release arm at
// 4151-4165, then the waveform upload, then the commit at 41E3. Not stopping
// costs the JMP as well, and the iteration that finally leaves the loop pays
// for the taken branch at 41F5 and the two LFO ports at 41FA-420D, which is 78
// more. 2,280 is therefore a bound the instruction sequence gives rather than
// one this route happens to satisfy, and it still leaves more than half the
// 5,068-cycle timer period usable; 2,304 rounds it clear.
//
// A tighter margin was measured rather than assumed. At 500 -- enough for the
// plain path only -- chunks run longer and 4143 is entered half as often, but
// an iteration that turns out to want a waveform overruns the horizon and the
// guard below refuses that whole chunk instead of shortening it. That cost 55
// of 4142's calls and was worth +166,605 master cycles, 0.013% of the route,
// for a constant whose worst case is that a tune with heavy waveform switching
// stops being admitted at all. Not taken.
constexpr std::uint32_t kChannelIterationMasterCycles = 2304U;

[[nodiscard]] std::uint8_t set_nz(
    const std::uint8_t status,
    const std::uint8_t value)
{
    auto result = static_cast<std::uint8_t>(
        status & static_cast<std::uint8_t>(~(kZero | kNegative)));
    if (value == 0U) {
        result = static_cast<std::uint8_t>(result | kZero);
    }
    result = static_cast<std::uint8_t>(
        result | static_cast<std::uint8_t>(value & kNegative));
    return result;
}

// The entry points this module owns. Anything else declines before the walk
// starts, so an unrelated routine in the bank is never mistaken for one of
// these.
[[nodiscard]] constexpr bool is_owned_entry(const std::uint16_t logical_pc)
{
    return logical_pc == 0x4015U
        || logical_pc == 0x4044U
        || logical_pc == 0x409CU
        || logical_pc == 0x413BU
        || logical_pc == 0x4142U
        || logical_pc == 0x4143U
        || logical_pc == 0x4154U
        || logical_pc == 0x4169U
        || logical_pc == 0x4211U
        || logical_pc == 0x4228U
        || logical_pc == 0x457AU
        || logical_pc == 0x469BU
        || logical_pc == 0x47E0U
        || logical_pc == 0x482EU
        || logical_pc == 0x485EU
        || logical_pc == 0x4883U
        || logical_pc == 0x48B4U
        || logical_pc == 0x48F6U
        || logical_pc == 0x4936U
        || logical_pc == 0x49A2U
        || logical_pc == 0x49B6U
        || logical_pc == 0x49E4U
        || logical_pc == 0x49F9U
        || logical_pc == 0x4BCCU
        || logical_pc == 0x4C42U;
}

// Every SEI in the ported span. A checkpoint cannot describe one: the core
// takes two different things from its single status byte -- the P that an
// interrupt taken at that boundary pushes, and the IFlagSample that decides
// whether it may be taken there at all -- and SEI wants opposite answers,
// because its LastCycle samples the mask before the flag retires. Publishing
// the open mask lets the interrupt through and then hands the handler an `I`
// the machine had already closed, so the RTI reopens a window the routine had
// just shut; publishing the closed one suppresses an interrupt the hardware
// takes. There is no third byte.
//
// So the walk stops in front of each of these and the core runs the one
// instruction, exactly as the sound driver's epilogue does at 00:FC81. The
// address after each is registered, so the body resumes immediately with the
// mask the machine actually set.
//
// The stop is conditional, and that is what keeps it cheap: the ambiguity only
// exists when the SEI has something to close. Reached with the mask already
// set -- which is most of `4153`, entered from a masked caller -- the deferred
// byte and the true byte are the same byte, and the walk transcribes it.
[[nodiscard]] constexpr bool closes_interrupt_window(const std::uint16_t pc)
{
    return pc == 0x4153U
        || pc == 0x4168U
        || pc == 0x47DFU
        || pc == 0x482DU
        || pc == 0x485DU
        || pc == 0x49E3U;
}

// Every RTS in the ported set. At call depth zero the walk stops in front of
// one of these; deeper than that it executes the RTS outright, because the
// matching JSR was one of its own.
//
// All twenty-six are ordinary returns rather than dispatches, checked against
// the ROM, so a stop can be absorbed -- run as the last instruction of the
// transcript instead of being handed to the core -- and the runtime test is
// that the pointer still stands where the entry left it. That is the whole of
// it, without the pop counts bank 00's wrappers needed: these returns close
// the call their own entry opened.
//
// The ported span holds the other shape twice. 45:4579 and 45:4BE6 each push
// an address and let the RTS jump to it, which is not a return; both are
// deliberately absent from the list, and the pointer test would refuse them in
// any case, standing two below wherever such a walk began.
[[nodiscard]] constexpr bool is_return_instruction(const std::uint16_t pc)
{
    return pc == 0x4141U
        || pc == 0x4210U
        || pc == 0x4227U
        || pc == 0x4243U || pc == 0x4270U || pc == 0x429EU
        || pc == 0x42B2U || pc == 0x42B9U
        || pc == 0x46D6U || pc == 0x4775U || pc == 0x4882U
        || pc == 0x48ACU || pc == 0x48B3U
        || pc == 0x48B8U || pc == 0x48C0U || pc == 0x48CDU || pc == 0x48D6U
        || pc == 0x48E5U || pc == 0x48F5U
        || pc == 0x4935U
        || pc == 0x494BU || pc == 0x495EU || pc == 0x496CU
        || pc == 0x49B5U
        || pc == 0x49D1U
        || pc == 0x49F8U
        || pc == 0x4A28U
        || pc == 0x4A9BU
        || pc == 0x4CB8U;
}

}  // namespace

std::optional<RoutineReplacement> replace_music_sequencer(
    const RoutineEntry& entry,
    const std::span<const std::byte> work_ram,
    const std::span<const std::byte> rom)
{
    if (entry.routine.physical_bank != kBank
        || !is_owned_entry(entry.routine.logical_pc)
        || work_ram.size() < 0x2000U) {
        return std::nullopt;
    }
    // None of these routines is written to run in decimal mode or under the
    // memory-operand T flag; both would change ADC/SBC and the loads.
    if ((entry.registers.status & (kDecimal | kMemory)) != 0U) {
        return std::nullopt;
    }
    // The sequencer reaches its command tables through MPR2, which has to be
    // this bank for the walk's ROM reads to mean anything.
    if (entry.registers.mpr[2] != kBank) {
        return std::nullopt;
    }

    std::uint8_t ram[0x2000];
    for (std::size_t index = 0U; index < 0x2000U; ++index) {
        ram[index] = std::to_integer<std::uint8_t>(work_ram[index]);
    }

    RoutineReplacement replacement{};
    replacement.work_ram_writes.reserve(24U);
    replacement.checkpoints.reserve(96U);

    auto registers = entry.registers;
    std::uint32_t cycle = 0U;
    // Set by any read or write the walk cannot resolve. Checked at the top of
    // every step, so a bad address declines instead of guessing.
    bool overflow = false;
    // JSR/RTS nesting inside the transcript. Zero means the next RTS belongs
    // to the caller and terminates the walk.
    unsigned depth = 0U;
    // Whether a timer interrupt could be delivered as the walk stands. It
    // begins as the entry mask and can only open: a walk stops in front of any
    // SEI that would close it (see `closes_interrupt_window`), so nothing
    // inside a transcript ever masks the machine, while the bank's four CLIs
    // and the PLP at 41F1 all open it.
    //
    // This used to be read from `entry.registers.status` alone, and that was
    // wrong in the direction that matters. The tick is called from inside the
    // VBlank handler, so it is entered with the mask already set -- which the
    // old guard read as "no interrupt can land during this body, so the
    // driver's bytes cannot go stale" and then exempted the whole walk. Bank
    // 45 does not honour that: 4837, 4872, 47EE and 49F2 each end a critical
    // section with an unconditional CLI rather than restoring the caller's
    // mask, so a body entered masked runs most of itself unmasked. The
    // exemption has to follow the machine's mask instruction by instruction,
    // not the one byte it was handed on the way in.
    bool interrupt_window_open =
        (entry.registers.status & kInterruptDisable) == 0U;
    // Where the walk last read one of the timer driver's channel bank bytes
    // *while an interrupt could land*, as a master-cycle offset into the body,
    // and whether it ever did. Those are the only bytes here another thread of
    // control writes, and a transcript reads them from a snapshot taken before
    // it runs, so such a read is only correct if the walk reaches it before the
    // next serviceable timer interrupt. A read taken under the mask needs no
    // such promise: the driver cannot have run since the snapshot.
    //
    // **It is the read's own cycle that has to beat the horizon, not the
    // transcript's length**, and those are very different numbers here. This
    // guard used to compare the finished body against the horizon, so a walk
    // that read the byte at cycle 300 and ran on to 12,000 was refused for
    // something that happened 4,700 cycles after the read it was worried
    // about. What the driver can invalidate is a read, and a read is over at
    // the boundary of the instruction that made it.
    //
    // The *last* such read is the one to keep, not the first. Once the body
    // crosses the horizon the core may deliver the interrupt at a checkpoint
    // and the driver may write these bytes; a read after that point is stale
    // however early the first one was.
    //
    // Priced exactly the way a store is, and for the same reason: the offset is
    // not known until the instruction's cycle count is, so the read is recorded
    // here and stamped by the next `append_checkpoint`.
    bool reads_driver_state = false;
    bool driver_state_read_pending = false;
    std::uint32_t driver_state_read_cycle = 0U;

    const auto read_ram =
        [&ram,
         &overflow,
         &reads_driver_state,
         &driver_state_read_pending,
         &interrupt_window_open](const std::uint16_t address) -> std::uint8_t {
            if (address < kWorkRamBase || address >= kWorkRamEnd) {
                overflow = true;
                return 0U;
            }
            if (interrupt_window_open
                && address >= kDriverChannelBankFirst
                && address <= kDriverChannelBankLast) {
                reads_driver_state = true;
                driver_state_read_pending = true;
            }
            return ram[address - kWorkRamBase];
        };

    // Reads the byte a logical address currently maps to. The command tables
    // at $5B6E/$5C2F/$5D81 live in this bank, reached through MPR2, but the
    // pointer the tables install is data-driven, so the mapping is resolved
    // generally rather than assumed.
    const auto read_mapped =
        [&registers, &ram, rom, &overflow](
            const std::uint16_t address) -> std::uint8_t {
            const auto bank = registers.mpr[address >> 13U];
            if (bank == 0xF8U) {
                return ram[address & 0x1FFFU];
            }
            if (bank >= 0x80U || rom.size() != 0x60000U) {
                overflow = true;
                return 0U;
            }
            const auto rom_bank = bank < 0x40U
                ? static_cast<std::size_t>(bank & 0x1FU)
                : static_cast<std::size_t>((bank & 0x0FU) + 0x20U);
            const auto offset = rom_bank * 0x2000U
                + static_cast<std::size_t>(address & 0x1FFFU);
            if (offset >= rom.size()) {
                overflow = true;
                return 0U;
            }
            return std::to_integer<std::uint8_t>(rom[offset]);
        };

    // Stores land on their instruction's final cycle, so a write shares the
    // offset and the instruction of its checkpoint. They are recorded here
    // and priced by the next append_checkpoint, once the instruction's cycle
    // count is known.
    std::size_t committed_writes = 0U;
    // Prices every write recorded since the last commit at a given cycle.
    // Almost every store lands on its instruction's final cycle, which is
    // what append_checkpoint does. JSR is the exception: the core pushes the
    // return address *before* `ADDCYC(6)`, so its two stack writes land on
    // the instruction's first cycle and must be committed explicitly.
    const auto commit_pending_writes =
        [&replacement, &committed_writes](const std::uint32_t at_cycle) {
            while (committed_writes < replacement.work_ram_writes.size()) {
                replacement.work_ram_writes[committed_writes]
                    .master_cycle_offset = at_cycle;
                ++committed_writes;
            }
        };
    // SEI and CLI both apply the flag after the core has already sampled the
    // interrupt for that instruction, so the checkpoint at a SEI still
    // publishes the old mask and the change lands on the following one.
    bool status_change_pending = false;
    std::uint8_t pending_status = 0U;

    const auto append_checkpoint =
        [&replacement,
         &registers,
         &cycle,
         &commit_pending_writes,
         &status_change_pending,
         &pending_status,
         &driver_state_read_pending,
         &driver_state_read_cycle,
         &interrupt_window_open](
            const std::uint16_t instruction_pc,
            const std::uint16_t resume_pc,
            const std::uint32_t instruction_cycles) {
            cycle += instruction_cycles;
            commit_pending_writes(cycle);
            // A driver-state read is over once its instruction is, which is
            // the first boundary at which the core could deliver the interrupt
            // that lets the driver rewrite the byte.
            if (driver_state_read_pending) {
                driver_state_read_cycle = cycle;
                driver_state_read_pending = false;
            }
            registers.program_counter = resume_pc;
            replacement.checkpoints.push_back({
                .interrupt_sample_master_cycle_offset = cycle - 3U,
                .master_cycle_offset = cycle,
                .instruction = {kBank, instruction_pc},
                .registers = registers,
            });
            if (status_change_pending) {
                registers.status = pending_status;
                status_change_pending = false;
                // Every deferred mask change lands here, which makes this the
                // one place that has to notice the window opening -- the four
                // CLIs and the PLP alike, at exactly the boundary the core
                // will first sample with the new mask.
                if ((registers.status & kInterruptDisable) == 0U) {
                    interrupt_window_open = true;
                }
            }
        };

    const auto store =
        [&replacement, &ram](
            const std::uint16_t instruction_pc,
            const std::uint16_t address,
            const std::uint8_t value) {
            if (address >= kWorkRamBase && address < kWorkRamEnd) {
                ram[address - kWorkRamBase] = value;
            }
            replacement.work_ram_writes.push_back({
                .instruction = {kBank, instruction_pc},
                .logical_address = address,
                .value = value,
                .master_cycle_offset = 0U,
            });
        };

    const auto push =
        [&registers, &store](
            const std::uint16_t instruction_pc,
            const std::uint8_t value) {
            store(
                instruction_pc,
                static_cast<std::uint16_t>(
                    0x2100U + registers.stack_pointer),
                value);
            registers.stack_pointer = static_cast<std::uint8_t>(
                registers.stack_pointer - 1U);
        };

    const auto pull = [&registers, &ram]() -> std::uint8_t {
        registers.stack_pointer = static_cast<std::uint8_t>(
            registers.stack_pointer + 1U);
        return ram[0x0100U + registers.stack_pointer];
    };

    // PLP. The core is `ADDCYC(3); LastCycle(); P = POP();`, so it samples
    // the interrupt *before* the pop lands and the mask this instruction
    // publishes has to be the caller's. Every other bit is different: an
    // interrupt taken here pushes P after the pop, so the checkpoint has to
    // carry the popped flags already. Only the mask is deferred a step.
    const auto pull_status =
        [&registers, &pull, &pending_status, &status_change_pending]() {
            const auto popped = pull();
            registers.status = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(
                    registers.status & kInterruptDisable)
                | static_cast<std::uint8_t>(
                    popped & static_cast<std::uint8_t>(~kInterruptDisable)));
            pending_status = popped;
            status_change_pending = true;
        };

    // Zero page lives at $2000; an absolute address is used as written.
    const auto zp = [](const std::uint8_t address) {
        return static_cast<std::uint16_t>(kWorkRamBase + address);
    };
    // Zero-page indexed wraps inside the page: $2000 | ((base + X) & $FF).
    const auto zp_indexed_x = [&registers](const std::uint8_t base) {
        return static_cast<std::uint16_t>(
            kWorkRamBase
            | static_cast<std::uint8_t>(base + registers.x));
    };
    const auto indexed_x = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.x));
    };
    const auto indexed_y = [&registers](const std::uint16_t base) {
        return static_cast<std::uint16_t>(
            base + static_cast<std::uint16_t>(registers.y));
    };
    // ($D2),Y: the 16-bit command pointer the tables install, plus Y. The
    // core adds Y without wrapping, so a sum past $FFFF would index outside
    // the logical address space; that declines rather than guessing.
    const auto command_pointer = [&ram, &registers, &overflow]() {
        const auto base = static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(ram[0x00D2U])
            | static_cast<std::uint16_t>(ram[0x00D3U]) << 8U);
        const auto target = base + static_cast<std::uint32_t>(registers.y);
        if (target > 0xFFFFU) {
            overflow = true;
            return static_cast<std::uint16_t>(0U);
        }
        return static_cast<std::uint16_t>(target);
    };

    // ($D0),Y: the sequence cursor the command interpreter reads its stream
    // through. Separate from the ($D2) scratch pointer above, and indexed the
    // same way, so the same out-of-range check applies.
    const auto sequence_pointer = [&ram, &registers, &overflow]() {
        const auto base = static_cast<std::uint32_t>(
            static_cast<std::uint16_t>(ram[0x00D0U])
            | static_cast<std::uint16_t>(ram[0x00D1U]) << 8U);
        const auto target = base + static_cast<std::uint32_t>(registers.y);
        if (target > 0xFFFFU) {
            overflow = true;
            return static_cast<std::uint16_t>(0U);
        }
        return static_cast<std::uint16_t>(target);
    };

    // ($D2): the same pointer without an index. Used once, by the sample
    // trigger at 47B5, to read the header byte the pointer lands on.
    const auto indirect_pointer = [&ram]() {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(ram[0x00D2U])
            | static_cast<std::uint16_t>(ram[0x00D3U]) << 8U);
    };

    const auto load_accumulator = [&registers](const std::uint8_t value) {
        registers.accumulator = value;
        registers.status = set_nz(registers.status, value);
    };
    const auto compare =
        [&registers](const std::uint8_t lhs, const std::uint8_t rhs) {
            const auto difference = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(lhs)
                - static_cast<std::uint16_t>(rhs));
            registers.status = set_nz(
                registers.status,
                static_cast<std::uint8_t>(difference));
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            if (lhs >= rhs) {
                registers.status = static_cast<std::uint8_t>(
                    registers.status | kCarry);
            }
        };
    // BIT against memory: Z from A & operand, N and V straight off the
    // operand's top two bits. The immediate form, which leaves N and V
    // alone, is written out inline where it occurs.
    const auto bit_test = [&registers](const std::uint8_t operand) {
        auto status = static_cast<std::uint8_t>(
            registers.status
            & static_cast<std::uint8_t>(~(kZero | kOverflow | kNegative)));
        if ((registers.accumulator & operand) == 0U) {
            status = static_cast<std::uint8_t>(status | kZero);
        }
        registers.status = static_cast<std::uint8_t>(
            status
            | static_cast<std::uint8_t>(operand & (kOverflow | kNegative)));
    };

    const auto add_with_carry = [&registers](const std::uint8_t operand) {
        const auto sum = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            + static_cast<std::uint16_t>(operand)
            + static_cast<std::uint16_t>(registers.status & kCarry));
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(0x43U | kNegative)));
        const auto overflow_bit = static_cast<std::uint8_t>(
            ((((registers.accumulator ^ operand) & 0x80U) ^ 0x80U)
             & ((registers.accumulator ^ sum) & 0x80U))
            >> 1U);
        status = static_cast<std::uint8_t>(status | overflow_bit);
        status = static_cast<std::uint8_t>(
            status | static_cast<std::uint8_t>((sum >> 8U) & kCarry));
        registers.accumulator = static_cast<std::uint8_t>(sum);
        registers.status = set_nz(status, registers.accumulator);
    };
    const auto subtract_with_carry = [&registers](const std::uint8_t operand) {
        const auto difference = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(registers.accumulator)
            - static_cast<std::uint16_t>(operand)
            - static_cast<std::uint16_t>((registers.status & kCarry) ^ kCarry));
        auto status = static_cast<std::uint8_t>(
            registers.status & static_cast<std::uint8_t>(~(0x43U | kNegative)));
        const auto overflow_bit = static_cast<std::uint8_t>(
            ((registers.accumulator ^ difference)
             & (registers.accumulator ^ operand) & 0x80U)
            >> 1U);
        status = static_cast<std::uint8_t>(status | overflow_bit);
        status = static_cast<std::uint8_t>(
            status
            | static_cast<std::uint8_t>(
                (static_cast<std::uint8_t>((difference >> 8U) & kCarry))
                ^ kCarry));
        registers.accumulator = static_cast<std::uint8_t>(difference);
        registers.status = set_nz(status, registers.accumulator);
    };

    const auto branch =
        [&append_checkpoint](
            const std::uint16_t instruction_pc,
            const std::uint16_t taken_pc,
            const std::uint16_t fallthrough_pc,
            const bool taken) {
            const auto next = taken ? taken_pc : fallthrough_pc;
            append_checkpoint(instruction_pc, next, taken ? 12U : 6U);
            return next;
        };

    auto pc = entry.routine.logical_pc;
    for (;;) {
        if (overflow
            || replacement.checkpoints.size() > 1800U
            || replacement.work_ram_writes.size() > 480U) {
            return std::nullopt;
        }

        // The tick's two extra stops, both at the outermost level, are calls
        // this module does not run: 4936 at 4099, which is not transcribed
        // here at all, and 4142 at 4138, which is transcribed but deliberately
        // handed back so that it is entered on its own and keeps the
        // driver-state guard it already carries rather than imposing it on the
        // whole tick.
        //
        // The other two are chunk boundaries -- the JMP $4044 that closes the
        // tick's voice loop and the JMP $4143 that closes 4142's channel loop
        // -- taken by the core once a chunk has filled either channel far
        // enough that one more iteration might not fit, or once the horizon it
        // was handed no longer has an iteration left in it. One iteration is
        // reached on every pass of either loop, so neither needs lookahead.
        //
        // Both bound the chunk whenever an interrupt could land, rather than
        // only once the walk has already read the driver's bytes. Waiting for
        // the read is too late by construction: the flag is tested at the top
        // of an iteration and set part-way through the next one, so a chunk
        // that reads driver state late runs past the horizon before any stop
        // is ever considered, and the guard below then refuses the whole
        // thing. Bounding every chunk instead costs the walks that would never
        // have needed it one extra boundary of twelve master cycles, and it is
        // what makes the guard unreachable at these two addresses.

        // An SEI with a window still open cannot be described by a checkpoint,
        // so it is handed to the core whatever the call depth: a transcript
        // that ends inside a nested call is merely less efficient, while one
        // that guesses this instruction's status byte is wrong.
        const auto mask_stop = closes_interrupt_window(pc)
            && (registers.status & kInterruptDisable) == 0U;

        const auto tick_stop = pc == 0x4099U || pc == 0x4138U
            || (pc == 0x40FFU
                && (replacement.checkpoints.size() >= kChunkCheckpoints
                    || replacement.work_ram_writes.size() >= kChunkWrites
                    || (interrupt_window_open
                        && cycle + kIterationMasterCycles
                            >= entry.master_cycles_until_timer_interrupt)))
            || (pc == 0x41F7U
                && (replacement.checkpoints.size() >= kChunkCheckpoints
                    || replacement.work_ram_writes.size() >= kChunkWrites
                    || (interrupt_window_open
                        && cycle + kChannelIterationMasterCycles
                            >= entry.master_cycles_until_timer_interrupt)));

        // A stop at the outermost level ends the transcript in front of the
        // instruction standing there. A tick stop is a handover and stays that
        // way; a return is absorbed if the stack agrees, and only then does the
        // core resume above this bank instead of inside it.
        if (mask_stop
            || (depth == 0U && (is_return_instruction(pc) || tick_stop))) {
            // The driver-state guard. A read taken while the machine was
            // masked needs nothing: the driver cannot have run between the
            // snapshot and the read, so `reads_driver_state` was never set for
            // it. An unmasked one has to be reached before the next
            // serviceable timer interrupt, or the value may already be stale
            // by the time the walk uses it -- and it is the read that has to
            // be reached in time, not the transcript that has to end in time.
            if (reads_driver_state
                && driver_state_read_cycle
                    >= entry.master_cycles_until_timer_interrupt) {
                return std::nullopt;
            }
            registers.program_counter = pc;
            replacement.return_instruction = {kBank, pc};
            replacement.output_registers = registers;
            replacement.body_master_cycles = cycle;
            // The absorbed RTS reads only the stack, so the driver-state guard
            // above still covers the whole body; what the twenty-one extra
            // cycles can cross is an interrupt horizon. A chunked transcript
            // already carries checkpoints and gets one more for the return, so
            // the host can stop on it. An atomic one cannot be stopped at all
            // and has to be shown to land clear of every horizon first.
            if (is_return_instruction(pc)
                && registers.stack_pointer == entry.registers.stack_pointer
                && (!replacement.checkpoints.empty()
                    || absorbed_return_fits_atomically(entry, cycle))) {
                absorb_return(
                    replacement,
                    work_ram,
                    registers.stack_pointer,
                    {kBank, pc},
                    cycle,
                    registers);
            }
            return replacement;
        }

        switch (pc) {
        // ---- 45:4015  the per-frame tick --------------------------------
        // The sequencer's top level, called once a frame from outside this
        // bank. It marks itself in progress in $D6, runs the pending-event
        // gate at 4211 and the tempo divider at 49B6, walks the nine voice
        // slots from $2E34 = 8 downwards, then steps the fade at 410B and
        // programs the PSG file through 4142.
        //
        // 4044 is the head of the voice loop and the chunk head. This routine
        // was recorded as unportable for four slices on the arithmetic that
        // its 12,667-cycle body cannot finish inside the 5,068-cycle timer
        // period. That is true of the routine and says nothing about a
        // transcript: the body is a loop, 40FF closes it with a JMP $4044,
        // and one iteration is 1,137 composed master cycles.
        //
        // The other half of that rejection was simply wrong, and it is the
        // half that mattered. The voice loop does not read the driver's
        // channel bank bytes: the only JSR $4142 in the bank is at 4138,
        // *past* the loop's closing jump, and 4148's executions are three per
        // call of 4142 rather than three per iteration. The walk stops in
        // front of 4138 and leaves 4142 to be entered on its own with the
        // guard it already carries, so no chunk here reads driver state
        // except through the sample trigger inside 457A and 4228, which carry
        // that guard themselves.
        //
        // Every other call in the loop is composed, which is free here in a
        // way it was not for the Sorcerer tick: the callees are cases in this
        // same switch, so a JSR is a push and a jump rather than a spliced
        // child transcript.
        case 0x4015U:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x4015U, 0x4017U, 12U);
            pc = 0x4017U;
            break;

        // The core runs the immediate form of BIT through the same macro as
        // the memory forms -- `case 0x89: LD_IM(BIT)` -- so it clears N and V
        // and takes them from the operand rather than moving Z alone the way a
        // 65C02 does. All three of the tick's immediate BITs have a zero in
        // both of those bits, so each one clears N and V as well as setting Z.
        case 0x4017U:  // BIT #$04
            bit_test(0x04U);
            append_checkpoint(0x4017U, 0x4019U, 6U);
            pc = 0x4019U;
            break;

        // Taken on every call on this route: the fall-through at 401B is the
        // already-in-progress arm and has never executed.
        case 0x4019U:  // BEQ $401C
            pc = branch(
                0x4019U, 0x401CU, 0x401BU,
                (registers.status & kZero) != 0U);
            break;

        case 0x401CU:  // ORA #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x04U));
            append_checkpoint(0x401CU, 0x401EU, 6U);
            pc = 0x401EU;
            break;

        case 0x401EU:  // STA $D6
            store(0x401EU, zp(0xD6U), registers.accumulator);
            append_checkpoint(0x401EU, 0x4020U, 12U);
            pc = 0x4020U;
            break;

        case 0x4020U:  // BIT #$01
            bit_test(0x01U);
            append_checkpoint(0x4020U, 0x4022U, 6U);
            pc = 0x4022U;
            break;

        case 0x4022U:  // BEQ $402F
            pc = branch(
                0x4022U, 0x402FU, 0x4024U,
                (registers.status & kZero) != 0U);
            break;

        case 0x402FU:  // JSR $4211 -- the pending-event gate
            push(0x402FU, 0x40U);
            push(0x402FU, 0x31U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x402FU, 0x4211U, 21U);
            pc = 0x4211U;
            break;

        case 0x4032U:  // JSR $49B6 -- the tempo divider
            push(0x4032U, 0x40U);
            push(0x4032U, 0x34U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4032U, 0x49B6U, 21U);
            pc = 0x49B6U;
            break;

        case 0x4035U:  // LDY #$08 -- nine voice slots, 8 down to 0
            registers.y = 0x08U;
            registers.status = set_nz(registers.status, 0x08U);
            append_checkpoint(0x4035U, 0x4037U, 6U);
            pc = 0x4037U;
            break;

        case 0x4037U:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x4037U, 0x4039U, 12U);
            pc = 0x4039U;
            break;

        case 0x4039U:  // AND #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x02U));
            append_checkpoint(0x4039U, 0x403BU, 6U);
            pc = 0x403BU;
            break;

        case 0x403BU:  // ORA $2E4F
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(0x2E4FU)));
            append_checkpoint(0x403BU, 0x403EU, 15U);
            pc = 0x403EU;
            break;

        case 0x403EU:  // BEQ $4041
            pc = branch(
                0x403EU, 0x4041U, 0x4040U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4041U:  // STY $2E34 -- seed the slot cursor
            store(0x4041U, 0x2E34U, registers.y);
            append_checkpoint(0x4041U, 0x4044U, 15U);
            pc = 0x4044U;
            break;

        // ---- 45:4044  the voice loop, and the chunk head ----------------
        case 0x4044U: {  // LDX $2E34
            const auto value = read_ram(0x2E34U);
            registers.x = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4044U, 0x4047U, 15U);
            pc = 0x4047U;
            break;
        }

        case 0x4047U:  // LDA $2E46,X -- the slot's state byte
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4047U, 0x404AU, 15U);
            pc = 0x404AU;
            break;

        case 0x404AU:  // BNE $404F
            pc = branch(
                0x404AU, 0x404FU, 0x404CU,
                (registers.status & kZero) == 0U);
            break;

        case 0x404CU:  // JMP $40EE -- an idle slot skips to the cursor step
            append_checkpoint(0x404CU, 0x40EEU, 12U);
            pc = 0x40EEU;
            break;

        case 0x404FU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x404FU, 0x4050U, 6U);
            pc = 0x4050U;
            break;

        case 0x4050U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x4050U, 0x4053U, 15U);
            pc = 0x4053U;
            break;

        case 0x4053U:  // AND #$7F -- drop the retrigger flag
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU));
            append_checkpoint(0x4053U, 0x4055U, 6U);
            pc = 0x4055U;
            break;

        case 0x4055U:  // STA $2EBE,X
            store(0x4055U, indexed_x(0x2EBEU), registers.accumulator);
            append_checkpoint(0x4055U, 0x4058U, 15U);
            pc = 0x4058U;
            break;

        case 0x4058U:  // LDA $2E78,X -- the gate-length step
            load_accumulator(read_ram(indexed_x(0x2E78U)));
            append_checkpoint(0x4058U, 0x405BU, 15U);
            pc = 0x405BU;
            break;

        case 0x405BU:  // BEQ $4066
            pc = branch(
                0x405BU, 0x4066U, 0x405DU,
                (registers.status & kZero) != 0U);
            break;

        case 0x405DU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x405DU, 0x405EU, 6U);
            pc = 0x405EU;
            break;

        case 0x405EU:  // ADC $2EDC,X
            add_with_carry(read_ram(indexed_x(0x2EDCU)));
            append_checkpoint(0x405EU, 0x4061U, 15U);
            pc = 0x4061U;
            break;

        case 0x4061U:  // STA $2EDC,X
            store(0x4061U, indexed_x(0x2EDCU), registers.accumulator);
            append_checkpoint(0x4061U, 0x4064U, 15U);
            pc = 0x4064U;
            break;

        // No carry out of the gate accumulator means the note is still
        // sounding, so the release test below is skipped entirely.
        case 0x4064U:  // BCC $408F
            pc = branch(
                0x4064U, 0x408FU, 0x4066U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4066U:  // LDA $2EB4,X -- the voice's mode byte
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4066U, 0x4069U, 15U);
            pc = 0x4069U;
            break;

        case 0x4069U:  // AND #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x04U));
            append_checkpoint(0x4069U, 0x406BU, 6U);
            pc = 0x406BU;
            break;

        case 0x406BU:  // BEQ $4081
            pc = branch(
                0x406BU, 0x4081U, 0x406DU,
                (registers.status & kZero) != 0U);
            break;

        case 0x406DU:  // LDA $2F18,X
            load_accumulator(read_ram(indexed_x(0x2F18U)));
            append_checkpoint(0x406DU, 0x4070U, 15U);
            pc = 0x4070U;
            break;

        // Taken on every execution on this route; the fall-through at 4072 is
        // a path the census has never produced and is left to assembly.
        case 0x4070U:  // BEQ $4081
            pc = branch(
                0x4070U, 0x4081U, 0x4072U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4081U: {  // INC $2EC8,X -- the note-length counter
            const auto address = indexed_x(0x2EC8U);
            const auto value =
                static_cast<std::uint8_t>(read_ram(address) + 1U);
            store(0x4081U, address, value);
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4081U, 0x4084U, 21U);
            pc = 0x4084U;
            break;
        }

        case 0x4084U:  // LDA $2EC8,X
            load_accumulator(read_ram(indexed_x(0x2EC8U)));
            append_checkpoint(0x4084U, 0x4087U, 15U);
            pc = 0x4087U;
            break;

        case 0x4087U:  // CMP $2ED2,X -- reached the note's length?
            compare(registers.accumulator, read_ram(indexed_x(0x2ED2U)));
            append_checkpoint(0x4087U, 0x408AU, 15U);
            pc = 0x408AU;
            break;

        case 0x408AU:  // BNE $408F
            pc = branch(
                0x408AU, 0x408FU, 0x408CU,
                (registers.status & kZero) == 0U);
            break;

        case 0x408CU:  // JSR $457A -- pull the next command
            push(0x408CU, 0x40U);
            push(0x408CU, 0x8EU);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x408CU, 0x457AU, 21U);
            pc = 0x457AU;
            break;

        case 0x408FU:  // JSR $4883 -- the pitch update
            push(0x408FU, 0x40U);
            push(0x408FU, 0x91U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x408FU, 0x4883U, 21U);
            pc = 0x4883U;
            break;

        case 0x4092U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x4092U, 0x4095U, 15U);
            pc = 0x4095U;
            break;

        case 0x4095U:  // AND #$01
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            append_checkpoint(0x4095U, 0x4097U, 6U);
            pc = 0x4097U;
            break;

        case 0x4097U:  // BEQ $409C
            pc = branch(
                0x4097U, 0x409CU, 0x4099U,
                (registers.status & kZero) != 0U);
            break;

        case 0x409CU:  // JSR $48F6 -- the sweep update
            push(0x409CU, 0x40U);
            push(0x409CU, 0x9EU);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x409CU, 0x48F6U, 21U);
            pc = 0x48F6U;
            break;

        case 0x409FU:  // JSR $49A2 -- the vibrato update
            push(0x409FU, 0x40U);
            push(0x409FU, 0xA1U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x409FU, 0x49A2U, 21U);
            pc = 0x49A2U;
            break;

        case 0x40A2U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x40A2U, 0x40A5U, 15U);
            pc = 0x40A5U;
            break;

        case 0x40A5U:  // BPL $40AA
            pc = branch(
                0x40A5U, 0x40AAU, 0x40A7U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x40A7U:  // JSR $4228 -- the portamento stepper
            push(0x40A7U, 0x40U);
            push(0x40A7U, 0xA9U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x40A7U, 0x4228U, 21U);
            pc = 0x4228U;
            break;

        case 0x40AAU:  // JSR $49F9 -- commit the voice to the register file
            push(0x40AAU, 0x40U);
            push(0x40AAU, 0xACU);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x40AAU, 0x49F9U, 21U);
            pc = 0x49F9U;
            break;

        case 0x40ADU:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x40ADU, 0x40AFU, 12U);
            pc = 0x40AFU;
            break;

        case 0x40AFU:  // AND #$3F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x3FU));
            append_checkpoint(0x40AFU, 0x40B1U, 6U);
            pc = 0x40B1U;
            break;

        case 0x40B1U:  // STA $D6
            store(0x40B1U, zp(0xD6U), registers.accumulator);
            append_checkpoint(0x40B1U, 0x40B3U, 12U);
            pc = 0x40B3U;
            break;

        case 0x40B3U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x40B3U, 0x40B6U, 15U);
            pc = 0x40B6U;
            break;

        case 0x40B6U:  // AND #$20 -- is this voice on a timed release?
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x20U));
            append_checkpoint(0x40B6U, 0x40B8U, 6U);
            pc = 0x40B8U;
            break;

        case 0x40B8U:  // BEQ $40EE
            pc = branch(
                0x40B8U, 0x40EEU, 0x40BAU,
                (registers.status & kZero) != 0U);
            break;

        case 0x40BAU:  // LDA $2F36,X -- the release step
            load_accumulator(read_ram(indexed_x(0x2F36U)));
            append_checkpoint(0x40BAU, 0x40BDU, 15U);
            pc = 0x40BDU;
            break;

        case 0x40BDU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x40BDU, 0x40BEU, 6U);
            pc = 0x40BEU;
            break;

        case 0x40BEU:  // ADC $2F40,X
            add_with_carry(read_ram(indexed_x(0x2F40U)));
            append_checkpoint(0x40BEU, 0x40C1U, 15U);
            pc = 0x40C1U;
            break;

        case 0x40C1U:  // STA $2F40,X
            store(0x40C1U, indexed_x(0x2F40U), registers.accumulator);
            append_checkpoint(0x40C1U, 0x40C4U, 15U);
            pc = 0x40C4U;
            break;

        case 0x40C4U:  // BCC $40EE
            pc = branch(
                0x40C4U, 0x40EEU, 0x40C6U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x40C6U:  // LDA $2E50,X -- the release cursor
            load_accumulator(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x40C6U, 0x40C9U, 15U);
            pc = 0x40C9U;
            break;

        case 0x40C9U:  // CMP $2F4A,X
            compare(registers.accumulator, read_ram(indexed_x(0x2F4AU)));
            append_checkpoint(0x40C9U, 0x40CCU, 15U);
            pc = 0x40CCU;
            break;

        case 0x40CCU:  // BNE $40D8
            pc = branch(
                0x40CCU, 0x40D8U, 0x40CEU,
                (registers.status & kZero) == 0U);
            break;

        case 0x40CEU:  // LDA $2EB4,X -- the cursor has arrived; latch it
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x40CEU, 0x40D1U, 15U);
            pc = 0x40D1U;
            break;

        case 0x40D1U:  // ORA #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x02U));
            append_checkpoint(0x40D1U, 0x40D3U, 6U);
            pc = 0x40D3U;
            break;

        case 0x40D3U:  // STA $2EB4,X
            store(0x40D3U, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x40D3U, 0x40D6U, 15U);
            pc = 0x40D6U;
            break;

        // The ORA above always leaves bit 1 set, so this is an unconditional
        // jump written as a branch; the fall-through at 40D8 is unreachable
        // from here and is only entered through the BNE at 40CC.
        case 0x40D6U:  // BNE $40EE
            pc = branch(
                0x40D6U, 0x40EEU, 0x40D8U,
                (registers.status & kZero) == 0U);
            break;

        case 0x40D8U: {  // LDY $2E50,X
            const auto value = read_ram(indexed_x(0x2E50U));
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x40D8U, 0x40DBU, 15U);
            pc = 0x40DBU;
            break;
        }

        case 0x40DBU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x40DBU, 0x40DEU, 15U);
            pc = 0x40DEU;
            break;

        case 0x40DEU:  // AND #$FD -- clear the arrival latch
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xFDU));
            append_checkpoint(0x40DEU, 0x40E0U, 6U);
            pc = 0x40E0U;
            break;

        case 0x40E0U:  // STA $2EB4,X
            store(0x40E0U, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x40E0U, 0x40E3U, 15U);
            pc = 0x40E3U;
            break;

        case 0x40E3U:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x40E3U, 0x40E4U, 6U);
            pc = 0x40E4U;
            break;

        // Tests bit 4 of the mode byte still in A, not of Y: the two INYs
        // below step the cursor by two instead of one for a looped release.
        case 0x40E4U:  // AND #$10
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x10U));
            append_checkpoint(0x40E4U, 0x40E6U, 6U);
            pc = 0x40E6U;
            break;

        case 0x40E6U:  // BNE $40EA
            pc = branch(
                0x40E6U, 0x40EAU, 0x40E8U,
                (registers.status & kZero) == 0U);
            break;

        case 0x40E8U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x40E8U, 0x40E9U, 6U);
            pc = 0x40E9U;
            break;

        case 0x40E9U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x40E9U, 0x40EAU, 6U);
            pc = 0x40EAU;
            break;

        case 0x40EAU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x40EAU, 0x40EBU, 6U);
            pc = 0x40EBU;
            break;

        case 0x40EBU:  // STA $2E50,X
            store(0x40EBU, indexed_x(0x2E50U), registers.accumulator);
            append_checkpoint(0x40EBU, 0x40EEU, 15U);
            pc = 0x40EEU;
            break;

        // ---- the cursor step that closes the loop ------------------------
        case 0x40EEU:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x40EEU, 0x40F1U, 15U);
            pc = 0x40F1U;
            break;

        case 0x40F1U:  // BIT #$20 -- bit 5 of the state byte ends the walk
            bit_test(0x20U);
            append_checkpoint(0x40F1U, 0x40F3U, 6U);
            pc = 0x40F3U;
            break;

        case 0x40F3U:  // BNE $4102
            pc = branch(
                0x40F3U, 0x4102U, 0x40F5U,
                (registers.status & kZero) == 0U);
            break;

        case 0x40F5U:  // DEX
            registers.x = static_cast<std::uint8_t>(registers.x - 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x40F5U, 0x40F6U, 6U);
            pc = 0x40F6U;
            break;

        case 0x40F6U:  // STX $2E34
            store(0x40F6U, 0x2E34U, registers.x);
            append_checkpoint(0x40F6U, 0x40F9U, 15U);
            pc = 0x40F9U;
            break;

        case 0x40F9U:  // BMI $4102 -- past slot 0
            pc = branch(
                0x40F9U, 0x4102U, 0x40FBU,
                (registers.status & kNegative) != 0U);
            break;

        case 0x40FBU:  // CPX #$08
            compare(registers.x, 0x08U);
            append_checkpoint(0x40FBU, 0x40FDU, 6U);
            pc = 0x40FDU;
            break;

        case 0x40FDU:  // BEQ $4102 -- wrapped back to the seed
            pc = branch(
                0x40FDU, 0x4102U, 0x40FFU,
                (registers.status & kZero) != 0U);
            break;

        // The loop's closing jump, and the chunk boundary. Reached on every
        // iteration, so where a chunk ends needs no lookahead: the walk stops
        // here when either channel is close to full, the core executes the
        // jump, lands on 4044 and calls back for the next chunk. The cost of
        // a chunk is this one instruction, twelve master cycles.
        case 0x40FFU:  // JMP $4044
            append_checkpoint(0x40FFU, 0x4044U, 12U);
            pc = 0x4044U;
            break;

        // ---- the tail ---------------------------------------------------
        case 0x4102U:  // JSR $4211 -- the pending-event gate again
            push(0x4102U, 0x41U);
            push(0x4102U, 0x04U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4102U, 0x4211U, 21U);
            pc = 0x4211U;
            break;

        case 0x4105U:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x4105U, 0x4107U, 12U);
            pc = 0x4107U;
            break;

        case 0x4107U:  // AND #$20 -- is a fade running?
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x20U));
            append_checkpoint(0x4107U, 0x4109U, 6U);
            pc = 0x4109U;
            break;

        case 0x4109U:  // BEQ $4138
            pc = branch(
                0x4109U, 0x4138U, 0x410BU,
                (registers.status & kZero) != 0U);
            break;

        case 0x410BU:  // LDA $2E42 -- the fade step
            load_accumulator(read_ram(0x2E42U));
            append_checkpoint(0x410BU, 0x410EU, 15U);
            pc = 0x410EU;
            break;

        case 0x410EU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x410EU, 0x410FU, 6U);
            pc = 0x410FU;
            break;

        case 0x410FU:  // ADC $2E43
            add_with_carry(read_ram(0x2E43U));
            append_checkpoint(0x410FU, 0x4112U, 15U);
            pc = 0x4112U;
            break;

        case 0x4112U:  // STA $2E43
            store(0x4112U, 0x2E43U, registers.accumulator);
            append_checkpoint(0x4112U, 0x4115U, 15U);
            pc = 0x4115U;
            break;

        case 0x4115U:  // BCC $4138
            pc = branch(
                0x4115U, 0x4138U, 0x4117U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4117U:  // LDA $2E44 -- the fade level
            load_accumulator(read_ram(0x2E44U));
            append_checkpoint(0x4117U, 0x411AU, 15U);
            pc = 0x411AU;
            break;

        case 0x411AU:  // CMP $2E45 -- reached the target?
            compare(registers.accumulator, read_ram(0x2E45U));
            append_checkpoint(0x411AU, 0x411DU, 15U);
            pc = 0x411DU;
            break;

        case 0x411DU:  // BEQ $4138
            pc = branch(
                0x411DU, 0x4138U, 0x411FU,
                (registers.status & kZero) != 0U);
            break;

        case 0x411FU:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x411FU, 0x4121U, 12U);
            pc = 0x4121U;
            break;

        case 0x4121U:  // AND #$10 -- fading up or down
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x10U));
            append_checkpoint(0x4121U, 0x4123U, 6U);
            pc = 0x4123U;
            break;

        // Taken on all four executions on this route. The fade-up arm at
        // 4125 has never run and is left to assembly.
        case 0x4123U:  // BNE $4135
            pc = branch(
                0x4123U, 0x4135U, 0x4125U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4135U: {  // DEC $2E44
            const auto value =
                static_cast<std::uint8_t>(read_ram(0x2E44U) - 1U);
            store(0x4135U, 0x2E44U, value);
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4135U, 0x4138U, 21U);
            pc = 0x4138U;
            break;
        }

        case 0x413BU:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x413BU, 0x413DU, 12U);
            pc = 0x413DU;
            break;

        case 0x413DU:  // AND #$FB -- clear the in-progress bit
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xFBU));
            append_checkpoint(0x413DU, 0x413FU, 6U);
            pc = 0x413FU;
            break;

        case 0x413FU:  // STA $D6
            store(0x413FU, zp(0xD6U), registers.accumulator);
            append_checkpoint(0x413FU, 0x4141U, 12U);
            pc = 0x4141U;
            break;

        // ---- 45:4142  PSG register programmer ---------------------------
        // Walks the six PSG channels in X, pushing each voice's latched
        // state out to the register file. Each iteration is bracketed by
        // PHP/SEI ... PLP, so the driver cannot preempt the read-modify of
        // $F5/$F7/$F9 in the middle; the tail programs the two LFO ports.
        //
        // 4143 is the loop head and the chunk head, and it is an entry point
        // in its own right for the same reason 4044 is. The bracket is what
        // makes it a legal one: ROM's own PLP at 41F1 has already restored the
        // caller's mask by the time control reaches 41F7, so the top of an
        // iteration is a place the hardware may take an interrupt anyway.
        // Stopping there hands the core a boundary ROM already exposes rather
        // than inventing one.
        case 0x4142U:  // CLX
            registers.x = 0x00U;
            append_checkpoint(0x4142U, 0x4143U, 6U);
            pc = 0x4143U;
            break;

        case 0x4143U:  // PHP -- pushes P with T clear and B set
            push(
                0x4143U,
                static_cast<std::uint8_t>(registers.status | 0x10U));
            append_checkpoint(0x4143U, 0x4144U, 9U);
            pc = 0x4144U;
            break;

        case 0x4144U:  // TXA
            load_accumulator(registers.x);
            append_checkpoint(0x4144U, 0x4145U, 6U);
            pc = 0x4145U;
            break;

        case 0x4145U: {  // LSR A -- carry is the channel's low bit
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator >> 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x4145U, 0x4146U, 6U);
            pc = 0x4146U;
            break;
        }

        case 0x4146U:  // BCS $4168 -- odd channels skip the idle check
            pc = branch(
                0x4146U,
                0x4168U,
                0x4148U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4148U:  // LDA $F5,X -- the driver's channel bank byte
            load_accumulator(read_ram(zp_indexed_x(0xF5U)));
            append_checkpoint(0x4148U, 0x414AU, 12U);
            pc = 0x414AU;
            break;

        case 0x414AU:  // BEQ $4168
            pc = branch(
                0x414AU,
                0x4168U,
                0x414CU,
                (registers.status & kZero) != 0U);
            break;

        case 0x414CU:  // LDA $2E0A,X
            load_accumulator(read_ram(indexed_x(0x2E0AU)));
            append_checkpoint(0x414CU, 0x414FU, 15U);
            pc = 0x414FU;
            break;

        case 0x414FU:  // BNE $4167
            pc = branch(
                0x414FU,
                0x4167U,
                0x4151U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4151U:  // STZ $F5,X -- release the channel back to the driver
            store(0x4151U, zp_indexed_x(0xF5U), 0x00U);
            append_checkpoint(0x4151U, 0x4153U, 12U);
            pc = 0x4153U;
            break;

        // Only reached with the mask already set; otherwise
        // `closes_interrupt_window` stops the walk in front of it.
        case 0x4153U:  // SEI, already masked
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x4153U, 0x4154U, 6U);
            pc = 0x4154U;
            break;

        case 0x4154U:  // STX $0800
            store(0x4154U, kPsgChannelSelect, registers.x);
            append_checkpoint(0x4154U, 0x4157U, 15U);
            pc = 0x4157U;
            break;

        case 0x4157U:  // STZ $0805
            store(0x4157U, kPsgBalance, 0x00U);
            append_checkpoint(0x4157U, 0x415AU, 15U);
            pc = 0x415AU;
            break;

        case 0x415AU:  // LDA $F5
            load_accumulator(read_ram(0x20F5U));
            append_checkpoint(0x415AU, 0x415CU, 12U);
            pc = 0x415CU;
            break;

        case 0x415CU:  // ORA $F7
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(0x20F7U)));
            append_checkpoint(0x415CU, 0x415EU, 12U);
            pc = 0x415EU;
            break;

        case 0x415EU:  // ORA $F9
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(0x20F9U)));
            append_checkpoint(0x415EU, 0x4160U, 12U);
            pc = 0x4160U;
            break;

        case 0x4160U:  // BNE $416C -- some channel is still live
            pc = branch(
                0x4160U,
                0x416CU,
                0x4162U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4162U:  // STZ $0C01 -- all three idle: stop the timer
            store(0x4162U, kTimerControl, 0x00U);
            append_checkpoint(0x4162U, 0x4165U, 15U);
            pc = 0x4165U;
            break;

        case 0x4165U:  // BEQ $416C -- always taken, A is zero here
            pc = branch(
                0x4165U,
                0x416CU,
                0x4167U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4167U:  // INX
            registers.x = static_cast<std::uint8_t>(registers.x + 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4167U, 0x4168U, 6U);
            pc = 0x4168U;
            break;

        // Reached only with the mask already set -- `closes_interrupt_window`
        // stops the walk otherwise -- and there the deferred publish is exact,
        // because the byte it defers is the byte the machine already has.
        case 0x4168U:  // SEI, already masked
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x4168U, 0x4169U, 6U);
            pc = 0x4169U;
            break;

        case 0x4169U:  // STX $0800 -- the entry after an emulated SEI
            store(0x4169U, kPsgChannelSelect, registers.x);
            append_checkpoint(0x4169U, 0x416CU, 15U);
            pc = 0x416CU;
            break;

        case 0x416CU:  // LDA $2E22,X -- the latched amplitude
            load_accumulator(read_ram(indexed_x(0x2E22U)));
            append_checkpoint(0x416CU, 0x416FU, 15U);
            pc = 0x416FU;
            break;

        case 0x416FU:  // BNE $4179
            pc = branch(
                0x416FU,
                0x4179U,
                0x4171U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4171U:  // STZ $0807
            store(0x4171U, kPsgNoise, 0x00U);
            append_checkpoint(0x4171U, 0x4174U, 15U);
            pc = 0x4174U;
            break;

        case 0x4174U:  // STA $0804 -- silence: A is zero here
            store(0x4174U, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x4174U, 0x4177U, 15U);
            pc = 0x4177U;
            break;

        case 0x4177U:  // BEQ $41F1 -- always taken
            pc = branch(
                0x4177U,
                0x41F1U,
                0x4179U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4179U:  // LDA $2E16,X
            load_accumulator(read_ram(indexed_x(0x2E16U)));
            append_checkpoint(0x4179U, 0x417CU, 15U);
            pc = 0x417CU;
            break;

        case 0x417CU:  // STA $0802
            store(0x417CU, kPsgFrequencyLow, registers.accumulator);
            append_checkpoint(0x417CU, 0x417FU, 15U);
            pc = 0x417FU;
            break;

        case 0x417FU:  // ORA $2E1C,X
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(indexed_x(0x2E1CU))));
            append_checkpoint(0x417FU, 0x4182U, 15U);
            pc = 0x4182U;
            break;

        case 0x4182U:  // BNE $4188
            pc = branch(
                0x4182U,
                0x4188U,
                0x4184U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4184U:  // LDA #$0F -- a zero period reads as the longest one
            load_accumulator(0x0FU);
            append_checkpoint(0x4184U, 0x4186U, 6U);
            pc = 0x4186U;
            break;

        case 0x4186U:  // BNE $418B -- always taken
            pc = branch(
                0x4186U,
                0x418BU,
                0x4188U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4188U:  // LDA $2E1C,X
            load_accumulator(read_ram(indexed_x(0x2E1CU)));
            append_checkpoint(0x4188U, 0x418BU, 15U);
            pc = 0x418BU;
            break;

        case 0x418BU:  // STA $0803
            store(0x418BU, kPsgFrequencyHigh, registers.accumulator);
            append_checkpoint(0x418BU, 0x418EU, 15U);
            pc = 0x418EU;
            break;

        case 0x418EU:  // LDA $2E28,X
            load_accumulator(read_ram(indexed_x(0x2E28U)));
            append_checkpoint(0x418EU, 0x4191U, 15U);
            pc = 0x4191U;
            break;

        case 0x4191U:  // STA $0805
            store(0x4191U, kPsgBalance, registers.accumulator);
            append_checkpoint(0x4191U, 0x4194U, 15U);
            pc = 0x4194U;
            break;

        case 0x4194U:  // LDA $2E2E,X -- the requested waveform
            load_accumulator(read_ram(indexed_x(0x2E2EU)));
            append_checkpoint(0x4194U, 0x4197U, 15U);
            pc = 0x4197U;
            break;

        case 0x4197U:  // CMP #$FF
            compare(registers.accumulator, 0xFFU);
            append_checkpoint(0x4197U, 0x4199U, 6U);
            pc = 0x4199U;
            break;

        case 0x4199U:  // BEQ $41E3 -- $FF means "leave the waveform alone"
            pc = branch(
                0x4199U,
                0x41E3U,
                0x419BU,
                (registers.status & kZero) != 0U);
            break;

        case 0x419BU:  // CMP $2E3A,X -- already the loaded one?
            compare(registers.accumulator, read_ram(indexed_x(0x2E3AU)));
            append_checkpoint(0x419BU, 0x419EU, 15U);
            pc = 0x419EU;
            break;

        case 0x419EU:  // STA $2E3A,X
            store(0x419EU, indexed_x(0x2E3AU), registers.accumulator);
            append_checkpoint(0x419EU, 0x41A1U, 15U);
            pc = 0x41A1U;
            break;

        case 0x41A1U:  // BEQ $41E3
            pc = branch(
                0x41A1U,
                0x41E3U,
                0x41A3U,
                (registers.status & kZero) != 0U);
            break;

        case 0x41A3U:  // STZ $D3 -- start of index * 32 + $4E59
            store(0x41A3U, zp(0xD3U), 0x00U);
            append_checkpoint(0x41A3U, 0x41A5U, 12U);
            pc = 0x41A5U;
            break;

        case 0x41A5U:
        case 0x41A8U:
        case 0x41ABU:
        case 0x41AEU:
        case 0x41B1U: {  // ASL A
            const auto shifted = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = shifted;
            registers.status = set_nz(status, shifted);
            const auto shift_pc = pc;
            append_checkpoint(
                shift_pc,
                static_cast<std::uint16_t>(shift_pc + 1U),
                6U);
            pc = static_cast<std::uint16_t>(shift_pc + 1U);
            break;
        }

        case 0x41A6U:
        case 0x41A9U:
        case 0x41ACU:
        case 0x41AFU:
        case 0x41B2U: {  // ROL $D3 -- shifts the carry into the high byte
            const auto value = read_ram(zp(0xD3U));
            const auto carry_out = static_cast<std::uint8_t>(value >> 7U);
            const auto rotated = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(value << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kZero | kNegative | kCarry)));
            status = static_cast<std::uint8_t>(status | carry_out);
            registers.status = set_nz(status, rotated);
            const auto rotate_pc = pc;
            store(rotate_pc, zp(0xD3U), rotated);
            append_checkpoint(
                rotate_pc,
                static_cast<std::uint16_t>(rotate_pc + 2U),
                18U);
            pc = static_cast<std::uint16_t>(rotate_pc + 2U);
            break;
        }

        case 0x41B4U:  // ADC #$59
            add_with_carry(0x59U);
            append_checkpoint(0x41B4U, 0x41B6U, 6U);
            pc = 0x41B6U;
            break;

        case 0x41B6U:  // STA $D2
            store(0x41B6U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x41B6U, 0x41B8U, 12U);
            pc = 0x41B8U;
            break;

        case 0x41B8U:  // LDA #$4E
            load_accumulator(0x4EU);
            append_checkpoint(0x41B8U, 0x41BAU, 6U);
            pc = 0x41BAU;
            break;

        case 0x41BAU:  // ADC $D3
            add_with_carry(read_ram(zp(0xD3U)));
            append_checkpoint(0x41BAU, 0x41BCU, 12U);
            pc = 0x41BCU;
            break;

        case 0x41BCU:  // STA $D3
            store(0x41BCU, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x41BCU, 0x41BEU, 12U);
            pc = 0x41BEU;
            break;

        case 0x41BEU:  // LDA #$40 -- arm the waveform write pointer
            load_accumulator(0x40U);
            append_checkpoint(0x41BEU, 0x41C0U, 6U);
            pc = 0x41C0U;
            break;

        case 0x41C0U:  // STA $0804
            store(0x41C0U, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x41C0U, 0x41C3U, 15U);
            pc = 0x41C3U;
            break;

        case 0x41C3U:  // STZ $0804 -- resets the waveform index to zero
            store(0x41C3U, kPsgAmplitude, 0x00U);
            append_checkpoint(0x41C3U, 0x41C6U, 15U);
            pc = 0x41C6U;
            break;

        case 0x41C6U:  // CLY
            registers.y = 0x00U;
            append_checkpoint(0x41C6U, 0x41C7U, 6U);
            pc = 0x41C7U;
            break;

        case 0x41C7U:
        case 0x41CDU:
        case 0x41D3U:
        case 0x41D9U: {  // LDA ($D2),Y -- one of 32 waveform bytes
            load_accumulator(read_mapped(command_pointer()));
            const auto load_pc = pc;
            append_checkpoint(
                load_pc,
                static_cast<std::uint16_t>(load_pc + 2U),
                21U);
            pc = static_cast<std::uint16_t>(load_pc + 2U);
            break;
        }

        case 0x41C9U:
        case 0x41CFU:
        case 0x41D5U:
        case 0x41DBU: {  // STA $0806
            const auto store_pc = pc;
            store(store_pc, kPsgWaveform, registers.accumulator);
            append_checkpoint(
                store_pc,
                static_cast<std::uint16_t>(store_pc + 3U),
                15U);
            pc = static_cast<std::uint16_t>(store_pc + 3U);
            break;
        }

        case 0x41CCU:
        case 0x41D2U:
        case 0x41D8U:
        case 0x41DEU: {  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            const auto increment_pc = pc;
            append_checkpoint(
                increment_pc,
                static_cast<std::uint16_t>(increment_pc + 1U),
                6U);
            pc = static_cast<std::uint16_t>(increment_pc + 1U);
            break;
        }

        case 0x41DFU:  // CPY #$20
            compare(registers.y, 0x20U);
            append_checkpoint(0x41DFU, 0x41E1U, 6U);
            pc = 0x41E1U;
            break;

        case 0x41E1U:  // BNE $41C7
            pc = branch(
                0x41E1U,
                0x41C7U,
                0x41E3U,
                (registers.status & kZero) == 0U);
            break;

        case 0x41E3U:  // LDA $2E22,X
            load_accumulator(read_ram(indexed_x(0x2E22U)));
            append_checkpoint(0x41E3U, 0x41E6U, 15U);
            pc = 0x41E6U;
            break;

        case 0x41E6U:  // ORA #$80 -- enable the channel
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x80U));
            append_checkpoint(0x41E6U, 0x41E8U, 6U);
            pc = 0x41E8U;
            break;

        case 0x41E8U:  // STA $0804
            store(0x41E8U, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x41E8U, 0x41EBU, 15U);
            pc = 0x41EBU;
            break;

        case 0x41EBU:  // LDA $2E34,X
            load_accumulator(read_ram(indexed_x(0x2E34U)));
            append_checkpoint(0x41EBU, 0x41EEU, 15U);
            pc = 0x41EEU;
            break;

        case 0x41EEU:  // STA $0807
            store(0x41EEU, kPsgNoise, registers.accumulator);
            append_checkpoint(0x41EEU, 0x41F1U, 15U);
            pc = 0x41F1U;
            break;

        case 0x41F1U:  // PLP -- restores the caller's mask, one step late
            pull_status();
            append_checkpoint(0x41F1U, 0x41F2U, 12U);
            pc = 0x41F2U;
            break;

        case 0x41F2U:  // INX
            registers.x = static_cast<std::uint8_t>(registers.x + 1U);
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x41F2U, 0x41F3U, 6U);
            pc = 0x41F3U;
            break;

        case 0x41F3U:  // CPX #$06
            compare(registers.x, 0x06U);
            append_checkpoint(0x41F3U, 0x41F5U, 6U);
            pc = 0x41F5U;
            break;

        case 0x41F5U:  // BCS $41FA
            pc = branch(
                0x41F5U,
                0x41FAU,
                0x41F7U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x41F7U:  // JMP $4143
            append_checkpoint(0x41F7U, 0x4143U, 12U);
            pc = 0x4143U;
            break;

        case 0x41FAU:  // LDA $2E40
            load_accumulator(read_ram(0x2E40U));
            append_checkpoint(0x41FAU, 0x41FDU, 15U);
            pc = 0x41FDU;
            break;

        case 0x41FDU:  // STA $0808
            store(0x41FDU, kPsgLfoFrequency, registers.accumulator);
            append_checkpoint(0x41FDU, 0x4200U, 15U);
            pc = 0x4200U;
            break;

        case 0x4200U:  // LDA $2E41
            load_accumulator(read_ram(0x2E41U));
            append_checkpoint(0x4200U, 0x4203U, 15U);
            pc = 0x4203U;
            break;

        case 0x4203U:  // BPL $420D
            pc = branch(
                0x4203U,
                0x420DU,
                0x4205U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4205U:  // STA $0809 -- a pending reset writes the port twice
            store(0x4205U, kPsgLfoControl, registers.accumulator);
            append_checkpoint(0x4205U, 0x4208U, 15U);
            pc = 0x4208U;
            break;

        case 0x4208U:  // AND #$7F -- and clears the request
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU));
            append_checkpoint(0x4208U, 0x420AU, 6U);
            pc = 0x420AU;
            break;

        case 0x420AU:  // STA $2E41
            store(0x420AU, 0x2E41U, registers.accumulator);
            append_checkpoint(0x420AU, 0x420DU, 15U);
            pc = 0x420DU;
            break;

        case 0x420DU:  // STA $0809
            store(0x420DU, kPsgLfoControl, registers.accumulator);
            append_checkpoint(0x420DU, 0x4210U, 15U);
            pc = 0x4210U;
            break;

        // ---- 45:4211  pending-event gates -------------------------------
        case 0x4211U:  // LDA $2E02
            load_accumulator(read_ram(0x2E02U));
            append_checkpoint(0x4211U, 0x4214U, 15U);
            pc = 0x4214U;
            break;

        case 0x4214U:  // BEQ $421C
            pc = branch(
                0x4214U,
                0x421CU,
                0x4216U,
                (registers.status & kZero) != 0U);
            break;

        case 0x421CU:  // LDA $2E03
            load_accumulator(read_ram(0x2E03U));
            append_checkpoint(0x421CU, 0x421FU, 15U);
            pc = 0x421FU;
            break;

        case 0x421FU:  // BEQ $4227
            pc = branch(
                0x421FU,
                0x4227U,
                0x4221U,
                (registers.status & kZero) != 0U);
            break;

        // ---- 45:4883  pitch/period update -------------------------------
        case 0x4883U:  // LDA $2E5A,X
            load_accumulator(read_ram(indexed_x(0x2E5AU)));
            append_checkpoint(0x4883U, 0x4886U, 15U);
            pc = 0x4886U;
            break;

        case 0x4886U: {  // ASL A
            const auto shifted = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = shifted;
            registers.status = set_nz(status, shifted);
            append_checkpoint(0x4886U, 0x4887U, 6U);
            pc = 0x4887U;
            break;
        }

        case 0x4887U:  // BEQ $48AD
            pc = branch(
                0x4887U,
                0x48ADU,
                0x4889U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4889U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4889U, 0x488AU, 6U);
            pc = 0x488AU;
            break;

        case 0x488AU:  // LDA $5C2F,Y
            load_accumulator(read_mapped(indexed_y(0x5C2FU)));
            append_checkpoint(0x488AU, 0x488DU, 15U);
            pc = 0x488DU;
            break;

        case 0x488DU:  // STA $D2
            store(0x488DU, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x488DU, 0x488FU, 12U);
            pc = 0x488FU;
            break;

        case 0x488FU:  // LDA $5C30,Y
            load_accumulator(read_mapped(indexed_y(0x5C30U)));
            append_checkpoint(0x488FU, 0x4892U, 15U);
            pc = 0x4892U;
            break;

        case 0x4892U:  // STA $D3
            store(0x4892U, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x4892U, 0x4894U, 12U);
            pc = 0x4894U;
            break;

        case 0x4894U:  // LDY $2F72,X
            registers.y = read_ram(indexed_x(0x2F72U));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4894U, 0x4897U, 15U);
            pc = 0x4897U;
            break;

        case 0x4897U:  // JSR $48B4
            push(0x4897U, 0x48U);
            push(0x4897U, 0x99U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4897U, 0x48B4U, 21U);
            pc = 0x48B4U;
            break;

        case 0x489AU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x489AU, 0x489BU, 6U);
            pc = 0x489BU;
            break;

        case 0x489BU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x489BU, 0x489CU, 6U);
            pc = 0x489CU;
            break;

        case 0x489CU:  // ADC $2E50,X
            add_with_carry(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x489CU, 0x489FU, 15U);
            pc = 0x489FU;
            break;

        case 0x489FU:  // SBC #$1E
            subtract_with_carry(0x1EU);
            append_checkpoint(0x489FU, 0x48A1U, 6U);
            pc = 0x48A1U;
            break;

        case 0x48A1U:  // BCS $48A5
            pc = branch(
                0x48A1U,
                0x48A5U,
                0x48A3U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x48A3U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0x48A3U, 0x48A5U, 6U);
            pc = 0x48A5U;
            break;

        case 0x48A5U:  // STA $2E37
            store(0x48A5U, 0x2E37U, registers.accumulator);
            append_checkpoint(0x48A5U, 0x48A8U, 15U);
            pc = 0x48A8U;
            break;

        case 0x48A8U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x48A8U, 0x48A9U, 6U);
            pc = 0x48A9U;
            break;

        case 0x48A9U:  // STA $2F72,X
            store(0x48A9U, indexed_x(0x2F72U), registers.accumulator);
            append_checkpoint(0x48A9U, 0x48ACU, 15U);
            pc = 0x48ACU;
            break;

        case 0x48ADU:  // LDA $2E50,X
            load_accumulator(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x48ADU, 0x48B0U, 15U);
            pc = 0x48B0U;
            break;

        case 0x48B0U:  // STA $2E37
            store(0x48B0U, 0x2E37U, registers.accumulator);
            append_checkpoint(0x48B0U, 0x48B3U, 15U);
            pc = 0x48B3U;
            break;

        // ---- 45:48B4  sequence-data fetch -------------------------------
        // The shared command decoder. Values below $80 are literal data; the
        // five commands $80-$84 re-read the stream relative to the cursor.
        case 0x48B4U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48B4U, 0x48B6U, 21U);
            pc = 0x48B6U;
            break;

        case 0x48B6U:  // BMI $48B9
            pc = branch(
                0x48B6U,
                0x48B9U,
                0x48B8U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x48B9U:  // CMP #$80
            compare(registers.accumulator, 0x80U);
            append_checkpoint(0x48B9U, 0x48BBU, 6U);
            pc = 0x48BBU;
            break;

        case 0x48BBU:  // BNE $48C1
            pc = branch(
                0x48BBU,
                0x48C1U,
                0x48BDU,
                (registers.status & kZero) == 0U);
            break;

        case 0x48BDU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48BDU, 0x48BEU, 6U);
            pc = 0x48BEU;
            break;

        case 0x48BEU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48BEU, 0x48C0U, 21U);
            pc = 0x48C0U;
            break;

        case 0x48C1U:  // CMP #$81
            compare(registers.accumulator, 0x81U);
            append_checkpoint(0x48C1U, 0x48C3U, 6U);
            pc = 0x48C3U;
            break;

        case 0x48C3U:  // BNE $48CE
            pc = branch(
                0x48C3U,
                0x48CEU,
                0x48C5U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48C5U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x48C5U, 0x48C6U, 6U);
            pc = 0x48C6U;
            break;

        case 0x48C6U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48C6U, 0x48C7U, 6U);
            pc = 0x48C7U;
            break;

        case 0x48C7U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0x48C7U, 0x48C8U, 6U);
            pc = 0x48C8U;
            break;

        case 0x48C8U:  // SBC ($D2),Y
            subtract_with_carry(read_mapped(command_pointer()));
            append_checkpoint(0x48C8U, 0x48CAU, 21U);
            pc = 0x48CAU;
            break;

        case 0x48CAU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48CAU, 0x48CBU, 6U);
            pc = 0x48CBU;
            break;

        case 0x48CBU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48CBU, 0x48CDU, 21U);
            pc = 0x48CDU;
            break;

        case 0x48CEU:  // CMP #$82
            compare(registers.accumulator, 0x82U);
            append_checkpoint(0x48CEU, 0x48D0U, 6U);
            pc = 0x48D0U;
            break;

        case 0x48D0U:  // BNE $48D7
            pc = branch(
                0x48D0U,
                0x48D7U,
                0x48D2U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48D2U:  // LDY #$00
            registers.y = 0x00U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48D2U, 0x48D4U, 6U);
            pc = 0x48D4U;
            break;

        case 0x48D4U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48D4U, 0x48D6U, 21U);
            pc = 0x48D6U;
            break;

        case 0x48D7U:  // CMP #$83
            compare(registers.accumulator, 0x83U);
            append_checkpoint(0x48D7U, 0x48D9U, 6U);
            pc = 0x48D9U;
            break;

        case 0x48D9U:  // BNE $48E6
            pc = branch(
                0x48D9U,
                0x48E6U,
                0x48DBU,
                (registers.status & kZero) == 0U);
            break;

        case 0x48DBU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x48DBU, 0x48DEU, 15U);
            pc = 0x48DEU;
            break;

        case 0x48DEU:  // AND #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x04U));
            append_checkpoint(0x48DEU, 0x48E0U, 6U);
            pc = 0x48E0U;
            break;

        case 0x48E0U:  // BNE $48BD
            pc = branch(
                0x48E0U,
                0x48BDU,
                0x48E2U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48E2U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48E2U, 0x48E3U, 6U);
            pc = 0x48E3U;
            break;

        case 0x48E3U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48E3U, 0x48E5U, 21U);
            pc = 0x48E5U;
            break;

        case 0x48E6U:  // CMP #$84
            compare(registers.accumulator, 0x84U);
            append_checkpoint(0x48E6U, 0x48E8U, 6U);
            pc = 0x48E8U;
            break;

        case 0x48E8U:  // BNE $48E5
            pc = branch(
                0x48E8U,
                0x48E5U,
                0x48EAU,
                (registers.status & kZero) == 0U);
            break;

        case 0x48EAU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x48EAU, 0x48EDU, 15U);
            pc = 0x48EDU;
            break;

        case 0x48EDU:  // AND #$04
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x04U));
            append_checkpoint(0x48EDU, 0x48EFU, 6U);
            pc = 0x48EFU;
            break;

        case 0x48EFU:  // BNE $48C5
            pc = branch(
                0x48EFU,
                0x48C5U,
                0x48F1U,
                (registers.status & kZero) == 0U);
            break;

        case 0x48F1U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48F1U, 0x48F2U, 6U);
            pc = 0x48F2U;
            break;

        case 0x48F2U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x48F2U, 0x48F3U, 6U);
            pc = 0x48F3U;
            break;

        case 0x48F3U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x48F3U, 0x48F5U, 21U);
            pc = 0x48F5U;
            break;

        // ---- 45:48F6  sweep accumulator update --------------------------
        case 0x48F6U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x48F6U, 0x48F9U, 15U);
            pc = 0x48F9U;
            break;

        case 0x48F9U:  // ORA $2F2C,X
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(indexed_x(0x2F2CU))));
            append_checkpoint(0x48F9U, 0x48FCU, 15U);
            pc = 0x48FCU;
            break;

        case 0x48FCU:  // BEQ $4935
            pc = branch(
                0x48FCU,
                0x4935U,
                0x48FEU,
                (registers.status & kZero) != 0U);
            break;

        case 0x48FEU:  // LDA $2E64,X
            load_accumulator(read_ram(indexed_x(0x2E64U)));
            append_checkpoint(0x48FEU, 0x4901U, 15U);
            pc = 0x4901U;
            break;

        case 0x4901U:  // AND #$1F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x1FU));
            append_checkpoint(0x4901U, 0x4903U, 6U);
            pc = 0x4903U;
            break;

        case 0x4903U: {  // ASL A
            const auto shifted = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = shifted;
            registers.status = set_nz(status, shifted);
            append_checkpoint(0x4903U, 0x4904U, 6U);
            pc = 0x4904U;
            break;
        }

        case 0x4904U:  // BEQ $4935
            pc = branch(
                0x4904U,
                0x4935U,
                0x4906U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4906U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4906U, 0x4907U, 6U);
            pc = 0x4907U;
            break;

        case 0x4907U:  // LDA $5B6E,Y
            load_accumulator(read_mapped(indexed_y(0x5B6EU)));
            append_checkpoint(0x4907U, 0x490AU, 15U);
            pc = 0x490AU;
            break;

        case 0x490AU:  // STA $D2
            store(0x490AU, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x490AU, 0x490CU, 12U);
            pc = 0x490CU;
            break;

        case 0x490CU:  // LDA $5B6F,Y
            load_accumulator(read_mapped(indexed_y(0x5B6FU)));
            append_checkpoint(0x490CU, 0x490FU, 15U);
            pc = 0x490FU;
            break;

        case 0x490FU:  // STA $D3
            store(0x490FU, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x490FU, 0x4911U, 12U);
            pc = 0x4911U;
            break;

        case 0x4911U:  // LDY $2F7C,X
            registers.y = read_ram(indexed_x(0x2F7CU));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4911U, 0x4914U, 15U);
            pc = 0x4914U;
            break;

        case 0x4914U:  // JSR $48B4
            push(0x4914U, 0x49U);
            push(0x4914U, 0x16U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4914U, 0x48B4U, 21U);
            pc = 0x48B4U;
            break;

        case 0x4917U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4917U, 0x4918U, 6U);
            pc = 0x4918U;
            break;

        case 0x4918U:  // STA $D5
            store(0x4918U, zp(0xD5U), registers.accumulator);
            append_checkpoint(0x4918U, 0x491AU, 12U);
            pc = 0x491AU;
            break;

        case 0x491AU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x491AU, 0x491BU, 6U);
            pc = 0x491BU;
            break;

        case 0x491BU:  // STA $2F7C,X
            store(0x491BU, indexed_x(0x2F7CU), registers.accumulator);
            append_checkpoint(0x491BU, 0x491EU, 15U);
            pc = 0x491EU;
            break;

        case 0x491EU:  // LDY #$00
            registers.y = 0x00U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x491EU, 0x4920U, 6U);
            pc = 0x4920U;
            break;

        case 0x4920U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4920U, 0x4921U, 6U);
            pc = 0x4921U;
            break;

        case 0x4921U:  // LDA $D5
            load_accumulator(ram[0x00D5U]);
            append_checkpoint(0x4921U, 0x4923U, 12U);
            pc = 0x4923U;
            break;

        case 0x4923U:  // BEQ $4935
            pc = branch(
                0x4923U,
                0x4935U,
                0x4925U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4925U:  // BPL $4928
            pc = branch(
                0x4925U,
                0x4928U,
                0x4927U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4927U:  // DEY -- sign-extends the delta into the high byte
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4927U, 0x4928U, 6U);
            pc = 0x4928U;
            break;

        case 0x4928U:  // ADC $2F22,X
            add_with_carry(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4928U, 0x492BU, 15U);
            pc = 0x492BU;
            break;

        case 0x492BU:  // STA $2F22,X
            store(0x492BU, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x492BU, 0x492EU, 15U);
            pc = 0x492EU;
            break;

        case 0x492EU:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x492EU, 0x492FU, 6U);
            pc = 0x492FU;
            break;

        case 0x492FU:  // ADC $2F2C,X
            add_with_carry(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x492FU, 0x4932U, 15U);
            pc = 0x4932U;
            break;

        case 0x4932U:  // STA $2F2C,X
            store(0x4932U, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x4932U, 0x4935U, 15U);
            pc = 0x4935U;
            break;

        // ---- 45:4936  pitch-slide stepper -------------------------------
        case 0x4936U:  // LDA $2F04,X -- no note means no slide
            load_accumulator(read_ram(indexed_x(0x2F04U)));
            append_checkpoint(0x4936U, 0x4939U, 15U);
            pc = 0x4939U;
            break;

        case 0x4939U:  // BEQ $494B
            pc = branch(
                0x4939U,
                0x494BU,
                0x493BU,
                (registers.status & kZero) != 0U);
            break;

        case 0x493BU:  // LDA $2F2C,X -- current pitch high byte
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x493BU, 0x493EU, 15U);
            pc = 0x493EU;
            break;

        case 0x493EU:  // CMP $2F68,X -- destination high byte
            compare(registers.accumulator, read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x493EU, 0x4941U, 15U);
            pc = 0x4941U;
            break;

        case 0x4941U:  // BNE $496D -- cross a high-byte boundary
            pc = branch(
                0x4941U,
                0x496DU,
                0x4943U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4943U:  // LDA $2F22,X -- current pitch low byte
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4943U, 0x4946U, 15U);
            pc = 0x4946U;
            break;

        case 0x4946U:  // CMP $2F5E,X -- destination low byte
            compare(registers.accumulator, read_ram(indexed_x(0x2F5EU)));
            append_checkpoint(0x4946U, 0x4949U, 15U);
            pc = 0x4949U;
            break;

        case 0x4949U:  // BNE $494C
            pc = branch(
                0x4949U,
                0x494CU,
                0x494BU,
                (registers.status & kZero) == 0U);
            break;

        case 0x494CU:  // BCC $495F -- select upward or downward motion
            pc = branch(
                0x494CU,
                0x495FU,
                0x494EU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x494EU:  // SBC $2F86,X -- downward pitch step
            subtract_with_carry(read_ram(indexed_x(0x2F86U)));
            append_checkpoint(0x494EU, 0x4951U, 15U);
            pc = 0x4951U;
            break;

        case 0x4951U:  // BCC $4958 -- clamp on underflow
            pc = branch(
                0x4951U,
                0x4958U,
                0x4953U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4953U:  // CMP $2F5E,X -- clamp when the step crossed target
            compare(registers.accumulator, read_ram(indexed_x(0x2F5EU)));
            append_checkpoint(0x4953U, 0x4956U, 15U);
            pc = 0x4956U;
            break;

        case 0x4956U:  // BCS $4969
            pc = branch(
                0x4956U,
                0x4969U,
                0x4958U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4958U:  // LDA $2F5E,X -- exact destination low byte
            load_accumulator(read_ram(indexed_x(0x2F5EU)));
            append_checkpoint(0x4958U, 0x495BU, 15U);
            pc = 0x495BU;
            break;

        case 0x495BU:  // STA $2F22,X
            store(0x495BU, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x495BU, 0x495EU, 15U);
            pc = 0x495EU;
            break;

        case 0x495FU:  // ADC $2F86,X -- upward pitch step
            add_with_carry(read_ram(indexed_x(0x2F86U)));
            append_checkpoint(0x495FU, 0x4962U, 15U);
            pc = 0x4962U;
            break;

        case 0x4962U:  // BCS $4958 -- clamp on overflow
            pc = branch(
                0x4962U,
                0x4958U,
                0x4964U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4964U:  // CMP $2F5E,X -- clamp when the step crossed target
            compare(registers.accumulator, read_ram(indexed_x(0x2F5EU)));
            append_checkpoint(0x4964U, 0x4967U, 15U);
            pc = 0x4967U;
            break;

        case 0x4967U:  // BCS $4958
            pc = branch(
                0x4967U,
                0x4958U,
                0x4969U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4969U:  // STA $2F22,X
            store(0x4969U, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x4969U, 0x496CU, 15U);
            pc = 0x496CU;
            break;

        case 0x496DU:  // LDA $2F22,X -- step across a high-byte boundary
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x496DU, 0x4970U, 15U);
            pc = 0x4970U;
            break;

        case 0x4970U:  // BCC $498A -- high byte is below destination
            pc = branch(
                0x4970U,
                0x498AU,
                0x4972U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4972U:  // SBC $2F86,X
            subtract_with_carry(read_ram(indexed_x(0x2F86U)));
            append_checkpoint(0x4972U, 0x4975U, 15U);
            pc = 0x4975U;
            break;

        case 0x4975U:  // STA $2F22,X
            store(0x4975U, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x4975U, 0x4978U, 15U);
            pc = 0x4978U;
            break;

        case 0x4978U:  // BCS $496C -- no borrow from the high byte
            pc = branch(
                0x4978U,
                0x496CU,
                0x497AU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x497AU: {  // DEC $2F2C,X
            const auto address = indexed_x(0x2F2CU);
            const auto value = static_cast<std::uint8_t>(
                read_ram(address) - 1U);
            registers.status = set_nz(registers.status, value);
            store(0x497AU, address, value);
            append_checkpoint(0x497AU, 0x497DU, 21U);
            pc = 0x497DU;
            break;
        }

        case 0x497DU:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x497DU, 0x4980U, 15U);
            pc = 0x4980U;
            break;

        case 0x4980U:  // CMP $2F68,X
            compare(registers.accumulator, read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x4980U, 0x4983U, 15U);
            pc = 0x4983U;
            break;

        case 0x4983U:  // BNE $496C
            pc = branch(
                0x4983U,
                0x496CU,
                0x4985U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4985U:  // LDA $2F22,X -- final low-byte clamp after borrow
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4985U, 0x4988U, 15U);
            pc = 0x4988U;
            break;

        case 0x4988U:  // BCS $4953
            pc = branch(
                0x4988U,
                0x4953U,
                0x498AU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x498AU:  // ADC $2F86,X
            add_with_carry(read_ram(indexed_x(0x2F86U)));
            append_checkpoint(0x498AU, 0x498DU, 15U);
            pc = 0x498DU;
            break;

        case 0x498DU:  // STA $2F22,X
            store(0x498DU, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x498DU, 0x4990U, 15U);
            pc = 0x4990U;
            break;

        case 0x4990U:  // BCC $496C -- no carry into the high byte
            pc = branch(
                0x4990U,
                0x496CU,
                0x4992U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4992U: {  // INC $2F2C,X
            const auto address = indexed_x(0x2F2CU);
            const auto value = static_cast<std::uint8_t>(
                read_ram(address) + 1U);
            registers.status = set_nz(registers.status, value);
            store(0x4992U, address, value);
            append_checkpoint(0x4992U, 0x4995U, 21U);
            pc = 0x4995U;
            break;
        }

        case 0x4995U:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x4995U, 0x4998U, 15U);
            pc = 0x4998U;
            break;

        case 0x4998U:  // CMP $2F68,X
            compare(registers.accumulator, read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x4998U, 0x499BU, 15U);
            pc = 0x499BU;
            break;

        case 0x499BU:  // BNE $496C
            pc = branch(
                0x499BU,
                0x496CU,
                0x499DU,
                (registers.status & kZero) == 0U);
            break;

        case 0x499DU:  // LDA $2F22,X -- final low-byte clamp after carry
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x499DU, 0x49A0U, 15U);
            pc = 0x49A0U;
            break;

        case 0x49A0U:  // BCS $4964
            pc = branch(
                0x49A0U,
                0x4964U,
                0x49A2U,
                (registers.status & kCarry) != 0U);
            break;

        // ---- 45:49A2  vibrato accumulator update ------------------------
        case 0x49A2U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x49A2U, 0x49A5U, 15U);
            pc = 0x49A5U;
            break;

        case 0x49A5U:  // AND #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x02U));
            append_checkpoint(0x49A5U, 0x49A7U, 6U);
            pc = 0x49A7U;
            break;

        case 0x49A7U:  // BEQ $49B5
            pc = branch(
                0x49A7U,
                0x49B5U,
                0x49A9U,
                (registers.status & kZero) != 0U);
            break;

        case 0x49A9U:  // LDA $2F5E,X
            load_accumulator(read_ram(indexed_x(0x2F5EU)));
            append_checkpoint(0x49A9U, 0x49ACU, 15U);
            pc = 0x49ACU;
            break;

        case 0x49ACU:  // BEQ $49B5
            pc = branch(
                0x49ACU,
                0x49B5U,
                0x49AEU,
                (registers.status & kZero) != 0U);
            break;

        case 0x49AEU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x49AEU, 0x49AFU, 6U);
            pc = 0x49AFU;
            break;

        case 0x49AFU:  // ADC $2F68,X
            add_with_carry(read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x49AFU, 0x49B2U, 15U);
            pc = 0x49B2U;
            break;

        case 0x49B2U:  // STA $2F68,X
            store(0x49B2U, indexed_x(0x2F68U), registers.accumulator);
            append_checkpoint(0x49B2U, 0x49B5U, 15U);
            pc = 0x49B5U;
            break;

        // ---- 45:49B6  voice-latch clear ---------------------------------
        case 0x49B6U:  // STZ $2E22
            store(0x49B6U, 0x2E22U, 0x00U);
            append_checkpoint(0x49B6U, 0x49B9U, 15U);
            pc = 0x49B9U;
            break;

        case 0x49B9U:  // STZ $2E23
            store(0x49B9U, 0x2E23U, 0x00U);
            append_checkpoint(0x49B9U, 0x49BCU, 15U);
            pc = 0x49BCU;
            break;

        case 0x49BCU:  // STZ $2E24
            store(0x49BCU, 0x2E24U, 0x00U);
            append_checkpoint(0x49BCU, 0x49BFU, 15U);
            pc = 0x49BFU;
            break;

        case 0x49BFU:  // STZ $2E25
            store(0x49BFU, 0x2E25U, 0x00U);
            append_checkpoint(0x49BFU, 0x49C2U, 15U);
            pc = 0x49C2U;
            break;

        case 0x49C2U:  // STZ $2E26
            store(0x49C2U, 0x2E26U, 0x00U);
            append_checkpoint(0x49C2U, 0x49C5U, 15U);
            pc = 0x49C5U;
            break;

        case 0x49C5U:  // STZ $2E27
            store(0x49C5U, 0x2E27U, 0x00U);
            append_checkpoint(0x49C5U, 0x49C8U, 15U);
            pc = 0x49C8U;
            break;

        case 0x49C8U:  // STZ $2E0A
            store(0x49C8U, 0x2E0AU, 0x00U);
            append_checkpoint(0x49C8U, 0x49CBU, 15U);
            pc = 0x49CBU;
            break;

        case 0x49CBU:  // STZ $2E0C
            store(0x49CBU, 0x2E0CU, 0x00U);
            append_checkpoint(0x49CBU, 0x49CEU, 15U);
            pc = 0x49CEU;
            break;

        case 0x49CEU:  // STZ $2E0E
            store(0x49CEU, 0x2E0EU, 0x00U);
            append_checkpoint(0x49CEU, 0x49D1U, 15U);
            pc = 0x49D1U;
            break;

        // ---- 45:49D2  amplitude commit ----------------------------------
        // Reached from the voice commit below. Selects the PSG channel in Y,
        // writes the pending level to $0804, and marks the voice latched.
        case 0x49D2U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x49D2U, 0x49D3U, 6U);
            pc = 0x49D3U;
            break;

        case 0x49D3U: {  // LSR A
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator >> 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x49D3U, 0x49D4U, 6U);
            pc = 0x49D4U;
            break;
        }

        case 0x49D4U:  // BCS $49D1
            pc = branch(
                0x49D4U,
                0x49D1U,
                0x49D6U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x49D6U:  // LDA $20F5,Y
            load_accumulator(read_ram(indexed_y(0x20F5U)));
            append_checkpoint(0x49D6U, 0x49D9U, 15U);
            pc = 0x49D9U;
            break;

        case 0x49D9U:  // BEQ $49D1
            pc = branch(
                0x49D9U,
                0x49D1U,
                0x49DBU,
                (registers.status & kZero) != 0U);
            break;

        case 0x49DBU:  // LDA $2E05,Y
            load_accumulator(read_ram(indexed_y(0x2E05U)));
            append_checkpoint(0x49DBU, 0x49DEU, 15U);
            pc = 0x49DEU;
            break;

        case 0x49DEU:  // CMP $2F0E,X
            compare(registers.accumulator, read_ram(indexed_x(0x2F0EU)));
            append_checkpoint(0x49DEU, 0x49E1U, 15U);
            pc = 0x49E1U;
            break;

        case 0x49E1U:  // BNE $49D1
            pc = branch(
                0x49E1U,
                0x49D1U,
                0x49E3U,
                (registers.status & kZero) == 0U);
            break;

        // Only reached with the mask already set; otherwise
        // `closes_interrupt_window` stops the walk in front of it.
        case 0x49E3U:  // SEI, already masked
            // The mask is applied after the core has sampled the interrupt
            // for this instruction, so an interrupt already pending here is
            // still taken; the checkpoint publishes the old status.
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x49E3U, 0x49E4U, 6U);
            pc = 0x49E4U;
            break;

        case 0x49E4U:  // STY $0800
            store(0x49E4U, kPsgChannelSelect, registers.y);
            append_checkpoint(0x49E4U, 0x49E7U, 15U);
            pc = 0x49E7U;
            break;

        case 0x49E7U:  // LDA $2E37
            load_accumulator(read_ram(0x2E37U));
            append_checkpoint(0x49E7U, 0x49EAU, 15U);
            pc = 0x49EAU;
            break;

        case 0x49EAU:  // ORA #$C0
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0xC0U));
            append_checkpoint(0x49EAU, 0x49ECU, 6U);
            pc = 0x49ECU;
            break;

        case 0x49ECU:  // STA $0804
            store(0x49ECU, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x49ECU, 0x49EFU, 15U);
            pc = 0x49EFU;
            break;

        case 0x49EFU:  // STA $2E22,Y
            store(0x49EFU, indexed_y(0x2E22U), registers.accumulator);
            append_checkpoint(0x49EFU, 0x49F2U, 15U);
            pc = 0x49F2U;
            break;

        case 0x49F2U:  // CLI -- opens the window one instruction later
            pending_status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kInterruptDisable));
            status_change_pending = true;
            append_checkpoint(0x49F2U, 0x49F3U, 6U);
            pc = 0x49F3U;
            break;

        case 0x49F3U:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0x49F3U, 0x49F5U, 6U);
            pc = 0x49F5U;
            break;

        case 0x49F5U:  // STA $2E0A,Y
            store(0x49F5U, indexed_y(0x2E0AU), registers.accumulator);
            append_checkpoint(0x49F5U, 0x49F8U, 15U);
            pc = 0x49F8U;
            break;

        // ---- 45:49F9  voice commit --------------------------------------
        case 0x49F9U:  // LDY $2E82,X
            registers.y = read_ram(indexed_x(0x2E82U));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x49F9U, 0x49FCU, 15U);
            pc = 0x49FCU;
            break;

        case 0x49FCU:  // LDA $2E22,Y
            load_accumulator(read_ram(indexed_y(0x2E22U)));
            append_checkpoint(0x49FCU, 0x49FFU, 15U);
            pc = 0x49FFU;
            break;

        case 0x49FFU:  // BNE $49D1
            pc = branch(
                0x49FFU,
                0x49D1U,
                0x4A01U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A01U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4A01U, 0x4A04U, 15U);
            pc = 0x4A04U;
            break;

        case 0x4A04U:  // AND #$08
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x08U));
            append_checkpoint(0x4A04U, 0x4A06U, 6U);
            pc = 0x4A06U;
            break;

        case 0x4A06U:  // BEQ $4A16
            pc = branch(
                0x4A06U,
                0x4A16U,
                0x4A08U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A08U:  // LDA $2E37
            load_accumulator(read_ram(0x2E37U));
            append_checkpoint(0x4A08U, 0x4A0BU, 15U);
            pc = 0x4A0BU;
            break;

        case 0x4A0BU:  // ADC $2E44 -- no CLC: the carry is the caller's
            add_with_carry(read_ram(0x2E44U));
            append_checkpoint(0x4A0BU, 0x4A0EU, 15U);
            pc = 0x4A0EU;
            break;

        case 0x4A0EU:  // SBC #$1F
            subtract_with_carry(0x1FU);
            append_checkpoint(0x4A0EU, 0x4A10U, 6U);
            pc = 0x4A10U;
            break;

        case 0x4A10U:  // BCS $4A13
            pc = branch(
                0x4A10U,
                0x4A13U,
                0x4A12U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4A12U:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0x4A12U, 0x4A13U, 6U);
            pc = 0x4A13U;
            break;

        case 0x4A13U:  // STA $2E37
            store(0x4A13U, 0x2E37U, registers.accumulator);
            append_checkpoint(0x4A13U, 0x4A16U, 15U);
            pc = 0x4A16U;
            break;

        case 0x4A16U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x4A16U, 0x4A19U, 15U);
            pc = 0x4A19U;
            break;

        case 0x4A19U:  // AND #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x02U));
            append_checkpoint(0x4A19U, 0x4A1BU, 6U);
            pc = 0x4A1BU;
            break;

        case 0x4A1BU:  // BNE $49D2
            pc = branch(
                0x4A1BU,
                0x49D2U,
                0x4A1DU,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A1DU:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4A1DU, 0x4A20U, 15U);
            pc = 0x4A20U;
            break;

        case 0x4A20U:  // ORA $2F2C,X
            load_accumulator(
                static_cast<std::uint8_t>(
                    registers.accumulator | read_ram(indexed_x(0x2F2CU))));
            append_checkpoint(0x4A20U, 0x4A23U, 15U);
            pc = 0x4A23U;
            break;

        case 0x4A23U:  // BNE $4A29
            pc = branch(
                0x4A23U,
                0x4A29U,
                0x4A25U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A25U:  // STA $2E22,Y
            store(0x4A25U, indexed_y(0x2E22U), registers.accumulator);
            append_checkpoint(0x4A25U, 0x4A28U, 15U);
            pc = 0x4A28U;
            break;

        case 0x4A29U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4A29U, 0x4A2CU, 15U);
            pc = 0x4A2CU;
            break;

        case 0x4A2CU:  // AND #$0A
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x0AU));
            append_checkpoint(0x4A2CU, 0x4A2EU, 6U);
            pc = 0x4A2EU;
            break;

        case 0x4A2EU:  // BEQ $4A57
            pc = branch(
                0x4A2EU,
                0x4A57U,
                0x4A30U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A30U:  // CMP #$02
            compare(registers.accumulator, 0x02U);
            append_checkpoint(0x4A30U, 0x4A32U, 6U);
            pc = 0x4A32U;
            break;

        case 0x4A32U:  // BEQ $4A45
            pc = branch(
                0x4A32U,
                0x4A45U,
                0x4A34U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A34U:  // LDA $2F68,X
            load_accumulator(read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x4A34U, 0x4A37U, 15U);
            pc = 0x4A37U;
            break;

        case 0x4A37U:  // AND #$1F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x1FU));
            append_checkpoint(0x4A37U, 0x4A39U, 6U);
            pc = 0x4A39U;
            break;

        case 0x4A39U:  // ORA #$80
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x80U));
            append_checkpoint(0x4A39U, 0x4A3BU, 6U);
            pc = 0x4A3BU;
            break;

        case 0x4A3BU:  // STA $2E34,Y
            store(0x4A3BU, indexed_y(0x2E34U), registers.accumulator);
            append_checkpoint(0x4A3BU, 0x4A3EU, 15U);
            pc = 0x4A3EU;
            break;

        case 0x4A3EU:  // LDA #$FF
            load_accumulator(0xFFU);
            append_checkpoint(0x4A3EU, 0x4A40U, 6U);
            pc = 0x4A40U;
            break;

        case 0x4A40U:  // STA $2E2E,Y
            store(0x4A40U, indexed_y(0x2E2EU), registers.accumulator);
            append_checkpoint(0x4A40U, 0x4A43U, 15U);
            pc = 0x4A43U;
            break;

        case 0x4A43U:  // BNE $4A8F -- always taken, A is $FF
            pc = branch(
                0x4A43U,
                0x4A8FU,
                0x4A45U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A45U:  // CPY #$04
            compare(registers.y, 0x04U);
            append_checkpoint(0x4A45U, 0x4A47U, 6U);
            pc = 0x4A47U;
            break;

        case 0x4A47U:  // BCS $4A34
            pc = branch(
                0x4A47U,
                0x4A34U,
                0x4A49U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4A49U:  // LDY $2EAA,X
            registers.y = read_ram(indexed_x(0x2EAAU));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4A49U, 0x4A4CU, 15U);
            pc = 0x4A4CU;
            break;

        case 0x4A4CU:  // LDA $2E22,Y
            load_accumulator(read_ram(indexed_y(0x2E22U)));
            append_checkpoint(0x4A4CU, 0x4A4FU, 15U);
            pc = 0x4A4FU;
            break;

        case 0x4A4FU:  // BNE $4A54
            pc = branch(
                0x4A4FU,
                0x4A54U,
                0x4A51U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4A51U:  // JSR $4A34
            push(0x4A51U, 0x4AU);
            push(0x4A51U, 0x53U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4A51U, 0x4A34U, 21U);
            pc = 0x4A34U;
            break;

        case 0x4A54U:  // LDY $2E82,X
            registers.y = read_ram(indexed_x(0x2E82U));
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4A54U, 0x4A57U, 15U);
            pc = 0x4A57U;
            break;

        case 0x4A57U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4A57U, 0x4A5AU, 15U);
            pc = 0x4A5AU;
            break;

        case 0x4A5AU:  // AND #$01
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            append_checkpoint(0x4A5AU, 0x4A5CU, 6U);
            pc = 0x4A5CU;
            break;

        case 0x4A5CU:  // BEQ $4A9B
            pc = branch(
                0x4A5CU,
                0x4A9BU,
                0x4A5EU,
                (registers.status & kZero) != 0U);
            break;

        case 0x4A5EU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4A5EU, 0x4A61U, 15U);
            pc = 0x4A61U;
            break;

        case 0x4A61U:  // AND #$01
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            append_checkpoint(0x4A61U, 0x4A63U, 6U);
            pc = 0x4A63U;
            break;

        case 0x4A63U:  // BEQ $4A6B
            // The not-taken arm at 4A65 calls back into 4A6B and then jumps
            // to $4A9C, outside this slice, so it is left to the core.
            if ((registers.status & kZero) == 0U) {
                return std::nullopt;
            }
            pc = branch(0x4A63U, 0x4A6BU, 0x4A65U, true);
            break;

        case 0x4A6BU:  // CPY #$04
            compare(registers.y, 0x04U);
            append_checkpoint(0x4A6BU, 0x4A6DU, 6U);
            pc = 0x4A6DU;
            break;

        case 0x4A6DU:  // BCC $4A73
            pc = branch(
                0x4A6DU,
                0x4A73U,
                0x4A6FU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4A6FU:  // CLA
            registers.accumulator = 0x00U;
            append_checkpoint(0x4A6FU, 0x4A70U, 6U);
            pc = 0x4A70U;
            break;

        case 0x4A70U:  // STA $2E34,Y
            store(0x4A70U, indexed_y(0x2E34U), registers.accumulator);
            append_checkpoint(0x4A70U, 0x4A73U, 15U);
            pc = 0x4A73U;
            break;

        case 0x4A73U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4A73U, 0x4A76U, 15U);
            pc = 0x4A76U;
            break;

        case 0x4A76U:  // STA $2E16,Y
            store(0x4A76U, indexed_y(0x2E16U), registers.accumulator);
            append_checkpoint(0x4A76U, 0x4A79U, 15U);
            pc = 0x4A79U;
            break;

        case 0x4A79U:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x4A79U, 0x4A7CU, 15U);
            pc = 0x4A7CU;
            break;

        case 0x4A7CU:  // STA $2E1C,Y
            store(0x4A7CU, indexed_y(0x2E1CU), registers.accumulator);
            append_checkpoint(0x4A7CU, 0x4A7FU, 15U);
            pc = 0x4A7FU;
            break;

        case 0x4A7FU:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4A7FU, 0x4A82U, 15U);
            pc = 0x4A82U;
            break;

        case 0x4A82U:  // BPL $4A89
            pc = branch(
                0x4A82U,
                0x4A89U,
                0x4A84U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4A84U:  // LDA $2FB8,X
            load_accumulator(read_ram(indexed_x(0x2FB8U)));
            append_checkpoint(0x4A84U, 0x4A87U, 15U);
            pc = 0x4A87U;
            break;

        case 0x4A87U:  // BRA $4A8C
            pc = branch(0x4A87U, 0x4A8CU, 0x4A89U, true);
            break;

        case 0x4A89U:  // LDA $2EA0,X
            load_accumulator(read_ram(indexed_x(0x2EA0U)));
            append_checkpoint(0x4A89U, 0x4A8CU, 15U);
            pc = 0x4A8CU;
            break;

        case 0x4A8CU:  // STA $2E2E,Y
            store(0x4A8CU, indexed_y(0x2E2EU), registers.accumulator);
            append_checkpoint(0x4A8CU, 0x4A8FU, 15U);
            pc = 0x4A8FU;
            break;

        case 0x4A8FU:  // LDA $2E37
            load_accumulator(read_ram(0x2E37U));
            append_checkpoint(0x4A8FU, 0x4A92U, 15U);
            pc = 0x4A92U;
            break;

        case 0x4A92U:  // STA $2E22,Y
            store(0x4A92U, indexed_y(0x2E22U), registers.accumulator);
            append_checkpoint(0x4A92U, 0x4A95U, 15U);
            pc = 0x4A95U;
            break;

        case 0x4A95U:  // LDA $2FAE,X
            load_accumulator(read_ram(indexed_x(0x2FAEU)));
            append_checkpoint(0x4A95U, 0x4A98U, 15U);
            pc = 0x4A98U;
            break;

        case 0x4A98U:  // STA $2E28,Y
            store(0x4A98U, indexed_y(0x2E28U), registers.accumulator);
            append_checkpoint(0x4A98U, 0x4A9BU, 15U);
            pc = 0x4A9BU;
            break;

        // ---- 45:4228  portamento / sweep stepper -------------------------
        // Advances one voice's pitch by its programmed step. $2F86,X selects
        // the mode: below $40 it is a shift count and the voice's 16-bit
        // period at $2F22/$2F2C is scaled by it, at or above $40 it is a
        // signed note offset applied to $2F54,X, which then falls through
        // into the note-to-period tail at $4705.
        case 0x4228U:  // LDA $2F86,X
            load_accumulator(read_ram(indexed_x(0x2F86U)));
            append_checkpoint(0x4228U, 0x422BU, 15U);
            pc = 0x422BU;
            break;

        case 0x422BU:  // CMP #$40
            compare(registers.accumulator, 0x40U);
            append_checkpoint(0x422BU, 0x422DU, 6U);
            pc = 0x422DU;
            break;

        case 0x422DU:  // BCC $4271 -- shift-count mode
            pc = branch(
                0x422DU, 0x4271U, 0x422FU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x422FU:  // AND #$3F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x3FU));
            append_checkpoint(0x422FU, 0x4231U, 6U);
            pc = 0x4231U;
            break;

        case 0x4231U:  // STA $D2
            store(0x4231U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4231U, 0x4233U, 12U);
            pc = 0x4233U;
            break;

        case 0x4233U:  // BIT $D6
            bit_test(read_ram(zp(0xD6U)));
            append_checkpoint(0x4233U, 0x4235U, 12U);
            pc = 0x4235U;
            break;

        case 0x4235U:  // BPL $4244
            pc = branch(
                0x4235U, 0x4244U, 0x4237U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4237U:  // LDA $2F04,X
            load_accumulator(read_ram(indexed_x(0x2F04U)));
            append_checkpoint(0x4237U, 0x423AU, 15U);
            pc = 0x423AU;
            break;

        case 0x423AU:  // BEQ $4240
            pc = branch(
                0x423AU, 0x4240U, 0x423CU,
                (registers.status & kZero) != 0U);
            break;

        case 0x423CU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x423CU, 0x423DU, 6U);
            pc = 0x423DU;
            break;

        case 0x423DU:  // ADC $2E6E,X
            add_with_carry(read_ram(indexed_x(0x2E6EU)));
            append_checkpoint(0x423DU, 0x4240U, 15U);
            pc = 0x4240U;
            break;

        case 0x4240U:  // STA $2F54,X
            store(0x4240U, indexed_x(0x2F54U), registers.accumulator);
            append_checkpoint(0x4240U, 0x4243U, 15U);
            pc = 0x4243U;
            break;

        case 0x4244U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4244U, 0x4247U, 15U);
            pc = 0x4247U;
            break;

        case 0x4247U:  // AND #$40
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x40U));
            append_checkpoint(0x4247U, 0x4249U, 6U);
            pc = 0x4249U;
            break;

        case 0x4249U:  // CMP #$40 -- carry is the direction bit, kept
                       // across the load that follows
            compare(registers.accumulator, 0x40U);
            append_checkpoint(0x4249U, 0x424BU, 6U);
            pc = 0x424BU;
            break;

        case 0x424BU:  // LDA $2F54,X
            load_accumulator(read_ram(indexed_x(0x2F54U)));
            append_checkpoint(0x424BU, 0x424EU, 15U);
            pc = 0x424EU;
            break;

        case 0x424EU:  // BEQ $4243
            pc = branch(
                0x424EU, 0x4243U, 0x4250U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4250U:  // BCS $4258
            pc = branch(
                0x4250U, 0x4258U, 0x4252U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4252U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0x4252U, 0x4253U, 6U);
            pc = 0x4253U;
            break;

        case 0x4253U:  // SBC $D2
            subtract_with_carry(read_ram(zp(0xD2U)));
            append_checkpoint(0x4253U, 0x4255U, 12U);
            pc = 0x4255U;
            break;

        case 0x4255U:  // JMP $425B
            append_checkpoint(0x4255U, 0x425BU, 12U);
            pc = 0x425BU;
            break;

        case 0x4258U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4258U, 0x4259U, 6U);
            pc = 0x4259U;
            break;

        case 0x4259U:  // ADC $D2
            add_with_carry(read_ram(zp(0xD2U)));
            append_checkpoint(0x4259U, 0x425BU, 12U);
            pc = 0x425BU;
            break;

        case 0x425BU:  // BEQ $4267
            pc = branch(
                0x425BU, 0x4267U, 0x425DU,
                (registers.status & kZero) != 0U);
            break;

        case 0x425DU:  // CMP #$79
            compare(registers.accumulator, 0x79U);
            append_checkpoint(0x425DU, 0x425FU, 6U);
            pc = 0x425FU;
            break;

        case 0x425FU:  // BCS $4267 -- off the top of the note table
            pc = branch(
                0x425FU, 0x4267U, 0x4261U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4261U:  // STA $2F54,X
            store(0x4261U, indexed_x(0x2F54U), registers.accumulator);
            append_checkpoint(0x4261U, 0x4264U, 15U);
            pc = 0x4264U;
            break;

        case 0x4264U:  // JMP $4705 -- into the shared note-to-period tail
            append_checkpoint(0x4264U, 0x4705U, 12U);
            pc = 0x4705U;
            break;

        case 0x4267U:  // STZ $2F54,X
            store(0x4267U, indexed_x(0x2F54U), 0x00U);
            append_checkpoint(0x4267U, 0x426AU, 15U);
            pc = 0x426AU;
            break;

        case 0x426AU:  // STZ $2F22,X
            store(0x426AU, indexed_x(0x2F22U), 0x00U);
            append_checkpoint(0x426AU, 0x426DU, 15U);
            pc = 0x426DU;
            break;

        case 0x426DU:  // STZ $2F2C,X
            store(0x426DU, indexed_x(0x2F2CU), 0x00U);
            append_checkpoint(0x426DU, 0x4270U, 15U);
            pc = 0x4270U;
            break;

        case 0x4271U:  // TAY -- the shift count
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4271U, 0x4272U, 6U);
            pc = 0x4272U;
            break;

        case 0x4272U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4272U, 0x4275U, 15U);
            pc = 0x4275U;
            break;

        case 0x4275U:  // STA $D2
            store(0x4275U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4275U, 0x4277U, 12U);
            pc = 0x4277U;
            break;

        case 0x4277U:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x4277U, 0x427AU, 15U);
            pc = 0x427AU;
            break;

        case 0x427AU: {  // LSR A -- top half of the 16-bit shift
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator >> 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x427AU, 0x427BU, 6U);
            pc = 0x427BU;
            break;
        }

        case 0x427BU: {  // ROR $D2 -- bottom half, carry in from the LSR
            const auto operand = read_ram(zp(0xD2U));
            const auto carry_out = static_cast<std::uint8_t>(operand & 0x01U);
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(operand >> 1U)
                | static_cast<std::uint8_t>(
                    (registers.status & kCarry) << 7U));
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(status | carry_out);
            registers.status = set_nz(status, result);
            store(0x427BU, zp(0xD2U), result);
            append_checkpoint(0x427BU, 0x427DU, 18U);
            pc = 0x427DU;
            break;
        }

        case 0x427DU:  // DEY
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x427DU, 0x427EU, 6U);
            pc = 0x427EU;
            break;

        case 0x427EU:  // BNE $427A
            pc = branch(
                0x427EU, 0x427AU, 0x4280U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4280U:  // STA $D3
            store(0x4280U, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x4280U, 0x4282U, 12U);
            pc = 0x4282U;
            break;

        case 0x4282U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4282U, 0x4285U, 15U);
            pc = 0x4285U;
            break;

        case 0x4285U:  // AND #$40 -- sweep direction
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x40U));
            append_checkpoint(0x4285U, 0x4287U, 6U);
            pc = 0x4287U;
            break;

        case 0x4287U:  // BNE $429F -- downward
            pc = branch(
                0x4287U, 0x429FU, 0x4289U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4289U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4289U, 0x428CU, 15U);
            pc = 0x428CU;
            break;

        case 0x428CU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x428CU, 0x428DU, 6U);
            pc = 0x428DU;
            break;

        case 0x428DU:  // ADC $D2
            add_with_carry(read_ram(zp(0xD2U)));
            append_checkpoint(0x428DU, 0x428FU, 12U);
            pc = 0x428FU;
            break;

        case 0x428FU:  // STA $2F22,X
            store(0x428FU, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x428FU, 0x4292U, 15U);
            pc = 0x4292U;
            break;

        case 0x4292U:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x4292U, 0x4295U, 15U);
            pc = 0x4295U;
            break;

        case 0x4295U:  // ADC $D3
            add_with_carry(read_ram(zp(0xD3U)));
            append_checkpoint(0x4295U, 0x4297U, 12U);
            pc = 0x4297U;
            break;

        case 0x4297U:  // STA $2F2C,X
            store(0x4297U, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x4297U, 0x429AU, 15U);
            pc = 0x429AU;
            break;

        case 0x429AU:  // CMP #$10 -- period ceiling
            compare(registers.accumulator, 0x10U);
            append_checkpoint(0x429AU, 0x429CU, 6U);
            pc = 0x429CU;
            break;

        case 0x429CU:  // BCS $42B3
            pc = branch(
                0x429CU, 0x42B3U, 0x429EU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x429FU:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x429FU, 0x42A2U, 15U);
            pc = 0x42A2U;
            break;

        case 0x42A2U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0x42A2U, 0x42A3U, 6U);
            pc = 0x42A3U;
            break;

        case 0x42A3U:  // SBC $D2
            subtract_with_carry(read_ram(zp(0xD2U)));
            append_checkpoint(0x42A3U, 0x42A5U, 12U);
            pc = 0x42A5U;
            break;

        case 0x42A5U:  // STA $2F22,X
            store(0x42A5U, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x42A5U, 0x42A8U, 15U);
            pc = 0x42A8U;
            break;

        case 0x42A8U:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x42A8U, 0x42ABU, 15U);
            pc = 0x42ABU;
            break;

        case 0x42ABU:  // SBC $D3
            subtract_with_carry(read_ram(zp(0xD3U)));
            append_checkpoint(0x42ABU, 0x42ADU, 12U);
            pc = 0x42ADU;
            break;

        case 0x42ADU:  // STA $2F2C,X
            store(0x42ADU, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x42ADU, 0x42B0U, 15U);
            pc = 0x42B0U;
            break;

        case 0x42B0U:  // BCC $42B3 -- underflowed past zero
            pc = branch(
                0x42B0U, 0x42B3U, 0x42B2U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x42B3U:  // STZ $2F22,X
            store(0x42B3U, indexed_x(0x2F22U), 0x00U);
            append_checkpoint(0x42B3U, 0x42B6U, 15U);
            pc = 0x42B6U;
            break;

        case 0x42B6U:  // STZ $2F2C,X
            store(0x42B6U, indexed_x(0x2F2CU), 0x00U);
            append_checkpoint(0x42B6U, 0x42B9U, 15U);
            pc = 0x42B9U;
            break;

        // ---- 45:457A  command interpreter --------------------------------
        // Reads one voice's sequence stream at ($D0),Y until it reaches a
        // note. Bytes under $80 are notes, $80-$BF are commands dispatched
        // through the table at $42BA, and $C0-$DD select an instrument from
        // the record at $5AA9. The note itself falls through to $4622, which
        // sets the voice's duration and calls the note-to-period conversion
        // at $46D7.
        case 0x457AU:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x457AU, 0x457DU, 15U);
            pc = 0x457DU;
            break;

        case 0x457DU:  // AND #$FD -- clear the "note started" flag
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xFDU));
            append_checkpoint(0x457DU, 0x457FU, 6U);
            pc = 0x457FU;
            break;

        case 0x457FU:  // STA $2EBE,X
            store(0x457FU, indexed_x(0x2EBEU), registers.accumulator);
            append_checkpoint(0x457FU, 0x4582U, 15U);
            pc = 0x4582U;
            break;

        case 0x4582U:  // LDA $2E8C,X
            load_accumulator(read_ram(indexed_x(0x2E8CU)));
            append_checkpoint(0x4582U, 0x4585U, 15U);
            pc = 0x4585U;
            break;

        case 0x4585U:  // STA $D0
            store(0x4585U, zp(0xD0U), registers.accumulator);
            append_checkpoint(0x4585U, 0x4587U, 12U);
            pc = 0x4587U;
            break;

        case 0x4587U:  // LDA $2E96,X
            load_accumulator(read_ram(indexed_x(0x2E96U)));
            append_checkpoint(0x4587U, 0x458AU, 15U);
            pc = 0x458AU;
            break;

        case 0x458AU:  // STA $D1 -- the cursor is now in ($D0)
            store(0x458AU, zp(0xD1U), registers.accumulator);
            append_checkpoint(0x458AU, 0x458CU, 12U);
            pc = 0x458CU;
            break;

        case 0x458CU:  // CLY
            registers.y = 0x00U;
            append_checkpoint(0x458CU, 0x458DU, 6U);
            pc = 0x458DU;
            break;

        case 0x458DU:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x458DU, 0x458FU, 21U);
            pc = 0x458FU;
            break;

        case 0x458FU:  // BMI $4594 -- $80 and up is not a note
            pc = branch(
                0x458FU, 0x4594U, 0x4591U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x4591U:  // JMP $4622
            append_checkpoint(0x4591U, 0x4622U, 12U);
            pc = 0x4622U;
            break;

        case 0x4594U:  // CMP #$C0
            compare(registers.accumulator, 0xC0U);
            append_checkpoint(0x4594U, 0x4596U, 6U);
            pc = 0x4596U;
            break;

        case 0x4596U:  // BCS $45A1 -- $C0 and up selects an instrument
            pc = branch(
                0x4596U, 0x45A1U, 0x4598U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4598U:  // JSR $4569 -- dispatch the command
            push(0x4598U, 0x45U);
            push(0x4598U, 0x9AU);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4598U, 0x4569U, 21U);
            pc = 0x4569U;
            break;

        case 0x459BU: {  // LDX $2E34 -- handlers may have clobbered X
            const auto value = read_ram(0x2E34U);
            registers.x = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x459BU, 0x459EU, 15U);
            pc = 0x459EU;
            break;
        }

        case 0x459EU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x459EU, 0x459FU, 6U);
            pc = 0x459FU;
            break;

        case 0x459FU:  // BPL $458D -- keep reading commands
            pc = branch(
                0x459FU, 0x458DU, 0x45A1U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x45A1U:  // CMP #$DE
            compare(registers.accumulator, 0xDEU);
            append_checkpoint(0x45A1U, 0x45A3U, 6U);
            pc = 0x45A3U;
            break;

        case 0x45A3U:  // BCS $4618
            pc = branch(
                0x45A3U, 0x4618U, 0x45A5U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x45A5U:  // SBC #$BF -- instrument index, one-based
            subtract_with_carry(0xBFU);
            append_checkpoint(0x45A5U, 0x45A7U, 6U);
            pc = 0x45A7U;
            break;

        case 0x45A7U:
        case 0x45A8U:
        case 0x45A9U: {  // ASL A x3 -- eight bytes per instrument record
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            registers.status = set_nz(status, registers.accumulator);
            const auto shift_pc = pc;
            append_checkpoint(
                shift_pc, static_cast<std::uint16_t>(shift_pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(shift_pc + 1U);
            break;
        }

        case 0x45AAU:  // STY $2E35 -- park the cursor across the record read
            store(0x45AAU, 0x2E35U, registers.y);
            append_checkpoint(0x45AAU, 0x45ADU, 15U);
            pc = 0x45ADU;
            break;

        case 0x45ADU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x45ADU, 0x45AEU, 6U);
            pc = 0x45AEU;
            break;

        case 0x45AEU:  // LDA $5AA9,Y -- instrument number
            load_accumulator(read_mapped(indexed_y(0x5AA9U)));
            append_checkpoint(0x45AEU, 0x45B1U, 15U);
            pc = 0x45B1U;
            break;

        case 0x45B1U:  // STA $2EA0,X
            store(0x45B1U, indexed_x(0x2EA0U), registers.accumulator);
            append_checkpoint(0x45B1U, 0x45B4U, 15U);
            pc = 0x45B4U;
            break;

        case 0x45B4U:  // LDA $2E5A,X
            load_accumulator(read_ram(indexed_x(0x2E5AU)));
            append_checkpoint(0x45B4U, 0x45B7U, 15U);
            pc = 0x45B7U;
            break;

        case 0x45B7U:  // BMI $45BF -- the voice's level is already forced
            pc = branch(
                0x45B7U, 0x45BFU, 0x45B9U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x45B9U:  // LDA $5AAA,Y
            load_accumulator(read_mapped(indexed_y(0x5AAAU)));
            append_checkpoint(0x45B9U, 0x45BCU, 15U);
            pc = 0x45BCU;
            break;

        case 0x45BCU:  // STA $2E5A,X
            store(0x45BCU, indexed_x(0x2E5AU), registers.accumulator);
            append_checkpoint(0x45BCU, 0x45BFU, 15U);
            pc = 0x45BFU;
            break;

        case 0x45BFU:  // LDA $5AAC,Y
            load_accumulator(read_mapped(indexed_y(0x5AACU)));
            append_checkpoint(0x45BFU, 0x45C2U, 15U);
            pc = 0x45C2U;
            break;

        case 0x45C2U:  // AND #$1F -- the detune field
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x1FU));
            append_checkpoint(0x45C2U, 0x45C4U, 6U);
            pc = 0x45C4U;
            break;

        case 0x45C4U:  // STA $2F68,X
            store(0x45C4U, indexed_x(0x2F68U), registers.accumulator);
            append_checkpoint(0x45C4U, 0x45C7U, 15U);
            pc = 0x45C7U;
            break;

        case 0x45C7U:  // LDA $5AAC,Y
            load_accumulator(read_mapped(indexed_y(0x5AACU)));
            append_checkpoint(0x45C7U, 0x45CAU, 15U);
            pc = 0x45CAU;
            break;

        case 0x45CAU:  // PHP -- parks the record byte's sign
            push(
                0x45CAU,
                static_cast<std::uint8_t>(registers.status | 0x10U));
            append_checkpoint(0x45CAU, 0x45CBU, 9U);
            pc = 0x45CBU;
            break;

        case 0x45CBU:
        case 0x45CCU:
        case 0x45CDU:
        case 0x45CEU: {  // ROL A x4 -- rotate the mode field down
            const auto carry_out = static_cast<std::uint8_t>(
                registers.accumulator >> 7U);
            const auto result = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(registers.accumulator << 1U)
                | static_cast<std::uint8_t>(registers.status & kCarry));
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(status | carry_out);
            registers.accumulator = result;
            registers.status = set_nz(status, result);
            const auto rotate_pc = pc;
            append_checkpoint(
                rotate_pc, static_cast<std::uint16_t>(rotate_pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(rotate_pc + 1U);
            break;
        }

        case 0x45CFU:  // AND #$03
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x03U));
            append_checkpoint(0x45CFU, 0x45D1U, 6U);
            pc = 0x45D1U;
            break;

        case 0x45D1U:  // BNE $45D5
            pc = branch(
                0x45D1U, 0x45D5U, 0x45D3U,
                (registers.status & kZero) == 0U);
            break;

        case 0x45D3U:  // LDA #$81 -- mode 0 means the default waveform
            load_accumulator(0x81U);
            append_checkpoint(0x45D3U, 0x45D5U, 6U);
            pc = 0x45D5U;
            break;

        case 0x45D5U:  // STA $D2
            store(0x45D5U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x45D5U, 0x45D7U, 12U);
            pc = 0x45D7U;
            break;

        case 0x45D7U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x45D7U, 0x45DAU, 15U);
            pc = 0x45DAU;
            break;

        case 0x45DAU:  // AND #$7C
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x7CU));
            append_checkpoint(0x45DAU, 0x45DCU, 6U);
            pc = 0x45DCU;
            break;

        case 0x45DCU:  // ORA $D2
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0xD2U))));
            append_checkpoint(0x45DCU, 0x45DEU, 12U);
            pc = 0x45DEU;
            break;

        case 0x45DEU:  // STA $2E46,X
            store(0x45DEU, indexed_x(0x2E46U), registers.accumulator);
            append_checkpoint(0x45DEU, 0x45E1U, 15U);
            pc = 0x45E1U;
            break;

        case 0x45E1U:  // LDA $5AAB,Y
            load_accumulator(read_mapped(indexed_y(0x5AABU)));
            append_checkpoint(0x45E1U, 0x45E4U, 15U);
            pc = 0x45E4U;
            break;

        case 0x45E4U:  // PLP -- brings back the record byte's sign
            pull_status();
            append_checkpoint(0x45E4U, 0x45E5U, 12U);
            pc = 0x45E5U;
            break;

        case 0x45E5U:  // BMI $45F4
            pc = branch(
                0x45E5U, 0x45F4U, 0x45E7U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x45E7U:  // STA $2E64,X
            store(0x45E7U, indexed_x(0x2E64U), registers.accumulator);
            append_checkpoint(0x45E7U, 0x45EAU, 15U);
            pc = 0x45EAU;
            break;

        case 0x45EAU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x45EAU, 0x45EDU, 15U);
            pc = 0x45EDU;
            break;

        case 0x45EDU:  // AND #$7F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU));
            append_checkpoint(0x45EDU, 0x45EFU, 6U);
            pc = 0x45EFU;
            break;

        case 0x45EFU:  // STA $2EB4,X
            store(0x45EFU, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x45EFU, 0x45F2U, 15U);
            pc = 0x45F2U;
            break;

        case 0x45F2U:  // BRA $45FC
            append_checkpoint(0x45F2U, 0x45FCU, 12U);
            pc = 0x45FCU;
            break;

        case 0x45F4U:  // STZ $2E64,X
            store(0x45F4U, indexed_x(0x2E64U), 0x00U);
            append_checkpoint(0x45F4U, 0x45F7U, 15U);
            pc = 0x45F7U;
            break;

        case 0x45F7U:  // AND #$FF -- flags only
            load_accumulator(registers.accumulator);
            append_checkpoint(0x45F7U, 0x45F9U, 6U);
            pc = 0x45F9U;
            break;

        case 0x45F9U:  // JSR $4440 -- reuse the sweep-arm command handler
            push(0x45F9U, 0x45U);
            push(0x45F9U, 0xFBU);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x45F9U, 0x4440U, 21U);
            pc = 0x4440U;
            break;

        case 0x45FCU:  // STA $2E64,X
            store(0x45FCU, indexed_x(0x2E64U), registers.accumulator);
            append_checkpoint(0x45FCU, 0x45FFU, 15U);
            pc = 0x45FFU;
            break;

        case 0x45FFU:  // LDA $5AB0,Y
            load_accumulator(read_mapped(indexed_y(0x5AB0U)));
            append_checkpoint(0x45FFU, 0x4602U, 15U);
            pc = 0x4602U;
            break;

        case 0x4602U:  // BEQ $4607
            pc = branch(
                0x4602U, 0x4607U, 0x4604U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4604U:  // JSR $4522 -- program the balance
            push(0x4604U, 0x46U);
            push(0x4604U, 0x06U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4604U, 0x4522U, 21U);
            pc = 0x4522U;
            break;

        case 0x4607U:  // LDA $5AAD,Y
            load_accumulator(read_mapped(indexed_y(0x5AADU)));
            append_checkpoint(0x4607U, 0x460AU, 15U);
            pc = 0x460AU;
            break;

        case 0x460AU:  // BNE $461F
            pc = branch(
                0x460AU, 0x461FU, 0x460CU,
                (registers.status & kZero) == 0U);
            break;

        case 0x460CU:  // STA $D2
            store(0x460CU, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x460CU, 0x460EU, 12U);
            pc = 0x460EU;
            break;

        case 0x460EU: {  // LDY $2E35
            const auto value = read_ram(0x2E35U);
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x460EU, 0x4611U, 15U);
            pc = 0x4611U;
            break;
        }

        case 0x4611U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4611U, 0x4612U, 6U);
            pc = 0x4612U;
            break;

        case 0x4612U:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4612U, 0x4614U, 21U);
            pc = 0x4614U;
            break;

        case 0x4614U:  // BPL $4622
            pc = branch(
                0x4614U, 0x4622U, 0x4616U,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4616U:  // BMI $4625
            pc = branch(
                0x4616U, 0x4625U, 0x4618U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x4618U:  // LDA $2F04,X
            load_accumulator(read_ram(indexed_x(0x2F04U)));
            append_checkpoint(0x4618U, 0x461BU, 15U);
            pc = 0x461BU;
            break;

        case 0x461BU:  // PHP
            push(
                0x461BU,
                static_cast<std::uint8_t>(registers.status | 0x10U));
            append_checkpoint(0x461BU, 0x461CU, 9U);
            pc = 0x461CU;
            break;

        case 0x461CU:  // JMP $462E
            append_checkpoint(0x461CU, 0x462EU, 12U);
            pc = 0x462EU;
            break;

        case 0x461FU: {  // LDY $2E35
            const auto value = read_ram(0x2E35U);
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x461FU, 0x4622U, 15U);
            pc = 0x4622U;
            break;
        }

        // ---- 45:4622  note commit ----------------------------------------
        case 0x4622U:  // STA $D2 -- the note byte
            store(0x4622U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4622U, 0x4624U, 12U);
            pc = 0x4624U;
            break;

        case 0x4624U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4624U, 0x4625U, 6U);
            pc = 0x4625U;
            break;

        case 0x4625U:  // LDA $2F04,X -- the note that was already sounding
            load_accumulator(read_ram(indexed_x(0x2F04U)));
            append_checkpoint(0x4625U, 0x4628U, 15U);
            pc = 0x4628U;
            break;

        case 0x4628U:  // PHP -- parks "was the voice silent?"
            push(
                0x4628U,
                static_cast<std::uint8_t>(registers.status | 0x10U));
            append_checkpoint(0x4628U, 0x4629U, 9U);
            pc = 0x4629U;
            break;

        case 0x4629U:  // LDA $D2
            load_accumulator(read_ram(zp(0xD2U)));
            append_checkpoint(0x4629U, 0x462BU, 12U);
            pc = 0x462BU;
            break;

        case 0x462BU:  // STA $2F04,X
            store(0x462BU, indexed_x(0x2F04U), registers.accumulator);
            append_checkpoint(0x462BU, 0x462EU, 15U);
            pc = 0x462EU;
            break;

        case 0x462EU:  // BEQ $4664 -- a rest
            pc = branch(
                0x462EU, 0x4664U, 0x4630U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4630U:  // PHA
            push(0x4630U, registers.accumulator);
            append_checkpoint(0x4630U, 0x4631U, 9U);
            pc = 0x4631U;
            break;

        case 0x4631U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x4631U, 0x4634U, 15U);
            pc = 0x4634U;
            break;

        case 0x4634U:  // AND #$01 -- is this voice slurring?
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            append_checkpoint(0x4634U, 0x4636U, 6U);
            pc = 0x4636U;
            break;

        case 0x4636U:  // BEQ $4663
            pc = branch(
                0x4636U, 0x4663U, 0x4638U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4638U:  // LDA $2F22,X -- keep the period being slurred from
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4638U, 0x463BU, 15U);
            pc = 0x463BU;
            break;

        case 0x463BU:  // STA $D2
            store(0x463BU, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x463BU, 0x463DU, 12U);
            pc = 0x463DU;
            break;

        case 0x463DU:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x463DU, 0x4640U, 15U);
            pc = 0x4640U;
            break;

        case 0x4640U:  // STA $D3
            store(0x4640U, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x4640U, 0x4642U, 12U);
            pc = 0x4642U;
            break;

        case 0x4642U:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x4642U, 0x4643U, 12U);
            pc = 0x4643U;
            break;

        case 0x4643U:  // JSR $46D7
            push(0x4643U, 0x46U);
            push(0x4643U, 0x45U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4643U, 0x46D7U, 21U);
            pc = 0x46D7U;
            break;

        case 0x4646U:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x4646U, 0x4649U, 15U);
            pc = 0x4649U;
            break;

        case 0x4649U:  // STA $2F5E,X -- the slur's destination period
            store(0x4649U, indexed_x(0x2F5EU), registers.accumulator);
            append_checkpoint(0x4649U, 0x464CU, 15U);
            pc = 0x464CU;
            break;

        case 0x464CU:  // LDA $2F2C,X
            load_accumulator(read_ram(indexed_x(0x2F2CU)));
            append_checkpoint(0x464CU, 0x464FU, 15U);
            pc = 0x464FU;
            break;

        case 0x464FU:  // STA $2F68,X
            store(0x464FU, indexed_x(0x2F68U), registers.accumulator);
            append_checkpoint(0x464FU, 0x4652U, 15U);
            pc = 0x4652U;
            break;

        case 0x4652U:  // PLP
            pull_status();
            append_checkpoint(0x4652U, 0x4653U, 12U);
            pc = 0x4653U;
            break;

        case 0x4653U:  // PHP
            push(
                0x4653U,
                static_cast<std::uint8_t>(registers.status | 0x10U));
            append_checkpoint(0x4653U, 0x4654U, 9U);
            pc = 0x4654U;
            break;

        case 0x4654U:  // BEQ $4667
            pc = branch(
                0x4654U, 0x4667U, 0x4656U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4656U:  // LDA $D2 -- restore the period to slur from
            load_accumulator(read_ram(zp(0xD2U)));
            append_checkpoint(0x4656U, 0x4658U, 12U);
            pc = 0x4658U;
            break;

        case 0x4658U:  // STA $2F22,X
            store(0x4658U, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x4658U, 0x465BU, 15U);
            pc = 0x465BU;
            break;

        case 0x465BU:  // LDA $D3
            load_accumulator(read_ram(zp(0xD3U)));
            append_checkpoint(0x465BU, 0x465DU, 12U);
            pc = 0x465DU;
            break;

        case 0x465DU:  // STA $2F2C,X
            store(0x465DU, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x465DU, 0x4660U, 15U);
            pc = 0x4660U;
            break;

        case 0x4660U:  // JMP $4667
            append_checkpoint(0x4660U, 0x4667U, 12U);
            pc = 0x4667U;
            break;

        case 0x4663U:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x4663U, 0x4664U, 12U);
            pc = 0x4664U;
            break;

        case 0x4664U:  // JSR $46D7
            push(0x4664U, 0x46U);
            push(0x4664U, 0x66U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4664U, 0x46D7U, 21U);
            pc = 0x46D7U;
            break;

        case 0x4667U:  // LDA ($D0),Y -- the duration byte
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4667U, 0x4669U, 21U);
            pc = 0x4669U;
            break;

        case 0x4669U:  // CMP #$EF
            compare(registers.accumulator, 0xEFU);
            append_checkpoint(0x4669U, 0x466BU, 6U);
            pc = 0x466BU;
            break;

        case 0x466BU:  // BCC $468B
            pc = branch(
                0x466BU, 0x468BU, 0x466DU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x466DU:  // SBC #$EF
            subtract_with_carry(0xEFU);
            append_checkpoint(0x466DU, 0x466FU, 6U);
            pc = 0x466FU;
            break;

        case 0x466FU:  // BNE $4676
            pc = branch(
                0x466FU, 0x4676U, 0x4671U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4671U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4671U, 0x4672U, 6U);
            pc = 0x4672U;
            break;

        case 0x4672U:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4672U, 0x4674U, 21U);
            pc = 0x4674U;
            break;

        case 0x4674U:  // BCS $467C
            pc = branch(
                0x4674U, 0x467CU, 0x4676U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4676U:  // PHY
            push(0x4676U, registers.y);
            append_checkpoint(0x4676U, 0x4677U, 9U);
            pc = 0x4677U;
            break;

        case 0x4677U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4677U, 0x4678U, 6U);
            pc = 0x4678U;
            break;

        case 0x4678U:  // LDA $5A98,Y -- the duration table
            load_accumulator(read_mapped(indexed_y(0x5A98U)));
            append_checkpoint(0x4678U, 0x467BU, 15U);
            pc = 0x467BU;
            break;

        case 0x467BU:  // PLY
            registers.y = pull();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x467BU, 0x467CU, 12U);
            pc = 0x467CU;
            break;

        case 0x467CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x467CU, 0x467DU, 6U);
            pc = 0x467DU;
            break;

        case 0x467DU:  // STA $2EFA,X
            store(0x467DU, indexed_x(0x2EFAU), registers.accumulator);
            append_checkpoint(0x467DU, 0x4680U, 15U);
            pc = 0x4680U;
            break;

        case 0x4680U:  // STA $2ED2,X
            store(0x4680U, indexed_x(0x2ED2U), registers.accumulator);
            append_checkpoint(0x4680U, 0x4683U, 15U);
            pc = 0x4683U;
            break;

        case 0x4683U:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4683U, 0x4685U, 21U);
            pc = 0x4685U;
            break;

        case 0x4685U:  // STA $2F18,X
            store(0x4685U, indexed_x(0x2F18U), registers.accumulator);
            append_checkpoint(0x4685U, 0x4688U, 15U);
            pc = 0x4688U;
            break;

        case 0x4688U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4688U, 0x4689U, 6U);
            pc = 0x4689U;
            break;

        case 0x4689U:  // BNE $46A8
            pc = branch(
                0x4689U, 0x46A8U, 0x468BU,
                (registers.status & kZero) == 0U);
            break;

        case 0x468BU:  // SBC #$DD
            subtract_with_carry(0xDDU);
            append_checkpoint(0x468BU, 0x468DU, 6U);
            pc = 0x468DU;
            break;

        case 0x468DU:  // BEQ $4696 -- an explicit duration follows
            pc = branch(
                0x468DU, 0x4696U, 0x468FU,
                (registers.status & kZero) != 0U);
            break;

        case 0x468FU:  // BCS $469B
            pc = branch(
                0x468FU, 0x469BU, 0x4691U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4691U:  // LDA $2EFA,X -- reuse the last duration
            load_accumulator(read_ram(indexed_x(0x2EFAU)));
            append_checkpoint(0x4691U, 0x4694U, 15U);
            pc = 0x4694U;
            break;

        case 0x4694U:  // BCC $46A5
            pc = branch(
                0x4694U, 0x46A5U, 0x4696U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4696U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4696U, 0x4697U, 6U);
            pc = 0x4697U;
            break;

        case 0x4697U:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4697U, 0x4699U, 21U);
            pc = 0x4699U;
            break;

        case 0x4699U:  // BCS $46A1
            pc = branch(
                0x4699U, 0x46A1U, 0x469BU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x469BU:  // PHY
            push(0x469BU, registers.y);
            append_checkpoint(0x469BU, 0x469CU, 9U);
            pc = 0x469CU;
            break;

        case 0x469CU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x469CU, 0x469DU, 6U);
            pc = 0x469DU;
            break;

        case 0x469DU:  // LDA $5A98,Y
            load_accumulator(read_mapped(indexed_y(0x5A98U)));
            append_checkpoint(0x469DU, 0x46A0U, 15U);
            pc = 0x46A0U;
            break;

        case 0x46A0U:  // PLY
            registers.y = pull();
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x46A0U, 0x46A1U, 12U);
            pc = 0x46A1U;
            break;

        case 0x46A1U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x46A1U, 0x46A2U, 6U);
            pc = 0x46A2U;
            break;

        case 0x46A2U:  // STA $2EFA,X
            store(0x46A2U, indexed_x(0x2EFAU), registers.accumulator);
            append_checkpoint(0x46A2U, 0x46A5U, 15U);
            pc = 0x46A5U;
            break;

        case 0x46A5U:  // STA $2ED2,X -- the note's remaining length
            store(0x46A5U, indexed_x(0x2ED2U), registers.accumulator);
            append_checkpoint(0x46A5U, 0x46A8U, 15U);
            pc = 0x46A8U;
            break;

        case 0x46A8U:  // STZ $2EC8,X
            store(0x46A8U, indexed_x(0x2EC8U), 0x00U);
            append_checkpoint(0x46A8U, 0x46ABU, 15U);
            pc = 0x46ABU;
            break;

        case 0x46ABU:  // STZ $2F7C,X
            store(0x46ABU, indexed_x(0x2F7CU), 0x00U);
            append_checkpoint(0x46ABU, 0x46AEU, 15U);
            pc = 0x46AEU;
            break;

        case 0x46AEU:  // PLP -- was the voice silent before this note?
            pull_status();
            append_checkpoint(0x46AEU, 0x46AFU, 12U);
            pc = 0x46AFU;
            break;

        case 0x46AFU:  // BEQ $46B8
            pc = branch(
                0x46AFU, 0x46B8U, 0x46B1U,
                (registers.status & kZero) != 0U);
            break;

        case 0x46B1U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x46B1U, 0x46B4U, 15U);
            pc = 0x46B4U;
            break;

        case 0x46B4U:  // BIT #$10 -- N and V come from the immediate here
            bit_test(0x10U);
            append_checkpoint(0x46B4U, 0x46B6U, 6U);
            pc = 0x46B6U;
            break;

        case 0x46B6U:  // BNE $46C3
            pc = branch(
                0x46B6U, 0x46C3U, 0x46B8U,
                (registers.status & kZero) == 0U);
            break;

        case 0x46B8U:  // STZ $2F72,X
            store(0x46B8U, indexed_x(0x2F72U), 0x00U);
            append_checkpoint(0x46B8U, 0x46BBU, 15U);
            pc = 0x46BBU;
            break;

        case 0x46BBU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x46BBU, 0x46BEU, 15U);
            pc = 0x46BEU;
            break;

        case 0x46BEU:  // ORA #$04 -- restart the envelope
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x04U));
            append_checkpoint(0x46BEU, 0x46C0U, 6U);
            pc = 0x46C0U;
            break;

        case 0x46C0U:  // STA $2EB4,X
            store(0x46C0U, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x46C0U, 0x46C3U, 15U);
            pc = 0x46C3U;
            break;

        case 0x46C3U:  // LDA $D6
            load_accumulator(read_ram(zp(0xD6U)));
            append_checkpoint(0x46C3U, 0x46C5U, 12U);
            pc = 0x46C5U;
            break;

        case 0x46C5U:  // ORA #$80
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x80U));
            append_checkpoint(0x46C5U, 0x46C7U, 6U);
            pc = 0x46C7U;
            break;

        case 0x46C7U:  // STA $D6
            store(0x46C7U, zp(0xD6U), registers.accumulator);
            append_checkpoint(0x46C7U, 0x46C9U, 12U);
            pc = 0x46C9U;
            break;

        case 0x46C9U:  // TYA
            load_accumulator(registers.y);
            append_checkpoint(0x46C9U, 0x46CAU, 6U);
            pc = 0x46CAU;
            break;

        case 0x46CAU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x46CAU, 0x46CBU, 6U);
            pc = 0x46CBU;
            break;

        case 0x46CBU:  // ADC $D0 -- advance the stored cursor past the note
            add_with_carry(read_ram(zp(0xD0U)));
            append_checkpoint(0x46CBU, 0x46CDU, 12U);
            pc = 0x46CDU;
            break;

        case 0x46CDU:  // STA $2E8C,X
            store(0x46CDU, indexed_x(0x2E8CU), registers.accumulator);
            append_checkpoint(0x46CDU, 0x46D0U, 15U);
            pc = 0x46D0U;
            break;

        case 0x46D0U:  // CLA -- no flags, the carry survives
            registers.accumulator = 0x00U;
            append_checkpoint(0x46D0U, 0x46D1U, 6U);
            pc = 0x46D1U;
            break;

        case 0x46D1U:  // ADC $D1
            add_with_carry(read_ram(zp(0xD1U)));
            append_checkpoint(0x46D1U, 0x46D3U, 12U);
            pc = 0x46D3U;
            break;

        case 0x46D3U:  // STA $2E96,X
            store(0x46D3U, indexed_x(0x2E96U), registers.accumulator);
            append_checkpoint(0x46D3U, 0x46D6U, 15U);
            pc = 0x46D6U;
            break;

        // ---- 45:46D7  note to period -------------------------------------
        case 0x46D7U:  // STA $2F22,X
            store(0x46D7U, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x46D7U, 0x46DAU, 15U);
            pc = 0x46DAU;
            break;

        case 0x46DAU:  // AND #$FF -- flags only
            load_accumulator(registers.accumulator);
            append_checkpoint(0x46DAU, 0x46DCU, 6U);
            pc = 0x46DCU;
            break;

        case 0x46DCU:  // BNE $46E2
            pc = branch(
                0x46DCU, 0x46E2U, 0x46DEU,
                (registers.status & kZero) == 0U);
            break;

        case 0x46DEU:  // STA $2F2C,X -- note zero silences the voice
            store(0x46DEU, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x46DEU, 0x46E1U, 15U);
            pc = 0x46E1U;
            break;

        case 0x46E2U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x46E2U, 0x46E5U, 15U);
            pc = 0x46E5U;
            break;

        case 0x46E5U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x46E5U, 0x46E6U, 6U);
            pc = 0x46E6U;
            break;

        case 0x46E6U:  // BIT #$08 -- noise voices bypass the note table
            bit_test(0x08U);
            append_checkpoint(0x46E6U, 0x46E8U, 6U);
            pc = 0x46E8U;
            break;

        case 0x46E8U:  // BEQ $46FC
            pc = branch(
                0x46E8U, 0x46FCU, 0x46EAU,
                (registers.status & kZero) != 0U);
            break;

        case 0x46EAU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0x46EAU, 0x46ECU, 6U);
            pc = 0x46ECU;
            break;

        case 0x46ECU:  // STA $2F2C,X
            store(0x46ECU, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x46ECU, 0x46EFU, 15U);
            pc = 0x46EFU;
            break;

        case 0x46EFU:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x46EFU, 0x46F2U, 15U);
            pc = 0x46F2U;
            break;

        case 0x46F2U:  // ADC $2E6E,X
            add_with_carry(read_ram(indexed_x(0x2E6EU)));
            append_checkpoint(0x46F2U, 0x46F5U, 15U);
            pc = 0x46F5U;
            break;

        case 0x46F5U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0x46F5U, 0x46F6U, 6U);
            pc = 0x46F6U;
            break;

        case 0x46F6U:  // SBC #$01
            subtract_with_carry(0x01U);
            append_checkpoint(0x46F6U, 0x46F8U, 6U);
            pc = 0x46F8U;
            break;

        case 0x46F8U:  // STA $2F68,X
            store(0x46F8U, indexed_x(0x2F68U), registers.accumulator);
            append_checkpoint(0x46F8U, 0x46FBU, 15U);
            pc = 0x46FBU;
            break;

        case 0x46FCU:  // STY $2E35
            store(0x46FCU, 0x2E35U, registers.y);
            append_checkpoint(0x46FCU, 0x46FFU, 15U);
            pc = 0x46FFU;
            break;

        case 0x46FFU:  // LDA $2F22,X
            load_accumulator(read_ram(indexed_x(0x2F22U)));
            append_checkpoint(0x46FFU, 0x4702U, 15U);
            pc = 0x4702U;
            break;

        case 0x4702U:  // ADC $2E6E,X -- the voice's transpose
            add_with_carry(read_ram(indexed_x(0x2E6EU)));
            append_checkpoint(0x4702U, 0x4705U, 15U);
            pc = 0x4705U;
            break;

        // ---- 45:4569  command dispatch -----------------------------------
        // Pushes an address off the table at $42BA and returns to it, so the
        // handler runs with the JSR's own return address still on the stack
        // and its RTS lands back at $459B. The walk does the same: the RTS
        // at 4579 is a jump, not a return, so it leaves the depth alone.
        case 0x4569U: {  // ASL A
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x4569U, 0x456AU, 6U);
            pc = 0x456AU;
            break;
        }

        case 0x456AU:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x456AU, 0x456BU, 6U);
            pc = 0x456BU;
            break;

        case 0x456BU:  // LDA $42BB,X
            load_accumulator(read_mapped(indexed_x(0x42BBU)));
            append_checkpoint(0x456BU, 0x456EU, 15U);
            pc = 0x456EU;
            break;

        case 0x456EU:  // PHA
            push(0x456EU, registers.accumulator);
            append_checkpoint(0x456EU, 0x456FU, 9U);
            pc = 0x456FU;
            break;

        case 0x456FU:  // LDA $42BA,X
            load_accumulator(read_mapped(indexed_x(0x42BAU)));
            append_checkpoint(0x456FU, 0x4572U, 15U);
            pc = 0x4572U;
            break;

        case 0x4572U:  // PHA
            push(0x4572U, registers.accumulator);
            append_checkpoint(0x4572U, 0x4573U, 9U);
            pc = 0x4573U;
            break;

        case 0x4573U: {  // LDX $2E34
            const auto value = read_ram(0x2E34U);
            registers.x = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4573U, 0x4576U, 15U);
            pc = 0x4576U;
            break;
        }

        case 0x4576U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4576U, 0x4577U, 6U);
            pc = 0x4577U;
            break;

        case 0x4577U:  // LDA ($D0),Y -- the command's operand
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4577U, 0x4579U, 21U);
            pc = 0x4579U;
            break;

        case 0x4579U: {  // RTS -- the dispatch itself; depth is unchanged
            const auto low = pull();
            const auto high = pull();
            const auto handler_pc = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(low)
                 | static_cast<std::uint16_t>(high) << 8U)
                + 1U);
            append_checkpoint(0x4579U, handler_pc, 21U);
            pc = handler_pc;
            break;
        }

        // ---- 45:4705  note-to-period tail --------------------------------
        // Turns the note number in A into the voice's 16-bit period. Reached
        // by falling out of 4228 and, once 45:457A is native, by the command
        // interpreter as well. Percussion voices ($2E46,X negative) first
        // look their note up in an instrument table, and a match against the
        // $5B41 table means a sample: that arm remaps MPR6 to the sample's
        // bank and hands the channel to the timer driver at $4776.
        case 0x4705U: {  // LDY $2E46,X
            const auto value = read_ram(indexed_x(0x2E46U));
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4705U, 0x4708U, 15U);
            pc = 0x4708U;
            break;
        }

        case 0x4708U:  // BPL $4758 -- melodic voices skip the lookup
            pc = branch(
                0x4708U, 0x4758U, 0x470AU,
                (registers.status & kNegative) == 0U);
            break;

        case 0x470AU:  // PHA
            push(0x470AU, registers.accumulator);
            append_checkpoint(0x470AU, 0x470BU, 9U);
            pc = 0x470BU;
            break;

        case 0x470BU:  // LDA $2EA0,X -- instrument number
            load_accumulator(read_ram(indexed_x(0x2EA0U)));
            append_checkpoint(0x470BU, 0x470EU, 15U);
            pc = 0x470EU;
            break;

        case 0x470EU: {  // ASL A -- carry selects the sample table
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x470EU, 0x470FU, 6U);
            pc = 0x470FU;
            break;
        }

        case 0x470FU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x470FU, 0x4710U, 6U);
            pc = 0x4710U;
            break;

        case 0x4710U:  // BCC $4730
            pc = branch(
                0x4710U, 0x4730U, 0x4712U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4712U:  // LDA $5B41,Y
            load_accumulator(read_mapped(indexed_y(0x5B41U)));
            append_checkpoint(0x4712U, 0x4715U, 15U);
            pc = 0x4715U;
            break;

        case 0x4715U:  // STA $D2
            store(0x4715U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4715U, 0x4717U, 12U);
            pc = 0x4717U;
            break;

        case 0x4717U:  // LDA $5B42,Y
            load_accumulator(read_mapped(indexed_y(0x5B42U)));
            append_checkpoint(0x4717U, 0x471AU, 15U);
            pc = 0x471AU;
            break;

        case 0x471AU:  // STA $D3
            store(0x471AU, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x471AU, 0x471CU, 12U);
            pc = 0x471CU;
            break;

        case 0x471CU:  // CLY -- no flags
            registers.y = 0x00U;
            append_checkpoint(0x471CU, 0x471DU, 6U);
            pc = 0x471DU;
            break;

        case 0x471DU:  // TMA #$40 -- save MPR6
            registers.accumulator = registers.mpr[6];
            append_checkpoint(0x471DU, 0x471FU, 12U);
            pc = 0x471FU;
            break;

        case 0x471FU:  // STA $D4
            store(0x471FU, zp(0xD4U), registers.accumulator);
            append_checkpoint(0x471FU, 0x4721U, 12U);
            pc = 0x4721U;
            break;

        case 0x4721U:  // LDA $FC06 -- the sample bank table's first entry
            load_accumulator(read_mapped(0xFC06U));
            append_checkpoint(0x4721U, 0x4724U, 15U);
            pc = 0x4724U;
            break;

        case 0x4724U:  // TAM #$40 -- map the sample bank at $C000
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0x4724U, 0x4726U, 15U);
            pc = 0x4726U;
            break;

        case 0x4726U:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x4726U, 0x4727U, 12U);
            pc = 0x4727U;
            break;

        case 0x4727U:  // CMP ($D2),Y
            compare(registers.accumulator, read_mapped(command_pointer()));
            append_checkpoint(0x4727U, 0x4729U, 21U);
            pc = 0x4729U;
            break;

        case 0x4729U:  // BCS $4776 -- this entry covers the note
            pc = branch(
                0x4729U, 0x4776U, 0x472BU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x472BU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x472BU, 0x472CU, 6U);
            pc = 0x472CU;
            break;

        case 0x472CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x472CU, 0x472DU, 6U);
            pc = 0x472DU;
            break;

        case 0x472DU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x472DU, 0x472EU, 6U);
            pc = 0x472EU;
            break;

        case 0x472EU:  // BNE $4727
            pc = branch(
                0x472EU, 0x4727U, 0x4730U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4730U:  // LDA $4DA9,Y
            load_accumulator(read_mapped(indexed_y(0x4DA9U)));
            append_checkpoint(0x4730U, 0x4733U, 15U);
            pc = 0x4733U;
            break;

        case 0x4733U:  // STA $D2
            store(0x4733U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4733U, 0x4735U, 12U);
            pc = 0x4735U;
            break;

        case 0x4735U:  // LDA $4DAA,Y
            load_accumulator(read_mapped(indexed_y(0x4DAAU)));
            append_checkpoint(0x4735U, 0x4738U, 15U);
            pc = 0x4738U;
            break;

        case 0x4738U:  // STA $D3
            store(0x4738U, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x4738U, 0x473AU, 12U);
            pc = 0x473AU;
            break;

        case 0x473AU:  // CLY
            registers.y = 0x00U;
            append_checkpoint(0x473AU, 0x473BU, 6U);
            pc = 0x473BU;
            break;

        case 0x473BU:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x473BU, 0x473CU, 12U);
            pc = 0x473CU;
            break;

        case 0x473CU:  // CMP ($D2),Y -- scan the instrument's range table
            compare(registers.accumulator, read_mapped(command_pointer()));
            append_checkpoint(0x473CU, 0x473EU, 21U);
            pc = 0x473EU;
            break;

        case 0x473EU:  // BCS $4745
            pc = branch(
                0x473EU, 0x4745U, 0x4740U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4740U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4740U, 0x4741U, 6U);
            pc = 0x4741U;
            break;

        case 0x4741U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4741U, 0x4742U, 6U);
            pc = 0x4742U;
            break;

        case 0x4742U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4742U, 0x4743U, 6U);
            pc = 0x4743U;
            break;

        case 0x4743U:  // BNE $473C
            pc = branch(
                0x4743U, 0x473CU, 0x4745U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4745U:  // PHA
            push(0x4745U, registers.accumulator);
            append_checkpoint(0x4745U, 0x4746U, 9U);
            pc = 0x4746U;
            break;

        case 0x4746U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4746U, 0x4747U, 6U);
            pc = 0x4747U;
            break;

        case 0x4747U:  // LDA ($D2),Y -- waveform for this range
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4747U, 0x4749U, 21U);
            pc = 0x4749U;
            break;

        case 0x4749U:  // STA $2FB8,X
            store(0x4749U, indexed_x(0x2FB8U), registers.accumulator);
            append_checkpoint(0x4749U, 0x474CU, 15U);
            pc = 0x474CU;
            break;

        case 0x474CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x474CU, 0x474DU, 6U);
            pc = 0x474DU;
            break;

        case 0x474DU:  // LDA $2E5A,X
            load_accumulator(read_ram(indexed_x(0x2E5AU)));
            append_checkpoint(0x474DU, 0x4750U, 15U);
            pc = 0x4750U;
            break;

        case 0x4750U:  // BMI $4757 -- the voice already has a level
            pc = branch(
                0x4750U, 0x4757U, 0x4752U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x4752U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4752U, 0x4754U, 21U);
            pc = 0x4754U;
            break;

        case 0x4754U:  // STA $2E5A,X
            store(0x4754U, indexed_x(0x2E5AU), registers.accumulator);
            append_checkpoint(0x4754U, 0x4757U, 15U);
            pc = 0x4757U;
            break;

        case 0x4757U:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x4757U, 0x4758U, 12U);
            pc = 0x4758U;
            break;

        case 0x4758U: {  // ASL A -- note number to table index
            auto status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator >> 7U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            registers.status = set_nz(status, registers.accumulator);
            append_checkpoint(0x4758U, 0x4759U, 6U);
            pc = 0x4759U;
            break;
        }

        case 0x4759U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4759U, 0x475AU, 6U);
            pc = 0x475AU;
            break;

        case 0x475AU:  // LDA $2F90,X -- the signed detune
            load_accumulator(read_ram(indexed_x(0x2F90U)));
            append_checkpoint(0x475AU, 0x475DU, 15U);
            pc = 0x475DU;
            break;

        case 0x475DU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x475DU, 0x475EU, 6U);
            pc = 0x475EU;
            break;

        case 0x475EU:  // PHP -- parks the detune's sign across the add
            push(
                0x475EU,
                static_cast<std::uint8_t>(registers.status | 0x10U));
            append_checkpoint(0x475EU, 0x475FU, 9U);
            pc = 0x475FU;
            break;

        case 0x475FU:  // ADC $4CB7,Y
            add_with_carry(read_mapped(indexed_y(0x4CB7U)));
            append_checkpoint(0x475FU, 0x4762U, 15U);
            pc = 0x4762U;
            break;

        case 0x4762U:  // STA $2F22,X
            store(0x4762U, indexed_x(0x2F22U), registers.accumulator);
            append_checkpoint(0x4762U, 0x4765U, 15U);
            pc = 0x4765U;
            break;

        case 0x4765U:  // LDA $4CB8,Y
            load_accumulator(read_mapped(indexed_y(0x4CB8U)));
            append_checkpoint(0x4765U, 0x4768U, 15U);
            pc = 0x4768U;
            break;

        case 0x4768U:  // ADC #$00
            add_with_carry(0x00U);
            append_checkpoint(0x4768U, 0x476AU, 6U);
            pc = 0x476AU;
            break;

        case 0x476AU:  // PLP -- restores the detune's sign, one step late
            pull_status();
            append_checkpoint(0x476AU, 0x476BU, 12U);
            pc = 0x476BU;
            break;

        case 0x476BU:  // BPL $476F
            pc = branch(
                0x476BU, 0x476FU, 0x476DU,
                (registers.status & kNegative) == 0U);
            break;

        case 0x476DU:  // SBC #$00 -- sign-extend a negative detune
            subtract_with_carry(0x00U);
            append_checkpoint(0x476DU, 0x476FU, 6U);
            pc = 0x476FU;
            break;

        case 0x476FU:  // STA $2F2C,X
            store(0x476FU, indexed_x(0x2F2CU), registers.accumulator);
            append_checkpoint(0x476FU, 0x4772U, 15U);
            pc = 0x4772U;
            break;

        case 0x4772U: {  // LDY $2E35
            const auto value = read_ram(0x2E35U);
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4772U, 0x4775U, 15U);
            pc = 0x4775U;
            break;
        }

        // ---- 45:4776  sample trigger -------------------------------------
        // Hands a channel over to the timer sound driver: copies the sample
        // header out of the remapped bank into the driver's per-channel zero
        // page ($DD/$DE/$E4/$E9/$EA/$EF/$F0/$F5/$F6, indexed by channel),
        // silences the PSG channel while it does, and starts the timer if no
        // channel was streaming yet. The reads of $F5/$F7/$F9 at 4841 are
        // what arms the driver-state guard.
        case 0x4776U:  // PHA
            push(0x4776U, registers.accumulator);
            append_checkpoint(0x4776U, 0x4777U, 9U);
            pc = 0x4777U;
            break;

        case 0x4777U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4777U, 0x4778U, 6U);
            pc = 0x4778U;
            break;

        case 0x4778U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4778U, 0x477AU, 21U);
            pc = 0x477AU;
            break;

        case 0x477AU:  // PHA
            push(0x477AU, registers.accumulator);
            append_checkpoint(0x477AU, 0x477BU, 9U);
            pc = 0x477BU;
            break;

        case 0x477BU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x477BU, 0x477CU, 6U);
            pc = 0x477CU;
            break;

        case 0x477CU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x477CU, 0x477EU, 21U);
            pc = 0x477EU;
            break;

        case 0x477EU:  // STA $D3 -- the sample header pointer's high byte
            store(0x477EU, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x477EU, 0x4780U, 12U);
            pc = 0x4780U;
            break;

        case 0x4780U:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x4780U, 0x4781U, 12U);
            pc = 0x4781U;
            break;

        case 0x4781U:  // STA $D2
            store(0x4781U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4781U, 0x4783U, 12U);
            pc = 0x4783U;
            break;

        case 0x4783U:  // ORA $D3 -- a null pointer means no sample
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0xD3U))));
            append_checkpoint(0x4783U, 0x4785U, 12U);
            pc = 0x4785U;
            break;

        case 0x4785U:  // BEQ $479A
            pc = branch(
                0x4785U, 0x479AU, 0x4787U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4787U: {  // LDY $2E82,X -- the driver channel this voice owns
            const auto value = read_ram(indexed_x(0x2E82U));
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4787U, 0x478AU, 15U);
            pc = 0x478AU;
            break;
        }

        case 0x478AU:  // LDA $2E22,Y
            load_accumulator(read_ram(indexed_y(0x2E22U)));
            append_checkpoint(0x478AU, 0x478DU, 15U);
            pc = 0x478DU;
            break;

        case 0x478DU:  // BNE $479A -- the channel is locked
            pc = branch(
                0x478DU, 0x479AU, 0x478FU,
                (registers.status & kZero) == 0U);
            break;

        case 0x478FU:  // LDA $20F5,Y -- the driver's channel bank byte
            load_accumulator(read_ram(indexed_y(0x20F5U)));
            append_checkpoint(0x478FU, 0x4792U, 15U);
            pc = 0x4792U;
            break;

        case 0x4792U:  // BEQ $479E -- channel idle, take it
            pc = branch(
                0x4792U, 0x479EU, 0x4794U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4794U:  // TXA
            load_accumulator(registers.x);
            append_checkpoint(0x4794U, 0x4795U, 6U);
            pc = 0x4795U;
            break;

        case 0x4795U:  // CMP $2E04,Y -- priority against the current owner
            compare(registers.accumulator, read_ram(indexed_y(0x2E04U)));
            append_checkpoint(0x4795U, 0x4798U, 15U);
            pc = 0x4798U;
            break;

        case 0x4798U:  // BCS $479F
            pc = branch(
                0x4798U, 0x479FU, 0x479AU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x479AU:  // PLA -- give up, unwind to the MPR6 restore
            load_accumulator(pull());
            append_checkpoint(0x479AU, 0x479BU, 12U);
            pc = 0x479BU;
            break;

        case 0x479BU:  // JMP $4873
            append_checkpoint(0x479BU, 0x4873U, 12U);
            pc = 0x4873U;
            break;

        case 0x479EU:  // TXA
            load_accumulator(registers.x);
            append_checkpoint(0x479EU, 0x479FU, 6U);
            pc = 0x479FU;
            break;

        case 0x479FU: {  // TST #$40, $2E46,X
            const auto operand = read_ram(indexed_x(0x2E46U));
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(
                    ~(kZero | kOverflow | kNegative)));
            if ((operand & 0x40U) == 0U) {
                status = static_cast<std::uint8_t>(status | kZero);
            }
            registers.status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(
                    operand & (kOverflow | kNegative)));
            append_checkpoint(0x479FU, 0x47A3U, 24U);
            pc = 0x47A3U;
            break;
        }

        case 0x47A3U:  // BEQ $47A7
            pc = branch(
                0x47A3U, 0x47A7U, 0x47A5U,
                (registers.status & kZero) != 0U);
            break;

        case 0x47A5U:  // ORA #$10
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x10U));
            append_checkpoint(0x47A5U, 0x47A7U, 6U);
            pc = 0x47A7U;
            break;

        case 0x47A7U:  // STA $2E04,Y -- claim the channel
            store(0x47A7U, indexed_y(0x2E04U), registers.accumulator);
            append_checkpoint(0x47A7U, 0x47AAU, 15U);
            pc = 0x47AAU;
            break;

        case 0x47AAU:  // LDA $2F0E,X
            load_accumulator(read_ram(indexed_x(0x2F0EU)));
            append_checkpoint(0x47AAU, 0x47ADU, 15U);
            pc = 0x47ADU;
            break;

        case 0x47ADU:  // STA $2E05,Y
            store(0x47ADU, indexed_y(0x2E05U), registers.accumulator);
            append_checkpoint(0x47ADU, 0x47B0U, 15U);
            pc = 0x47B0U;
            break;

        case 0x47B0U: {  // SXY -- X becomes the driver channel index
            const auto swapped = registers.x;
            registers.x = registers.y;
            registers.y = swapped;
            append_checkpoint(0x47B0U, 0x47B1U, 9U);
            pc = 0x47B1U;
            break;
        }

        case 0x47B1U:  // STZ $F5,X -- stop the channel before rewriting it
            store(0x47B1U, zp_indexed_x(0xF5U), 0x00U);
            append_checkpoint(0x47B1U, 0x47B3U, 12U);
            pc = 0x47B3U;
            break;

        case 0x47B3U:  // LDY #$02
            registers.y = 0x02U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x47B3U, 0x47B5U, 6U);
            pc = 0x47B5U;
            break;

        case 0x47B5U:  // LDA ($D2)
            load_accumulator(read_mapped(indirect_pointer()));
            append_checkpoint(0x47B5U, 0x47B7U, 21U);
            pc = 0x47B7U;
            break;

        case 0x47B7U:  // STA $E4,X
            store(0x47B7U, zp_indexed_x(0xE4U), registers.accumulator);
            append_checkpoint(0x47B7U, 0x47B9U, 12U);
            pc = 0x47B9U;
            break;

        case 0x47B9U:  // LDA $D2
            load_accumulator(read_ram(zp(0xD2U)));
            append_checkpoint(0x47B9U, 0x47BBU, 12U);
            pc = 0x47BBU;
            break;

        case 0x47BBU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x47BBU, 0x47BCU, 6U);
            pc = 0x47BCU;
            break;

        case 0x47BCU:  // ADC #$08 -- sample data starts past the header
            add_with_carry(0x08U);
            append_checkpoint(0x47BCU, 0x47BEU, 6U);
            pc = 0x47BEU;
            break;

        case 0x47BEU:  // STA $2E10,X
            store(0x47BEU, indexed_x(0x2E10U), registers.accumulator);
            append_checkpoint(0x47BEU, 0x47C1U, 15U);
            pc = 0x47C1U;
            break;

        case 0x47C1U:  // CLA -- no flags, and the carry survives
            registers.accumulator = 0x00U;
            append_checkpoint(0x47C1U, 0x47C2U, 6U);
            pc = 0x47C2U;
            break;

        case 0x47C2U:  // ADC $D3
            add_with_carry(read_ram(zp(0xD3U)));
            append_checkpoint(0x47C2U, 0x47C4U, 12U);
            pc = 0x47C4U;
            break;

        case 0x47C4U:  // STA $2E11,X
            store(0x47C4U, indexed_x(0x2E11U), registers.accumulator);
            append_checkpoint(0x47C4U, 0x47C7U, 15U);
            pc = 0x47C7U;
            break;

        case 0x47C7U:  // LDA #$FF
            load_accumulator(0xFFU);
            append_checkpoint(0x47C7U, 0x47C9U, 6U);
            pc = 0x47C9U;
            break;

        case 0x47C9U:  // STA $F0,X
            store(0x47C9U, zp_indexed_x(0xF0U), registers.accumulator);
            append_checkpoint(0x47C9U, 0x47CBU, 12U);
            pc = 0x47CBU;
            break;

        case 0x47CBU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x47CBU, 0x47CDU, 21U);
            pc = 0x47CDU;
            break;

        case 0x47CDU:  // STA $DD,X
            store(0x47CDU, zp_indexed_x(0xDDU), registers.accumulator);
            append_checkpoint(0x47CDU, 0x47CFU, 12U);
            pc = 0x47CFU;
            break;

        case 0x47CFU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x47CFU, 0x47D0U, 6U);
            pc = 0x47D0U;
            break;

        case 0x47D0U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x47D0U, 0x47D2U, 21U);
            pc = 0x47D2U;
            break;

        case 0x47D2U:  // STA $DE,X
            store(0x47D2U, zp_indexed_x(0xDEU), registers.accumulator);
            append_checkpoint(0x47D2U, 0x47D4U, 12U);
            pc = 0x47D4U;
            break;

        case 0x47D4U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x47D4U, 0x47D5U, 6U);
            pc = 0x47D5U;
            break;

        case 0x47D5U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x47D5U, 0x47D7U, 21U);
            pc = 0x47D7U;
            break;

        case 0x47D7U:  // STA $E9,X
            store(0x47D7U, zp_indexed_x(0xE9U), registers.accumulator);
            append_checkpoint(0x47D7U, 0x47D9U, 12U);
            pc = 0x47D9U;
            break;

        case 0x47D9U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x47D9U, 0x47DAU, 6U);
            pc = 0x47DAU;
            break;

        case 0x47DAU:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x47DAU, 0x47DCU, 21U);
            pc = 0x47DCU;
            break;

        case 0x47DCU:  // STA $EA,X
            store(0x47DCU, zp_indexed_x(0xEAU), registers.accumulator);
            append_checkpoint(0x47DCU, 0x47DEU, 12U);
            pc = 0x47DEU;
            break;

        case 0x47DEU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x47DEU, 0x47DFU, 6U);
            pc = 0x47DFU;
            break;

        // Only reached with the mask already set; otherwise
        // `closes_interrupt_window` stops the walk in front of it.
        case 0x47DFU:  // SEI, already masked
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x47DFU, 0x47E0U, 6U);
            pc = 0x47E0U;
            break;

        case 0x47E0U:  // STX $0800 -- select the channel
            store(0x47E0U, kPsgChannelSelect, registers.x);
            append_checkpoint(0x47E0U, 0x47E3U, 15U);
            pc = 0x47E3U;
            break;

        case 0x47E3U:  // STZ $0807
            store(0x47E3U, kPsgNoise, 0x00U);
            append_checkpoint(0x47E3U, 0x47E6U, 15U);
            pc = 0x47E6U;
            break;

        case 0x47E6U:  // LDA #$40 -- DDA mode, silent
            load_accumulator(0x40U);
            append_checkpoint(0x47E6U, 0x47E8U, 6U);
            pc = 0x47E8U;
            break;

        case 0x47E8U:  // STA $0804
            store(0x47E8U, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x47E8U, 0x47EBU, 15U);
            pc = 0x47EBU;
            break;

        case 0x47EBU:  // STZ $0806
            store(0x47EBU, kPsgWaveform, 0x00U);
            append_checkpoint(0x47EBU, 0x47EEU, 15U);
            pc = 0x47EEU;
            break;

        case 0x47EEU:  // CLI -- also applies one instruction late
            pending_status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kInterruptDisable));
            status_change_pending = true;
            append_checkpoint(0x47EEU, 0x47EFU, 6U);
            pc = 0x47EFU;
            break;

        case 0x47EFU:  // LDA ($D2),Y -- the sample's base note
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x47EFU, 0x47F1U, 21U);
            pc = 0x47F1U;
            break;

        case 0x47F1U:  // BMI $483A -- unpitched, play it as recorded
            pc = branch(
                0x47F1U, 0x483AU, 0x47F3U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x47F3U:  // STA $EF,X
            store(0x47F3U, zp_indexed_x(0xEFU), registers.accumulator);
            append_checkpoint(0x47F3U, 0x47F5U, 12U);
            pc = 0x47F5U;
            break;

        case 0x47F5U:  // PLA -- the requested note
            load_accumulator(pull());
            append_checkpoint(0x47F5U, 0x47F6U, 12U);
            pc = 0x47F6U;
            break;

        case 0x47F6U:  // SEC
            registers.status = static_cast<std::uint8_t>(
                registers.status | kCarry);
            append_checkpoint(0x47F6U, 0x47F7U, 6U);
            pc = 0x47F7U;
            break;

        case 0x47F7U:  // SBC $EF,X -- semitones away from the base note
            subtract_with_carry(read_ram(zp_indexed_x(0xEFU)));
            append_checkpoint(0x47F7U, 0x47F9U, 12U);
            pc = 0x47F9U;
            break;

        case 0x47F9U:  // BEQ $483B -- exactly the base note
            pc = branch(
                0x47F9U, 0x483BU, 0x47FBU,
                (registers.status & kZero) != 0U);
            break;

        case 0x47FBU:  // BCS $480B -- above it
            pc = branch(
                0x47FBU, 0x480BU, 0x47FDU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x47FDU:  // EOR #$FF
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator ^ 0xFFU));
            append_checkpoint(0x47FDU, 0x47FFU, 6U);
            pc = 0x47FFU;
            break;

        case 0x47FFU:  // CMP #$24 -- three octaves down is the limit
            compare(registers.accumulator, 0x24U);
            append_checkpoint(0x47FFU, 0x4801U, 6U);
            pc = 0x4801U;
            break;

        case 0x4801U:  // BCS $482D
            pc = branch(
                0x4801U, 0x482DU, 0x4803U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4803U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4803U, 0x4804U, 6U);
            pc = 0x4804U;
            break;

        case 0x4804U:  // LDA $5B1D,Y -- downward rate table
            load_accumulator(read_mapped(indexed_y(0x5B1DU)));
            append_checkpoint(0x4804U, 0x4807U, 15U);
            pc = 0x4807U;
            break;

        case 0x4807U:  // STA $EF,X
            store(0x4807U, zp_indexed_x(0xEFU), registers.accumulator);
            append_checkpoint(0x4807U, 0x4809U, 12U);
            pc = 0x4809U;
            break;

        case 0x4809U:  // BCC $483D
            pc = branch(
                0x4809U, 0x483DU, 0x480BU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x480BU:  // CMP #$0D -- one octave up fits the table
            compare(registers.accumulator, 0x0DU);
            append_checkpoint(0x480BU, 0x480DU, 6U);
            pc = 0x480DU;
            break;

        case 0x480DU:  // BCS $481B
            pc = branch(
                0x480DU, 0x481BU, 0x480FU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x480FU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x480FU, 0x4810U, 6U);
            pc = 0x4810U;
            break;

        case 0x4810U:  // LDA $5B10,Y -- upward rate table
            load_accumulator(read_mapped(indexed_y(0x5B10U)));
            append_checkpoint(0x4810U, 0x4813U, 15U);
            pc = 0x4813U;
            break;

        case 0x4813U:  // STA $EF,X
            store(0x4813U, zp_indexed_x(0xEFU), registers.accumulator);
            append_checkpoint(0x4813U, 0x4815U, 12U);
            pc = 0x4815U;
            break;

        case 0x4815U:  // LDA #$80 -- one octave of oversampling
            load_accumulator(0x80U);
            append_checkpoint(0x4815U, 0x4817U, 6U);
            pc = 0x4817U;
            break;

        case 0x4817U:  // STA $F6,X
            store(0x4817U, zp_indexed_x(0xF6U), registers.accumulator);
            append_checkpoint(0x4817U, 0x4819U, 12U);
            pc = 0x4819U;
            break;

        case 0x4819U:  // BCC $483F
            pc = branch(
                0x4819U, 0x483FU, 0x481BU,
                (registers.status & kCarry) == 0U);
            break;

        case 0x481BU:  // SBC #$0C -- drop an octave and try again
            subtract_with_carry(0x0CU);
            append_checkpoint(0x481BU, 0x481DU, 6U);
            pc = 0x481DU;
            break;

        case 0x481DU:  // CMP #$0D
            compare(registers.accumulator, 0x0DU);
            append_checkpoint(0x481DU, 0x481FU, 6U);
            pc = 0x481FU;
            break;

        case 0x481FU:  // BCS $482D -- more than two octaves up, give up
            pc = branch(
                0x481FU, 0x482DU, 0x4821U,
                (registers.status & kCarry) != 0U);
            break;

        case 0x4821U:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4821U, 0x4822U, 6U);
            pc = 0x4822U;
            break;

        case 0x4822U:  // LDA $5B10,Y
            load_accumulator(read_mapped(indexed_y(0x5B10U)));
            append_checkpoint(0x4822U, 0x4825U, 15U);
            pc = 0x4825U;
            break;

        case 0x4825U:  // STA $EF,X
            store(0x4825U, zp_indexed_x(0xEFU), registers.accumulator);
            append_checkpoint(0x4825U, 0x4827U, 12U);
            pc = 0x4827U;
            break;

        case 0x4827U:  // LDA #$C0 -- two octaves of oversampling
            load_accumulator(0xC0U);
            append_checkpoint(0x4827U, 0x4829U, 6U);
            pc = 0x4829U;
            break;

        case 0x4829U:  // STA $F6,X
            store(0x4829U, zp_indexed_x(0xF6U), registers.accumulator);
            append_checkpoint(0x4829U, 0x482BU, 12U);
            pc = 0x482BU;
            break;

        case 0x482BU:  // BCC $483F
            pc = branch(
                0x482BU, 0x483FU, 0x482DU,
                (registers.status & kCarry) == 0U);
            break;

        // Only reached with the mask already set; otherwise
        // `closes_interrupt_window` stops the walk in front of it.
        case 0x482DU:  // SEI, already masked; out of range, so silence
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x482DU, 0x482EU, 6U);
            pc = 0x482EU;
            break;

        case 0x482EU:  // STX $0800
            store(0x482EU, kPsgChannelSelect, registers.x);
            append_checkpoint(0x482EU, 0x4831U, 15U);
            pc = 0x4831U;
            break;

        case 0x4831U: {  // LDX $2E34
            const auto value = read_ram(0x2E34U);
            registers.x = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4831U, 0x4834U, 15U);
            pc = 0x4834U;
            break;
        }

        case 0x4834U:  // STZ $0805
            store(0x4834U, kPsgBalance, 0x00U);
            append_checkpoint(0x4834U, 0x4837U, 15U);
            pc = 0x4837U;
            break;

        case 0x4837U:  // CLI
            pending_status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kInterruptDisable));
            status_change_pending = true;
            append_checkpoint(0x4837U, 0x4838U, 6U);
            pc = 0x4838U;
            break;

        case 0x4838U:  // BCS $4873
            pc = branch(
                0x4838U, 0x4873U, 0x483AU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x483AU:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x483AU, 0x483BU, 12U);
            pc = 0x483BU;
            break;

        case 0x483BU:  // STZ $EF,X
            store(0x483BU, zp_indexed_x(0xEFU), 0x00U);
            append_checkpoint(0x483BU, 0x483DU, 12U);
            pc = 0x483DU;
            break;

        case 0x483DU:  // STZ $F6,X
            store(0x483DU, zp_indexed_x(0xF6U), 0x00U);
            append_checkpoint(0x483DU, 0x483FU, 12U);
            pc = 0x483FU;
            break;

        case 0x483FU:  // LDY #$07
            registers.y = 0x07U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x483FU, 0x4841U, 6U);
            pc = 0x4841U;
            break;

        case 0x4841U:  // LDA $F5 -- is any channel already streaming?
            load_accumulator(read_ram(zp(0xF5U)));
            append_checkpoint(0x4841U, 0x4843U, 12U);
            pc = 0x4843U;
            break;

        case 0x4843U:  // ORA $F7
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0xF7U))));
            append_checkpoint(0x4843U, 0x4845U, 12U);
            pc = 0x4845U;
            break;

        case 0x4845U:  // ORA $F9
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator | read_ram(zp(0xF9U))));
            append_checkpoint(0x4845U, 0x4847U, 12U);
            pc = 0x4847U;
            break;

        case 0x4847U:  // PHA
            push(0x4847U, registers.accumulator);
            append_checkpoint(0x4847U, 0x4848U, 9U);
            pc = 0x4848U;
            break;

        case 0x4848U:  // LDA ($D2),Y -- the sample's bank index
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4848U, 0x484AU, 21U);
            pc = 0x484AU;
            break;

        case 0x484AU:  // TAY
            registers.y = registers.accumulator;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x484AU, 0x484BU, 6U);
            pc = 0x484BU;
            break;

        case 0x484BU:  // LDA $FC06,Y
            load_accumulator(read_mapped(indexed_y(0xFC06U)));
            append_checkpoint(0x484BU, 0x484EU, 15U);
            pc = 0x484EU;
            break;

        case 0x484EU:  // ORA #$80 -- the driver's "channel busy" bit
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x80U));
            append_checkpoint(0x484EU, 0x4850U, 6U);
            pc = 0x4850U;
            break;

        case 0x4850U:  // STA $F5,X -- hand the channel to the driver
            store(0x4850U, zp_indexed_x(0xF5U), registers.accumulator);
            append_checkpoint(0x4850U, 0x4852U, 12U);
            pc = 0x4852U;
            break;

        case 0x4852U:  // PLA
            load_accumulator(pull());
            append_checkpoint(0x4852U, 0x4853U, 12U);
            pc = 0x4853U;
            break;

        case 0x4853U:  // BNE $485D -- the timer is already running
            pc = branch(
                0x4853U, 0x485DU, 0x4855U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4855U:  // STZ $0C01 -- restart the sample timer
            store(0x4855U, kTimerControl, 0x00U);
            append_checkpoint(0x4855U, 0x4858U, 15U);
            pc = 0x4858U;
            break;

        case 0x4858U:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0x4858U, 0x485AU, 6U);
            pc = 0x485AU;
            break;

        case 0x485AU:  // STA $0C01
            store(0x485AU, kTimerControl, registers.accumulator);
            append_checkpoint(0x485AU, 0x485DU, 15U);
            pc = 0x485DU;
            break;

        // Only reached with the mask already set; otherwise
        // `closes_interrupt_window` stops the walk in front of it.
        case 0x485DU:  // SEI, already masked
            pending_status = static_cast<std::uint8_t>(
                registers.status | kInterruptDisable);
            status_change_pending = true;
            append_checkpoint(0x485DU, 0x485EU, 6U);
            pc = 0x485EU;
            break;

        case 0x485EU:  // STX $0800
            store(0x485EU, kPsgChannelSelect, registers.x);
            append_checkpoint(0x485EU, 0x4861U, 15U);
            pc = 0x4861U;
            break;

        case 0x4861U: {  // LDX $2E34 -- back to the voice index
            const auto value = read_ram(0x2E34U);
            registers.x = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4861U, 0x4864U, 15U);
            pc = 0x4864U;
            break;
        }

        case 0x4864U:  // LDA $2E50,X
            load_accumulator(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x4864U, 0x4867U, 15U);
            pc = 0x4867U;
            break;

        case 0x4867U:  // ORA #$C0 -- channel on, DDA off
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0xC0U));
            append_checkpoint(0x4867U, 0x4869U, 6U);
            pc = 0x4869U;
            break;

        case 0x4869U:  // STA $0804
            store(0x4869U, kPsgAmplitude, registers.accumulator);
            append_checkpoint(0x4869U, 0x486CU, 15U);
            pc = 0x486CU;
            break;

        case 0x486CU:  // LDA $2FAE,X
            load_accumulator(read_ram(indexed_x(0x2FAEU)));
            append_checkpoint(0x486CU, 0x486FU, 15U);
            pc = 0x486FU;
            break;

        case 0x486FU:  // STA $0805
            store(0x486FU, kPsgBalance, registers.accumulator);
            append_checkpoint(0x486FU, 0x4872U, 15U);
            pc = 0x4872U;
            break;

        case 0x4872U:  // CLI
            pending_status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~kInterruptDisable));
            status_change_pending = true;
            append_checkpoint(0x4872U, 0x4873U, 6U);
            pc = 0x4873U;
            break;

        case 0x4873U:  // LDA $D4
            load_accumulator(read_ram(zp(0xD4U)));
            append_checkpoint(0x4873U, 0x4875U, 12U);
            pc = 0x4875U;
            break;

        case 0x4875U:  // TAM #$40 -- put MPR6 back
            registers.mpr[6] = registers.accumulator;
            append_checkpoint(0x4875U, 0x4877U, 15U);
            pc = 0x4877U;
            break;

        case 0x4877U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x4877U, 0x487AU, 15U);
            pc = 0x487AU;
            break;

        case 0x487AU:  // ORA #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x02U));
            append_checkpoint(0x487AU, 0x487CU, 6U);
            pc = 0x487CU;
            break;

        case 0x487CU:  // STA $2EBE,X
            store(0x487CU, indexed_x(0x2EBEU), registers.accumulator);
            append_checkpoint(0x487CU, 0x487FU, 15U);
            pc = 0x487FU;
            break;

        case 0x487FU: {  // LDY $2E35
            const auto value = read_ram(0x2E35U);
            registers.y = value;
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x487FU, 0x4882U, 15U);
            pc = 0x4882U;
            break;
        }

        // ---- 45:430E-45:4559  command handlers ---------------------------
        // The arms of the $42BA dispatch table. Only the ones the census
        // route actually reaches are transcribed; the rest fall through to
        // `default` and decline, which costs nothing measurable and keeps
        // untested code out of the transcript. `4356`, the $82/$92 handler,
        // is deliberately among them: it pops four bytes and jumps to $40EE,
        // abandoning 457A's frame entirely, which a leaf transcript that has
        // to stop in front of an RTS cannot express.

        case 0x430EU:  // TAX -- retarget the sequence cursor
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x430EU, 0x430FU, 6U);
            pc = 0x430FU;
            break;

        case 0x430FU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x430FU, 0x4310U, 6U);
            pc = 0x4310U;
            break;

        case 0x4310U:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x4310U, 0x4312U, 21U);
            pc = 0x4312U;
            break;

        case 0x4312U:  // STA $D1
            store(0x4312U, zp(0xD1U), registers.accumulator);
            append_checkpoint(0x4312U, 0x4314U, 12U);
            pc = 0x4314U;
            break;

        case 0x4314U:  // STX $D0
            store(0x4314U, zp(0xD0U), registers.x);
            append_checkpoint(0x4314U, 0x4316U, 12U);
            pc = 0x4316U;
            break;

        case 0x4316U:  // LDY #$FF -- 459E's INY restarts the scan at zero
            registers.y = 0xFFU;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4316U, 0x4318U, 6U);
            pc = 0x4318U;
            break;

        case 0x4335U:  // CLC -- loop back if the counter has not run out
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4335U, 0x4336U, 6U);
            pc = 0x4336U;
            break;

        case 0x4336U:  // ADC $2E34
            add_with_carry(read_ram(0x2E34U));
            append_checkpoint(0x4336U, 0x4339U, 15U);
            pc = 0x4339U;
            break;

        case 0x4339U:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4339U, 0x433AU, 6U);
            pc = 0x433AU;
            break;

        case 0x433AU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x433AU, 0x433BU, 6U);
            pc = 0x433BU;
            break;

        case 0x433BU:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x433BU, 0x433DU, 21U);
            pc = 0x433DU;
            break;

        case 0x433DU: {  // DEC $2EE6,X
            const auto address = indexed_x(0x2EE6U);
            const auto result = static_cast<std::uint8_t>(
                read_ram(address) - 1U);
            registers.status = set_nz(registers.status, result);
            store(0x433DU, address, result);
            append_checkpoint(0x433DU, 0x4340U, 21U);
            pc = 0x4340U;
            break;
        }

        case 0x4340U:  // BNE $430E
            pc = branch(
                0x4340U, 0x430EU, 0x4342U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4342U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4342U, 0x4343U, 6U);
            pc = 0x4343U;
            break;

        case 0x4344U:  // CLC -- set a loop counter
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4344U, 0x4345U, 6U);
            pc = 0x4345U;
            break;

        case 0x4345U:  // ADC $2E34
            add_with_carry(read_ram(0x2E34U));
            append_checkpoint(0x4345U, 0x4348U, 15U);
            pc = 0x4348U;
            break;

        case 0x4348U:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4348U, 0x4349U, 6U);
            pc = 0x4349U;
            break;

        case 0x4349U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4349U, 0x434AU, 6U);
            pc = 0x434AU;
            break;

        case 0x434AU:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x434AU, 0x434CU, 21U);
            pc = 0x434CU;
            break;

        case 0x434CU:  // STA $2EE6,X
            store(0x434CU, indexed_x(0x2EE6U), registers.accumulator);
            append_checkpoint(0x434CU, 0x434FU, 15U);
            pc = 0x434FU;
            break;

        case 0x4360U:  // STA $2E64,X -- set the voice's level
            store(0x4360U, indexed_x(0x2E64U), registers.accumulator);
            append_checkpoint(0x4360U, 0x4363U, 15U);
            pc = 0x4363U;
            break;

        case 0x4364U:  // STA $2E5A,X -- force the voice's level
            store(0x4364U, indexed_x(0x2E5AU), registers.accumulator);
            append_checkpoint(0x4364U, 0x4367U, 15U);
            pc = 0x4367U;
            break;

        case 0x4368U:  // CMP #$80 -- transpose, absolute or relative
            compare(registers.accumulator, 0x80U);
            append_checkpoint(0x4368U, 0x436AU, 6U);
            pc = 0x436AU;
            break;

        case 0x436AU:  // BNE $436F
            pc = branch(
                0x436AU, 0x436FU, 0x436CU,
                (registers.status & kZero) == 0U);
            break;

        case 0x436CU:  // CLA -- no flags
            registers.accumulator = 0x00U;
            append_checkpoint(0x436CU, 0x436DU, 6U);
            pc = 0x436DU;
            break;

        case 0x436DU:  // BEQ $4373
            pc = branch(
                0x436DU, 0x4373U, 0x436FU,
                (registers.status & kZero) != 0U);
            break;

        case 0x436FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x436FU, 0x4370U, 6U);
            pc = 0x4370U;
            break;

        case 0x4370U:  // ADC $2E6E,X
            add_with_carry(read_ram(indexed_x(0x2E6EU)));
            append_checkpoint(0x4370U, 0x4373U, 15U);
            pc = 0x4373U;
            break;

        case 0x4373U:  // STA $2E6E,X
            store(0x4373U, indexed_x(0x2E6EU), registers.accumulator);
            append_checkpoint(0x4373U, 0x4376U, 15U);
            pc = 0x4376U;
            break;

        case 0x4377U:  // STA $2E46,X -- set the voice's mode word
            store(0x4377U, indexed_x(0x2E46U), registers.accumulator);
            append_checkpoint(0x4377U, 0x437AU, 15U);
            pc = 0x437AU;
            break;

        case 0x437BU:  // CLC -- adjust the voice's volume, clamped
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x437BU, 0x437CU, 6U);
            pc = 0x437CU;
            break;

        case 0x437CU:  // BMI $4389 -- a negative delta
            pc = branch(
                0x437CU, 0x4389U, 0x437EU,
                (registers.status & kNegative) != 0U);
            break;

        case 0x437EU:  // ADC $2E50,X
            add_with_carry(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x437EU, 0x4381U, 15U);
            pc = 0x4381U;
            break;

        case 0x4381U:  // CMP #$20
            compare(registers.accumulator, 0x20U);
            append_checkpoint(0x4381U, 0x4383U, 6U);
            pc = 0x4383U;
            break;

        case 0x4383U:  // BCC $438F
            pc = branch(
                0x4383U, 0x438FU, 0x4385U,
                (registers.status & kCarry) == 0U);
            break;

        case 0x4385U:  // LDA #$1F -- clamp at full volume
            load_accumulator(0x1FU);
            append_checkpoint(0x4385U, 0x4387U, 6U);
            pc = 0x4387U;
            break;

        case 0x4387U:  // BNE $438F
            pc = branch(
                0x4387U, 0x438FU, 0x4389U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4389U:  // ADC $2E50,X
            add_with_carry(read_ram(indexed_x(0x2E50U)));
            append_checkpoint(0x4389U, 0x438CU, 15U);
            pc = 0x438CU;
            break;

        case 0x438CU:  // BCS $438F
            pc = branch(
                0x438CU, 0x438FU, 0x438EU,
                (registers.status & kCarry) != 0U);
            break;

        case 0x438EU:  // CLA -- clamp at silence
            registers.accumulator = 0x00U;
            append_checkpoint(0x438EU, 0x438FU, 6U);
            pc = 0x438FU;
            break;

        case 0x438FU:  // STA $2E50,X
            store(0x438FU, indexed_x(0x2E50U), registers.accumulator);
            append_checkpoint(0x438FU, 0x4392U, 15U);
            pc = 0x4392U;
            break;

        case 0x4393U:  // BEQ $43A6 -- arm or disarm the portamento
            pc = branch(
                0x4393U, 0x43A6U, 0x4395U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4395U:  // STA $2F86,X
            store(0x4395U, indexed_x(0x2F86U), registers.accumulator);
            append_checkpoint(0x4395U, 0x4398U, 15U);
            pc = 0x4398U;
            break;

        case 0x4398U:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0x4398U, 0x439AU, 6U);
            pc = 0x439AU;
            break;

        case 0x439AU:  // STA $2F04,X
            store(0x439AU, indexed_x(0x2F04U), registers.accumulator);
            append_checkpoint(0x439AU, 0x439DU, 15U);
            pc = 0x439DU;
            break;

        case 0x439DU:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x439DU, 0x43A0U, 15U);
            pc = 0x43A0U;
            break;

        case 0x43A0U:  // ORA #$01
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x01U));
            append_checkpoint(0x43A0U, 0x43A2U, 6U);
            pc = 0x43A2U;
            break;

        case 0x43A2U:  // STA $2EBE,X
            store(0x43A2U, indexed_x(0x2EBEU), registers.accumulator);
            append_checkpoint(0x43A2U, 0x43A5U, 15U);
            pc = 0x43A5U;
            break;

        case 0x43A6U:  // LDA $2EBE,X
            load_accumulator(read_ram(indexed_x(0x2EBEU)));
            append_checkpoint(0x43A6U, 0x43A9U, 15U);
            pc = 0x43A9U;
            break;

        case 0x43A9U:  // AND #$FE
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xFEU));
            append_checkpoint(0x43A9U, 0x43ABU, 6U);
            pc = 0x43ABU;
            break;

        case 0x43ABU:  // STA $2EBE,X
            store(0x43ABU, indexed_x(0x2EBEU), registers.accumulator);
            append_checkpoint(0x43ABU, 0x43AEU, 15U);
            pc = 0x43AEU;
            break;

        case 0x43C8U:  // STA $D2 -- program the envelope
            store(0x43C8U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x43C8U, 0x43CAU, 12U);
            pc = 0x43CAU;
            break;

        case 0x43CAU:  // AND #$3F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x3FU));
            append_checkpoint(0x43CAU, 0x43CCU, 6U);
            pc = 0x43CCU;
            break;

        case 0x43CCU:  // STA $2F4A,X
            store(0x43CCU, indexed_x(0x2F4AU), registers.accumulator);
            append_checkpoint(0x43CCU, 0x43CFU, 15U);
            pc = 0x43CFU;
            break;

        case 0x43CFU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x43CFU, 0x43D2U, 15U);
            pc = 0x43D2U;
            break;

        case 0x43D2U:  // AND #$CD
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xCDU));
            append_checkpoint(0x43D2U, 0x43D4U, 6U);
            pc = 0x43D4U;
            break;

        case 0x43D4U:  // ORA #$20
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x20U));
            append_checkpoint(0x43D4U, 0x43D6U, 6U);
            pc = 0x43D6U;
            break;

        case 0x43D6U:  // BIT $D2
            bit_test(read_ram(zp(0xD2U)));
            append_checkpoint(0x43D6U, 0x43D8U, 12U);
            pc = 0x43D8U;
            break;

        case 0x43D8U:  // BPL $43DC
            pc = branch(
                0x43D8U, 0x43DCU, 0x43DAU,
                (registers.status & kNegative) == 0U);
            break;

        case 0x43DAU:  // ORA #$10
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x10U));
            append_checkpoint(0x43DAU, 0x43DCU, 6U);
            pc = 0x43DCU;
            break;

        case 0x43DCU:  // STA $2EB4,X
            store(0x43DCU, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x43DCU, 0x43DFU, 15U);
            pc = 0x43DFU;
            break;

        case 0x43DFU:  // LDA #$00
            load_accumulator(0x00U);
            append_checkpoint(0x43DFU, 0x43E1U, 6U);
            pc = 0x43E1U;
            break;

        case 0x43E1U:  // STA $2F40,X
            store(0x43E1U, indexed_x(0x2F40U), registers.accumulator);
            append_checkpoint(0x43E1U, 0x43E4U, 15U);
            pc = 0x43E4U;
            break;

        case 0x43E4U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x43E4U, 0x43E5U, 6U);
            pc = 0x43E5U;
            break;

        case 0x43E5U:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x43E5U, 0x43E7U, 21U);
            pc = 0x43E7U;
            break;

        case 0x43E7U:  // STA $2F36,X
            store(0x43E7U, indexed_x(0x2F36U), registers.accumulator);
            append_checkpoint(0x43E7U, 0x43EAU, 15U);
            pc = 0x43EAU;
            break;

        case 0x43EAU:  // BNE $43F4
            pc = branch(
                0x43EAU, 0x43F4U, 0x43ECU,
                (registers.status & kZero) == 0U);
            break;

        case 0x43ECU:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x43ECU, 0x43EFU, 15U);
            pc = 0x43EFU;
            break;

        case 0x43EFU:  // AND #$DF
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xDFU));
            append_checkpoint(0x43EFU, 0x43F1U, 6U);
            pc = 0x43F1U;
            break;

        case 0x43F1U:  // STA $2EB4,X
            store(0x43F1U, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x43F1U, 0x43F4U, 15U);
            pc = 0x43F4U;
            break;

        case 0x43F5U:  // LDA $2EB4,X -- conditional retarget
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x43F5U, 0x43F8U, 15U);
            pc = 0x43F8U;
            break;

        case 0x43F8U:  // AND #$02
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x02U));
            append_checkpoint(0x43F8U, 0x43FAU, 6U);
            pc = 0x43FAU;
            break;

        case 0x43FAU:  // BNE $4401
            pc = branch(
                0x43FAU, 0x4401U, 0x43FCU,
                (registers.status & kZero) == 0U);
            break;

        case 0x43FCU:  // LDA ($D0),Y
            load_accumulator(read_mapped(sequence_pointer()));
            append_checkpoint(0x43FCU, 0x43FEU, 21U);
            pc = 0x43FEU;
            break;

        case 0x43FEU:  // JMP $430E
            append_checkpoint(0x43FEU, 0x430EU, 12U);
            pc = 0x430EU;
            break;

        case 0x4401U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4401U, 0x4402U, 6U);
            pc = 0x4402U;
            break;

        case 0x4434U:  // BEQ $443C -- adjust the detune
            pc = branch(
                0x4434U, 0x443CU, 0x4436U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4436U:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4436U, 0x4437U, 6U);
            pc = 0x4437U;
            break;

        case 0x4437U:  // ADC $2F68,X
            add_with_carry(read_ram(indexed_x(0x2F68U)));
            append_checkpoint(0x4437U, 0x443AU, 15U);
            pc = 0x443AU;
            break;

        case 0x443AU:  // AND #$1F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x1FU));
            append_checkpoint(0x443AU, 0x443CU, 6U);
            pc = 0x443CU;
            break;

        case 0x443CU:  // STA $2F68,X
            store(0x443CU, indexed_x(0x2F68U), registers.accumulator);
            append_checkpoint(0x443CU, 0x443FU, 15U);
            pc = 0x443FU;
            break;

        case 0x4440U:  // BMI $4450 -- arm the sweep, up or down
            pc = branch(
                0x4440U, 0x4450U, 0x4442U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x4442U:  // BEQ $445E
            pc = branch(
                0x4442U, 0x445EU, 0x4444U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4444U:  // STA $2F86,X
            store(0x4444U, indexed_x(0x2F86U), registers.accumulator);
            append_checkpoint(0x4444U, 0x4447U, 15U);
            pc = 0x4447U;
            break;

        case 0x4447U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4447U, 0x444AU, 15U);
            pc = 0x444AU;
            break;

        case 0x444AU:  // AND #$BF
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0xBFU));
            append_checkpoint(0x444AU, 0x444CU, 6U);
            pc = 0x444CU;
            break;

        case 0x444CU:  // ORA #$80
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0x80U));
            append_checkpoint(0x444CU, 0x444EU, 6U);
            pc = 0x444EU;
            break;

        case 0x444EU:  // BMI $445A
            pc = branch(
                0x444EU, 0x445AU, 0x4450U,
                (registers.status & kNegative) != 0U);
            break;

        case 0x4450U:  // AND #$7F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU));
            append_checkpoint(0x4450U, 0x4452U, 6U);
            pc = 0x4452U;
            break;

        case 0x4452U:  // STA $2F86,X
            store(0x4452U, indexed_x(0x2F86U), registers.accumulator);
            append_checkpoint(0x4452U, 0x4455U, 15U);
            pc = 0x4455U;
            break;

        case 0x4455U:  // LDA $2EB4,X
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x4455U, 0x4458U, 15U);
            pc = 0x4458U;
            break;

        case 0x4458U:  // ORA #$C0
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator | 0xC0U));
            append_checkpoint(0x4458U, 0x445AU, 6U);
            pc = 0x445AU;
            break;

        case 0x445AU:  // STA $2EB4,X
            store(0x445AU, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x445AU, 0x445DU, 15U);
            pc = 0x445DU;
            break;

        case 0x445EU:  // LDA $2EB4,X -- a zero delta disarms the sweep
            load_accumulator(read_ram(indexed_x(0x2EB4U)));
            append_checkpoint(0x445EU, 0x4461U, 15U);
            pc = 0x4461U;
            break;

        case 0x4461U:  // AND #$7F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x7FU));
            append_checkpoint(0x4461U, 0x4463U, 6U);
            pc = 0x4463U;
            break;

        case 0x4463U:  // STA $2EB4,X
            store(0x4463U, indexed_x(0x2EB4U), registers.accumulator);
            append_checkpoint(0x4463U, 0x4466U, 15U);
            pc = 0x4466U;
            break;

        case 0x4467U:  // LDA $2E46,X -- toggle the voice's key-off bit
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4467U, 0x446AU, 15U);
            pc = 0x446AU;
            break;

        case 0x446AU:  // EOR #$10
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator ^ 0x10U));
            append_checkpoint(0x446AU, 0x446CU, 6U);
            pc = 0x446CU;
            break;

        case 0x446CU:  // STA $2E46,X
            store(0x446CU, indexed_x(0x2E46U), registers.accumulator);
            append_checkpoint(0x446CU, 0x446FU, 15U);
            pc = 0x446FU;
            break;

        case 0x446FU:  // DEY -- this command takes no operand
            registers.y = static_cast<std::uint8_t>(registers.y - 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x446FU, 0x4470U, 6U);
            pc = 0x4470U;
            break;

        case 0x4517U:  // STA $2EA0,X -- select an instrument outright
            store(0x4517U, indexed_x(0x2EA0U), registers.accumulator);
            append_checkpoint(0x4517U, 0x451AU, 15U);
            pc = 0x451AU;
            break;

        case 0x451FU:  // JMP $4522
            append_checkpoint(0x451FU, 0x4522U, 12U);
            pc = 0x4522U;
            break;

        case 0x4522U:  // CMP $2FAE,X -- program the stereo balance
            compare(registers.accumulator, read_ram(indexed_x(0x2FAEU)));
            append_checkpoint(0x4522U, 0x4525U, 15U);
            pc = 0x4525U;
            break;

        case 0x4525U:  // BEQ $4559 -- already there
            pc = branch(
                0x4525U, 0x4559U, 0x4527U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4527U:  // STA $2FAE,X
            store(0x4527U, indexed_x(0x2FAEU), registers.accumulator);
            append_checkpoint(0x4527U, 0x452AU, 15U);
            pc = 0x452AU;
            break;

        case 0x452AU:  // BIT $D7
            bit_test(read_ram(zp(0xD7U)));
            append_checkpoint(0x452AU, 0x452CU, 12U);
            pc = 0x452CU;
            break;

        case 0x452CU:  // BMI $4559 -- panning is disabled
            pc = branch(
                0x452CU, 0x4559U, 0x452EU,
                (registers.status & kNegative) != 0U);
            break;

        case 0x452EU:  // AND #$0F
            load_accumulator(
                static_cast<std::uint8_t>(registers.accumulator & 0x0FU));
            append_checkpoint(0x452EU, 0x4530U, 6U);
            pc = 0x4530U;
            break;

        case 0x4530U:  // STA $D8
            store(0x4530U, zp(0xD8U), registers.accumulator);
            append_checkpoint(0x4530U, 0x4532U, 12U);
            pc = 0x4532U;
            break;

        case 0x4532U:  // LDA $2FAE,X
            load_accumulator(read_ram(indexed_x(0x2FAEU)));
            append_checkpoint(0x4532U, 0x4535U, 15U);
            pc = 0x4535U;
            break;

        case 0x4535U:
        case 0x4536U:
        case 0x4537U:
        case 0x4538U: {  // LSR A x4 -- the left channel's nibble
            auto status = static_cast<std::uint8_t>(
                registers.status
                & static_cast<std::uint8_t>(~(kCarry | kNegative | kZero)));
            status = static_cast<std::uint8_t>(
                status
                | static_cast<std::uint8_t>(registers.accumulator & 0x01U));
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator >> 1U);
            registers.status = set_nz(status, registers.accumulator);
            const auto shift_pc = pc;
            append_checkpoint(
                shift_pc, static_cast<std::uint16_t>(shift_pc + 1U), 6U);
            pc = static_cast<std::uint16_t>(shift_pc + 1U);
            break;
        }

        case 0x4539U:  // CMP $D8
            compare(registers.accumulator, read_ram(zp(0xD8U)));
            append_checkpoint(0x4539U, 0x453BU, 12U);
            pc = 0x453BU;
            break;

        case 0x453BU:  // BEQ $4559 -- balanced, nothing to ramp
            pc = branch(
                0x453BU, 0x4559U, 0x453DU,
                (registers.status & kZero) != 0U);
            break;

        // ---- 45:4BCC  command descriptor loader ------------------------
        // Every routed game-to-audio command has a nonzero descriptor tag,
        // so the loader selects its little-endian pointer and falls directly
        // into the proven 4C42 record initializer below. A zero tag reaches
        // the adjacent push/RTS dispatch and remains an assembly fallback.
        case 0x4BCCU:  // STA $D4 -- accepted command ID
            store(0x4BCCU, zp(0xD4U), registers.accumulator);
            append_checkpoint(0x4BCCU, 0x4BCEU, 12U);
            pc = 0x4BCEU;
            break;

        case 0x4BCEU: {  // ASL A -- two-byte descriptor pointer index
            const auto carry = static_cast<std::uint8_t>(
                (registers.accumulator >> 7U) & kCarry);
            registers.accumulator = static_cast<std::uint8_t>(
                registers.accumulator << 1U);
            registers.status = set_nz(
                static_cast<std::uint8_t>(
                    registers.status
                    & static_cast<std::uint8_t>(~kCarry)),
                registers.accumulator);
            registers.status = static_cast<std::uint8_t>(
                registers.status | carry);
            append_checkpoint(0x4BCEU, 0x4BCFU, 6U);
            pc = 0x4BCFU;
            break;
        }

        case 0x4BCFU:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4BCFU, 0x4BD0U, 6U);
            pc = 0x4BD0U;
            break;

        case 0x4BD0U:  // LDA $5D81,X -- descriptor pointer low byte
            load_accumulator(read_mapped(indexed_x(0x5D81U)));
            append_checkpoint(0x4BD0U, 0x4BD3U, 15U);
            pc = 0x4BD3U;
            break;

        case 0x4BD3U:  // STA $D2
            store(0x4BD3U, zp(0xD2U), registers.accumulator);
            append_checkpoint(0x4BD3U, 0x4BD5U, 12U);
            pc = 0x4BD5U;
            break;

        case 0x4BD5U:  // LDA $5D82,X -- descriptor pointer high byte
            load_accumulator(read_mapped(indexed_x(0x5D82U)));
            append_checkpoint(0x4BD5U, 0x4BD8U, 15U);
            pc = 0x4BD8U;
            break;

        case 0x4BD8U:  // STA $D3
            store(0x4BD8U, zp(0xD3U), registers.accumulator);
            append_checkpoint(0x4BD8U, 0x4BDAU, 12U);
            pc = 0x4BDAU;
            break;

        case 0x4BDAU:  // LDY #$00
            registers.y = 0U;
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4BDAU, 0x4BDCU, 6U);
            pc = 0x4BDCU;
            break;

        case 0x4BDCU:  // LDA ($D2),Y -- descriptor tag
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4BDCU, 0x4BDEU, 21U);
            pc = 0x4BDEU;
            break;

        case 0x4BDEU:  // BNE $4C42 -- ordinary record descriptor
            pc = branch(
                0x4BDEU, 0x4C42U, 0x4BE0U,
                (registers.status & kZero) == 0U);
            break;

        // ---- 45:4C42  event command-record initializer -----------------
        // The pending-event dispatcher reaches this entry through the
        // push/RTS trampoline at 4BE0-4BE6. The command byte already sits in
        // A; this tail consumes one or more voice records through ($D2),Y,
        // clears their transient state, and reuses the proven balance helper
        // at 4527. The two route calls with a zero record byte take the
        // ordinary inactive-voice arm at 4BE7-4C1D. Active driver teardown
        // below 4BEF and the 4BE6 tail dispatch are not observed here and
        // deliberately fall through to the conservative default.
        case 0x4C42U:  // INY -- record count follows the command byte
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4C42U, 0x4C43U, 6U);
            pc = 0x4C43U;
            break;

        case 0x4C43U:  // STA $D5 -- remaining record count
            store(0x4C43U, zp(0xD5U), registers.accumulator);
            append_checkpoint(0x4C43U, 0x4C45U, 12U);
            pc = 0x4C45U;
            break;

        case 0x4C45U:  // LDA ($D2),Y -- record presence byte
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4C45U, 0x4C47U, 21U);
            pc = 0x4C47U;
            break;

        case 0x4C47U:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4C47U, 0x4C48U, 6U);
            pc = 0x4C48U;
            break;

        case 0x4C48U:  // TAX -- voice index
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4C48U, 0x4C49U, 6U);
            pc = 0x4C49U;
            break;

        case 0x4C49U:  // LDA ($D2),Y -- voice payload marker
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4C49U, 0x4C4BU, 21U);
            pc = 0x4C4BU;
            break;

        case 0x4C4BU:  // BEQ $4BE7 -- inactive voice
            pc = branch(
                0x4C4BU, 0x4BE7U, 0x4C4DU,
                (registers.status & kZero) != 0U);
            break;

        case 0x4BE7U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4BE7U, 0x4BEAU, 15U);
            pc = 0x4BEAU;
            break;

        case 0x4BEAU:  // AND $2EA0,X
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator
                & read_ram(indexed_x(0x2EA0U))));
            append_checkpoint(0x4BEAU, 0x4BEDU, 15U);
            pc = 0x4BEDU;
            break;

        case 0x4BEDU:  // BPL $4C19 -- active teardown remains emulated
            pc = branch(
                0x4BEDU, 0x4C19U, 0x4BEFU,
                (registers.status & kNegative) == 0U);
            break;

        case 0x4C19U:  // STZ $2E46,X
            store(0x4C19U, indexed_x(0x2E46U), 0x00U);
            append_checkpoint(0x4C19U, 0x4C1CU, 15U);
            pc = 0x4C1CU;
            break;

        case 0x4C1CU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4C1CU, 0x4C1DU, 6U);
            pc = 0x4C1DU;
            break;

        case 0x4C1DU:  // JMP $4CB1
            append_checkpoint(0x4C1DU, 0x4CB1U, 12U);
            pc = 0x4CB1U;
            break;

        case 0x4C4DU:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4C4DU, 0x4C50U, 15U);
            pc = 0x4C50U;
            break;

        case 0x4C50U:  // AND #$40
            load_accumulator(static_cast<std::uint8_t>(
                registers.accumulator & 0x40U));
            append_checkpoint(0x4C50U, 0x4C52U, 6U);
            pc = 0x4C52U;
            break;

        case 0x4C52U:  // BNE $4BE6 -- tail dispatch remains emulated
            pc = branch(
                0x4C52U, 0x4BE6U, 0x4C54U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4C54U:
        case 0x4C57U:
        case 0x4C5AU:
        case 0x4C5DU:
        case 0x4C60U:
        case 0x4C63U:
        case 0x4C66U:
        case 0x4C69U: {  // clear the voice's transient state arrays
            constexpr std::uint16_t kBases[] = {
                0x2EC8U, 0x2F18U, 0x2F90U, 0x2F04U,
                0x2F5EU, 0x2F68U, 0x2EB4U, 0x2EBEU,
            };
            const auto index = static_cast<std::size_t>(
                (pc - 0x4C54U) / 3U);
            const auto store_pc = pc;
            const auto next = static_cast<std::uint16_t>(pc + 3U);
            store(store_pc, indexed_x(kBases[index]), registers.accumulator);
            append_checkpoint(store_pc, next, 15U);
            pc = next;
            break;
        }

        case 0x4C6CU:  // LDA $D4
            load_accumulator(read_ram(zp(0xD4U)));
            append_checkpoint(0x4C6CU, 0x4C6EU, 12U);
            pc = 0x4C6EU;
            break;

        case 0x4C6EU:  // CMP $2F0E,X
            compare(
                registers.accumulator,
                read_ram(indexed_x(0x2F0EU)));
            append_checkpoint(0x4C6EU, 0x4C71U, 15U);
            pc = 0x4C71U;
            break;

        case 0x4C71U:  // BNE $4C7F
            pc = branch(
                0x4C71U, 0x4C7FU, 0x4C73U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4C73U:  // LDA $2E46,X
            load_accumulator(read_ram(indexed_x(0x2E46U)));
            append_checkpoint(0x4C73U, 0x4C76U, 15U);
            pc = 0x4C76U;
            break;

        case 0x4C76U:  // BEQ $4C82
            pc = branch(
                0x4C76U, 0x4C82U, 0x4C78U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4C78U:  // LDA #$04
            load_accumulator(0x04U);
            append_checkpoint(0x4C78U, 0x4C7AU, 6U);
            pc = 0x4C7AU;
            break;

        case 0x4C7AU:  // STA $2EBE,X
            store(0x4C7AU, indexed_x(0x2EBEU), registers.accumulator);
            append_checkpoint(0x4C7AU, 0x4C7DU, 15U);
            pc = 0x4C7DU;
            break;

        case 0x4C7DU:  // BNE $4C82
            pc = branch(
                0x4C7DU, 0x4C82U, 0x4C7FU,
                (registers.status & kZero) == 0U);
            break;

        case 0x4C7FU:  // STA $2F0E,X
            store(0x4C7FU, indexed_x(0x2F0EU), registers.accumulator);
            append_checkpoint(0x4C7FU, 0x4C82U, 15U);
            pc = 0x4C82U;
            break;

        case 0x4C82U:  // TXA
            load_accumulator(registers.x);
            append_checkpoint(0x4C82U, 0x4C83U, 6U);
            pc = 0x4C83U;
            break;

        case 0x4C83U:  // PHA
            push(0x4C83U, registers.accumulator);
            append_checkpoint(0x4C83U, 0x4C84U, 9U);
            pc = 0x4C84U;
            break;

        case 0x4C84U:  // LDA #$0B
            load_accumulator(0x0BU);
            append_checkpoint(0x4C84U, 0x4C86U, 6U);
            pc = 0x4C86U;
            break;

        case 0x4C86U:  // STA $D4 -- eleven interleaved voice bytes
            store(0x4C86U, zp(0xD4U), registers.accumulator);
            append_checkpoint(0x4C86U, 0x4C88U, 12U);
            pc = 0x4C88U;
            break;

        case 0x4C88U:  // LDA ($D2),Y
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4C88U, 0x4C8AU, 21U);
            pc = 0x4C8AU;
            break;

        case 0x4C8AU:  // INY
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(0x4C8AU, 0x4C8BU, 6U);
            pc = 0x4C8BU;
            break;

        case 0x4C8BU:  // STA $2E46,X
            store(0x4C8BU, indexed_x(0x2E46U), registers.accumulator);
            append_checkpoint(0x4C8BU, 0x4C8EU, 15U);
            pc = 0x4C8EU;
            break;

        case 0x4C8EU:  // TXA
            load_accumulator(registers.x);
            append_checkpoint(0x4C8EU, 0x4C8FU, 6U);
            pc = 0x4C8FU;
            break;

        case 0x4C8FU:  // CLC
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4C8FU, 0x4C90U, 6U);
            pc = 0x4C90U;
            break;

        case 0x4C90U:  // ADC #$0A -- next interleaved voice field
            add_with_carry(0x0AU);
            append_checkpoint(0x4C90U, 0x4C92U, 6U);
            pc = 0x4C92U;
            break;

        case 0x4C92U:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4C92U, 0x4C93U, 6U);
            pc = 0x4C93U;
            break;

        case 0x4C93U: {  // DEC $D4
            const auto value = static_cast<std::uint8_t>(
                read_ram(zp(0xD4U)) - 1U);
            store(0x4C93U, zp(0xD4U), value);
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4C93U, 0x4C95U, 18U);
            pc = 0x4C95U;
            break;
        }

        case 0x4C95U:  // BNE $4C88
            pc = branch(
                0x4C95U, 0x4C88U, 0x4C97U,
                (registers.status & kZero) == 0U);
            break;

        case 0x4C97U:  // PLA -- restore the voice index
            load_accumulator(pull());
            append_checkpoint(0x4C97U, 0x4C98U, 12U);
            pc = 0x4C98U;
            break;

        case 0x4C98U:  // TAX
            registers.x = registers.accumulator;
            registers.status = set_nz(registers.status, registers.x);
            append_checkpoint(0x4C98U, 0x4C99U, 6U);
            pc = 0x4C99U;
            break;

        case 0x4C99U:  // LDA $2F0E,X
            load_accumulator(read_ram(indexed_x(0x2F0EU)));
            append_checkpoint(0x4C99U, 0x4C9CU, 15U);
            pc = 0x4C9CU;
            break;

        case 0x4C9CU:  // STA $D4
            store(0x4C9CU, zp(0xD4U), registers.accumulator);
            append_checkpoint(0x4C9CU, 0x4C9EU, 12U);
            pc = 0x4C9EU;
            break;

        case 0x4C9EU:  // LDA #$01
            load_accumulator(0x01U);
            append_checkpoint(0x4C9EU, 0x4CA0U, 6U);
            pc = 0x4CA0U;
            break;

        case 0x4CA0U:  // STA $2ED2,X
            store(0x4CA0U, indexed_x(0x2ED2U), registers.accumulator);
            append_checkpoint(0x4CA0U, 0x4CA3U, 15U);
            pc = 0x4CA3U;
            break;

        case 0x4CA3U:  // CLC -- ROM intentionally subtracts with borrow
            registers.status = static_cast<std::uint8_t>(
                registers.status & static_cast<std::uint8_t>(~kCarry));
            append_checkpoint(0x4CA3U, 0x4CA4U, 6U);
            pc = 0x4CA4U;
            break;

        case 0x4CA4U:  // SBC $2E78,X
            subtract_with_carry(read_ram(indexed_x(0x2E78U)));
            append_checkpoint(0x4CA4U, 0x4CA7U, 15U);
            pc = 0x4CA7U;
            break;

        case 0x4CA7U:  // STA $2EDC,X
            store(0x4CA7U, indexed_x(0x2EDCU), registers.accumulator);
            append_checkpoint(0x4CA7U, 0x4CAAU, 15U);
            pc = 0x4CAAU;
            break;

        case 0x4CAAU:  // LDA ($D2),Y -- stereo balance
            load_accumulator(read_mapped(command_pointer()));
            append_checkpoint(0x4CAAU, 0x4CACU, 21U);
            pc = 0x4CACU;
            break;

        case 0x4CACU:
        case 0x4CADU: {  // INY x2
            const auto increment_pc = pc;
            registers.y = static_cast<std::uint8_t>(registers.y + 1U);
            registers.status = set_nz(registers.status, registers.y);
            append_checkpoint(
                increment_pc,
                static_cast<std::uint16_t>(increment_pc + 1U),
                6U);
            pc = static_cast<std::uint16_t>(increment_pc + 1U);
            break;
        }

        case 0x4CAEU:  // JSR $4527 -- apply the balance
            push(0x4CAEU, 0x4CU);
            push(0x4CAEU, 0xB0U);
            commit_pending_writes(cycle);
            ++depth;
            append_checkpoint(0x4CAEU, 0x4527U, 21U);
            pc = 0x4527U;
            break;

        case 0x4CB1U: {  // DEC $D5 -- next command record
            const auto value = static_cast<std::uint8_t>(
                read_ram(zp(0xD5U)) - 1U);
            store(0x4CB1U, zp(0xD5U), value);
            registers.status = set_nz(registers.status, value);
            append_checkpoint(0x4CB1U, 0x4CB3U, 18U);
            pc = 0x4CB3U;
            break;
        }

        case 0x4CB3U:  // BEQ $4CB8
            pc = branch(
                0x4CB3U, 0x4CB8U, 0x4CB5U,
                (registers.status & kZero) != 0U);
            break;

        case 0x4CB5U:  // JMP $4C45
            append_checkpoint(0x4CB5U, 0x4C45U, 12U);
            pc = 0x4C45U;
            break;

        // ---- nested returns ---------------------------------------------
        // Only reached with depth > 0; the depth-zero case terminated the
        // walk above, in front of the RTS.
        //
        // Seven of these are recent, and their absence was the bank's largest
        // single refusal. `4141`, `4243`, `4270`, `429E`, `42B2`, `42B9` and
        // `46D6` were already declared ordinary returns by
        // `is_return_instruction` -- which is what lets a walk *stop* in front
        // of one -- and simply had no case to *execute* one, so a walk that
        // inlined a call ending at any of them fell through to `default` and
        // gave the whole transcript up. 46D6 and 429E alone accounted for
        // 9,830 refusals on the census route, six times every other cause in
        // the bank put together. The two lists have to agree: an address in
        // one and not the other is a routine that can be entered and never
        // left.
        case 0x4141U:
        case 0x4210U:
        case 0x4227U:
        case 0x4243U:
        case 0x4270U:
        case 0x429EU:
        case 0x42B2U:
        case 0x42B9U:
        case 0x4318U:
        case 0x4343U:
        case 0x434FU:
        case 0x4363U:
        case 0x4367U:
        case 0x4376U:
        case 0x437AU:
        case 0x4392U:
        case 0x43A5U:
        case 0x43AEU:
        case 0x43F4U:
        case 0x4402U:
        case 0x443FU:
        case 0x445DU:
        case 0x4466U:
        case 0x4470U:
        case 0x451AU:
        case 0x4559U:
        case 0x46D6U:
        case 0x46E1U:
        case 0x46FBU:
        case 0x4775U:
        case 0x4882U:
        case 0x48ACU:
        case 0x48B3U:
        case 0x48B8U:
        case 0x48C0U:
        case 0x48CDU:
        case 0x48D6U:
        case 0x48E5U:
        case 0x48F5U:
        case 0x4935U:
        case 0x49B5U:
        case 0x49D1U:
        case 0x49F8U:
        case 0x4A28U:
        case 0x4A9BU:
        case 0x4CB8U: {
            const auto low = pull();
            const auto high = pull();
            const auto return_pc = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(low)
                 | static_cast<std::uint16_t>(high) << 8U)
                + 1U);
            --depth;
            const auto rts_pc = pc;
            append_checkpoint(rts_pc, return_pc, 21U);
            pc = return_pc;
            break;
        }

        default:
            // Any path this slice does not transcribe -- including the two
            // pending-event arms at 4216/4221, which call out to $4B86 --
            // runs emulated instead.
            return std::nullopt;
        }
    }
}

}  // namespace devils_crush::app::detail
