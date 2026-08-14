# Native Port Iteration Workflow

This workflow keeps routine replacement faithful while making the normal
iteration loop fast enough to port related code as a unit. It applies to
Milestone 4 native gameplay work; it does not weaken the release or milestone
acceptance criteria in [PLAN.md](../PLAN.md).

## Validation tiers

### Per slice

Run these after every bounded native change:

1. Incrementally build the affected Release targets.
2. Run the ROM-free core tests, in Debug as well as Release. They cost
   seconds and they are where a Debug build earns its keep; see below.
3. Run one Release ROM regression for the owning routine family. Extend that
   family regression with the new replacement count, normalized replay, and
   relevant artifact assertions instead of creating another full-route test.
4. Regenerate the admitted-routine registry when the slice added or changed an
   `@all` admission, and commit the regenerated table with the slice.
5. Run `git diff --check` and inspect resulting artifact hashes before
   recording them.

Release is the normal behavioral gate because deterministic state-census
routes execute substantially faster there. The tested native adapter must
still preserve the original instruction checkpoints, registers, timing, RAM
writes, and interrupt hand-off.

### Family checkpoint

Run the full Release suite after a related batch, before declaring that family
complete, and before a milestone or hand-off checkpoint. A related batch
normally contains two to five sibling routines that share a route, actor
family, or state machine. In parallel this is under three minutes, so there is
no reason to narrow it.

Run the **ROM-free** Debug tests here too, and per slice. They are the
standing Debug obligation and they cost seconds.

### Broad acceptance

Run the full Release suite at milestone, release, CI, or deliberate
cross-family integration checkpoints, and whenever changing shared runtime,
trace, checkpoint, save-state, or ROM-load behavior.

## Debug, and why the ROM sweep is the exception

**The ROM-free Debug tests are not optional and never were the problem.** They
carry the whole of what a Debug build catches that a Release build cannot --
`assert`, `/RTC1`'s uninitialized-local and stack-corruption checks, and
`_ITERATOR_DEBUG_LEVEL=2`'s bounds and iterator validation. They also cover
exactly the code where such a fault would live: `dc_core`, the seven device
models, the enumerated interrupt boundary, the game-logic C. Run them on every
slice. They take seconds.

**The Debug ROM sweep is a different thing wearing the same name.** What it
proves is that census, call and write artifacts are byte-identical in Debug and
Release -- a determinism claim, and an indirect one: it says no result depends
on optimization level or on memory that was never initialized. That is worth
something, but it is worth it *per family*, and a sweep re-proves it for every
family a change could not reach. It is also the single most expensive thing in
this project: 219 tests, roughly three and a half hours serial and twenty-five
minutes parallel, against 147 seconds for the whole Release suite.

So do not run it as a matter of course. Run **one owning-family Debug ROM
route** -- not the sweep -- when one of these is true:

- A new artifact hash is being recorded as a golden for the first time. This
  is the strongest case: a value pinned under one configuration that turns out
  to be configuration-dependent is a defect discovered much later and much
  more expensively.
- The change touches shared transaction, checkpoint, interrupt-delivery, or
  save-state machinery, where Debug and Release shuttle state differently.
- A Release artifact moved for a reason not yet explained, and the question is
  whether the cause is undefined behavior.

Run the **full** Debug sweep only at a release checkpoint, or to chase a
suspected configuration-dependent artifact across families.

### Which preset is which, because the names do not say

This is a trap worth naming, because the expensive tier is the one with the
obvious name. `windows-debug` is **not** the ROM-free Debug set — it uses the
`windows-vs2022` configure preset, which has `DC_ROM_PATH` set, so it selects
the whole 219-test Debug ROM sweep.

| Want | Command | Cost |
| --- | --- | ---: |
| ROM-free Debug (run every slice) | `ctest --test-dir out/build/windows-core-only --preset windows-core-debug` | seconds |
| Release suite | `ctest --test-dir out/build/windows-vs2022 --preset windows-release` | ~165 s |
| Full Debug ROM sweep (release checkpoints only) | `ctest --test-dir out/build/windows-vs2022 --preset windows-debug` | ~25 min |

**There is no preset for one owning-family Debug ROM route**, which is the tier
the list above actually asks for. Run that family's recorder arguments against
`out/build/windows-vs2022/Debug/devils-crush.exe` directly and compare the
hashes to the Release goldens by hand. Note that many family verify scripts are
Release-only on purpose — `verify_palette_shadow_semantic.cmake` hard-errors on
any other configuration — so a direct invocation is the only way to get one, and
reaching for a preset instead gets the sweep.

One caveat that applies whichever tier is running: a Debug ROM route is a
genuinely independent execution, so when it does run it must reproduce the
Release artifact exactly. A route that reproduces the census but not the
call/write artifacts has found something, and is not to be waved through.

## Run the suites in parallel

The ROM regressions are independent processes writing distinct outputs, so
they parallelise almost perfectly, and the presets now ask for 24 jobs. This
is not a tuning detail — it is the difference between a validation tier being
usable per slice and being something to avoid:

| Suite | Serial | `-j 24` |
| --- | ---: | ---: |
| Release, 404 tests | 3,213 s | **147 s** |
| Debug, 219 tests | ~3.5 h | ~25 min |

The critical path when parallel is the longest single test, currently
`dc_rom_bonus_scored_results` at 33 seconds, so more jobs buys progressively
less. Raise `execution.jobs` in `CMakePresets.json` if a machine has the
cores; each test is single-threaded and needs a few hundred megabytes.

Nothing in the suite *appears* to require serialisation: no test declares
`RUN_SERIAL` or a resource lock, every `-DOUTPUT` path is distinct along with
the trace paths derived from it, and the only fixed path any test writes
(`portable-data/high-score-check.hiscore`) belongs to a single test. The
recording routes never touch the rest of `portable-data` — backup RAM, the
cabinet configuration and the high-score autosave are all reached from the
interactive host only. Keep it that way: a new regression writing a shared
path under a fixed name would reintroduce the need to run serially.

**The caveat this section used to carry has been explained and fixed.** For
eight runs after the switch to `-j 24`, roughly one test per full Release run
failed, never the same one and never reproducible alone. It was not
nondeterminism in anything under test: every captured message said
`Could not commit <artifact>; Windows error 32`, a sharing violation raised
*after* the route had run and every replacement had matched its entry count.
The recorders write `<path>.tmp` and replace `<path>` at the end, and a scanner
opens the temporary in the moment after it is closed, so the replace was
refused by another process's housekeeping. `commit_replacement` in
`devils_crush::core` now retries that replace while the refusal is one another
process can stop causing, and prints a line naming the artifact and the wait
whenever it absorbs one. See `test_atomic_commit` for the reproduction, which
takes the same handle a scanner takes.

An isolated failure under `-j` is still worth re-running alone before believing
it. **Read the message first**: a commit failure names a file and is
infrastructure, while a hash mismatch names an artifact and is not.

Do not run the two configurations at once against the same build directory.
Release and Debug tests share `CMAKE_CURRENT_BINARY_DIR` for their artifacts,
so concurrent runs overwrite each other's outputs and CTest's own log.

## Regression design

- Prefer one family CTest invocation that observes all sibling routines and
  enables their replacements together. It should assert every newly admitted
  replacement count and at least one normalized replay per newly implemented
  branch.
- Add a separate focused ROM regression only when it exercises a unique route,
  branch, write scope, or artifact that the family regression cannot express.
  Mark its reason in the test and in `PROGRESS.md`.
- Keep pure-C/game-logic tests exhaustive for classifier states, malformed
  work RAM, and invalid slots. They are cheap and run on every slice.
- Do not bless a changed hash by itself. Explain the changed behavior, inspect
  call/write output, and preserve an original-assembly comparison first.
- Use the same family trace to profile several candidate sibling routines
  before choosing the next batch. A natural route is stronger evidence than a
  synthetic state patch.

## Fallback hotspot profiling

Use `tools/Analyze-NativeFallbackHotspots.ps1` to rank a bounded observer set
from one or more routine-call traces. It measures calls, master cycles, and
ordered writes, then assigns a transparent heuristic weight of total master
cycles plus 12 per write. The native dispatch registry automatically marks
registered addresses; all other observed addresses are assembly candidates.

The runtime deliberately records routine-call scopes only for explicit
`--observe-hook` addresses. This is therefore a measured candidate/family
profile, not a misleading whole-ROM call graph. Before selecting a batch,
observe the current native parent plus its likely continuation or sibling
addresses on a natural route, then rank them:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Analyze-NativeFallbackHotspots.ps1 `
  -CallPath out\build\windows-vs2022\candidate.calls.csv `
  -AssemblyOnly
```

Use `-AssemblyRoutine 02:4AEB` to force an address to be shown as an assembly
candidate when a deliberately broad experimental registration exists. Passing
multiple call traces combines their route weights; compare like-for-like
recording plans, not Debug and Release duplicates. Treat the result as a
prioritization aid: confirm each selected branch with its original transcript
before writing a replacement.

## Admitted-routine registry

A native replacement is deployable to live play only when a regression has
proven it for every naturally observed call at its address. That evidence lives
in the `--replace-hook <bank>:<pc>@all` invocations and the expected
`Native replacement <bank>:<pc>@all entries: <native> of <observed>` lines in
`tests/verify_*.cmake`. An admission proven only for a single ordinal is not
deployable: an ordinal names one call on one deterministic recorded route.

`tools/Generate-NativeRegistry.ps1` sweeps that evidence, cross-checks every
address against the dispatch chain in `src/app/native_replacements.cpp` and the
self-guarding `kRoutine` constants in the family sources, and writes the X-macro
table behind `admitted_native_routines()`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Generate-NativeRegistry.ps1 `
  -ReportPath .\docs\NATIVE_REGISTRY.md
```

The `dc_native_registry_manifest` test runs the same script with `-Check` and
fails when the checked-in table no longer matches the regressions, so a new
admission cannot silently skip the registry. The generated inventory and the
current coverage totals are in [NATIVE_REGISTRY.md](NATIVE_REGISTRY.md).

A partial route — fewer native calls than observed calls — is expected and
safe. Each handler re-checks its own entry state and returns no replacement for
a state it was not proven against, so those calls run the original assembly.

### Withheld routines

Admission is necessary but not sufficient. A routine belongs in
`kWithheldRoutines` in `src/app/main.cpp` when it passes an `@all` regression
but deploying it alone moves the state census off the assembly-only baseline,
*and* its own regression expects the moved hash rather than the baseline. Its
evidence then locks the divergence in instead of catching it, so the list holds
it back from every composed deployment — live play and cycle measurement alike.
The deployment summary reports the list on every run.

**The list is currently empty**, and the mechanism stays for the next routine
that needs it. Both former entries, `02:57E9` and `03:6054`, turned out to be
replacement faults rather than properties of the routines, and both were the
same shape: a boundary charged the wrong number of master cycles, with the
error taken back off a later instruction so that the body total came out right.

- `03:6054` charged `ADC $26` 9 instead of 12 and gave the three back on the
  taken branch at `606E`, which put both of its writes three cycles early.
- `02:57E9` charged `CMP $3980,X` 18 instead of 15 the same way, and separately
  published the caller's N and Z at the boundary after `LDA $2D40` instead of
  the load's.

Two things are worth carrying from that. **A correct body total is why a state
census can tell you something is wrong and never where** — the routine ends at
the right cycle, so only what happens *inside* it is misplaced, and the census
only samples frame ends. And **the fault does not have to be a write**:
`02:57E9` writes nothing at all, so no write trace could have shown its flag
fault. It is visible only as the status an interrupt stacks when it lands on
that boundary, which is why the routine's own call trace — which records entry
and exit registers — is the artifact that caught it.

The method that localised both was the same: a write trace scoped to the
addresses the routine touches plus the stack, diffed against a pure-assembly
run of the same route, which put `03:6054`'s fault on two rows out of 4.38
million. See "Diff against an assembly run, not only against the baseline
hash".

Check any new `@all` admission against the baseline before trusting it. A
regression that blesses whatever hash the recorder produced proves the routine
is deterministic, not that it is faithful.

### Stack depth is part of the proven shape

A replacement that reproduces a `JSR` must reproduce its pushes, and those land
at `$2100 + S`. Derive both cells from `entry.registers.stack_pointer`; never
write the recorded addresses as literals. A literal silently encodes the depth
the transcript happened to be recorded at, and the handler's own entry checks
will not catch a second call one level shallower — slot, registers, and status
can all match there.

This is not a cosmetic slip. The replacement writes the return bytes into cells
the real push never touched, the scheduler at `01:C5C9` pops whatever was
already in the real ones, and the processor resumes inside data and writes
through work RAM until the frame ends. The visible result is a game that dies
seconds later, far from the routine at fault.

`02:4DC8` shipped this way. Its single admitted call is the frame-872 table
setup at `S=$F3`; re-initializing the table after a drained ball reaches it
again at `S=$F2`, which wiped work RAM and made the second ball unplayable.
Eighteen sibling one-shot initializers carried the same latent defect. All of
them now go through `proven_stack_return`, which declines any depth but the
recorded one and derives the cells from it.

`--cabinet-drain-probe` is the regression for this class: it replays the quick
state hands-off until the ball drains, works the plunger, and fails on a ball
coordinate or ball count that leaves its range. Run it with the registry
deployed, since that is the configuration a session runs.

## Deploying to the interactive host

The interactive host installs the whole admitted registry before its first
frame, so a session on the cabinet executes every routine the regressions have
proven rather than the emulated original:

```powershell
.\devils-crush.exe --cabinet --rom "path\to\Devil's Crush.pce"
```

No flag is required — the registry is the default. `--emulate-only` opts back
out to the untouched assembly, which is the A/B control when a live session
misbehaves and the question is whether a native routine caused it.

The host prints the installed count on startup and the same deployment,
call, and cycle summary the recordings print on exit;
`--native-cycle-census <census.csv>` writes the per-routine breakdown for the
session that was actually played. `--registry-slice` and `--registry-exclude`
work here too, so a live-only misbehaviour can be bisected the same way a
changed census is.

The registry is installed after every fallible startup step and cleared before
the deployment it captures goes out of scope; nothing between those two points
returns early. `dc_rom_cabinet_smoke` drives the host through that whole path
with the registry deployed.

The horizon valve is identical to the one the recordings use: an atomic body
that would span a user event, a video event, or a serviceable timer interrupt
is declined and the original assembly runs. Live input arrives at
unpredictable offsets, so the host declines more calls than a deterministic
recorded route does. That is the valve working, not a defect.

## Measuring native cycle share

Per-address call counts cannot answer how much of a session runs natively,
because the runtime records call scopes only for addresses an `--observe-hook`
named. `--replace-registry` deploys the whole admitted set in one recording and
`--native-cycle-census` reports what it actually covered:

```powershell
.\devils-crush.exe --record-state-census out\registry.csv `
  --replace-registry --native-cycle-census out\cycles.csv `
  --rom "path\to\Devil's Crush.pce"
```

The census totals native body master cycles against the elapsed master-cycle
span, bracketed by the first and last registry entry of the run. Divide the
span by the frame count to sanity-check it: the state-census route yields
358,321 cycles per frame against the PC Engine's ~358,312, so the bracket
covers effectively the whole recording.

That recording is faithful by default: `--replace-registry` alone reproduces
the assembly-only baseline `9036a9de...`, and now does so with **every admitted
routine deployed** rather than with two held back. A composed run that changes
the census therefore indicates a new problem rather than a known one.

`--registry-slice FIRST:COUNT` deploys a contiguous slice and
`--registry-exclude BANK:PC` holds one address back. Together they bisect a
composed run to the routine responsible for a changed census or a failed frame.

The measured baseline, taken on the 3,600-frame state-census route, is in
[NATIVE_REGISTRY.md](NATIVE_REGISTRY.md).

## Whole-ROM candidate profiling

`--instruction-profile` records every executed instruction address and its
master cycles with no keyed observer set, so it ranks code nobody has thought
to observe yet. It costs about 11% of recording wall time and does not perturb
the route: the state census is unchanged with profiling on.

```powershell
.\devils-crush.exe --record-state-census out\prof.csv `
  --instruction-profile out\profile.csv `
  --rom "path\to\Devil's Crush.pce"

.\tools\Analyze-InstructionProfile.ps1 -ProfilePath out\profile.csv `
  -ExcludeAddress "00:E323","00:E325"
```

The analyzer groups contiguous executed addresses into approximate routines,
ranks them by cycles, and marks the ones the registry already enters. Exclude
the frame-sync spin loop — `00:E323`/`00:E325` is 45% of all master cycles and
is the game waiting on the VBlank counter, not work to port.

`tools/Disassemble-Rom.py` reads a candidate back, addressed the same way the
profile prints it, so a ranked `bank:pc` can be turned into instructions
without a separate emulator debugger:

```powershell
python tools\Disassemble-Rom.py "path\to\Devil's Crush.pce" 00:E321-E32B
python tools\Disassemble-Rom.py "path\to\Devil's Crush.pce" 45:4142 --count 40
```

It knows the cartridge's bank mapping, which is not arithmetic on the bank
number — a 384 KB HuCard mirrors banks `$40`–`$7F` onto its last 128 KB, so
`45:4142` is ROM offset `$4A142` — and refuses a size whose mapping it does
not know rather than decoding the wrong bytes. It tracks no data, so a span
that starts mid-instruction prints nonsense until it resynchronises.

### Cycle share is not the whole selection criterion

