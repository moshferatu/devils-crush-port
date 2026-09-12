# Native HuC6280 mapping, register and flag operations

Patch `0050-huc6280-native-register.patch` implements 22 opcodes through
`cpu::run_huc6280_register`: TAM/TMA, TAX/TXA/TAY/TYA/TSX/TXS, SXY/SAX/SAY,
CLA/CLX/CLY, CLC/SEC/CLI/CLV/CLD/SED, SET and NOP. Validated transcripts retain
precedence, frame-wait instructions retain idle ownership, and
`--emulated-boundary` retains the pinned original opcode oracle.

## Live ordering

TAM and TMA fetch the immediate mask before changing mappings or A. TAM calls
the core's cache-aware SetMPR for selected banks in ascending order, then always
refreshes MPR[8] from MPR[0], even for a zero mask. TMA reads selected banks in
ascending order, so the highest selected bank wins; a zero mask preserves A.
Neither changes NZ. Mapping changes precede the clock advance and sample.
TAM takes five CPU cycles; TMA takes four.

Register transfers update their destination and NZ before advancing one cycle,
sampling interrupts and advancing the final cycle. TXS preserves NZ. Swaps
preserve flags and update both registers before advancing two cycles, sampling
and advancing the final cycle. Register clears happen after the final cycle
and preserve NZ.

Flag operations advance one cycle, sample interrupts, advance the final cycle
and then change the flag. CLI publishes the new mask after that sample. SET
preserves its newly set T bit; all other instructions clear T at retirement.
NOP advances and samples without changing registers.

`Native register/mapping operations` reports actual starts and exclusive CPU
cycles already included in translated ownership. Device stalls remain separate.
The counters reset with the accounting window and are not serialized.

## Measured coverage

Across 31 routes, this batch transfers **45,567 CPU master cycles** over
**5,219 starts**, removing **63.7657%** of the remaining non-native CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,647,703,340 | 20,647,748,907 |
| Retained opcode cycles | 0 | 0 |
| Fallback opcode cycles | 71,460 | 25,893 |
| Native share outside waits/stalls | 99.9997% | 99.9999% |

The denominator remains 20,647,774,800 CPU master cycles. All routes preserve
frame hashes, stalls, idle time, IRQ counts, earlier native counters and
abandonments. Translated ownership increases by exactly the new measured cycles;
original dispatches decrease by exactly the new measured starts. Other and
unaccounted cycles remain zero. Three pre-existing single abandonments remain
unchanged.

Alley transfers 3,672 cycles over 536 starts and reaches **99.9997%** native
execution, with 2,340 fallback CPU cycles and its frame hash unchanged.

None of these 22 opcodes remains in mapped-ROM interpreter profiles outside
the wait. Remaining elapsed interval costs begin with ADC zero page (4,464),
ADC immediate (3,978), SBC absolute (3,750), SBC immediate (2,904), CSH (2,511)
and CSL (2,232). Arithmetic and CPU-speed changes are next. These intervals
guide selection; coverage uses exclusive ownership and describes exercised
paths, not completeness of unvisited ROM code.
## Verification

`dc_register` compares 11,534,336 cases with the pinned register/mapping macros
and flag/clear opcode bodies: 22 opcodes, all 256 status bytes, all 256 data/mask
values and eight scenarios. These cover zero and multiple mapping selections,
the bank-zero mirror, both CPU speeds, divided clocks, operand stalls, interrupt
horizons and PC wrapping. The test compares every mapping read/write, operand
access, clock batch, interrupt sample and mask publication, including
intermediate register/status values. Final mappings, registers, RAM, clock
remainder and IRQ state must agree.

State accounting checks restored counter deltas. Gameplay checks compare native
execution with the original scheduler and original opcode engine, including
frames and exact ownership/dispatch normalization. Build/test logs, the prior
executable and the 31-route replay corpus are under `out/native-register`.

All six selected Release checks and all three selected Debug checks pass,
including the Release ranked-VDC, corrupted-face and upper-hand regressions.
Registry generation remains current at 932 admissions, all 399 classified seam
ranges validate, and the downstream patch passes reverse-application checking.
