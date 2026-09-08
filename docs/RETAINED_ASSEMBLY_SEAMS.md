# Retained Assembly Seams

## Scheduler continuation calls: 01:C69B, 01:C6A6 and 01:C6BF

The shared scheduler JSR replacement now also owns these three calls. C69B
hands off to the delay owner C5EA; C6A6 and C6BF hand off to C5C9. The live
stack frame, cycle-zero high/low writes, 18-cycle interrupt sample and
21-cycle completion are unchanged. Both event horizons must exceed 21 cycles;
the existing mapping, transfer-mode and RAM-size guards remain in force.

The fixture covers all 327,680 stack-pointer/status combinations across the
five calls, plus both horizon guards and invalid mappings/PC/RAM sizes. The
assembly oracle checks the three additional ROM instructions and enables all
five calls together. Its five existing artifact hashes are unchanged, covering
frame state, scheduler call registers/timing, ordered call writes, all RAM
writes and hardware writes. The new calls admit 1/1, 42/48 and 1/1 canonical
entries respectively. The registry now contains 712 entries; the seam manifest
still contains 198 ranges.

All 31 current-registry routes match with only these three addresses excluded
on the before side: state hashes and abandonment counts are unchanged, and
every interpreter-cycle delta is confined to the target calls. Their cost
falls from 61,026 to 8,064 master cycles (52,962 removed, 86.79%). Total
non-wait interpreter work falls from 14,974,119 to 14,921,157 (0.35%). See
`out/scheduler-followup-cost/manifest.csv`, its before/after primary rankings,
and `out/scheduler-followup-cost-summary.log`. This is an isolated comparison
against the current registry, not fresh whole-registry assembly parity.

Release and Debug each pass the expanded call oracle, exhaustive fixture,
face-corruption capture and upper-hand bus capture (four checks each).
Logs are `out/scheduler-followup-tests-{Release,Debug}.log`; the original
capture files remain unchanged. Registry generation, all 198 seam ranges
and `git diff --check` pass. The full test suite was not rerun.

## Scheduler call frames: 01:C693 and 01:C696

Both standalone JSR instructions now stage the authentic high/low return
address bytes at the live, page-wrapped stack pointer and hand off to C5EA
or C5C9. The children keep ownership of coroutine continuation and dispatch.
The calls preserve registers and flags, require the live RAM/code mappings
and clear transfer mode, and decline unless both event horizons exceed
21 master cycles. Stack writes occur at offset zero; the interrupt sample
is at 18 and the instruction completes at 21. No RTS observer is closed.

The fixture covers both instructions across all 256 stack pointers and all
256 status values, plus both horizons and mapping/PC/RAM guards. The pinned
gameplay assembly comparison observes the calls and both children, admitting
1/1 C693 and 54/60 C696 calls with identical frame, call, ordered call-write,
all-RAM-write and bus-write artifacts. The six close-horizon calls remain
visible as guarded interpreter work. The registry contains 709 entries;
the manifest still has 198 ranges.

The fresh 31-route registry comparison excludes only these two calls on the
before side. Every state hash and abandonment count matches, and all cycle
deltas stay at the two call addresses. Their interpreter cost falls from
73,281 to 7,908 master cycles: 65,373 removed (89.21%). Total non-wait
interpreter work falls from 15,039,492 to 14,974,119 (0.43%). The paired
manifest and refreshed primary rankings are in `out/coroutine-calls-cost/`.
This is an isolated current-registry comparison, not a new whole-registry
assembly-parity claim.

