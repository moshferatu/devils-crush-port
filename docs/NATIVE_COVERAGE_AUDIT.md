# Native coverage audit — September 11, 2026

This is the pre-change audit. Its accounting implementation is now documented in [Execution accounting](EXECUTION_ACCOUNTING.md), including corrected ownership measurements and validation.

The twelve-address residue ranking is no longer an adequate guide to completing
the port. The current percentage measures credited replacement bodies, not
independence from the interpreter. Some credited capabilities deliberately run
the original opcode, and most of the displayed coverage gap is not represented
by the residue ranking at all.

This audit changes the proposed priority to execution accounting, interrupt
entry, and shared opcode mechanisms. It does not change runtime behavior or
claim that the remaining work has been ported.

## Measured baseline

Source revision: `8e71a34eafcaed0f58e7aa97d9910c17b1e08af4` (932 admissions).
Inputs are the 31 post-change route logs and full profiles under
`out/ranked-vdc-cost`: 30 routes cover 3,600 frames each and the base gameplay
route covers 1,800 frames. These deterministic routes
are a regression corpus, not a representative weighting of cabinet play or
proof of all reachable states. The supported USA ROM hash is checked by the
audit script.

| Counter, summed across 31 routes | Master cycles |
| --- | ---: |
| Reported working time, excluding the measured wait | 20,873,529,783 |
| Reported native credits | 20,651,620,359 |
| Difference between those counters | 221,909,424 |
| Reported non-wait interpreter residue | 2,624,532 |
| Difference after subtracting residue | 219,284,892 |
| Core-capability credits, already included above | 73,716,372 |

The weighted reported share is **98.936886%**. Residue is **0.0125735%** of
working time, or **1.1827%** of the displayed gap. These are comparisons of
existing counters, not a disjoint ownership ledger. In particular, subtracting
residue does not identify the remainder as interpreter work.

The previous batch removed 85,116 residue cycles from these same routes. On
this denominator that is only **0.000408 percentage points**: substantially
less than the displayed percentage's 0.01-point resolution. Even removing all
currently reported residue would have a nominal effect of only 0.01257 points
if credited one-for-one with a fixed denominator. Neither calculation is a
prediction of actual ownership coverage or host execution speed.

## What the counters actually count

1. `main.cpp`, `NativeCycleSpan`, measures between first and last observed
   registry/capability entries, not a common start/end boundary for all counters.
   The native numerator sums requested `body_master_cycles` immediately when
   the host callback returns a result. It does not measure completed work in
   the core. Abandoned or core-rejected bodies can therefore retain credits.
2. `pce_runtime_beetle.cpp`, `InstructionProfile::sample`, charges the elapsed
   time between two observed instruction entries to the earlier address. The
   interval can include IRQ entry, device stalls, idle skipping, and resumed
   transcript execution. It is not an opcode-retirement cycle counter.
3. `replacement_hook` calls `mark_previous_native` before the core validates
   and executes the replacement. This excludes the entire interval from
   residue on host acceptance, even when the core subsequently runs the opcode.
4. The core's capability path in `huc6280.cpp` explicitly sets `valid = false`
   for recognized capability flags after checking them. It then falls through
   to the original opcode path. This includes retained RTS, PLP, JSR, JMP,
   VDC reads, branches and block transfers. See tracked patch
   `third_party/beetle-pce-fork/patches/0024-huc6280-native-tia-transfer.patch`
   and its subsequent extensions, and the applied core around lines 1320–1520.
   These are compatibility mechanisms, not proof that their instructions have
   been replaced. Core-capability credits are a mixed category: for example,
   the now-translated `03:66A3` is also included there. Do not simply subtract
   all 73,716,372 credits as a supposedly exact correction.
5. There is a definite duplicate: the registry callback adds every accepted
   `03:66A3` body to both its row and `core_capability_master_cycles`, and the
   summary adds both totals. The fresh alley census confirms **55,137 cycles**
   credited twice. The call/acceptance totals also duplicate this category.
