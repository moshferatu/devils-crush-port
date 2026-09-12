# Native HuC6280 comparisons

Patch `0048-huc6280-native-compare.patch` executes all 15 CMP, CPX and CPY
addressing forms through `cpu::run_huc6280_compare`. Validated transcripts retain
precedence, frame-wait instructions retain idle ownership, and
`--emulated-boundary` keeps the pinned original opcode engine as the oracle.

## Live ordering

The engine fetches the operand address and advances one cycle for immediate,
three for zero page, four for absolute or six for indirect addressing. It then
samples interrupts, advances the final cycle and reads the data operand (unless
immediate). Only then do N, Z and C change; registers, decimal and overflow flags
are preserved. T clears at retirement without redirecting the comparison to
zero-page X.

Zero-page indexing and indirect pointers wrap at 256 bytes. Absolute indexing
retains wide arithmetic. Indirect-Y advances the operand PC after its pointer
reads, preserving the pinned bus trace at wrap boundaries.

`Native compare operations` counts actual engine starts and exclusive CPU
cycles, already included in translated ownership. Device stalls remain separate.
Counters reset with the accounting window and are not serialized; no savestate
format changes. The profiling callback records native execution and the engine
bypasses the original opcode switch.

## Measured coverage

Across 31 routes, the batch transfers **51,051 CPU master cycles** over
**5,234** shared-engine starts, removing **26.5770%** of the remaining
non-native CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,647,582,713 | 20,647,633,764 |
| Retained opcode cycles | 8,412 | 0 |
| Fallback opcode cycles | 183,675 | 141,036 |
| Remaining non-native CPU cycles | 192,087 | 141,036 |
| Native share outside waits/stalls | 99.9991% | 99.9993% |

The denominator remains 20,647,774,800 CPU master cycles. All 31 routes preserve
frame hashes, device stalls, idle time, IRQ counts, earlier native counters and
abandonments. Translated ownership increases by exactly the new measured cycles;
original dispatches decrease by exactly the new measured starts. Other and
unaccounted cycles remain zero. Three pre-existing single abandonments remain
unchanged.

Alley transfers 5,466 cycles over 803 starts and reaches **99.9984%** native
execution, with 11,085 fallback cycles and its pinned frame hash unchanged.
It no longer executes retained opcodes, so the accounting regression now asserts
zero retained cycles in native modes while requiring the original oracle to
exercise retained execution. Exact cycle and dispatch reconciliation still apply.

No covered comparison opcode remains in mapped-ROM interpreter profiles outside
the wait. The remaining elapsed interval ranking starts with BRA (26,472),
TAM (11,700), PLX (8,388), PHA (7,998) and PLA (7,416). Stack operations and
BRA are the next groups. These intervals guide selection; coverage uses exclusive
ownership. The percentage describes the exercised corpus, not all unvisited paths.

## Verification

`dc_compare` compares 7,864,320 cases against addressing, comparison and clock
macros copied from the pinned core: 15 opcodes, all 256 status bytes, all 256
index values and eight scenarios. Both CPU speeds, divided clocks, device
stalls, interrupt horizons, zero-page wrap, wide indexing and PC wrap are
exercised. Every bus access, clock batch and interrupt sample must match,
including intermediate PC, status, accumulator and index registers. Final RAM,
registers, IRQ latch, sampled I and clock remainder must also match.

State accounting checks restored counter deltas. The three-mode gameplay test
compares frames and normalized ownership/dispatch counts under native execution,
the original scheduler and the original opcode engine.

All six selected Release checks and all three selected Debug checks pass,
including the Release ranked-VDC, corrupted-face and upper-hand regressions.
The initial accounting runs rejected zero retained cycles; after correcting that
obsolete fixture assertion, the accounting reruns pass in both configurations.
Their logs are `accounting-release.log` and `accounting-debug.log`; the initial
suite logs preserve the earlier assertion failure. No runtime change was needed.
Registry generation remains current at 932 admissions, all 399 classified seam
ranges validate, and the downstream patch passes reverse-application checking.

Build/test logs, the prior executable and the measured corpus are under
`out/native-compare`.