A candidate also has to fit its interrupt horizon, and the profile does not
show that. `08:40DF`, the sprite emitter, was the largest portable cluster
left at 7.77% of all master cycles, but its mean body is 1,328 cycles and it
runs inside the video window, where the user-event horizon is far shorter than
the ~5,068-cycle timer period. Priced atomically the valve admitted 535 of
73,818 calls, turning 7.77% into 0.056%.

Estimate acceptance before building an adapter: divide the candidate's mean
body by its call count and compare against the horizons its `RoutineEntry`
reports. A body that approaches a frame's event spacing needs a checkpointed
transcript, not an atomic one, and that is a materially larger job — but it is
the job that pays. Checkpointing `08:40DF` took it from 535 calls to all
73,818 and the registry from 13.75% to 21.35% of all master cycles, because a
checkpointed transcript crosses the horizons the valve exists to protect.

Measure the decline reasons before rewriting anything. Counting them at each
`return std::nullopt` in an adapter costs one build and one route, and it is
the difference between porting an unmodelled state and lifting a valve that
was never the limit. Both bank-01 sweeps looked like partial coverage in the
registry — `01:C510` at 136,509 of 321,839 arrivals and `01:CB48` at 89,762 of
260,683 — and both turned out to decline *entirely* on the horizon floor:
zero classifier failures, zero decimal or counter mismatches, zero overruns.
Neither needed new game logic. Each needed its existing classifier re-emitted
as a transcript, which took them to every arrival and the registry from 21.35%
to 23.60%.

### Two divisions decide whether a candidate can be admitted at all

Shared state is a harder constraint than the horizon, and unlike the horizon
it cannot be lifted by rewriting the adapter. A transcript is computed once
from a snapshot taken before it runs, so if it reads a byte another thread of
control writes, every one of those reads has to happen before the next
serviceable interrupt.

Check it with two numbers, both available from a profile already on disk:

1. **Does the candidate's subtree read shared state, on how many calls, and
   *where*?** Use execution counts, not the presence of the instruction. Bank
   45's `4142` has a conspicuous `LDA $F5` at `415A` that runs on 12 of 3,604
   calls and an inconspicuous `LDA $F5,X` at `4148` that runs on all of them.
   Reading the first and stopping gives the wrong answer.
2. **Is the body shorter than the interrupt period?** The period is elapsed
   cycles divided by interrupt-handler entries — 1,292,370,213 / 254,990 =
   5,068 on this route. A body well under that is admitted at roughly
   `1 - body/period`: `4142` at 2,054 cycles predicts 59% and measures 49.7%.

**Both numbers are properties of a transcript, not of a routine, and this
section got that wrong for four slices.** It closed by ruling out `45:4015` on
its 12,667-cycle body against the 5,068-cycle period — "two queries, no code".
The queries were right and the body was the wrong one. The tick's body is a
loop, so it chunks at the loop head like any other, and the unit that has to
fit the period is one iteration: 1,137 composed master cycles, predicting
about 78% admission rather than zero. Ask "is the *chunk* shorter than the
period", and if the answer is no, ask where the loop head is before concluding
anything.

**The distinction that makes this work is between a cap and a deadline.** The
channel limits — 2,048 checkpoints, 512 writes — are caps, and a cap is met by
writing less. The shared-state rule is a deadline: the reads have to happen
before the next serviceable interrupt, and a deadline is met by doing less
*before it*. Chunking meets both, but only the second explains why it rescues a
routine the caps never bothered. `45:4015` fits 1,058 checkpoints and 245 writes
into an average call and was never near a cap; it was refused on every call by
the deadline. Cutting the walk at the loop head so each chunk lands inside the
horizon it was handed took it from 0 of 3,599 to 24,667 of 32,391 iterations
running inside a transcript.

Two costs come with it, both visible in that ratio. A chunk boundary is by
construction the moment the horizon is nearly spent, so the entry immediately
after one often cannot fit an iteration either and hands it back — which is why
`4015` and `4044` are admitted on about half their arrivals rather than the 78%
the division predicts. And the walk has to decide to stop *before* running the
iteration, so the margin is the iteration's cost rounded up, not its average.

The "and *where*" in the first check is the other half of the same lesson, and
the tick is a warning rather than a success here. The recorded rejection said
the voice loop reaches `4148`, `LDA $F5,X`; it does not reach it at all, since
the bank's only `JSR $4142` is at `4138`, *past* the `JMP $4044` at `40FF` that
closes the loop. But the conclusion was right anyway, because the loop reads
driver state somewhere else entirely — the amplitude commit at `49D2`, reached
by both `49B6` and `49F9`, on every iteration. Nobody had looked there. A count
tells you how often a shared read happens; only the control flow tells you which
side of a prospective chunk boundary it is on, and that is what decides whether
the constraint lands on every chunk or on one. So this check is cheap but it is
not "before disassembling anything" — it is before writing a *transcript*, and
it needs the loop bounds. **Find which read imposes the deadline before
believing any story about it**, including a story that reaches the right answer.

Where a shared read really does sit behind a `JSR` to an already-native callee,
it costs nothing: stop in front of the call and the callee is entered on its own
with the guard it already carries. That is what the tick does with `4142` at
`4138`, and it is why the tail carries no deadline even though `4142` does.

### A long transcript used to shadow the interrupt handler

**This tax no longer exists**, and the section is kept because the reasoning
recurs and because it is the clearest record of what the no-nesting rule cost.

The core would not consult the replacement hook while a replacement was active,
so every interrupt that landed inside a transcript ran its handler emulated.
The cost scaled with body length and was invisible in the candidate's own
numbers. The palette slice added 9,937,182 native cycles and gave back about
694,000 of them, because `00:E9B6` runs 9,745 cycles per call — roughly two
timer periods — and cost the sound driver 1,181 native entries. The collision
sweep paid 683,271 and the wrapper group 756,342, which was 15.6% of its own
gross.

Every slice paid it, and the toll grew as the transcripts grew: that
compounding, not the size of any one bill, is what eventually made the rule
worth removing rather than working around. Replacements nest now (fork patch
`0007`), a suspended transcript is pushed to a frame stack, and a candidate's
length costs the handler nothing. What survives is the habit: read a slice's
effect out of the whole cycle census before and after, not out of the
candidate's own row.

An atomic body priced against the horizon also truncates the calls it does
admit, so the loss is larger than the decline count suggests. `01:C510` walked
a mean of 142 master cycles per admitted call against a mean horizon of 265;
as a transcript it walks to the loop's own exit and averages 192, and its
native total went from 19.3M cycles to 33.9M.

### Whether the routine runs masked decides the shadowing cost

This mattered while shadowing did, and the technique it teaches — ask the
observer instead of reasoning about the body — is worth keeping.

The section above prices a long transcript by how many timer periods it spans.
That is the upper bound, not the cost. `00:EB30` spans up to three periods and
cost the sound driver nothing: not one entry, not one cycle, measured by
diffing the whole 298-routine cycle census before and after. Every one of its
3,599 calls arrives with `I` already set, because a VCE upload has to land
inside the blanking interval, so there was never an interrupt to shadow.

One `--observe-hook <bank>:<pc> --routine-call-trace` run answers this before
any code is written: count the `input_status` column and check bit 2. If it is
set on every call, transcript length costs nothing on this axis and the only
caps that matter are the write and checkpoint limits.

### Total native time sets the shadowing bill's scale, but does not fix it

The section above prices a *long* transcript. That framing is wrong in the
other direction too. The bank-wrapper slice is nine transcripts of 36 to 162
master cycles each — a fortieth of `00:E9B6` — and it cost the sound driver
1,326 entries and 756,342 cycles, 15.6% of its own 4,849,479-cycle gross. There
are 52,911 of them.

A thousand small transcripts shadow the handler exactly as well as one large
one, because what the driver loses is proportional to how much wall time the
hook spends unavailable, and that is the sum of the bodies. Per-call length
only tells you whether a *single* call can straddle a period.

What that does not give you is the ratio to the digit, but nine slices in it
does give you a usable estimate — for work that runs while the rest of the
game does, which is the qualifier the last row below exists to add:

| Slice | Gross native | Shadowing bill | Share of gross |
| --- | ---: | ---: | ---: |
| Bank wrappers | 4,849,479 | 756,342 | 15.6% |
| Tilemap upload | 5,180,931 | 937,827 | 18.1% |
| Collision frame | 4,091,373 | 859,038 | 21.0% |
| Composing the scanner | 3,524,010 | 695,061 | 19.7% |
| Sorcerer tick | 3,156,294 | 560,934 | 17.8% |
| Bank-01 geometry leaves | 3,066,105 | 626,298 | 20.4% |
| Collision-table interpreter | 2,942,940 | 578,541 | 19.7% |
| Interpreter into the Sorcerer tick | 1,538,439 | 242,907 | 15.8% |
| Fade-to-black stepper (chunked) | 1,640,100 | 1,224 | **0.07%** |
| Music sequencer tick (chunked) | 14,664,984 | 1,647,495 | 11.2% |

The spread looks arbitrary until the per-event numbers are pulled out of it.
Those are almost identical every time: the driver costs **568 to 577 native
cycles per displaced entry**, and **one entry is displaced for every 2,775 to
3,300 master cycles** a transcript holds the core. So the bill is close to
`gross / 3,000 × 575`, which is about 19%, and the slice-to-slice variation is
small next to that. Both ends of the displacement band belong to the extremes
of transcript length: the 9,700-cycle palette interpolator sits at 3,300 and
the three 700-cycle geometry leaves at 2,775, so short transcripts cost a
little more per cycle transcribed than long ones. Not enough to change a
decision, but the direction is consistent.

The last row is worth a caveat, because the obvious reading of it is wrong. It
is the first slice on which the route's *total* hook entries fell — 2,367,401 to
2,332,709 — since composing a call deletes the two boundaries around it rather
than adding a new one. It is tempting to conclude that composing is therefore
systematically cheaper than replacing. It is not: the other composing slice in
the table, the scanner, billed 19.7% and came in slightly *above* what the rule
predicts. 15.8% is the second-lowest of eight and sits inside the existing
spread, next to a slice that composed nothing at all. Whatever drives the three
points of variation, it is not this.

**The last row is the exception that names the mechanism.** `00:EAA7` bills
1,224 cycles where the rule predicts 314,000 — it displaces eight entries of
`00:E136` on the whole route and nothing else. The bill is the cost of
*displacing other native routines*, and the stepper only runs during screen
fades, when the game is deliberately doing nothing else: there is nothing in
the shadow to displace. So the estimate applies to work that competes with
the sound driver and the actor loop, and idle-time work — fades, wipes,
attract-mode transitions — is close to free to port. Check what else is
running before budgeting a region, not just how long its transcripts are.

Two things keep it an estimate rather than a formula. The first eight points span
three percentage points either side of 19%, which is a hundred thousand cycles
on a typical slice. And the displacement rate is 1.7 times the naive figure:
the timer fires every 5,068 master cycles, so a transcript covering N cycles
should overlap N/5,068 of them, not N/3,000. Part of that is the shadow window
being wider than the transcript — a driver entry that lands inside one runs
emulated while the transcript stays suspended, so its own ~575 cycles are
shadowed too — but that accounts for a fifth of the excess, not all of it. The
rest is unexplained.

So: use `gross / 3,000 × 575` to decide whether a region is worth starting, and
**measure the bill with a `--registry-exclude` run against the same route**
before reporting what the slice delivered.

Masking still decides which of those cycles are free, and the wrapper slice
separates the two cleanly: its three wrappers that run inside the VBlank
handler cost nothing at all, and the whole bill comes from the six that run on
the main thread.

### A device *read* is a harder blocker than a device write

The write channel can be widened; there is nothing to widen for a read. A
replacement callback receives the entry's registers and a work-RAM snapshot,
and that is the whole of its input, so a routine whose result depends on what a
device register returns cannot be a transcript at any length.

Two examples sit next to each other in bank `00`. `00:E195`, the pad reader,
strobes `$1000` and reads it back five times a frame; `00:E058`, the VDC
interrupt handler, opens by reading the VDC status register at `$0000`.
Together they are 7.5M master cycles that a plan written from the write side
had counted as portable — the plan for that region said to add `$1000` to
`is_replaceable_hardware_port`, which would have let the strobes through and
changed nothing about the reads.

Screen for this at the same time as shared state, and from the same
disassembly: list every load in the candidate's subtree whose address is on
the hardware page. If there is one, note the *instruction* — not the routine —
and read on; the next two sections are about how narrow that block usually is.

### An instruction that changes `I` cannot be summarised by a checkpoint

A checkpoint carries one register set. The core applies it, samples the
interrupt with it, and then advances — so the mask a checkpoint publishes is the
mask its instruction *ended* with. The machine samples earlier than that: the
IRQ decision is taken at `LastCycle`, and for the two instructions that matter
`LastCycle` runs before the register effect.

Both halves of this have now been paid for.

- `CLI` in the sound driver: a checkpoint publishing the post-`CLI` status
  opened the window one instruction early, and a nested timer interrupt arrived
  nine master cycles ahead of the machine. Fix: publish the old mask and let the
  clear land on the next checkpoint.
- `PLP` in the VDC interrupt handler: the core's `PLP` calls `LastCycle()`
  before `P = POP()`, so a transcript covering it samples with the *restored*
  mask. Assembly services a timer interrupt between that `PLP` and the `RTI`
  after it; the transcript swallowed it. Fix: stop the walk in front of the
  `PLP` and let the core execute it. Twelve master cycles a call, against a
  fault the two-run diff would otherwise have to catch every time.

The general form: if an instruction's mask sample is taken *before* it changes
`I`, either delay the published change by one checkpoint or do not cover the
instruction. The clause about the sample is the whole rule and it was missing
from the first version of this note, which said simply "if an instruction
changes `I`". In that form it reads as a ban on `RTI`, and it kept the sound
driver's return emulated for two slices on a property `RTI` does not have:
the core restores `P` from the stack *before* `LastCycle`, so its status byte
and its `IFlagSample` agree and one checkpoint says both. Read the core for
where the sample falls; do not infer it from what the opcode does to the flag.
Do not assume a body is masked throughout because an interrupt sequence sets
`I` on the way in — check. The VBlank handler is entered masked and does not
stay that way, because bank `45` ends three critical sections with an
unconditional `CLI` rather than restoring the caller's mask.

The stack page is what makes this class of fault visible. A swallowed interrupt
moves where its handler pushes, so an assembly-diff that includes `$2100`-`$21FF`
fails loudly; an end-state census may not.

### Rank what the deployed registry does not cover, not what is expensive

A `--instruction-profile` taken *with* `--replace-registry` attributes each
native transaction's whole body to its entry address, so it cannot be read
directly as remaining work. It can be read as remaining work once the rows
whose `registry_entry` is 1 are dropped: what is left is exactly the
instructions the deployed registry still emulates, per address, with the
counts to cluster them by contiguous range. That ranking is what surfaced
`01:C606` and `01:C52C`, which no whole-ROM profile highlights because neither
is a hot address on its own — they are the two halves of a dispatch that runs
171,734 times.

**The ranking is exact, and it is worth checking that it closes.** Checkpoint
replay does *not* sample the instruction hook, so a native run's profile is not
the assembly profile with the cycles moved around: it drops from 8,977
addresses and 108.5M executions to 6,397 and 55.8M. Every row that is not an
entry point therefore carries only what is still being emulated at that
address. Add the native total, the spin loop and the rest back together and the
elapsed total comes back within a few per cent, which is what makes the residue
trustworthy.

#### Classify the residue by opcode, not by routine

Ranking the residue by address, and then by the routine each address sits in, is
the instrument this file has recommended for most of the port, and it stopped
being the right one. Three consecutive slices — the sprite emitter's four calls,
the wrappers' four, the Tower insects' two — gained 1,550,178, 639,828 and
701,016 master cycles, and not one of them was a routine. They were instructions
of one kind, scattered across families that were otherwise finished.

So read the opcode at each emulated address. The ROM byte is a lookup: physical
bank to ROM page, plus `pc & 0x1FFF`. Bucketing the whole residue that way takes
a few lines and answers a different question — not *which routine is left* but
*what kind of instruction is left* — and on this route it found 2,942,532 master
cycles in 464 uncrossed `JSR`/`BSR` sites and 1,422,765 more in unabsorbed
`RTS`, against a largest remaining routine of 397,719. One technique, applied
where the profile already says it applies.

Two cautions when reading such a bucket. A call site inside a *composed*
transcript is not a candidate: the composition runs the callee in line, and the
row exists because a chunk ended or an interrupt cut the walk short. And a call
site in code that was never ported recovers the call's own 21 cycles and leaves
the body interpreted, which is real but small. Check which of the two a cluster
is before pricing it.

There is a third, and it cost this file a wrong recommendation the first time
the classification was run. **A `JSR` row can be a call that is already
crossed.** The bucket is built from the profile, and the profile records what
the *interpreter* executed; a walk that declines its entry runs the whole
routine interpreted, call sites and all, and every one of those instructions
gets a row. Bank `45`'s cluster — `45:408F`, `45:4138`, `45:40AA`, `45:409F`,
313,257 cycles and the densest four addresses in the whole bucket — is exactly
this. All four have been crossings in `native_replacements_music.cpp` for
slices; the rows are the music tick's declined entries, 6,516 of 7,724 at
`45:4044` alone, and the way to recover them is to make the walk admissible
more often, not to edit a call that is already an edit. Grep the family source
for the address before planning a slice around its row. The check costs one
command and the mistake costs the slice.

