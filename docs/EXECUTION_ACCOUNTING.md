# Execution accounting

The primary coverage report now measures actual core clock advances. It replaces
accepted replacement-body estimates as the progress metric. Capabilities still
executed by the original core remain visible as original opcode execution, even
when the host offered and credited them as native.

## The measurement window

The ledger starts immediately after ROM load, reset or successful state restore.
Its elapsed time is the difference between the core's absolute master clock at
that point and at reporting time. Timestamp rebases do not restart it. The
ledger is diagnostic state and is not serialized into game savestates.

Every divided clock advance is assigned exactly once to one of these owners:

| Owner | Meaning |
| --- | --- |
| translated | Actual advances in native IRQ entry, native block transfers, native bit-test branches, native control flow, native absolute loads/status instructions, native conditional branches, native stores, native loads, native logical operations, native read/modify/write operations, native comparisons, native stack operations/BRA, native mapping/register/flag operations, native arithmetic/speed operations or a validated transcript/checkpoint |
| retained | Original opcode path following a recognized, validated core-capability offer |
| fallback | Original opcode path without an installed translation or validated capability |
| interrupt | Original core IRQ-entry sequence, including its final sample |
| idle | Actual instruction/skip advances at the supported frame-wait range `00:E321–E32B` |
| device | Stolen cycles, including fixed register-access penalties and additional video waits |
| other | Clock movement without one of the above owners; reported explicitly |

Device ownership temporarily overrides its caller and restores that caller on
return. Original IRQ entry is separate from translated time; native IRQ entry is translated CPU work. Accounting
occurs before scheduler callbacks, after determining the divided clock advance,
so nested steals cannot be included in both an instruction and device total.
Both the native scheduler and original scheduler implement this observation.
Neither clock timing nor event callbacks change.

`Execution cycles` compares the sum of these categories with elapsed absolute
time and prints signed `unaccounted` cycles. A nonzero value is an accounting
failure, not another category to silently call interpreted work. `other` must
also be inspected even when the ledger balances.

`Native execution outside wait/stalls` is:

```
translated / (elapsed - idle - device)
```

Retained opcodes, fallback opcodes, IRQ entry and unidentified movement remain
in that denominator. Device time is excluded regardless of which implementation
requested it. This measures native CPU execution ownership; it does not measure
host wall-clock speed or completeness of unvisited ROM paths.

Native IRQ entry is counted separately as `Native interrupt entries`; the original path remains `Core interrupt entries`. Both reset with the measurement window. [Native IRQ entry](NATIVE_INTERRUPT_ENTRY.md) raised corpus coverage to 99.6894%; [native block transfers](NATIVE_BLOCK_MOVES.md) now raise it to **99.8089%**.

`Native block moves` reports starts by mode, resumes and exclusive CPU cycles.
Those cycles are already included in `translated`; do not add them to the ledger
again. Native transfer starts do not count as original opcode dispatches or
transcript installations.

`Native bit branches` likewise reports actual shared-engine starts and exclusive
CPU cycles, already included in `translated`. It raises weighted corpus coverage
to **99.8727%**; see [native bit branches](NATIVE_BIT_BRANCHES.md).

`Native control flow` reports actual shared JSR/BSR/RTS/JMP starts and exclusive
CPU cycles, already included in `translated`. Frame-wait returns retain idle
ownership so the denominator remains comparable. It raises weighted corpus
coverage to **99.9493%**; see [native control flow](NATIVE_CONTROL_FLOW.md).

`Native load/status` reports shared absolute LDA, PLP and SEI starts and
exclusive CPU cycles, already included in `translated`. It raises weighted
corpus coverage to **99.9902%**; see [native load/status](NATIVE_LOAD_STATUS.md).

`Native branches` reports actual starts and exclusive CPU cycles for all eight
conditional branch opcodes. These cycles are already included in `translated`.
Weighted corpus coverage reaches **99.9921%**; see
[native conditional branches](NATIVE_BRANCHES.md).

