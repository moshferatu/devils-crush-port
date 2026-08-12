// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace devils_crush::cpu {

// The claim that a machine sitting at a particular address is doing nothing,
// and may therefore be advanced through rather than executed.
//
// This is the seventh seam and the first that is neither a device nor a rule
// but a *capability*: it takes no work away from the emulator and computes no
// answer the machine would otherwise have computed. Handing it back changes
// nothing except how long the same frame takes to produce.
//
// **Why it exists.** A routine replacement declares what it costs at the
// moment it is offered, which is the whole basis of the transcript machinery.
// A frame-sync wait cannot: it ends when the raster reaches a scanline, so its
// cost depends on where the next interrupt lands and is not knowable in
// advance. That single structural fact is why 45.2% of the census route's
// master cycles were still being interpreted, one 24-cycle turn at a time,
// long after the code around them had gone native.
//
// **What is checked, and why it is bytes rather than a description.** The
// idiom this recognises is an equality spin on a zero-page byte:
//
//     CMP <$zz     C5 zz     compare the accumulator against zero page
//     BEQ  -4      F0 FC     and go back and do it again if they matched
//
// A model that merely *believed* an address held those two instructions would
// be making a claim about a ROM it had never read. So the machine hands over
// the instruction stream at the watched address and the model compares all
// four bytes, every time it is asked. The zero-page address it then reads is
// taken out of the verified operand rather than declared beside it, and the
// branch displacement must land back on the compare -- so the loop's own
// bytes are the whole of the description, and a build pointed at the wrong
// address, a mid-frame bank switch, or a ROM that differs by one byte answers
// "not idle" and executes normally rather than skipping time it should not.
//
// **What makes the skip exact.** The loop reads two bytes and writes nothing,
// so nothing it does can change its own exit condition; only an interrupt
// handler can. The machine will not advance past the scheduler's near
// horizon, and no line can be raised before that horizon, so between here and
// there the exit condition provably cannot move.
//
// That is only half of it, and the other half cost a census to find: nothing
// may be *standing* either. A request the mask already admits has not been
// latched yet, so the decision taken at the top of the instruction loop says
// "next instruction" and the loop looks idle -- but the next instruction's own
// end-of-cycle sample would catch it and the boundary after that would take
// the vector. Skipping past that boundary delivers the interrupt a turn late,
// which is exactly what the first working version of this did. Both halves
// are the machine's, and both are argued at the seam in
// `dc_beetle_execution_hook.h`; so are the phase and the latch.
//
// **What is deliberately not here.** How far to advance. The horizon belongs
// to the scheduler and is not this model's to see, which is exactly why this
// answers a per-iteration cost rather than a duration.
class HuC6280IdleSkip {
public:
    // The two opcodes, the four bytes they occupy, and the displacement that
    // makes the second of them point back at the first.
    static constexpr std::uint8_t kCompareZeroPageOpcode = 0xC5;
    static constexpr std::uint8_t kBranchEqualOpcode = 0xF0;
    static constexpr std::size_t kSignatureLength = 4U;
    static constexpr std::int8_t kBranchDisplacement =
        -static_cast<std::int8_t>(kSignatureLength);

    // Processor cycles the two instructions cost together. Each is three for
    // the operation and one for the last cycle that takes the interrupt
    // sample, which at the fast rate this loop runs at is the 24 master
    // cycles a turn the profile measures.
    //
    // The T flag does not enter into it: `CMP` compares the accumulator in
    // both of its forms, so unlike the arithmetic opcodes it has no
    // memory-operand variant for a raised T flag to select.
    static constexpr std::int32_t kIterationCycles = 8;

    // Where the spin is. Not what it is -- that is read from the machine.
    struct Loop {
        std::uint8_t physical_bank = 0;
        std::uint16_t logical_pc = 0;
    };

    // Every path this model can take, counted. The interesting number is the
    // ratio of `iterations` to `skips`: it is how many turns of the loop an
    // average arrival was able to cover, and therefore what the capability is
    // actually worth on the route being measured.
    struct Coverage {
        // Times the machine reached the watched address and asked.
        std::uint64_t arrivals = 0;
        // Arrivals refused because the four bytes were not there, or because
        // the machine offered no readable instruction stream or zero page.
        // A nonzero count here is a fault rather than a variation: it means
        // the address being watched is not the loop this model describes.
        std::uint64_t refusals = 0;
        // Arrivals where the compare was about to differ -- the wait is over
        // and the loop is one branch from falling out of itself. This is one
        // per completed wait, so it counts the waits.
        std::uint64_t exits = 0;
        // Arrivals answered "idle", and the ones that then skipped at least
        // one whole iteration. The difference is the arrivals that were
        // already too close to the horizon to skip anything, which is a
        // property of the raster rather than of the loop.
        std::uint64_t idle = 0;
        std::uint64_t skips = 0;
        // Iterations actually skipped, the master cycles they covered, and
        // the largest single skip.
        std::uint64_t iterations = 0;
        std::uint64_t skipped_master_cycles = 0;
        std::uint64_t longest_skip = 0;
        // Master cycles the machine spent waiting here, skipped or executed.
        //
        // This is the number that makes a coverage share mean something. A
        // native share measured against the whole route is measured against a
        // denominator that is nearly half frame-sync wait, and no amount of
        // porting can move that part: the game is waiting for the raster, and
        // there is no work in the waiting to port. Subtracting this leaves the
        // cycles in which the machine was actually doing something.
        std::uint64_t wait_master_cycles = 0;
    };

    HuC6280IdleSkip() noexcept = default;
    explicit HuC6280IdleSkip(const Loop loop) noexcept : loop_{loop} {}

    [[nodiscard]] const Loop& loop() const noexcept { return loop_; }

    // The question the machine asks at the watched address: how much one turn
    // of this spin costs, or zero for "execute this normally".
    //
    // `code` is the instruction stream at the watched address and `zero_page`
    // is the processor's zero page -- 256 bytes at logical `$2000` on this
    // machine -- both as it has them mapped right now.
    [[nodiscard]] std::int32_t iteration_cycles(
        std::uint8_t accumulator,
        std::span<const std::uint8_t> code,
        std::span<const std::uint8_t> zero_page) noexcept;

    // What the machine did with that answer, told on every idle answer rather
    // than only on a skip. The model cannot work any of this out for itself:
    // the horizon it was measured against is not its to see, and neither is
    // the rate that prices a turn of the loop in master cycles.
    void advanced(
        std::int32_t iterations,
        std::int32_t master_skipped,
        std::int32_t master_per_iteration) noexcept;

    [[nodiscard]] const Coverage& coverage() const noexcept
    {
        return coverage_;
    }

private:
    Loop loop_{};
    Coverage coverage_{};
};

}  // namespace devils_crush::cpu