**Then stop using the profile for this question altogether.** Three unrelated
conditions produce a `JSR` row and only one of them is a crossable call, so the
bucket is a poor instrument for the very thing it was built to find. The
condition that matters is written down somewhere exact: a walk that ends in
front of a call is an address in that family's `is_stop_instruction`. Sweep
every `native_replacements_*.cpp` for its `is_stop_instruction` body, read
`kBank` from the same file, look up the ROM opcode at each address, and keep
the ones that are `$20` or `$44`. That is the complete list of uncrossed calls
in the port, with no false positives and nothing hiding behind a declined entry
— it took twenty lines of Python and settled in one run a question two slices
had answered wrong from the profile. Cross-check each survivor's target against
`native_registry_table.inc` before pricing it: a crossing onto an address no
one owns costs its twenty-one cycles and buys nothing, which is the one way
this edit can be worthless.

#### Take the cost from the assembly run and the weight from the ratio

Three corrections turn that ranking from suggestive into usable. All three were
found by trusting it too far first; one retired three entries from this file's
own next-target list, and one hid an eight-figure target for four slices.

**Price each address from the assembly-only profile.** The profiler charges
every sample the span to the next sample, and a native body produces no
samples, so a native body's cycles land on the last instruction sampled before
it. That is normally the entry point, but it is *any* instruction that precedes
a native call — the `RTI` at `00:FC8D` returned into a suspended transcript and
carried 99,636,684 cycles of other routines' work. That address is native now,
which moves the effect rather than removing it: the same cycles land on the
three entries the sound driver is deployed at instead. Pricing from assembly
does not remove it either, and that is the limit of this instrument — the same
`00:FC8D` costs 86.5 master cycles per execution in the *assembly-only* profile
against a real 21, so a return is over-priced by the method meant to correct
it. For a boundary instruction, trust `--native-cycle-census` over this.

How badly depends on what the return returns *into*, which is not something the
address predicts. Of the display handler's two, `00:E135` prices at 26.1 per
execution against the same real 21 and `00:E194` at 21.1 — a 24% error and a
0.5% one, against `00:FC8D`'s 312%. So the correction is not a constant that
could be applied and forgotten; it is a reason to census any row you are about
to plan a slice around.

Watch the entry-point flag rather than the row when checking one afterwards.
Absorbing a return into a transcript entered elsewhere deletes its profile row;
making the return an entry in its own right leaves the row untouched and flips
that flag. Both mean the instruction is no longer executed, so a row that has
not moved is not evidence the slice failed.

Excluding known offenders by hand does not scale and does not find the ones you
have not noticed. Instead run the same route with no replacements at all, where
nothing can be absorbed, and take each address's true cost from there:

```
cost = native_run_executions × (assembly_cycles ÷ assembly_executions)
```

Every leak goes at once.

**Drop the entry rows; they are phantoms, not cheap calls.** The instruction
hook runs *before* the replacement hook, so a replaced routine's entry address
is sampled once per call for an instruction the core then skips. Priced from
assembly that is 37,655,643 cycles route-wide — 36% of everything the profile
would otherwise call "still emulated", and enough on its own to have made the
sound driver's blocked arrivals look twice their real size for four slices.
Excluding `registry_entry = 1` rows is what makes the books close: native plus
spin loop plus what is left comes back to elapsed within 5.9%, and the
remaining overshoot is the method's own — each address is priced at one average
cost, so an address whose branch splits differently under the two runs is
charged the blend.

**Rank by the execution ratio, not by cycles.** A busy address is not evidence
of unported code. Compare the same address's executions in both runs:

| | assembly | with the registry | |
| --- | ---: | ---: | --- |
| `03:6967` | 54,149 | 119 | 99.8% ported; the rest is resumption |
| `00:E1A0` | 17,995 | 17,995 | genuinely unported |

A ratio near 1 on a non-entry address means the code is still emulated. A ratio
near 0 means the routine is ported and what you are looking at is one of the
hybrid's own overheads, which no amount of porting removes:

- **~~Call and return boundaries.~~** *This entry was wrong, and the palette
  family is where that was found out.* It read: a transcript stops in front of
  its `RTS` and is reached through a `JSR`, so every native call pays 21 master
  cycles at each end — 13.3M on the route — and it is what `08:40DE`-`41AB`
  with `08:4085`-`4094` really was, recorded as a 2.8M target but actually four
  `JSR $40DF`s and an `RTS` around a routine that was already native. The
  observation about `08:40DE` stands. The conclusion that the 21 cycles were
  structural does not. **A transcript can end past its own `RTS`**: see the
  section below — and both of those two returns are now absorbed, 1,634,598
  master cycles of the 13.3M, taken by two edits. Banks `03`, `02` and `01`
  have since taken 1,054,221, 1,314,528, 873,453 and 872,514, bank `00`
  694,653, and bank `45` 575,916 in a single condition, so a little over 7.2M
  of the 13.3M is gone. `01:C52B`'s 72,717 followed once the core could report
  an abandoned transcript, leaving 1,562,493 returns plus the `JSR` half — and
  most of what remains is unported parents rather than absorbable returns.
- **Resumption after an interrupt.** An interrupt landing inside a transcript
  rewinds to a checkpoint and interprets the rest of the body, so a fully
  admitted routine still shows interpreted executions, climbing monotonically
  along its fall-through path as later instructions become reachable from more
  checkpoints. That climb is the signature; straight-line code cannot otherwise
  have different counts on consecutive instructions. 8.6M on the route.

Resumption disappears with the emulator rather than before it. Rank without it
and what is left is the actual porting queue — with the return half of the call
boundary now *in* that queue rather than filed beside it.

#### A transcript can end past its own `RTS`

The entry above was struck out because it was tested and failed. Three things
were supposed to make ending past the return impossible, and none of them does:

- **The core does not know what routine a transcript was entered at.** Nothing
  ties `resume_logical_pc` to the entry address, at the install site or at a
  checkpoint. The host's only related check is a consistency one — the reported
  bank must be what `output_registers.mpr[resume >> 13]` maps that segment to
  (`pce_runtime_beetle.cpp:2925`) — so a walk resolves the bank through its own
  MPRs instead of assuming its own.
- **The return address is data the entry already carries.** The stack is
  ordinary work RAM at `$2100`-`$21FF`. `RTS` pops the low byte from
  `$2100 + S + 1` and the high from `$2100 + S + 2` and lands on that word plus
  one, `JSR` having pushed the address of its own last byte. Read it out of the
  staged work RAM and bump `S` by two in `output_registers`.
- **Interrupt delivery is unchanged.** `ApplyReplacementCheckpoint` samples at
  the checkpoint's declared sample offset, advances to its end, assigns the
  resume PC, and — this being the final checkpoint — pops the frame before the
  loop restarts at `DcSelectInterrupt`. So the boundary after the absorbed
  `RTS` is a real boundary, with the caller's address in `PC` and `S` already
  popped, which is exactly what the machine does.

`RTS` is 6 cycles plus the one `LastCycle` charges after it samples: 21 master
with the interrupt sampled 3 back, needing no special `sample_back`. And it
**emits no write**, which is what makes the change provable rather than merely
plausible. Nothing about the trace may move, so a family whose regression pins
work-RAM writes, port writes *and* the census has no room to hide an error.

That last sentence was half right, and the half it got wrong cost a slice.
`RTS` emits no write, so nothing the *machine* records moves. But one of the
instruments records the `RTS` itself, and it went blind — see below.

The palette family was converted first because it is well covered — seven
registered addresses, all admitted on every call. Result: 14,369 absorbed
returns, **+301,749 native master cycles**, and all three golden artifacts
byte-identical, in the family regression and in the whole-registry census. The
arithmetic closes exactly: 14,369 × 21 = 301,749, against a measured
671,640,741 → 671,942,490.

One measured side effect, benign and worth expecting elsewhere. Nested installs
rose 295,164 → 295,258. Extending each transcript by one checkpoint means the
instruction before the `RTS` no longer ends it, so there is one more boundary
at which an interrupt can arrive while the transcript is still active and its
handler nests. Delivery lands on the same boundary at the same cycle either
way, which is why nothing moved; depth stayed at 3 of a permitted 4 and no
replacement was refused.

#### An absorbed return has to be declared, or the call trace stops working

`--routine-call-trace` completes a call on an emulated `RTS` boundary whose
stack pointer matches the one the call was entered on. That is its only
completion signal. A transcript that ends past its return deletes the boundary
the observer is waiting for, and the observer does not notice: the call stays
open until some ancestor returns at the same stack pointer, and is then written
out with **that** routine's return address, its cycle, and every write made in
between.

It is not a subtle failure. Traced over the palette family before this was
fixed, `00:E93C` reported 8,966 calls instead of 8,969, the first of them
returning to `01:CAB6` after 53,486,487 master cycles instead of to `00:F0FE`
after 78, and 38,057,951 writes in place of 12,905. None of it showed up in the
family regression, because that regression pins the census, the work-RAM writes
and the bus writes, and all three were byte-identical. The instrument was wrong
and the machine was right.

So a transcript that absorbs a return **declares it**, in
`RoutineReplacement::absorbed_returns`: the return instruction, where its
boundary sits inside the transaction, and the registers at it. The host holds
the observed call open and closes it when the machine is demonstrably past the
return. There are two ways to be past it, and both are needed:

- **The transcript resumed at the caller**, so the next boundary stands on the
  resume address with the caller's stack pointer restored. The return's cycle
  is measured *back* from there, which is what makes preemption come out right:
  an interrupt taken at a checkpoint suspends a transcript rather than
  cancelling it, so the return happens hundreds of cycles later than the
  transaction predicted, and a prediction would report the wrong one.
- **The interrupt took the return.** When delivery lands on the last
  checkpoint, the core assigns the resume address and goes straight to the
  vector, so the machine never stands on it. That case is recognised from the
  frame the interrupt has just pushed — three bytes below the returned-to stack
  pointer, carrying the address the return produced — and the cycle is measured
  back from the recognising boundary here too, over the return *and* the vector
  sequence. The sequence is eight CPU cycles, and its third and fourth push the
  address the return produced: consecutive cycles, so the gap between those two
  writes is what a CPU cycle costs at whatever speed the machine is running,
  and the host measures it rather than assuming the high speed a transcript is
  usually written at.

  This case originally used the transcript's own reckoning instead, on the
  argument that it had run to its end. That is true and not sufficient: the
  reckoning is fixed when the transaction is installed, and a transcript long
  enough to be *suspended* part-way through has the whole handler inserted into
  it before it ends. `08:40DF` does both on 58 of its 73,818 calls — an
  interrupt inside the routine and a second one at the end — and reported those
  returns 729 cycles early, with the last sprite entry's writes missing.
  Anything a prediction can say about when a transcript ended, a suspension can
  falsify.

Two smaller things fall out. The return address comes from the transaction's
resume rather than a second reading of the stack, because by the time the call
is closed an interrupt may have pushed its own frame over exactly those two
bytes. And a call's writes are trimmed to those made at or before the return,
because the interrupt's three pushes arrive in the gap between the return and
its recognition, and the emulated observer never saw them.

The regression is `dc_rom_absorbed_return_trace`, and it is a comparison rather
than a recorded hash: it traces the same route with the routine emulated and
with it replaced, and requires the census, the call trace and the call-write
trace to be byte-identical across the two. A recorded expectation could drift
into agreeing with a mistake; the emulated run cannot. It runs over six
routines, registered as six tests so they run side by side. `00:E93C` is the
family the technique was established on and exercises both recognitions;
`08:40DF` is long enough to be suspended part-way through and interrupted at
its end, which is the case neither recognition got right on its own; `02:58C2`
runs a call in line and then ends past its own return, so its transcript writes
to the stack page three bytes below where the absorbed return reads; `03:6966`
is a routine whose return is absorbed at the entry rather than inside the
routine, and is the busiest thing on the route; `01:C5C9` reads its frame at
$2100+S+3 and +4 rather than the +1 and +2 every other absorption reads, having
pulled its own return address off first; and `00:ECE2` closes a call that
`00:ECC6` opened two transcripts earlier, three bytes above its own entry and in
interrupt context, which is why it is the one test whose `OBSERVE` names a
different address from its `ROUTINE`. Point the script at a seventh when a
family reaches a shape these six do not.

**The comparison finds more than absorption bugs, and one of them was four
slices old.** Pointed at `02:5408`, it reported the same calls with the same
bodies and the same registers, and two writes per call on the wrong cycle: the
transcript credited `STA $3E00,X` at offset 48 and `INC $3980,X` at 75, where a
store lands on its instruction's *final* cycle and the two belong at 51 and 72.
Every clean advance and reset call was three cycles out, in both directions.
The family's own regression records a hash of that write trace, so it had been
agreeing with the mistake since the transcript was written. Rebuilding with the
absorption removed and re-running is what separated the two: the control run
still differed, which made it older than the slice, and the emulated run said
what the right answer was.

**Before absorbing a return in a new family, check what its regression
observes.** 315 of the verify scripts take a call trace, usually of the routine
they replace, so most families will fail loudly rather than silently — but they
will only fail correctly if the walk declares what it absorbed. Declaring is one
call to `detail::absorb_return` per arm that reaches a return
(`native_replacements_absorb.cpp`), and the host validates it: exactly one per
transcript, ending the transcript, leaving `S` two above where the return
instruction found it. The helper takes the state on the return instruction's
own boundary and rewrites the transcript's resume, output registers, length and
final checkpoint from it — so it goes *last* in a walk, after every write is
staged, because it reads the stack out of the staged transcript rather than out
of entry work RAM.

**Absorb only what the walk itself ends on.** Two mistakes are easy here and
neither one is loud. An arm that hands the machine back mid-routine ends at an
ordinary instruction, not at a return, and absorbing there would claim a return
that never happened: `03:7B7A`'s suppressed arm stops at `$7B87` and its active
arm at `$7BB1`, and only its dormant arm reaches `$7BDF`. And an emulated `RTS`
is only a target if the routine that owns it is *already ported* — `03:603E` is
the second-largest emulated return left in bank `03` and belongs to an
unported parent at `03:6005`, so it is a request for a ported caller wearing a
return's clothes. Rank a bank by ported owner, not by cycles; the cheap
discriminator is whether the address already appears in a walk as a
`return_instruction`.

**An atomic walk has to ask whether it can afford the twenty-one cycles.** A
transcript with no checkpoints has no boundary inside it at which an interrupt
could be taken, so the horizon valve — in the registry deployment and in the
`@all` routes alike — declines it outright if its body would reach a user event
or a serviceable timer interrupt. Absorbing makes that body twenty-one cycles
longer, and a walk that absorbs regardless trades those twenty-one cycles for
its *whole body* on every call that no longer fits: three of bank `02`'s atomic
families lost 311 calls that way, and the loss is invisible except as a fall in
the admitted count. `absorbed_return_fits_atomically` in
`native_replacements_absorb.cpp` is the guard; wrap the `absorb_return` call in
it whenever the walk produces no checkpoints, and read the admitted counts
before and after as the check that it worked. An interruptible transcript is
not subject to the valve and does not need this.

**A shared routine absorbs at its entry, not inside itself.** A routine that is
both a registered entry and a callee some other transcript runs in line cannot
end past its own `RTS`: every composition site reads `return_instruction` to
learn which return the child stopped on, so that it can transcribe that return
into its own transcript at its own cycle. A child that ran past it would hand
the site an address in the *caller* and a body twenty-one cycles longer than the
one being spliced, and the site would then transcribe the return a second time.
The fix is additive and is not a rewrite: leave the routine alone, add a wrapper
that calls it and absorbs afterwards, and point the registry's dispatch at the
wrapper. `replace_collision_table_entry` and `replace_collision_response_entry`
are the two. Nothing composes a wrapper, which is the whole point of it. The
wrapper still has to check what it is absorbing — that the return is one of the
routine's own, that the stack pointer came back to where it started, and, if the
body it got is atomic, that twenty-one more cycles fit inside the horizon.

**Absorbing removes the return only from calls that finish.** A checkpointed
transcript is replayed checkpoint by checkpoint and can be cut short: if the
machine leaves it — an interrupt whose handler does not come back to the
checkpoint's exact resume state — the rest of the routine is interpreted,
including the `RTS` the transcript was going to absorb. So the row does not go
to zero for a long family, and the shortfall is not a bug. `03:6966` absorbs on
every one of its 14,307 registry entries — native cycles rise by exactly
14,307 × 21, which is how you confirm it — and its two returns still execute
8,381 times. Eleven of bank `03`'s twenty ordinary rows went to zero and the
long ones did not. Read the profile's residue after the edit; the ranking before
it cannot tell you which kind you have.

**A walking interpreter absorbs at its stop list, which is the cheapest edit
available and the one place a stop list has to be read rather than trusted.**
`replace_collision_tail` and `replace_actor_geometry_leaf` each end by stopping
in front of whichever address in a fixed list the walk reached, and each is
dispatched for many registered entries, so a single absorption at that one place
covered fourteen returns and 372,411 master cycles. But a stop list mixes
returns with handovers, and it mixes returns with things that only look like
them: eleven of the collision tail's twenty-five stops are `RTS` and only ten
are returns. `01:CBF9` is a tail dispatch — `01:CBF4` and `01:CBF8` push a vector
read out of a table and the `RTS` jumps to it — so the word it pops is one the
routine wrote two instructions earlier rather than a frame a caller pushed.
`absorb_return` would compute the right landing address from the staged writes
and still be wrong, because it would tell the observer a call had completed when
none had. Check the opcode at every stop, and then check the stack pointer each
`RTS` is reached with — which is not always the entry's, as the next lesson says.