6. The idle model counts skipped iterations plus the next nominal loop turn;
   residue excludes the entire address range `00:E321–E32B`. These definitions
   differ. On alley, measured wait is 609,549,048 cycles while intervals
   attributed to those addresses total 593,188,092 cycles. That 16,360,956
   difference cannot be treated as missing gameplay instructions.
7. The label `Instructions interpreted` prints the native boundary model's
   instruction decisions. The source comment itself explains that checkpoints
   also make those decisions. It is not the number of executed interpreter
   opcodes. Likewise, `declined_at_horizon` only increments at the host's final
   atomic-body horizon check; it does not classify every reason a helper can
   return no result or the core can reject one.

Consequently, neither 100% credited cycles nor zero residue would establish
interpreter independence. Nor does the audit justify claiming 99.9874% true
native coverage by taking the complement of the residue percentage.

## Large work absent from the address ranking

Fresh Release replays with existing boundary/device reporting reproduced the
saved alley and twist frame hashes and reported zero abandonments.

| Observation | Alley | Twist |
| --- | ---: | ---: |
| Timer interrupts selected | 259,443 | 258,629 |
| IRQ1 interrupts selected | 7,198 | 7,198 |
| Nominal IRQ entry cost at fast speed: count × 24 | 6,399,384 | 6,379,848 |
| Reported cycle gap | 7,515,900 | 7,415,607 |
| Reported non-wait residue, saved profiles | 171,279 | 161,874 |
| VCE stolen CPU cycles | 224,638 | 208,227 |

`HuC6280::RunSub` still performs IRQ entry: dummy reads, stack pushes, flag
updates, vector reads and the final sample. It takes eight CPU cycles per
entry (24 master cycles at fast speed). The nominal IRQ figures alone are
about **85–86% of the size of the reported gaps** on these two routes.
They are derived estimates, not newly instrumented elapsed-cycle ownership
totals; they must not be blindly subtracted alongside the interval residue.
Some IRQ time is already attributed to preceding intervals, including wait
and native-marked intervals.

The interrupt decision, timer/controller and clock scheduler already have
native models. Moving IRQ entry means porting its remaining processor actions,
not rewriting those models. The code and measurements make it a much larger
candidate than the latest twelve-site batch.

Device wait cycles also advance the machine during native execution without
necessarily appearing in declared body costs. VCE reports processor cycles,
not master cycles; conversion depends on speed. These are hardware wait costs,
not automatically unported game logic. `AdvanceReplacementTo` prices the fixed
port penalty into the declared write offset, while a device can add further
waits. Counting all port penalties again would overcount.

**The existing telemetry cannot close the remaining ledger exactly.** In
addition to device waits and IRQ attribution, start/end scopes differ,
capability credits overlap actual opcode execution, and bodies are credited
before completion. This audit identifies the dominant structural candidate
and accounting defects; it does not assign the unexplained balance to a
guessed owner.

## Remaining visible residue, grouped for implementation

The full profiles contain 3,387 distinct non-wait residue sites. Opcode grouping
reads the supported ROM using the live bank identity and ROM mirror layout.
It is a candidate grouping, not proof that every site can share an unrestricted
replacement: memory mapping, T/D flags, stack timing and device effects remain
part of each instruction's contract.

| Family | Sites | Residue cycles |
| --- | ---: | ---: |
| JSR | 396 | 381,861 |
| RTS | 134 | 282,738 |
| STA zero page | 323 | 179,808 |
| BNE | 95 | 159,798 |
| LDA immediate | 436 | 106,236 |
| LDA absolute,X | 129 | 105,831 |
| STA absolute | 127 | 98,649 |
| STA absolute,X | 193 | 95,790 |

