# Native HuC6280 bit-test branches

Patch `0040-huc6280-native-bit-branches.patch` executes all sixteen BBR0–BBR7
and BBS0–BBS7 instructions through `cpu::run_huc6280_bit_branch`. It handles
every bank and mapped zero-page access, after existing accepted transcripts
have had precedence. `--emulated-boundary` retains the original engine as an
independent gameplay oracle.

The native routine owns operand decoding, bit selection, branch decisions,
PC arithmetic and T-flag retirement. The adapter supplies live mapped reads,
clock advances and interrupt sampling. It bypasses the original opcode switch,
`LD_ZP`, `BBRi`/`BBSi` and `JR` bodies.

Ordering matches the pinned core: fetch the zero-page operand, advance three
cycles, sample interrupts, advance one cycle, then read the mapped byte. A taken
branch fetches the signed displacement, increments PC, advances three cycles,
and adds the displacement before its second interrupt sample and final cycle.
An untaken branch advances one cycle and increments PC without reading the
displacement, then samples and advances its final cycle. Both paths clear T
after retirement. PC remains wide until bus mapping, including address wrap.
Clock batching, speed conversion, overclock remainder and device stalls remain
live. No new savestate fields are required.

## Measured coverage

The corrected profiles ranked BBR0, BBR3 and BBS2 at about 13.3 million elapsed
instruction-interval cycles. The exclusive ledger measures the actual saving:
**13,184,280 CPU master cycles**, or **33.4059%** of the remaining non-native CPU
time after the shared block-transfer batch.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,608,307,901 | 20,621,492,181 |
| Retained opcode cycles | 31,370,997 | 19,151,889 |
| Fallback opcode cycles | 8,095,902 | 7,130,730 |
| Remaining non-native CPU cycles | 39,466,899 | 26,282,619 |
| Native share outside waits/stalls | 99.8089% | 99.8727% |

The denominator remains **20,647,774,800 master cycles**. All 31 routes preserve
frames, device stalls, idle cycles, IRQ counts, native block-transfer counters
and abandonments. Other and unaccounted cycles remain zero. Original dispatches
decrease by exactly **628,608**, the number of shared native branch executions.
The three pre-existing single abandonments remain unchanged.

Alley transfers 431,490 CPU cycles across 20,594 branches and reaches
**99.8496%** native execution. Native/core engines and both schedulers preserve
the pinned frame hash and agree on normalized execution ownership and total
instruction starts.

Artifacts: `out/native-bit-branches/corpus`, including per-route frames,
profiles, logs, `manifest.csv` and `totals.json`. The pre-change executable is
`out/native-bit-branches/devils-crush-before.exe`.

## Accounting and verification

`Native bit branches` reports instruction count and exclusive CPU cycles.
These cycles are already part of `translated`; they are not an additional
ledger owner. Native starts are separate from transcript installations and
original opcode dispatches. The profiler marks them native only when this
engine executes them. Counters reset with the measurement window and state
restore; they are not serialized.

- `dc_bit_branch`: **2,097,152** differential cases cover all sixteen opcodes,
  every byte value and signed displacement, every status byte, both CPU speeds,
  overclock division, injected read stalls, PC wrap and interrupt arrival
  horizons. Compares every bus access, clock batch, sample, intermediate PC/P,
  final state and clock remainder against the original core ordering.
- `dc_execution_accounting_state`: checks restored branch counts and CPU-cycle
  deltas alongside the existing reset, state replay and transfer handoff checks.
- `dc_rom_execution_accounting`: native/core engines and both schedulers agree
  on pinned frames, normalized CPU ownership and instruction starts. The native
  BBR0 site at `00:F423` disappears from interpreter residue.
- Release: all six selected checks passed, including the five-hash ranked-VDC
  oracle and corrupted-face/upper-hand captures. The final expanded state check
  also passed. Registry generation remains current at 932 admissions and all
  399 seam ranges validate.
- Debug: differential branches, state accounting and the three-mode gameplay
  accounting replay all passed (3/3).

Next priority is the shared call/return/jump paths, guided by the remaining
profiles and measured with the exclusive ledger. Corpus coverage describes
exercised routes; it does not establish completeness of unvisited ROM paths.
