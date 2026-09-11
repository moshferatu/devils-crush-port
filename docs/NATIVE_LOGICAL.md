# Native HuC6280 logical operations

Patch `0046-huc6280-native-logical.patch` executes all 27 AND, ORA and EOR
opcodes through `cpu::run_huc6280_logical`. Validated transcripts retain
precedence, frame-wait instructions retain idle ownership, and
`--emulated-boundary` preserves the pinned original opcode engine as the oracle.

## Live ordering

The engine fetches operands and indirect pointers before advancing the original
clock batch, sampling interrupts and advancing the final cycle. Memory operands
are then read from the live bus. Zero-page indexing and pointers wrap at 256
bytes; absolute indexing retains the core's wide address arithmetic.
Indirect-indexed addressing increments PC after both pointer reads.

With T clear, the operation updates A and NZ, preserving the other flags.
With T set, the already-fetched source operand is retained while an additional
cycle precedes reading the destination at `$2000 + X`. The operation temporarily
updates A and NZ. Another cycle precedes writing the result back to that
destination; a second interrupt sample and final cycle precede restoring A.
T clears only at retirement. This preserves both sampled flag states and the
transient accumulator visible during bus and scheduler callbacks. Decimal mode
does not alter logical operations.

`Native logical operations` reports actual shared-engine starts and exclusive
CPU cycles, already included in translated ownership. Device stalls remain
separately owned. Counters reset with the accounting window and are not
serialized; no savestate fields change. The native profiling callback runs on
execution and the original opcode switch is bypassed.

## Measured coverage

The 31-route exclusive ledger transfers **207,930 CPU master cycles** across
**21,761** shared logical starts, removing **36.5309%** of remaining non-native
CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,647,205,610 | 20,647,413,540 |
| Retained opcode cycles | 167,385 | 8,412 |
| Fallback opcode cycles | 401,805 | 352,848 |
| Remaining non-native CPU cycles | 569,190 | 361,260 |
| Native share outside waits/stalls | 99.9972% | 99.9983% |

The useful-work denominator remains **20,647,774,800 master cycles**. All 31
routes preserve frame hashes, idle time, device stalls, IRQ counts, prior
native engine counters and abandonments. Other and unaccounted time remain
zero. Translated cycles increase by exactly the new logical cycles; retained
and fallback cycles decrease by that amount. Original dispatches decrease by
exactly the shared logical count. The three pre-existing single abandonments
are unchanged.

Alley transfers **13,185** CPU cycles across **2,013** logical instructions,
reaching **99.9948%** native execution with its pinned frame hash unchanged.

No AND, ORA or EOR executions remain in mapped-ROM interpreter profiles
outside the excluded wait. Remaining opcode interval costs are led by DEC
absolute-X (37,845), BRA (26,472), CMP immediate (21,972), INY (20,202),
DEC absolute (18,426) and INC absolute-X (17,808). Increment/decrement and
related read/modify/write operations are the next batch. These elapsed interval
costs guide selection; coverage uses exclusive ownership. Results describe
exercised routes, not completeness of unvisited paths.

## Verification

`dc_logical` compares 14,155,776 cases against the pinned core's addressing,
load, logical and transfer-mode macros and opcode map: 27 opcodes, all 256
status bytes, all 256 index values and eight scenarios. It exercises both CPU
speeds, source/destination aliasing, PC and pointer wrap, wide indexed addresses,
device stalls, divided clocks and interrupt horizons. It compares every bus
access, clock batch and interrupt sample, intermediate PC/status/accumulator,
final registers and RAM, IRQ latch, sampled I and clock remainder. Transfer mode
must sample twice; ordinary execution must sample once.

State accounting checks restored counter deltas. The three-mode gameplay
comparison normalizes ownership and dispatches using the new measured counters
and requires identical frames with native execution, the original scheduler and
the original opcode engine.

Release passes all six selected checks, including the five-hash ranked-VDC
oracle and the corrupted-face and upper-hand captures. Debug passes logical
differential execution, restored state accounting and the three-mode gameplay
comparison. Registry generation remains current at 932 admissions; all 399
seam ranges validate. The downstream patch passes reverse-application checking.

Artifacts are under `out/native-logical`, including the prior executable,
build/test logs and the 31-route corpus.