Release passes the call oracle, delay oracle, exhaustive fixture and both
pentagram regressions. Debug passes the call oracle, fixture and both
pentagram regressions. Both 4,366-frame face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`
and zero abandonments. The original capture inputs, checkpoint and high-score
file remain unchanged. Logs are `out/coroutine-calls-{release,debug}-tests.log`
and `out/coroutine-calls-capture-{Release,Debug}.log`. The full CTest suite
was not rerun.

## Selection-controller return: 08:5A69

The selection controller still ends its body at the RTS, whose standalone
entry now shares the atomic live-stack implementation with `08:5976` and
`01:C4C8`. It verifies the live code and stack mappings, preserves registers
and flags, and runs only when both event horizons exceed 21 master cycles.
The offset-zero observer contract preserves scopes rooted at the return
itself and at its `08:5A3E` parent. Close horizons remain explicit fallback.

The gameplay assembly comparison admits 517/550 returns and preserves frame
fingerprints, call registers/timing, ordered call writes, every RAM write and
all bus writes. The exhaustive fixture now covers 589,824 return cases across
the three addresses, plus horizon and mapping guards. The existing selection
controller regression and both Release pentagram comparisons also pass.

A fresh 31-route registry comparison excludes only this return on the before
side. State hashes and abandonment counts match on every route. The return
falls from 358,050 to 21,483 interpreted master cycles (94% removed), reducing
total non-wait interpreter work by exactly 336,567 cycles (2.19%). The paired
manifest and refreshed ranking are in `out/selection-return-cost/`. The
remaining 33 returns per route retain the event-horizon fallback.
Debug passes the same pinned gameplay assembly comparison, the exhaustive
fixture and all 4,366 pentagram capture frames with the return deployed.

## Delay scheduler continuation: 01:C5EA-C603

The standalone entry saves the delay, consumes the live JSR frame and installs
its successor as the actor continuation before jumping through `$202F`.
Stack indices wrap within page 21 and the continuation increments with 16-bit
wrap. MPR1 must map F8, MPR6 must map 01, X must remain below 80, and transfer
mode must be clear. Each instruction has its own interrupt checkpoint. The
indirect jump does not close an RTS observer; the caller scope remains live.

Release assembly comparisons match all frame, call, RAM-write and bus-write
artifacts for gameplay (2 calls) and higharc (23 calls). Full-registry profiles
remove all interpreted C5EA-C603 work on gameplay, higharc and nudge: 294,
3,258 and 5,439 master cycles. Frame hashes and abandonment counts are unchanged.
The separate C5E9 return remains classified as a core boundary.
Debug also passes the pinned gameplay assembly comparison and the full
4,366-frame pentagram regression with the delay scheduler deployed.

## Run-input return: 01:C4C8

The standalone RTS uses the same 21-cycle live-stack transcript as `08:5976`.
It requires MPR1=F8 and the live code segment to map bank 01. Stack indices
wrap within their page, the return word increments with 16-bit wrap, and the
destination bank comes from the current MPRs. Either event horizon at or
within 21 cycles leaves execution in the core. The registry owns this entry;
it is no longer installed by the legacy automatic core-return hook list.

The gameplay comparison observes both C4C8 and its C49E parent, including
frame state, call registers/timing, every RAM write and all bus writes.
Release and Debug admit 712/860 returns with identical assembly artifacts.
Full-registry higharc and nudge profiles remove 4,830 and 4,599 interpreted
master cycles respectively while preserving state and abandonment counts.
The shared exhaustive fixture covers both return addresses and their guards.

## Upper-hand timer bus read: 02:5983

The live setup at `02:5975` stops before absolute `ADC $0080`. MPR0 normally
maps FF, so this is a VDC status mirror, including its hardware side effects.
The core owns that read. Native execution resumes at `02:5986` using the
live accumulator and flags, writes the timer and invokes the scheduler.
No RAM snapshot substitutes for the bus value. Both sections preserve the
existing hand-slot and decimal/transfer-mode guards. RNG return and CLC
checkpoints use their individual 21- and six-master-cycle timings.

The portable hand/RNG route preserves its existing trace hashes,
including the interrupt passage. The pentagram capture compares both native
sections to the original hand setup across all 4,366 frames and both hand
activations, including frame fingerprints and ordered call write traces.
The new continuation is admitted separately; the bus instruction stays
visible as a core seam in the execution-cost ranking.

## Resident pattern loader correction: 00:F768-F782

The native head executes `PHX / ASL A / TAY`, saving X for the existing PLX
tail and selecting the two-byte descriptor with the shifted accumulator.
The startup regression had A, X and Y all zero, concealing the former
PHA/immediate-load misinterpretation. The face request uses A=01, X=36, Y=05:
using Y directly selected the wrong pattern pack and saving A restored the
wrong actor slot, disrupting the scheduler after the graphics upload.

ASL and TAY have separate six-cycle checkpoints, and the following indexed
load takes 15 cycles. The mapper and decoder JSRs push at instruction entry.
The E1FD mapper and E41F decoder remain separate child boundaries. The
4,366-frame pentagram replay and the higharc regression compare native work
against the original loader, including complete frame fingerprints, call
registers and timing, and ordered call writes. The ROM-free fixture varies
the selector, flags and stack position independently of the incoming X/Y.

## Shared scheduler return: 08:5976

This return now has a standalone native transcript. It reads the caller's
current stack bytes through MPR1=F8, wraps each pop within the stack page,
adds one to the return word and resolves the destination through the live
MPRs. It consumes 21 master cycles and declines unless both event horizons
are strictly farther away. Near-horizon returns retain the core path.

The absorbed-return observer must not replay a return at transcript offset
zero when it is the entry instruction: the ordinary instruction observer
already processed that boundary before opening any new observation there.
The gameplay regression observes both this RTS and C5C9 and compares frame
state, call/stack traces, every RAM write and all bus writes. Release and Debug admit
575/670 returns. The exhaustive fixture covers 196,608 return-word/status/
stack combinations plus mapping, buffer and both timing guards. All ten
Release absorbed-return/coroutine regressions pass after the observer fix.

## Sequencer gate continuation: 45:404F-4070

The standalone `$404F` entry reuses the sequencer's gate calculation after
an enclosing walk declines. It preserves TAY, retrigger-bit clearing,
byte-wide gate accumulation and the mode/release branches, with a checkpoint
for every instruction. It stops at `$4072`, `$4081` or `$408F`; composed walks
retain their existing control flow. Release and Debug admit all 19 natural arrivals
and preserves the music family's assembly hashes. The ROM-free fixture checks
196,608 gate-step/accumulator/branch combinations, including carry, overflow,
write timing and all three successors.

## PSG programmer timer horizon: 45:4142-4148

The programmer preserves its CLX/PHP/TXA/LSR/BCS prefix when the read of
the timer driver's `$F5,X` byte cannot complete before the supplied timer
horizon. It hands control back at `$4148`, before reading the snapshot,
with five checkpoints, 33 master cycles and the PHP write at cycle 15.
The core performs the live read and can deliver interrupts at the original
instruction boundaries. The final shared-state guard still rejects composed
walks whose earlier reads already missed the horizon; no timer guard is relaxed.

The music replay now admits all 3,604 `$4142` entries. Its former 42 standalone
`$4143` arrivals disappear because the prefix is preserved. The registry and
seam manifest need no new entries. Unit coverage checks 12,032 horizon/stack
combinations, including the exact 45/46-cycle read boundary and stack wrap.

## VBlank mode-2 display setup: 00:E09E-E0CD

Mode 2 now programs vertical scroll from `$2D56` and the raster compare from
the byte-wide `94 - displacement`, then joins the existing control-shadow
update and palette-call handoff. The complete native transaction is 345 master
cycles, 27 instruction checkpoints and 14 ordered writes, including the
palette JSR's two stack bytes at instruction start. Each VDC write retains its
stolen cycle and earlier interrupt sample. The dispatcher at `E05F` now hands
mode 2 to this entry instead of declining its entire transaction.

The VDC acknowledge, palette child, PLP and interrupt-return boundaries keep
their existing owners. Mode 1 and the mode-5 dispatch remain unimplemented in
this family. The mode-2 body reads RAM only; no live device read is predicted.
The alley and spiral routes exercise 203 and 187 entries respectively.
`dc_vblank_mode_two_tests` checks all displacement/status pairs, arithmetic
overflow and wrapping, stack wrapping, ordered port writes, mapping/buffer
guards, and the dispatch handoff. The ROM regression compares frame state,
ordered RAM writes (all RAM in Release, the handler's ranges in Debug), and
timestamped device writes. An interrupt arm has no ordinary RTS boundary for
a subroutine-call observer.

## VDC attribute write body: 01:DA0E–DA3B

The attribute merge and six VDC port writes now execute as a native body:
294 master cycles, 21 instruction checkpoints, and 13 total writes. Four
ASLs update the live $29 byte and carry before merging the device-read
accumulator nibble. VDC stores sample interrupts six cycles before completion;
RAM stores and arithmetic retain their ordinary three-cycle sample offset.
DA06/DA0B still read the actual device buffer. DA3E PLY and DA3F RTS remain
core-owned, including callers taking the transaction's early exits.

The isolated gameplay replay admits 2/2 entries with identical frame/state,
call, RAM-write, and timestamped bus traces. Exhaustive tests cover all 65,536
accumulator/attribute byte pairs, all status admission values, mapping and
buffer guards, stack preservation, output flags, and ordered writes.

## Actor initialization and continuation: 01:C655–C6BF

Four native RAM bodies now cover C655–C691, C699, C69E–C6A3, and C6A9–C6BD.
They initialize slot zero, set the delay argument, toggle an actor attribute,
and activate the actor after its delay. CLX preserves flags; each store is
timestamped at instruction completion. Continuations accept all byte-sized
X values and arbitrary stack depth, with RAM/code mapping and D/T guards.
The C693/C696/C69B/C6A6/C6BF JSRs remain separate because C5EA and C5C9 own
live scheduler frames and resumption. No stack operation is synthesized.

The isolated gameplay replay exercises 1/1, 1/1, 48/48, and 1/1 entries,
respectively, and compares state, call/write traces, and hardware-bus writes.
ROM-free fixtures check 262,144 slot/value/segment combinations, flags,
write addresses, checkpoint/write timing, preserved stack/Y, and guards.

The next substantial body to investigate is DA0E–DA3B. Its VDC reads occur
at DA06/DA0B, before this entry; the residual body itself packs the attribute
and writes the VDC. Keep the preceding reads and the DA3E PLY/DA3F RTS
ownership explicit rather than rejecting the entire range as device reads.

## VDC setup loop: 00:E271–E292

E271 now owns the complete twelve-triple setup sequence through E290,
including its pointer initialization and closing branches. The existing VRAM
transcript machinery preserves each instruction checkpoint, zero-page writes,
and ordered device writes. Port stores take 18 master cycles and sample the
interrupt six cycles before completion; ordinary instructions sample three
cycles before completion. CLY preserves flags, while INY and CPY update them.
No device state is predicted and E292 remains a core RTS.

The isolated gameplay replay admits all three calls and compares frame state,
complete call/RAM-write traces, and hardware-bus writes. ROM-free tests exercise
all 65,536 status/stack combinations (D/T modes decline), every register triple,
all checkpoint sample offsets, and invalid mapping/buffer guards.

## Shooter release countdown: 02:51AF–51D5

The native body now includes expiry, arbitrary X/SP, and the full byte-wide
appearance lookup index. It corrects the dormant continuation's indexed write
to the ROM's absolute `STA $3980` and emits stores at instruction completion.
Non-expiry ends at core RTS 51D5; expiry ends before JSR 51D2. The independently
native 5475/5481 child retains its own stack and resumption ownership.
The alley replay covers 340 calls including 68 expiry writes to shared $3980,
with identical state, call/write, and bus artifacts. Direct tests cover 65,536
timer/state combinations, varying X/SP, byte wrap, flags, and mapping guards.
The larger 08:5A69 residue entry is only an RTS, not a competing body port.

## Appearance submission callers: 02:5475 / 02:5481

The native appearance submission helper now accepts arbitrary actor slots and
stack depths with work RAM mapped through MPR1=F8, code through MPR2=02, and
the child jump table through MPR6=01. Decimal and transfer modes still decline.
Its PHA/JSR stack writes wrap independently; the continuation reads the live
saved appearance, computes the 16-bit palette offset, and preserves the
original C042 and C05D child boundaries. No child return is absorbed.

The implementation is isolated in `native_replacements_appearance.cpp` for
direct testing: 65,536 appearance/stack combinations cover pointer carry,
overflow flags, intermediate shift writes, and mapping rejection. The alley
replay includes 68 calls at SP=F9, beyond the former FB-only prefix guard.
The following 54A3 controller remains outside this change.

These entries record why particular paths currently retain emulated
instructions: interrupt sampling, caller ownership, transaction capacity, or
missing verification. They remain porting candidates when measured interpreter
cost justifies solving those constraints. Use the coverage sweep's
`registry-interpreter-residue.md` to prioritize them alongside guarded
controllers, and supply the required boundary or resumption model before
expanding native ownership.

Before proposing a replacement that touches any address below, read the reason.
If the reason no longer holds, say why in the slice, and change the entry.

## Collision vector dispatch `01:CBF9`

The synthetic table-vector RTS is now installed as an exact core capability.
Patch 0036 admits this opcode to the existing RTS contract; the collision
transcript still stops before it. The original core performs the live stack
reads, increments the vector target and samples interrupts. It does not mark
the enclosing collision call complete or infer a caller return from the stack.
No stack bytes, register flags or mappings are synthesized by this descriptor.

This is a core-backed seam, not a C++ rewrite of the collision handler and not
evidence of a speedup: the original core opcode body still executes. Any
reduction in reported interpreter residue here is a change in admitted core
coverage. The CLI retains its conservative interrupt-horizon fallback.
`verify_collision_vector_rts.cmake` compares the vector-dispatch call and write
traces, full recorded state, and device writes against assembly on gameplay
and alternate routes. Descriptor tests cover every status/stack-pointer pair.

## Eyeball final-animation controller `02:4F3D-4F69`

The old slot-specific timer-wait fast path remains intact. Its fallback now
models the complete animation prefix: timer reload, four-frame selection,
frame wrap (including flag-preserving CLA), dirty marking, and phase borrow.
Each instruction has its own checkpoint and each RAM write retains its cycle.
The fixed frame table is read from the verified bank-02 ROM mapping. Unsupported
mapping or D/T modes still defer. The prefix hands off at `4F6C`; the existing
countdown owner now also handles zero expiry through `4F81`, clearing the frame
and marking the actor dirty before yielding at the `4F83` child call. Child calls
and core RTS handling are unchanged. Both mappings and D/T mode are guarded.

`dc_eyeball_animation_tests` covers 131,328 timer/frame/borrow cases. The
3600-frame `dc_rom_eyeball_animation` fixture reaches 224/224 native calls and
compares census, enclosing calls, ordered RAM writes and all device writes
against assembly. The older final-wait test retains all four artifact hashes
and its 102-cycle routine-replay check; only admission rises from 122 to 224.

## Compound-sampled `BBRn` boundaries

`00:F423`, `00:FA67`, `01:C6E3`, and `01:C756` are HuC6280 `BBRn` instructions
whose zero-page read and relative branch are sampled together. Collapsing the
pair into one native checkpoint shifts VDC event handling.

The core therefore executes each of these instructions itself and re-enters
native code on both proven arms — for `00:FA67`, all 832 instances, resuming at
`FA6A` or `FA6E`. The outer `00:FAA5` branch is retained as the row-transaction
boundary for the same reason.

This is a deliberate hardware-timing seam, not missing semantic coverage.
Moving `01:C6E3` would require a bridge checkpoint that preserves
the core's four-step event sampling; an ordinary two-boundary transcript is not
sufficient, and it is not needed to keep porting adjacent gameplay code.

## VDC word-stream loop boundaries

The ordinary word-stream uploader at `00:E365-E39A` does not need a block
transfer capability. It is now native in 32-word segments at its existing
`E37E` loop head. The closing branch remains inside each transcript, with its
original interrupt checkpoint; the final `E39C` RTS remains in the core. Live
MPR source resolution, Y wrap/source-page carry, and 16-bit counter borrowing
are preserved rather than inferred from the 64-word natural fixture.

The adjacent address-fill loop `00:E34C` now owns its `E362` back-edge too.
Its previous auxiliary installation has been replaced by an admitted registry
row, not left installed alongside it. Whole-route RAM and VDC bus traces
prove both changes without enlarging the write or checkpoint channels.

Unlike the compound-sampled `BBRn` seams above, these are ordinary BNE
instructions with a single interrupt-sample point. Lack of a subroutine return
inside a large transfer was a chunking constraint, not a reason to retain the
whole loop in assembly.

The strided row uploader at `00:EF2A-EF6C` now uses that same real-loop
boundary. Each native segment writes 32 words, advances the source by 64
bytes and the destination by 64 **words**, decrements X (zero means 256
rows), and includes the closing BNE checkpoint. Only `EF6E` RTS remains
core-owned. Four nested row arrivals with one enclosing return are no longer
a reason to retain the body: each row fits 74 writes and 283 checkpoints.
VDC stores retain their stolen cycle and early interrupt sample; device source
reads decline the whole row. The 1,800-frame assembly/native replay compares
every RAM/VDC write and row/return boundary, while direct fixtures cover RAM
aliases, MPR crossings, carries, count rollover and a complete 256-row stream.

## Selection actor input and glyph handoffs

The initializer at `08:5A2E` already owns its first glyph and coroutine
schedule. Its recurring `5A3E` callback and `5A6A/5A7A/5A86` glyph controller
are now admitted separately. The callback snapshots the old choice, handles
both directions and their wrap rules, and stops after the original `5A66`
JSR. Erasing the old glyph stops after `5A77`'s JSR; drawing the new glyph
ends at the tail JMP into the existing `5B01` writer. The `5A69` RTS remains
core-owned. Neither the VDC writer nor the coroutine scheduler is folded into
these segments, and their interrupted return handling is unchanged.

The earlier neutral-only recording did not exercise a selection change.
Admission now includes two real-joypad replays, with both wrap directions,
normal/wide glyph layout and address carries, comparing each frame's complete
RAM, video memory, pixels, audio, ordered RAM/VDC writes and child boundaries
against assembly. This replaces missing verification with a reproducible
controller-level test rather than treating the handoffs as permanent barriers.

## Bank-09 parent animation startup

`09:705C-70C0` now uses seven bounded native entries for the descending,
holding and ascending phases. Calls into `F08E`, `ECFD`, and `590C` remain
independent; `C5C9` consumes its actual JSR frame and installs the next actor
continuation. `709E` restores X from the live post-child stack, not from the
parent's entry snapshot. The already-native `70C1` score-panel scheduler
handoff and `70C4` frame stepper are unchanged. The three ordinary returns at
`7071`, `7099`, and `70BB` stay core-owned.

Natural gameplay and alternate routes compare complete frame state, all 92
enclosing calls and their ordered writes, and the `0000-1FFF` device-write
stream. A controlled final-counter fixture covers the ascending phase's audio
handoff, which natural startup does not reach before changing scenes. Direct
tests exhaust counter pairs and all stack pointers. The ordered-write oracle
caught an initial PHX timing error: unlike JSR, PHX publishes its write at
instruction completion. No bridge capacity or scheduler semantics changed.

The sibling `09:7194-71E6` now has five native entry/return boundaries:
`7194`, `7197`, `71BC`, `71BF`, and `71CD`. Its byte-identical descending and
ascending phases share the semantic implementation with relocated checkpoint
identities and JSR return bytes. Its shorter hold phase is explicit: it has
neither the other parent's audio comparison nor its PHX/child/PLX sequence.
The final `71E7` scheduler call and existing `71EA` frame stepper are unchanged;
the three ordinary RTS instructions at `71A9`, `71C4`, and `71E1` stay core-owned.

Natural gameplay and alternate replays each cover 92 enclosing calls. A
controlled frame-166 timer/frame patch to the actual X=06 actor reaches the
final audio/scheduler handoff. All three routes compare complete census,
enclosing call records, ordered RAM writes, and the entire device-write stream.
The combined direct suite now checks 329,472 cases, including all sibling
timer/frame pairs and every JSR stack pointer. No ROM, scheduler, bridge,
or prior sorcerer crash fix was altered.

## Selective scene-record loader

`02:40A8` and the real child-return continuation `02:40BD` now own the
category-1 scan, pointer carry, skipped records, and both original JSRs.
The actor loader at `40F6` and finalizer at `C08A` remain independent children;
`40CE` RTS remains core-owned. A missing local return was a decomposition
constraint, not a reason to retain the entire asset transaction. Each segment
stops at a child entry, staging the actual wrapping JSR stack writes. A scan
over 100 records declines atomically, below the existing bridge capacities.

Two unpatched gameplay routes and the seeded drain fixture compare frame
state, enclosing call/write artifacts and the complete VDC write stream with
assembly. Direct fixtures cover all pointer low bytes and stack pointers for
selected, skipped and terminal records, including CLA carry preservation.

## Audio command bank-call boundary

`00:ED58` now owns PHX, all three mapper-shadow saves, the consecutive-bank
mapping, and the real `JSR E262`. `00:ED75` owns the live-stack restoration of
MPR4/MPR3/MPR2 and X. The audio driver remains an independent child and `ED85`
RTS stays core-owned. The earlier description of `ED5E` as an unusual entry
was misleading: it is an interior PHA; the actual wrapper starts at `ED58`.
No partially constructed caller frame needs to be guessed.

The existing 3,600-frame bank-wrapper regression now covers both entries,
without changing its assembly census or ordered stack/mapper-write goldens.
An additional joypad replay compares every frame's audio, pixels, RAM, video
memory, ordered RAM/VDC writes and restored register/MPR boundary, with the
wrapper isolated and composed with the existing bank-call family. Direct
fixtures cover all 65,536 combinations of stack pointer and command bank,
including wrapping pushes/pops and bank increments. The `ED4D-ED57` command
selector prefix remains core-owned; it is not a JSR instruction at `ED4D`.

## Audio stack wrapper

The separate audio wrapper at `45:4B7B` now owns PHP/PHX/PHY, SEI and
the real `JSR 4BCC`. Entry `4B82` restores Y and X from the live stack,
including stack-pointer wrap. The child stays independently owned. PHP
writes P with B set at cycle 9; the JSR pushes its return bytes at cycle 33,
before charging its 21 cycles. SEI samples the pre-instruction interrupt mask.

`4B84` PLP and `4B85` RTS remain interpreted. The generic checkpoint format
cannot describe PLP sampling I set before restoring I clear. The fixed native
PLP descriptor currently admits only VBlank epilogues; it must not be claimed
as deployed at this address. An initial generic PLP implementation passed an
800-frame menu replay but changed gameplay RAM/audio at frame 1064. The
corrected boundary passes a 1,800-frame menu/input replay plus exact gameplay
census, enclosing calls, ordered stack writes and full hardware-write stream.
Direct fixtures cover 16,384 supported stack/status combinations and D/T,
short-RAM and mapper rejection. No existing port or crash fix is reverted.

## Music sequence-end nonlocal exit

The command interpreter's `$82/$92` handlers at `45:4356` no longer force
the whole enclosing tick back to assembly. The missing behavior was not an
ordinary return: STZ deactivates the voice, four PLAs discard the dispatch
and interpreter frames, and JMP transfers to `40EE`. The native walk now
records each pop and the final jump, then ends there regardless of its
internal call depth. It does not absorb an RTS, guess a caller return address,
or continue with an invalid nesting count. The `$86` variant at `4350` also
publishes the voice's value before taking the same exit.

The existing whole-music regression retains all assembly goldens and now
accepts all 3,599 tick calls. An independent 3,600-frame replay keeps coverage
of the child entries now inlined by that tick: all 32,391 `40EE` boundary
snapshots, 12,647,397 ordered RAM writes, 699,251 PSG/timer writes and frame
states match assembly. Direct tests cover 196,608 combinations of the three
commands, all X and stack-pointer values, wrapping/live stack reads, flags,
write timing and interrupt checkpoints. Unsupported command paths and the
existing driver-state/SEI interrupt guards remain in place.

## Session input gate

`01:C49E` now retires its session-bit early exit and active Run-input
branch, including the `C4AE` JSR into `00:ED37`. The pause audio child and
subsequent frame-wait loop remain independently owned; the new early exit
stops at `C4C8`. The existing idle branch is unchanged. New paths require
the expected RAM/code mappings and clear D/T flags. Each instruction has
an interrupt checkpoint, and JSR writes the wrapped live stack at its
instruction-start cycle. No new registry entry is needed.

`dc_session_gate_active_tests` covers the flag/input combinations, indexed
RAM reads, stack wrapping, cycle/flag preservation and rejected mappings.
`dc_rom_session_gate_active` compares isolated replacement and assembly
census, ordered call writes and full device-bus writes on natural gameplay,
a forced session-bit exit, and a forced pause/resume path. Fixtures assert
that the new paths actually execute; a quiet idle-only replay is insufficient.

## Nudge controller locks and timer reload

`01:C3CB` now owns its session-bit and tilt-lock exits, including the live
`C3DC` return, and active start/finish paths when the nudge timer expires.
The reload clears the counter before the active-input test; it is not valid
to use the entry counter to decide whether to tilt. These branches extend
the existing controller rather than creating instruction-sized entries.

Controlled assembly/native replay of timer reloads exposed two older active
path errors: CLI must retain the old interrupt mask until the next checkpoint,
and CLX preserves flags. Both are corrected. SEI now declares its pre-mask
sample, and VDC writes retain their stolen-cycle sample offset. Rejected
active models no longer fall through to an idle return. Unsupported active
states still defer; this does not change the bridge or prior crash fixes.

The lanes registry baseline changes only at frames 1461-1464 and 1469
(RAM at 1461, audio at all five). The corrected full registry matches a
fresh run with only C3CB left in assembly. `dc_rom_nudge_registry_parity`
repeats that comparison including ordered stack and VDC/PSG writes. This
is evidence for correcting the old baseline, not proof of full-registry
assembly parity; the separate pre-existing strict registry failure remains.

## Fixed overlapping `TII` transfers

The 4,095-write boot clear at `08:585A` and the 1,023-write palette-shadow
clear at `00:E950` cannot use the ordinary 512-write transaction channel, but
they are no longer interpreter-owned: compact descriptors hand their original
overlapping moves back to the core's resumable opcode loop.

The compact capabilities are intentionally limited to fixed, verified opcode
descriptors; a generic block-transfer replacement would need a wider bridge
contract.

## Giant-Face active-wait return at `03:76CA`

`03:76C5` is native for its safe, one-write active-animation wait, but its
nonzero-timer path reaches the following `RTS` at `03:76CA`. An exact-core
instruction candidate for that return was tested in live play and changed the
return interaction: the ball accelerated erratically, eventually left the
visible table, and gameplay slowed sharply. The instruction is inexpensive,
so there is no reason to trade correctness for its small residue contribution.

Keep `03:76CA` interpreter-owned. Revisit it only with a transcript model of
the suspended caller/return interaction and a long interactive validation;
do not re-add it as a standalone exact-RTS capability.

## Timer-horizon fallbacks in audio-visible routines

Two routines fall back to assembly when a serviceable timer IRQ is close
enough that native execution would move a PSG write. Both were found by the
`twist` route's audio-only drift (see
[ROUTE_SCREENING.md](ROUTE_SCREENING.md)).

| Routine | Guard | Effect |
| --- | --- | --- |
| `02:570A` (Big-Star idle) | Serviceable timer-IRQ horizon within its 81-cycle body | 280 no-interrupt calls native, 10 interrupt-bearing calls to the core |
| `03:66A3` (Three-Blobs first-movement prefix) | Timer horizon of 300 master cycles or less | 998 of 1,107 calls native, 109 to the core |

Do not widen either admission into its timer-sensitive path without an exact
device-write transcript. With the guards in place the deployed 3,600-frame
recording is byte-identical to assembly for audio, work RAM, and video.

## Write-capacity boundaries

The bridge carries 512 ordered writes per transaction; the checkpoint capacity
is separate and is 2,048 for a bounded parent.

- `01:D466` has two whole-parent fallbacks. Their current-slot paths enter
  `D525` at frames 1,820 and 2,255 and emit 823 and 868 writes. These belong to
  the `D525` decomposition boundary, not to an unresolved `D466` branch. The
  bounded `D525-D594` prefix is already native; resolving the two parents means
  decomposing the remaining callee-owned segments below 512 ordered writes, not
  widening that prefix.

## Interrupt-bearing routines needing their own checkpoints

Faithful checkpointed mid-routine interrupt delivery exists and is proven by
all 540 `01:D081` calls, so interrupts are no longer a blanket blocker. Each
routine still needs an exact instruction-boundary transcript for the semantic
branch being implemented.

Still emulated for want of one:

- `03:6966@11` (enters via `03:69A2` / `00:FC26`) and `03:6966@13` (shifts
  interrupt-owned stores from about `03:69B7`).
- `03:6177`, the Undead-Head launch consumer. It is dynamically bounded through
  fixed slot `$4D`, but its 5,778-cycle combined-route scope contains a
  periodic interrupt: RNG-owned writes are deterministic while normalized
  replay still detects shifted interrupt-owned writes. Keep it out of
  replacement coverage until its native semantic path supplies checkpoints, or
  a separately proven no-interrupt invocation is reproduced.
- `00:E807`, the low-byte helper adjacent to the signed multiply wrapper. No
  deterministic route enters it, so it remains emulated until a natural caller
  is reproduced. The two signed-word normalizers beside it, `00:E813` and
  `00:E825`, are now native: player-session-6 naturally reaches each 2,422
  times through `00:E5EC`, including the wrapper states that return to the
  core. Their neighbour `00:E85D` was in the same position until a
  player-session capture reached it — see [ROUTE_SCREENING.md](ROUTE_SCREENING.md).

Already resolved, and recorded here because the pattern recurs: `01:DD09@52`
showed that interrupt-owned stack and controller writes can enter through
`00:E058/E169` even when a natural trace never visits the catalogued
`FCxx/FDxx` handler range. Semantic `00:E905@all` and its `01:DD09@all` parent
supply those checkpoints, so every observed damping call is native.
`01:D09E` (all 540 calls) and `00:E65C` (all 3,700 calls) are likewise fully
native, with their wide observed cycle ranges explained by original interrupt
handlers retained through composed checkpoints.

## Defensive fallbacks that are meant to fire

These are not faults. Routes reproduce them deterministically in both
configurations, and the resulting recording stays byte-identical to a fully
emulated run.

- `00:E241 -> 00:E250` and `00:E255 -> 00:E25F`: the banked-call epilogue
  abandons on an MPR mismatch and resumes in the core. Keep them covered.

## The screening horizon

The scene-selection controller at `08:56D3-577E` is no longer retained as one
enclosing bulk-upload/scheduler transaction. It now has bounded native segments
at its actual child calls and return addresses, including all three input loops
and choice transitions. The child bodies and frame waits keep their existing
owners. This resolves the old transaction-size objection without increasing
bridge capacity or predicting input across frames. The adjacent bootstrap and
countdown at `08:55A6-56D0` now use the same real-boundary model, including VDC
setup, attract-loop input, actor creation, timeout and restart. Its children
still run independently; no bulk upload or frame wait was absorbed. See the
measured evidence in [NATIVE_PORT_WORKFLOW.md](NATIVE_PORT_WORKFLOW.md).

The canonical 3,600-frame route has no further standalone table-rule entry and
return contract to admit. Its filtered residue — 3,228,057 unregistered
non-wait cycles in 238 clusters as of the 2026-08-22 re-rank — consists only of
the device, block-transfer, compound-branch, setup/scheduler, music, and
caller-owned continuation seams above. The remaining bank-`02`/`03` table rows
in the profiled tail are all already-classified actor-loader, scheduler,
collision, appearance, allocator, or caller-return interiors.

This historical screen is a prioritization horizon, not an external blocker.
Real child-call and return boundaries are valid controller seams when exact
state, ordered writes and timing prove the handoff, as the scene controllers
above demonstrate. Avoid arbitrary instruction-sized splits merely to increase
the entry count. Use measured deployed residue and new boundary evidence, or
an additional route/capture when existing routes do not reach the work. See
[ROUTE_SCREENING.md](ROUTE_SCREENING.md).

## Cabinet-side deferrals

- The DMD P1/P2 discriminator is deferred at the user's request. When it
  resumes it still needs a two-player route that distinguishes the actual
  player-count and current-player fields from `$2251`'s already-proven
  score-array selection role.
- `$F768` selectors 1-4 remain a future scene-mapping question. No unresolved
  sprite-pattern source blocks normal-table fixed-actor composition.
- The longer state route has a configuration-specific Beetle framebuffer
  variance from frame 2,147, and the maximum-score route has a second
  presentation-only variance at frames 1,555-1,680. In both cases work RAM,
  audio, ordered writes, and semantic traces are identical, so
  configuration-specific census hashes guard them. Each must be explained or
  normalized before that route can become a single cross-configuration
  framebuffer golden.