JSR and RTS together account for **664,599 cycles**, about **25.3%** of visible
residue, across 530 sites. A shared, correctly sampled call/return mechanism
with ROM-derived site admission offers broader coverage than another list of
twelve independent wrappers. Its core-capability users must be included too.

The largest individual visible site is `08:5976 RTS`, 61,194 cycles. Other
known cross-cutting requirements remain: `01:CB68 BBS1`, 53,832;
`00:FA67 BBR0`, 42,408; `09:7F44 TIN`, 26,040; `01:C107 TII`, 25,017;
and `45:4B84 PLP`, 19,176. Block-transfer resumability and PLP's old-I sample
matter for complete coverage, but these visible costs alone do not make them
the first throughput priority. Read the full generated family/site outputs
instead of truncating the planning input to twelve entries.

## Recommended implementation order and acceptance criteria

1. **Repair measurement before reporting another coverage gain.** Add core-side
   observation of actual execution disposition: translated transcript work,
   retained capability opcode, ordinary fallback, interrupt entry, idle and
   device stall. Use one measurement window and reconcile to its elapsed
   cycles; device stalls and nested interrupts must be exclusive categories,
   not additions to an already inclusive instruction interval. Credit retired
   portions rather than entire accepted bodies, remove the `66A3` duplicate,
   distinguish decline reasons, and rename the instruction-decision counter.
   Publish any residual accounting error explicitly. Keep a separate count of
   original opcode executions, including capability fall-through.
2. **Port the shared IRQ-entry processor sequence.** Preserve every timed dummy
   read, stack write, vector read, speed-dependent advance, status transition,
   and interrupt sample. Validate with boundary/stack/device traces and the
   existing capture regressions. Acceptance means the old core IRQ-entry path
   is actually unused on the tested routes, not merely newly credited.
3. **Unify shared call/return and retained capability execution.** Prioritize
   actual core opcode counts from step 1. Calls/returns are the strongest
   visible family candidate. Extend exact sampling support and admit verified
   ROM sites in groups; move capability users to the same real implementation.
   Avoid changing only their labels or suppressing their residue records.
4. **Complete connected routines and the remaining semantic exceptions.** Use
   the resulting full family/owner inventory for loads/stores/branches, PLP,
   multi-sample bit branches and resumable block transfers. Build connected
   blocks once their primitives are proven, preserving instruction boundaries.
   Keep the face/hand captures, activation routes and whole-machine trace
   parity as regression gates.

For route completeness, require zero actual original-opcode dispatches and
zero retained core IRQ-entry executions outside explicitly documented reset
or wait policy. For complete reachable gameplay coverage, also require an
inventory of reachable code and activation evidence for unvisited paths.
Registry admissions alone establish neither. Host wall-clock performance is a
third measure and requires CPU profiling; emulated master cycles are not a
measurement of C++ execution cost.

## Reproduction and validation

Run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Audit-NativeCoverage.ps1
```

The script verifies each route's residue sum against its log and emits
`out/coverage-gap-audit/{routes,sites,opcodes}.csv` and `totals.json`.
The checked-in script does not alter the runtime or input captures.

Fresh diagnostics used the Release executable with `--replace-registry`,
`--report-boundary-coverage` and the supported ROM. Alley additionally used
`--instruction-profile` in one replay and `--native-cycle-census` plus
`--report-vce-coverage` in another. Twist used `--report-vce-coverage`,
`--report-vdc-coverage` and `--report-scheduler-coverage`. Artifacts are in
`out/coverage-gap-audit`. The three replays completed successfully:

* Alley (both diagnostic runs):
  `964C9E6D67EAF95D75188FAA22F52F9ABA1016CE19F74F22B2F5F11DFD604D58`.
* Twist:
  `CA4A0762F74BC1B44CF5487A5C658CB0DB5FAE09CEBBE61422632C88E6D7C8A3`.

All match the saved post-port frame CSVs. No gameplay implementation changed,
so no new build or full regression-suite rerun was needed for this audit.
