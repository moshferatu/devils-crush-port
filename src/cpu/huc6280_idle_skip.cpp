// SPDX-License-Identifier: GPL-3.0-or-later

#include "devils_crush/cpu/huc6280_idle_skip.hpp"

#include <cstdint>
#include <span>

namespace devils_crush::cpu {

std::int32_t HuC6280IdleSkip::iteration_cycles(
    const std::uint8_t accumulator,
    const std::span<const std::uint8_t> code,
    const std::span<const std::uint8_t> zero_page) noexcept
{
    coverage_.arrivals++;

    // The four bytes, read from the machine rather than assumed. A compare
    // against zero page, and a branch-if-equal whose displacement lands back
    // on that compare: together they are a loop that reads one byte and
    // writes nothing.
    if (code.size() < kSignatureLength
        || code[0] != kCompareZeroPageOpcode
        || code[2] != kBranchEqualOpcode
        || static_cast<std::int8_t>(code[3]) != kBranchDisplacement) {
        coverage_.refusals++;
        return 0;
    }

    const std::size_t address = code[1];
    if (zero_page.size() <= address) {
        coverage_.refusals++;
        return 0;
    }

    // The branch is taken while the two are equal, so equality is the machine
    // still waiting and difference is the wait already over. Refusing the
    // second is not an optimisation detail: the very next instruction leaves
    // the loop, and there is nothing there to skip.
    if (accumulator != zero_page[address]) {
        coverage_.exits++;
        return 0;
    }

    coverage_.idle++;
    return kIterationCycles;
}

void HuC6280IdleSkip::advanced(
    const std::int32_t iterations,
    const std::int32_t master_skipped,
    const std::int32_t master_per_iteration) noexcept
{
    // The wait is every turn of the loop, whether the machine was advanced
    // through it or ran it. The turn about to execute counts because it is
    // the machine waiting just as much as the ones skipped over it -- and
    // counting only the skipped ones would make the measured wait shrink as
    // the fast-forward got better at its job, which is precisely backwards.
    if (master_per_iteration > 0) {
        coverage_.wait_master_cycles +=
            static_cast<std::uint64_t>(master_per_iteration);
    }
    if (iterations <= 0) {
        return;
    }

    const auto skipped_iterations = static_cast<std::uint64_t>(iterations);

    coverage_.skips++;
    coverage_.iterations += skipped_iterations;
    coverage_.skipped_master_cycles +=
        static_cast<std::uint64_t>(master_skipped);
    coverage_.wait_master_cycles +=
        static_cast<std::uint64_t>(master_skipped);
    if (skipped_iterations > coverage_.longest_skip) {
        coverage_.longest_skip = skipped_iterations;
    }
}

}  // namespace devils_crush::cpu