`Native stores` reports actual starts and exclusive CPU cycles for all 18
STA, STX, STY and STZ opcodes. These cycles are already included in `translated`.
Weighted corpus coverage reaches **99.9952%**; see [native stores](NATIVE_STORES.md).

`Native loads` reports actual starts and exclusive CPU cycles for the remaining
18 LDA, LDX and LDY opcodes. Absolute LDA remains in `Native load/status`.
Both contribute to `translated`. Weighted corpus coverage reaches
**99.9972%**; see [native loads](NATIVE_LOADS.md).

`Native logical operations` reports actual starts and exclusive CPU cycles for
all 27 AND, ORA and EOR opcodes, including transfer mode. These cycles are
already included in `translated`. Weighted corpus coverage reaches
**99.9983%**; see [native logical operations](NATIVE_LOGICAL.md).

`Native rmw operations` reports actual starts and exclusive CPU cycles for
all 34 increment/decrement, shift and rotate opcodes, already included in
`translated`. Weighted corpus coverage reaches **99.9991%** after this batch;
see [native read/modify/write operations](NATIVE_RMW.md).

`Native compare operations` reports actual starts and exclusive CPU cycles for
all 15 CMP, CPX and CPY opcodes, already included in `translated`. Device stalls
remain separate; the original opcode engine remains available as the oracle.
See [native comparisons](NATIVE_COMPARISONS.md).

`Native stack/branch operations` reports actual starts and exclusive CPU cycles
for PHA, PHP, PHX, PHY, PLA, PLX, PLY and BRA, already included in `translated`.
PLP remains in the earlier status engine. Device stalls remain separate and the
original opcode engine remains the oracle. Weighted corpus coverage reaches
**99.9997%**, with no retained opcode cycles on these routes.
See [native stack operations and BRA](NATIVE_STACK_BRANCH.md).

`Native register/mapping operations` reports actual shared mapping, register,
swap, clear, flag and NOP starts and exclusive CPU cycles already included in
`translated`. It raises weighted corpus coverage to **99.9999%**, with zero
retained execution and 25,893 fallback CPU cycles remaining; see
[native register and mapping operations](NATIVE_REGISTERS.md).

`Native arithmetic/speed operations` reports actual shared ADC/SBC and CSL/CSH
starts and exclusive CPU cycles already included in `translated`. It raises
weighted corpus coverage to **99.9999903%**, with zero retained execution and
2,010 fallback CPU cycles remaining; see [native arithmetic](NATIVE_ARITHMETIC.md).
The CLI percentage rounds to four decimals and can display 100% while the
ownership ledger still contains fallback cycles. Use those exact cycle counts
to assess completion.

## Counts and profiles

`Original opcode dispatches` counts entry into the original opcode switch,
separately for retained, fallback and idle instructions. A resumed block move
continues its original dispatch rather than adding another. Its clock advances
retain the current block's owner across normal run slices. After state restore,
an already-running core block has no capability offer in the new measurement
window and is conservatively classified as fallback. A native transfer resume
is translated work regardless of which engine started the saved transfer.

`Core replacement outcomes` separates real transcript installations, recognized
retained-capability offers and handled offers that the core rejects. A helper
returning no offer is not a rejected core transaction; neither is an address
with no replacement hook. A recognized capability offer may subsequently run
through a shared native engine; its actual clock owner determines coverage.
The legacy host horizon count remains only that
particular host-side decline check, not a census of all decline reasons.

Accepted-body credits no longer determine interpreter-profile membership. The
core notifies the profiler only after it validates a real translated
transaction or selects a shared native instruction engine. Offers that still
execute in the original core leave the interval in residue.
Intervals still include elapsed time until the next observed entry and may
contain IRQs, device waits or transcript continuations. Use the exclusive ledger
for cycle shares and these profiles for address attribution; do not sum the two.

`Instruction-boundary decisions` replaces the misleading label `Instructions
interpreted`. It continues to count the boundary model's decisions, including
checkpoint boundaries. The original-opcode dispatch counts are distinct.