Bank `45`'s music sequencer is the limit case of how cheap this gets. Fifteen
entry points share one walk, its `is_return_instruction` lists twenty-five
addresses, all twenty-five are genuine returns reached on a balanced stack, and
absorbing them all is **one condition for 575,916 master cycles** — more than
bank `00`'s six edits together. The bank contains the `01:CBF9` shape twice,
`45:4579` and `45:4BE6`, and both were already off the list; the stack-pointer
test would have refused them anyway, since a routine that pushes its own target
stands two below where it was entered. Reading the list still cost more than
writing the edit, and that is the right ratio.

**A return does not have to be reached at the pointer the routine was entered
with, and the pointer at the return is the one that matters.** Bank `00`'s call
wrappers are written in two halves: `00:E241` saves the MPR shadows and makes
its own `JSR`, and control comes back from that call not to `00:E241`
but to `00:E255`, a registered routine in its own right, which pulls the shadows
back off and returns two bytes above where it was entered — on the frame
`00:E241`'s caller pushed. `00:ECC6`/`00:ECE2` is the same shape one shadow
wider and taken in interrupt context. `absorb_return`'s stack-pointer argument
has always meant *the pointer at the return* rather than at the entry; every
family before `01:C5C9` just had them equal. So pass `registers.stack_pointer`
as the walk stands at the `RTS`, and give each stop the count of what its walk
gave back rather than testing it for equality with the entry. The count is a
structural constant per stop — it comes from modelled pushes and pops, not from
data — so a disagreement is a modelling error worth refusing on.

**A routine that returns above its entry cannot be watched, so watch the head
instead.** The routine-call observer opens a call at the entry pointer and
closes it on the next `RTS` standing there. For a tail like `00:ECE2` no such
`RTS` exists at that depth, so the call stays open and collects every write the
rest of the route makes — the trace passed twenty gigabytes before it was
stopped. Nothing is wrong with the routine or the absorption; the instrument
simply does not model this shape. The call the return actually closes was opened
by the head of the wrapper, so `dc_rom_absorbed_return_trace` takes an `OBSERVE`
argument and the sixth test replaces `00:ECE2` while watching `00:ECC6`. If a
write trace grows without bound, check whether the routine returns where it was
entered before looking for a bug.

**A loop head does not compare call-for-call, and expecting it to will send you
hunting a fault that is not there.** An entry reached by a `JMP` from inside its
own body — `45:4044`, `45:409C` and `45:4143` each close a loop that way — opens
a fresh call on every arrival when the route is emulated, but one transcript
runs through many arrivals and swallows them. At `45:4143` that is 19,062
emulated calls against 9,602 replaced, with every one of the 9,602 present in
the emulated trace verbatim. The right assertion is the subset relation, and in
that direction: a replaced call *absent* from the emulated trace is a real
fault, while emulated calls the replacement swallowed are the coarsening working
as designed. When the hashes differ anyway, the two-build check settles it —
rebuilding bank `45` without the absorption reproduced all three replaced
artifacts byte-for-byte, which also says what the absorbed-return declaration is
for: it leaves the observer's record of a loop head exactly as it found it.

**A promise the transcript never keeps has to be withdrawn, and for one slice
that was the technique's second limit.** A transaction stages the return it will
absorb when it is *installed*; the observer redeems the promise at the boundary
the machine lands on afterwards. Nothing withdrew it if the transcript was cut
short before reaching that point. For every family shipped up to bank `45` this
was harmless: the core interprets the rest, executes the `RTS`, closes the call
correctly, and the stale promise is redeemed one boundary later against a call
that is already closed — a no-op. It stops being harmless when the routine is
*re-entered at the same stack depth*, because then the stale promise finds a
live victim and closes it with an abandoned transcript's registers. `01:C510`
is the first family that is both, and it reported the wrong `Y` on 82,201 of
176,861 calls and the wrong `MPR4`/`MPR5` on 42,753, with every cycle column
still right — which is why the comparison catches it and a hash of the census
would not.

**The fix is `patches/0020-huc6280-replacement-abandonment.patch`, and what it
adds is a report rather than a behaviour.** The core already handled an
abandoned transcript correctly by interpreting the rest of the routine; what it
did not do was say so. `PopReplacementFrame` now takes a `completed` argument —
reaching the last checkpoint while still live is completion, every other way out
is not — and calls back with a `transcript_token` the host attached to the
result and the core only ever stored. The runtime erases the pendings that token
names. `01:C52B` fell from 73,536 master cycles to 819 and all 176,861 replaced
calls agree with the emulated trace on every column.

Two things about that are worth carrying. The half-fix tried first — withdrawing
when the core executes the `RTS` itself — did not partly work, it made the trace
*worse* (129,236 wrong `Y` against 82,201), because a cut-short scan usually
leaves through the dispatch at `01:C52C` and never reaches the return at all.
And the patch was expected to unblock bank `03`'s two largest rows on the same
reasoning; it did not move them by a cycle. Those 176,025 are residue on a
routine that already absorbs — transcripts an interrupt cut short, and arrivals
through in-line compositions — which are cycles the assembly really runs. A
limitation that explains one symptom is not the explanation of every symptom
that resembles it.

**One thing in the host that a single family could not have shown.** The
observer reports a boundary's cycle on two clocks, and only the absolute one
is monotonic: `master_cycle` is a slice timestamp and restarts every time the
core is re-entered. The deleted `RTS` boundary is recovered by counting back
from the boundary the return is recognised at, and that arithmetic is only
right while both are inside one slice. A return landing in the last cycles of a
slice is recognised at the first boundary of the next, and the subtraction then
measures against the wrong origin — `03:7F80` does this twice on the census
route, reporting 9 where the emulated observer reported 176,064. The slice
origin is now recovered from the absolute clock, and the transcript's own
reckoning is used only when the return is behind that origin entirely. The
palette family never straddled a slice, which is why its regression could not
have found this and the emulated-versus-replaced comparison in the next family
did, immediately.

The declaration deliberately does not cover a return absorbed on behalf of a
*composed* routine, part-way through a transcript — `03:7F57` has absorbed its
two children's returns since it was written. The observer cannot recognise those
by arrival, because the machine passes through those addresses without ever
stopping on them, and no regression observes a child while its parent is
replaced. That is a real gap, not a solved problem, and it is the first thing to
answer if one ever needs to.

The `JSR` half is the same argument from the other side, and is not new either:
a caller's walk that steps into the callee is composition, which `01:CB48` has
been doing since it was written — it pushes the frame, inlines the transcript,
and adds two back to `S` (`native_replacements_collision.cpp:298`-`364`). What
that needs is a *ported caller*, which is why it is a per-family question
rather than a mechanical sweep.

One thing that ranking will show which is *not* a target: the frame-sync spin at
`00:E323` is 45% of all master cycles. It is a `CMP`/`BEQ` pair waiting on the
VBlank counter, and replacing it would declare "burn N cycles doing nothing".
That raises the headline share enormously and ports no game logic, so it stays
emulated until the native frame boundary takes it.

This section used to name a second non-target: the timer handler at `00:FC00`
and the bank `45` code it maps in, on the grounds that they are the ROM sound
driver writing PSG registers on the hardware page and re-mapping banks with
TAM/TMA between them. That was wrong in the end — both are native now, the
write channel took the PSG page, and the epilogue that was the last emulated
part of the driver has since been taken too. Bank `45` is still easy to mistake for a
video bank: it only ever runs in interrupt context, entered by the `TAM #$40`
at `00:FC10`. On a 384KB image Mednafen maps physical banks 64-127 to ROM bank
`((x & 0x0F) + 32)`, so `45` is ROM bank `25`, not a mirror of a low bank.

#### A suspended transcript does not hide the code that interrupted it

For a long time the core would not consult the replacement hook while a
checkpointed transcript was suspended, and the ranking could not see past that.
An interrupt taken inside any transcript ran its handler emulated, so the sound
driver and the whole VDC handler chain appeared partly unported no matter how
completely they had been walked — and the appearance got worse with every slice,
because a longer transcript shadows more interrupts.

A suspension is machine state, not a lock. The handler is 65C02 code like any
other, and the transcript it interrupted is ordinary saved state; the core now
pushes it to a frame stack and lets the handler carry a transcript of its own.
Two consequences for how you read a profile:

- **A routine that is admitted on every call when hooked alone, but not under
  the deployed registry, is telling you about the harness, not the routine.**
  Compare the two runs before concluding anything about coverage.
- **A block that reads as uniformly blocked can be hiding real work.** The
  driver's 110M looked like one thing and was three: 37.7M of phantom entry-row
  samples, 46M the rule really did block, and 26.4M of epilogue nobody had
  walked. Only the first two went away when the rule did; the third had to be
  ported, and was, in the slice after.

### A device read is not automatically a wall

The standing rule was that a replacement callback receives work RAM and
registers, so a routine that reads a device register cannot be a transcript.
That rule is about the callback. It is not about the value, and the two come
apart whenever the value is something the walk can compute.

The pad reader at `00:E195` was recorded as unportable for four slices on this
rule, at 6.9M master cycles — the largest genuinely unported routine on the
route. What it reads back from `$1000` depends on exactly two things: the
frame's pad state, and the `SEL`/`CLR` latch lines, which the routine itself
drives through the same port. The latch is the walk's own bookkeeping. The pad
state is real outside input, and the fix for that is to hand it to the walk
rather than to let the walk guess: `RoutineEntry` carries it, taken from the
same field the core answers reads from, so the predicted value and the value
assembly would have read are one object instead of two kept in step.

Contrast the dispatcher at `00:E058`, one instruction earlier in the same
handler, which reads the VDC status register. That reports what the display is
doing — device state with its own clock — and it was written down here as
"genuinely phase 2, and it needs a native model of that device before anything
downstream can move."

**Both halves of that turned out to be wrong, and the slice that found out is
worth more than the cycles it recovered.** The VDC got its native model, and
the re-survey the next phase asked for produced a different answer on each
half:

- *The value was never the obstacle.* `PceVdc::read` takes a `peek` flag that
  answers without disturbing anything, so the byte could have been handed over
  on `RoutineEntry` exactly the way the frame's pad state is. Had that been the
  whole problem, it would have dissolved the moment the model landed.
- *The obstacle is the side effect.* Reading `$0000` clears every latched
  status source and is the only way the VDC's interrupt line falls. The
  replacement contract is a **write** channel, and a write to `$0000` selects a
  register rather than acknowledging one. A transcript that predicted the byte
  and skipped the read would return to a core whose interrupt had never been
  taken, and the handler would be re-entered on the spot.

So the classification is not two kinds of read but three, and the third is the
one that actually blocks:

- **Outside input, plus lines the routine set itself** — portable now. Model
  the port, emit the writes so the device is left where assembly would leave
  it, and pin both against an assembly trace.
- **State the device evolves on its own** — portable once that device has a
  native model *and* the entry carries a peek of it. This is a smaller category
  than it looks.
- **A read that changes the device** — not portable at any length, because the
  transcript has no channel for the change. This is the only hard one.

### The block is an instruction; the row says a routine

The corollary is the more useful half. `00:E058`'s status read is a single
instruction, and the whole dispatcher it sits in — 769,938 master cycles, the
nineteen instructions the census route reaches plus the arms below them — sat
on the blocked list for four slices because the
hotspot table, the registry and this document all recorded the *routine* as the
thing that was blocked.

Split at the instruction and both sides are ordinary. Above the read are four
register saves that touch only the stack page. Below it the byte the dispatcher
branches on is already in the accumulator, put there by a load the core
performed itself, so the device is never wanted again. `00:E058` walks to the
read and stops, the core executes it at its own cycle with its acknowledge
intact, and `00:E05F` picks the dispatch up. 640,374 of the 769,938 are native;
the 129,564 that remain are the one instruction.

This needed no new capability, which is the part worth remembering. The
execution and replacement hooks fire in `RunSub` at **every** instruction
boundary, before the opcode fetch — not at call targets — so any address in the
ROM is a legal entry point. `00:E05F` is not a jump target and nothing in the
game ever branches to it. That freedom had been available since patch `0001`.

**So when a candidate is refused for something one instruction does, record the
refusal against the instruction and check what is left either side of it.** Ask
it again, in those terms, whenever the reason a routine was parked has changed.
Widening the write channel is the small part; it took one address.

**And then check whether the instruction is also the cost, because usually it
is.** The palette row was re-read the same way one slice later. `00:EB30`'s
whole-palette arm splits cleanly around its `TIA $27B9, $0404, $0400` — 1,024
writes against a 512-write channel, and a block transfer has no interior
boundary for a chunk to stop at — so the arm is now native either side of the
transfer and the routine is admitted on every call. It moved 2,523 master
cycles, because the transfer is 625,095 of the arm's 627,618. `00:E058` was the
favourable case, not the typical one: there the unportable instruction was 17%
of its block and the ordinary code around it was the rest. Split anyway — full
admission is worth having, and an entry with an exception is an entry whose
regression cannot state a simple invariant — but price the slice on what is
either side of the instruction, not on the row's total.

There is a third verdict besides "native" and "blocked", and the same survey
produced it. The two fade drivers above the shadow buffer are portable: they
call the frame-sync spin at `00:E321`, and a walk stops in front of a `JSR`
like any other instruction. They are left emulated because the census route
enters one of them once and the other eight times. A transcript admitted on one
call cannot be shown to be state-generic rather than a transcription of the one
path observed, which is the whole difference between this method and the
replay-and-match method it replaced. **Too few calls to prove anything is a
reason to decline, and it is not the same as blocked** — writing it down as
"blocked" is what makes a later reader re-survey a row that has no answer left
in it.

### The size of a replacement is not the size of the routine

`03:6966` sat on the "next slice" line for several rounds because converting it
from an atomic body to a checkpointed transcript was priced as a rewrite of a
2,200-line replacement. The routine it replaces is 0xD2 bytes and about a
hundred instructions, and the walk that transcribes all of it is shorter than
the code it replaced.

The 2,200 lines were never the routine's complexity. They are what it cost to
*predict* the routine's result from its entry state, which is what the
replay-and-match method required: a resolver for the record format, a case for
each header shape, and a proof matrix pinning the paths that had been observed.
A transcript predicts nothing, so none of that is load-bearing for it.

So when a replacement looks too expensive to redo, price the **routine** —
disassemble it, count the instructions, and check what it touches. If the
answer is work RAM, its own bank's ROM and a stack page, the walk is a day's
work no matter how large the file in front of it is. The same reasoning applies
to the rejection log: a rejection that names a property of the data is worth
re-reading whenever the method changes, and so is an estimate that was made
against a different method's artifacts.

### A routine that calls a native child in a loop: one walk, many entry points

A routine whose body is mostly `JSR` into an already-native child cannot be
replaced as a routine. `08:4036` calls `08:40DF` twenty-one times a frame at
about 944 cycles each; one transaction covering it would blow past both the
512-write cap and every interrupt horizon.

Stop thinking of a replacement as covering a routine. The hook fires at every
instruction where no replacement is active, so what matters is the set of
addresses control *arrives* at with the machine between transactions: the
routine's head, and the address after each `JSR` it makes. Write one walk that
starts at any of them and runs until it reaches the next `JSR` — which the
machine then executes, re-entering the child's own replacement.

Make it one shared function rather than one transcript per entry, because the
stopping points chain: falling out of one loop drops into the next, so an
arrival at one return address can legitimately end at a completely different
one. A `switch` on the current pc, one case per instruction group, with a stop
list checked after each case, reads cleanly and handles every entry uniformly.

Two things to get right:

- Shadow any byte the walk both writes and re-reads. Queue cursors that a loop
  decrements and then reloads are the usual case; reading them from `work_ram`
  gives the value before the transaction's own writes.
- Entry counts change when the hooks are deployed together, because one walk
  absorbs arrivals another would have seen. Record the deployed-together
  numbers in the regression, not the solo ones.
- A child's `RTS` is not necessarily *this* loop's return. If the child has
  other callers, read the frame under the stack pointer and decline when it is
  not the address the loop's own `JSR` pushed. `00:F4FC` has fourteen such
  arrivals on the standard route, which is why its sweep is admitted at 66,432
  of 66,446 rather than at 100%.

### The call boundary costs 42 master cycles, and it can dominate a region

The entry-point pattern above has a price that is easy to leave unstated: every
`JSR` a walk stops in front of, and every `RTS` that comes back, stays
emulated. That is 21 master cycles each, 42 for the pair.

In a loop that calls out once per iteration, this becomes the whole remainder.
The collision sweep `01:CB48` calls `01:CC1A` at `01:CB5E` 84,735 times on the
census route. Both routines are native, and the loop between them is native
instruction for instruction, but `01:CB5E` and its matching `RTS` at `01:CC40`
were 3,592,728 master cycles — 0.28% of the entire route, and more than the
logic the sweep surrounds. Before deciding a region is finished, profile the
call instructions separately; a family whose entry-point count is high relative
to its cycle count is paying this tax rather than doing work.

The way out is **composition**: instead of stopping in front of the `JSR`,
transcribe it — push the return address, charge its 21 cycles — and keep
walking into the callee, then walk its `RTS` back. `01:CC1A` already did this
one level down with `01:CCD5`, staging the child's entry, calling the child's
replacement function and splicing its checkpoints; `01:CB48` now does the same
to `01:CC1A`. Four things are worth knowing before doing it again.

**Stage the parent's own writes.** The child is a replacement function like any
other and reads work RAM as it finds it. Copy the snapshot, apply every write
the parent has emitted so far, and pass that — the sweep had already bumped its
cursor and countdown by the time it reached the call.

