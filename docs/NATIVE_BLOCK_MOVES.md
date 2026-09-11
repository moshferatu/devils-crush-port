# Native HuC6280 block transfers

The shared native transfer engine now executes TIA, TAI, TDD, TII and TIN.
Patch `0039-huc6280-native-block-moves.patch` connects
`cpu::run_huc6280_block_move` to live CPU registers, mapped memory, divided
clock advances and interrupt sampling. It runs when the native boundary is
installed; `--emulated-boundary` selects the original transfer and IRQ engines.
Existing accepted transcripts retain precedence over shared instruction dispatch.

## Execution and compatibility

The native engine owns operand decoding, stack pushes/pops, transfer addressing,
length and alternate-byte updates, suspension and retirement. The adapter supplies
primitive bus/clock operations; the original `LD_BMT` and `BMT_*` bodies are
bypassed on this path. No per-address admission is required.

| Mode | Source progression | Destination progression |
| --- | --- | --- |
| TIA | Increment | Alternate between two addresses |
| TAI | Alternate between two addresses | Increment |
| TDD | Decrement | Decrement |
| TII | Increment | Increment |
| TIN | Increment | Fixed |

Ordering remains the pinned core's: push Y/A/X, fetch three operands, advance
14 CPU cycles, then advance six cycles before each byte read/write. Retirement
clears the transfer mode, pops X/A/Y, advances two cycles, samples interrupts,
advances one final cycle and clears T. Device stalls remain separate clock
ownership. Clock advances retain their original batching, speed conversion and
overclock remainder.

Memory stays live, including overlapping transfers, stack aliases, mapped
devices and address wrapping. An encoded zero length transfers 65,536 bytes.
Alternating address sums stay wide until they reach the mapped bus.

The existing savestate fields and mode numbers are unchanged. At a scheduler
yield, a byte has already been written and the addresses advanced, but length
has not yet decremented. Resumption performs that decrement first, including
when the suspended byte was the last one. There is no extra opcode dispatch or
interrupt sample at a yield. Both engines can resume each other's saved states
or take over a live suspended transfer. A live handoff to the original engine
changes subsequent ownership to fallback.

## Measured coverage

The corrected profiles ranked TIA/TII first, with about 27.4 million elapsed
instruction-interval cycles across the 31-route corpus. Those intervals include
device and intervening work. The exclusive execution ledger measures the actual
CPU saving: **24,670,854 master cycles**, removing **38.4654%** of the remaining
non-native CPU time after native IRQ entry.

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,583,637,047 | 20,608,307,901 |
| Retained opcode cycles | 54,845,964 | 31,370,997 |
| Fallback opcode cycles | 9,291,789 | 8,095,902 |
| Remaining non-native CPU cycles | 64,137,753 | 39,466,899 |
| Native share outside waits/stalls | 99.6894% | 99.8089% |

The denominator remains **20,647,774,800 master cycles**. Every route preserves
its frames, device stalls, idle cycles, IRQ counts and abandonment count. Other
and unaccounted cycles remain zero. The three pre-existing single abandonments
remain unchanged. Original opcode dispatches decrease by exactly the number of
native transfer starts: 899 TIA, 155 TII and 62 TIN, or **1,116** total.
TAI and TDD are covered by the differential fixture, but were not reached in
these gameplay routes.

Alley transfers 795,834 CPU cycles to native execution, raising its measured
share from 99.6679% to **99.7857%**. Native/core engines and both schedulers agree
on its pinned frame hash and normalized ownership totals. The corpus has no
frame-spanning transfers; a dedicated test supplies that coverage.

Artifacts are in `out/native-block-moves/corpus`: per-route frame/profile/log
outputs, `manifest.csv` and `totals.json`. The comparison executable is
`out/native-block-moves/devils-crush-before.exe`.

## Accounting and verification

`Native block moves` reports starts by mode, resume count and exclusive CPU
cycles, including resumed portions. These diagnostics reset with the measurement
window and are not serialized. The CPU cycles are a subset of `translated`,
not an additional owner or accepted-body estimate. Starts are separate from
transcript installations and original opcode dispatches. The profiler marks
native transfers only when the shared engine actually executes them.

- `dc_block_move`: **10,300** differential transfers compare every bus access,
  clock batch, interrupt sample, intermediate register, final memory and yield
  state against the original core ordering. Covers all five modes, every status
  and stack byte, both CPU speeds, overclock division, injected read/write
  stalls, overlapping and stack-aliasing copies, operand/address wrap, encoded
  zero, and first/middle/final-byte suspension.
- `dc_execution_accounting_state`: reset/window checks plus a deterministic
  65,536-byte slow-speed TIN program. Real frame-boundary states resume through
  both engines with matching pixels, RAM, frame timing and device stalls. Also
  checks live engine handoffs without state restoration.
- `dc_rom_execution_accounting`: pinned alley frame oracle, native/core engines,
  both schedulers, exact CPU ownership transfer, matching total instruction
  starts and actual interpreter-profile membership.
- Existing five-hash ranked-VDC oracle and corrupted-face/upper-hand capture
  regressions pass. All 31 deterministic route comparisons pass.

Release: all six selected regression checks passed; the final accounting,
transfer and expanded state checks passed again (3/3). Debug: transfer fixture,
three-mode accounting replay and the expanded state test passed. Registry
generation remains current at 932 admissions, and all 399 seam ranges validate.

The next priority is shared bit-test branches, followed by the shared
call/return/jump paths, using the remaining profiles to guide work and the
exclusive ledger to measure savings. The aggregate above describes exercised
routes, not completeness of unvisited ROM paths.