Legacy registry estimates remain available, explicitly identified as accepted
bodies. `03:66A3` credits, entries and accepts are now included once in the
registry totals rather than also in the core-capability totals. Its legacy
Three-blobs diagnostic count remains available. An accepted transcript can be
interrupted or abandoned: only its executed portions enter the primary ledger.

## Accounting baseline before native IRQ entry

The existing 3,600-frame alley route gives:

| Owner | Master cycles |
| --- | ---: |
| translated | 666,724,638 |
| retained | 1,872,891 |
| fallback | 369,975 |
| interrupt | 6,399,384 |
| idle | 609,816,960 |
| device | 7,194,303 |
| other | 0 |
| **Elapsed / accounted** | **1,292,378,151** |

Unaccounted cycles are **zero**. The native share outside wait/stalls is
**98.7204%**. There are 75,620 retained, 21,490 fallback and 6,690,905 idle opcode
dispatches; 266,641 IRQ entries cost exactly 6,399,384 master cycles.

The newly visible non-wait interval residue is 2,365,686 cycles, versus 171,279
in the previous report. This is an accounting correction, not a regression:
the frame CSV hash remains
`964C9E6D67EAF95D75188FAA22F52F9ABA1016CE19F74F22B2F5F11DFD604D58`,
and the scheduler and interrupt-boundary observations are unchanged. For example,
the retained TIA at `00:EB46` now appears in residue while translated
`09:62E0` does not. Both scheduler implementations produce the same owner totals
and opcode counts on this route.

Across the full 31-route corpus (30 routes of 3,600 frames and the base route
of 1,800), all frame hashes and abandonment counts match the pre-accounting
build. Every route has zero unaccounted and zero unidentified cycles, including
the three routes with one pre-existing abandonment each.

| Corpus owner | Master cycles |
| --- | ---: |
| translated | 20,392,943,919 |
| retained | 54,845,964 |
| fallback | 9,291,789 |
| interrupt | 190,693,128 |
| idle | 18,552,136,050 |
| device | 217,620,741 |
| other | 0 |
| **Elapsed / accounted** | **39,417,531,591** |

The weighted native share is **98.7658%** outside wait/stalls. The remaining
CPU time is 254,830,881 cycles; IRQ entry accounts for **74.8312%** of it.
This supersedes the initial audit's nominal IRQ estimates with exclusive,
measured clock ownership.

## Implementation and verification

Patch `0037-huc6280-execution-accounting.patch` adds observation at the core's
clock advances, opcode dispatch, IRQ entry, transcript application and device
steals. `dc_beetle_execution_hook` owns the counters and translated-disposition
notification. `PceRuntime::execution_accounting()` exposes a copied ledger;
the application prints it ahead of the legacy registry estimates.

The scheduler differential tests verify divided advances, including overclock
remainders, and exclusive/restored ownership through nested device scopes.
`dc_execution_accounting_state` checks empty windows after load/reset/restore,
zero translation for unhooked execution, exact reconciliation and replayed RAM,
clock ownership and opcode counts under both schedulers.
`dc_rom_execution_accounting` checks the fixed alley frame oracle, all owner
categories, the denominator, independently counted IRQs, restored retained
residue and equality between native and original scheduler accounting.

The existing host-registry, face/hand captures and ranked-VDC five-trace oracle
remain compatibility gates. Validation artifacts and the 31-route measurement
are under `out/execution-accounting`. [Native IRQ entry](NATIVE_INTERRUPT_ENTRY.md) has since removed the measured IRQ cost; shared retained/fallback instruction families are next.

Completed validation: Release **7/7** tests passed (80.06 seconds), including
the three new/extended accounting tests, host registry, both captured
regressions and the ranked-VDC five-trace oracle. Debug **3/3** accounting tests
passed (195.86 seconds). Registry generation remains current at 932 admissions,
all 399 seam ranges validate, and the downstream patch reverse-application
check passes against the tested core sources.
