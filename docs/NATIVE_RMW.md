# Native HuC6280 increments, decrements, shifts and rotates

Patch `0047-huc6280-native-rmw.patch` executes 34 opcodes through
`cpu::run_huc6280_rmw`: all INC, DEC, ASL, LSR, ROL and ROR addressing modes,
plus INX, INY, DEX and DEY. Validated transcripts retain precedence;
frame-wait instructions retain idle ownership. `--emulated-boundary` keeps
the original pinned opcode engine available as the oracle.

## Live ordering

Memory operations fetch the effective address, advance five cycles for zero
page or six for absolute addressing, then read the live bus. The operation
updates flags before sampling interrupts and advancing the final cycle. Only
then does it write the result. There is no synthetic dummy write. Zero-page
indexing wraps at 256 bytes; absolute indexing retains wide address arithmetic.

Register operations update the register and flags before advancing one cycle,
sampling interrupts, and advancing the final cycle. INC/DEC preserve carry;
shifts and rotates publish the outgoing bit as carry, and rotates consume the
incoming carry. NZ reflects the result. T clears at retirement without applying
transfer mode; the other flags remain unchanged.

`Native rmw operations` reports actual shared-engine starts and exclusive CPU
cycles, already included in translated ownership. Device stalls remain separate.
Counters reset with the accounting window and are not serialized. No savestate
fields change. The profiling callback records execution, and the shared engine
bypasses the original opcode switch.

## Measured coverage

Across 31 routes, the batch transfers **169,173 CPU master cycles** over
**14,953** shared-engine starts, removing **46.8286%** of the remaining
non-native CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,647,413,540 | 20,647,582,713 |
| Retained opcode cycles | 8,412 | 8,412 |
| Fallback opcode cycles | 352,848 | 183,675 |
| Remaining non-native CPU cycles | 361,260 | 192,087 |
| Native share outside waits/stalls | 99.9983% | 99.9991% |

The denominator remains 20,647,774,800 CPU master cycles. All 31 routes preserve
frame hashes, device stalls, idle time, IRQ counts, prior native counters and
abandonments. Translated ownership increases by exactly the new measured cycles;
original dispatches decrease by exactly the new measured starts. Other and
unaccounted cycles remain zero. Three pre-existing single abandonments remain
unchanged.

Alley transfers 18,657 cycles over 1,682 starts and reaches **99.9975%** native
execution, with 16,551 non-native cycles and its pinned frame hash unchanged.

No covered opcode remains in mapped-ROM interpreter profiles outside the wait.
The remaining elapsed interval ranking starts with BRA (26,472), CMP immediate
(21,972), CMP absolute-X (15,639), TAM (11,700) and PLX (8,388). Comparison and
stack operations, plus the remaining unconditional branch, are the next targets.
These interval costs guide selection; coverage uses exclusive ownership. The
coverage percentage describes the exercised corpus, not all unvisited paths.

## Verification

`dc_rmw` compares 17,825,792 cases against operation, addressing and clock macros
copied from the pinned core: 34 opcodes, all 256 status bytes, all 256 index
values and eight scenarios. Both CPU speeds, divided clocks, device stalls,
interrupt horizons, zero-page wrap, wide absolute indexing and PC wrap are
exercised. Every bus access, clock batch and interrupt sample is compared,
including intermediate PC, status, accumulator and index registers. Final RAM,
registers, IRQ latch, sampled I and clock remainder must match.

State accounting verifies restored counter deltas. The three-mode gameplay
comparison requires identical frames and normalized ownership/dispatch counts
with native execution, the original scheduler and the original opcode engine.

Release passes all six selected checks, including the ranked-VDC oracle and the
corrupted-face and upper-hand captures. Debug passes all three selected checks:
differential execution, restored state accounting and the three-mode gameplay
comparison.
Registry generation remains current at 932 admissions; all 399 seam ranges
validate. The downstream patch passes reverse-application checking.

Build/test logs, the prior executable and the 31-route corpus are under
`out/native-rmw`.