**Make it an attempt, not a requirement.** A callee can decline, and it can
also return from somewhere the parent cannot resume from: `01:CC1A` leaves from
the middle of itself at `CCD2` or `CCD4` on a hit, where there is no `RTS` to
transcribe. Snapshot the checkpoint and write counts before composing and
truncate back to them if the child's `return_instruction` is not the one the
parent expects. 83,905 of the scanner's 85,715 calls compose; the other 1,810
end in front of the call exactly as before, and the callee stays installed for
them.

**Check the caps at the join.** Both vectors are bounded by the core's 2,048
checkpoints and 512 writes, and the parent's worst case is now its own plus the
child's.

**Expect to pay for the extra length.** This is the part that does not show up
in the arithmetic. The gross gain was exactly the tax — 83,905 × 42 = 3,524,010
— but a composed walk is a longer transcript, and a longer transcript is
suspended across more timer interrupts, each of which arrives while a
replacement is active and so runs the sound driver emulated. `00:FC00` lost
1,184 admitted entries and 683,271 native cycles; sixteen other routines lost
11,790 between them. Net `+2,828,949`, a bill of 19.7%. Composition is worth it
around a short body, where the tax is a large fraction of the region. Around a
long body the 42 cycles are noise and the shadowing is not.

**And there is a third option, which is neither of the two above.** Composition
answers "how do I get the callee's body into this transcript". Often you do not
want it there — that is the whole reason the walk stopped — and you still do
not want to pay for the call. So cross the `JSR` and stop: push the return
address, charge the 21 cycles, and end the transcript on the callee's *first
instruction* rather than on the `JSR`. The core resumes there, and if the
callee is a registered routine its own replacement is installed exactly as it
would have been. A transcript's `return_instruction` may be any address; it
does not have to be inside the routine that produced it, and it may be another
transcript's entry point.

The sprite-emitter driver `08:4036` is the case. Its four walks each ended in
front of a `JSR $40DF`, and the callee is 944 cycles — far too long to
compose, twenty-one calls a frame. Crossing the four calls took 73,818 × 21 =
1,550,178 master cycles with no new routine, no new entry point and no change
to any entry count in the family, because the callee is entered at the same
address either way. The transcripts grew by one instruction, so unlike
composition it pays no shadowing bill at all: it moves cycles out of the
interpreter without lengthening anything enough to swallow an extra interrupt.

