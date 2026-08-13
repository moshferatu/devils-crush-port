# Removing the emulator

The native-share percentage in [NATIVE_REGISTRY.md](NATIVE_REGISTRY.md)
measures one thing: how many master cycles are executed by native
replacements instead of by the emulated HuC6280. That is a good proxy for
"how much of the game logic have we ported", and a poor one for "can we
delete Beetle PCE". This document is about the second question.

## The three axes

Removing the emulator needs work on three fronts. The metric tracks one.

| Axis | What it means | Tracked by native share |
| --- | --- | --- |
| **CPU code** | Every routine runs as native C/C++ instead of emulated 6280 | Yes |
| **Device state** | The PSG, VDC, VCE and the timer/IRQ controller are native models, not Beetle's | No |
| **Time** | Interrupt scheduling and cycle pacing are native | No — and the metric never saw it move |

The gap matters concretely, and the sound and video paths are the examples
that have now been closed. `replace_sound_timer_driver` emits a write to
`$0806` and the core applies it with `WrMem` — but what it reaches is
[`src/audio/pce_psg.cpp`](../src/audio/pce_psg.cpp), a native device model,
not Beetle's PSG; a write to `$0000` reaches
[`src/video/pce_vdc.cpp`](../src/video/pce_vdc.cpp) and one to `$0400`
reaches [`src/video/pce_vce.cpp`](../src/video/pce_vce.cpp), which also draws
the picture. A write to `$0C01` reaches
[`src/cpu/huc6280_irq.cpp`](../src/cpu/huc6280_irq.cpp), which decides when
the timer interrupt is asserted.

And time itself is now native.
[`src/cpu/huc6280_scheduler.cpp`](../src/cpu/huc6280_scheduler.cpp) holds the
master-cycle timestamp every device in this machine measures itself against,
both event horizons, the overclock divider, the rate a processor cycle is
priced at and the loop flag that ends a frame. The emulator does not keep a
copy: the model's register block *is* the machine's, and `HuC6280` reads its
clock out of it.

What is still Beetle's is the *taking* of an interrupt, and the fetching of an
opcode. The native controller publishes a level and the native scheduler says
when; the emulator's processor samples the level at an instruction boundary
and picks a vector. At a hypothetical 100% native cycle share the emulator
would still be present and still own that.

So the percentage will plateau well short of "removable". Track the phases
below, not the number.

## The seam already exists

`PceRuntime` in
[`include/devils_crush/emulation/pce_runtime.hpp`](../include/devils_crush/emulation/pce_runtime.hpp)
is a pure virtual interface, and there are already two implementations:
`src/emulation/pce_runtime_beetle.cpp` and `src/emulation/pce_runtime_stub.cpp`.

The end state is a third — a native runtime satisfying the same contract with
no Beetle dependency. The part of the contract that has to hold is small:

```cpp
virtual EmulatedFrame run_frame(InputFrame) = 0;   // framebuffer + audio
virtual std::vector<std::byte> export_work_ram() const = 0;
virtual std::size_t backup_ram_size() const noexcept = 0;
virtual SaveState serialize() const = 0;
```

Everything else on the interface — execution hooks, replacement hooks,
routine-call observation, instruction profiling, write observers — is
transition scaffolding for the hybrid. A native runtime does not need any of
it, and it all gets deleted with the emulator.

## The acceptance test already exists

`--record-state-census` records, for each of 3,600 frames:

```
frame,input_p1,input_p2,work_ram_sha256,
framebuffer_width,framebuffer_height,framebuffer_sha256,
audio_sample_rate,audio_stereo_samples,audio_sha256
```

That is a complete oracle. It pins memory, video output and audio output,
frame by frame, against a fixed input script. A native runtime is finished
when the census still hashes to `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`.

Two consequences worth noting:

- Device models are directly verifiable. A native PSG is correct when
  `audio_sha256` matches; a native VDC when `framebuffer_sha256` matches. No
  new test harness is required for phase 2 — but the census answers "is the
  output the same", not "did my code produce it", and for a device swap those
  come apart. A model that fails to install matches the oracle perfectly. The
  PSG slice hit exactly that and had to prove liveness separately; see
  [NATIVE_PORT_WORKFLOW.md](NATIVE_PORT_WORKFLOW.md).
- The oracle covers one route. Code the census route never executes is not
  proven by it — see the unexercised arms of the sound driver noted in
  `native_replacements_sound.cpp`. Broader routes are worth adding before
  relying on the census as a completeness argument.

## Inventory: what still executes as emulated 6280

From a whole-ROM `--instruction-profile` of the deployed route
(1,292,378,139 master cycles), counting only addresses the profile does *not*
mark as a native entry — so a region that is fully ported drops out of the
table rather than appearing at its old size.

**Read the first row differently from the rest.** Master cycles measure where
the machine's *time* goes, not where its *work* is, and those two part company
exactly once: the frame-sync wait is 45% of the time and none of the work. It
cannot drop out of this table however much is ported, because the cycles are
the game waiting for the raster and something has to account for them. Every
other row is a porting target; that one is a measurement of idleness. The host
prints both denominators for the same reason — see *Two shares* below.

| Region | Master cycles | Share | What it is |
| --- | ---: | ---: | --- |
| `00:E2B7`–`E39C` | 584,758,197 | 45.25% | **Waiting, not work.** The frame-sync spin: the loop is `00:E321`–`E32B` (584,453,517) and all but 204,123 of *that* is the two-instruction wait at `00:E323`/`00:E325`. This row is the one entry in the table that is not a porting target and never was — the game is waiting for the raster, and there is no computation in the waiting to move. It is **done** as far as this document is concerned: phase 3 advances the machine through it rather than executing it, so 21,077,517 of the 24,248,940 turns are no longer interpreted. The cycles remain, because the wait is real machine time; what is gone is the cost of simulating it. The rest of the region is the colour encoder's initialiser and the two bit-set/bit-clear helpers around it |
| `00:FC03`–`FC8D` | 6,830,889 | 0.53% | Sound driver — body **and epilogue** now native on every arrival. Three deliberate refusals account for 6,810,810 of this: the `RTI` at `FC8D` (5,292,108), the `SEI` at `FC81` (1,509,498), and the chain exit `FC89` with the `FC03` trampoline it lands on (9,204). The remaining 20,079 is the body itself running emulated on the few dozen arrivals a route where the walk declines |
| `00:FD07`–`FD81` | 82,117 | 0.01% | The driver's second arm, likewise, with its own `RTI` (57,771) and `SEI` (16,548) |
| bank `45` (`4000`–`4A9B`) | 5,266,590 | 0.41% | Music sequencer — the tick is **native**, chunked at its voice loop, and so is the PSG programmer `45:4142`, chunked at its channel loop; what is left is `45:4936`, the track loader `45:4BCC` and the chunk boundaries |
| `00:E128`–`E290` | 1,289,166 | 0.10% | Interrupt epilogue and the two `RTI`s. The dispatcher at `00:E058` used to be listed here as blocked; both its halves are native now and what is left of it is the single `LDA $0000`, 129,564 cycles, whose read is the interrupt acknowledge. The handler bodies, the call wrappers and the pad reader are native |
| `01:C0FE`–`C1B5` | 1,407,828 | 0.11% | The top-level frame loop itself — glue between calls rather than a routine |
| `03:694A`–`6A37` | 2,721,348 | 0.21% | Collision-table interpreter — **already ported**; the `RTS` instructions and the resumption after an interrupt lands on a checkpoint, see below. The returns are 442,872 of it and are now known to be answerable — but not by a one-line change, because the same walk serves three composition sites that need it to stop in front of them |
| `00:E93C`–`EBD6` | 772,635 | 0.06% | Palette shadow buffer. **Not one blocked routine**: 625,095 of it is the single `TIA` at `00:EB46`, 140,037 is the two fade drivers, 7,440 is chunk boundaries and resumption — and the `RTS` instructions, 302,628 of this row in the revision before, are now **63**. The transcripts absorb their own returns. See below |
| `02:586F`–`5903` | 501,195 | 0.04% | Sorcerer tick — **already ported**, and both interpreter calls are now composed in line; what is left is the two bank-wrapper calls, three `RTS` instructions and resumption |
| `01:CA84`–`CC04` | 621,648 | 0.05% | Collision frame — **already ported**, and the call into the scanner is now composed rather than emulated, see below |

Earlier revisions of this table mixed two conventions: five rows counted only
emulated addresses and four counted the whole range including native entries,
which made ported regions look like remaining work. Every row above is now the
emulated count.

The sound-driver row has been wrong twice, in the same direction both times,
and the corrections are the reason it read 31,738,295 rather than the
193,007,589 it started at. Porting the epilogue then took it to 6,830,889.

A native body's cycles are charged to the last instruction the profile sampled
before it, which is normally the routine's own entry point — except where a
handler returns *into* a suspended transcript. The `RTI` at `00:FC8D` does
exactly that, and was carrying 99,636,684 cycles of other routines' native work
into the driver's total. `00:FC8D` turned out not to be a special case but the
visible instance of a general one — *any* instruction that precedes a native
call absorbs it — so the rows above are not priced from the native run's cycle
column at all. Each address is priced at its executions in the native run times
its cost in an assembly-only run of the same route, where nothing is replaced
and so nothing can be absorbed.

That left 110,519,836, recorded here as driver entries the no-nesting rule
blocked. It was still too large by more than half, for two reasons that only
separated once the rule was gone. **A registry entry point is sampled but never
executed**: the instruction hook runs before the replacement hook, so every
native call leaves one sample on an instruction the core then skips, and
pricing those from assembly invents 37,655,643 cycles route-wide. And what
remained after that was not one thing but two — arrivals the rule really did
block, and a genuinely unported epilogue that had no way to show itself while
the whole driver read as blocked. Nesting recovered 85,778,658 cycles across
the driver, the VDC handler chain and the sequencer, and the epilogue slice
then took 25,180,701 of what nesting had exposed. Both driver rows above now
total 6,913,006, and 6,880,569 of that is deliberate refusal rather than
unwritten work: two `RTI`s (5,349,879), two `SEI`s (1,526,046) and two
`JMP $FC03` chain exits (4,644). The rest is the `FC03` trampoline and the few
dozen arrivals a route on which the walk declines and the body runs emulated.

Pricing this way also separates the hybrid's own overhead from unported code:
the `JSR` and `RTS` at each end of every native call, and interpretation
resumed after an interrupt landed inside a transcript (8,581,961 route-wide).
Neither is work anyone can port; both go when the emulator does. See
`docs/NATIVE_PORT_WORKFLOW.md` for the method.

