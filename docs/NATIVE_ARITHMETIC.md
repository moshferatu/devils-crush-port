# Native HuC6280 arithmetic and CPU-speed operations

Patch `0051-huc6280-native-arithmetic.patch` implements all nine ADC and nine
SBC addressing forms plus CSL and CSH through `cpu::run_huc6280_arithmetic`.
Validated transcripts retain precedence, frame-wait instructions retain idle
ownership, and `--emulated-boundary` retains the pinned original opcode oracle.

## Live ordering

Operand reads use the same live bus and address arithmetic as the pinned core:
zero-page pointers wrap at eight bits, absolute indexing remains wide, and
indirect-Y advances PC after reading the pointer. The addressing cycle batch
and interrupt sample precede the final cycle and data read.

Binary ADC/SBC update NZCV. Decimal arithmetic preserves V, calculates NZ from
the adjusted byte, and samples interrupts again before its extra cycle. The
implementation matches the pinned core for invalid BCD digits as well.

Only ADC supports transfer mode. After fetching the operand it advances one
cycle and reads the temporary accumulator from zero page at X. Following
arithmetic, it advances one cycle, writes the result, samples interrupts,
advances another cycle and restores A. Decimal transfer-mode ADC therefore
samples three times. SBC ignores transfer mode. Both clear T at retirement.

CSL and CSH advance two cycles at the old speed, call the core's live speed
change, sample interrupts and advance the final cycle at the new speed. They
preserve clock remainders and clear T at retirement.

`Native arithmetic/speed operations` reports actual starts and exclusive CPU
cycles already included in translated ownership. Device stalls remain separate.
The counters reset with the accounting window and are not serialized.

## Measured coverage

Across 31 routes, this batch transfers **23,883 CPU master cycles** over
**1,877 starts**, removing **92.2373%** of the remaining non-native CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,647,748,907 | 20,647,772,790 |
| Retained opcode cycles | 0 | 0 |
| Fallback opcode cycles | 25,893 | 2,010 |
| Native share outside waits/stalls | 99.9999% | 99.9999903% |

The denominator remains 20,647,774,800 CPU master cycles. All routes preserve
frame hashes, stalls, idle time, IRQ counts, earlier native counters and
abandonments. Translated ownership increases by exactly the new measured cycles;
original dispatches decrease by exactly the new measured starts. Other and
unaccounted cycles remain zero. Three pre-existing single abandonments remain
unchanged.

Alley transfers 2,259 cycles over 241 starts, leaving 81 fallback CPU cycles
with its frame hash unchanged. The four-decimal CLI percentage rounds to 100%;
the exact ownership ledger still records the remaining fallback execution.

None of these 20 opcodes remains in mapped-ROM interpreter profiles outside
the wait. Remaining elapsed interval costs are TST opcode B3 (1,536), BIT zero
page (372), SMB1 (84) and BIT immediate (18). Those instruction families are
next. Intervals guide selection; coverage uses exclusive ownership and describes
exercised paths, not completeness of unvisited ROM code.

## Verification

`dc_arithmetic` compares 12,582,912 cases with the pinned arithmetic macros:
20 opcodes, every status byte and data value across eight bus/clock scenarios,
plus every accumulator/operand pair with independent C/D/V/T flags for ADC
and SBC. Cases include invalid BCD digits, transfer-mode aliases, both CPU
speeds, divided clocks, bus stalls, interrupt horizons and PC wrapping.
The test compares every bus access, clock batch and interrupt sample, including
intermediate accumulator/status values, plus final memory, registers, speed,
clock remainder and IRQ state.

State accounting checks restored counter deltas. Gameplay checks compare native
execution with both the original scheduler and original opcode engine, including
frames and exact ownership/dispatch normalization. Build/test logs, the prior
executable and the replay corpus are under `out/native-arithmetic`.

All six selected Release checks and all three selected Debug checks pass,
including the Release ranked-VDC, corrupted-face and upper-hand regressions.
Registry generation remains current at 932 admissions, all 399 classified seam
ranges validate, and the downstream patch passes reverse-application checking.