The same move takes an indirect jump, and that is where it is worth the most,
because a `JMP ($nnnn)` is 21 master cycles with no stack traffic at all. Bank
`00`'s wrappers end on four of these between them — two `JSR`s and two
`JMP [$nnnn]` — and the module had written the jumps off in a comment: each
reads a vector the same transcript wrote two instructions earlier, so leaving
them to the core "removes the only place this module would have to compute a
resume bank." That is a cost, not an obstacle. Resolve the vector out of the
walk's own copy of work RAM, take the bank from `output_registers.mpr[target >>
13]`, and the transcript ends on the callee. All 30,468 calls, 639,828 master
cycles.

Resolving a vector yourself does move where it is read: the machine reads the
cell at the jump, the transcript reads it when the walk is built. Before
crossing an indirect jump, ask what could write that cell in between. Here
nothing could that would not already break the game, since the routine jumps
through a cell it has just written — but that argument has to be made, and a
work-RAM write trace scoped to the vector names every writer for you.

**A parent whose children are already native is the case where the two options
look most alike, and it is the one to get right.** The dynamic Tower-insect
controller `03:6016` is six instructions: a load, a `JSR` into the collision
interpreter, a `BSR` into the animation leaf, a load, a branch, an `RTS`. Both
callees were registered routines. Composition is written for exactly that
shape, and it was still wrong here, because the question is not whether the
callee is native — it is whether the parent needs the callee's *body* in this
transcript. This one does not: it passes its argument in the accumulator and
reads nothing back, and both callees are entered at their own addresses either
way, so composing would carry a variable-length interpreter body and a 147- or
183-cycle leaf for no cycles at all and be suspended across more interrupts for
it. Ask what the parent reads *after* the call, not how many calls there are.
When the answer is nothing, the routine decomposes into one transcript per
straight-line run — here three, of one, one and three instructions, 701,016
master cycles.

That decomposition needs an entry per resumption point, and the addresses are
not call targets: `03:601C` and `03:601E` are where a callee's `RTS` lands.
Registering them is ordinary — the core admits a transcript wherever `PC`
reaches a registered address — but it means the last walk is entered on a
frame it did not push and returns above where it started, so its absorbed
return has to be watched at the head that opened the call rather than at
itself. See *An absorbed return has to be declared* below.

**A call into another bank is the same edit, and the two banks it needs are
both already to hand.** `08:5977` builds an asset reference and calls the
resolver at `00:E1FD`, which lives in segment 7 and exists to switch MPR4 and
MPR5. Composing it is pointless — it is eleven instructions that are already
transcribed at `00:E20D` and `00:E200`, and their `TAM`s would land on this
walk's own register file for nothing — so the walk crosses it. Two things
follow, and neither is new machinery. The `return_instruction` bank is
`output_registers.mpr[target >> 13]`, read out of the register file the
transcript is declaring rather than assumed to be the walk's own bank, exactly
as it is for a resolved vector. And the walk that resumes afterwards reads its
data through the MPRs *the resolver left*, which arrive in `entry.registers.mpr`
like any other input; a source in `$8000`-`$BFFF` is then ROM through whichever
banks that call mapped, and `read_mapped` resolves it per address.

The callee's first instruction does not have to be registered for this to work.
The transcript ends on it and the core resumes there; if an entry exists it is
taken immediately, and if not the core interprets from that address as it would
have anyway. So crossing is never worse than stopping in front of the call, and
it is worth 21 master cycles even when nothing native waits on the other side.
`00:E1FD` is a live example of both halves: `08:5977` crosses into it, and its
own first instruction — a `JSR $E20D` that three call sites reach — is still
interpreted for want of a fourth entry.

Use composition when you want the callee's body in line — to reach a state the
parent needs afterwards, or to absorb the `RTS` too. Use this when you only
want the call. The `RTS` half of the tax is a separate question with its own
answer; see the absorbed-return sections.

### The stack is work RAM, and a transcript may push and pop

`RoutineReplacement`'s comment says the outer `RTS` "remains emulated,
preserving stack and caller-return behavior". That is the convention for a
whole-routine replacement — it describes where such a replacement should stop —
not a limit on what any transcript may contain. `output_registers` carries the
stack pointer, the stack page is ordinary work RAM at `$2100-$21FF`, and the
nested collision compositions in `native_replacements_collision.cpp` have been
writing `JSR` frames since they were written. `01:C51E`, `01:C520`, and
`01:C654` are the whole of the actor loop's linkage and are nothing but stack
traffic.

Three details decide whether such a transcript matches:

- `JSR` (`$20`) and `BSR` (`$44`) `PUSH` both bytes **before** the first
  `ADDCYC`, so each stack write carries the instruction's *start* offset. A
  read-modify-write is the opposite — `RMW_ZP` writes after `LastCycle()`, so
  it carries its end offset. Do not assume one convention for both.
- `JSR` and `RTS` are 7 CPU cycles (21 master); `BSR` is 8 (24), because it
  adds a taken `JR` to `ADDCYC(4)`. `RTS` pops, which is a read, so it emits
  no write at all and the census is its only assertion.
- `RMB`/`SMB` cost 7 CPU cycles rather than the 6 an ordinary zero-page
  read-modify-write costs: `RMW_ZP_B` adds a cycle that `RMW_ZP` does not.

Scope the regression's write range to the stack bytes the routine actually
touches, not the whole page. Observe the routine first and group the entries by
stack pointer; `01:C51E` sits at `$FD` for 168,748 of its 171,734 arrivals, so
`21FC-21FD` traces 1,068,240 writes where `21F0-21FF` traces 3,722,042. Both
match assembly, but only the narrow one is a reasonable artifact to keep.

### A transcript may change the bank mapping

`CpuRegisters` carries all eight `MPR` slots and the core applies them at every
checkpoint, so `TAM` is an ordinary instruction to a walk: set
`registers.mpr[slot]` and charge fifteen master cycles. The bank-switching call
wrappers do nothing else, and they are five instructions long.

Two details bound it. The core validates each declared write against the
**entry** MPR, not the checkpoint's, so a transcript that remaps a segment and
then writes through the new mapping is not expressible today — in practice this
costs nothing, because the shadows these routines write live in zero page and
segment 1 never moves. And `output_registers.mpr[return_instruction >> 13]`
must equal `return_instruction.physical_bank`, which is a real constraint when
the walk stops somewhere it has just remapped.

### A resume address does not have to be a fixed address

`01:C52C` ends on `JMP ($202F)`, and its transcript resumes on the first
instruction of whichever handler the jump table names — a different address
almost every call, in whichever bank `MPR` already maps for it. Nothing in the
contract requires `return_instruction` to be constant, so an indirect jump is
replaceable as long as the vector is readable from the snapshot. Compute the
bank from the entry's own `mpr[]`; the jump changes no mapping.

### A routine too large for one transcript can chunk at a loop's backward branch

The channel is 512 writes and 2,048 checkpoints. A routine that needs more than
either is not thereby unportable: it can be transcribed a piece at a time.

Give it a second entry point at the head of the loop that makes it large, and
end the transcript in front of the backward branch that closes that loop
instead of at an `RTS`. The core executes the branch itself, lands on the loop
head, and calls the replacement again for the next piece. Registers carry the
loop state across the boundary because `output_registers` are the machine's
registers — a chunk that stops mid-record resumes with the same `X`, `Y` and
pointer the assembly would have had. Nothing in the core needs changing: the
hook fires on any fetch at a registered address, and a replacement already ends
by handing one instruction back.

Two properties have to hold, and only one of them is obvious.

- **Every chunk must make progress.** A chunk that transcribes nothing leaves
  the core at the same address in the same state, and the next call produces
  the same empty chunk for ever. Decide where to stop by budget rather than by
  a fixed count, and make the first unit of work in a chunk unconditional: read
  the next unit's size, take it whole if it fits in what is left of both
  channels, otherwise stop — but always take the first one. If a single unit
  cannot fit in an empty chunk, decline the whole transcript and let the
  assembly run that unit; the hook fires again on the next iteration.
- **The leak is the branch.** Each chunk boundary leaves the closing branch,
  and whatever sits between it and the loop head, to the emulator. For the VRAM
  decoder at `00:E41F` that is a `BRA` and a `CLY` — eighteen master cycles
  across about 350 chunks on the census route, 0.09% of the 6.9M the family
  covers. Budget the chunk large enough that this stays noise.

`00:E41F` is the worked example: 167,000 master cycles, 3,400 port writes and
12,400 instructions in a single call, transcribed as one head chunk and about
eight continuations. Before this, routine length was a hard ceiling and
candidates like `00:EAA7` were set aside for exceeding it. It is now a
budgeting question, and `00:EAA7` was duly collected two slices later: 205,012
cycles a call, 101 transcripts, 1,640,100 cycles moved.

The stepper is the easier of the two worked examples and the difference is
worth naming. The decoder needs a lookahead — its stream records vary in
length, so it reads the next record's command byte at `00:E479` and stops if
that record would not fit in what is left of either channel, so that a
transcript never abandons work it has already transcribed. A colour in the
stepper's loop is a fixed seventeen writes and fifty-seven checkpoints, so it
decides on the counters alone and needs no lookahead at all. **Uniform loop
bodies chunk for free; variable ones cost you a lookahead.** Screen for that
when picking where to cut.

#### Chunk for a deadline as readily as for a cap

The channel budgets are what this started as, and they are the less common
reason to cut. A deadline chunks the same way and the arithmetic that says a
routine cannot meet one is the same arithmetic that says where to cut it. Both
bank-45 chunk heads came from a rejection of exactly this form: *this body is
N cycles, the timer period is 5,068, therefore every call declines.* Each time
the numbers were right and the conclusion was about one transcript. `45:4015`
is 12,667 cycles and chunks at `4044`; `45:4142` averages 2,055 and chunks at
`4143`. Being only twice the period rather than three times it is not a reason
to leave the second one alone — it is a reason it declines half the time
instead of always.

Two things to get right when the reason is a deadline rather than a budget.

- **Cut where the hardware already yields.** A chunk boundary is a point the
  core may take an interrupt at, so prefer one ROM itself leaves open. `4142`
  brackets each channel in `PHP`/`SEI` … `PLP`, and its own `PLP` at `41F1`
  runs before the closing `JMP` — so the top of an iteration is somewhere an
  interrupt could land anyway, and stopping there invents nothing.
- **The margin is a performance knob, not a correctness one.** The guard at the
  stop is what enforces the deadline; the margin only decides whether the
  transcript stops in time to satisfy it or overruns and is thrown away whole.
  That means it can be tuned by measurement — but prefer a bound the
  instruction sequence gives you over one the route happens to satisfy.
  `4142`'s iterations are 390 cycles plain and 2,190 with a waveform upload,
  and a margin of 500 measured 166,605 cycles better than the safe 2,304
  precisely because it gambles on the 186-in-19,062 case. Its worst case is
  different in kind: not a wrong answer, but a routine that quietly stops being
  admitted on data this route never produced.

### A transcript cannot see what preempts it

A transcript is computed once, from one snapshot, and then replayed. That is
fine while the routine owns its state, and it is wrong the moment the routine
runs with interrupts enabled and then reads memory that a preempting handler
writes. Checkpoints let the core deliver the interrupt at the right boundary,
but they do not re-run the callback: the values the walk baked in are the ones
that get applied.

The timer sound driver is the worked example. It does `CLI` at `00:FC32` and
its epilogue reads `$DA`, the rerun request another handler sets while it is
running. The walk's own `STZ $DA` at `00:FC30` is inside the transcript, so
`LDA $DA` at `00:FC84` would always read back zero and the handler would never
take its `JMP $FC03` chain exit. That is not a small effect: `00:E12A` writes
`$DA` 387 times on the census route and the chain is taken 387 times, so a
transcript predicting the branch would be wrong every time it mattered.

So when a candidate enables interrupts partway through, audit every load after
that point. A byte the routine only writes is safe. A byte another handler
writes is a hard boundary for the walk.

Ending in front of it is the cheap answer, and for a long time it was the only
one on offer here. The better one is to end in front of it *and start again
after it*: register the address the walk stopped at as an entry point of its
own, so the core re-enters and the callback is asked a second time, against a
snapshot taken after the preempting handler has run. That is the same
mechanism as chunking a transcript that is too long — `00:EAB0` in the palette
stepper — applied to staleness rather than to length, and it is what finally
took the driver's epilogue. Reach for it whenever the load is a hard boundary
but the code after it is worth having.

One caveat, learned the expensive way: pick the re-entry point by what the
core can express, not by where the stale read is. See "an instruction a
checkpoint cannot express" below.

Audit by measurement, not by reading the call graph, whenever the state is
widely shared. `$2F`/`$30` is the game's general-purpose source pointer and the
VRAM decoder reads it every stream record; 340 instructions across eight banks
write it, so no amount of reading proves which of them can run underneath. The
cheap answer is a two-column question against an assembly write trace: bracket
each call between a store only that routine's own header makes and its last
output write, then list every writer of the shared cells inside those windows.
For the decoder that returned 23,300 writes, all of them its own. One run,
before the walk is written, and it settles the question the census would
otherwise raise as an unexplained hash.

The bracket has to be a real call boundary, not a plausible one. That trick
works for the decoder because the only way into `00:E41F` is a call to it. It
does not work for the tilemap upload, whose tails at `00:F43D`, `00:F48D` and
`00:F4DF` are also reached from elsewhere, so a window inferred from "the first
write to the last write of an owned address" ran thirty times too long and
reported hundreds of imaginary intruders. Take the boundaries from an
`--observe-hook` call trace instead — it gives the entry and return cycle of
every call — and intersect the write trace against those. That gave 2,649 exact
windows and a clean answer: 92,978 owned writes, and nothing else but interrupt
frames below the stack pointer.

Once you have exact windows, the *in-window share* of each foreign writer tells
you which ones are dangerous, and it separates them without ambiguity. A
routine the walk calls runs only inside the windows, so it scores near 100%; an
asynchronous writer is uncorrelated with them, so it scores near zero. The
collision frame reads `$2D`–`$30`, the game's general-purpose pointer scratch,
which three hundred instructions across seven banks write. Ranked by in-window
share the list has no middle: eighteen arithmetic instructions at
`00:E56E`–`00:E847` at 99.3%–100%, and every other writer at exactly 0.00%. The
first group are callees reached through the family's own `JSR`s, which can only
run where the walk has already stopped; the second never overlaps at all.
A gap that clean is the answer. A writer landing at 40% would be the thing to
worry about, and none has appeared yet.

### An instruction the core builds from two `LastCycle`s cannot be a checkpoint

A checkpoint expresses one shape: advance to the interrupt sample, sample,
advance to the end. Nearly every opcode fits, because the addressing macros are
`GetXX(EA); ADDCYC(n); LastCycle(); …` — one cycle jump, one sample, one more
jump. `BBR` and `BBS` do not. They are `LD_ZP` wrapping `JR`, so the core reads
the zero-page byte and samples, then branches and samples again: four clock
steps, not two.

The second sample is the one that survives to the instruction boundary, so the
interrupt outcome *at that boundary* is identical either way — which is what
makes this hard to find by reasoning about interrupts. The damage is elsewhere.
Those steps are where `ADDCYC` tests `next_event`, and `HappySync` runs the
video `EventHandler` at whichever step first crosses zero. A coarser step runs
it at a later timestamp, and that shifted a VDC event far enough to move a timer
interrupt by one instruction four times in 3,600 frames.

Two of those four landed in assembly code *downstream* of the walk, in a frame
where every write inside the transcript matched. That is the tell worth
remembering: when a transcript's own artifacts are byte-exact and the divergence
is somewhere else entirely, the walk is perturbing the core's event phase, not
its own output.

There is no way to split the difference. An extra checkpoint inside the `BBR`
would reproduce the clock steps, but the core takes an interrupt at every
checkpoint boundary, so it would also invent an interrupt opportunity the
machine does not have and push the wrong return address. The only faithful
expression is to stop in front of the instruction and register the addresses
its two arms arrive at. In the tilemap loop that turned 539 transcripts into
17,248 and left 365,664 cycles emulated — 77% of everything still emulated in
the region — and it is the price of exactness.

When screening a candidate, grep its opcodes for `$0F`–`$7F` and `$8F`–`$FF`
with the low nibble `F`. Those are the only opcodes in the instruction set with
this shape.

### CLI opens the interrupt window one instruction late

The core samples the interrupt on an instruction's last cycle, and `OP_CLEARF`
runs that sample *before* it clears the flag:

```c
#define OP_CLEARF(f, ...) { ADDCYC(1); LastCycle(); P &= ~f; __VA_ARGS__ break; }
```

So the instruction after a `CLI` still runs masked. A checkpoint publishes
post-instruction registers, and publishing the post-`CLI` status hands the core
an unmasked `P` one instruction early. In the sound driver that let a nested
timer interrupt arrive nine master cycles ahead of the machine — the only
difference in an otherwise byte-identical 954,241-write census, and enough to
move the state census off the baseline.

The fix is to publish the old mask on the `CLI` checkpoint and let the clear
land on the next one. `SEI` has the mirror-image delay for the same reason —
but *not* the mirror-image fix, and this file used to say it did. See below.

### PLP defers the mask, and only the mask

`PLP` has the same shape — `ADDCYC(3); LastCycle(); P = POP();` — so the same
reasoning says to publish the caller's status and let the pop land on the next
checkpoint. That is right about the mask and wrong about everything else.

The two flags are used for different things. `IFlagSample` decides *whether*
the interrupt is taken, and the core sampled it before the pop, so it has to
be the old mask. But if the interrupt is taken, the core pushes `P` — and by
then the pop has happened, so the pushed byte is the popped flags. Deferring
the whole byte publishes the pre-pop status, and an interrupt landing on that
exact instruction pushes the wrong `P` onto the stack.

In the command interpreter this hit `46AE`, a `PLP` that restores a carry the
handler set eighty instructions earlier: the timer interrupt landed there
twice in 3,600 frames and pushed `$00` where the machine pushes `$41`. The
census still matched, because the interrupt handler restores `P` on the way
out and nothing read the stack byte in between.

`ReplacementCheckpoint` carries one status, so the correct split is: publish
the popped flags with the *old* mask bit, and apply the popped mask on the
next checkpoint. That leaves a single residual — an interrupt taken on a `PLP`
that also changes the mask would push the old mask bit. Nothing in the ported
set does that.

### Deferring a mask *clear* is safe; deferring a mask *set* is not

This file used to close the section above by saying the deferral is "exact for
`SEI`/`CLI`, where only the mask changes". That is right for `CLI` and wrong
for `SEI`, and the driver epilogue is where it was caught.

The two are not mirror images, because the residual the `PLP` note identifies
— an interrupt taken on this instruction pushes the wrong mask bit — is only
reachable in one direction. Deferring a `CLI`'s clear publishes `I` set, which
*suppresses* the interrupt at that boundary; nothing is pushed, so nothing can
be pushed wrong. Deferring an `SEI`'s set publishes `I` clear, which *permits*
it — and then the core pushes the checkpoint's status, so the handler stacks
an `I` the machine had already set, and the `RTI` restores an open window the
routine had just closed.

The census caught it in one frame of 3,600. The epilogue's writes were
byte-identical in value and order and 117 master cycles late, and the stack
trace showed a timer interrupt taken at the `SEI` boundary with `PC=$FC82,
P=$02` pushed. `--work-ram-write-range 2100-21FF` is how you read that: an
interrupt entry appears as three descending pushes at one timestamp, and the
`P` byte tells you which flags the core thought it had.

There is no publishable byte. `IFlagSample` needs the old mask and the pushed
`P` needs the new one, and a checkpoint carries one status for both. So `SEI`
joins `BBR` as an instruction the format cannot express, and takes the same
remedy: stop in front of it, register the address after it, and let the core
run the instruction itself. In the driver epilogue that costs 1,526,046 cycles
of a 26.7M slice.

The two exceptions are worth holding together, because they are found
differently. `BBR` is a *timing* defect — the transcript's own artifacts are
exact and the divergence is downstream. `SEI` is a *state* defect — the
divergence is right there in the transcript's own cycle stamps, and the stack
page names the instruction. If a walk's writes are correct in value and order
but uniformly late from some point onward, look for an interrupt the machine
did not take, and look at the mask first.

### The write channel reaches device registers, not just work RAM

`work_ram_writes` is a legacy name. Entries may target work RAM (physical bank
`F8`) or a bounded set of device registers on the hardware page (bank `FF`):

| Range | Device | Wait state |
| --- | --- | --- |
| `$0000`-`$0003` | VDC base ports | 3 CPU cycles |
| `$0400`-`$0405` | VCE register file | 3 CPU cycles |
| `$0800`-`$0809` | PSG register file | none |
| `$0C00`-`$0C01` | Timer counter and control | none |
| `$1402`-`$1403` | Interrupt controller | none |

The VDC and the VCE steal cycles — those are the two cases where `IOWrite`
calls `StealCycle` — so only they take the `- 6` interrupt sample offset; PSG,
timer and interrupt-controller stores are ordinary stores priced at `- 3` like
anything else. Confirm the cost against the profile before trusting it —
`00:FC27 STZ $1403` and `00:FC43 STX $0800` both measure exactly 15 master
cycles, which is the plain five-cycle store, while `00:EB73 STA $0404`
measures 18.

Widening this list is two edits, not one. The allowlist
`is_replaceable_hardware_port` in `src/emulation/pce_runtime_beetle.cpp` is in
this repo and decides what the channel will carry. The wait-state
compensation is in the fork, in `AdvanceReplacementTo`
(`patches/0006-huc6280-interruptible-replacement.patch`), and decides whether
a stealing port is priced correctly. Add a stealing port to the first and not
the second and the transcript runs three master cycles long per write, with
every value correct and every one of them late.

Device writes are invisible to a *work-RAM* census, so a routine that drives
them wants `--bus-write-range` and `--bus-write-trace` in its regression. The
state census does catch them indirectly — it hashes the framebuffer and the
audio as well as work RAM, so a wrong value reaching `$0806` moves
`audio_sha256` and with it the census hash. What the bus trace adds is
localisation: which write, which value, which cycle, from which instruction,
rather than "the audio changed somewhere in 3,600 frames".

### A hardware wait cycle belongs to the transcript, not the total

Only the checkpointed path prices a write to a stealing port correctly. It advances to the boundary *before* the write and lets the
ordinary HuC6280 bus path charge the wait cycle, so the declared offset is
where the write really lands. An atomic body pays that cycle twice: the
transaction's own `body_master_cycles` already contains it and `WrMem` charges
it again. `08:40DF` ran 24 master cycles per entry long for exactly that
reason — eight port writes, three cycles each — and the excess shifted every
later interrupt.

The same distinction sets the interrupt-sample offset. Ordinary instructions
sample one CPU cycle before they end, so `master_cycle_offset - 3` is right
for them. A stealing port store's last cycle is the stolen wait, which lands
*after* `LastCycle`, so it samples at `master_cycle_offset - 6`. Getting this wrong by
one CPU cycle is not cosmetic: it moved a VDC event, which moved a timer
interrupt by one instruction 943 frames later, which moved the state census.

Note also what the harnesses do not cover. `--routine-replay` compares
registers, return state, relative cycles, and ordered work-RAM writes; the
work-RAM write observer does not capture hardware-page writes at all. A
routine whose real output is VDC port traffic can pass every available check
and still move the state census.

### JSR writes its return address before it charges its cycles

Nearly every store lands on its instruction's final cycle, because the core's
addressing macros are `GetXX(EA); ADDCYC(n); LastCycle(); WrMem(EA, r)`. A
transcript can price a write by the checkpoint that follows it and be right.

`JSR` is the exception. Its body is `PUSH(PC >> 8); PUSH(PC); PC = ...;
ADDCYC(6); LastCycle();` — the two stack writes happen *before* any cycle is
charged, so they land on the instruction's **first** cycle, 21 master cycles
earlier than the same-shaped `PHP` two lines away (`ADDCYC(2); LastCycle();
PUSH(...)`, which does land at the end).

Check the operand order in `huc6280_ops.inc` for anything that writes outside
the `ST_*`/`RMW_*` macros rather than assuming. Getting it wrong is invisible
to the state census and to a device-write trace; it shows up only as a
constant cycle skew in a work-RAM trace that includes the stack.

There is a consequence worth stating separately, because it looks like a
collision and is not. A `JSR`'s start offset is also the *end* offset of the
instruction in front of it, so its two writes carry the same number as the
preceding checkpoint. The core disambiguates by instruction, not by offset: a
write whose offset equals a checkpoint's is flushed at that checkpoint only if
it names the same instruction. So the pair lands with the call, and an
interrupt delivered at the boundary before it finds a stack the call has not
touched — which is the correct machine behaviour and needs nothing said in the
transcript.

### Read a whole bank by its call graph before reading its instructions

When the candidate is a region rather than a family — a whole bank, or a
cluster with no obvious parent — the instruction profile alone is the wrong
lens. Bank `45` profiles as 46.1M cycles over 1,078 addresses with no address
above 0.042%, which says only "large and flat".

Scanning those executed addresses for `JSR` (`$20`) and `BSR` (`$44`) opcodes
gives the region's entire call graph in one pass. For bank `45` that was
sixteen targets, twelve of them leaves. Then walking control flow from each
target — following `JMP` and both arms of every branch, but *not* following
calls — attributes the region's cycles to routines rather than addresses.

That is a few minutes of work on an artifact already on disk, and it decides
the shape of the slice: a set of small leaves is taken incrementally, one
shared walk with several entry points, while a genuine monolith needs the flat
whole-routine treatment the sound driver got. Guessing wrong in that direction
is expensive — the same bank is either a 900-line module or a 10,000-line one.

### Diff against an assembly run, not only against the baseline hash

The state census proves the end state of each frame. It does not prove *when*
inside a frame a write happened, and a write credited to the wrong cycle can
still produce a matching census if nothing observes it before the frame ends —
until some later change makes something observe it.

The stronger check costs one extra run: record the same `--work-ram-write-range`
/ `--bus-write-range` trace twice, once with the hooks and once without, and
compare the files. Identical files mean every write matches in value, address,
order **and** master-cycle offset. Prefer this to asserting a hash you obtained
from the native run itself, which only proves the native run is reproducible.

**Include the stack in the range.** The first bank-45 slice passed the census,
the device-write trace and both configurations while crediting a `JSR`'s two
return-address bytes 21 cycles late. Adding `$21E0`-`$21FF` to the range
surfaced it immediately, as a constant +21 skew from frame 148 onward. The
same range later caught the `PLP` status fault above, on the strength of two
differing bytes in 5.5 million writes. Both faults are stack-only by
construction: an interrupt frame and a return address are written nowhere
else, so a trace that omits the stack cannot see either.

**Widen the range to whatever the new code writes.** The sample trigger at
`45:4776` writes nine bytes of the timer driver's per-channel zero page, none
of which the previous slice's ranges covered. A range chosen for the last
slice silently stops proving anything about the next one.

**Name zero page by its effective address.** The filter compares the write's
logical address, and the HuC6280 addresses zero page through MPR1, so `STA $2F`
writes `$202F`. A range written as `002F-0030` therefore selects nothing at
all, and the run still succeeds, still prints a write count and still produces
a stable hash — it is simply not looking at zero page. Two of the five ranges
in the bank-01 geometry regression were dead this way; corrected to
`202D-2030` and `2042-2045` the same trace goes from 128,629 writes to
2,277,424 and still matches assembly byte for byte. When a range is added,
check that the write count moves.

Profile before selecting a batch. Choosing by family narrative produced a
registry where 67 of 260 installed routines never execute on the route and 48
more never accept a call, while four addresses carry 79% of the native share.
Use this tool to pick the neighbourhood and
`Analyze-NativeFallbackHotspots.ps1` to profile the chosen family in detail.

### A guard that has never been crossed proves nothing about what is behind it

A composition site that gates on the *shape* of its child — a specific body
length, a specific return address, a specific path — is not only leaving
admissions on the table. It is suppressing exactly the calls that would
exercise the code behind it, so whatever is wrong there stays untested for as
long as the guard stands.

`03:73A6` pinned its collision child to the single 165-cycle one-pass
transcript, on a recorded fear that a longer record could take an interrupt at
the interpreter's return boundary. That fear expired when the interpreter
became an instruction-boundary transcript. Lifting the gate raised admission
from 64 of 129 to 128 of 129 and moved both the census and the write trace —
and the fault was not in the child at all. The parent's own tail had no
checkpoint for the boundary the hit gate's `RTS` at `73FD` lands on, so an
interrupt arriving there was deferred to the next checkpoint and stacked
`73CC` where ROM stacks `73CA`. The defect was already present in the 64 calls
the gate admitted. None of them happened to take an interrupt there.

Two habits follow. **When a guard is lifted, watch the artifacts, not the
admission count** — a rise in admissions with unchanged hashes is the result;
a rise with moved hashes means the guard was load-bearing for a reason its
comment did not name. And **when composing, take the child's return
instruction from the child**. The interpreter has four `RTS` instructions and
only the 165-cycle transcript always reached the first of them; the parent had
`0x6988` written in, which is correct exactly as long as the gate holds.

The rest of the tree was swept for the same pattern — a composition site
gating on a child's body length, checkpoint count, write count or return
address rather than on the caps. Six sites, one fault:

| Site | Gate | Finding |
| --- | --- | --- |
| `03:73A6` | child body `!= 165` | The fault above. Lifted; admission 64 → 128 of 129. |
| `03:7F57` | table child returns `6988`/`6A37` | Inert. Lifting it leaves admission at 20,862 of 20,867 and all three artifacts identical, because no call on the route returns through `6994` or `69A1`. Kept, because those two are unexercised — not because the code needs it. Both post-`RTS` boundaries already take the address from the child. |
| `03:7F57` | response child returns `7FD1` | Tautological. `replace_collision_response` sets `7FD1` on every path. |
| `01:C7F9` | child body `!= 102`, writes `!= 3` | Semantic, not conservatism: the one call it refuses is the countdown reaching zero, which starts a different routine's state transition. 254 of 255. |
| `01:C804`, `01:C812`, `03:7420` | child body `!= 159` | Refuse nothing anywhere measured — each admits 1 of 1 on its own fixture — and all three are entered zero times on the 3,600-frame route, so lifting them would be an unverifiable change. |
| `01:CC98`'s parent | child returns `CCD2`/`CCD4` | Propagates `mask->return_instruction` rather than hardcoding, so the gate restricts the set without the tail depending on it. |

The distinction that separates the one fault from the five non-faults is not
how tight the gate is. It is whether the code *behind* the gate reads the
child's shape from the child or has it written in. `03:73A6` had `0x6988`
written in; every other site takes the address from the transcript it spliced.
**A shape gate is only dangerous when something downstream assumes the shape.**

### A replaced *device* can pass the oracle by not running at all

Every lesson above is about transcripts, where a native body that never
installs shows up immediately: the entry counter reads zero. A device model
has no entry counter. It is installed by a pointer somewhere in the emulator,
and if that pointer is null the emulator quietly does the work itself and
produces exactly the output the census expects.

The native PSG matched `9036a9de…` on its first run. It was not being called.
`load_rom` shuts the core down before loading, the model was being torn down
in `shutdown_core`, and the install happened before the load — so the model
was created and destroyed between the request and the frame that needed it.
The census could not tell, because a correct emulator and an absent model
give the same bytes.

**A device swap needs a liveness proof separate from its correctness proof,
and the cheapest one is a deliberate wrong answer.** Double every delta the
model emits, rebuild, and confirm the census *moves*. If it does not, the code
is not in the path and any match is meaningless. Two attempts were needed here
even for that: the first perturbation changed the constant an idle channel
emits, which is a one-sample impulse of magnitude 1 against samples of ~10,000
and vanished into the resampler's rounding. A liveness perturbation has to be
gross.

Once liveness was established the real bug appeared, and it was the kind a
hash cannot localise: the model was built as a HuC6280 while the host pins
`pce_psgrevision` to `HuC6280A`, which centres waveform amplitudes and halves
every accumulated sample. **The remedy is not to read the setting in two
places but to have the emulator report the device it built.** Patch `0008`
passes the revision alongside the output buffers, so there is one decision and
the model cannot drift from it.

For localising the difference, the analogue of "diff against an assembly run"
is to run both devices at once. A temporary harness inside `Emulate` gave the
emulator's PSG its own buffers, seeded them from the live ones at the top of
each iteration, fed both the same writes, and compared every buffer position
after the frame's update. That reported the first divergent position, channel
and timestamp — and the ratio at that position, exactly 2, named the bug
outright. One caveat learned the hard way: seed the shadow buffers at the top
of the *inner* loop, not before it. `Emulate` may run its body more than once
per frame, and a harness seeded once per call reports a wall of false
mismatches from the second iteration onward, because the live buffer has been
integrated and resampled in between and the shadow has not.

### A route is an oracle for the game, not for the device

The census route proves what the game asks a device to do. That is not the
same as what the device does, and the gap is not always small: the native PSG
matched the emulator on every frame of the route while its LFO had never run
once, its noise source had only ever run on one of its two channels, and its
accumulating output mode had fired seven times in an hour of frames.

**Measure the gap; do not argue about it.** The model counts its own paths and
`--report-psg-coverage` prints them for any recorded route, which costs an
increment next to work that was already being done and turns "probably fine"
into a table. Do the same for the next device — the counters are the cheapest
part of the slice and the only part that says what the acceptance test is
worth.

**One counter is worth more than the path counts, when you can find it: the
one that says the model did the thing at all.** The controller port has plenty
of paths and they are all reachable by a machine that nobody is pressing
anything on, so `PcePad` counts reads that answered with a button held. That
is the host's input arriving, measured where it arrives, and
`dc_rom_native_pad` fails on zero rather than merely printing it. A path count
says which parts of a model ran; this kind says the model was in the path.

**Then check whether another route would close it.** For the PSG it would not.
The sequencer writes `$0809` from `$2E41`, and every store in the ROM that
names that byte writes a zero to it, so the plan's answer — "a censused route
that plays looping music" — would have reported the same zero the current one
does. Fifteen minutes of scanning the cartridge for stores to one address
saved recording a route to prove nothing. When a path looks uncovered, find
out whether the game *can* reach it before designing a route that supposedly
does.

**Where it cannot, the emulator is the oracle — drive it directly.** Beetle's
`PCE_PSG` needs nothing from the machine around it, so a small `extern "C"`
shim in the fork exposes it as a bare reference device and
`tests/psg_device_tests.cpp` runs both chips from the same synthetic register
stream. Three things make that test worth its runtime:

- Compare **state, not just output**. The buffers only show what has already
  been emitted; comparing every counter, latch and shift register catches a
  divergence at the edge it happens on rather than at the next one that emits.
- Assert the **coverage counters** the scenarios were written for. Agreement
  is only worth what it covers, and a scenario that quietly stops reaching a
  path should fail rather than pass.
- **Perturb the model** and confirm the test fails, path by path, exactly as
  with the census. A wrong LFO shift should fail the oscillator scenario; a
  wrong accumulating midpoint should fail on the revision that subtracts it
  and no other.

Such a test lives exactly as long as the emulator does, which is the right
lifetime: it is the record that the model was right while there was still
something to be right against.

**The VDC settled the question with much larger numbers.** Installing the
native video device and asking it what 3,600 frames reached: zero 16-colour
sprite fetches, zero sprite overflows, zero sprite-zero collisions, zero words
moved by the video-memory transfer engine — the game moves its sprite
attribute table 3,599 times and never copies within video memory once — zero
forced completions out of 11,008 stalls, and twenty words read back out of
video memory against 974,757 written. The census that passed with a
byte-identical hash was silent about every one of those, and a model that got
any of them wrong would have passed it just as cleanly.

So the division of labour is not a hedge, it is the design: **the census
proves the model is right about this game; the differential test proves it is
right about the chip.** Neither is sufficient and they are not redundant. Plan
both for any device from the start, and let the coverage counters — not
judgement — decide which paths the synthetic test has to carry.

### Build the shim before planning around the dependency

The PSG slice ended by recording what the VDC would cost: that unlike the
sound chip, the video chip is read back by the game, so a reference device
would need the machine around it, and that this was the part to design first.
It was designed first — by writing it, in an afternoon — and the premise was
wrong. `VDC` is standalone in the same way `PCE_PSG` is, its reads are methods
on the device rather than queries against the machine, and the shim came out
the same size as the sound chip's.

The lesson is not that the estimate was pessimistic. It is that the question
"can this be lifted out?" is answered by the class's includes and its member
list, and both are ten minutes of reading; a claim about coupling that has not
been checked against them is a guess, and one that had already been written
into two documents and was about to shape a slice. **When a plan turns on
whether a dependency can be isolated, isolate it and see, before designing
around the answer.**

Building it early pays twice, because the shim is where the real couplings
show up rather than the imagined ones. Two appeared that no amount of planning
had predicted:

- The device needed a **clock schedule**, not a timestamp. `VCE` decides how
  `run`, `hsync` and `vsync` interleave, and the picture depends on it — the
  same register writes draw 57,347 pixels on one dot clock and 76,413 on
  another. Copying that schedule into the test's driver is most of what a
  native VDC will have to reproduce, and it exists now, working, to build
  against.
- The wait-state hook's **refusal is load-bearing**. The device stalls by
  looping until the hook says no, so a shim whose hook always agreed would
  hang rather than fail — and it would hang only on the first port access
  during a block transfer, which the first draft of the test never made. That
  edge is worth reaching deliberately: assert the stall count, not just that
  the transfer finished.

### A differential test's comparison is easy; its stimulus is not

The native VDC was compared against the emulator's own device on one clock
schedule — every pixel, every register, all of video memory, the interrupt
line, the stall budget — and it passed on the first run, complete. That was
worth nothing. Thirty-eight deliberate one-line breaks to the model were then
tried, and **twelve survived**. The model was wrong in twelve ways the test
could not see, and every one of them was a hole in the stimulus rather than in
the comparison.

The comparison was right the first time because it is the easy half: compare
everything, after everything. The stimulus is the hard half, and **its
adequacy is not observable from a passing run**. The only instrument that
measures it is breaking the thing under test on purpose, and the number worth
recording afterwards is not how many were caught but how many were missed
first.

The holes generalise past this device, and are worth checking for by name:

- **Uniform stimulus hides indexing.** A tile map filled with one character
  makes every wrong-map-entry fetch draw the identical picture. This is the
  same shape as a test that writes the same byte to every address.
- **Periodic stimulus is barely better than uniform.** The first varying fill
  repeated every 1,024 entries, and the rows a wrong height mask selects are
  exactly a power of two apart, so it caught nothing. The window between a
  per-line latch and its use is a fixed 96 clocks, so a value alternating
  every clock presents the same value at both ends. Both were fixed by hashing
  the index rather than deriving it arithmetically. **If the thing being
  tested has a natural period, any stimulus sharing a factor with it is
  blind.**
- **Latching costs a cycle of the outer loop.** A register latched at vertical
  sync, in a driver whose vertical sync falls near the *end* of a field, does
  not affect the next field — it affects the one after. Writing a setting and
  asserting on the next field tests the previous setting twice and the new one
  never.
- **A section starts where the last one stopped, which is rarely where you
  want it.** A walk meant to land mid-display began in the vertical front
  porch, where nothing is drawn.
- **The device's own program can make a path unreachable.** The usual vertical
  program totals 263 lines inside a 262-line field, so the phase machine never
  completes a cycle and one of its two latch sites is dead code. Reaching it
  required programming a field the device could actually wrap inside.

The defence that works is a **liveness assertion per section**, not only per
run: assert that the thing the section is about was in use while the section
ran. Asserting "the walk ran while both layers were being drawn" is what
exposed `(hash >> 26) & 0xC0` — always zero on a six-bit value — quietly
writing *both layers off* at every clock for the entire section.

One more discipline is worth the words. Of the thirty-eight breaks, two were
*correctly* uncaught: two expressions in the background renderer decode the
same four bits and are then ORed together, so dropping a bit from one is not a
behaviour change. Chasing those would have meant contorting the stimulus to
detect a difference that does not exist. **When a perturbation survives, the
first question is whether it is a real behaviour change** — and the way to
settle it is to perturb the copy that is not redundant and confirm that one is
caught.

### An "already applied?" fallback must not share evidence with another patch

The fork applies its patch series with `git apply`, and when a patch will not
apply it checks whether the source is *already* patched by looking for two
markers. The branch that picks the markers was a chain of `if(name MATCHES
"^000N-")` arms ending in a bare `else()` — and the `else()` held `0007`'s
markers. When `0008` was added, nobody moved it out of the fallback, so `0008`
was being checked against `0007`'s evidence: it would have been declared
already-applied whenever `0007` was, including when it had not been applied at
all. The build would then have compiled a core silently missing a patch.

Nothing failed, because both patches were always applied together. That is the
shape of the trap: **a fallback that happens to be right for the last patch
added is wrong for the next one, and stays wrong quietly.** Give every patch
its own branch and its own markers, taken from files that patch alone touches,
and let the fallback be the newest one only because it is about to stop being
the newest.

**It happened again, exactly as described.** `0018` was added and left in the
`else()`, so it was being checked against `0017`'s markers -- the same failure
mode this section was written about, one patch later. It surfaced only when
`0019` needed the fallback and `0018` had to come out of it. The rule is not
"remember to move the fallback"; a rule that depends on remembering has
already failed twice. Give the new patch its own branch **in the same edit
that adds it to the series**, and leave the `else()` holding markers for the
patch you are adding right now, so the next slice has to displace it to do
anything at all.

`0020` was the first patch added after that was written, and the procedure held:
`0019` got its own branch in the same edit that added `0020` to the series, and
the `else()` now holds `0020`'s markers — `dc_beetle_active_replacement_abandoned_hook`
in the CPU source and `ReplacementToken` in the CPU header, both touched by no
other patch. Both halves were then exercised rather than assumed: `git apply
--check` against a tree carrying `0001`-`0019` and none of the slice's edits,
and a CMake configure of a tree where `0020` was already applied, which is the
only path that runs the marker branch at all.

### Starve the alternative, then let the census speak

An acceptance test that says "the model produced the same output" is weaker
than it looks whenever the emulator would have produced that output too. Every
device seam in this port has that problem: a model that silently failed to
install leaves the emulator answering, and the census matches perfectly.
The usual mitigation is to make the model the default arm and removal an
explicit flag, so at least the arms are labelled.

The controller port allowed something better, and the shape generalises.
While a pad model is installed, the host's libretro input callback answers
**zero** for every query -- so the emulator's own input path is not merely
bypassed, it has nothing to answer with. The census route presses RUN to start
a game, I to launch a ball, and a flipper every thirty frames for two thousand
of them; a run that had fallen back is a run where the game never starts. The
route reproducing `9036a9de...` anyway is then evidence rather than agreement.

Two things make it work, and both are worth checking for before reaching for
it. The seam has to be an *input* the emulator draws from somewhere the host
controls -- there is no equivalent for a device the machine drives and reads
back. And the starved path has to be provably unobserved elsewhere: the same
callback also feeds the frontend's turbo and six-button toggles, which this
host disables, so starving it costs nothing else.

Where starving is impossible, the fallback is the negative control: change one
line so the model receives nothing, record the route, and keep the hash. The
pad's is `24b1614f...`, and it is what makes the positive result mean
something -- an oracle nobody has ever seen fail is not yet an oracle.

### When the device cannot be lifted out, lift the machine down

The previous lesson says to build the shim before planning around the
dependency, and the VDC's shim rewarded that: the device turned out to be
standalone and the shim was a naming wrapper. `VCE` is not standalone, and it
is not close. It steals processor cycles for a stalled port access, drives an
interrupt line, asks the processor to stop early at two scanlines, and takes
its own timestamps from `HuCPU.Timestamp()` inside every one of its register
entry points. Reading the class's includes and member list — the ten minutes
the previous lesson recommends — says so immediately.

The reflex at that point is to conclude the device needs a differently shaped
test, or to reimplement the entry points without their machine coupling so
they can be called in isolation. Both are wrong, and the second is worse than
wrong: a reference arm that reimplements any part of the thing it is the
oracle for has stopped being an oracle for that part.

**The shim's job is not to remove the device from the machine. It is to supply
the smallest machine the device will run in.** Here that is the emulator's own
`HuC6280`, initialised and never executed, moved forward by
`StealMasterCycles` instead of by instructions. Everything `VCE` asks the
processor for turns out to be a counter it advances or a flag it sets, so an
un-run processor answers all of it correctly. Nothing is reimplemented; the
reference arm runs the emulator's code for every line under test.

The payoff was larger than the cost. Because the processor's clock is the
encoder's clock, it becomes the *test's* clock too — and a stalled port access,
which is otherwise an invisible side effect on a counter nobody compares,
shows up as the two arms' clocks parting company by different amounts. The
edge that is hardest to observe became the one that is impossible to miss.

Two small things are worth stealing from the shape of it. Keep the machine
from acting on its own — the processor's event handler was pointed at
something inert and its next event armed far away, so no code path could
re-enter the device behind the test's back. And leave the pieces that
genuinely cannot be reached alone rather than approximating them: one repair
inside `RunPartial` needs a running processor, so it is checked against a
stated expectation and *labelled as not compared against the emulator*, which
is worth more than a comparison against something the shim made up.

### A device installable only in a pair is exonerable only in a pair

`VCE` owns `VDC vdc[2]` by value, so a native encoder needs a controller, and
the obvious move is to give it the native one. That is one line and it is a
trap: the encoder would then only ever run beside the native controller, so
every arm of every test would move both devices at once, and a divergence
could not be attributed to either.

The fix is an interface — eight methods, exactly the ones the encoder calls —
with the native controller behind a wrapper on one side and the emulator's
chip reachable on the other. It costs one indirection on a call that already
crosses a scheduling boundary. **The reason to build it is not to join the two
native devices; joining them needs no interface. It is so the encoder can be
installed with the controller handed back**, which is the arm that says the
encoder alone did or did not move the picture.

The same argument settled the second question. The interface could have been a
base class of the controller, saving the wrapper — but the controller is
already installed and correct, and two of its signatures would have had to
widen to match. **Do not change a device that is currently right to suit one
that does not exist yet.** A wrapper is the cheaper apology.

### Two models, one place that decides which

Installing the encoder created a question the earlier device seams never
raised: with two native video devices that talk to each other, who decides
whether the encoder clocks the native controller or the emulator's chip?

The tempting answer is the host, which owns both objects: hand the encoder a
native-controller bridge when the native controller is installed, and an
emulator bridge otherwise. It is a few lines and it is wrong. The fork already
makes that decision — `DcVDC_*` has dispatched on `dc_beetle_active_vdc_model`
since the controller was installed — so the host's version would be a *second*
copy of it, and the two would have to agree not only about the answer but
about installation order, since either model can be registered first.

So the fork exports its existing dispatch as `dc_beetle_vce_vdc_*`, and the
host's bridge is one stateless object that always forwards there. **One
decision point, made where the evidence already lives.** All four combinations
then work without anyone enumerating them, and installing the two models in
either order lands in the same place.

The same question came back in the other direction and had the same answer. A
native controller stalls and interrupts through `dc_beetle_vdc_wait_state` and
`dc_beetle_vdc_irq_change`, which reached the emulator's encoder by name. With
a native encoder they must reach *that* one instead — so those two functions
gained the same dispatch rather than the controller gaining a second pair of
edges. The `### Next` note had asked whether to reuse them or give the encoder
its own; reusing them turned out not to be an economy but the only correct
answer, because a controller must not have to know which encoder is clocking
it.