Two rows have left the table. `00:E3C0`–`E47C`, the VRAM and VCE upload paths,
was 7,031,316 cycles two slices ago and is 10,779 now — the loop-closing
branches the chunked transcripts hand back to the core, plus a one-call stub at
`00:E3E7` that jumps into the frame-sync region. `00:F240`–`F4FB`, the tilemap
upload and the camera sweep's outer loop, was 5,761,776 and is 569,859. Most of
what is left there is one instruction: the `BBR` at `00:F423`, which the core
has to execute itself because it is assembled from two `LastCycle`s and a
checkpoint expresses one. That costs 365,664 cycles, and the section on it in
[NATIVE_REGISTRY.md](NATIVE_REGISTRY.md) explains why paying it is the only
faithful option.

The collision row is the same story told louder, and it now has an ending.
`01:CA84`–`CC04` was 3,974,346 two slices ago and is 621,648; `01:CD74`–`CE4D`
— the playfield-tile probe, which never had a row of its own — fell from
2,643,867 to 89,427. The step between those two figures was the call boundary.
Porting the frame left the row at 2,399,445, of which 1,795,419 was a single
`JSR $CC1A` at `01:CB5E` and a further 1,797,309 was its `RTS` at `01:CC40`:
42 master cycles paid 84,735 times, with both sides native and the loop between
them native instruction for instruction, because **a transcript cannot contain
a call**. The sweep now composes the callee instead of stopping in front of it,
and those two instructions are 17,622 and 19,776.

The sound-driver row grew by 8.2M at the time, and almost none of that was
driver work. 7.5M of it landed on `00:FC8D`, the driver's `RTI`, which the
profile charges with whatever the timer interrupted — and what it interrupted
was now a longer transcript, so more already-native cycles replayed with no hook
in between. The part that was a real loss is 683,271: the 1,184 driver entries
that arrived while the composed sweep was suspended and ran emulated because the
core would not nest a replacement. That is the whole reason 3,524,010 gross
became `+2,828,949` net. **That tax no longer exists**: replacements nest, and a
long transcript costs the driver nothing.

The Sorcerer row is the same shape one slice later. `02:586F`–`5903` was
4,055,142 with no native entry anywhere inside it and is 910,530, and about 87%
of that remainder is again the leaf contract: two calls to the collision-table
interpreter at `03:6966`, two to the bank-01 wrapper at `$C018`, and three
`RTS` instructions. The interpreter was the obvious thing to compose next, and
the reason not to rush it was in the row above it — `03:694A`–`6A37` was
5,274,582 because the interpreter was an atomic body rather than a checkpointed
transcript, so it declined 2,850 of its own 34,259 entries at an interrupt
horizon. Splicing it into a longer parent would have put that uncheckpointed
stretch inside the parent as well.

That is fixed: `03:6966` is now transcribed instruction by instruction, and no
call anywhere on the route is refused at an interrupt horizon. The consumer at
`03:7F57` showed what that was worth without any new code at all — its
composition was already written and gated on the child having checkpoints, and
it picked up 749 calls and 886,317 cycles the moment the child had them.

The tick took a slice of its own, and it is done: both `JSR $6966` calls are
run in line. The Sorcerer row is 501,195 and the interpreter row 2,700,153. The
two rows are the clearest illustration in this table of what composition is and
is not. What actually stopped being emulated is small — the two `JSR`
instructions and the interpreter's `RTS`, 731,907 cycles between them. The other
806,532 of the 1,538,439 gross is three replacements becoming one, which costs
the machine nothing directly but removes two hook boundaries per call. The route
went from 2,367,401 hook entries to 2,332,709, and because the shadowing bill is
paid per displaced entry, that made this the cheapest slice yet measured
relative to its gross: 15.8%.

The interpreter's own remainder is worth separating, and composition separates
it cleanly. 442,977 of the 2,700,153 is the `RTS` instructions the transcripts
still stop in front of, which is the leaf contract and is priced in — it was
765,549 before the tick composed and the difference is exactly the returns that
composition absorbed. The other 2,257,176 is resumption: when an interrupt lands
on a checkpoint the core delivers it and hands the rest of the routine back to
the emulator rather than re-entering the transcript afterwards. That figure is
identical before and after, to the cycle, which is the demonstration that
composing does not touch it. It is a harness property, not unported code, and it
belongs in the same category as the two blocked rows below.

Two rows never made it into the table at all, and that is the point of
recording them here. `01:C8C2`–`C971` was 1,743,819 cycles and `01:DAF8`–`DB9D`
was 1,508,400 — the two largest single routines on the route with no native
entry anywhere inside them, both below the threshold this table uses. They are
113,355 and 45,600 now, and 158,472 of that combined remainder is the four
`RTS` instructions the transcripts stop in front of. The lesson is about the
table rather than the routines: a row that groups by address range hides
anything that is large but not adjacent to anything else large. Rank by routine
as well as by region before deciding a phase is finished.

The sound-driver row is not unported work. That code is native, proven and
byte-exact; it runs emulated on 48% of entries only because the replacement
hook is gated on `!ReplacementActive` and a timer interrupt usually preempts
another suspended transcript. **Do not fix that in the emulator.** It is a
limitation of the hybrid harness, it ports nothing, and it disappears on its
own when the CPU does.

## Two shares

`--replace-registry` prints the native cycle share twice, and the second line
is the one to watch:

```
Registry cycles: 679286022 native of 1292371731 elapsed master cycles (52.56%)
Registry cycles outside the frame-sync wait: 679286022 native of 710482755
  (95.61%); the machine waited at 00:E323 for 581888976 master cycles
  (45.02% of the route)
Instructions interpreted: 66312528
```

The first line is true and nearly useless as a progress signal. Its
denominator is 45% frame-sync wait, and no amount of porting can move that
part, so the figure is pinned near half however much of the game goes native.
Watching it would suggest the port had stalled at exactly the point it stopped
having anything left to stall on.

The second line divides by the cycles in which the machine had something to
do. It is the figure this document has always quoted informally as "outside
the idle spin loop", now measured by the machine rather than worked out by
hand — every turn of the loop, skipped or executed, priced at the rate the
processor was running. It is only available while the fast-forward is
installed, because that is what counts the wait; `--emulated-idle` prints a
line saying so rather than guessing.

It has a denominator too, and the denominator is itself a measurement. Two
values for the wait appear in these documents — 584,249,394 and 581,888,976 —
so the cycles outside it are quoted both as 708,122,337 and as 710,482,755,
and the same share reads as 94.8% under the first and 94.53% under the second.
Nothing regressed between those readings — the port gained 640,374 native
cycles across them — the denominator simply grew by more than the numerator
did. **Compare native cycles when comparing two runs**; a percentage is a
reading of one run, and two percentages measured against different
denominators do not subtract.

The third line is there because the first two cannot show it. Fast-forwarding
the wait removes *instructions* without removing the *cycles* they would have
taken, so it moves neither share: the route interprets 66,315,525 instructions
where it used to interpret 108,470,069, and both cycle shares are unchanged to
the byte. That is the correct outcome and worth stating plainly, because the
opposite expectation — that eliminating 45% of the cycles should move the
coverage — is the natural one and it is wrong.

## Phases

### Phase 1 — finish the CPU code

Remaining real logic, in rough order of value:

