# Native HuC6280 stores

Patch `0044-huc6280-native-stores.patch` executes all 18 STA, STX, STY
and STZ opcodes through `cpu::run_huc6280_store`. Validated transcripts
retain precedence; the shared engine covers remaining sites across banks.
`--emulated-boundary` retains the pinned original opcode switch as the oracle.
Frame-sync wait ownership is preserved.

## Live ordering

The engine fetches operands through the live bus and calculates zero-page,
indexed, absolute and indirect destinations. Zero-page indexing and pointer
increments wrap at 256 bytes. Absolute indexing retains the pinned core's
wide effective address. Indirect-indexed STA increments PC after both pointer
reads; other indirect modes increment it before those reads.

Zero-page stores advance three CPU cycles before sampling interrupts;
absolute stores advance four and indirect stores six. Each then advances the
final cycle, writes the live register value (or zero), and clears T. The bus
adapter retains device write effects and stalls, while scheduler callbacks
preserve clock batches, divided-clock remainders and interrupt timing.

`Native stores` reports actual shared-engine starts and exclusive CPU cycles.
Those cycles are already included in translated ownership; device stalls
remain separately owned. Counters reset with the accounting window and are
not serialized. The native profiling callback runs when the engine executes,
and the original switch is bypassed. No savestate fields are added.

## Measured coverage

The 31-route exclusive ledger transfers **649,431 CPU master cycles** across
**45,917** shared store starts, removing **39.5643%** of remaining non-native
CPU time.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,646,133,344 | 20,646,782,775 |
| Retained opcode cycles | 281,910 | 167,385 |
| Fallback opcode cycles | 1,359,546 | 824,640 |
| Remaining non-native CPU cycles | 1,641,456 | 992,025 |
| Native share outside waits/stalls | 99.9921% | 99.9952% |

The useful-work denominator remains **20,647,774,800 master cycles**. All 31
routes preserve frame hashes, idle time, device stalls, IRQ counts, prior native
engine counters and abandonments. Other and unaccounted time remain zero.
Translated cycles increase by exactly the store cycles; retained plus fallback
cycles decrease by that amount. Original dispatches decrease by exactly the
shared store count. The three pre-existing single abandonments are unchanged.

Alley transfers **30,648** CPU cycles across **2,223** stores, reaching
**99.9894%** native execution with its pinned frame hash unchanged.

No STA, STX, STY or STZ executions remain in mapped-ROM interpreter profiles
outside the excluded wait. Remaining opcode interval costs are led by AND
absolute-X (115,440), LDA immediate (106,422), LDA absolute-X (105,831),
LDA zero page (89,736) and AND immediate (77,802). The remaining load modes
are the next family to port, followed by logical operations. These elapsed
interval costs guide selection; coverage uses exclusive ownership.
The measurement describes exercised routes, not completeness of unvisited paths.

## Verification

`dc_store` compares 9,437,184 cases against the pinned addressing/store macros
and opcode map: all 18 opcodes, all 256 status bytes, all 256 index values and
eight scenarios. It covers both CPU speeds, PC wrap, wide indexed addresses,
zero-page pointer wrap, operand aliasing, device stalls, divided clocks and
interrupt horizons. Every read, write, clock batch and interrupt sample is
compared, including intermediate PC/status and final RAM, registers, IRQ latch,
sampled I and clock remainder.

State accounting checks restored store-counter deltas. The three-mode gameplay
test compares native execution, the original scheduler and the original opcode
engine, requiring identical frames and normalized ownership and dispatches.

Release passes all six selected checks, including the five-hash ranked-VDC
oracle and the corrupted-face and upper-hand captures. Debug passes store
differential execution, state accounting and the three-mode gameplay comparison.
Registry generation remains current at 932 admissions; all 399 seam ranges
validate. The downstream patch passes reverse-application verification.

Artifacts are under `out/native-stores`, including the previous executable,
build/test logs and the 31-route corpus.