### The census cannot reach the savestate

Two device models now carry state the emulator's savestate format is written
through, and each has a shuttle that moves it into the dormant object before a
save and back out after a load. Both shuttles had no test.

Not through neglect — through a gap in what a census can see. **A recorded
route never saves or loads a state**, so every arm of every census would match
with a shuttle that dropped half its fields. The acceptance that has carried
this port since the sound slice is structurally blind to this one thing.

The cheapest instrument that is not blind to it was already in the host: a
routine replay serializes at the frame before a chosen call, deserializes back
into it, and re-runs the call, so a shuttle that lost a field restores a
machine that produces different work RAM or a different write transcript.
`dc_rom_device_state_shuttle` runs that in four arms and requires the
restored call to be the *same* call — same frame, same body cycles, same
write count —
because a replay compares each arm only against itself, and two different
machines can each be self-consistent.

**When an acceptance has been reliable for several slices, ask what shape of
bug it cannot have a symptom for.**

### A routing patch moves memory, and silent corruption stops being silent

Installing the encoder broke four ROM tests with an access violation. Two
faults were behind it, and only one of them was mine.

The first was: on a state load the emulator's own loader calls `SetVCECR` on
the dormant object, which reported a change of horizontal timing to the
frontend — and the model's `apply_state` reported the same change a moment
later. Two entries per load where there should be one. A model that owns a
device must not leave the dormant object able to speak for it, and
`write_scanline_info` now returns early when a model is installed.

The second was upstream's, and it is the more useful lesson. The frontend's
`scanline_info` array holds sixteen entries behind an unbounded counter, safe
only while a field restart arrives before a seventeenth report. A host that
loads ten savestates without a full field between them breaks that promise.
**The overrun reproduces with every device model handed back**, so the code
that overruns is the emulator's — what the patch changed was only which
statics sit behind the array, and therefore whether the corruption crashed or
was quietly absorbed.

Two things to take from that. **A patch that adds functions and file-scope
declarations to a translation unit is not inert with respect to memory, even
when it is inert with respect to behaviour** — it relocates everything after
it, so a pre-existing overrun can change from harmless to fatal, or the
reverse. And when a new seam produces a crash, **check the fully-handed-back
arm before believing the seam caused it**; here that one run separated a bug
worth fixing carefully from a bug that was already there.

The wrong turn is also worth recording. I diagnosed the double-report first,
bounded the array, then reverted the bound on the argument that it masked the
double-report and removed the alarm. That argument was built on the assumption
that the overrun was mine. It was not, the bound was the fix for a real
upstream fault, and the reasoning had to be thrown away rather than patched.
**An elegant argument resting on an unverified premise is worth less than the
one run that tests the premise.**

### Mutate the model, or the test only proves it compiles

