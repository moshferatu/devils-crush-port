# Native HuC6280 conditional branches

Patch `0043-huc6280-native-branches.patch` executes BPL, BMI, BVC, BVS,
BCC, BCS, BNE and BEQ through `cpu::run_huc6280_branch`. Accepted
transcripts retain precedence; the shared implementation covers remaining
sites across banks. `--emulated-boundary` keeps the pinned original opcode
switch as the oracle. Frame-sync wait instructions retain idle ownership.

## Live ordering

The native engine decodes the condition from the live status byte. A taken
branch reads its signed displacement, increments the wide PC, advances three
CPU cycles and publishes the target. An untaken branch advances one cycle and
skips the displacement without reading it. Both paths sample interrupts,
advance the final cycle and clear T. The engine uses the live operand bus and
scheduler callbacks, preserving clock batches, divided-clock remainders and
interrupt arrival timing.

`Native branches` reports actual instruction starts and exclusive CPU cycles.
The cycles are already included in translated ownership; device stalls retain
their separate ownership. Counters reset with the accounting window and are
not serialized. Native profiling occurs when the shared engine runs, and the
original switch is bypassed. No savestate fields are added.

## Measured coverage

The 31-route exclusive ledger transfers **389,418 CPU master cycles** across
**43,293** shared branch starts, removing **19.1749%** of remaining non-native
CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,645,743,926 | 20,646,133,344 |
| Retained opcode cycles | 405,012 | 281,910 |
| Fallback opcode cycles | 1,625,862 | 1,359,546 |
| Remaining non-native CPU cycles | 2,030,874 | 1,641,456 |
| Native share outside waits/stalls | 99.9902% | 99.9921% |

The useful-work denominator remains **20,647,774,800 master cycles**. All 31
routes preserve frame hashes, idle time, device stalls, IRQ counts, prior native
engine counters and abandonments. Other and unaccounted time remain zero.
Translated cycles increase by exactly the branch cycles; retained plus fallback
cycles decrease by that amount. Original dispatches decrease by exactly the
shared branch count. The three pre-existing single abandonments are unchanged.

Alley transfers **31,500** CPU cycles across **3,843** branches, reaching
**99.9849%** native execution with its pinned frame hash unchanged.

No conditional branch executions remain in mapped-ROM interpreter profiles
outside the excluded wait. Remaining opcode interval costs are led by STA zero
page (179,808), STZ absolute-X (147,639), AND absolute-X (115,440), LDA immediate
(106,422) and LDA absolute-X (105,831). Shared stores are the next priority.
These elapsed interval costs guide selection; coverage uses exclusive ownership.
The measurement describes exercised routes, not completeness of unvisited paths.

## Verification

`dc_branch` compares 6,291,456 cases against independently expanded pinned
opcode conditions and JR semantics: eight opcodes, all 256 status bytes, all
256 displacement bytes and twelve scenarios. Each PC location runs at both CPU
speeds, including operand wrap, target underflow/overflow, divided clocks,
bus stalls and interrupt horizons. It compares every operand read, clock batch,
interrupt sample and intermediate state, plus final registers, RAM, IRQ latch,
sampled I and clock remainder.

State accounting checks restored branch counter deltas. The three-mode gameplay
test compares the native engine, original scheduler and original opcode engine,
requiring identical frames and normalized exclusive ownership and dispatches.

Release passes all six selected checks, including the five-hash ranked-VDC
oracle and the corrupted-face and upper-hand captures. Debug passes branch
differential execution, state accounting and the three-mode gameplay comparison.
The expanded 6,291,456-case differential suite passes separately in both builds.
Registry generation remains current at 932 admissions; all 399 seam ranges
validate. The downstream patch passes reverse-application verification.

Artifacts are under `out/native-branches`, including the previous executable,
build/test logs, and the 31-route corpus.
