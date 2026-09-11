# Native HuC6280 absolute load and status instructions

Patch `0042-huc6280-native-load-status.patch` executes absolute LDA, PLP and
SEI through `cpu::run_huc6280_load_status`. These were the three largest
remaining opcode families after native control flow. Accepted transcripts retain
precedence; remaining sites use the shared implementation across banks.
`--emulated-boundary` preserves the original opcode switch as the gameplay oracle.

## Live ordering

- Absolute LDA fetches both operand bytes using the wide live PC, advances four
  CPU cycles, samples interrupts, advances its final cycle, then reads through
  the mapped bus. It updates A and NZ and clears T. Device reads and their stalls
  happen at the original point, including VDC mirrors.
- PLP advances three cycles, samples using the old interrupt mask, advances its
  final cycle, increments the wrapping stack pointer and reads the restored
  status. It then publishes the new interrupt mask. The restored T flag survives.
- SEI advances one cycle, samples using the old interrupt mask, advances its
  final cycle, then sets I, publishes the mask and clears T.

The adapter supplies live bus operations, clock advances, interrupt sampling and
mask publication. The native function owns instruction decoding, PC and stack
arithmetic, register changes and flag retirement. It bypasses the original
opcode bodies and their load/stack helpers. No new savestate fields or interrupt
horizon declines are introduced. The excluded frame wait retains idle ownership.

`Native load/status` reports actual shared-engine instruction starts and
exclusive CPU cycles. Those cycles are a subset of translated ownership;
device stalls remain separate. Counters reset with the accounting window and
are not serialized. Native profiling is emitted only when the engine runs.

## Measured coverage

The 31-route exclusive ledger transfers **8,447,097 CPU master cycles**, or
**80.6177%** of the remaining non-native CPU time, across **810,866** shared
instruction starts.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,637,296,829 | 20,645,743,926 |
| Retained opcode cycles | 8,754,585 | 405,012 |
| Fallback opcode cycles | 1,723,386 | 1,625,862 |
| Remaining non-native CPU cycles | 10,477,971 | 2,030,874 |
| Native share outside waits/stalls | 99.9493% | 99.9902% |

The useful-work denominator stays **20,647,774,800 master cycles**. All 31 routes
preserve frames, device stalls, idle time, IRQ counts, earlier native-engine
counters and abandonments. Other and unaccounted time remain zero. The increase
in translated cycles exactly matches the reduction in retained/fallback cycles,
and original dispatches decrease by exactly the shared-engine instruction count.
The three pre-existing single abandonments are unchanged.

Alley transfers **306,459** CPU cycles across **28,612** shared instructions and
reaches **99.9802%** native CPU execution. Its pinned frame hash and normalized
ownership agree with the original instruction engine under both schedulers.

No absolute LDA, PLP or SEI executions remain in the mapped-ROM interpreter
profiles outside the excluded wait. Remaining opcode interval costs are led by
BNE (221,694), STA zero page (179,808), STZ absolute-X (147,639), BEQ (137,514)
and AND absolute-X (115,440). Shared conditional branches and stores are the
next priority. These elapsed interval figures guide selection; they are not
exclusive CPU cycle totals. Corpus coverage describes exercised routes rather
than completeness of unvisited paths.

## Verification

`dc_load_status` compares 1,572,864 cases with independently expanded pinned
opcode bodies. It covers every old status byte against every read/restored byte,
all stack positions, both CPU speeds, divided clock remainders, bus stalls,
interrupt arrival horizons and operand-PC wrap. It compares every operand/data
read, clock batch, interrupt sample and mask publication, including intermediate
PC/P/SP/A, final state, RAM, IRQ latch and sampled I.

`dc_execution_accounting_state` verifies restored counter deltas.
`dc_rom_execution_accounting` compares pinned frames, normalized exclusive
ownership and instruction starts between native execution, the original
scheduler, and the original instruction engine. The route corpus additionally
compares device stalls, idle time, IRQs, earlier native-engine counters and
abandonments against the control-flow baseline.

Release passes all six selected checks, including the five-hash ranked-VDC oracle
and the corrupted-face and upper-hand captures. Debug passes all three checks:
differential load/status execution, state accounting and the three-mode gameplay
accounting replay. Registry generation remains current at 932 admissions and
all 399 seam ranges validate.
Artifacts are under `out/native-load-status`, including the previous executable
and `corpus` logs, profiles and frame hashes.