The previous slice shipped a shuttle test that passed while the defect it was
written to guard was present. The fix there was to make the test harder. The
lesson generalises past that: **a differential test that has only ever been
run against a correct model has not been shown to be able to fail.**

So the timer and interrupt controller's test was accepted only after seven
deliberate mutations of the model were each shown to break it — the short
reload tick removed, the divider started at the wrong value, an acknowledge
dropped, the mid-reload horizon off by one tick, the reload byte widened to
eight bits, the underflow moved from below zero to zero, and the I/O bus
latch dropped from a status read.

Six of the seven were caught by a named check. The seventh — the bus latch —
was caught **only** by the 200,000-step walk, because every named check
around the status register masked those bits off before comparing. That is
the argument for keeping a broad differential walk alongside the specific
checks even when the specific checks look exhaustive: the named checks encode
what I thought to ask, and the walk encodes what I did not.

The mechanism is cheap enough to be routine. A script rewrites one line of
the model, builds the single test target, runs it, records whether it failed,
and restores the file; six mutations cost about a minute. It belongs in the
scratchpad rather than the repository — it is a check on the test, run once
when the test is written, not a thing to keep running.

### A device that was never an object

The three device models before this one were classes in the emulator with
clear edges, and each reference arm was some version of "construct the
emulator's class". The timer and interrupt controller is not a class. It is a
dozen private fields inside `HuC6280` and four public methods the machine's
I/O page handler calls from outside, and there is nothing to construct.

The shim that worked is the one the encoder's slice invented for a different
reason: **run the emulator's processor without executing it.** For the
encoder that was a compromise — the encoder reaches the processor by name, so
the smallest machine it runs in is a processor. For this device it is exact,
because the device's entire relationship with time is one phase against the
processor's clock and that clock advances whether or not instructions do. A
stolen cycle and an executed instruction are indistinguishable to it.

There is a second, better property that came free. Stealing cycles runs the
processor's own `HappySync`, so the emulator catches its timer up at the
moments a real machine would — sometimes several times inside one call —
while the model catches up once at the end. The two arms therefore reach the
same state by genuinely different routes, which is a stronger comparison than
two objects being stepped in lockstep. **When a reference arm can be driven
through the machine's own scheduling rather than around it, the test stops
being a mirror and starts being an experiment.**

### A seam on the hot path is a mirror, not a call

Every device model before this one was installed at the machine's *edge*: the
I/O page handler, or the event handler the processor calls into. An indirect
call there is free, because the machine was already crossing a boundary at
that point.

The timer and interrupt controller is not at an edge. `CalcNextEvent` reads
its divider from `ADDCYC`, which runs on every instruction, and the interrupt
test at the top of `RunSub`'s loop reads its request register and mask just as
often. Routing those three reads through a vtable would have put a load, a
call and a return on the emulator's hottest path to fetch three integers — for
a device whose whole state is under fifty bytes.

What works is to split reads from writes. **Every write goes through the
model; the three values read once per instruction are mirrored in the
processor and refreshed by one call after every dispatch that can move them.**
`CalcNextEvent` is then left byte-for-byte as it was. The mirror is safe
because the set of things that can move those values is exactly the set of
dispatches, so a single `publish` at the end of each is total.

The cost is a rule rather than a risk: the mirror carries three of the
device's values back to the emulator and the other four have nothing to carry
them, so **handing the device back is only safe before a machine that will
reach its own power-on.** That is worth writing down at the boundary, because
it is the one property this seam has that the three before it did not.

### Route coverage says as much by what it never reaches

The census over 3,600 frames drives this device 8.8 million times, and it
never once reads the counter back, writes the interrupt mask, or reads the
status register. The game opens the mask by taking the reset vector and never
narrows it again; it acknowledges the timer by writing `$1403` rather than by
reading `$1402`, and there is no compact disc unit so IRQ2 is never asserted.

That is not a gap in the model — `dc_irq_device` enumerates all of it against
the emulator's own controller — but it is a gap in what a *census* can prove,
and the two tests only add up if the division is stated. **A device model
needs both: an enumeration against the reference for what the device can do,
and a census for what the game actually asks of it.** Printing the counts in
the census keeps the boundary between them visible instead of implied.

### When the device's answer is *when*, compare the sequence, not the state

Every device model before the scheduler had an answer to compare: a sample, a
pixel, a register, a divider. The scheduler's output is almost entirely a
*decision about when* — when the timer catches up, when the user event
handler is called, when the processor stops. Its state after a long drive is
a handful of small integers, and a wrong scheduler reaches the same handful
by a different road all the time: call the handler one advance late and then
once more to catch up, and the clock, both horizons and the rate all end up
identical.

So both arms fold the timestamp of every handler call into an order-sensitive
digest, and the digest is compared alongside the state after every single
operation. **A model whose product is timing has to be compared on the
timeline, not at the end of it.** The equivalent for the video chips was the
framebuffer hash, which is a sequence comparison wearing a state comparison's
clothes; for a device that produces no artifact at all, the sequence has to
be manufactured on purpose.

The corollary is cheap and worth taking: the digest costs four multiplies per
call and localises a failure to an operation, because the comparison that
detects it is the one immediately after the operation that caused it.

### What a device is *not* reset by is a fact about it

`HuC6280::Reset` and `HuC6280::Power` between them clear the whole processor
and the whole interrupt controller, and touch not one field of the clock.
Only `Init` does. That reads like an oversight until you notice the machine
calls `Init` for every ROM it loads, and that a reset which zeroed the
timestamp would rebase every device's phase underneath it mid-frame.

The model reproduces the asymmetry — an `init` and no `reset` — and the
differential test spends a section *demonstrating* it rather than asserting
it in a comment: drive the clock forward, reset, power, and check that
nothing moved. **A behaviour you decided not to implement deserves a test
that shows the reference does not have it either**, because the alternative
is a reader six months later "fixing" the omission.

### When the hot path *writes*, the seam is the storage, not a mirror

"A seam on the hot path is a mirror, not a call", above — mirror the reads,
dispatch the writes — held for the interrupt controller because the processor
only *reads* its three hot values. It does not transfer to the clock, and
noticing that before building rather than after was worth the hour it took.

`ADDCYC` runs on every instruction and it *writes* four scheduler fields.
Mirroring would mean refreshing the mirror from inside `ADDCYC`, which is the
whole cost the mirror existed to avoid. And there is a correctness problem
underneath the performance one: the handler an advance stops for reads the
clock back and re-arms its own horizon *while that advance is still on the
stack*, so a mirror would be stale at exactly the moment the machine depends
on it most.

So the seam inverted. **The model owns the storage and the machine points at
it; reads become loads and only writes stay calls.** `HuC6280` no longer has
a `timestamp` member — it has a pointer to the block the model holds, which
addresses the boundary's own storage when nothing is installed. The block is
laid out in the emulator's own field order, so nothing is translated in
either direction.

Three things fell out of that, and they are the reason to reach for this
shape rather than merely tolerate it. There is no publish, so there is no set
of dispatches to keep total. There is no capture-and-apply pair for the
savestate, because `SFVARN` writes the emulator's own format straight through
the model's fields — the only model so far that needs no shuttle at all. And
the reference shim lost two entry points the fork had been carrying purely
for it: with the block public, a test writes the two fields no opcode-free
driver could reach the same way a native machine will.

The cost is one rule, sharper than the mirror's: **installing moves where the
machine's clock lives, so it has to happen before the machine is built.** A
model swapped in mid-frame would not merely lose a device's state — it would
replace the clock underneath a running machine.

Two seams, two opposite answers, one question that chose between them: on the
hot path, is the traffic reads or writes?

### When the inputs can be counted, enumerate instead of driving

Every differential test in this series drove a stimulus and compared what came
out, and every one of them carried the same weakness unstated: **a stimulus
reaches what it reaches.** The timer's test drove 200,000 steps; the
scheduler's drove a route; both then had to argue that what they drove was
representative. That argument is the weakest part of each of them.

The instruction boundary did not need it. Its inputs are six bits of request
register, three bits of mask, one I flag and two latched values — so the test
enumerates all of them: 1,024 latches and 65,536 decisions, each against the
emulator's own rule. There is no coverage question left afterwards, because
there is no input that was not tried.

Before reaching for a stimulus, count the inputs. If the count fits in a
double loop, the stimulus is the weaker test *and* the more expensive one to
write. Two things make it fit surprisingly often: modelling the *decision*
separately from the acting on it, and offering the rule as a pure static
function beside the stateful wrapper the machine calls, so a test can drive
states no machine could be driven into — including the ones only a savestate
can produce.

Enumeration does not replace the named checks. The three asymmetries in this
rule are each asserted by name as well, because a summary of 65,536 agreements
says nothing about *which* facts a later rewrite must preserve, and the whole
point of modelling this piece separately was that its rule is not the obvious
one.

### Lift the rule out of the machine rather than copying it out

An observation patch usually only adds. This one moved code: the two
assignments at the top of `LastCycle` and the vector-selection cascade at the
top of `RunSub` became named methods that their original callers now call.

That is worth the churn in a hot function. A differential test is only as
good as its claim that the other arm is the machine — and if the patch had
*copied* the rule into an entry point beside the original, the test would
have been comparing a native model against a fork-side transcription, with
the real `RunSub` unexamined between them. Two copies of a rule drift; the
drift is silent, and it is invisible to precisely the test that exists to
catch it.

The cost is that a lift can be a semantic change by accident. Two guards
here: the census route must reproduce its hash afterwards, which is what says
the machine still runs the same, and the lift should keep the original's
structure rather than tidying it — the vestigial comment beside the timer's
test is left in place because it is evidence about the rule.

One compiler-level trap is worth naming, because it will recur in this loop: a
block move resumes by `goto` into the middle of `RunSub`'s do-loop, and a jump
may not cross an initialization. A `const` local at the top of that loop body
does not compile; the declaration has to sit outside it, beside `old_PC`.

### "The hot path cannot afford a call" is a measurement, not a rule

Two seams in a row concluded that a dispatch inside the instruction loop was
unaffordable, and both were right — but for reasons about *memory*, not about
frequency. The interrupt controller's three values are read once per
instruction and became a mirror; the scheduler's clock is written from
`ADDCYC` and became shared storage. Frequency was never the whole argument.

The instruction boundary runs more often than either — twice per instruction,
217 million times over a 3,600-frame route — and a plain call table turned out
to be the right shape anyway. Its inputs are values the processor already
holds in registers and its output is a small integer, so the call carries no
memory traffic and nothing on either side has to be kept coherent with
anything. Measured: 6.4 s per recording installed against 5.4 s handed back.
About a sixth, on a route that runs at nine times real time.

Two things make that a decision rather than a shrug. **Measure the arm you
would ship against the arm without it**, which the `--emulated-<device>` flag
every seam already has makes a two-command experiment. And **price the
alternative in correctness, not only in time**: the cheaper shape here was to
keep the emulator's `(IRQSample | IRQlow)` gate inline and dispatch only when
it opens, but that gate is the rule's own first clause, so buying the time
back would have put a second copy of the rule in the fork — the exact thing
the lift above exists to prevent. It would also have removed only one of the
two calls, because the latch is taken unconditionally.

The general form: a seam's cost is what it does to memory and to coherence,
and its price is a number you can go and get. Do not infer either from how
often the seam runs.

## Commands

Assuming the full host preset has been configured and `DC_ROM_PATH` is
available to CMake:

```powershell
cmake --build --preset windows-release --target dc_core_tests devils_crush --parallel 8
.\out\build\windows-vs2022\tests\Release\dc_core_tests.exe
ctest --test-dir out\build\windows-vs2022 -C Release --output-on-failure -R '^dc_rom_<family>$'
```

At a family checkpoint:

```powershell
cmake --build --preset windows-debug --target dc_core_tests devils_crush --parallel 8
.\out\build\windows-vs2022\tests\Debug\dc_core_tests.exe
ctest --test-dir out\build\windows-vs2022 -C Debug --output-on-failure -R '^dc_rom_<family>$'
```

Run Debug and Release builds sequentially: multi-config CMake generation and
build output are shared. Do not run their builds concurrently.

Always pass `--output-on-failure`, including on the full suite. `ctest` keeps
only the most recent run in `Testing/Temporary/LastTest.log`, so re-running a
failed test on its own destroys the record of why it failed — which is exactly
the moment the record is wanted. Redirect the full-suite run to a file as
well; a background run whose output is filtered down to a summary keeps the
exit code and loses the reason.

**While a suite is running, do not touch the build tree.** Not a build, not
another `ctest`, not even `ctest -N`. Both do damage:

- A concurrent `cmake --build` tries to relink `devils-crush.exe` out from
  under the tests running it. The link fails — which looks harmless, because
  the binary survives — but tests that were mid-run against it can fail, and
  the failure looks like a real regression.
- A concurrent `ctest`, *including a listing with `-N` that runs nothing at
  all*, rewrites `Testing/Temporary/LastTest.log`. The suite that is still
  running keeps appending to a file whose header now says a different run,
  and the failure records are gone.

This was learned by doing both at once: a full-suite run reported two failures
in `dc_rom_bonus_layout` and `dc_rom_bonus_scored_results`, and by the time
they were investigated there was no record of why, because a `ctest -N` five
minutes earlier had clobbered the log. Both passed in isolation, and a clean
run with nothing else touching the tree was 393/393. The failures were almost
certainly caused by the concurrent build, but "almost certainly" is the cost of
having contaminated the run — the whole point of the discipline above is not
to have to say it.

One intermittent failure is known and unexplained.
`dc_rom_knight_terminal_release_helper_post_return_actor_attribute_store_prefix`
failed once under `ctest -j 6` on 2026-08-10 and passed in the two full runs
either side of it, and on its own. The message was lost to the overwrite
above, so what diverged is not known. The obvious shared state was ruled out:
every ROM test writes its own artifact paths, and the backup RAM and cabinet
configuration are only touched by the windowed host, not by the recording path
these tests use. Treat a lone failure in this family as unconfirmed until it is
seen twice, and capture the output when it is.

**A second one was captured, and it is not a divergence at all.**
`dc_rom_palette_shadow_semantic` failed once under `ctest -j 6` on 2026-08-10
with `Could not commit work-RAM write trace; Windows error 32` — a sharing
violation on the artifact's atomic replace, raised *after* the 3,600-frame
census had completed and every native replacement had matched its expected
entry count. Nothing about the emulated run differed; the process could not
replace its own output file. It passed on its own immediately afterwards.

The lesson is to **read the failure before ranking the suspects.** This landed
in the middle of a slice that had just replaced the video device, which is the
most attractive possible explanation and the wrong one — the message names an
I/O error on a file, not a mismatch in anything. Windows error 32 under `-j 6`
means something outside the test held the file for the moment of the replace,
which on this machine means a scanner or the indexer. A test whose only
failure mode is "could not write its artifact" should be read as
infrastructure until the numbers themselves disagree, and it is worth
suspecting the same cause for the unexplained failure above.

**It happened a third time on 2026-08-11**, in
`dc_rom_actor_camera_sweep_semantic` under `ctest -j 6`, with the identical
message and the same shape: the 3,600-frame census completed and every
replacement reported its entry count, and only the atomic replace failed. It
passed on its own immediately afterwards. Two different tests with the same
signature is enough to stop reading it as a property of either — the artifact
commit itself is what is racing something outside the suite.

**Fixed on 2026-08-13, and the diagnosis above was right.** A fourth instance
was captured deliberately — `dc_rom_knight_terminal_release_helper_post_return_-
position_high_scratch_add_prefix`, `Could not commit State census recording;
Windows error 32`, at `-j 24` — which made four instances across four unrelated
regressions with one message between them. `devils_crush::core::commit_replacement`
now performs the replace, and retries while the refusal is one that another
process can stop causing: ERROR_SHARING_VIOLATION, ERROR_ACCESS_DENIED,
ERROR_LOCK_VIOLATION and ERROR_USER_MAPPED_FILE, on a doubling backoff inside a
five-second budget. Anything else fails on the first attempt as before, and so
does a transient that outlasts the budget, so a genuine permissions failure
still fails inside the same run rather than hanging it. All fifteen commit
sites go through it.

**Which file the scanner holds was measured rather than assumed, and the
intuitive answer is wrong.** A handle on the *destination* is refused with
ERROR_ACCESS_DENIED; only a handle on the *temporary* gives error 32. Every
message from the suite was error 32, so what the scanner had open was the
artifact the process had just written and closed — which is what an on-close
scan is for — and not the older file being replaced. `test_atomic_commit` in
the ROM-free core suite pins both, by opening the same no-sharing handle a
scanner opens and releasing it part way through the wait.

When a commit absorbs a wait it says so on stdout, naming the artifact, the
retries and the error — but **do not expect to see that line after a green
run, and do not read its absence as evidence the race stopped happening.** Each
verify script captures the recorder's output into an `execute_process`
`OUTPUT_VARIABLE`, and CTest surfaces that only when the test fails, so an
absorbed wait is swallowed by the same capture that would have shown the
failure. Five clean full runs after the fix are 2,055 test executions without a
commit failure, against roughly one per run before it; that is the outcome
evidence. The mechanism evidence is `test_atomic_commit`, which is
deterministic. To watch an absorption directly, run a recorder command by hand
rather than through CTest.

## Future acceleration work

The next infrastructure improvement is a persisted routine-replay corpus:
record normalized pre-call save states and expected artifacts from an
authoritative family route, then replay adapters directly without booting the
entire 3,600-frame scenario. Until that corpus exists, batching sibling work
against a single Release family route provides the best speed/fidelity trade.
