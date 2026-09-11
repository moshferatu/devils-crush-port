# Native HuC6280 interrupt entry

The shared maskable-interrupt entry now runs through
`cpu::enter_huc6280_interrupt` when the native interrupt boundary is installed.
Patch `0038-huc6280-native-interrupt-entry.patch` connects the native routine to
the machine's live registers, memory bus, divided clock and interrupt sampler.
`--emulated-boundary` retains the original entry sequence for comparison.
Reset continues through its separate existing path.

The native routine owns all eight cycles and their ordering:

| CPU cycle | Operation after the advance |
| --- | --- |
| 1 | Dummy read at PC |
| 2 | Dummy read at PC + 1 |
| 3 | Push PC high, decrement S |
| 4 | Push PC low, decrement S |
| 5 | Push P with B clear, decrement S; set I, publish its mask, clear T and D |
| 6 | Read vector low into live PC |
| 7 | Read vector high into live PC; sample interrupt lines |
| 8 | Final internal cycle |

The routine uses references to live PC, S and P so device callbacks observe
each intermediate state. Stack addressing and wrapping belong to the native
routine; mapping, bus side effects and device stalls use the existing machine
interfaces. Each cycle advances separately through the selected scheduler,
preserving speed conversion, overclock remainder and event ordering. No
instruction dispatch or interrupt service boundary is introduced inside entry.

## Execution accounting

Native entry advances belong to `translated`; nested device stalls still belong
to `device`. `interrupt` measures only the original core entry. Separate
`Native interrupt entries` and `Core interrupt entries` counters establish which
implementation actually ran. Neither counter is serialized. Native entries do
not count as transcript installations or original opcode dispatches.

The 31-route corpus transfers exactly **190,693,128 master cycles** from original
IRQ entry to native execution:

| Measurement | Before | After |
| --- | ---: | ---: |
| Native CPU cycles | 20,392,943,919 | 20,583,637,047 |
| Remaining non-native CPU cycles | 254,830,881 | 64,137,753 |
| Original IRQ-entry cycles | 190,693,128 | 0 |
| Native IRQ entries | 0 | 7,945,547 |
| Core IRQ entries | 7,945,547 | 0 |
| Native share outside waits/stalls | 98.7658% | 99.6894% |

The denominator remains **20,647,774,800 master cycles**. This removes **74.8312%**
of the measured remaining non-native CPU time. All 31 frame hashes, abandonment
counts and original opcode counts match the accounting baseline. Retained,
fallback, idle and device cycles are unchanged on every route; unidentified and
unaccounted cycles are zero. Three routes retain their single pre-existing
abandonment. Artifacts are in `out/native-irq-entry/corpus`, including the
per-route manifest and aggregate `totals.json`.

The alley route alone moves 6,399,384 cycles across 266,641 entries, increasing
native coverage from 98.7204% to 99.6679%. Native and original entry produce the
same fixed frame hash, original opcode counts and total CPU work. Native entry
also agrees under both scheduler implementations.

## Verification

- `dc_interrupt_entry`: 786,432 entries covering every status and stack byte,
  all three maskable vectors, PC boundary values, both CPU speeds, injected
  read/write stalls, intermediate registers and exact sampling order.
- `dc_execution_accounting_state`: core and native entries across reset and
  savestate restore, including counter deltas, replayed RAM and clock ownership.
- `dc_rom_execution_accounting`: fixed alley frame oracle, native/core entry
  selection, both clocks, independent boundary counts, zero retained IRQ time
  on native paths, and exact transfer of measured cycles between owners.
- Existing ranked-VDC oracle: all five pinned frame, call, scoped-write,
  work-RAM-write and device-bus-write hashes.
- Existing corrupted-face and upper-hand capture regressions.
- All 31 deterministic routes, checked against the execution-accounting build.

Completed validation: Release **7/7** checks passed (six regression checks in
85.75 seconds plus the state test in 0.49 seconds); Debug **4/4** passed in
272.78 seconds. Registry generation remains current at 932 admissions, all
399 seam ranges validate, and patch 0038 reverse-applies cleanly to the tested
core. Build and test logs are in `out/native-irq-entry`.

The next coverage work is shared retained/fallback instruction families. The
remaining measured CPU time consists of 54,845,964 retained-opcode cycles and
9,291,789 fallback-opcode cycles. Coverage describes these exercised routes;
unvisited ROM paths still require their own evidence.
