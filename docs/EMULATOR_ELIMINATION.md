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
| **Time** | Interrupt scheduling and cycle pacing are native | Partly |

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

What is still Beetle's is the clock and the *taking* of an interrupt. The
native controller publishes a level; the emulator's processor samples it at an
instruction boundary and picks a vector. At a hypothetical 100% native cycle
share the emulator would still be present and still own that.

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
table rather than appearing at its old size:

| Region | Master cycles | Share | What it is |
| --- | ---: | ---: | --- |
| `00:E2B7`–`E39C` | 584,758,197 | 45.25% | Frame-sync spin loop |
| `00:FC03`–`FC8D` | 6,830,889 | 0.53% | Sound driver — body **and epilogue** now native on every arrival. Three deliberate refusals account for 6,810,810 of this: the `RTI` at `FC8D` (5,292,108), the `SEI` at `FC81` (1,509,498), and the chain exit `FC89` with the `FC03` trampoline it lands on (9,204). The remaining 20,079 is the body itself running emulated on the few dozen arrivals a route where the walk declines |
| `00:FD07`–`FD81` | 82,117 | 0.01% | The driver's second arm, likewise, with its own `RTI` (57,771) and `SEI` (16,548) |
| bank `45` (`4000`–`4A9B`) | 5,266,590 | 0.41% | Music sequencer — the tick is **native**, chunked at its voice loop, and so is the PSG programmer `45:4142`, chunked at its channel loop; what is left is `45:4936`, the track loader `45:4BCC` and the chunk boundaries |
| `00:E128`–`E290` | 1,289,166 | 0.10% | Interrupt epilogue and the dispatcher at `00:E058`, still **blocked** on the VDC status read; the handler bodies, the call wrappers and the pad reader are native |
| `01:C0FE`–`C1B5` | 1,407,828 | 0.11% | The top-level frame loop itself — glue between calls rather than a routine |
| `03:694A`–`6A37` | 2,721,348 | 0.21% | Collision-table interpreter — **already ported**; the `RTS` instructions and the resumption after an interrupt lands on a checkpoint, see below |
| `00:E93C`–`EBD6` | 1,079,359 | 0.08% | Palette shadow buffer — the two fade drivers and the whole-palette `TIA` arm, **blocked**, see below |
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
   | `00:EB30` VCE upload | 8,636,040 | **native** except the 29-call `TIA` arm |
   | `00:E9B6` block interpolator | 6,548,946 | **native** |
   | `00:EBA5` block stager | 2,629,866 | **native** |
   | `00:EAA7` block decrement | 1,640,100 | **native**, in thirteen chunks a call |
   | `00:E93C` sixteen-bit compare | 758,370 | **native** |
   | `00:E94D`, `00:EA77` fade drivers | 140,037 | call the frame-sync spin at `00:E321` |

   The two fade drivers are not portable for the same structural reason the
   spin loop is not: a transcript for either would have to span the
   twenty-one frames they wait through. `00:EB30` is done: it took the
   bounded-register move described in item 3, applied to the VCE, plus the
   matching widening of the core's wait-state compensation. Its whole-palette
   arm stays emulated — one `TIA` moving 1,024 bytes is 1,024 writes against
   a 512-write channel — but that arm is 29 calls of 3,599.

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
   | `00:E058`–`E0EA` dispatcher | 769,938 | **blocked**: reads `$0000` |
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
   its own is not: `00:E058` reads what the display is doing, and stays phase
   2 beside the VDC. The joypad has come off that list, and so has the PSG.

   The bank-switching wrappers went instead, together with the two in the
   `00:ECC6` family, and they are done: nine entry points, 52,911 calls,
   4,849,479 cycles, admitted on every call. They carry no game logic — they
   are the game's linkage — and they were the first transcripts in the port to
   change the bank mapping. `TAM` is expressible because `CpuRegisters` carries
   all eight `MPR` slots and a checkpoint applies them.

   The VBlank body followed, and needed no new capability either: six entry
   points around the calls the core keeps, writing the VDC ports the channel
   already carries. 13,932 calls, admitted on every one. The pad reader made
   it seven and 17,531, and was the one addition here that did need a new
   capability — the pad state on `RoutineEntry`.

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
   94.8% of everything outside the idle spin loop runs natively.
16. **Gameplay tails** (32.2M, 2.49% of the machine, of which 6.9M is the
   driver residue item 15 leaves deliberately). Once entry points, boundary
   instructions and interrupt resumption are separated out —
   see `docs/NATIVE_PORT_WORKFLOW.md` — nothing genuinely unported is above
   1.6M. In order: `08:41AB` (1.56M), `01:C700`–`C77F` (1.22M),
   `08:4080`–`40FF` (1.20M), `01:DC80`–`DCFF` (950K), `00:E200`–`E27F` (894K),
   `03:6000`–`607F` (857K), `01:C100`–`C17F` (839K), `00:E000`–`E07F` (775K).
   Three entries that used to head this list — `08:40DE`–`41AB` with
   `08:4085`–`4094` at "2.8M", `01:C5C8`–`C770` at "1.6M", `03:6001`–`6053` at
   "883K" — were boundary instructions and resumption around routines that
   were already native.
17. **Boot and reset path.** Off the census route's hot path but required.
18. **The frame-sync spin loop** (584.8M). Trivial code, but it is where the
   native frame boundary ends up living — do it with phase 3, not before.

### Phase 2 — native device models

- **Joypad.** Largely **done**, and it turned out not to need a device model at
  all. `00:E195` models the strobe/select protocol inside its own walk against
  the `InputFrame` the host already supplies to `run_frame`, now carried on
  `RoutineEntry`; the transcript diffs clean against assembly in the pad bytes
  and in the `$1000` writes themselves, and it settled 6.9M of phase 1. What
  remains for this phase is owning the port rather than mirroring it — reading
  the host's input directly instead of through the core's `current_input_`,
  which happens when the frame loop goes native.
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

  The savestate shuttle has its own test, `dc_rom_video_state_shuttle`,
  because the census cannot reach it: a recorded route never saves or loads a
  state, so both video models could carry a broken shuttle and every arm would
  still match. A routine replay serializes, restores and re-runs a call over
  eight round trips, and the two arms must reproduce the *same* calls.

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
- VDC/VBlank interrupt timing against the raster.
- Cycle pacing. Native transcripts currently declare `body_master_cycles` so
  Beetle can schedule around them; something must still model time to the
  same precision, because the game's behaviour depends on where the timer
  interrupt lands relative to the frame. This is the least obvious phase and
  the one most likely to be underestimated.
- The spin loop becomes the native frame boundary.

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
  A native time model has to be at least that precise.
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