1. **Bank `45` music sequencer** — **finished for the hybrid** at 39.5M of
   46.1M. The PSG write channel and the bus-write census built for the timer
   driver applied unchanged, as expected. The bank turned out to be sixteen
   small subroutines rather than a monolith, so it went in slices: the
   sample/envelope group, the PSG register programmer `45:4142`, the
   portamento stepper `45:4228` and the command interpreter `45:457A` are
   native. See
   [NATIVE_FALLBACK_HOTSPOTS.md](NATIVE_FALLBACK_HOTSPOTS.md#the-music-sequencer-and-screening-a-bank-instead-of-a-family).

   The per-frame tick `45:4015` (13.8M), which calls all of them, is native as
   of the chunking slice; what is left of the bank is 6.6M, most of it
   `45:4936`, the track loader `45:4BCC` and the chunk boundaries themselves.

   **This entry said `45:4015` cannot be ported while the emulator is here,
   and that was wrong on both of its two grounds.** It is recorded here as it
   was found, because the way it was wrong is the most reusable thing in this
   file. The claim was: the tick's voice loop reaches `45:4148`, `LDA $F5,X`,
   so a transcript for the tick reads timer-driver state and the shared-state
   guard applies to the whole body; the body is 12,667 master cycles against a
   5,068-cycle timer period, so it can never finish before the next
   serviceable interrupt and would decline 3,599 calls out of 3,599.

   The second ground is the `00:EAA7` error again: **the body that has to fit
   is a transcript's, and a transcript is a free choice.** The tick's body is
   a loop. `45:4044` is its head, `45:40FF` is a `JMP $4044` that closes it,
   and the profile counts the shape exactly — 32,391 iterations across 3,599
   calls, 28,792 of them continuing and 3,599 exiting. Chunked at `45:4044`
   the unit that must fit the period is one voice iteration, and that is
   **1,137 master cycles composed** (12,728,115 of the tick's own plus
   24,091,110 of already-native callees, over 32,391 iterations), not 12,667.
   Against a 5,068-cycle period the same arithmetic that predicted zero
   admissions predicts about 78%.

   The first ground is a plain factual error, though not the one that
   mattered. **The voice loop does not reach `45:4148`.** `JSR $4142` occurs
   exactly once in the bank, at `45:4138` — *after* `45:40FF` closes the loop
   — so it runs once per call in the tail, and `4148`'s 10,812 executions are
   three per call of `4142`, not three per call of the tick. Every branch in
   `4044`-`4101` targets an address inside the loop or the exit at `4102`. The
   walk accordingly stops in front of `45:4138` and leaves `4142` to be
   entered on its own, as it already is, with its own guard.

   **But the conclusion that rejection drew was right for a reason it never
   identified, and the port had to find it the hard way.** The tick does read
   driver state on every voice iteration — not through `4142` but through the
   amplitude commit at `45:49D2`, which reads `$20F5,Y` and which both `49B6`
   and `49F9` run. So the guard genuinely applies to any transcript that runs
   the loop even once, and a whole-call transcript at 12,667 cycles against a
   5,068-cycle period is refused every time, exactly as the arithmetic said.

   That is what makes this the useful entry rather than a correction. **The
   guard is a deadline, not a cap**, and the answer to a deadline is to do
   less before it. Chunked at `4044`, the walk stops at the loop's closing
   jump once one more iteration would not fit inside the horizon it was
   handed; the core takes the jump and the interrupt, and the next chunk
   starts against a fresh horizon. 24,667 of the route's 32,391 voice
   iterations now run inside a transcript, and the tick is native at
   `+13,017,489` cycles.

   Two lessons, and the second is the one to carry. Run the two divisions on
   the **chunk**, not the routine. And when the arithmetic says a body cannot
   fit, find out *which* read imposes the deadline before believing any story
   about it: the recorded story named an instruction the loop never executes,
   which is how a correct conclusion sat for four slices on a reason that
   would not have survived being checked.

   `45:4142` also surfaced the first **shared-state** boundary in the port.
   It reads `$F5`/`$F7`/`$F9`, which the timer interrupt handler writes, so a
   transcript computed from one snapshot can act on a stale value. The walk
   declines unless interrupts are provably masked for its whole body. `457A`
   reaches the same state from the other side: its sample trigger at `45:4776`
   *writes* the driver's whole per-channel zero page. Expect more of this as
   the remaining code moves closer to the interrupt handlers, and note that
   phase 3 dissolves it: with native time and interrupts there is no snapshot
   to go stale.

   `45:457A` also found the first **inexpressible** control flow. Its
   `$82`/`$92` command pops four bytes and jumps to `$40EE`, discarding the
   routine's own stack frame; a transcript that must stop in front of an `RTS`
   cannot represent that, so those calls decline. This is a property of the
   hybrid's leaf-transcript contract, not of the code — porting `45:4015`
   would make the same escape ordinary internal control flow, if the tick
   could be ported at all — and it is another thing that disappears with the
   emulator rather than needing a fix in it.
2. **Video upload paths** (33.4M). The palette group is under way. What looked
   like one 20.6M "VCE palette DMA" in the row above is six routines, and they
   have to be separated before any of them can be ported:

   | Routine | Master cycles | State |
   | --- | ---: | --- |
   | `00:EB30` VCE upload | 8,636,040 | **native** on every call, its `TIA` arm split either side of the transfer |
   | `00:E9B6` block interpolator | 6,548,946 | **native** |
   | `00:EBA5` block stager | 2,629,866 | **native** |
   | `00:EAA7` block decrement | 1,640,100 | **native**, in thirteen chunks a call |
   | `00:E93C` sixteen-bit compare | 758,370 | **native** |
   | `00:E94D`, `00:EA77` fade drivers | 140,037 | reachable, not worth a transcript: one call and eight |

   `00:EB30` took the bounded-register move described in item 3, applied to
   the VCE, plus the matching widening of the core's wait-state compensation.
   Its whole-palette arm — 29 calls of 3,599, where a single `TIA` moves all
   1,024 bytes — is now native either side of the transfer: the walk stops in
   front of it, the core executes it, and `00:EB4D` resumes the arm. The
   transfer itself stays emulated and always will; 1,024 writes against a
   512-write channel is a capacity limit, and a block transfer has no interior
   instruction boundary for a chunk to stop at.

   The group is also where transcripts stopped handing their returns back.
   Every walk in it now ends *past* its own `RTS`: the stack is ordinary work
   RAM at `$2100`, so the return address is two bytes the entry already
   carries, and the core never relates a resume address to the entry address.
   `RTS` emits no write, so nothing in any trace was allowed to move and
   nothing did. That is 14,369 returns and 301,749 master cycles here, and it
   retires the largest category that used to be filed as an unremovable
   overhead of the hybrid — 13.3M route-wide at 21 cycles each end of every
   native call. The `JSR` end is the same argument from the caller's side, and
   needs a ported caller, so it goes family by family.

   The palette group is therefore finished for the hybrid at 18.2M of 20.6M.

   The VRAM and VCE upload paths at `00:E3C0`–`E47C` (7.0M) are now native
   too, and they needed a technique rather than a channel widening. The
   generic word-stream decoder at `00:E41F` is 167,000 master cycles a call:
   3,400 port writes and 12,400 instructions against a channel of 512 writes
   and 2,048 checkpoints, so no transcript can hold one call. It **chunks**.
   Each routine gets a second entry point at the head of its loop, and a
   transcript ends in front of the backward branch that closes the loop; the
   core executes that branch, lands on the loop head and calls the replacement
   again. Nothing in the core changed to allow it — the hook already fires on
   any fetch at a registered address, and a replacement already ends by handing
   one instruction back. That removes the routine-length ceiling from the rest
   of phase 1, and it was the answer to `00:EAA7` in the table above as
   well: that row said "205,012 cycles per call, over the checkpoint cap"
   for three slices, and the cap bounds a transcript rather than a routine.
   `00:EAA7` is now native at 1,640,100 of its 1,640,100, chunking at
   `00:EAB0`, the head of its colour loop.

   The tilemap upload at `00:F261`–`F4F1` (5.7M) is native as well, and it
   closes this item: every video path that does not read a device register is
   now ported. It needed no new technique — ten ordinary entry points, six of
   them the return addresses of calls the transcripts stop in front of — but it
   did find a limit of the checkpoint contract. `BBR` is not one instruction in
   the core, it is `LD_ZP` wrapping `JR`, so it samples the interrupt twice and
   crosses in four clock steps where a checkpoint crosses in two. The coarser
   step ran the video `EventHandler` late, which moved a timer interrupt one
   instruction early four times in 3,600 frames — twice in assembly downstream
   of the walk, which is how it became clear the perturbation outlives the
   transcript. `00:F423` therefore goes back to the core, at a cost of 365,664
   cycles. Anything else in the ROM built from two `LastCycle`s has the same
   constraint; `BBR`/`BBS` are the only such opcodes.

   What is left in this item is not CPU work: the two fade drivers wait on the
   frame-sync loop, and `00:EB30`'s whole-palette `TIA` arm overruns the write
   channel. Both are phase-2 or phase-3 problems. The palette group is
   18.5M of 20.6M.
3. **The `00:E0EB`–`E292` region** (15.3M). This was listed as one block, "the
   VBlank handler and pad reader", and the plan for it was to widen the write
   channel to reach the joypad port at `$1000`. Both halves of that were
   wrong, and the correction is the useful part.

   It is not one block. It is an interrupt dispatcher, a VBlank body, a pad
   reader, an asset resolver and three of the game's five bank-switching call
   wrappers, and they have nothing in common but an address range — the same
   mistake the palette row above made. Separated:

   | Routine | Master cycles | State |
   | --- | ---: | --- |
   | `00:E195` pad reader | 6,929,670 | **native** — see below |
   | `00:E241`/`E255` banked call | 2,878,650 | **native** |
   | `00:E111`–`E13F` VBlank body | 2,412,678 | **native** except its `RTI` |
   | `00:E262` trampoline | 953,343 | **native** |
   | `00:E140`–`E178` VBlank epilogue | 821,685 | **native** |
   | `00:E058`–`E0EA` dispatcher | 769,938 | **native** except its `LDA $0000` — 640,374 of it — see below |
   | `00:E0EB`–`E110` VBlank prologue | 544,104 | **native** |
   | `00:E219` bank pair | 501,639 | **native** |
   | `00:E1FD`/`E20D` asset resolver | 377,085 | **native** |

   The widening would not have helped on its own, because the blocker is a
   device **read**, not a write. `00:E195` strobes `$1000` and then reads it
   back five times a frame; `00:E058` opens by reading the VDC status register
   at `$0000`. A replacement callback receives work RAM and registers and
   nothing else, so no allowlist reaches either one.

   This document then drew the wrong general rule from that — "a routine that
   reads a device register cannot be a transcript until that device has a
   native model" — and left the pad reader sitting as the largest unported
   routine on the route for four slices. The rule holds for the callback and
   not for the value. `00:E195` is native now, `+6,013,791`, admitted on every
   call. What the joypad answers is a function of the frame's pad state and of
   the `SEL`/`CLR` latch lines the routine drives itself, so the walk models
   the port rather than reading it; `RoutineEntry` carries the pad state,
   taken from the same field the core answers reads from, and `$1000` joined
   the write channel so the four latch stores leave the port where assembly
   would have left it.

   The corrected rule distinguishes two kinds of read. **A read whose answer
   depends only on outside input and on lines the routine itself set is
   portable now.** A read whose answer depends on state the device evolves on
   its own was recorded as not portable, and `00:E058` was filed under that
   heading for four more slices.

   **That was the same mistake a second time, and the tell was that it named
   a routine.** Asked again after the VDC had a native model, the answer came
   out differently on both halves. The value stopped being the obstacle the
   day `PceVdc::read` gained its `peek` flag — the entry could have carried
   the byte the way it carries the pad state. What actually blocks the
   instruction is that reading `$0000` *is* the interrupt acknowledge: it
   clears every latched status source and is the only way the VDC's interrupt
   line falls. The replacement contract is a write channel and a write to
   `$0000` selects a register rather than acknowledging one, so the side
   effect has nowhere to go. A transcript that predicted the byte would return
   to a core whose interrupt had never been taken.

   That is a fact about **one instruction**, and the dispatcher was blocked
   only because someone had written the routine's name next to it. Splitting
   at the read makes both sides ordinary: above it, four register saves that
   touch only the stack page; below it, a dispatch on a byte that is by then
   in the accumulator. `00:E058` and `00:E05F` are both native and admitted on
   every one of their 7,198 arrivals, `+640,374`, and the census, the whole
   stack page and every VDC and joypad bus write are byte-identical to
   assembly. The core still executes the `LDA` itself, 129,564 cycles, at its
   own cycle and with its acknowledge intact.

   So the rule to carry is narrower again, and it is about instructions:
   **a device read is transcribable when the walk can compute the value, and
   not when the read changes the device.** The joypad has come off the blocked
   list, and so has the PSG, and so has all but one instruction of this.

   The bank-switching wrappers went instead, together with the two in the
   `00:ECC6` family, and they are done: nine entry points, 52,911 calls,
   4,849,479 cycles, admitted on every call. They carry no game logic — they
   are the game's linkage — and they were the first transcripts in the port to
   change the bank mapping. `TAM` is expressible because `CpuRegisters` carries
   all eight `MPR` slots and a checkpoint applies them.

   The VBlank body followed, and needed no new capability either: six entry
   points around the calls the core keeps, writing the VDC ports the channel
   already carries. 13,932 calls, admitted on every one. The pad reader made
   it seven entry points and 17,531 calls, and was the one addition that did
   need a new capability — the pad state on `RoutineEntry`. The two halves
   made it nine and 31,927, and needed no new capability at all: the
   replacement hook fires at every instruction boundary, so `00:E05F` is a
   legal entry point even though nothing in the game jumps to it.

   It did settle a rule that had only been half-stated. Each walk stops one
   instruction short of its `RTI`, in front of the `PLP` that restores the
   interrupted status byte, because the core samples the IRQ at `LastCycle` —
   which for `PLP` runs before the pop — while a checkpoint carries one register
   set and samples with it. A transcript covering `PLP` samples with the
   restored mask instead of the one the instruction ran under, and swallows an
   interrupt assembly services between the `PLP` and the `RTI`. The sound driver
   hit the same fault from the other direction with `CLI`. **An instruction that
   changes `I` cannot be summarised by a checkpoint that publishes its result.**

   That is not a theoretical concern here: the handler is entered with `I` set
   but does not stay masked, because bank `45` ends three critical sections with
   an unconditional `CLI` instead of restoring the caller's mask.

   What is left in the region is the dispatcher, which is phase 2, plus the
   two `RTI`s and the three `JSR`s the walks stop at — 1.2M of instructions
   the core executes by design.
4. **The collision frame** (8.4M) — **done**. `01:CA84`–`CDF7` is the code
   around the overlap scanner `01:CC1A`, which has been native since the
   twenty-sixth replacement: the playfield-tile probe `01:CD74` (2.6M), the
   ball/table gate `01:CA84`, the sweep setup and tail around `01:CB48`, the
   single-slot probe `01:CBAB` and the contact dispatcher `01:CBCF`. Nineteen
   entry points, admitted on every call, `+3,232,335` net.

   Nothing here needed a new technique. `01:CD74` is a pure leaf — no calls at
   all — and it is the largest single piece of logic phase 1 has ported in one
   entry point. The contact dispatcher ends in the `PHA`/`PHA`/`RTS` tail jump
   the collision boundary route documents, which turned out to be ordinary for
   a transcript: the pushes are transcribed and the core executes the `RTS`,
   which jumps exactly where it would have.

   What it did settle is what the leaf-transcript contract costs. Eleven of the
   nineteen entry points exist only because a transcript stops in front of a
   call, and the `JSR`/`RTS` pair at `01:CB5E`/`01:CC40` was 3.6M cycles on its
   own — the first place where the call tax was larger than the logic it
   surrounds. The slice below spends it.

   Shared state was measured the way the workflow now prescribes, on exact
   windows from observers rather than inferred from writes, and the result was
   unusually clean: the eighteen arithmetic instructions at `00:E56E`–`00:E847`
   that write this family's zero page land inside a window on 99.3%–100% of
   their executions — the signature of a callee — and every one of the three
   hundred other instructions that writes the same addresses anywhere on the
   route lands inside a window on exactly 0.00% of them.
5. **The sweep's call into the scanner** (3.5M) — **done**. `01:CB48` no longer
   stops in front of `JSR $CC1A`. It transcribes the call, splices the
   scanner's own transcript in where the core would have run it, transcribes
   the `RTS`, and hands back at `01:CB61` with the scan answered — the pattern
   `01:CC1A` already used one level down for `01:CCD5`. 83,905 of the scanner's
   85,715 calls compose; the 1,810 that do not are the hit path, which leaves
   the scanner from the middle of itself and has no `RTS` to transcribe, so
   `01:CC1A` stays installed and the walk rolls its attempt back.

   The gross gain is exactly the tax — 83,905 × 42 = 3,524,010 — and the bill
   is 695,061, 19.7% of it, almost all of it the sound driver losing 1,184
   admitted entries to a transcript that is now longer. Net `+2,828,949`.
   `01:CB48` is now the largest single native address on the route at 7.96% of
   all master cycles.
6. **The Sorcerer tick** (4.1M) — **done**. `02:586F`–`5903` was the largest
   block of unported gameplay left on the route and the only block of its size
   with no native entry inside it: the staggered deployment delay, the arrival
   state that reseeds the collision-table cursors, and orbit, where all eight
   Sorcerers sit for the rest of the route. Five entry points, admitted on
   every call, 3,156,294 gross and `+2,595,360` net.

   Its screening was the cleanest the in-window ranking has produced. Of the
   ninety-nine instructions that write any address the family reads, exactly
   one ever writes inside one of its call windows, and that one is its own
   callee. The other ninety-eight are at 0.0%.
7. **Three bank-01 arithmetic leaves** (3.3M) — **done**. `01:C8F3`, the
   path-length accumulator; `01:DAF8`, the direction-octant classifier; and
   `01:C969`, the range-squared guard whose body this route never runs. Not one
   routine and not one subsystem — they went in as a single walk because they
   share a shape: straight-line integer arithmetic over work RAM and zero page,
   ending in an `RTS`, with no call, no device register and no bank change.
   3,066,105 gross and `+2,439,807` net, admitted on every call, and no foreign
   writer in any call window.

   `01:DAF8` had been screened and rejected twice, because its cycle, write and
   result populations were mixed rather than exposing a stable state-derived
   return. That was correct for the method in use then, which had to predict a
   return value from the entry state. A transcript predicts nothing, so a mixed
   population is just two exits. **Re-read old rejections when the method
   changes**; this one had been standing for several slices.
8. **The collision-table interpreter as a transcript** (4.7M declined, 2.9M
   gross) — **done**. `03:6966` was already replaced and already correct; what
   it lacked was an instruction-boundary transcript on every path, so 2,850 of
   its 34,259 entries were refused at an interrupt horizon and neither
   composition site could take it. Rewriting it as a walk removed every horizon
   decline on the route — 3,599 to zero — and gave the consumer at `03:7F57`
   749 calls it had been written to accept and could not.

   It sat unstarted for several slices because the existing replacement is
   about 2,200 lines and rewriting it looked like the price. **The size of a
   replacement is not the size of the routine.** `03:6966` is 0xD2 bytes and
   about a hundred instructions; the 2,200 lines were what it cost to predict
   the result from the entry state, which is what the older method required. A
   transcript predicts nothing, so the walk came out shorter than the code it
   fronts. Check the routine before pricing the rewrite.
9. **The interpreter composed into the Sorcerer tick** (1.5M gross) — **done**.
   The slice above made composing possible; this is the composing. Both
   `JSR $6966` sites in the tick are run in line: push the return address, hand
   the staged work RAM to the interpreter's own walk, splice its checkpoints and
   writes at the cycle the call landed on, transcribe the `RTS`. `+1,295,532`
   net, and the registry passed 41.48%.

   The thing it settled is that **most of the gross is not code that stopped
   being emulated**: only 731,907 of the 1,538,439 is the two `JSR`s and the
   `RTS`. The rest is three replacements becoming one. Composition buys
   boundaries, not instructions, and this is the first slice on which the
   route's total hook entries went *down* — 2,367,401 to 2,332,709. That did
   not make it systematically cheap, though: the bill was 15.8%, low but inside
   the existing spread, and the other composing slice on record billed 19.7%.
10. **The fade-to-black stepper chunked** (1.6M gross) — **done**. `00:EAA7`
   had been set aside twice as past the checkpoint cap however the transcript
   was arranged, which was a true sentence about a transcript and a false one
   about the routine. Chunking it at `00:EAB0` makes eight calls into 101
   transcripts and moves all but 1,284 of its 1,640,100 cycles native. The
   registry passed 41.61%.

   Two things came out of it worth more than the cycles. The **shadowing bill
   was 1,224 cycles**, against a rule of thumb that predicted 314,000, because
   the bill is the cost of displacing other native routines and the stepper
   runs during screen fades when nothing else is running — so idle-time work
   is nearly free to port and does not belong in the same distribution as the
   other eight measurements. And the **census over-credits truncated
   transcripts**: it adds a transcript's declared body when the replacement is
   offered, before the core discovers an interrupt will cut it short, which is
   why the registry line moved 1,651,296 on a routine that costs 1,640,100.
   Halving the chunk budget lowers the reported total by 9,231 without
   changing a byte of the state census. Every native share in these documents
   is high by some small amount of this kind.
11. **The two withheld routines** (1.1M gross) — **done**, and the value is
   not the cycles. `02:57E9` and `03:6054` each passed an `@all` regression but
   moved the state census, and each one's own regression asserted the moved
   hash, so `kWithheldRoutines` held both out of every composed deployment.
   Both were replacement faults, not properties of the routines, and both were
   the same shape: a boundary charged the wrong number of master cycles with
   the error taken back off a later instruction, so the body total stayed
   right. `03:6054` charged `ADC $26` 9 instead of 12; `02:57E9` charged
   `CMP $3980,X` 18 instead of 15 and also published the caller's N and Z at
   the boundary after `LDA $2D40`.

   **The withheld list is now empty and the deployed registry is 362 of 362.**
   That is what the number is for: 42.69% is now the whole registry against the
   assembly census rather than all but two of it. Gross 1,143,930, bill 198,927
   (17.4%), net `+945,003`, 42.62% → **42.69%**.

   The general lesson is about what each artifact can see. A correct body total
   is why a state census could tell that something was wrong and never where —
   the routine ends at the right cycle, so only the inside is misplaced. And
   `02:57E9` writes nothing at all, so no write trace could have found its flag
   fault; it exists only as the status an interrupt stacks.
12. **The PSG register programmer `45:4142`** (4.1M gross) — **done**. It had
   sat at 1,792 of 3,604 arrivals for four slices under the driver-state guard,
   on the arithmetic that a whole call averages 2,055 master cycles against a
   5,068-cycle timer period. That is the same argument `45:4015` was rejected
   on one level up, and it fails the same way: `4142` is a loop, `4143` is its
   head, `41F7` closes it with a `JMP $4143`, and it chunks exactly as the tick
   does. 15,972 of the route's 19,062 channel iterations now run inside a
   transcript. Net `+2,271,558`, 42.69% → **42.87%**, with the census, the
   5,458,568-write work-RAM trace and the 699,251-write PSG trace all still
   byte-identical to assembly.

   The lesson is about the shape of the rejection rather than the routine.
   Twice now a "this cannot be ported" note has been an arithmetic comparison
   between a whole body and the timer period, and twice the body was a loop. A
   ratio of two rather than three is not a reason to leave the second one
   alone; it is the reason it declined half the time instead of always.
13. **The pad reader `00:E195`** (6.9M gross, `+6,013,791` net) — **done**. The
   largest genuinely unported routine on the route, and it was blocked by a
   rule this document had over-generalised rather than by anything about the
   routine. See item 3 above for the corrected rule.
14. **Nested replacements** (85,778,658 net, 43.33% → 49.97%) — **done**, and
   this document argued against doing it. The core gated the replacement hook
   on `!ReplacementActive`, so a transcript suspended at a checkpoint hid the
   interrupt handler that preempted it: the sound driver reached 113,164 of
   254,990 arrivals and the VDC handler chain missed two frames in three. The
   argument for leaving it alone was that a replacement-frame stack is
   emulator scaffolding that ports nothing and is deleted in phase 4. Both
   halves of that are true and it was still the wrong call. Suspension is
   machine state, not a lock — the handler is 65C02 code like any other, and
   the interrupted transcript is ordinary saved state while its own transcript
   runs. Leaving it unfixed also taxed every future slice: a longer transcript
   shadowed more of the driver, so each port gave part of its gain back (see
   the collision-sweep and Sorcerer notes above). `patches/0007` is 256 lines,
   the census is unchanged at `9036a9de…`, and the measured depth is 3 of a
   permitted 4. What it bought beyond the cycles was visibility: the driver's
   epilogue had been indistinguishable from blocked arrivals.
15. **The sound driver's epilogue `00:FC7C`–`FC8C`** (25,180,701 net, 49.97%
   → 51.92%) — **done**. Nine instructions, and the largest remaining target
   by an order of magnitude once nesting made it visible. The blocker was
   never length: both copies of the epilogue end by reading `$DA`, the rerun
   request `00:E12A` writes from inside the video handler that preempts this
   one, so the value the branch turns on can arrive *after* the snapshot a
   transcript would have to predict it from. The chain exit is taken 387 times
   on this route, so guessing it was not an option. The fix is to make the
   epilogue two transcripts and re-enter the walk for the second, which is the
   `00:EAB0` chunking pattern used against staleness rather than length.
   *Where* they meet is the finding, and it cost a day: the obvious split is
   at the read, and it is wrong, because the `SEI` two instructions earlier
   is an instruction the checkpoint format cannot express. A checkpoint
   carries one status byte and the core derives both the `P` an interrupt
   pushes and the `IFlagSample` its timer dispatch tests from it; `SEI` is the
   one instruction where those must differ, since it retires with `I` set but
   sampled the mask before setting it. Publishing the open mask corrupts the
   pushed `P` — the census caught it in frame 951, with `PC=$FC82, P=$02` on
   the stack, an `I` clear that the `RTI` would have restored. Publishing the
   true `P` shuts a boundary the hardware leaves open. So the `SEI` stays
   emulated (1,526,046, 5.7% of the slice) and the transcripts meet either
   side of it, entering at `00:FC82` and `00:FD76`. This is the second such
   instruction after `BBR` at `00:F423`; both were found by the census, and in
   both cases the fix was to hand the instruction back rather than widen a
   format that phase 4 deletes. The `RTI`s at `00:FC8D` and `00:FD81` are left
   (5,349,879) — returning from an interrupt is the frame boundary's work,
   item 17 — as are the two `JMP $FC03` chain exits (4,644). The driver is now
   the largest native address on the route at 12.25% of all master cycles, and
   this slice took everything outside the idle spin loop past 94%.
