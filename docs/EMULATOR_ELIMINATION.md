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
| **Device state** | The VDC and PSG are native models, not Beetle's | No |
| **Time** | Interrupt scheduling and cycle pacing are native | No |

The gap matters concretely. Today a "native" routine still drives emulated
hardware: `replace_sound_timer_driver` emits a write to `$0806`, the core
applies it with `WrMem`, and it lands in **Beetle's** PSG, which synthesises
the audio. Same for every VDC port write. At a hypothetical 100% native
cycle share the emulator would still be present, still own all device state,
and still own the clock.

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
  new test harness is required for phase 2.
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
| `00:FC03`–`FCB3`, `00:FD07`–`FDF1` | 193,007,589 | 14.93% | Sound driver — **already ported**; these are the entries the no-nesting rule blocks (see below) |
| bank `45` (`4000`–`4A9B`) | 21,152,865 | 1.64% | Music sequencer — the per-frame tick and two helpers the snapshot rule **blocks**, see below |
| `00:E0EB`–`E292` | 10,888,014 | 0.84% | Pad reader and interrupt dispatcher, both **blocked** on a device read; the handler bodies and call wrappers are native |
| `03:694A`–`6A37` | 2,700,153 | 0.21% | Collision-table interpreter — **already ported**; the `RTS` instructions and the resumption after an interrupt lands on a checkpoint, see below |
| `00:E93C`–`EBD6` | 3,780,738 | 0.29% | Palette shadow buffer — the two fade drivers and the block decrement, **blocked**, see below |
| `02:586F`–`5903` | 501,195 | 0.04% | Sorcerer tick — **already ported**, and both interpreter calls are now composed in line; what is left is the two bank-wrapper calls, three `RTS` instructions and resumption |
| `01:CA84`–`CC04` | 621,648 | 0.05% | Collision frame — **already ported**, and the call into the scanner is now composed rather than emulated, see below |

Earlier revisions of this table mixed two conventions: five rows counted only
emulated addresses and four counted the whole range including native entries,
which made ported regions look like remaining work. Every row above is now the
emulated count.

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

The sound-driver row grew by 8.2M, and almost none of that is driver work. 7.5M
of it lands on `00:FC8D`, the driver's `RTI`, which the profile charges with
whatever the timer interrupted — and what it interrupts is now a longer
transcript, so more already-native cycles replay with no hook in between. The
part that is a real loss is 683,271: the 1,184 driver entries that arrive while
the composed sweep is suspended and run emulated because the core will not nest
a replacement. That is the whole reason 3,524,010 gross became `+2,828,949`
net, and it is the no-nesting note below rather than anything unported.

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

