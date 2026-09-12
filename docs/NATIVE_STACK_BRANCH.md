# Native HuC6280 stack operations and BRA

Patch `0049-huc6280-native-stack-branch.patch` executes PHA, PHP, PHX, PHY,
PLA, PLX, PLY and BRA through `cpu::run_huc6280_stack_branch`. Together with
the existing PLP engine, all push/pop instructions are native. Validated
transcripts retain precedence, frame-wait instructions retain idle ownership,
and `--emulated-boundary` preserves the original opcode oracle.

## Live ordering

Pushes advance two cycles, sample interrupts, advance the final cycle and write
the live register to `$2100 | S` before decrementing the eight-bit stack pointer.
PHP clears T after the sample and before pushing status with B set. Other
pushes clear T at retirement, after the write.

Pulls advance three cycles, sample interrupts, advance the final cycle, increment
the stack pointer and read the stack. They then update the destination register
and NZ, preserving other flags before clearing T at retirement. PLP remains in
its existing status engine because its interrupt-mask and T behavior differ.

BRA fetches a signed byte displacement and increments PC, advances three cycles,
adds the displacement with the pinned core's wide PC arithmetic, samples
interrupts and advances the final cycle. T clears at retirement.

`Native stack/branch operations` reports actual starts and exclusive CPU cycles,
already included in translated ownership. Device stalls remain separate. The
counters reset with the accounting window and are not serialized; savestate
format and earlier instruction counters remain unchanged.

## Measured coverage

Across 31 routes, this batch transfers **69,576 CPU master cycles** over
**5,464 starts**, removing **49.3321%** of the remaining non-native CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,647,633,764 | 20,647,703,340 |
| Retained opcode cycles | 0 | 0 |
| Fallback opcode cycles | 141,036 | 71,460 |
| Native share outside waits/stalls | 99.9993% | 99.9997% |

The denominator remains 20,647,774,800 CPU master cycles. All 31 routes preserve
frame hashes, device stalls, idle time, IRQ counts, earlier native counters and
abandonments. Translated ownership increases by exactly the new measured cycles;
original dispatches decrease by exactly the new measured starts. Other and
unaccounted cycles remain zero. Three pre-existing single abandonments remain
unchanged.

Alley transfers 5,073 cycles over 445 starts and reaches **99.9991%** native
execution, with 6,012 fallback cycles and its frame hash unchanged.

None of the eight covered opcodes remains in mapped-ROM interpreter profiles
outside the wait. The remaining elapsed interval ranking starts with TAM
(11,700), TAY (4,884), ADC zero page (4,464), SXY (4,365) and CLC (3,984).
Mapping, register transfers and flags are the next groups, followed by arithmetic.
Intervals guide selection; coverage uses exclusive ownership. This percentage
describes the exercised corpus, not all unvisited paths.

## Verification

`dc_stack_branch` compares 4,194,304 cases with the pinned stack opcode bodies
and taken-branch helper: eight opcodes, all 256 status values, all 256 stack
pointers and eight scenarios. It exercises all byte values, both CPU speeds,
divided clocks, device stalls, interrupt horizons, stack wrapping, signed
displacements and PC wrapping. Every bus access, clock batch and interrupt
sample must match, including intermediate registers and status. Final RAM,
registers, IRQ latch, sampled I and clock remainder must also match.

State accounting checks restored counter deltas. The three-mode gameplay check
compares frames and normalized ownership/dispatch counts under native execution,
the original scheduler and the original opcode engine.

All six selected Release checks and all three selected Debug checks pass,
including the Release ranked-VDC, corrupted-face and upper-hand regressions.
Registry generation remains current at 932 admissions, all 399 classified seam
ranges validate, and the downstream patch passes reverse-application checking.

Build/test logs, the prior executable and the replay corpus are under
`out/native-stack-branch`.