16. **Gameplay tails** (32.2M, 2.49% of the machine, of which 6.9M is the
   driver residue item 15 leaves deliberately). Once entry points, boundary
   instructions and interrupt resumption are separated out —
   see `docs/NATIVE_PORT_WORKFLOW.md` — nothing genuinely unported is above
   1.6M. In order: `08:41AB` (1.56M — since absorbed into the sprite
   emitter's own transcript, leaving 987), `01:C700`–`C77F` (1.22M),
   `08:4080`–`40FF` (1.20M), `01:DC80`–`DCFF` (950K), `00:E200`–`E27F` (894K),
   `03:6000`–`607F` (857K), `01:C100`–`C17F` (839K), `00:E000`–`E07F` (775K).
   `02:5900`–`597F` belongs on that list too and no longer does: `02:5903`,
   the Sorcerer tick's own return, was 384K and is now 8,547. `03:6980`–`6A37`
   went the same way for the same reason and did not fall as far: the
   collision-table interpreter's two returns were 442K and are now 176K,
   because it is long enough that an interrupt cuts one call in eight short of
   the return it absorbed.
   That last one is the interrupt dispatcher, and it has since been split at
   its status read: 640,374 of it is native and 129,564 is the `LDA $0000`,
   whose read is the interrupt acknowledge.
   Three entries that used to head this list — `08:40DE`–`41AB` with
   `08:4085`–`4094` at "2.8M", `01:C5C8`–`C770` at "1.6M", `03:6001`–`6053` at
   "883K" — were boundary instructions and resumption around routines that
   were already native.
17. **Boot and reset path.** Off the census route's hot path but required.
18. **The frame-sync spin loop** (584.8M). Trivial code, but it is where the
   native frame boundary ends up living — do it with phase 3, not before.

### Phase 2 — native device models

- **Joypad.** **Done**, in two steps that turned out to be different kinds of
  work. `00:E195` models the strobe/select protocol inside its own walk against
  the `InputFrame` the host supplies to `run_frame`, carried on `RoutineEntry`;
  the transcript diffs clean against assembly in the pad bytes and in the
  `$1000` writes themselves, and it settled 6.9M of phase 1. That was the
  protocol. The port itself — two strobe lines, a multitap counter, five
  devices — was still the emulator's, and reading through
  `current_input_` was still reading through the emulator's chain.

  [`src/input/pce_pad.cpp`](../src/input/pce_pad.cpp) is that port, installed
  through patch `0019`. It is the first seam that moves no computation: the
  arithmetic was already native, and what changes is who the input belongs to.
  The host hands its `InputFrame` to the model directly and the model holds it
  until the machine's own frame latch, so *when* a device sees a press is still
  the machine's decision.

  **The acceptance is stronger than the census for once, and deliberately.**
  For the sound and video models a silent fallback would have produced the same
  census as the model — which is why those tests had to prove the model was the
  default arm. Here the host starves the alternative: while a pad model is
  installed the libretro input callback answers zero for every query, so the
  emulator's own path is dead rather than merely bypassed. The route reproduces
  `9036a9de…` anyway, with the model reporting 540 of its 35,991 reads
  answering with a button held. Feeding the model nothing instead — the same
  binary, one line changed — produces `24b1614f…`, a route where the game never
  starts. That is what makes the match evidence rather than a coincidence.

  What the route does *not* reach is measured rather than assumed, on the PSG's
  pattern. `--report-pad-coverage` says: every read went through the multitap
  and none straight to the first port; 17,996 answered the button half and
  17,995 the direction half; the counter was reset 3,599 times and stepped
  17,995; the half-selector toggled 17,995 times and the six-button read path
  it selects ran **zero** times, because a two-button host never sets the bit
  that reaches it; and neither filter — the soft-reset suppression or the
  opposing-directions drop — removed anything, because the route never asks for
  either. The six-button path, a mouse, and the Tsushin keyboard are the port's
  remaining unexercised surface, and the last two are counted as unmodelled
  rather than answered.
- **PSG.** **Done.** [`src/audio/pce_psg.cpp`](../src/audio/pce_psg.cpp)
  accepts `$0800`–`$0809`, models the six channels, direct digital audio,
  the LFO and the noise LFSR, and drives the two band-limited delta buffers.
  It is the device only — it owns no CPU state and reaches nothing outside
  itself. Installed through the fork's C boundary by patch `0008`, so the
  emulator's PSG receives no writes, no time and no reset while it is
  running; the emulator keeps the object solely as the shape its savestate
  format is written through.

  Acceptance was the census unchanged, `9036a9de…`, over all 3,600 frames,
  and it also holds at the device output itself: a temporary harness ran both
  PSGs side by side from the same starting buffers and compared every one of
  the ~60,000 buffer positions per frame per channel, finding zero
  differences across the route. `tests/verify_native_psg.cmake` pins the
  census in both arms.

  What that proof does **not** cover was measured rather than guessed. The
  model counts its own paths, so `--report-psg-coverage` reproduces this from
  any recorded route:

  | Path | Emits on the census route | Covered by the census |
  | --- | --- | --- |
  | Waveform / direct digital audio (`Normal`) | 7,048,794, all six channels | Thoroughly |
  | Silent channel (`Off`) | 470,150, all six channels | Thoroughly |
  | Noise LFSR | 824,410, **channel 5 only** | Channel 5 only; channel 4 never enters noise mode |
  | Accumulating output | **7**, across channels 0, 3 and 5 | Effectively not at all |
  | LFO | **0** updates, **0** recursions | Not at all |

  The LFO result is not an accident of this route. The sequencer at `45:4142`
  writes `$0809` from `$2E41`, and every write in the ROM that names that byte
  writes a zero to it: the cold start's `STZ $2E41` at `45:4AB19`, and the
  sequencer's own `STA $2E41` at `45:4A20A`, which stores the value it just
  masked its high bit out of. Nothing gives the byte a value. That was checked
  across every absolute and absolute-indexed store in the cartridge whose
  target can reach `$2E40`/`$2E41`; a bulk block transfer over work RAM is not
  excluded by that search, but the sequencer reloads the register from the
  byte every frame and the route shows `$00` on all 3,605 of them. **So a
  censused route that plays looping music — the plan's answer here — would
  report the same zero.**

  What settles them is
  [`tests/psg_device_tests.cpp`](../tests/psg_device_tests.cpp), which drives
  this model and the emulator's own `PCE_PSG` from the same synthetic register
  stream and compares both the output buffers and the entire device state
  after each scenario. The emulator's PSG needs nothing from the rest of the
  machine — buffers and a revision to construct, a timestamp per call — so it
  is reachable as a bare reference device through
  `dc_beetle_reference_psg.*` in the fork. Nine scenarios plus three seeded
  random streams, each run against both chip revisions, cover the LFO at all
  three depths and both of the ways the mixer turns it back off, noise on
  channels 4 and 5 across all 32 periods, the accumulating mode on all six
  channels either side of the cutoff, the volume walk, direct digital audio,
  power, and the frame rebase. The test asserts the path counters it needed
  are non-zero, so a scenario that stopped reaching a path fails rather than
  passing quietly.

  That test is worth exactly what it can catch, so it was checked the same way
  the device was: perturbing the LFO shift by one fails the oscillator
  scenario and all three random streams, and perturbing the accumulating
  mode's midpoint by one fails the accumulating scenario on the HuC6280A and
  only there — which is the revision that subtracts it.

  This is also the shape of the answer for the VDC. Where the game exercises a
  device, the census proves it; where the game does not, the emulator itself
  is the oracle, for as long as it is still here.

  The scope shrank once the boundary was drawn in the right place. The plan
  said this was the larger of the two devices because Beetle's PSG "includes
  resampling to 44.1 kHz that has to be matched bit-for-bit" — but the
  resampler is not part of the PC Engine. The chip produces deltas at the
  master clock divided by twelve; turning those into 44.1 kHz is something a
  host does, and Mednafen's `OwlResampler` is one host's answer with its own
  SIMD-dependent float arithmetic. Modelling it as part of the machine would
  have meant porting 770 lines of someone else's signal processing to
  reproduce a number that is not a property of the hardware. The device stops
  at the delta buffers; the resampler stays borrowed until phase 4 deletes
  the emulator, and `audio_sha256` is re-baselined once, deliberately, at
  that point.
- **VDC.** Accept the VDC register file, maintain VRAM, the post-DMA SAT,
  palette and scroll, and produce a `VideoMemorySnapshot`. Acceptance:
  `framebuffer_sha256`. Much of this is already done: `src/render/` decodes a
  snapshot into pixels natively today. What is missing is the state machine
  that *maintains* the snapshot in response to port writes, plus the raster
  timing that decides when each line is sampled.

  The reference device this needs already exists, and building it first
  corrected the plan. This section assumed the VDC could not be lifted out the
  way the PSG was, because the game reads it back — `00:E058` opens by reading
  the status register — and that a reference device would therefore need the
  machine around it. It does not. `vdc.cpp` touches no emulator global; the
  class owns its VRAM, sprite attribute table, register file and raster phase;
  and its only outward edges are two function-pointer hooks. Read-back is a
  method on the device, so two devices can be asked the same question and
  compared. `dc_beetle_reference_vdc.*` is the same shim `PCE_PSG` got, and
  `tests/vdc_reference_device_tests.cpp` drives a bare device through a field.

  What the VDC does need, and the PSG did not, is a **clock schedule**. `VCE`
  decides how `run`, `hsync` and `vsync` interleave, and the picture depends on
  it: the same register writes that draw 57,347 pixels on the 5 MHz dot clock
  draw 76,413 on the 7 MHz one. That schedule is the substance of the slice,
  more than the pixel decoding is, and the test's `Driver` is a working copy of
  it to build against.

  It also has one genuine coupling back to the machine, where the PSG had
  none: the device stalls the CPU by looping until its wait-state hook
  *refuses*, so the refusal is load-bearing rather than advisory. The
  emulator's escape is a per-frame budget of 455 × 64 stolen cycles, after
  which the device completes the blocking transfer itself. A native model owes
  the same answer, because the CPU's timing depends on it — which makes this a
  phase 3 obligation as much as a phase 2 one.

  **The model is built.** `src/video/pce_vdc.cpp` is a HuC6270: the register
  file, 64K words of video memory with its character cache, the horizontal and
  vertical phase machines, both transfer engines, the background and sprite
  renderers, the interrupt sources, and the stall. It reaches nothing outside
  itself, and unlike the emulator's device its two hooks carry a context
  pointer, so more than one may exist at a time.
  `tests/vdc_device_tests.cpp` drives it and the reference device side by side
  on one copy of `VCE::SyncSub`, comparing every pixel, every register, every
  word of video memory and the sprite attribute table, the interrupt line and
  its hook traffic, and the stall count — over 29 fields, 2.6 million pixels,
  reaching sprite overflow, sprite-zero collision, 16-colour sprites, both
  transfer directions, all four address increments, and a stall that exhausts
  its budget. None of that stimulus comes from the ROM, which is the point:
  the census can only prove what the game asks for.

  **The model is installed.** Patch `0009` routes every entry point `VCE` has
  into `vdc[0]` — reset, the three clocking calls, both port directions, the
  interrupt poll, the register read the encoder does for its own bookkeeping,
  the two debugging controls, and the two peeks the frame snapshot uses. The
  emulator's device receives nothing and survives only as the shape its
  savestate format is written through. Three arms produce byte-identical
  3,600-frame censuses — both models installed, the video device handed back,
  and both handed back — and the third is what earns the other two, because
  without it a matching hash would prove only that the two models agree with
  each other.

  The liveness caveat was answered from the model's own side, and the answer
  is the most useful measurement in the slice. **The route asks for a small
  fraction of the chip.** Over 3,600 frames: zero 16-colour sprite fetches,
  zero sprite overflows, zero sprite-zero collisions, zero words moved by the
  video-memory transfer engine — the game moves its sprite attribute table
  3,599 times and never copies within video memory at all — zero forced
  completions out of 11,008 stalls, and twenty words read back out of video
  memory against 974,757 written. Every one of those is a path
  `dc_vdc_device` drives synthetically against the emulator's own device. The
  two tests are not redundant and neither is sufficient: the census proves the
  model is right about this game, the differential test proves it is right
  about the chip. This is the split this section predicted, now measured.

- **Colour encoder (`VCE`) — done.** `devils_crush::video::PceVce` is the
  dot-clock divider, the two blanking counters, the 512-entry palette with its
  cached expansion, the scanline bookkeeping, and the schedule that drives the
  controller and writes the picture into a framebuffer. It is installed behind
  patch `0011`, which routes `PCE_Power`, the `$0400`-`$0405` register file,
  the three controller-facing entry points, `RunPartial`, `StartFrame`, the
  frame boundary and the savestate through
  `dc_beetle_active_vce_model`.

  **This is the first device that owns time rather than answering it.** The
  emulator's processor now runs until the model says to stop: `Sync` is the
  entire seam, and behind it the master clock divides, the blanking counters
  flip, the scanline advances and the picture is drawn. It is also the first
  that draws — the frontend's framebuffer, pitch and line-width array cross
  the boundary every frame, guarded by a `static_assert` that `bpp_t` is still
  sixteen bits wide.

  `dc_rom_native_vce` runs four arms over the 3,600-frame route and all four
  reproduce `9036a9de…`, the hash the sound slice pinned: both models
  installed, the controller handed back, the encoder handed back, and all
  three device models handed back. The census carries a framebuffer hash per
  frame, so a single wrong pixel on a single frame would fail it. The route
  drew 294,840,000 pixels through the model.

  The question this section left open — `VCE` owns its controllers by value,
  so a native encoder and a native controller have to be joined to each other
  — has a smaller answer than expected. `VdcDevice` is eight methods, exactly
  the ones `VCE` calls, with a wrapper putting the native controller behind
  it. The interface exists so the encoder can be tested and *installed*
  without the native controller, which matters more than the joining does: a
  device that can only be installed in a pair can only be exonerated in a
  pair.

  Which controller the encoder clocks is decided in one place, the fork's own
  `DcVDC_*` dispatch, exported as `dc_beetle_vce_vdc_*`. The host's bridge is
  stateless and always forwards there, so all four combinations work without
  anyone enumerating them and the two models may be installed in either order.
  The controller's two edges back — its stall and its interrupt — gained the
  same dispatch rather than a second pair, because a controller must not have
  to know which encoder is clocking it.

  The encoder could not be lifted out the way the controller was. `VDC`
  touches no emulator global; `VCE` reaches the processor by name to steal
  cycles, drive an interrupt line, request an early stop, and take its
  timestamps. So `dc_vce_device`'s reference arm is not a bare device but the
  smallest machine one runs in — the emulator's own `HuC6280`, initialised
  and never executed, moved forward by stolen master cycles. Everything the
  encoder asks the processor for is a counter it advances or a flag it sets,
  which is why that works, and it makes the processor's clock the test's
  clock: a stall becomes the two arms' clocks parting company rather than a
  number nobody checks.

  Seventy-five checks over fourteen schedule fields, the next-event horizon,
  the processor clock, the interrupt line, 512 palette entries, 1,024 colour
  cache entries, every pixel of a 2048×264 framebuffer, 263 line widths, and
  the controller inside each encoder. It reaches four things the game never
  asks for: three of the four dot rates, a rate changed mid-field, black and
  white, the 263-line field, and the upscaled pixel path. It also reaches the
  one path the 3,600-frame route never did — a stall refused because the
  frame's budget of 455 × 64 stolen cycles ran out, which is the only thing
  that ends a stall loop.

  Two things are deliberately outside the model, and both stayed behind when
  it was installed. What a nine-bit palette entry looks like on a screen is
  the display's decision, so the model is handed the two expansions ready made
  by `SetPixelFormat`. And frame geometry — where the picture sits in the
  line, the multi-resolution rescale in `EndFrame`, the display rectangle — is
  computed from what the encoder *recorded* rather than by it, so the model
  reports each change of horizontal timing and `write_scanline_info` became
  that report's destination rather than its author. Both stay with the
  frontend until something native owns the display, and both are one function
  wide.

  The savestate shuttle has its own test, `dc_rom_device_state_shuttle`,
  because the census cannot reach it: a recorded route never saves or loads a
  state, so both video models could carry a broken shuttle and every arm would
  still match. A routine replay serializes, restores and re-runs a call over
  eight round trips, and the arms must reproduce the *same* calls. It now
  covers five models — the two video devices, the interrupt controller, the
  instruction boundary and the controller port — and deliberately not the
  scheduler, which writes the emulator's format straight through its own
  storage.

  Installing it broke four ROM tests, and the two faults behind that are worth
  keeping. One was the install's: on a state load the emulator's own loader
  reported a change of horizontal timing to the frontend and so did the model,
  and `write_scanline_info` now returns early when a model is installed. The
  other was upstream's — the frontend's sixteen-entry `scanline_info` sits
  behind an unbounded counter, and ten savestate loads without a field between
  them overrun it. That reproduces with every device model handed back, so the
  overrunning path is the emulator's; the routing patch only moved the statics
  it was landing in, and so turned a silent overwrite into a crash.

### Phase 3 — time and interrupts

- **Timer reload and IRQ generation (`$0C00`/`$0C01`), interrupt mask and
  acknowledge (`$1402`/`$1403`) — native.**
  `devils_crush::cpu::HuC6280Irq` holds the two register files, the three
  request lines behind them, and the divider that turns the master clock into
  the only interrupt this machine generates for itself. Installed by
  `0013-huc6280-irq-device-model.patch`; `dc_rom_native_irq` is what says the
  route did not move.

  This is the first device model that is purely reactive. The sound chip
  answered, the video controller answered, the colour encoder owned the
  schedule; this one has no hooks at all. Its inputs are a line assertion and
  a timestamp to catch up to; its outputs are a masked pending set the
  processor reads and a divider the processor folds into its own horizon.
  Nothing calls out of it.

  It is also the first device that was never an object in the emulator. The
  PSG, the VDC and the VCE are classes; this is a dozen fields inside
  `HuC6280` and four methods the machine's I/O page handler calls from
  outside. So `dc_irq_device`'s reference arm is the processor those fields
  live in, initialised and never executed, driven by stolen master cycles.
  That substitution is exact rather than convenient: the timer's whole
  relationship with time is one phase against the processor's clock, and the
  clock advances whether or not instructions run.

  The device is small enough that the test is closer to an enumeration than
  a sample. All 128 reload values are driven to their underflow and checked
  against the tick they should land on; all eight mask values are written and
  read back inverted; both acknowledge paths are taken; the counter is read
  in the one-clock window where it holds −1. Then a 200,000-step walk over
  every entry point compares every field of both arms after every step —
  209,374 comparisons, no disagreement.

  Stealing cycles runs the processor's own `HappySync` whenever the divider
  comes due, so the emulator catches its timer up at the moments a real
  machine would, sometimes several times inside one call, while the model
  catches up once at the end. Proving those agree is most of what the test is
  for and it is not a formality: the tick carrying a reload is one clock
  short and the tick before it is one clock long, so a catch-up landing
  inside that pair has to come out where one stepping through it does.

  Seven deliberate mutations of the model were each detected, and the one
  that dropped the I/O bus latch from a status read was caught *only* by the
  walk — the named checks all masked it off. That is the answer to the
  question the previous slice raised by writing a shuttle test that passed
  while the defect it was meant to guard was present.

  Three things are deliberately outside the model. The interrupt is not
  *taken* here: which vector wins, when the line is sampled and how the I
  flag gates it are decided at instruction boundaries, and this device has no
  notion of an instruction. That split is what makes the model installable
  now, with the emulator's processor still latching. The I/O bus latch is not
  held here either — reads of both register files merge device bits with the
  last byte the processor drove onto a bus shared with the sound chip and the
  controller port — so it is passed in, which is also what keeps those two
  reads pure. And the reset request is an input like any other, held until
  the processor takes the vector.

  The install answered the one question the seam actually posed. Every other
  device model was reached from the machine's *edge* — the I/O page handler,
  or the event handler the processor calls. This one is read from `ADDCYC`
  and from the top of `RunSub`'s instruction loop, so a vtable call at either
  point would have cost more than the whole device does. The answer is that
  the processor keeps a **read-only mirror** of the three values it reads that
  often — `timer_div`, `IRQlow` and `IRQMask` — refreshed by one `publish`
  call after every dispatch that can move them. Every write still goes through
  the model, and `CalcNextEvent` is left exactly as it was. The other four
  fields have no mirror, which is why handing the device back is only safe
  before a machine that will reach its own power-on.

  The census over the 3,600-frame route reproduces the hash the sound slice
  pinned, in all four arms, including the one where the model runs alone
  against the emulator's own sound and video chips. Over that route the device
  is asked for 8,862,034 catch-ups, 679,115 ticks, 258,549 timer underflows
  and reloads, and 254,990 acknowledges — and for none of its register file
  beyond that: the counter is never read back, the mask is never written or
  read, and the status register is never read. The game opens the mask once by
  taking the reset vector and never narrows it again.
- **The clock and the event scheduler.** `devils_crush::cpu::HuC6280Scheduler`
  holds the master-cycle timestamp every device in this machine measures
  itself against, the two horizons that decide when the processor stops to
  let something else happen, the overclock divider, the rate a cycle is
  priced at, and the loop flag that ends a frame. `dc_scheduler_device`
  proves it against the emulator's own; patch `0015` installs it.

  **This seam is a block of memory, not a call table**, and it is the only
  one of the five that is. Every model before it replaced a device the
  machine reached at its own edge, where a dispatch per operation costs
  nothing. This one holds values the processor reads from inside its
  instruction loop: the clock on every memory write and every replacement
  offer, the near horizon in the one opcode that waits, the run flag in the
  loop condition itself. So the model hands `HuC6280` its register block and
  the processor reads it directly, while every *change* to it is still a call
  on the model. Reads are loads, writes are calls.

  The controller's mirror could not have worked here, and the reason is worth
  keeping. `0013` mirrors three values because the processor only *reads*
  them on its hot path; this device is *written* on its hot path, so a mirror
  would have to be refreshed from inside `ADDCYC` — and worse, the handler an
  advance stops for reads the clock back and re-arms its own horizon while
  that advance is still on the stack, which is exactly where a mirror would
  be stale. Sharing the storage removes the question rather than answering
  it. It also removes two things every other model needed: there is no
  publish, and there is no capture-and-apply pair, because the savestate
  writes the emulator's own format straight through the model's fields.

  The census over the 3,600-frame route reproduces the same hash in all four
  arms — including the one where a native clock runs alone underneath the
  emulator's own sound, video and interrupt controller, which is the
  strongest arm this series has run: every other device in the machine
  measures itself against this clock, so a scheduler wrong by one master
  clock anywhere would move every device relative to every other. Over that
  route the model is advanced 221,493,523 times in processor cycles, moves
  the clock 1,292,378,151 master clocks, reaches the near horizon 8,854,191
  times and finds the user event due for 8,375,409 of them, and changes the
  clock rate five times in an hour of play.

  It is the smaller half of what is left inside `HuC6280`. The other half is
  execution, and the two are worth separating because only one of them is a
  device: a cycle costing three master clocks or twelve, an event handler
  coming due, a frame boundary wrapping the clock — none of that needs to
  know what an instruction is. Taking an interrupt does.

  Three things about it were surprising enough to be worth writing down.
  **Reset and power do not touch it.** Between them those two clear the whole
  processor and the whole interrupt controller and leave every scheduler
  field exactly as it was; only `Init`, run for every ROM the machine loads,
  puts the clock back to zero. So the model has an `init` and no `reset`, and
  that asymmetry is the emulator's rather than a simplification. **The
  horizons are relative and the clock is absolute** — which is why the frame
  boundary can wrap the timestamp to a small remainder without touching
  either horizon, and why a savestate carries the horizons but not the clock.
  And **how long a cycle is belongs here**: `speed` is written by the CSL and
  CSH opcodes, which makes it look like a processor register, but nothing
  reads it except the conversion from cycles to master clocks.

  The differential test compares three things after every operation, and only
  the first is obvious. The state, including the derived shift the rate is
  used through, because a model that held the rate right and shadowed it
  wrong would price every cycle wrongly and be invisible in `speed` alone.
  The *ordered sequence of moments* the user event handler was called at,
  folded into a digest by both arms — a scheduler that ended a long drive
  with the right clock but had called the handler at a different set of
  moments would be wrong in exactly the way a frame of video is sensitive to.
  And the near horizon's choice between two devices. 120,043 comparisons, no
  disagreement.

  The timer underneath the native arm is a `HuC6280Irq` composed through the
  scheduler's hooks, which is the composition a native machine will use and
  is sound to rely on because `dc_irq_device` already proves that model equal
  to the emulator's field for field. Any difference this test sees is
  therefore the scheduler's.

  Two paths the route never takes are enumerated by that test instead.
  `advance_master` is never reached, because the machine's only master-clock
  advances come from the `WAI` opcode this core does not emulate for the PC
  Engine and from `StealMasterCycles`, which nothing in a census route calls.
  And the overclock divider never swallows an advance, because the host runs
  at the machine's own rate of one and the remainder is then always zero.
- **The decision between two instructions — native.**
  `devils_crush::cpu::HuC6280InterruptBoundary` holds the two values latched
  at the end of an instruction and the rule that decides, at the start of the
  next one, whether the next thing to happen is an instruction or an
  interrupt. Patch `0016` lifts both out of `LastCycle` and `RunSub` into
  named entry points the emulator itself calls and `0017` routes those to the
  model; `dc_interrupt_boundary` is what proves the model equal to them and
  `dc_rom_native_boundary` is what says the route did not move.

  **This one is a rule, not a device, and the difference shows up in how it
  is proved.** Every model before it was checked by driving a stimulus and
  comparing what came out, and every one of them carried the same unstated
  weakness: a stimulus reaches what it reaches. This device's inputs can be
  counted — six bits of request register, three of mask, one I flag, and two
  latched values — so the test enumerates them instead. Every latch the
  machine can take, 1,024 of them, and every decision the boundary can be
  left able to make, 65,536 of them. No disagreement, and no argument
  afterwards about whether the stimulus was representative.

  Its seam is a third answer to a question that has now had two. The
  controller is read on the hot path, so it became a mirror; the scheduler is
  written on the hot path, so it became a block of memory. This is neither:
  it is a *call* whose inputs are five values the processor already holds and
  whose output is a small enumeration. Recognising that as a third answer
  rather than forcing it into one of the first two is most of the design.

  **The call was measured rather than assumed, and it costs about a sixth of
  the route.** Over the census the boundary latches 108,763,180 times and
  decides 108,732,259 times, so installing it adds roughly 217 million
  indirect calls; a 3,600-frame recording takes 6.4 s with the model
  installed against 5.4 s with it handed back. The alternative was to keep
  the emulator's own `(IRQSample | IRQlow)` gate inline and dispatch only
  when it opens — but that gate *is* the rule's first clause, so buying it
  back would mean a second copy of the rule in the fork, which is precisely
  what `0016` was shaped to prevent. It would also only remove one of the two
  calls, since the latch is taken unconditionally. A route that still runs at
  nine times real time is not worth that trade, and the cost is transitional
  in any case: it exists only while the processor on the other side of the
  call is the emulator's.

  What the route says about the rule is narrower than the enumeration but
  points at the same asymmetry. Of 254,990 timer vectors, **63,135 were taken
  on a line the latch did not hold** — so the difference between testing the
  live request register and testing the sample is not a curiosity of the
  emulator's source, it is a quarter of the timer interrupts in an hour of
  this game. IRQ1 is taken 7,198 times, IRQ2 never: it belongs to the compact
  disc unit, which this cartridge does not have. The reset line is signalled
  twice — power-on and the ROM load behind it — and acknowledged once.

  Three asymmetries in the rule are the reason it was worth separating from
  the processor at all, and each is a thing a from-scratch 6280 would
  plausibly get wrong. **The reset line is never sampled** — the mask is
  three bits wide, so `requests & mask` cannot produce it however the lines
  are driven; only `Reset` puts it in the latch and only taking the vector
  removes it, which makes a reset a state the boundary is left in rather than
  an interrupt that won. **The timer line is tested live where IRQ1 and IRQ2
  are tested latched**, so a timer request raised after the latch is taken at
  once while an IRQ1 raised in the same window waits for the next boundary —
  and, read backwards, a latched timer whose line has since gone away is not
  taken at all. **The I flag is consulted twice, in two different tenses**: it
  closes the latch when set, and then gates the timer branch again through
  the value latched rather than the current one.

  What the model does not do is take the vector. Eight cycles, two dummy
  reads, three pushes, a flag update and a two-byte fetch are the processor's
  work against the processor's own stack and memory map, and they stay in
  `RunSub`. That split is what lets the rule be proved exhaustively instead
  of differentially: the claim is about forty lines of decision, not about a
  processor.

  Its savestate is a capture-and-apply pair rather than the scheduler's
  written-through block, because the two latched values are the model's own
  storage and the format is the emulator's. `StateAction` is the one caller
  that reaches the latch without an instruction having ended — a state load
  *restores* a boundary, it does not take one — and the apply runs after
  `REDOPIMCACHE`, which repairs the processor's copy of the I flag rather
  than the boundary's latched one. That shuttle is now the fourth arm of
  `dc_rom_device_state_shuttle`.
- **VDC/VBlank interrupt timing against the raster — native, and this
  bullet had outlived itself.** `PceVdc::do_vblank_irq_test` runs off
  `need_vblank_irq_test_`, set when the vertical phase leaves the display
  window and consumed at the same point in the raster the emulator consumes
  it at; the encoder's schedule is what drives it, and that encoder is
  `PceVce`. Nothing was left to port here after `0009` and `0011` landed —
  the item survived because it was written before them.
- Cycle pacing. Native transcripts currently declare `body_master_cycles` so
  Beetle can schedule around them; something must still model time to the
  same precision, because the game's behaviour depends on where the timer
  interrupt lands relative to the frame. The scheduler above is now that
  model — what remains is that the transcripts still declare their cost to it
  rather than being executed by it.
- **The spin loop becomes the native frame boundary.** Read, measured, and
  now the only structural item phase 3 has left. The loop is
  `00:E321`–`E32B`, five instructions, of which two are the loop:

  ```
  E321  LDA <$4B      ; the frame number the previous wait consumed
  E323  CMP <$00      ; $00 is the counter 00:E117 increments in VBlank
  E325  BEQ $E323     ; spin until they differ
  E327  LDA <$00
  E329  STA <$4B      ; swallow however many ticks arrived
  E32B  RTS
  ```

  It waits on one byte of work RAM, `$0000`. Traced across all 3,567 waits
  on the census route, that byte was written 3,455 times and always by one
  instruction — `00:E117`, the `INC $00` in the VBlank body — and `$004B`
  3,567 times and always by the `STA <$4B` above; nothing else reached
  either. The loop body reads `$00` and writes nothing. So the exit
  condition cannot change except through an interrupt, which is the premise
  everything below rests on — established for this route, and something a
  fast-forward would have to establish in general, since an indexed store
  could reach zero page from anywhere.

  The cost is the largest single number in the profile: `00:E323` and
  `00:E325` execute **24,248,940 times each** over 3,600 frames and account
  for **584,249,394 master cycles, 45.2% of the route**. An iteration is 24
  master cycles: both instructions are four CPU cycles and the processor is
  in fast mode, three master cycles to the cycle. The profile runs about
  0.4% over that, and the excess is accounted for rather than noise —
  **95,646 interrupts are taken inside the loop** over the route, and the
  6280 charges its eight-cycle entry sequence to whichever instruction the
  boundary fell after. So the wait is quantised to 24 cycles everywhere
  except at the boundaries a fast-forward would stop at anyway.

  What it waits *for* is not one frame. `CMP`/`BEQ` exits on *difference*,
  not on a tick, and `$4B` is then set to whatever `$00` has reached, so a
  wait that started late swallows every tick that arrived. Of 3,566 waits,
  3,557 consumed one tick, six consumed two, and one each consumed three,
  four and seventeen; 109 of the 3,567 calls exited without spinning at all
  because the previous frame's work had already overrun. The game drops
  frames on this route, rarely, and the loop is how.

  It cannot be a routine replacement. A transcript declares its cost when
  it is offered, and this one's cost is not known then — it depends on
  where the next VBlank IRQ lands, which depends on the raster. The shape
  that does work is available now that the scheduler and the boundary are
  both native: at `E323` with `$00 == $4B`, nothing can change before the
  next scheduled event, so the clock can advance by whole iterations to the
  scheduler's horizon instead of executing them. The horizon is an hblank
  or a vblank, a few hundred per frame against 6,798 iterations, and the
  phase matters — a fast-forward has to leave the processor about to
  execute the same one of the two instructions it would have been about to
  execute.

  **Built, and it reproduces the census.** Patch `0018` is the seam: a
  `dc_beetle_idle_model` asked at the watched address, answering a
  per-iteration cost, with the fork deciding how many whole iterations may
  be advanced through. The predicate is native and self-verifying: it reads
  the four bytes at the address every time it is asked (`C5 zz` / `F0 FC`,
  a compare against zero page and a branch that lands back on it), takes
  the zero-page address out of the verified operand rather than being told
  it, and answers "idle" only while the accumulator equals what that
  operand names. So a build pointed at the wrong address, a bank switch, or
  a ROM differing by one byte answers "not idle" instead of skipping time
  it should not.

  Over the census route the model refuses **nothing** across 3,171,424
  arrivals, recognises exactly the **3,567** waits this document already
  counted, and skips **21,077,517 of the 24,248,940** iterations — 86.9%,
  nine per skip on average and 41 at most. The route reproduces
  `9036a9de…` with the capability installed and with it handed back, in
  Debug and in Release, with the coverage identical to the unit in both —
  so what is skipped is a property of the machine rather than of a build.

  The instruction boundary's own coverage is the sharpest evidence that
  nothing but time was removed, because it counts the machine's interrupt
  behaviour directly. Every counter that describes what the boundary
  *decided* is identical either way — 1 reset, 254,990 timer vectors,
  63,135 of them on a line the latch did not hold, 7,198 IRQ1, 199,168
  samples that latched a line, 7,213,928 taken behind the I flag, and
  2,008,130 decisions woken and declined. The only counters that move are
  the raw per-instruction totals, and they move by exactly the right
  amount: 108,732,259 decisions become 66,577,225, a difference of
  42,155,034, which is two instructions for each of the 21,077,517
  iterations skipped and not one more. That is also why
  `verify_native_boundary.cmake` now floors its "a route's worth of
  instructions" check at sixty million rather than a hundred.

  **The bound has two halves, and only the first was obvious.** The first
  is the horizon: advance whole iterations that end *strictly before* the
  scheduler's near horizon, never up to it, so the crossing is left to a
  real instruction and the placement of an event relative to an instruction
  boundary stays the machine's decision. Nothing can raise a line before
  that horizon, and with no instruction running nothing can store to
  memory, so the exit condition provably cannot move.

  The second is that **nothing may be standing either**, and it cost a
  census to find. A request the mask already admits has not been *latched*
  yet. The decision at the top of the instruction loop reads the latch, so
  it answers "next instruction" and the spin looks idle — but the next
  instruction's own end-of-cycle sample would catch that request, and the
  boundary after it would take the vector. A skip spans both boundaries and
  delivers the interrupt a turn late, which is precisely what the first
  working version did: within one `00:E321` call the ordered writes stayed
  identical in sequence, value and instruction address, and the interrupt
  entry arrived 24 master cycles late. `(IRQlow & IRQMask) != 0` is the
  whole fix and it costs 1,452 skipped iterations out of 21 million.

  Two plausible explanations were eliminated on the way there, and both are
  worth recording because both point at the wrong mechanism:

  - *Not the granularity, and not the omitted interrupt samples.*
    Replacing the single advance with a per-iteration replay of the exact
    cycles — `ADDCYC(3)`, sample, `ADDCYC(1)`, twice a turn, which is what
    `LastCycle` does — produces a **byte-identical** census to the coarse
    skip, and both differ from the golden the same way.
  - *Not proximity to the horizon.* Leaving a whole iteration of slack
    diverges too, differently.

  Both results look like they exonerate the sampling, and they do. The
  fault was in the *deciding*, which is a different thing done at a
  different moment: `LastCycle` latches once per instruction, but
  `DcSelectInterrupt` runs once per pass of the instruction loop, and a
  skip silently spans as many of those passes as it covers. A replay that
  reproduces every sample and every cycle still reproduces no decisions,
  which is why the fine and coarse versions failed identically — a result
  that reads as exoneration and is really a fingerprint.

  One methodological note, since it wasted a cycle: counting skips after
  which `next_event` had gone non-positive returns zero, but that test
  cannot fail. `reach_horizon` recalculates the horizon to a fresh positive
  value before the advance returns, so a crossing is invisible to any test
  made after it.
- **There is no device at the frame boundary, and that is the finding.**
  The plan called for one more seam around `Emulate()` in `pce.cpp`. Read
  against this machine — a plain 384 KB HuCard, no CD, no SuperGrafx, no
  MCGenjin — that function has almost nothing of its own left. `PSG_Update`
  and `PSG_ResetTS` go to the sound model; `StartFrame`, `RunPartial`,
  `ResetTS` and `EndFrame` go to the encoder, including the `frame_done`
  that decides where a frame ends; `SyncAndResetTimestamp` goes to the
  scheduler; `HuC_Update` and `HuC_ResetTS` dereference `mcg`, which is
  only ever constructed for the MCGenjin mapper and is null here; every
  ADPCM, CDDA and `PCECD_` call is behind a null buffer or `PCE_IsCD`. What
  is left is `PCE_TimestampBase`, which only the debugger and the boundary
  observation read.

  So the frame loop is not an object that was missed. It is a rebase
  protocol over six models that are already native, plus two things that
  merely happened to be called from the same function. One was **the pad**
  (`INPUT_Frame`, `INPUT_AdjustTS`), which drove the emulator's
  `PCE_Input_Device` objects — that is now the eighth seam and **done**, so
  the two calls the frame loop makes are dispatches to
  `devils_crush::input::PcePad` and the loop has one less thing in it. The
  other is **the sound output chain**
  (`OwlBuffer::Integrate`, `HRRes->Resample`), which sits downstream of the
  native PSG because `dc_beetle_set_psg_device` hands the model the
  emulator's own blip buffers to synthesize into — and that one is not a
  gap, it is the resampling decision under *Open questions*, which keeps it
  in the path until phase 4 so `audio_sha256` stays a live oracle.

  With the pad done, that leaves one, and it is deliberate. The frame loop
  becomes native by subtraction, which is the phase 4 pattern rather than a
  ninth seam.

### Phase 4 — delete the CPU

Once every routine is native and phases 2–3 hold, there is no 6280 to
emulate. Bank mapping (`MPR`, `TAM`/`TMA`) collapses into ordinary ROM
addressing in native code. `RoutineReplacement`, `ReplacementCheckpoint`, the
hook machinery, the write channel, the replacement frame stack and most of
`PceRuntime` all go away together.

## Open questions

- **Cycle model fidelity.** Phase 3 is the real risk. The census is
  cycle-sensitive: the `CLI` delayed-effect bug found in the sound driver
  moved a nested interrupt by nine master cycles and changed the census hash.
  A native time model has to be at least that precise. The scheduler now
  installed is that precise — four census arms, unmoved hash — but it is
  only half the answer: it decides *when* the machine stops, and the emulator
  still decides how many cycles each instruction took to get there.
- **Can the census survive a native frame boundary?** Decided: yes, and for
  a better reason than expected. The worry was circularity — the route is
  3,600 frames measured *at* the frame boundary, so a model that moved the
  boundary would move the hash by construction and the hash would stop
  being evidence. It turns out the census frame is not the game's frame and
  never was. `run_frame` is one `retro_run`, which returns when the encoder
  reports `frame_done` at raster line `14 + max(240, slend + 1)`, and that
  boundary is already native — it is `PceVce`'s. The game's own frame
  boundary is the spin loop's exit, and the two drift against each other:
  over 3,600 raster frames the loop exits 3,567 times, with **73 frames
  containing no exit and 40 containing two**. So work on the spin loop
  happens *inside* a census frame and cannot move the sample point. What
  would break the oracle is redefining `run_frame` to end where the game
  thinks a frame ends — so that is the thing not to do, and the raster
  boundary stays the census boundary through phase 4.
- **Route coverage.** One 3,600-frame route is a strong oracle but not a
  complete one, and the PSG slice showed that adding routes is not always the
  remedy. The LFO fires zero times on the census route, and it would fire zero
  times on every other route as well, because the game never writes a non-zero
  LFO control byte. Where a device path is unreachable from the game, a second
  route buys nothing and a differential test against the emulator's own device
  buys everything — see `tests/psg_device_tests.cpp` and the coverage table
  under phase 2. Additional censused routes are still worth having for the
  parts of the machine the game *does* drive differently; they are just not
  the tool for this.
- **Audio resampling.** Decided: neither. The resampler is host-side, not
  part of the PC Engine, so the native device stops at the delta buffers and
  the emulator's resampler stays in the path until phase 4 removes it. That
  keeps `audio_sha256` a live oracle for the whole hybrid period at the cost
  of one deliberate re-baseline at the end. What still has to be decided is
  what replaces it — a windowed-sinc of our own is easy; sounding the same as
  the recordings people have of this game is the part worth thinking about.
- **Savestates.** `serialize`/`deserialize` are part of the contract and
  currently wrap Beetle's format.