1. **Bank `45` music sequencer** — **finished for the hybrid** at 25.0M of
   45.6M. The PSG write channel and the bus-write census built for the timer
   driver applied unchanged, as expected. The bank turned out to be sixteen
   small subroutines rather than a monolith, so it went in slices: the
   sample/envelope group, the PSG register programmer `45:4142`, the
   portamento stepper `45:4228` and the command interpreter `45:457A` are
   native. See
   [NATIVE_FALLBACK_HOTSPOTS.md](NATIVE_FALLBACK_HOTSPOTS.md#the-music-sequencer-and-screening-a-bank-instead-of-a-family).

   The remaining 20.6M is the per-frame tick `45:4015` (13.8M), which calls
   all of them, plus `45:4936` and the track loader `45:4BCC`. **`45:4015`
   cannot be ported while the emulator is here**, and the arithmetic is worth
   recording because it is cheap to check before writing a transcript and
   expensive to discover afterwards. Its voice loop reaches `45:4148`,
   `LDA $F5,X`, three times per call, so any transcript for the tick reads
   driver state and the shared-state guard below applies to the whole body.
   The body is 12,667 master cycles; the timer period on this route is 5,068
   (254,990 driver entries across 1,292,370,213 cycles). It can never finish
   before the next serviceable timer interrupt, so it would decline 3,599
   calls out of 3,599. The same guard admits `45:4142` at half its calls
   because its body is only 2,054 cycles.

   This is the second thing on the list blocked by the hybrid rather than
   unported, after the sound driver's nesting limit, and it has the same
   resolution: phase 3 removes the snapshot, so there is nothing to go stale.

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
   | `00:EAA7` block decrement | 1,640,100 | 205,012 cycles per call, over the checkpoint cap |
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
   of phase 1, and it is the answer to `00:EAA7` in the table above as well.

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
   channel. Both are phase-2 or phase-3 problems.
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
   | `00:E195` pad reader | 6,929,670 | **blocked**: reads `$1000` |
   | `00:E241`/`E255` banked call | 2,878,650 | **native** |
   | `00:E111`–`E13F` VBlank body | 2,412,678 | **native** except its `RTI` |
   | `00:E262` trampoline | 953,343 | **native** |
   | `00:E140`–`E178` VBlank epilogue | 821,685 | **native** |
   | `00:E058`–`E0EA` dispatcher | 769,938 | **blocked**: reads `$0000` |
   | `00:E0EB`–`E110` VBlank prologue | 544,104 | **native** |
   | `00:E219` bank pair | 501,639 | **native** |
   | `00:E1FD`/`E20D` asset resolver | 377,085 | **native** |

   The widening would not have helped, because the blocker is a device
   **read**, not a write. `00:E195` strobes `$1000` and then reads it back
   five times a frame; `00:E058` opens by reading the VDC status register at
   `$0000`. A replacement callback receives work RAM and registers and nothing
   else, so no allowlist reaches either one. **A routine that reads a device
   register cannot be a transcript until that device has a native model**, which
   makes both of these phase 2 rather than phase 1 — and puts the joypad on
   phase 2's list beside the PSG and the VDC.

   The bank-switching wrappers went instead, together with the two in the
   `00:ECC6` family, and they are done: nine entry points, 52,911 calls,
   4,849,479 cycles, admitted on every call. They carry no game logic — they
   are the game's linkage — and they were the first transcripts in the port to
   change the bank mapping. `TAM` is expressible because `CpuRegisters` carries
   all eight `MPR` slots and a checkpoint applies them.

   The VBlank body followed, and needed no new capability either: six entry
   points around the calls the core keeps, writing the VDC ports the channel
   already carries. 13,932 calls, admitted on every one.

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

   What is left in the region is the dispatcher and the pad reader, both phase
   2, plus the two `RTI`s and the three `JSR`s the walks stop at — 1.3M of
   instructions the core executes by design.
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
10. **Gameplay tails** (~17M spread thin, no single address above 0.14%). What
   is left of the interpreter's region is mostly resumption rather than
   unported code; the largest genuinely unported group is now the palette
   shadow buffer at `00:E94C`–`EBD6` (3.8M).
11. **Boot and reset path.** Off the census route's hot path but required.
12. **The frame-sync spin loop** (584.8M). Trivial code, but it is where the
   native frame boundary ends up living — do it with phase 3, not before.

### Phase 2 — native device models

- **Joypad.** The smallest of the three and now a phase-1 blocker rather than
  a nicety: `00:E195` cannot be ported until `$1000` reads can be answered
  natively. Model the strobe/select protocol against the `InputFrame` the host
  already supplies to `run_frame`. Acceptance: the pad reader's transcript
  diffs clean against assembly, which also settles 6.9M of phase 1.
- **PSG.** Accept `$0800`–`$0809`, model the six channels, DDA output, noise
  and LFO, and produce the `AudioView` float samples. Acceptance:
  `audio_sha256`. This is the larger of the two, because Beetle's PSG
  includes resampling to 44.1kHz that has to be matched bit-for-bit.
- **VDC.** Accept the VDC register file, maintain VRAM, the post-DMA SAT,
  palette and scroll, and produce a `VideoMemorySnapshot`. Acceptance:
  `framebuffer_sha256`. Much of this is already done: `src/render/` decodes a
  snapshot into pixels natively today. What is missing is the state machine
  that *maintains* the snapshot in response to port writes, plus the raster
  timing that decides when each line is sampled.

### Phase 3 — time and interrupts

- Timer reload and IRQ generation (`$0C00`/`$0C01`), interrupt mask and
  acknowledge (`$1402`/`$1403`).
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
hook machinery, the write channel, the replacement-nesting limitation and
most of `PceRuntime` all go away together.

## Open questions

- **Cycle model fidelity.** Phase 3 is the real risk. The census is
  cycle-sensitive: the `CLI` delayed-effect bug found in the sound driver
  moved a nested interrupt by nine master cycles and changed the census hash.
  A native time model has to be at least that precise.
- **Route coverage.** One 3,600-frame route is a strong oracle but not a
  complete one. Additional censused routes — particularly one that plays
  looping music, which the current route never does — should exist before
  phase 2 is declared correct.
- **Audio resampling.** Matching `audio_sha256` means matching Beetle's
  resampler exactly, or changing the oracle. Worth deciding early which.
- **Savestates.** `serialize`/`deserialize` are part of the contract and
  currently wrap Beetle's format.
