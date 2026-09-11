# Native HuC6280 loads

Patch `0045-huc6280-native-loads.patch` executes the remaining 18 LDA, LDX
and LDY opcodes through `cpu::run_huc6280_load`. Absolute LDA remains in
the earlier load/status engine, completing all 19 load opcodes. Validated
transcripts retain precedence, the frame wait retains idle ownership, and
`--emulated-boundary` keeps the pinned original opcode engine as the oracle.

## Live ordering

Immediate operands are fetched before the clock advances. Memory loads fetch
their address operands and indirect pointers first, then advance the original
clock batch, sample interrupts, advance the final cycle and read the live bus.
The destination register and NZ flags change after the final cycle; T clears
at retirement. Other flags remain unchanged, including decimal mode.

Zero-page indexing and pointer increments wrap at 256 bytes. Absolute indexing
retains the core's wide effective address. Indirect-indexed LDA increments PC
after both pointer reads; other indirect modes increment it before those reads.
Clock batches before the final cycle are one for immediate, three for zero
page, four for absolute and six for indirect modes. Bus callbacks preserve
device effects and stalls, and scheduler callbacks preserve divided clocks
and interrupt timing.

`Native loads` reports actual shared-engine starts and exclusive CPU cycles
for the newly ported 18 opcodes. Absolute LDA still contributes to
`Native load/status`. Both are subsets of translated ownership. Device stalls
remain separately owned. Counters reset with the accounting window and are
not serialized; no savestate fields are added. The native profiling callback
runs on execution and the original switch is bypassed.

## Measured coverage

The 31-route exclusive ledger transfers **422,835 CPU master cycles** across
**38,546** shared load starts, removing **42.6234%** of remaining non-native
CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,646,782,775 | 20,647,205,610 |
| Retained opcode cycles | 167,385 | 167,385 |
| Fallback opcode cycles | 824,640 | 401,805 |
| Remaining non-native CPU cycles | 992,025 | 569,190 |
| Native share outside waits/stalls | 99.9952% | 99.9972% |

The useful-work denominator remains **20,647,774,800 master cycles**. All 31
routes preserve frame hashes, idle time, device stalls, IRQ counts, prior
native engine counters and abandonments. Other and unaccounted time remain
zero. Translated cycles increase by exactly the new load cycles; fallback
cycles decrease by that amount. Original dispatches decrease by exactly the
shared load count. The three pre-existing single abandonments are unchanged.

Alley transfers **23,205** CPU cycles across **2,417** loads, reaching
**99.9928%** native execution with its pinned frame hash unchanged.

No LDA, LDX or LDY executions remain in mapped-ROM interpreter profiles
outside the excluded wait. Remaining opcode interval costs are led by AND
absolute-X (115,440), AND immediate (77,802), DEC absolute-X (37,845),
BRA (26,472) and CMP immediate (21,972). Logical operations are the next
family to port. These elapsed interval costs guide selection; coverage uses
exclusive ownership. Results describe exercised routes, not completeness of
unvisited paths.

## Verification

`dc_load` compares 9,437,184 cases against the pinned addressing/load macros
and opcode map: 18 opcodes, 256 status bytes, 256 index values and eight
scenarios. It exercises every loaded byte value, both CPU speeds, PC wrap,
wide indexed addresses, pointer wrap, operand aliasing, device stalls,
divided clocks and interrupt horizons. It compares every bus access, clock
batch and sample, intermediate PC/status, final registers and RAM, IRQ latch,
sampled I and clock remainder.

State accounting checks restored counter deltas. The three-mode gameplay
comparison requires identical frames and normalized ownership and dispatches
under native execution, the original scheduler and the original opcode engine.

Release passes all six selected checks, including the five-hash ranked-VDC
oracle and the corrupted-face and upper-hand captures. Debug passes load
differential execution, restored state accounting and the three-mode gameplay
comparison. Registry generation remains current at 932 admissions; all 399
seam ranges validate. The downstream patch passes reverse-application checking.

Artifacts are under `out/native-loads`, including the prior executable,
build/test logs and the 31-route corpus.
