# Native Port Iteration Workflow

This workflow keeps routine replacement faithful while making the normal
iteration loop fast enough to port related code as a unit. It applies to
Milestone 4 native gameplay work; it does not weaken the release or milestone
acceptance criteria in [PLAN.md](../PLAN.md).

## Validation tiers

### Per slice

Run these after every bounded native change:

1. Incrementally build the affected Release targets.
2. Run the ROM-free core tests.
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

Run Debug build/core coverage and the owning Debug ROM regression after a
related batch, before declaring that family complete, and before a milestone
or hand-off checkpoint. A related batch normally contains two to five sibling
routines that share a route, actor family, or state machine. Run Debug sooner
when a change affects transactional invocation, interrupt delivery, shared
memory helpers, undefined-behavior-sensitive code, or a previously unseen
branch.

The Debug ROM route is an audit, not a second required execution of every
single narrow slice. Do not run an additional focused Debug route when the
family regression already proves the same replacement count and artifact.

### Broad acceptance

Run all relevant Debug and Release ROM-backed regressions at milestone,
release, CI, or deliberate cross-family integration checkpoints. Use the full
suite when changing shared runtime, trace, checkpoint, save-state, or ROM-load
behavior.

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

Admission is necessary but not sufficient. `02:57E9` and `03:6054` each pass an
`@all` regression, yet deploying either one alone moves the state census off
the assembly-only baseline, and each one's own regression expects the moved
hash rather than the baseline. Their evidence locks the divergence in instead
of catching it, so `kWithheldRoutines` in `src/app/main.cpp` holds both back
from every composed deployment — live play and cycle measurement alike. The
deployment summary names them on every run.

Their own regressions still drive them through `--replace-hook`, which is where
the divergence should be characterized. A withheld routine becomes deployable
once its regression expects the baseline census; remove it from the list then.

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
the assembly-only baseline `9036a9de...`, because the state-diverging routines
above are withheld. A composed run that changes the census therefore indicates
a new problem rather than a known one.

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
serviceable interrupt. In practice the whole body does.

Check it with two numbers, both available from a profile already on disk, and
check it *before* disassembling anything:

1. **Does the candidate's subtree read shared state, on how many calls?** Use
   execution counts, not the presence of the instruction. Bank 45's `4142`
   has a conspicuous `LDA $F5` at `415A` that runs on 12 of 3,604 calls and an
   inconspicuous `LDA $F5,X` at `4148` that runs on all of them. Reading the
   first and stopping gives the wrong answer.
2. **Is the body shorter than the interrupt period?** The period is elapsed
   cycles divided by interrupt-handler entries — 1,292,370,213 / 254,990 =
   5,068 on this route. A body well under that is admitted at roughly
   `1 - body/period`: `4142` at 2,054 cycles predicts 59% and measures 49.7%.
   A body past the period is admitted never.

That is how `45:4015` was ruled out. It is the bank's obvious next slice, it
composes entirely from routines already native, and its 12,667-cycle body
against a 5,068-cycle period means a transcript builder for it would decline
3,599 calls out of 3,599. Two queries, no code.

### A long transcript shadows the interrupt handler

The core will not consult the replacement hook while a replacement is active,
so every interrupt that lands inside a transcript runs its handler emulated.
That cost scales with body length and is invisible in the candidate's own
numbers. The palette slice added 9,937,182 native cycles and gave back about
694,000 of them, because `00:E9B6` runs 9,745 cycles per call — roughly two
timer periods — and cost the sound driver 1,181 native entries.

Net it was strongly positive, and the same will usually be true. But compare
the candidate's per-call body against the interrupt period before and after,
and read the change in the handler's own admitted count out of the cycle
census rather than assuming the delta is the candidate's cycles.

An atomic body priced against the horizon also truncates the calls it does
admit, so the loss is larger than the decline count suggests. `01:C510` walked
a mean of 142 master cycles per admitted call against a mean horizon of 265;
as a transcript it walks to the loop's own exit and averages 192, and its
native total went from 19.3M cycles to 33.9M.

### Whether the routine runs masked decides the shadowing cost

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

Two things that ranking will show which are *not* targets, and why:

- The frame-sync spin at `00:E323` is 45% of all master cycles. It is a
  `CMP`/`BEQ` pair waiting on the VBlank counter, and replacing it would
  declare "burn N cycles doing nothing". That raises the headline share
  enormously and ports no game logic, so it stays emulated and stays outside
  the portable pool the registry measures itself against.
- The timer handler at `00:FC00` and the bank `45` code it maps in are 91% of
  everything still emulated outside that spin. They are reachable — a
  replacement may hook any instruction address, and `01:C510` and `01:C606`
  are both entered by a branch — but they are the ROM sound driver, writing
  PSG registers on the hardware page and re-mapping banks with TAM/TMA between
  them. The write channel is proven for work RAM and the four base VDC ports,
  and the plan keeps the original drivers emulated. Bank `45` is easy to
  mistake for a video bank: it only ever runs in interrupt context, entered by
  the `TAM #$40` at `00:FC10`. On a 384KB image Mednafen maps physical banks
  64-127 to ROM bank `((x & 0x0F) + 32)`, so `45` is ROM bank `25`, not a
  mirror of a low bank.

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

### A resume address does not have to be a fixed address

`01:C52C` ends on `JMP ($202F)`, and its transcript resumes on the first
instruction of whichever handler the jump table names — a different address
almost every call, in whichever bank `MPR` already maps for it. Nothing in the
contract requires `return_instruction` to be constant, so an indirect jump is
replaceable as long as the vector is readable from the snapshot. Compute the
bank from the entry's own `mpr[]`; the jump changes no mapping.

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
take its `JMP $FC03` chain exit. No amount of checkpointing fixes that; the
walk has to stop before the read, at `00:FC7C` and `00:FD70`, and leave the
epilogue emulated.

So when a candidate enables interrupts partway through, audit every load after
that point. A byte the routine only writes is safe. A byte another handler
writes is a hard boundary for the walk, and the cheapest correct answer is to
end the transcript in front of it rather than to model the race.

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
land on the next one. `SEI` has the mirror-image delay for the same reason; if
a walk ever covers one, it needs the same treatment in the other direction.

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
next checkpoint. That is exact for `SEI`/`CLI`, where only the mask changes,
and leaves a single residual — an interrupt taken on a `PLP` that also changes
the mask would push the old mask bit. Nothing in the ported set does that.

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

Profile before selecting a batch. Choosing by family narrative produced a
registry where 67 of 260 installed routines never execute on the route and 48
more never accept a call, while four addresses carry 79% of the native share.
Use this tool to pick the neighbourhood and
`Analyze-NativeFallbackHotspots.ps1` to profile the chosen family in detail.

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

## Future acceleration work

The next infrastructure improvement is a persisted routine-replay corpus:
record normalized pre-call save states and expected artifacts from an
authoritative family route, then replay adapters directly without booting the
entire 3,600-frame scenario. Until that corpus exists, batching sibling work
against a single Release family route provides the best speed/fidelity trade.
