# Project Progress

Last updated: 2026-07-30

## Current Status

**Phase:** Milestone 4 native gameplay port

**Current milestone:** 4. Native gameplay port

**Milestone 4 current gate:** native collision-table work is now measured by
semantic and dynamic-call coverage rather than an ordinal count.
`--replace-hook 03:6966@all` reads every selector pointer from the verified
user ROM and handles both the four-write alternate-pass early return and
ordinary headerless record steps for any selector and actor slot. The record
walker resolves the collision-list cursor, motion pointer, and motion cursor
from ROM, then emits one- or two-axis signed movement with exact carry/borrow
timing. The four-write alternate-pass return now supplies all 17 original
instruction boundaries, allowing Beetle to execute and return from original
interrupt handlers inside that native transaction. Other paths still use
core-provided master-cycle horizons to fall back before a user/video event or
serviceable timer interrupt. Exact checkpointing now covers both one-axis
records and two-axis records that do not carry or borrow into a position
high byte. The deterministic 1,800-frame route now executes 18,806 of 19,648
`03:6966` calls natively (95.7%) while
preserving the byte-identical gameplay artifact, complete 19,648-call trace,
and all 180,389 ordered writes.

Application orchestration is now separated from native transaction
synthesis. The replacement adapters and central dispatcher moved from
`src/app/main.cpp` behind the single public
`app/native_replacements.hpp` entry point. `main.cpp` fell from 16,939
lines/630 KB to 5,077 lines while retaining command-line, recording, replay,
cabinet, and runtime ownership. Collision, physics/arithmetic,
controls/actor, and RNG/score synthesis now occupy four subsystem
implementation units behind a private internal interface;
`native_replacements.cpp` is the 234-line central registry. The extracted
modules have no SDL, cabinet, recording-plan, or replay-capture dependency,
and `main.cpp` no longer includes the game-logic C headers directly.

Work has moved off the unusually large collision-table interpreter.
Semantic `00:F1B4@all` now derives position-integration sign branches,
comparison exits, flags, writes, and timing from live slot-zero state,
executing all 1,156 calls through path-sensitive 50-57-boundary transcripts,
including 250 original interrupt-bearing scopes. The adjacent state-generic
`00:F241@all` rollback executes all 404 calls through a 12-boundary
transcript. The complete 1,560-call position pipeline is native while both
routines retain exact gameplay and complete call/write artifacts.

Semantic `03:7FB1@all` now replaces the actor collision-response helper by
live slot state rather than ordinal. It derives response, countdown,
reload/phase advancement, result flags, three-or-five-write transcript, and
126/162-cycle timing for any slot. Its 11/14-boundary transcripts now admit
all 21,987 calls, including 1,042 calls with original interrupt-handler
passages; the state census and complete 21,987-call/91,295-write artifacts
remain byte-identical in Debug and Release.

Semantic `03:7F57@all` now composes those collision primitives at the
Small-Soldier consumer boundary. For a live no-hit state it invokes any
admitted semantic `03:6966` transaction, stages those writes into a private
snapshot, invokes either semantic `03:7FB1` branch from the resulting state,
and finally reproduces the caller's collision-byte test. Write offsets and
body timing are derived from the two child transactions without an ordinal
table. The alternate-pass, one-axis, and bounded two-axis headerless shapes
compose their children's checkpoints with both parent JSR/RTS boundaries
and the final collision test, producing 35/38-, 68/71-, or
73/76-boundary parent transcripts. This admits 20,113 of 20,867 calls
(96.4%); the complete
20,867-call/362,060-write artifacts remain byte-identical in Debug and
Release.

Semantic `00:F55C@all` now covers the central ordinary score-addition path:
it selects the live score slot, adds the 16-bit award to the 24-bit stored
score, proves the unsaturated/below-high-score exits, reproduces both nested
JSR stack writes and the presentation-dirty write, and returns with exact
flags after 558 master cycles. Its 44-boundary transcript admits all 40
census calls, including the ten calls whose original scopes contain an
interrupt-handler passage; the complete 40-call/446-write score artifacts
remain byte-identical in Debug and Release.

Semantic `00:E8B3@all` now covers the shared RNG update. It derives all six
scratch writes, both final seed writes, returned high byte, arithmetic
flags, and the fixed 282-cycle budget from the live seed and frame counter.
Its 23-boundary transcript admits all 78 census calls; the state census,
complete 78-call/703-write routine artifacts, and 3,771 selected seed writes
remain byte-identical in Debug and Release.

Semantic `00:F4FC@all` now covers the shared actor camera-window helper.
From live occupancy, signed vertical position, camera anchor, and flag state,
it derives the empty, below-window, inside-window, and above-window exits.
Its 2/18/31-boundary transcripts reproduce the exact zero-, three-, or
five-write scratch/flag transaction over 27-333 body cycles. All 65,927
census calls now execute natively, including 4,858 calls with original
interrupt-handler passages; the state census and complete
65,927-call/283,956-write artifacts remain byte-identical in Debug and
Release.

Semantic `01:C574@all` now covers the generic actor-slot reset. It preserves
Y through the emulated stack, swaps the requested slot into the indirect
index, and clears all 32 parallel fields while advancing `$2D:$2E` by `$80`.
One generated 361-boundary loop transcript reproduces all 99 writes and the
3,627-cycle body for any legal slot. All 132 census calls now execute
natively, including 35 calls with original interrupt-handler passages; the
state census and complete 132-call/14,163-write artifacts remain
byte-identical in Debug and Release.

Semantic `01:D466@all` now covers the high-frequency flipper/control-piece
input gate. It intersects scheduler input `$2CEE` with the indexed piece mask
at `$3B80,X` and reproduces both the 36-cycle, zero-write no-input return and
the common 282-cycle activation/coroutine handoff. The latter marks the piece
active, reproduces the nested JSR stack/scratch transcript, and installs
dispatch `01:D48E` with nine exact writes. The zero-input exit now supplies
three instruction checkpoints and the activation/coroutine handoff supplies
21, admitting 7,468 of 7,470 calls (99.97%). The complete
7,470-call/5,684-write artifacts remain byte-identical in Debug and Release.
The remaining frame-1,820 and frame-2,255 current-slot calls enter `D525`
and produce 823 and 868 ordered writes, respectively, beyond the bridge's
independent 512-write limit. They and held/release response variants remain
in assembly.

Semantic `01:D500@all` now covers the flipper controller's shared phase
step. From indexed countdown, reload, orientation, and phase state it
reproduces the timer-zero write, timer reload, and forward or reverse phase
write. The reverse branch returns after 126 cycles and the forward branch
after 138, with exact direction-specific accumulator and flag behavior.
The reverse/forward paths now supply 9/11 instruction checkpoints and admit
all 980 route calls; all 980 calls and 3,613 writes remain byte-identical in
Debug and Release. Endpoint and non-expiring timer states remain available
as assembly fallback outside this route.

Semantic `01:D081@all` now covers the flipper/ball interaction's shared
geometry lookup. It masks live state `$78` to five phase bits, indexes the
two verified vector tables at `01:D2AE` and `01:D2EE`, preserves Y through
the emulated stack, and writes both 16-bit vectors to `$7F/$80` and
`$7D/$7E`. The replacement now supplies all 14 original instruction
boundaries, including each interrupt-sampling offset and complete CPU/MPR
state. Beetle pauses the native transcript to execute original ROM interrupt
handlers and resumes it after a state-matching `RTI`. This admits all 540
calls, including 38 interrupt-bearing calls that retain their longer handler
transcripts; the complete 540-call/3,214-write artifacts remain
byte-identical in Debug and Release. The downstream `D09E` projection is now
native as a composed transaction; the larger `D525` flipper/ball controller
remains in assembly.

The downstream HuC6280 bridge now supports optional resumable replacement
transcripts. It commits ordered writes only as their checkpoints are reached,
samples IRQ state at the original instruction's last-cycle boundary, and
serializes pending writes/checkpoints in savestates. If an interrupt handler
returns without restoring the expected registers or MPRs, Beetle abandons the
remaining transcript at that original instruction and continues in assembly.
Atomic replacements without checkpoints retain their conservative event/IRQ
horizon policy.

Semantic `01:C5C9@all` now covers the shared actor-coroutine scheduler used
by flippers, collision consumers, and table actors. It consumes the stacked
JSR return, derives the next dispatch address, decrements indexed delay
`$3D80,X` when active, and either yields or installs `$3F00,X/$3F80,X`.
All immediate, delayed-yield, delayed-install, and return-page-carry shapes
are state-derived rather than caller- or ordinal-specific. Branch-sensitive
8- or 12-15-checkpoint transcripts now admit all 2,462 calls, including the
925 calls previously rejected by the conservative event/IRQ horizon. All
2,462 calls and 863,105 writes remain byte-identical in Debug and Release.

Semantic `01:C3CB@all` now covers the adjacent session/nudge input gate's
session-state, ordinary idle, timer-reload, active-nudge start, and
phase-finish branches. The three starts initialize the signed nudge vector,
apply it to the slot-zero motion pair, advance phase, and program the base
VDC ports; the three finishes apply the live vector, clear phase, and restore
the VDC write mode. Their 66/44-boundary, 762/522-cycle transcripts use a
strict physical-`$FF`, logical-`$0000-$0003` bus-write allowance whose
executor retains each VDC wait cycle. Together with the existing
8/17/20-boundary returns, all 2,649 calls now execute natively while the
complete 2,649-call/5,635-work-write artifacts and state census remain
byte-identical in Debug and Release.

Semantic `01:C49E@all` now covers the following pause/high-score session
gate's ordinary idle return. It verifies session eligibility, resolves the
active player through `$2251`, tests that player's Run edge at `$2018,Y`,
and reproduces the returned `A/Y/N/Z` state after 72 cycles and no writes.
Its seven-boundary transcript admits all 2,649 calls, including the 933
previously rejected by the event/IRQ horizon, while the complete
2,649-call/1,077-write artifacts remain byte-identical in Debug and Release.
Pause, high-score, alternate-session, and initialization paths remain
available as assembly fallback outside this route.

Semantic `00:E837@all` now covers the shared signed 16-bit scratch-word
negation used by the flipper projection and rotation paths. It derives the
complement, low-byte increment, optional high-byte carry, accumulator,
flags, writes, and 90/102-cycle timing from live `$2F:$30`. Its 8/9-boundary
transcripts admit all 894 route calls, including 24 original
interrupt-handler passages; the complete 894-call/3,122-write artifacts
remain byte-identical in Debug and Release. The adjacent `01:D09E`
projection's 5,328-33,471-cycle scopes are now fully composed from this leaf
and semantic signed multiplication.

Semantic `00:E624@all` now covers the fixed eight-bit unsigned multiply
round inside the shared signed fixed-point pipeline. It derives all eight
shift/admission decisions, 16-bit additions, carry propagation into `$30`,
flags, writes, and `600 + 84 * admitted-bit-count` timing from live
`$42:$43/$30/$2D:$2E`. Its generated 41-113-boundary transcripts admit all
5,371 route calls, including 1,592 original interrupt-handler passages; the
complete 5,371-call/181,868-write artifacts remain byte-identical in Debug
and Release. Interrupt-bearing ordinal 1 and interrupt-free ordinal 2 also
shadow restored assembly exactly.

Semantic `00:E65C@all` now composes that child into the shared signed 8.8
multiply wrapper. It derives both sign normalizations, the optional
high-byte round, mandatory low-byte round, fixed-point result shift, and
final sign restoration from live `$2D:$2E/$2F:$30`. Its generated
path-sensitive transcript includes the original nested stack transactions
and one or two complete `E624` child transcripts. All 3,700 calls execute
natively, including 2,157 calls with original handler writes; the complete
3,700-call/244,531-write artifacts remain byte-identical in Debug and
Release. Interrupt-free ordinal 3 independently shadows its 921-cycle,
35-write path against restored assembly.

Semantic `01:D09E@all` now composes four signed multiplications and one
negation into the indexed flipper vector projection. It derives both
`$C0:$C1` and `$BE:$BF` outputs from live motion and coefficient state,
staging every child result before the next operation. The replacement
checkpoint capacity is now a fixed 2,048 while the write capacity remains
512, admitting all five nested child transcripts without weakening either
bound. All 540 calls execute natively, including 448 calls with original
handler writes; the complete 540-call/168,532-write artifacts remain
byte-identical in Debug and Release. Interrupt-free ordinal 16 independently
shadows its 5,328-cycle, 194-write path against restored assembly.

Semantic `01:D1A2@all` now replaces the adjacent signed two-dimensional
rotation from live `$C0:$C1`, `$BE:$BF`, and coefficient state. The host
composes four semantic `E65C` children and one semantic `E837` child with
the exact parent scratch, indexed stores, additions, stack transactions,
and instruction checkpoints. All 354 current route calls execute natively,
including 296 calls with original handler writes; the complete
354-call/103,362-write artifacts remain byte-identical in Debug and Release.
The aggregate regression independently replays interrupt-free ordinal 23 at
frame 1,241 with 7,584 cycles and 273 writes.

Semantic `01:D612@all` now replaces the flipper-radius projection used by
the `D525` controller. It derives the signed 8.8 product of `$31:$32` and
`$7D:$7E`, composes one complete semantic `E65C` child, and preserves every
parent scratch, result, stack, and checkpoint boundary. All 62 calls execute
natively, including 51 calls with original handler writes; the complete
62-call/6,271-write artifacts remain byte-identical in Debug and Release.
Interrupt-free ordinal 36 independently shadows its frame-2,498,
2,079-cycle, 77-write path against restored assembly.

Semantic `01:D62E@all` now replaces the paired directional-angle projection.
It selects the direct or wrapped angle delta from indexed flipper state,
converts that byte to signed 8.8, composes one semantic `E65C` child, and
stores or negates the result into `$31:$32`. All 62 calls execute natively,
including 43 calls with original handler writes; the complete
62-call/5,062-write artifacts remain byte-identical in Debug and Release.
Interrupt-free ordinal 1 independently shadows its frame-1,268,
1,407-cycle, 51-write path against restored assembly.

Semantic `01:CFD8@all` now replaces the mode-selected collision-motion
adjustment used by `D525`. It covers positive and negative clamps plus signed
3/16, 1/4, 1/2, and 1/16 scaling with exact shift, carry, scratch, and flag
behavior. All 354 calls execute natively, including 41 calls with original
handler writes; the complete 354-call/4,103-write artifacts remain
byte-identical in Debug and Release. Interrupt-free ordinal 29 independently
shadows its frame-1,297, 102-cycle, two-write half-scale path.

Semantic `00:E905@all` now replaces the shared signed divider. It preserves
the inherited carry entering the initial dividend rotate, normalizes signed
divisor and dividend magnitudes, runs the fixed 16-round unsigned core, and
restores the quotient sign. All 752 calls execute natively, including 583
calls with original handler writes; the complete 752-call/52,804-write
artifacts remain byte-identical in Debug and Release. Interrupt-free ordinal
516 independently shadows its frame-1,564, 2,037-cycle, 59-write path.

Semantic `01:DD09@all` now composes that divider for each nonzero motion
component before subtracting the signed quotient and applying `$8B/256`
acceleration to the second component. All 753 calls execute natively,
including the 669 calls previously deferred for interrupt delivery; the
complete 753-call/61,291-write artifacts remain byte-identical in Debug and
Release. The existing ordinal-181 replay retains exact frame-1,228,
2,427-cycle, 69-write negative sign-restoration parity.

The larger `01:D525` controller was re-profiled after its arithmetic leaves
became native. Its 128 census calls still span 1,035-49,122 body master
cycles and 29-1,456 ordered writes, with several external tail exits. Since
the largest live paths exceed the bridge's independent 512-write envelope,
`D525` remains a decomposition boundary rather than one semantic
transaction.

Semantic `01:CC1A@all` now covers the central collision scan's earliest
vertical-separation rejection. A pure state classifier distinguishes
positive-page rejection, same-page low-byte rejection, negative-page
rejection, and negative low-byte-magnitude rejection. The host reproduces
their exact internal JSR stack writes, `$2D/$2E/$2F/$30` scratch writes,
binary subtraction/comparison flags, and return checkpoints over
150/180/246/282 base master cycles. This admits 19,192 of 85,715 calls
(22.4%); near-target, pointer, mask, and contact paths continue in assembly.
The complete 85,715-call/1,784,119-write artifacts remain byte-identical in
Debug and Release, with call SHA-256
`c01321d866af5636ce12db1174a9a3428d08ce273cf62820fcbf446dcab1a313`
and write SHA-256
`a0366616f0bbd85b22d5a4a72e8d03e09a8866525c4c90d50c51e2ea7d035881`.

Semantic `01:CCD5@all` now covers the near-target collision-geometry leaf
reached by the remaining `CC1A` calls. It resolves both ROM pointer
directories through the live MPR map, selects the phase descriptor, and
reproduces the signed offset, bounds, scratch pointers, stack write, flags,
and all four 783/789/885/891-cycle shapes. Its path-sensitive instruction
transcript replaces all 66,523 calls, including the previously deferred
interrupt-bearing scopes; all 66,523 calls and 1,414,954 writes remain
byte-identical in Debug and Release.

Twenty-one previously admitted `03:6966` ordinals remain as focused
restored-assembly fixtures for full records, continuations, and early
returns; they are no longer the scheduler's unit of progress. Alongside
them, `03:7FB1@1`, `@3`, and `@7` remain focused fixtures for the semantic
replacement's decrement and reload branches. `01:D1A2@23`
performs signed two-dimensional rotation in 5,076 master cycles with 190
ordered writes, `01:DD09@1` takes its zero-component acceleration path in
168 cycles with two writes, `01:DD09@2` takes its positive nonzero
second-component divider path in 2,409 cycles with 68 writes, and
`01:DD09@181` takes the negative divider/sign-restoration path in 2,427
cycles with 69 writes. `00:F1B4@1`
performs nonnegative fractional-only position integration in 702 cycles
with 16 writes, `00:F1B4@10` takes the integer-step early return in 639
cycles with 15 writes,
`00:F1B4@17` takes its negative-second-increment path in 714 cycles with 17
writes, and `00:F241@1` performs collision rollback in 162 cycles with six
writes. The 32 focused fixtures still shadow restored assembly from
identical pre-call states with 553 exact writes and 21,327 combined body
master cycles. Unimplemented semantic branches stay emulated while the
shared interpreter is generalized. Replayed `03:7FB1@2` contains a periodic
interrupt and is now admitted through the checkpointed aggregate helper.
The former `01:DD09@52` blocker is now covered by the semantic parent and
child checkpoints, including its interrupt entry through `00:E058/E169`.

**Deferred Milestone 3 gate:** all non-hidden fixed actors, all three flipper
pairs, ten Small Soldiers, four Tower insects, and the mapped type-`$8F`
impact-burst family now populate and animate across the stitched table while
the ball remains exact and trail-free through the live SAT. Exact ROM groups
now replace proximity association, all six flippers survive every camera
region, and off-camera Sorcerer movement and all 16 walking phases are
guarded. Moving enemies now resolve or restore the visual matching their
live phase instead of sliding a frozen fallback image; unrelated
camera-region VRAM is never sampled. Retained actor Y is now normalized past
the 30-line score band and retained colors follow the live VCE palette, so
launching the ball no longer makes mechanisms jump or exposes dark
pre-fade flipper silhouettes. Two-player/DMD work is deferred again at the
user's request; cabinet playfield work remains active.

**Milestone 2 progress:** complete; all four closure gates are closed

No whole-project percentage is reported yet. The remaining milestones have
not been decomposed finely enough for a single aggregate number to be useful.

### Milestone 2 closure checklist

Milestone 2 no longer uses a percentage estimate. The previous weighted
estimate mixed completed architecture work with an expanding set of
reverse-engineering discoveries, so its denominator was not stable. The
milestone closes when these four evidence-based gates are complete:

| Gate | Existing evidence | Required closure evidence | Status |
| --- | --- | --- | --- |
| Normal-table controls and physics | One 3,600-frame route connects Button-I Devil-Shooter charge/release, right/left flipper state changes, and three Button-II nudges to slot-0 motion and later integration; `$2CC7` remains clear throughout | Deterministic combined route, ROM gates, complete work-RAM timeline, ordered calls, and selected writes are guarded by `dc_rom_controls_physics` | Closed |
| Normal-table collision boundary | Natural Small-Soldier contacts at frames 1,479 and 1,852 connect `01:CBCF` and the type-`$82` ball response through `03:6966`/`03:7FB1` to actor state, score, command `$21`, `$2D16`, and complete actor release | ROM gates, complete work-RAM timeline, 97,379 ordered calls, 1,163,259 scoped writes, and selected state writes are guarded by `dc_rom_collision_boundary` | Closed |
| Normal-table session and scoring boundary | One route connects the frame-3,000 live-ball drain, `$2D52` decrement, ordinary continuation decision, category-1 reload, 1,020-unit stored score, dirty-panel handoff, and remaining-ball redraw | ROM gates, complete work-RAM timeline, 402 ordered calls, 27,769 scoped writes, and exact score/ball glyph streams are guarded by `dc_rom_session_scoring` | Closed |
| Coverage manifest and scope freeze | `docs/COVERAGE_MANIFEST.md` contains 105 unique classified behavior units: 57 dynamic, 29 static, and 19 deferred | `dc_coverage_manifest` guards unique IDs, the complete manual audit, classification totals, and registered evidence tests | Closed |

The instrumentation registry, trace and replay support, transactional
no-interrupt native boundary, shadow comparison, and faithful replacement of
`00:F241@1` already satisfy the Milestone 2 bridge proof. Every bonus stage
already has a bounded event family, timeout, completion, ordinary
scored-but-incomplete result, and perfect result; further outcome variants do
not block closure.

### Milestone 2 scope boundary

The following work is explicitly deferred and must not reopen Milestone 2:

- Exhaustive dynamic reproduction of every statically mapped normal-table
  mechanism branch. Exercise each family during Milestone 4 as its routines
  are observed, shadow-tested, and replaced.
- Additional bonus-stage event families and redundant success, perfect,
  failure, or timeout variants beyond the bounded baseline. Add them during
  the corresponding Milestone 4 port or Milestone 6 acceptance pass.
- Faithful mid-routine interrupt delivery was deferred from Milestone 2 and
  completed in Milestone 4 on 2026-07-29. Semantic `01:D081@all` is the first
  interrupt-bearing proof, with all 540 calls preserving exact artifacts.
- Exhaustive combinations of both speeds, both player counts, every drain,
  extra-ball, last-chance, and stuck-ball variant. These remain release
  acceptance requirements, not prerequisites for closing the instrumentation
  milestone.

### Milestone 2 closure result

All four closure gates are complete. Milestone 3 begins with table-background
decode and stitch work; Milestone 2 is not reopened by branches already
classified as static or deferred in the coverage manifest.

The first native Windows host is implemented. Milestone 2 now has a
project-owned Beetle downstream patch layer and a multi-hook
HuC6280 instruction-entry registry keyed by physical 8 KiB bank plus logical
PC. Pass-through and selective observe modes preserve the complete Milestone
1 canonical recording in Debug and Release. Single-address observe mode
retains the original trace bytes, while multi-address mode emits one atomic,
globally ordered register/MPR trace. A separate deterministic gameplay
recording now enters the table and exercises launch, flippers, and nudge
inputs without changing the Milestone 1 canonical artifact. Bounded
call-scope observation now captures callers, return paths, output state,
absolute cycle budgets, and ordered work-RAM writes for the initial session
and collision routines. A selective global work-RAM write census has now
identified and bounded the first four position/velocity physics candidates.
The independent Mesen profile reproduces their inputs and structure, and
separates exact leaf calls from calls needing normalized pre-call state. The
host can now capture an exact pre-instruction savestate at a selected routine
ordinal, restore it after the natural recording, and verify a normalized
bounded replay. All four initial physics scopes have a deterministic replay
sample with identical registers, return path, body-cycle budget, ordered
writes, relative write timing, and post-call work RAM. ROM instruction
signatures and the complete physics traces now establish the indexed slot,
signed 16.8 position, signed 8.8 motion/rotation, rollback, damping, and
acceleration semantics for these scopes.
The first native gameplay code now exists as a C implementation of the
`00:F241` cached-position rollback. A transactional replacement callback
receives an isolated work-RAM snapshot and returns final registers, ordered
logical writes with entry-relative timing, a body-cycle budget, and the outer
RTS at which Beetle resumes. Hardware time advances at each declared write
and at the body boundary. The initial contract permits only scopes proven not
to take an interrupt internally; newly asserted interrupts are sampled
before the resumed RTS. `00:F241@1` is the first safe proof slice.
Its natural native execution and restored assembly replay start from the same
pre-call state and match registers, return path, 162 body cycles, six writes,
relative timing, and complete post-call RAM. The complete gameplay, entry,
call, and ordered-write artifacts remain identical to the assembly goldens.
Unported assembly continues normally before and after that one native call.
The runtime now also exposes an exact-size copied work-RAM import boundary.
Recording-only `--work-ram-patch FRAME:ADDRESS:VALUE` seeds explicit bytes
before a frame, records them in artifact metadata, and validates frame,
address, value, and duplicate-location constraints without changing
unseeded recordings.
That boundary now activates both selective normal-table loaders. An exact
ball-Y drain seed reaches the category-1 remaining-ball reload, while a short
transition seed reaches the category-2 normal-table restoration. A distinct
full-entry seed now invokes all six bonus-stage layouts. Their record
populations, caller state machines, cycle budgets, and ordered writes have
deterministic goldens.
BS-4's two linked-target RNG consumers are now dynamically active in one
targeted route. `05:6B88` contributes 18 calls from the initial linked-target
state and `05:6CB1` contributes 22 calls from the post-allocation state. All
40 shared RNG calls preserve the exact 282-cycle, eight-write recurrence and
both consumer states reload their indexed timer to 32 ticks. Four bonus-bank
RNG consumers remain event-path dependent.
BS-6's natural objective-animation route now activates its paired hazard RNG
consumers. `05:732E` derives the hazard timer and `05:733B` selects its
motion vector. The complete allocator creates slot `$34` from source `$35`,
returns in 12,924 master cycles with 281 writes, and emits command `$32`.
That BS-6 checkpoint left two bank-7 RNG consumers event-path dependent.
The maximum-score route now closes those final consumers. Score saturation
sets `$2CC0.7`, the session dispatcher enters the bank-7 ending, and live
sites `07:48C7/48CF` select six-actor geometry and command `$2C/$2D`.
All nine bonus/ending-bank RNG consumers now have deterministic dynamic
coverage.
The upper Wall-Pocket hand routes now activate normal-table RNG consumers
`02:597D` and `02:59F5`. A targeted Y-coordinate seed retains fixed slot
`$40` and its authentic state, lets the camera sweep activate it, and
produces timer `$0133` from result `$73`. A second route starts the same
actor at its post-close response, maps result `$73` to response `$E3`, and
runs the ROM continuation back to `02:597D`.
The maximum-score gameplay route also naturally activates all seven
upper-chute Gray Blobs before the ending transition. Their first live calls
now bound normal-table RNG consumer `03:68B1`, including all seven
`$40 + (A & $1F)` movement timers.
The Three-Blobs controller now has a dynamically bounded equal-state
outcome. Aligning only its two mismatched global color bytes enters the
state-zero award through the authentic equality gate, then consumer
`03:6620` maps result `$5D` to consecutive states `1/2/3`.
The Little-Skull reactivation route now dynamically bounds normal-table RNG
consumer `02:45C9`. Fixed slot `$46` is moved into the authentic camera
window, enters gray state `02:458A`, is reintroduced after that state restores
its original layout Y coordinate, and reaches `02:45A4/45C9`. RNG result
`$75` selects left-pair pending value 2 at `$2D04`; both no-interrupt scopes
have normalized replays.
The Sorcerer group controller now has a dynamically bounded zero-live-count
outcome. Clearing only its existing live count `$2D0F` at frame 1,200 lets
fixed controller slot `$16` enter its authentic 128-tick reset delay and
consumer `02:57C2`. Eighteen routed RNG results select collision value
`$28/$29`, reload eight deployments, clear the selector two frames later,
and repeat through the original controller cycle.
The Undead Head now has a dynamically bounded post-trigger selector.
Retaining fixed slot `$4D`, moving it into the camera window, and selecting
exact continuation `03:6248` reaches consumer `03:625D`. RNG result `$73`
selects response value `$09`, stores continuation `$6279`, and enters shared
response state `$4D2D`; both scopes reproduce without interrupts.
A combined route now dynamically bounds the final three normal-table RNG
consumers. Devil-Shooter automatic-reset consumer `02:5148` maps result
`$E7` to selector `$67`. Giant-Face mouth-release consumer `03:7834` maps
even result `$58` to vector `$07` and signed velocity `(+12,+62)`.
Undead-Head launch consumer `03:6177` maps even result `$70` to vector `$17`
and signed velocity `(-12,-62)`. All 12 normal-table consumers now have
deterministic dynamic coverage.
BS-1 now has the first bounded bonus rule family: five six-hit objective
targets, five repeatable support actors, their score contracts, remaining
target counter, completion latch, and the dynamically reproduced no-progress
failure/result presentation.
Targeted result-state seeds now also reproduce its scored-progress and
perfect selectors. The perfect branch builds a 256,000-unit special award
and transfers it into the main score in 512 deterministic chunks.
Separate same-frame target seeds now reproduce a genuine nonterminal contact
and the target timer-expiry branch. They dynamically verify the 50-unit
award, commands `$36/$46`, RNG refresh, and type-`$C3` expiry-effect
allocation.
An earlier stage-selection route now also reproduces BS-1's natural perfect
outcome. Five terminal target contacts award 1,000 stored units, decrement
the original objective count from 5 to 0, and let controller `04:44CE` run
the full completion animation. That animation owns the `$2DBA` latch;
result presentation transfers the earned units and the multiplier-10
10,000-unit special award without seeding objective, score, or result state.
BS-2 now has its first bounded rule family: an eight-record layout, 28-count
controller, paired target emitters, exact type-`$C2` allocation contract,
subtype-indexed live count, and dynamically reproduced 50-unit contact.
Both emitter lanes and their five-target batches are now exercised. A valid
board contact maps through the exact 28-cell 8-by-5 perimeter, clears four
tiles for four 100-unit awards, adds the 50-unit target award, and decrements
the shared objective exactly once. The remaining stage-specific layout
actors establish the ball-entry and repeatable capture states. Separate
routes reproduce the zero-progress timeout and the completion selector; the
BS-2 controller's multiplier 50 builds a 50,000-unit special award and
transfers it in 100 chunks. A contact-driven perfect route now selects the
post-collision continuation once for every valid board coordinate. Its 28
target awards, 112 tile resolutions, lane balancing, actor releases, and
objective decrements produce 12,600 earned units, let the original controller
enter completion, and drive the completion-owned `$2DBA` latch plus both
score transfers without seeding objective count, tile state, or result state.
A second route now extends every one of those contacts backward through live
target state `04:5079` and collision helper `04:5A12`. Inverse X seeds account
for the helper's `$90`-step backward scan, so all 28 contacts still resolve
the intended board cells after the balanced vertical collision sweep.
The ordinary scored-but-incomplete result is now dynamically bounded across
BS-2 through BS-6 as well as the existing BS-1 oracle. Each route retains
its authentic nonzero objective state, enters shared result branch
`04:45F0`, advances handshake `$2D66` to one, clears the accumulated bonus,
and takes ordinary cleanup `04:46A4`. All six routes use 181 glyph ticks and
46 advances without entering the perfect-award builder or transfer.
BS-3 now has its first bounded rule family: three six-target emitter lanes,
three eight-hit objective records, 100-unit target and objective hits,
1,000-unit terminal objective awards, indexed completion flags, and exact
lane-count balancing. Separate routes reproduce its zero-progress timeout
and multiplier-20 completion, building and transferring a 20,000-unit
special award. Its natural-perfect route now drives all three terminal
contacts and delayed indexed finalizers, the original controller, completion
animation, gameplay-owned `$2DBA` latch, and both score transfers without
seeding objective flags or the result latch. This is the preserved mapping
checkpoint that preceded the first replacement proof.
BS-4 now has its first bounded rule family: three indexed eight-hit
objectives, their paired source actors, linked type-`$C2` targets, and the
four-child continuation retained by the target ROM state machine. Objective
hits add 100 stored units and command `$2E`; terminal hits add 1,000 units
and clear `$2D2F-$2D31`. A separately bounded source continuation allocates,
positions, indexes, dispatches, and links the target back to its source.
The target contact adds 100 units, selects response `$3B`, and enters the
child-producing continuation. Separate routes reproduce zero-progress
timeout and multiplier-20 completion, building and transferring a
20,000-unit special award.
BS-5 now has its first bounded rule family: a five-by-three field of 15
three-contact objectives. Each contact adds 100 stored units and command
`$21`; the third adds 400 more, decrements shared remaining count `$2D2E`,
and converts the target to its completed form. Separate routes reproduce
zero-progress timeout and multiplier-30 completion, building a 30,000-unit
special award and transferring it in 60 chunks.
BS-6 now has its first bounded rule family: two indexed ten-hit objectives.
Each hit adds 50 stored units and command `$2E`; the tenth adds 2,000,
decrements shared remaining count `$2D2E`, emits the destruction effect, and
releases the actor. The retained objective state machine also owns a
type-`$C2` emitted-hazard allocator. Separate routes reproduce zero-progress
timeout and multiplier-15 completion, building a 15,000-unit special award
and transferring it in 30 chunks.
The session-and-scoring closure route now connects one ordinary live-ball
drain through the session coroutine and back to active-table presentation.
The frame-3,000 Y seed raises `$53.1`; `01:C7BF` decrements `$2D52` from 3
to 2 at frame 3,016; the zero `$2D32` latch selects ordinary continuation
`01:C80A`; and `01:C812 -> 02:4003` reloads the table at frame 3,287.
Panel routine `09:7266` consumes the dirty bit that same frame. Its exact
`09:72FD` glyph stream proves the stored binary score is displayed times
100: stored 1,020 renders as 102,000, and saturation renders as 999,999,900.
The ball digits independently change from 02 to 01. No multiplier,
extra-ball grant, alternate transition, or score mutation occurs on this
ordinary route.

A separate 3,600-frame state-census route now traverses the scrolling table
instead of remaining in the launch lane. It maps the signed camera accumulator
and bounds the first camera integration plus visibility/activation sweep with
cross-configuration work-RAM, call, and ordered-write goldens.
The same route now establishes the first score boundary: a selected 24-bit
binary accumulator, unsigned award inputs, saturation, dirty-state update,
bounded call/write traces, and a normalized replay.
It also establishes the free-running 16-bit RNG seed, exact update recurrence,
two active actor-initialization consumers, interrupt-bearing call behavior,
and a normalized replay. The cartridge-wide RNG consumer census now guards
all 21 call sites: 12 normal-table and nine bonus-stage consumers split into
12 masked timer/offset transforms, six binary selectors, and three multi-way
selectors.
The 127-slot actor sweep is now bounded independently, including occupancy,
signed camera-window membership, persistent and dynamic slot ranges, direct
post-allocation refreshes, interrupt-bearing calls, and a normalized replay.
The actor identity layer now separates shared type bytes from 88 fixed layout
records, 59 initializer identities, live dispatch pointers, ball/flipper
ownership, and the active dynamic allocation pool.
All 88 fixed records and all 59 initializer identities now have a bounded
gameplay-facing or compatibility role. The final hidden slots `$5C-$5E`
resolve to an eight-frame triple banked-call scheduler, a three-marker
presentation animator, and a collision/scene-response controller whose
nonzero branch remains dormant on the standard census route.
The normal-table scene layout is now bounded as one deterministic cross-frame
call that clears and initializes all 88 fixed records through the original
banked-call wrapper.
The accepted game-to-audio boundary is now mapped from its `$2A` producer
latch through the banked `45:4006` export and descriptor parser. Its 66 calls,
16 command IDs, 21 caller paths, channel-state ownership, and first normalized
replay are covered by a Debug/Release regression. Absolute-cycle alignment
with actor identity changes now assigns every gameplay-era command to ball
slot `$00`, one of 10 fixed layout records, or one of three dynamic dispatch
families.
The first complete table-rule family is now identified as the Tower insects
at dynamic dispatch `03:6016`: four runtime actors use a two-hit counter,
couple each hit to a 10-unit score award and audio command `$29`, add a
50-unit completion award, and free their 32-array slots on the second hit.
Their fixed source at slot `$2C` owns the four-spawn lifecycle and 200-unit
group-clear award. Adjacent fixed slot `$2D` is the Tower; its `03:60C3`
update, 50-unit hit, command `$33`, progress transition, and replay are now
bounded independently.
A second two-hit family is identified as the ten Small Soldiers at `03:7F57`.
Their hits couple to 30-unit awards and command `$21`; second-hit behavior
branches between a standard 50-unit completion/free and subtype-`$0F`
holding dispatch `03:7F81`. Fixed Side-Star sources `$34/$35` now close the
family: they spawn the lower-left/upper-right five-member groups, own live
counts `$2D16/$2D17`, and retain a 20,000-point all-five regeneration rule.
The routed subtype hold persists for eight updates, then adds its deferred
5,000-point award, decrements the right group, and frees slot `$69`.
The central Giant Face is now mapped as fixed slots `$36-$39`: persisted
form/request controller, hidden animator, mouth, and crown collision surface.
The route bounds 5,608 inactive calls and four normalized no-op samples.
Exact ROM signatures retain six goddess-to-dragon transformations, 300-point
crown/closed-mouth hits with command `$20`, and the open-mouth 10,000-point
capture, command `$41`, ball placement, and bonus-stage-4 selection.
The Giant Face's two helmet openings are now mapped at fixed slots `$2A/$2B`.
They share `02:4C91`, run identical four-tick/four-appearance idle cycles,
and remain unhit across 1,748 routed calls. Exact ROM signatures retain the
activation path that sets `$2D0C` wake bit 0, awards 300 points with command
`$20`, emits command `$25`, and routes the ball back through the selected
side.
The manual's four upper-table Wall Pockets, also called the 4 Stars, are now
mapped at fixed slots `$3E/$3F/$42/$43`. Their stage IDs BS-1/5/3/6 and
Big-Star selectors VI/IV/VIII/II match their lower-left/right and
upper-left/right positions. The route bounds 1,008 inactive steady calls;
exact ROM signatures retain the normal 1,000-point Sorcerer-color advance
and the stopped-arrow 10,000-point bonus-stage path.
The upper-left middle-table Giant Door (the detailed guide's Wooden Gate) is
now mapped at fixed slot `$4C`, initializer `03:6294`, and steady dispatch
`03:62B8`. The route bounds 676 no-hit calls and a normalized zero-write leaf
replay. Exact ROM signatures retain its three-hit counter, 5,000-point hit
award, 50,000-point completion award, and four-channel post-destruction
handshake.
The four Knights behind that gate are now mapped at fixed slots `$3A-$3D`,
initializers `03:734E/7353/735A/7361`, and shared steady dispatch `03:7378`.
Their 10,600 routed calls remain gate-blocked. Exact ROM signatures close the
indexed `$2D06-$2D09` request/acknowledgment handshake, four-hit rule,
command `$26`, and 5,000-point hit plus destruction scoring.
The upper-left middle-table Walled Star is now mapped at fixed slot `$5B`,
initializer `03:6319`, and steady dispatch `03:6339`. The route bounds 2,650
no-hit calls and a normalized zero-write leaf replay. Exact ROM signatures
retain its five-collision wall lifecycle, command `$2E`, 40,000-point total,
and post-destruction access/reset states.
Its inner access pocket is now mapped at fixed slot `$11`, initializer
`02:4A44`, and steady dispatch `02:4B3F`. The route bounds 905 unhit idle
calls. Exact ROM signatures connect its 10,000-point award and
`$2DBC/$2D53` request/acknowledgment to the outer wall, then scale one RNG
byte into a random bonus-stage selection from 1 through 6.
The Screen-2 Side-Star access surfaces are now separated from their
Small-Soldier source controllers at fixed slots `$10/$12`, initializers
`02:4A54/4A4C`, and steady dispatches `02:4BD5/4C0A`. Their 1,277/905 routed
calls remain unhit and preserve the shared four-tick/four-appearance idle
cycle. Exact ROM signatures retain both 8,000-point awards, the Giant-Face
transformation request, and the upper-right bonus-stage-6 entrance.
The mirrored lower-table bumpers are now identified at fixed slots `$14/$15`
and bounded through steady dispatch `03:78C7`. Eleven routed hits cover their
primary and secondary damage phases, 1,000-point hit award, command `$20`,
and per-side progress. Exact ROM signatures retain the unobserved ninth-hit
20,000-point destruction and six-child release path.
The two Front Spiders below those bumpers are now identified at fixed slots
`$26/$27` and bounded at `03:6490`. Their three routed hits connect left/right
actor identity to trap-door/launcher safety flags `$2D13/$2D14`, a 4,000-point
award, command `$34`, and a common appearance cycle.
The far-right ball service is now identified as the Devil Shooter at fixed
slot `$0E`. Its `02:5116` catch and `02:5159` Button-I charge/release states
are bounded through ball placement, 96 held-input updates, `$25/$3D/$2B`
audio commands, `$F8` launch velocity, the absence of score writes, later
state transitions, and a normalized idle replay.
Its four surrounding type-`$80` companion actors are now mapped at fixed
slots `$0A-$0D`. Their 5,231 routed calls cover session-gated four-/five-frame
animations, exact charge-to-position projection from `$2D15`, 58
charge-appearance changes, dormant states, and eight normalized samples.
The manual's Devil's Coffin is now mapped at fixed slot `$0F`, initializer
`03:63E1`, and steady dispatch `03:63F1`. Two routed contacts each run the
complete seven-appearance return-gate cycle; completion clears the
Shooter-linked `$53.3` session state, changes type `$83` to `$82`, and owns
no score or audio command.
The two remaining lower-table command-`$2E` owners are now separated:
Dragon's Head slot `$13` uses an eight-hit opening counter and 5,000-point
hits, while Giant Skeleton slot `$30` has repeatable 4,000-point hits and an
RNG-selected appearance. Their three routed hits, 140 stored score units,
shared audio ID, fixed identities, and the Dragon's inactive eighth-hit
terminal path are regression-bounded.
The remaining fixed command-`$44` ownership is now identified as the Three
Blobs at slots `$58/$59/$5A`. Their six-state/four-appearance color cycle,
16 scoreless collision advances, per-slot global state, same-frame audio
coalescing, movement dispatches, and inactive all-state-4 result are bounded.
Their adjacent result controller is now independently bounded at fixed slot
`$57`, initializer `03:65BC`, and steady dispatch `03:65C2`. One initializer
and 2,650 routed updates separate 2,565 first-pair mismatches, 35 third-value
mismatches, and 50 interrupt-bearing calls; the route performs no
outcome-side write. Exact ROM signatures retain the all-red award and shared
randomized reset.
The previously unnamed dynamic type-`$8F` / `02:4290` family is now mapped
as shared impact-burst glue. Seven effects from Small Soldiers, Tower
insects, Dragon's Egg slot `$22`, and its `02:441E` hatchling copy their
source positions, emit one source-selected `$2D/$34/$39` command, run a
ten-step animation, and release their slots without owning score.
The command-`$39/$34` sources are now identified as the Dragon's-Egg family:
six fixed eggs at slots `$20-$25`, hidden controller slot `$1F`, and a
dynamic type-`$82` hatchling at `02:441E`. One routed egg/hatchling pair
covers 1,000- and 20,000-point awards, count 6 -> 5, both impact selections,
the next-frame egg-disable transition, and child release. The all-six
completion branch is ROM-bounded but inactive.
The three remaining unexplained fixed-target awards are now identified as
the manual's Skeleton Insects at slots `$31/$32/$33`. Front-Spider latches
`$2D13/$2D14` produce the two 4,000-point side awards, Tower latch `$2CFC`
produces the 10,000-point center award, and the second right-side latch does
not re-award after its target changes state. The target routines add no
audio command; feedback remains owned by the triggering Spider or Tower.
The eight upper-table Sorcerers (Warlocks in the detailed player guide) are
now identified at fixed slots `$17-$1E` with hidden controller `$16`. The
route bounds their eight staggered deployments, live/deployment counters,
arrival states, and 18,152 orbit calls. No routed hit occurs; the exact
two-hit 1,000-point rule, command `$29`, 5,000-point destruction, color
progression, and group regeneration remain ROM-bounded.

## Milestones

| Milestone | Status | Completion gate |
| --- | --- | --- |
| 1. Emulated Windows baseline | Complete | Verified ROM runs through the pinned Beetle PCE fork with SDL3 video, audio, input, saves, and sharp monitor-resolution scaling |
| 2. Instrumentation and hook bridge | Complete | Required game state is mapped and original routines can be observed, shadow-tested, and replaced |
| 3. Cabinet preview | In progress | Three-window cabinet mode shows the stitched playfield, score DMD, black backglass fallback, assigned displays, and playfield-hosted bonus stages |
| 4. Native gameplay port | In progress | Physics, collisions, RNG, scoring, table rules, bonus stages, and session transitions execute natively |
| 5. Final cabinet world model | Not started | Desktop and cabinet profiles share the same native gameplay state without divergence |
| 6. Release hardening | Not started | Tested, documented, ROM-free Windows x64 package is ready for release |

Status values used in this file are `Not started`, `In progress`, `Blocked`, and `Complete`.

## Completed

- [x] Inspected and identified the repository contents.
- [x] Confirmed the ROM as the supported USA image:
  - Size: 393,216 bytes
  - MD5: `92fbe89d4c87bf283dbfbeac69a64e6e`
  - SHA-256: `9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9`
- [x] Selected C++20, CMake, and SDL3 for the Windows host.
- [x] Selected a pinned Beetle PCE fork as the permanent emulated hardware layer.
- [x] Selected Mesen2 as the independent debugger and behavioral oracle.
- [x] Defined the progressive pass-through/observe/replace strategy for native gameplay routines.
- [x] Defined desktop and three-monitor cabinet presentation behavior.
- [x] Fixed sharp pixel scaling as the only v1 scaling method.
- [x] Approved the implementation and acceptance plan.
- [x] Added ROM/build/generated-content exclusions and a CI ROM-leakage guard.
- [x] Added the C++20/CMake/Visual Studio project and dependency-free test preset.
- [x] Licensed the combined Windows application as GPL-3.0-or-later and added dependency locks/notices.
- [x] Pinned SDL 3.4.12 by release-archive SHA-256.
- [x] Pinned and statically built Beetle PCE 1.29.0 at commit `ae99235c2139c176c1a8d0fde2957bf701d3cab0`.
- [x] Implemented exact ROM size, MD5, and SHA-256 validation.
- [x] Implemented the single-instance `PceRuntime` libretro adapter.
- [x] Verified ten emulated frames from the repository ROM:
  - 256x240 compact ARGB framebuffer.
  - 44,100 Hz interleaved stereo audio.
  - 2 KiB backup RAM with exact-size import/export.
- [x] Implemented SDL keyboard and first-gamepad input.
- [x] Implemented fixed-cadence emulation independent of monitor refresh.
- [x] Implemented high-DPI SDL presentation with the fixed two-pass sharp scaler.
- [x] Implemented atomic backup-RAM persistence in `portable-data/devils-crush.sav`.
- [x] Added ROM-free unit/CLI tests and an opt-in `rom_required` smoke test.
- [x] Verified Debug and Release builds, all public CTests, the
  dependency-free stub test, and the local ROM-required smoke test.
- [x] Added raw Beetle savestate capture/restore with the host frame counter.
- [x] Added a checksummed version-1 quick-state container bound to the exact
  ROM SHA-256 and pinned Beetle revision.
- [x] Added atomic F5 quick-save and validated F9 recovery, including queued
  audio reset and rejection of corrupt or incompatible files.
- [x] Verified a real-core savestate round trip with exact restored framebuffer
  output and matching audio sample rate/batch timing.
- [x] Added first-gamepad hot-plug, disconnect failover, and live SDL mapping
  reload from `portable-data/gamecontrollerdb.txt`.
- [x] Bounded queued audio latency to 150 ms and added non-fatal playback
  stream recovery after queue errors, device loss, or format changes.
- [x] Exposed a copy of the non-SuperGrafx 8 KiB PCE work-RAM region for
  deterministic recording and future typed game-state views.
- [x] Added a fixed 600-frame canonical boot/input recorder with per-frame
  work-RAM, framebuffer, audio payload, geometry, and audio-timing hashes.
- [x] Verified repeated Debug recordings and an optimized Release recording
  were byte-identical (SHA-256
  `67a55bed2d4f4e8dc5019c2b9b845df2f29514bbb564e2e44cde406d159575fe`).
- [x] Added Debug and Release opt-in ROM tests that fail if the complete
  canonical recording differs from that golden SHA-256.
- [x] Extracted exact-size backup-RAM file loading and atomic replacement into
  a tested core persistence boundary.
- [x] Added a ROM-backed 2 KiB backup test that writes a deterministic pattern,
  unloads/reloads the Beetle game, imports the file, and compares every byte.
- [x] Added a deterministic Release staging layout containing the executable,
  GPL license, README, and third-party notices.
- [x] Added local and CI distribution verification with an exact file
  allowlist and an independent supported-ROM hash scan.
- [x] Acquired and pinned the official Mesen 2.1.1 Windows oracle, added a
  reproducible headless 600-frame work-RAM comparison, write trace, and CPU
  frame-boundary stream:
  - Traced the persistent pair to Mesen's `0xA0` versus Beetle's `0x88`
    backup-RAM header policy and added strict pre-frame normalization.
  - After normalization, 341 of 4,915,200 byte observations differ
    (0.006938%) across 15 offsets, with 304 byte-exact frames.
  - All remaining observations are explained startup storage: 328 inactive
    stack bytes and 13 zero-page decompressor scratch bytes ending at frame
    6; no active stack or other work-RAM bytes differ.
  - Independent Mesen runs reproduce work RAM, normalized write events, and
    CPU frame boundaries exactly.
- [x] Added raw RGB888 framebuffer capture to both canonical branches and a
  reproducible cross-core normalization/comparison:
  - Mesen's 292x242 output normalizes from `(18,3)` into Beetle's 256x240
    output with one black bottom row.
  - Debug and Release Beetle framebuffer streams are byte-identical
    (SHA-256
    `699b6f3438868ec726cdcdde5114c9ed1d3df818c335ddfe189c08b324593bea`).
  - Pinned Mesen frame skipping off, resolving the earlier variable
    presentation phase.
  - Both Mesen framebuffer captures are byte-identical, and all 600
    normalized frames match Beetle pixel for pixel at zero temporal shift.
- [x] Exercised the original game's complete visible backup-RAM workflow in
  an isolated Release-host directory:
  - Saved a new game into slot 1 after confirming all five slots were unused.
  - Cleanly exited with an exact 2,048-byte atomic backup file.
  - Restarted the host, observed slot 1 as `00000000 03` in Data Load, and
    loaded it back into the expected zero-score, three-ball playfield.
  - Documented the procedure and evidence in
    `docs/BACKUP_RAM_VALIDATION.md`; the normal user save was untouched.
- [x] Closed the independent canonical audio timing gate:
  - Added optional atomic 44.1 kHz PCM16 WAV export to the Beetle recorder.
  - Added a reproducible managed Mesen runner patch that exposes its existing
    WAV recorder while verifying the official `MesenCore.dll` is unchanged.
  - Two Mesen WAVs are byte-exact; Beetle and Mesen differ by only two stereo
    samples over 600 frames, align within 9 samples (0.204082 ms), and have
    0.981922894 correlation across 10 ms RMS envelopes.
  - Work RAM, framebuffer, audio timing, and scene progression now have no
    unexplained semantic difference in the Milestone 1 canonical window.
- [x] Established the first Milestone 2 execution-hook boundary:
  - Added an in-tree downstream fork manifest, local bridge sources, and an
    ordered patch applied reproducibly to the exact pinned Beetle revision.
  - Added a pre-opcode HuC6280 callback exposing logical PC, physical 8 KiB
    bank, A/X/Y/SP/status, all eight MPRs, and the master-cycle timestamp.
  - Added a typed runtime API for one exact hook in pass-through or read-only
    observe mode; replace remains explicitly rejected.
  - Verified bank `0x00` / PC `0xE462` is entered 11,403 times during the
    canonical run and that hooked Debug and Release recordings retain SHA-256
    `67a55bed2d4f4e8dc5019c2b9b845df2f29514bbb564e2e44cde406d159575fe`.
  - Added a versioned, atomic routine-entry CSV containing frame, master
    cycle, A/X/Y/SP/status, and all eight MPR values.
  - Verified 11,403 observed entries across canonical frames 1–7 reproduce
    byte-for-byte in repeated Debug and Release runs (SHA-256
    `585b2221987701f24b9f6a271d48a7bedec3ce01fb08f3889d196cfe1a3191a4`).
  - Verified all 13 full-host tests in Debug and Release plus the
    dependency-free stub test.
- [x] Extended the execution boundary into a multi-hook registry:
  - Added duplicate-key rejection, exact-key removal, atomic clear, and
    registry-size inspection to the runtime API.
  - Allowed repeated pass-through and observe CLI hooks, including distinct
    modes in the same canonical run.
  - Preserved byte-exact v1 output for one observed address and added a v2
    trace that records the selected routine list and globally ordered entries.
  - Confirmed the controller poll at `00:E195` with 599 entries across frames
    2-600 and mapped four setup-screen candidates in bank `0x08`.
  - Matched all five entry counts and frame ranges against Mesen, documented
    confidence levels in `docs/ROUTINE_MAP.md`, and kept gameplay labels
    provisional because the canonical script does not reach the table.
  - Verified the five-hook 717-entry trace in Debug and Release (SHA-256
    `38ffd96714732c1f1f4073888c44f1b94b325dee8429e36d4712290bd1d5bddc`)
    while retaining canonical SHA-256
    `67a55bed2d4f4e8dc5019c2b9b845df2f29514bbb564e2e44cde406d159575fe`.
  - Verified all 14 full-host tests in Debug and Release plus the
    dependency-free stub test.
- [x] Added the first deterministic active-table recording and routine census:
  - Added a separate 1,800-frame gameplay mode that selects New Game, reaches
    the table, launches the ball, operates both flippers, and applies four
    nudges without changing the 600-frame canonical input or hash.
  - Reproduced the gameplay recording byte-for-byte in Debug and Release
    (SHA-256
    `1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353`).
  - Confirmed the controller poll through frame 1,800 and identified initial
    nudge/tilt, pause/high-score, and collision-lookup candidates.
  - Reproduced a 23,167-entry four-hook trace in Debug and Release (SHA-256
    `d66b6fd13beba8a36370a7508bad968f72570bfad7d947b8442376279bf928b0`)
    without changing the gameplay artifact.
  - Confirmed the same gameplay activation pattern in Mesen. Its two session
    candidates enter one frame and one call earlier, and its collision
    candidate has 26 additional calls; these timing deltas remain explicit
    mapping uncertainties.
  - Added a ROM-backed gameplay regression for the complete CSV, hook counts,
    and globally ordered trace.
  - Verified all 16 full-host tests in Debug and Release plus the
    dependency-free stub test.
- [x] Resolved the first gameplay cross-core deltas and mapped callers:
  - Added a checked-in Mesen gameplay oracle that reproduces the 1,800-frame
    input route, captures the four routine entries, derives JSR call sites
    from the HuC6280 stack, and exports frame-boundary work RAM.
  - Repeated Mesen entry/caller traces are byte-identical (SHA-256
    `bdce4c3cecc6587e5319e56dfea2a47ed603ec9aa01c7dd9053627468db9889e`);
    repeated work-RAM streams are also byte-identical (SHA-256
    `868b08b94d0d725b0a56f7c1f88a1286a232d7860c8f310695a694624631e183`).
  - Proved that Mesen's apparent `+1/+1/+26` counts are exactly one frame of
    table-activation phase: after four shared collision bootstrap calls,
    Beetle frame `n` matches Mesen frame `n-1` for every candidate call.
  - Verified aligned A/X/Y/SP and stable MPR values. Status differences are
    exclusively the transient HuC6280 T bit (`0x10`).
  - Mapped `01:C3CB` to caller `01:C166`, `01:C49E` to `01:C169`, and nine
    active callers of `03:6966`; documented three additional static collision
    call sites not exercised by this route.
  - Mapped the initial session gates, direct state fields, outer return sites,
    and the collision table interpreter's cursor, event, orientation, and
    signed-velocity fields.
  - Added an isolated two-run capture launcher and a deterministic comparison
    script that enforce repeatability, counts, phase, registers, stable MPRs,
    and T-bit normalization.
- [x] Added bounded call-scope observation and the first write/cycle oracle:
  - Extended the downstream HuC6280 boundary with opcode state, a post-write
    callback, and the serialized absolute master clock without changing the
    canonical or gameplay recordings.
  - Added a re-entrant runtime observer that brackets an exact bank/PC entry
    through its matching outer RTS and captures callers, return paths,
    input/output registers and MPRs, body cycles, and nested ordered writes.
  - Added atomic call and companion write CSVs. The 1,800-frame route
    completes all 23,167 calls and records 235,173 writes; Debug and Release
    reproduce SHA-256
    `920fb4f8132b58ded27e65eb0a4b0c88def481fcb5f29286f8029f3594057b38`
    and
    `84c398603f50b279ad670eed0d06d887006f085aa1d5aa7d4a8de4ad8178264c`.
  - Corrected cross-frame cycle accounting for 20 calls by retaining the
    frame-local timestamp for compatibility and using Beetle's serialized
    64-bit timestamp base for duration calculations.
  - Extended the independent Mesen oracle with matching call/output/write
    capture. Repeated call and write traces reproduce SHA-256
    `587425ed4915c1e8786e3bb6b31fdde8692e1d8181817f6ab000b9e7f807e9fd`
    and
    `859f2ba491ce08237870d1935746ee60d48e7b3928b710221fb9736a1e7d5aca`.
  - Verified all 21,368 phase-aligned session/collision calls: 18,075 have
    exact scoped cycles and writes; 3,293 differ only because a periodic
    interrupt lands in a neighboring scope, with routine-owned ordered
    writes, callers, returns, and outputs still matching.
  - Verified all 17 full-host tests in Debug and Release plus the
    dependency-free stub test.
- [x] Added the first selective state-writer census and physics scopes:
  - Exposed one read-only global 8 KiB work-RAM write observer through the
    runtime, including frame, absolute cycle, executing bank/PC, logical
    address, physical offset, and value.
  - Added repeatable `--work-ram-write-range` filters and atomic
    `--work-ram-write-trace` output without changing the existing entry,
    call, or scoped-write trace formats.
  - Recorded 2,820,652 selected writes across global, session, physics,
    actor, and rule-state ranges during the deterministic gameplay route.
  - Mapped and bounded `00:F1B4`, `00:F241`, `01:D1A2`, and `01:DD09` as
    the first position/velocity and motion-coordinator candidates.
  - Captured all 2,717 calls and 171,993 scoped writes, including exact
    callers, outer returns, and cycle ranges.
  - Verified Debug and Release entry, call, and write traces are byte-exact
    with SHA-256 values
    `9442322858ad18bfdf67ccb9a05aaf2312c512e168e2c7dbcd3ab858fed3cf79`,
    `92f638ae77cad9e2964cb2bafdca6b21a2aa50187abded37480ebd55146170a4`,
    and
    `6f272290712ed6432205e7858d6fa72102623b08dece9beeb5c4cee2d3ffbbce`.
  - Added a dedicated ROM-backed physics regression and documented the
    mapped fields and remaining confidence limits.
  - Verified all 21 full-host tests in Debug and Release plus the
    dependency-free core test.
- [x] Added the independent physics routine oracle:
  - Added selectable gameplay/physics profiles to the isolated Mesen capture
    launcher and Lua bounded-call observer without changing the existing
    gameplay profile.
  - Reproduced Mesen physics entry, frame-boundary work-RAM, call, and write
    traces across two isolated runs with SHA-256 values
    `d1f4091585a65e6f5d62eb0d1a738944fca5b2d708fbcac749bdae47bbcec5b2`,
    `868b08b94d0d725b0a56f7c1f88a1286a232d7860c8f310695a694624631e183`,
    `53ea96f7a690da43f08a2d5282ebd71224057897ce172890c408b376c8adfc7f`,
    and
    `0b414a8f27f33ca76573048af1f79eb8358d2211df32ff60ded3c695f09a6178`.
  - Verified all four callers, outer returns, counts, and caller-context input
    streams across Beetle and Mesen.
  - Classified 549 of 808 aligned `F241`/`D1A2` leaf calls as exact in
    outputs and routine-owned writes. The other 259 begin with different RAM
    state because slot-0 calls retaining `Y=06` caller context land in
    different output frames.
  - Re-ran the original 21,368-call gameplay comparison after profile
    refactoring; all 18,075 exact and 3,293 interrupt-relocated scopes retain
    their established results.
- [x] Added savestate-normalized pre-call replay for the first physics scopes:
  - Added repeatable `--routine-replay BANK:PC@ORDINAL` selection to observed
    deterministic recordings.
  - Captured Beetle's opaque state and an independent 8 KiB work-RAM image at
    the exact pre-instruction boundary, then restored and replayed the selected
    call after committing the unchanged natural recording artifacts.
  - Required exact pre-call RAM, input/output registers and MPRs, caller and
    return path, body-cycle budget, ordered writes with entry-relative timing,
    and complete post-call work RAM.
  - Verified `00:F1B4@17`, `00:F241@1`, `01:D1A2@1`, and `01:DD09@1` with
    respective body budgets of 714, 162, 8,736, and 168 master cycles.
    Milestone 4 later added independently replayed, interrupt-free
    `01:DD09@2` at 2,409 cycles and 68 ordered writes.
  - Documented Beetle's expected mid-frame clock-origin rebase and kept
    interrupt-bearing `DD09` paths outside the deterministic replay gate until
    the replacement interrupt-delivery contract is defined.
  - Verified the 21 existing full-host tests in Debug and Release, four new
    replay CLI validation tests in both configurations, and the dependency-free
    core-only test.
- [x] Resolved the first physics scopes' indexed-slot and fixed-point semantics:
  - Added `tools/Analyze-PhysicsSemantics.ps1` and made the ROM-backed physics
    regression verify defining instruction signatures and all 2,717 bounded
    calls.
  - Proved `X` is the parallel-array index and is `$00` throughout the current
    route; `Y` is retained caller context rather than an actor index.
  - Mapped the two positions as signed 16.8 values and their integration
    increments and transform components as signed 8.8 values.
  - Corrected `00:F241` from a provisional velocity helper to the six-byte
    pre-integration position rollback used by the collision path.
  - Mapped `01:D1A2` as a signed 8.8 two-dimensional rotation and `01:DD09`
    as signed per-axis damping plus `$8B/256` second-axis acceleration.
  - Re-ran the independent physics comparison after renaming its `Y`-grouped
    streams as caller contexts; the established 549 exact and 259
    pre-call-state-divergent leaf classifications remain unchanged.
  - Verified the extended ROM-backed physics regression in Debug and Release.
- [x] Extended the active-table census through the scrolling camera:
  - Added a separate 3,600-frame state-census input route without changing the
    canonical or gameplay recordings.
  - Traversed 208 distinct horizontal slot-0 integer coordinates and vertical
    coordinates 241-684, activating camera motion from frame 1,201 onward.
  - Mapped the camera accumulator as `$0C:$0B:$0A`, with `$0A` fractional and
    the VDC interrupt writing signed integer scroll `$0C:$0B` to register 8.
  - Bounded `00:F43D`, called from `01:C15A` and returning at `00:F4FB`, as
    camera integration plus the indexed visibility/activation sweep.
  - Captured 232 complete calls, body budgets of 34,395-88,980 master cycles,
    and 205,563 scoped writes.
  - Verified byte-identical Debug/Release work-RAM, entry, call, and write
    artifacts with SHA-256 values
    `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`,
    `4d40c0841a4783d612ed375ed0e12c74fd87014bd87f1d6c9b221323f4b07f74`,
    `36e126530f88760882ae1cd2a6ccd8264985c28b620cb0d49d6d075b68fb3481`,
    and
    `6c4678d6c0810d0aae3533b2389396310bcb9f0aa5575c17868203fe88f2c2c8`.
  - Added a ROM-backed regression with exact configuration-specific frame
    goldens. Debug and Release differ only in 908 framebuffer hashes beginning
    at frame 2,147; every work-RAM and audio row remains identical.
- [x] Mapped and bounded the first score-accumulation routine:
  - Identified `$2205,Y/$220C,Y/$2213,Y` as selected little-endian 24-bit
    score arrays and `$2251` as their slot selector.
  - Mapped `00:F55C` as unsigned `$30:$2F` award addition, saturation through
    `F5A9`, score-status processing through `F5D6`, and bit-0 dirty marking at
    `$2CC0`.
  - Proved the saturation constant is 9,999,999 stored units while leaving
    its visible digit scaling explicitly unresolved.
  - Captured 40 slot-0 calls across frames 1,409-3,466, with body budgets of
    558-1,305 master cycles and 446 scoped writes.
  - Reconstructed a final stored score of 1,280 from the direct writes and
    verified observed awards 10, 30, 40, 50, 100, and 200.
  - Added `tools/Analyze-ScoreSemantics.ps1`, a normalized `00:F55C@1`
    replay, and a ROM-backed Debug/Release regression.
  - Added a 44-boundary instruction transcript spanning the outer addition,
    both nested helpers, both JSR stack pairs, and their RTS returns. All 40
    calls now execute natively, including ten original interrupt-bearing
    scopes, while the restored-assembly replay remains exact.
  - Verified byte-identical entry, call, and write artifacts with SHA-256
    values
    `d4cbf19841208ff7045f87bf76677fb3ed7e9a69db2d14458d390f9e6ce43fa7`,
    `d0caf39e0f764c653d3289cdc1b105627744aa094995506ecb05e7a68c2e289e`,
    and
    `241c03d3f1fc8c2de7fb08cd25d43ad18f4d3e48220172476548fb98fbc678c4`.
- [x] Mapped and bounded the original RNG seed/update boundary:
  - Identified `$222B:$222A` as a free-running 16-bit seed advanced once per
    frame by the bank-zero interrupt.
  - Mapped `00:E8B3`, reached through trampoline `01:C081`, as the exact
    `$503F` plus frame-counter seed mixer that stores and returns the new high
    byte.
  - Verified all 78 calls and their eight direct writes, including six calls
    whose scopes contain a periodic interrupt.
  - Mapped active consumers `03:6678` and `03:6697` to masked actor-array
    initialization ranges.
  - Completed the exact 21-site `JSR C081` census across physical banks
    `$02-$07`: 12 normal-table and nine bonus-stage consumers.
  - ROM-bounded the immediate output contract of the then-19 inactive
    consumers across 12 masked timer/offset transforms, six binary selectors,
    and three multi-way selectors. Seeded bonus routes have since activated
    two of those sites; the remaining gameplay meaning and call order stays
    explicit route work.
  - Added `tools/Analyze-RngSemantics.ps1`, a 3,771-write seed census, and a
    normalized `00:E8B3@1` replay.
  - Verified byte-identical Debug/Release entry, call, scoped-write, and
    seed-write artifacts with SHA-256 values
    `3202fb333bba3b82e7126a08bad244f534237403983ccb65206342b4849a77c5`,
    `cca8a9e2c1dba861e4eb550e781eb969aac3625f76764a66569f66e450bd029b`,
    `06389c11544b16f5a98b6b509b124533cb9e0b57858c274471faf6eee506eb7e`,
    and
    `61bf739045460c23feb39498cc48c09d000cccb888bd0fe0110b897b50f66952`.
- [x] Mapped and bounded the actor-slot camera-window activation helper:
  - Identified `00:F4DF` as the shared 127-slot sweep entry and `00:F4FC` as
    the per-slot occupancy and signed vertical-window flag helper.
  - Proved `$3000,X` is the occupancy/type byte, `$3380:$3300,X` is the
    signed integer vertical coordinate, and `$3080,X` bit 7 records membership
    in `[$AA:$A9 - 48, $AA:$A9 + 240)`.
  - Captured 519 complete `$00-$7E` sweeps plus 14 immediate refreshes of
    newly allocated slots `$62-$6F`, for 65,927 calls and 283,956 scoped
    writes across frames 872-3,600.
  - Counted 10,873 empty, 28,823 below-window, 17,706 in-window, and 8,525
    above-window paths and verified every occupied slot-addressed flag result.
  - Established 111 occupied slots: `$04-$61` remain persistent, `$62-$71`
    exhibit dynamic lifecycles, and `$01-$03/$72-$7E` remain empty.
  - Added `tools/Analyze-ActorActivationSemantics.ps1`, a normalized
    `00:F4FC@1` replay, and a ROM-backed Debug/Release regression.
  - Added semantic `00:F4FC@all` replacement for all four live-state paths.
    Its 2/18/31 instruction checkpoints admit all 65,927 calls, including
    4,858 interrupt-bearing scopes, without a slot or occurrence allowlist.
  - Verified byte-identical entry, call, and write artifacts with SHA-256
    values
    `8cd60958fe27377c1eec09c3c665f27f3642c581ac10da84be319d83c7dbae74`,
    `6147c3b0a582a7810f7cbc97f3b9345b6138db4acf966a2d97873685f7042f91`,
    and
    `9d263045a2fbd253c3660ac1bbea41807bae3615627514a373421243a9f6ba7c`.
- [x] Decoded the actor layout and live dispatch identity layers:
  - Mapped slot 0 to the ball, slots `$04/$06/$08` and `$05/$07/$09` to
    paired flipper/control initializers, and slots `$0A-$61` to the fixed ROM
    layout table beginning at `02:45DB`.
  - Decoded all 88 fixed 12-byte records, including slot, initializer, signed
    integer position, state, flags, type, and initializer parameter.
  - Identified 59 distinct fixed initializer entries and proved the shared
    `$80-$84` type byte is not sufficient as a mechanism identity.
  - Mapped `01:C574` as the 32-array generic slot reset and `01:C5AB` as the
    first-free allocator over `$04-$7B`.
  - Added semantic `01:C574@all` replacement using one generated
    361-checkpoint loop transcript. It admits all 132 observed resets,
    including 35 interrupt-bearing scopes, without a caller or slot
    allowlist.
  - Captured 44 route allocations and 35 frees within dynamic slots
    `$62-$71`, plus seven active dynamic dispatch-writer families.
  - Identified `$3F80:$3F00,X` as the live state-machine pointer and captured
    3,296 balanced pointer-half writes, including 1,107 common transitions.
  - Added `tools/Analyze-ActorLayoutSemantics.ps1` and the ROM-backed
    `dc_rom_actor_layout` Debug/Release regression.
  - Verified the combined 3,872-write identity artifact with SHA-256
    `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`.
- [x] Bounded the normal-table scene-layout initializer:
  - Identified `02:4088` as the full 88-record mode and `02:40A8/40CF` as
    category-1/category-2 selective modes that remain inactive on this route.
  - Mapped the `00:E241/E262` banked-call wrapper and the active caller/resume
    path at `00:E252/E255`.
  - Captured the single frame-871/872 call at 421,956 body master cycles and
    11,331 scoped writes.
  - Verified every record-backed actor field, 176 initializer scratch writes,
    and all 2,816 parallel-array zero writes.
  - Kept the natural cross-frame call as the oracle because it cannot complete
    inside the normalized replay system's restored single-frame window.
  - Added `tools/Analyze-SceneLayoutSemantics.ps1` and the ROM-backed
    `dc_rom_scene_layout` Debug/Release regression.
  - Verified byte-identical entry, call, and write artifacts with SHA-256
    values
    `8fdc6c0c9807c5bbb22a560bd6e05d9288fe7d315db8056ee24527362995b847`,
    `bd601b053e6328985f279f4de1354b5617ecd3ca2ebb4feac9facbd114f63ab9`,
    and
    `dc65b4240bd68dd32cbf35f27da707d655ce0487551fbe94afac3aa2f895ac58`.
- [x] Mapped the accepted game-to-audio command boundary:
  - Identified `01:C087 -> 00:ECFD` as the common producer entry, `$2A` as its
    synchronous command latch, and `00:ED18` as the accepted-command boundary.
  - Mapped the `00:ED58/E262` wrapper through raw mapper banks `$45-$47` to
    audio export `45:4006` and the interrupt-safe parser at `45:4B7B/4BCC`.
  - Captured 66 accepted commands across 21 caller paths and 16 command IDs,
    with stable caller-to-command populations.
  - Decoded the 16 active `$5D81` descriptor-table entries and established
    that each takes the nonzero-tag data path.
  - Assigned all 2,737 `$2E00-$2FFF` state writes in the scopes to raw bank
    `$45`; the outer wrapper restores X, Y, SP, and every MPR.
  - Correlated gameplay calls with the actor identity trace: ball slot `$00`,
    10 fixed layout records, and three dynamic type/dispatch families account
    for every post-layout command.
  - Established that commands `$2E` and `$34` are shared across unrelated
    producer identities, so command ID alone is not a gameplay mechanism ID.
  - Added `tools/Analyze-AudioCommandSemantics.ps1`, a normalized
    `00:ED18@1` replay, and the ROM-backed `dc_rom_audio_command` regression.
  - Verified byte-identical entry, call, and write artifacts with SHA-256
    values
    `7f47c3163e51226ff5b85b5cd13dc354cdcdde832f24ca3c6b63f2e8eb2ef3b3`,
    `6f12c1827eee9f7b924c42664bc2720f78a285ff2409bd53d4816558eb758a84`,
    and
    `ffa478f6c45d353f37beda37bd15126f9e4bd6d8447ab058a2ebfca434b71c80`.
  - Reused the actor identity artifact SHA-256
    `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`
    to guard the absolute-cycle ownership correlation.
- [x] Bounded the first dynamic table-rule family:
  - Identified the family as the manual's insects living in Tower: fixed
    source slot `$2C` at `(140,526)` uses `BSR 03:5F78` to create the four
    children and declares a 200-unit group-clear award.
  - Identified `02:5F78` as a four-instance dynamic type-`$82` spawner that
    initializes `$3C00,Y` to 2 and transitions from `02:5FDB` to steady
    dispatch `03:6016`.
  - Mapped instances in slots `$64/$67/$6A/$6D`, their allocation and
    transition frames, and all 6,494 steady-state updates.
  - Proved each of seven hits awards 10 stored units and emits command `$29`;
    second hits add a 50-unit completion award.
  - Captured three completed instances, their live-count decrements, and the
    exact 32-array clears that free slots `$64/$6A/$6D`; slot `$67` remains
    after one hit.
  - Accounted for 220 stored score units from this family and all 10 nested
    score calls.
  - Added `tools/Analyze-TwoHitTargetSemantics.ps1`, a normalized
    `03:6016@5` no-hit replay, and `dc_rom_two_hit_target`.
  - Verified byte-identical entry, call, and write artifacts with SHA-256
    values
    `7e3e07db7f985011ff1d79972077c1524f7d14324e0b963d36cf99510cedf661`,
    `cb5d44ef5d6aad1cf78af90d34f382ef288dcdedfb31ca186fbe644f86f12059`,
    and
    `7d2174b63f3d9180d124b262345ce1ae5cf7a4072d8520a436aeeefde8dd41ab`.
- [x] Bounded the branching two-hit dynamic family:
  - Identified the ten actors as the manual's Small Soldiers from their
    count, two-hit lifecycle, and 30-unit hit/50-unit completion awards.
  - Identified `03:7E87` as a ten-instance type-`$82` spawner that initializes
    `$3C00,Y` to 2 and transitions from `03:7EFA` to `03:7F57`.
  - Captured 20,867 steady updates across slots
    `$62/$63/$65/$66/$68/$69/$6B/$6C/$6E/$6F`.
  - Proved each of five hits awards 30 stored units and emits command `$21`.
  - Mapped the second-hit subtype branch: slot `$62` adds 50 and is freed,
    while subtype-`$0F` slot `$69` enters holding dispatch `03:7F81`.
  - Accounted for 200 stored score units, six nested score calls, the standard
    group-count decrement, and the exact slot-`$62` 32-array clear.
  - Added `tools/Analyze-BranchingTwoHitSemantics.ps1`, a normalized
    `03:7F57@1` replay, and `dc_rom_branching_two_hit`.
  - Verified byte-identical entry, call, and write artifacts with SHA-256
    values
    `c4d55adb49df61c3ab64250c1429e3bb07c0eef92d8b2c3290e1f9e5947d8f4f`,
    `3ab64128aab89d056216f79d0d3f041e186eb109048aefe82785e2875f41ec4f`,
    and
    `3b2eb1b5dbd54c1b08cf6a509820e47c2320faf544b4d15824f5426f414c6544`.
- [x] Bounded and named the fixed lower-table Tower family:
  - Identified layout slot `$2D`, initializer `03:60B3`, position `(144,538)`,
    type `$83`, and steady dispatch `03:60C3` as the manual's Tower.
  - Captured 2,646 updates across frames 873-3,600: 2,615 zero-write no-hit
    calls, 30 interrupt-bearing no-hit calls, and one routed hit.
  - Proved the frame-3,353 hit awards 50 stored units, emits command `$33`,
    advances `$2D3F` to 1, and transitions through `03:611C/611F`.
  - Connected the Tower to neighboring insect source slot `$2C`, its four
    `03:6016` child allocations, and the shared `$2D2B` lifecycle counter.
  - Added `tools/Analyze-TowerFamilySemantics.ps1`, normalized
    `03:60C3@1` replay, and `dc_rom_tower_family`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256 values
    `9771eca6e3a0c8f297eb9311ebc18af9b498362770a3b157e91553af6f4f0c79`,
    `1b6440a384b815ba8ee97473dc389a56f9455ec0a82ee307c5baa499bbf3712f`,
    and
    `26696c169ae2296a752ea3916c9151134febbb89ef1a9eea54826645c64815c3`.
- [x] Bounded and named the fixed lower-table Bumper family:
  - Identified mirrored category-2 slots `$14/$15`, initializers
    `03:789A/78A4`, positions `(190/74,604)`, and shared dispatch `03:78C7`
    as the manual's Bumpers.
  - Captured 3,468 calls across frames 872-3,562, split evenly between the
    two slots, with 3,419 zero-write no-hit paths and 38 interrupt-bearing
    no-hit paths.
  - Proved all 11 routed hits award 10 stored units and emit command `$20`;
    slot `$15` reaches phase 8 while slot `$14` reaches phase 3.
  - Bounded the inactive ninth-hit terminal rule: a 200-unit destruction
    award followed by six type-`$83` child allocations at `03:7A71`.
  - Added `tools/Analyze-BumperFamilySemantics.ps1`, normalized
    `03:78C7@1` replay, and `dc_rom_bumper_family`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256 values
    `5fb7885f28519eec2758e06dac02327d2a9a86f3c0a416124fa0c7ce4865f3db`,
    `0495b9de1e3dcfb1781e0056411705464a2e52d8ddcde371d2efc4d0124b882a`,
    and
    `754938e2e0b04dad1c51ee416f6d22ea3865055cfe4390a1af9d1610072f57da`.
- [x] Bounded and named the fixed Front-Spider safety family:
  - Identified slots `$26/$27`, initializer `03:646B`, positions
    `(64/216,636)`, side parameters 0/1, and steady dispatch `03:6490` as the
    two Front Spiders beneath the Bumpers.
  - Captured 2,645 calls across frames 873-3,551, split 1,357/1,288 between
    the left/right slots.
  - Proved right-side hits at frames 2,182/2,564 set launcher-safety flag
    `$2D14`, while the left-side frame-3,466 hit sets trap-door flag `$2D13`.
  - Verified that all three hits award 40 stored units, emit command `$34`,
    and traverse the same multi-step appearance transition before returning
    to `03:6490`.
  - Added `tools/Analyze-FrontSpiderSemantics.ps1`, normalized
    `03:6490@1` replay, and `dc_rom_front_spider`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256 values
    `e7c5827a2cec93436b944294dfe9b212e383ef03a95a5a3294f0916a2b7a9e47`,
    `74f2ea92d7d9b105b0a2a416d7de871ef8723ed25beca28e7107b0f7f8045a4c`,
    and
    `743457198ef6ddaae820652b016fe0f6ade99e4937d09225f24dee13f2414659`.
- [x] Bounded and named the fixed Devil Shooter ball service:
  - Identified slot `$0E`, layout position `(264,630)`, initializer
    `02:50EE`, live position `(264,671)`, type `$82`, and idle dispatch
    `02:5116` as the manual's Devil Shooter.
  - Proved the frame-1,064 catch sets `$3C80`, emits command `$25`, and
    positions the ball at X=`$010E`.
  - Bounded 96 Button-I-held `02:5159` calls across frames 1,085-1,180,
    charge-counter progression `$00`-`$18`, and four command-`$3D` writes.
  - Proved release at frame 1,185 writes `$F8` launch velocity, emits `$2B`,
    advances the actor state, and does not award score.
  - Added `tools/Analyze-DevilShooterSemantics.ps1`, normalized
    `02:5116@1` replay, and `dc_rom_devil_shooter`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256
    values
    `c5b2500b726c9e161a138880e5f35b1dd306cd9c41e06bfb2994c8716367ec1d`,
    `0ee9384584f2d5e177ce8580a6dc535c0c8a30c94b3d7dbaf953c97ad96bfdaf`,
    and
    `90557ce04ba3e79132967255e9cdf6747cb4c5628f0f99a15d6f5c402ade0139`.
- [x] Bounded the lower-table Dragon's Head and Giant Skeleton:
  - Identified fixed slot `$13`, initializer `02:5A31`, position `(64,510)`,
    type `$82`, and dispatch `02:5A4B` as Dragon's Head.
  - Proved routed Dragon hits at frames 2,076/2,080 each award 50 stored
    units, emit `$2E`, and advance its eight-hit counter 8 → 7 → 6.
  - Bounded the inactive counter-zero path that adds another 50 units on the
    eighth hit and begins the mouth-opening transition.
  - Identified fixed slot `$30`, initializer `02:5EE4`, position `(224,518)`,
    type `$83`, and dispatch `02:5EEC` as the Giant Skeleton.
  - Proved its frame-2,642 hit awards 40 stored units, emits the shared
    command `$2E`, and leaves its repeatable dispatcher active.
  - Added `tools/Analyze-DragonGiantSkeletonSemantics.ps1`, normalized
    `02:5A4B@1` replay, and `dc_rom_dragon_giant_skeleton`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256
    values
    `ca858c9f1fcea148c369d95bd13b026f5b45b1f35d5ff0cefbd8ac825d129cf5`,
    `aeb9de1db6f9dfae7e59a45c37bc82f2cb2e1360892c76dfcafe1131d3332819`,
    and
    `698de7535fbd7958698b1e7ee897b5082ac5a18ddd4734fe263feabc45c95372`.
- [x] Bounded and named the fixed Three-Blobs color family:
  - Identified category-2 slots `$58/$59/$5A`, initializer `03:664A`,
    positions `(128/160/192,254)`, parameters 0/1/2, and type `$82` as the
    Three Blobs above the central face.
  - Mapped parameters 0/1/2 to global internal states `$2D39-$2D3B` and
    proved six states select four appearance IDs.
  - Captured 1,827 calls across movement dispatches `03:66A3/66D3`, split
    evenly across the three actors.
  - Proved 16 collisions advance the selected state and emit `$44` without
    awarding score; slot `$58` is unhit, `$59` receives seven hits, and
    `$5A` receives nine collision calls.
  - Explained the earlier 15-command audio census: two `$5A` producer calls
    in frame 1,546 coalesce into one per-frame audio export.
  - ROM-bounded the inactive all-state-4 rule, which submits 100 stored units
    and sets bit 7 of `$53`, plus its randomized post-match reset.
  - Added `tools/Analyze-ThreeBlobsSemantics.ps1`, normalized
    `03:66A3@1` replay, and `dc_rom_three_blobs`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256
    values
    `7f53b1f567fb59dc13c2967e8a083cb2d24b9fe0c0f45dd13e51fe962db00d5e`,
    `b2a96ba58d4ecc6063e1ea812b1e8b721e3a265f6c6333010e9efc94eb949f43`,
    and
    `e72fbf7baa742a8f23bace6ed57adda2e407e906953450306507fb6b476a6b2c`.
- [x] Bounded the shared type-`$8F` impact-burst family:
  - Traced default entry `02:41EC`, alternate entry `02:41FF`, common
    allocator `02:420B`, one-shot activation `02:4290`, and animation state
    `02:42B0`.
  - Identified seven source-to-effect allocations: two Small Soldiers, three
    Tower insects, Dragon's Egg slot `$22`, and its `02:441E` hatchling.
  - Proved every effect copies its source position, emits exactly one
    source-selected command (`$2D` x5, `$34` x1, `$39` x1), and performs no
    score-accumulator writes.
  - Verified all seven instances transition to the ten-step animation and
    release their dynamic slots after 35 or 36 frames.
  - Added `tools/Analyze-ImpactBurstSemantics.ps1`, normalized
    `02:4290@1` replay, and `dc_rom_impact_burst`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256
    values
    `c5101a7c2ab1ed2a6642bcba29d603f5781d7120e8b41ff0a867699ae547e769`,
    `afe846b641d472fa0de55aa6868f0c877dfb408f4f8e195e6ced7064632b300d`,
    and
    `0ddbb0da9a4af51466a80a7258dcf88500a966d6b2c92404fe7ddbff06241911`.
- [x] Bounded and named the Dragon's-Egg source/child family:
  - Identified fixed type-`$82` slots `$20-$25`, positions
    `(48,566)/(58,561)/(68,556)/(40,558)/(50,553)/(60,548)`, initializer
    `02:433F`, and steady dispatch `02:4345` as the manual's six Dragon's
    Eggs.
  - Identified invisible fixed slot `$1F`, initializer `02:42EB`, and steady
    dispatch `02:42F6` as the shared live-egg controller.
  - Proved the routed slot-`$22` hit awards 10 stored units, selects impact
    command `$39`, copies `(68,556)` into dynamic slot `$64`, and installs
    type `$82` / dispatch `02:441E`.
  - Bounded next-frame transition `02:4379`, which disables the egg and
    decrements the live count from 6 to 5.
  - Proved the hatchling's fifth update awards 200 stored units, selects
    impact command `$34`, and releases slot `$64` at frame 2,429.
  - ROM-bounded the inactive all-six branch: increment `$2CFB`, set bit 0 of
    `$2D20`, submit 100 stored units, and begin the timed reset.
  - Added `tools/Analyze-DragonEggSemantics.ps1`, normalized
    `02:42F6@1` / `02:4345@1` replays, and `dc_rom_dragon_eggs`.
  - Verified byte-identical Debug/Release entry, call, and write SHA-256
    values
    `f2fa919a64354707db835f25299715964c07887380e6057ff05f4f743a49717b`,
    `a1e20d918dd1098feb4d81e301ce5e0f6b4f63d5d087d042cb6330c15a4fc1d5`,
    and
    `a486c285201f6b932f5ad29948475b1e8d2ced44df28b513ee8258fb8958e071`.
- [x] Bounded and named the three Skeleton Insects:
  - Identified fixed slots `$31/$32/$33`, positions
    `(32,614)/(132,694)/(240,641)`, types `$82/$81/$82`, and initializer /
    steady entries `02:54A3/54B9`, `02:55D3/55F9`, and `02:5543/5551`.
  - Proved Front-Spider latch `$2D14` triggers the right slot's 40-unit award
    at frame 2,182; its second write at frame 2,564 does not re-award after
    the target transitions to `02:5590`.
  - Proved Tower latch `$2CFC` triggers the center slot's 100-unit award at
    frame 3,353 and Front-Spider latch `$2D13` triggers the left slot's
    40-unit award at frame 3,466.
  - Bounded the three post-trigger timelines and retained the inactive
    secondary collision/award paths in `02:5500` and `02:5590`.
  - Proved the target scopes call no audio producer; same-frame commands
    `$34/$33` remain owned by the source Front Spider or Tower.
  - Added `tools/Analyze-SkeletonInsectSemantics.ps1`, normalized
    `02:54B9@1` / `02:5551@1` / `02:55F9@1` replays, and
    `dc_rom_skeleton_insects`.
  - Verified byte-identical Debug/Release entry, call, write, and actor-state
    SHA-256 values
    `c0fc725d3e4a44094041fe74b900f27f1e896c881d310ea69836cf91d760a031`,
    `d8ef9a904913b93b0edbd8d6b014ffb90a4c5fb70bfa59d27d4aace0a41db8bb`,
    `024c0aebd843222cfea420e4144b2134004d64755927cb21ceddf27dbbd78f86`,
    and
    `c61ab3a4f0c35687c64ff7991f77e12ecf2864d8bb06ec8318a901db3c252d0a`.
  - Passed the focused Debug/Release regression and the complete 43-test
    Release suite.
- [x] Bounded and named the upper-table Sorcerer family:
  - Identified hidden controller slot `$16`, initializer `02:578E`, and eight
    fixed type-`$82` Sorcerers at slots `$17-$1E`, initializer `02:57F8`,
    with route parameters 0-7.
  - Proved frame-873 deployment state `02:5808` stages every child at signed
    position (-10,196), assigns its movement vector, and increments live
    count `$2D0F` from 0 to 8.
  - Bounded the 44-frame-staggered `02:586F -> 02:5886 -> 02:58C2`
    transitions; each arrival state lasts 225 calls and deployment count
    `$2D10` reaches 0 at frame 1,474.
  - Captured 18,152 orbit calls through frame 3,600, with every call taking
    the no-hit return and live count remaining 8.
  - ROM-bounded the inactive two-hit rule: 10 stored units and command `$29`
    per hit, then a 50-unit completion award, color/group helpers, impact,
    and conditional regeneration.
  - Added `tools/Analyze-SorcererSemantics.ps1`, normalized
    `02:57AD@1` / `02:5886@1` / `02:58C2@1` replays, and
    `dc_rom_sorcerers`.
  - Verified byte-identical entry, call, write, and actor-state SHA-256
    values
    `62d81fe4d2f68a22e56661826709bc30ab0c95b63666f16c30bed044d9734261`,
    `1e747ab17db2e5fa1521d1e755672a9da6a44063dccd959f7b11816ba863ad12`,
    `8ef36479191f192a24b5fc3eb79fde344469c02ab99d0ccd49d535d68e09ac99`,
    and
    `5060cead3f36f5e807e1804fcba4b2675455a9d259a8ac455e45f8e814972407`.
  - Passed the focused Debug/Release regression and the complete 44-test
    Release suite.
- [x] Bounded the Small-Soldier Side-Star sources and deferred completion:
  - Identified fixed source slots `$34/$35`, positions `(32,350)/(232,286)`,
    initializers `03:7D4D/7DE9`, mirrored collision IDs, and side-selected
    live counters `$2D16/$2D17` as the lower-left and upper-right Side Stars.
  - Proved delayed states `03:7D6F/7E0D` call shared spawner `03:7E87` in
    five frame-paired waves, creating the ten dynamic Small Soldiers in slots
    `$62/$63/$65/$66/$68/$69/$6B/$6C/$6E/$6F`.
  - Captured both group counts rising from 0 to 5 and the source dispatches
    entering steady waits `03:7D85/7E23` at frame 1,132.
  - Closed completion state `03:7F81`: slot `$62` exits immediately, while
    subtype-`$0F` slot `$69` self-holds for eight calls and then adds the
    deferred 50-unit award, decrements `$2D17`, creates its impact effect,
    clears all 32 arrays, and frees.
  - ROM-bounded the inactive all-five rule: award 200 stored units, cycle the
    Side-Star appearance, and regenerate that group.
  - Added `tools/Analyze-SmallSoldierSourceSemantics.ps1`, normalized
    `03:7D85@1` / `03:7E23@1` / `03:7E87@1` / `03:7F81@1` replays, and
    `dc_rom_small_soldier_sources`.
  - Verified byte-identical entry, call, write, and selected-state SHA-256
    values
    `c186bd9b395371e22a41e66f832561bd21c0632f349cdf44b00cf7a9b7bcd707`,
    `575c226bcdfbbae6ea4f889e9b4cbac75f25e7d508e210776bb92d550e5b61f4`,
    `336a25613adff3f64c608761a9cf6c1cea74053704cf6625067d9d1bb41ee774`,
    and
    `ab06bfa930972c7aca88244ce26bdf22b73eb0cf915cb6b5f9e2a085d1ee8af8`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 45 tests.
- [x] Bounded and named the central Giant-Face family:
  - Identified fixed slot `$36`, position `(128,292)`, initializer
    `03:7428`, and steady request dispatch `03:74D8` as the persisted
    Giant-Face form controller.
  - Identified hidden animator slot `$37`, mouth/bonus slot `$38`, and
    crown/helmet collision slot `$39`, including their fixed positions,
    types, initializers, and steady dispatches.
  - Captured the staged controller transition at frames 872/873/941, the
    form-0 clear, and 5,608 routed family calls with all request, collision,
    award, audio, and bonus paths inactive.
  - ROM-bounded request bit 0 wake behavior, request bit 1's six-vector
    transformation cycle, persisted forms `2,3,4,5,5,7`, explicit form-6
    mouth opening, and hidden animation modes.
  - ROM-bounded 3-unit crown/closed-mouth hits with command `$20` and the
    open-mouth 100-unit capture with command `$41`, ball position
    `(144,344)`, and bonus-stage-4 selection.
  - Added `tools/Analyze-GiantFaceSemantics.ps1`, normalized
    `03:74D8@1` / `03:767E@1` / `03:76FA@1` / `03:7726@1` replays, and
    `dc_rom_giant_face`.
  - Verified byte-identical entry, call, write, and selected-state SHA-256
    values
    `e00d2caf31a341328834cebb8deadaf3fa456f06184e415d45806acaf37d64dd`,
    `357c7cc9550d7dd5c4804cd1cb76ff62fec6f4db8621b8b3e36c383c369ba3d7`,
    `fc422de12e8b181f7456cb3d3d584a321be62eefeb73398bc2555ad3632f7c48`,
    and
    `804e0ddee017e941be7cb222c24f4922fb1064b255d1d03d5fdd22c515925946`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 46 tests.
- [x] Bounded the Giant-Face helmet activation channels:
  - Identified fixed type-`$84` slots `$2A/$2B`, positions
    `(134,292)/(158,292)`, initializers `02:4A5C/4A64`, and selectors
    `$13/$1D` as the left/right openings atop the Giant Face helmet.
  - Proved both enter shared setup `02:4C78`, retain original-state marker
    `4C78`, and use steady dispatch `02:4C91`.
  - Captured 1,748 idle calls split 874/874, 1,748 timer decrements, 438
    reload/appearance advances, and 146 four-state appearance resets.
  - ROM-bounded the inactive activation: evaluate the special-condition
    fields, set Giant Face request bit 0, run the channel animation, award
    3 stored units with command `$20`, emit `$25`, route the ball using the
    side selector, and restore the steady dispatch.
  - Added `tools/Analyze-GiantFaceHelmetSemantics.ps1`, normalized
    `02:4C91@1` replay, and `dc_rom_giant_face_helmet`.
  - Verified byte-identical entry, call, write, and selected-state SHA-256
    values
    `fb473f9da4c43e0eea2abcfed06c5510b3d2c6a457bcbd6e791c5b91be6cb057`,
    `48a1a89f272e80679e859d6ecdc34d4102d864985fba5ba64bdde12d0642e64b`,
    `1d70d3313ad65639066692b2721f1df8d5ffc4d041e5625a3f84fceaf3dcd4fa`,
    and
    `6692ff8cb8ddb9927b57e2844dea70ea8c11afc48a3ac18707350690b83c5d26`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 47 tests.
- [x] Bounded the upper-table Four-Star Wall Pockets:
  - Identified fixed type-`$84` slots `$3E/$3F/$42/$43`, positions
    `(80,214)/(254,214)/(86,76)/(246,76)`, and initializers
    `02:49FC/4A0E/4A20/4A32` as the lower-left/right and upper-left/right
    Wall Pockets (the detailed guide's 4 Stars).
  - Matched their `$3700,X` bonus-stage IDs 1/5/3/6 and `$3780,X`
    zero-based Roman selectors 5/3/7/1 to VI/IV/VIII/II.
  - Bounded common setup `02:4A6C`, steady dispatch `02:4A93`, 1,008
    collision-free idle calls, 254 appearance advances, and 84 resets.
  - ROM-bounded normal entry: advance Sorcerer color `$2D40` modulo eight,
    award 10 stored units, animate, emit `$25`, and route the ball.
  - ROM-bounded the stopped matching-arrow entry: clear `$2CFA`, emit `$41`,
    award 100 stored units, and pass the pocket's stage ID to the shared
    scene-transition helper.
  - Added `tools/Analyze-FourStarWallPocketSemantics.ps1`, normalized
    `02:4A93@1` replay, and `dc_rom_four_star_wall_pockets`.
  - Verified byte-identical entry, call, write, and selected-state SHA-256
    values
    `eafb2abd72e818a258eee861904f0abb097cb1703c885a66316b08a318fad3d9`,
    `2f86b84a3997f8a84239a554ecb34523fc94c564f337ff446b8df61f91b1d64b`,
    `d4fe55cf0b4b975eecd160accace5b3f0e753bf05b99b2e6a5c7b7d375f256b8`,
    and
    `6bb4604bcffd88960c62c76ae98bc5a11984a4711bef456da014b3d31cf1de4e`.
  - Passed the focused Debug/Release regression and the complete 48-test
    Release suite.
- [x] Bounded the hands guarding the upper Wall Pockets:
  - Identified mirrored fixed slots `$40/$41`, positions
    `(72,55)/(232,55)`, appearances `$14/$15`, initializer `02:5972`, and
    live dispatch `02:5975` as the guide's hands above pockets `$42/$43`.
  - Proved their delayed scheduler uses `(RNG & $3F) + $0080`, then gates on
    shared upper/lower pocket byte `$2D33`.
  - ROM-bounded the temporary type-`$83` collision surface, response
    selector 4, collision wait, two-step close/open animation, and random
    `$B0-$EF` ball response.
  - Proved the hand family contains no score-entry or audio-command call.
  - Captured both frame-872 initializers and 14 scoped writes. The route
    never dispatches live `02:5975`, so the physical gate remains statically
    bounded.
  - Added `tools/Analyze-UpperWallPocketHandSemantics.ps1`, normalized
    `02:5972@1` replay, and `dc_rom_upper_wall_pocket_hands`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `defcf166fa1b1bf77b4e415f57d082f06f60d73d8f73afbbad036b46713062d9`,
    `98595fe928d45a59437f77f804b662a67f1860c2358c649c109b48d2d420ed92`,
    `1f7ce0a73fc4e1c5c9dcc3953563ea48def6dfa37b482d9e827a57cbe23f3490`,
    and
    `e4521a0c6afa7d98deba96c4f21032e2ff7e702629371a2ac07b9c097bd3dc0b`.
  - Passed the focused Debug/Release regression and the complete 63-test
    Release suite.
- [x] Bounded the Little-Skull / Big-Star eight-direction feature network:
  - Identified fixed slots `$46/$47` and `$49/$4A` as the guide's two
    Little-Skull pairs, hidden slots `$45/$48` as their pair controllers,
    and central slot `$4B` as the Big Star.
  - Proved each pair owns live count `$2CF5,Y`, regeneration flag
    `$2CF8,Y`, and one direction bit in `$2D04`; a Little-Skull collision
    awards 20 stored units (2,000 displayed points), creates the default
    command-`$2D` impact, disables the actor, and decrements its pair count.
  - ROM-bounded the Big Star's signed speed states
    `+64/+32/+16/0/-16/-32/-64`, durations `3/6/8/0/8/6/3`, eight-state
    arrow, stopped selector `$2D18`, and numeral-V special-condition branch.
  - Closed the exact selector registry: I is Undead Head slot `$4D`,
    III/VII are Side Worm slots `$4F/$4E`, II/IV/VI/VIII are the four
    mapped Wall Pockets, and V is the Big-Star Warlock/session condition.
  - Captured 11 frame-872 initializers and 2,650 hidden clear-latch calls.
    The feature actors remain camera-inactive; 2,622 calls are 21-cycle
    zero-write leaves and 28 contain interrupts.
  - Added `tools/Analyze-BigStarFeatureNetworkSemantics.ps1`, normalized
    `02:445B@1` replay, and `dc_rom_big_star_feature_network`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `09eab95b760dce5e1113ff412a35f4e554d1f8c83008dd2b8688f5634af6d3e6`,
    `7677de9c364d382d657efbea5d2272322f50ba42c8253948efe6bb5b3acd6f36`,
    `4be8e9c06b82726a381b316d6eee5f7f5a6b725e75b38c7770e0c17f4c432ce0`,
    and
    `99f8a2f2ba146a08abc02fb2ebcc52f1f5fe0504636628cca249c95616776627`.
  - Passed the focused Debug/Release regression and the complete 62-test
    Release suite.
- [x] Reconciled the final fixed system/presentation controllers:
  - Identified hidden type-`$80` slots `$5C-$5E`, initializers
    `03:7B1F/7B6D/7CA7`, and live dispatches
    `03:7B27/7B7A/7CAF`.
  - Proved slots `$5C/$5D` share an eight-frame gated cadence. The first
    submits selectors `$02/$03/$07` to exported entry `08:4030` through the
    banked-call wrapper; the second renders three marker descriptors through
    phase table `0,1,2,3,4,3,2,1`.
  - ROM-bounded slot `$5E` as a collision/scene-response controller with a
    command-`$08` nonzero-scratch branch, ball-response selector `$78`, and
    16:14 countdown. The route never enters that branch and `$2D38` remains
    zero.
  - Captured the three frame-872 initializers, 2,650 steady calls per
    controller, 332 triple submissions, 332 marker updates, and three
    normalized zero-write samples.
  - Added `tools/Analyze-FixedSystemControllerSemantics.ps1` and
    `dc_rom_fixed_system_controllers`, completing bounded gameplay or
    compatibility roles for all 88 fixed records and 59 initializer
    identities.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `fade5f879a62c2e9aadcac7bed74374c2ede950de526632a72e0c611db619d7b`,
    `da3d925ec68f9295224ccec32fe500708b17f09404e4f8f99b2527a8cd736261`,
    `66e67562ba3f7702c485ab0ff10d0a3d0b84a24b1116c1aae2834c99e1e96588`,
    and
    `22e191e0f6dfb327096c30089d63f26c08a1e677cef0ffe0a41a376a7058f450`.
  - Passed the focused Debug/Release regression and the complete 64-test
    Release suite.
- [x] Added a controlled recording-time work-RAM mutation boundary:
  - Added exact-size `PceRuntime::import_work_ram` implementations for the
    Beetle runtime and dependency-free stub, complementing copied export
    without exposing emulator-owned pointers.
  - Added repeatable `--work-ram-patch FRAME:ADDRESS:VALUE` recording seeds,
    artifact metadata, `$2000-$3FFF` address validation, selected-recording
    frame bounds, and duplicate frame/address rejection.
  - Added four CLI failure regressions plus `dc_rom_work_ram_patch`. A
    frame-3,600 `$3FFF=$A5` seed changes the final work-RAM SHA-256 to
    `c858cafb51a462658046c9529e5d4c795d1b63bfd09db61198c9c149a77adc96`
    in both configurations.
  - Verified Debug/Release artifact SHA-256 values
    `1e8540272bb49e7b35a1cba902feed6723c3752ed6409ec2c276f77136d55c79`
    and
    `f9ad5753938bbab4d87a9b160b57c9cbcc1fe17610b3bfef892b27f082421b39`.
  - Mapped full/category-1/category-2 loader requests to
    `00:F935`, `00:F829` plus `01:C826`, and `00:F052`. Category 1 owns
    seven persistence-oriented records; category 2 owns 17 respawn-oriented
    records.
  - Proved passive 12,000- and 60,000-frame extensions do not enter either
    selective loader. Initial position/velocity, occupancy, and isolated
    `$CF` seeds also do not satisfy the complete transition prerequisites.
  - Passed the focused CLI and Debug/Release ROM regressions and the complete
    69-test Release suite.
- [x] Activated both selective normal-table loaders and the short bonus
  return transition:
  - Traced active ball handler `01:C765` through the exact `01:DD6E`
    normal-table drain threshold at signed Y=`$02D0`.
  - A frame-3,000 ball-slot Y=`$0300` seed reaches `01:C826` and category-1
    export `02:4003` at frame 3,287. The 98,931-master-cycle call reloads
    exactly seven persistence slots through 2,095 scoped writes.
  - A frame-1,300 ID/`$53.7` seed enters the short transition branch,
    bypasses the stage-layout loader, returns through `00:EF8B` at frame
    1,815, and reaches category-2 export `02:4006` at frame 1,851. The
    133,077-master-cycle call reloads exactly 17 respawn slots through 3,117
    scoped writes.
  - Corrected the category-2 banked-call site from `00:F05E` to `00:F052`;
    `F05E` is the following, distinct `08:4000` export.
  - Added `tools/Analyze-SelectiveSceneSemantics.ps1`,
    `tools/Analyze-BonusReturnSemantics.ps1`, `dc_rom_selective_scene`, and
    `dc_rom_bonus_return`.
  - Verified byte-identical Debug/Release category-1 entry, call, and write
    SHA-256 values
    `3d3014febaf1421cf4b5bc0bd91f97891a9d2ac6983af80b2cc0fed164245a2f`,
    `5b8c39149b64176c9eac0569886dab76123f0a04da420f2b30116eb5a0ec9e0e`,
    and
    `59751f692de3fbd36a5666fd14492c982e6fb40c46baf5f5129186bb6f299a6c`.
  - Verified byte-identical Debug/Release BS-1/category-2 census, entry,
    call, and write SHA-256 values
    `c88040c5de53e4d55e218c259a1a920d8e3ee05804832a3c5263ae52aba9b9ce`,
    `fe018ea73e784fe401ce98af51eb5df090891a75f7571a8b3173055819faa98c`,
    `2a76f0ad803d2304d44a2e5516ff5ed4a8f4607e123a7be62386d94d6079a1c4`,
    and
    `026a4bccd25048321aed98599b76b6a0abb7936fbbfed58b4bdad9d245de14c8`.
  - Passed both focused Debug/Release regressions and the complete 71-test
    Release suite.
- [x] Activated and bounded all six bonus-stage layouts:
  - Identified `$53.4` as the complete stage-entry request. It selects
    `01:C1BA`, calls full initializer `00:ED86`, preserves the normal table,
    and invokes stage-layout export `04:4000`.
  - Parameterized stage ID `$55` from 1 through 6. BS-1/2/3/4/5/6 load
    16/8/12/20/24/7 dynamic records at frames 1,341-1,342.
  - Bounded exact loader budgets of 76,704 / 33,240 / 52,353 / 91,932 /
    114,144 / 28,734 master cycles and 590 / 305 / 422 / 600 / 716 / 278
    scoped writes.
  - Added `tools/Analyze-BonusLayoutSemantics.ps1` and
    `dc_rom_bonus_layout`, guarding all six ROM tables and 87 records,
    dynamic allocation, initializer pointers, direct actor fields, calls,
    cycles, and writes.
  - Dynamically activated formerly static RNG consumers `04:47C7` five
    times on BS-1 and `05:7217` twice on BS-6. The other seven bonus-bank
    consumers remain event-path dependent on the no-input routes.
  - Passed the focused BS-1 Debug golden, all-six Release regression, and
    the complete 72-test Release suite.
- [x] Bounded and named the upper-left Giant Door / Wooden Gate:
  - Identified category-1 fixed slot `$4C`, world position `(64,254)`,
    initializer `03:6294`, type `$82`, and steady dispatch `03:62B8` as the
    manual's Giant Door and the detailed guide's Wooden Gate.
  - Proved initialization clears handshake bytes `$2D06-$2D09`, installs a
    three-hit counter, collision selector `$10`, and completion selector
    `$81`.
  - Bounded 676 no-hit steady calls: 669 are 27-cycle zero-write leaves and
    seven contain periodic interrupts.
  - ROM-bounded the 50-unit per-hit award, third-hit 500-unit completion
    award, common impact transition, and four-channel completion handshake.
  - Added `tools/Analyze-GiantDoorSemantics.ps1`, normalized `03:62B8@1`
    replay, and `dc_rom_giant_door`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `a58c119ca3ff2027db4f1ff36b9489a40ad477e5d8a00a08511cbba8bbd5c45f`,
    `44d2f8c8b9c2fd5ec20a27aaec5b0bbe379e75bb163cde3b296402c30e9cccff`,
    `d4fb4aabcf69de8c9b6dd89349ffe4afbc61dc55af4197aedb48fe69ba7ed05e`,
    and
    `64e95fd177928acbb6416a3048b3ceb6c12babde77d3279c14773697e5774cdc`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 49 tests.
- [x] Bounded the four-Knight queue behind the Giant Door:
  - Identified category-1 fixed slots `$3A-$3D`, vertical positions
    `(32,198)/(32,182)/(32,166)/(32,150)`, initializers
    `03:734E/7353/735A/7361`, and shared steady dispatch `03:7378` as the
    detailed guide's four Knights.
  - Proved queue indices 0-3 select Giant-Door handshake bytes
    `$2D06-$2D09`; bit 0 requests release and bit 1 acknowledges completion.
  - Bounded 10,600 gate-blocked steady calls split evenly across the four
    slots. Of those, 10,129 are 123-cycle/two-stack-write waits and 471
    contain periodic interrupts.
  - ROM-bounded the four-hit counter, nonterminal 50-unit award and command
    `$26`, terminal 100-unit hit/destruction result, and actor release.
  - Added `tools/Analyze-KnightSemantics.ps1`, normalized `03:7378@1`
    replay, and `dc_rom_knights`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `920aa86f9080f3cdeb54bdbc7ed0928c3106b4429c1c12e4f2e8fbff6d772db5`,
    `d0fd76fa97368baf2f2de9620ccf1d1fa478ced140c45b9e2d099b30b3e12dd0`,
    `cfd3d8b0b096fba9f6188d4c855597d69ada545d65238fdae07cf1e7c4cdc3fb`,
    and
    `97c15d5f95355bf7a7a70a876309688a1c982f165f7c213bedcf43bc8ebc3f18`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 50 tests.
- [x] Bounded and named the middle-table Walled Star:
  - Identified fixed slot `$5B`, position `(32,302)`, initializer `03:6319`,
    type `$82`, and steady dispatch `03:6339` as the detailed guide's
    upper-left brick-walled Star.
  - Proved setup clears wall damage, installs collision selector `$10`,
    completion selector `$84`, and the wall-active state.
  - Bounded 2,650 no-hit calls: 2,601 are 54-cycle zero-write leaves and 49
    contain periodic interrupts.
  - ROM-bounded four 50-unit hits with command `$2E`, the fifth-collision
    200-unit terminal award, and subsequent access/animation/reset states;
    one complete wall lifecycle adds 40,000 displayed points.
  - Added `tools/Analyze-WalledStarSemantics.ps1`, normalized `03:6339@1`
    replay, and `dc_rom_walled_star`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `768c7d7ec36841fc76780553fce953197a3fbb62228f45a9ab17f6732d5613af`,
    `e31eab9f63b6c4b81bcd81eba32f3acba9c0feaccb8e482146a228003de9afc7`,
    `7811e173ba484531f7545ca7dcf77d268216d0073aca628b5f689caa8baecc04`,
    and
    `1e222fd4e49400bd45aaff1a23fb113b73fdb9240e60ba424c3ed4d12b03f14c`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 51 tests.
- [x] Bounded Giant Skeleton's Eyeball and its Dragon's-Egg signal:
  - Identified category-1 fixed slot `$2F`, position `(225,535)`, initializer
    `02:4DC8`, type `$82`, and steady dispatch `02:4DD9` as the manual's
    Giant Skeleton's Eyeball inside Giant Skeleton slot `$30`.
  - Proved the Dragon's-Egg count-zero branch sets `$2D20` bit 0 and the
    Eyeball consumes that exact signal to begin the Giant Skeleton
    mouth-opening state machine.
  - Bounded 1,988 dormant steady calls: 1,951 are 57-cycle zero-write leaves
    and 37 contain periodic interrupts. The routed one-egg state never sets
    the completion signal.
  - ROM-bounded the opening, ball-coordinate preservation, presentation, and
    reset states without claiming the inactive branch as dynamically
    exercised.
  - Added `tools/Analyze-GiantSkeletonEyeballSemantics.ps1`, normalized
    `02:4DD9@1` replay, and `dc_rom_giant_skeleton_eyeball`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `ead3b1b7ce2290a86dac6bf535f9ac70de7534724636e0b4e03c04cdfa985d5c`,
    `5a5b2e1fc0edc68a5930c51a0f18714e6ce1f27541b0357718d0ae64c15b8d6f`,
    `dc8a4ef320c2acaa1ddf0601f0bbaaff69c83898e48da804b72e1243d9ddea4f`,
    and
    `7d5d7067ee92eeb9c25882867ffcb3b6b91824d2dea673444fe32a2517112091`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 52 tests.
- [x] Bounded the lower-table 3 Mercy Stars:
  - Identified fixed slots `$5F-$61`, positions `(132/148/164,660)`,
    initializer `02:4F9F`, type `$84`, and steady dispatch `02:4FC1` as the
    detailed guide's three last-chance Mercy Stars.
  - Proved the activation gate requires normal table mode, `$2D52 == 1`,
    and a selected-player score below stored `$001388`, exactly 500,000
    displayed points.
  - Bounded 7,950 dormant steady calls split evenly across the three slots:
    7,787 are 54-cycle zero-write leaves and 163 contain periodic interrupts.
  - ROM-bounded the 10-unit use award, presentation/ball transition,
    `$2CFD` Three-Blobs safety activation, and triggering-actor release.
  - Added `tools/Analyze-MercyStarSemantics.ps1`, normalized `02:4FC1@1`
    replay, and `dc_rom_mercy_stars`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `1843ecb62f1d3c7575aea3d44e19aadb145a4a7bfe24dfba3b1f1c1b6fd9c291`,
    `18c7eaacc8eb696d68fe37519de54ab04823e3c0edc1854a74644de1cb7528c1`,
    `444d3b49380e4ac58eafc96fac130f6e8a757165fa304347087e988cda444eab`,
    and
    `62528a08094cea8cea98955e821398c75e0477d6e04aa5d2460ccdfccbd17fc6`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 53 tests.
- [x] Bounded and separated the lower-table Side Spiders:
  - Identified fixed slots `$28/$29`, positions `(40/240,642)`, initializer
    `03:64FA`, type `$83`, and steady dispatch `03:651F` as the detailed
    guide's outer Side Spiders, distinct from Front Spider slots `$26/$27`.
  - Proved side parameters 0/1 select collision IDs `$2C/$2D`.
  - Bounded 2,796 collision-free steady calls split equally across the two
    slots, with 336-2,370 body cycles and 9-47 writes.
  - ROM-bounded the one-hit 100-unit award, command `$34`, shared transition
    latch `$2D32`, and disappearance/transition cycle.
  - Added `tools/Analyze-SideSpiderSemantics.ps1`, normalized `03:651F@1`
    replay, and `dc_rom_side_spiders`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `1b3f785f9b8f7d71822f545bc9a90f2e54a9992fe68aa52d26c09b488779939d`,
    `409c5913a3ad8105a4c16d4c1b29197e35b3a3aee8fada9ceb2d0e8c44e1eab9`,
    `f7b8545de33f6b1fd6c6a597ca2377523a413b7f70502d3460b2d2c792719ea5`,
    and
    `4a72e3b731fe3913181c59c195431fba9e9f4a95aa2499c1b2497afedcbd3cb9`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 54 tests.

- [x] Closed the Tower family with its Guard / BS-5 entrance surface:
  - Identified fixed slot `$2E`, position `(132,510)`, initializer `03:6099`,
    type `$82`, and steady dispatch `03:60A6` as the manual's Guard beside
    Tower slot `$2D`.
  - Proved nonzero five-step counter `$2D4D` selects closed completion ID
    `$84`, while zero selects open ID `$8E`.
  - Bounded 2,650 closed updates: 2,592 are 48-cycle one-write leaves and 58
    contain periodic interrupts. Every routed selector write remains `$84`.
  - Connected the Tower's first-hit `$2CFC` arming state and subsequent
    `$2D4D` decrements to the Guard, and ROM-bounded the open BS-5 path,
    command `$41`, gate reset, and next-cycle marker.
  - Added `tools/Analyze-TowerGuardSemantics.ps1`, normalized `03:60A6@1`
    replay, and `dc_rom_tower_guard`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `102589405057af635281317de488ab08131f2a9254b67b52b98d560994e271e6`,
    `141a3045e96ddfb1f0c736850ddcbebfa6ebc18231d66f115addb311e704258f`,
    `c37bed44613fad1cb3f8a8776d182be59e5cd6cd025faa4890832ba9c52bd3e6`,
    and
    `d66b67b3a2cac7a9f8ec532124a2404a548c7a184a6d2f58030b36e891552f6c`.
  - Passed the focused Debug/Release regression; full Release verification
    now contains 55 tests.

- [x] Bounded both Screen-2 Side-Star access surfaces:
  - Identified fixed slots `$10/$12`, positions `(38,372)/(238,308)`,
    initializers `02:4A54/4A4C`, type `$84`, and steady dispatches
    `02:4BD5/4C0A` as the lower-left and upper-right Side-Star access
    surfaces, distinct from source-controller slots `$34/$35`.
  - Bounded 1,277/905 collision-free steady calls and the shared
    four-tick/four-appearance idle animation, including 65 periodic
    interrupt-bearing calls.
  - ROM-bounded both 80-unit (8,000-point) hit awards, Giant-Face
    transformation request bit `$2D0C.1`, and the upper-right
    persisted-form-zero path that selects bonus stage 6.
  - Added `tools/Analyze-SideStarAccessSemantics.ps1`, normalized
    `02:4BD5@1` / `02:4C0A@1` replays, and `dc_rom_side_star_access`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `7091984892271017079a1d98450f5e72f1242017eed26bb66c1209d917113339`,
    `4cd9ebfab43aa644483536b36f95d49dc47c02743f5d3f51d5fe2b824ddcd956`,
    `e4a2b7c0be94f44bd7593d8b0b4930a7f95aca598fd8acf90e357ddaab5279c2`,
    and
    `a8932210562ebfb1b4195fbc1b27e9094a604fa10a9255c8364726d6bf01650e`.
  - Passed the focused Debug/Release regression and the complete 56-test
    Release suite.

- [x] Closed the Walled-Star family with its inner access pocket:
  - Identified fixed slot `$11`, position `(38,308)`, initializer `02:4A44`,
    type `$84`, and steady dispatch `02:4B3F` immediately inside outer
    Walled-Star slot `$5B`.
  - Bounded 905 collision-free calls: 661 short timer calls, 144 appearance
    advances, 72 appearance wraps, and 28 periodic interrupt-bearing calls.
  - Proved the inactive collision awards 100 stored units (10,000 displayed
    points), raises `$2DBC`, and handshakes with the outer wall through
    `$2D53.0`.
  - Proved the acknowledged path uses `00:E8A4/E569` to scale one RNG byte
    into `0..5`, adds one, and selects random bonus-stage ID 1 through 6.
  - Added `tools/Analyze-WalledStarAccessSemantics.ps1`, normalized
    `02:4B3F@1` replay, and `dc_rom_walled_star_access`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `bef6b62849397616eabdbc4f63247da80689c1472925c28e4bbc438ba82f830b`,
    `1ff4a18a3f7caa572e46000abcd46598b96f3239fae0e8c75cec3a7a98aad3b8`,
    `26c396dc657674d9d4e93c3ea07cd76390ef2ae9bb12bb7878997ede8bc4a8a5`,
    and
    `dcaf467b0f5e7bc35b3051723ddebb6da2ef5f4266c8573d15f48f3b34d6c876`.
  - Passed the focused Debug/Release regression and the complete 57-test
    Release suite.

- [x] Closed the Devil-Shooter family with its four companion actors:
  - Identified contiguous type-`$80` slots `$0A-$0D` surrounding
    Devil-Shooter slot `$0E` as its presentation/control companions.
  - Proved slot `$0B` writes
    `Y = $02A8 + floor($2D15 / 2)` on all 1,260 routed calls.
  - Proved slot `$0C` caches Shooter charge and advances its appearance
    modulo three across 58 routed charge changes and 19 wraps.
  - Bounded the slot `$0A/$0D` session-gated four-/five-frame animations,
    exact dispatch transitions, and later zero-write dormant states.
  - Added `tools/Analyze-DevilShooterCompanionSemantics.ps1`, eight
    normalized state replays, and `dc_rom_devil_shooter_companions`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `8b23e18603b196bf1adb7c67527f111769099e01682d0b47625a0a40a918c9b0`,
    `31713928b949d25e0a2345febd119371e14e2d4723574d425ffc02d8adb565d2`,
    `ecfc80c0488c5fe7e98f1ffe67db35d108c4746f222345f846f6af443ad8acc3`,
    and
    `13e3373182b0982eb40b61df93d0002d6ba9004c6b2d099a0129380ec6e21211`.
  - Passed the focused Debug/Release regression and the complete 58-test
    Release suite.

- [x] Independently bounded the Three-Blobs result controller:
  - Identified fixed slot `$57`, position `(192,254)`, initializer
    `03:65BC`, type `$82`, and steady dispatch `03:65C2` as the controller
    immediately preceding visible Three-Blobs slots `$58-$5A`.
  - Captured one initializer and 2,650 steady updates. Of the steady calls,
    2,565 exit after `$2D39 != $2D3A`, 35 reach the `$2D3B` comparison, and
    50 contain periodic interrupts.
  - Proved every routed call returns at `03:65E9` without entering an
    equal-state outcome or changing selected score, bonus, or reset state.
  - ROM-bounded the all-red 100-unit award, `$53.7` flag, alternate
    equal-state awards, and randomized consecutive-state reset.
  - Added `tools/Analyze-ThreeBlobsControllerSemantics.ps1`, normalized
    `03:65C2@1` replay, and `dc_rom_three_blobs_controller`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `900701d0d15596ed2c2fc7a27dc5b130758243460d32ea48a617f41baf690927`,
    `866089232b31b10cde14def18e2833b78f21ad47e7ef02c852cdbe7263787ce2`,
    `c834a7894530277d04cde21a1d3d3d17caf2ee44180f181f8906db8e12874dff`,
    and
    `2079deeea652e94c820715c3f9f4ad805c49a47da281991642c422b3d4d69f8e`.
  - Passed the focused Debug/Release regression and the complete 59-test
    Release suite.

- [x] Closed the static output contract for every RNG consumer:
  - Scanned the complete supported ROM and proved it contains exactly 21
    `JSR C081` call sites: 12 in normal-table banks and nine in bonus-stage
    banks.
  - Classified and ROM-guarded every immediate result transform as 12 masked
    timer/offset uses, six binary selectors, and three multi-way selectors.
  - Preserved `03:6678/6697` as the two dynamically exercised consumers with
    78 exact calls, while keeping the other 19 explicitly marked as
    dynamically inactive.
  - Documented each physical-bank/logical-PC pair, mask, result domain, and
    immediate destination without assigning unobserved gameplay behavior.
  - Extended `tools/Analyze-RngSemantics.ps1` and `dc_rom_rng` without
    changing any deterministic entry, call, scoped-write, or seed-write
    artifact.
  - Passed the focused Debug/Release regression and the complete 59-test
    Release suite.

- [x] Bounded the Devil's Coffin / Shooter return-gate target:
  - Identified fixed slot `$0F`, position `(221,257)`, initializer
    `03:63E1`, type `$83`, and steady dispatch `03:63F1` as the manual's
    Devil's Coffin.
  - Connected its completion directly to the Shooter session by proving it
    clears `$53.3`, changes its own type from `$83` to `$82`, and returns to
    the initializer.
  - Captured routed contacts at frames 1,244 and 1,297. Each executes the
    exact appearance cycle `0→1→2→3→2→1→0`.
  - Bounded 684 calls and 355 scoped writes across three initializers, 643
    steady checks, 18 opening calls, two handoffs, and 18 closing calls.
  - Proved the family itself submits neither a score award nor an audio
    command.
  - Added `tools/Analyze-DevilsCoffinSemantics.ps1`, four normalized replays,
    and `dc_rom_devils_coffin`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `d98bad357038cec227531a34c354103eec68890c1f0a73795e17af7bcaf6d4fa`,
    `5ca7ca788988c56bb24209faa87a4f08ece30a9be5148a34cf7f5c0ab05a6888`,
    `9ae1c018ec68f77c49a2188b6e0950527ba70f6c13cd5ea2260f79873284068c`,
    and
    `1c3fd4bac31662487cb5c48b2389a4584526bd567a42fdcc587bf5d0cbfe3169`.
  - Passed the focused Debug/Release regression and the complete 60-test
    Release suite.

- [x] Bounded the seven upper-chute Gray Blobs and their Three-Blobs link:
  - Identified fixed slots `$50-$56` as the guide's seven Gray Blobs from
    their unique upper-chute row, shared initializer `03:6876`, type `$83`,
    state `$41`, and parameters 0/1/2.
  - ROM-bounded their 64-95-tick RNG interval, four-tick appearance toggle,
    movement/collision loop, `$03E8` score submission (100,000 displayed
    points), default command-`$2D` impact burst, and actor-disable result.
  - Proved each hit raises one-frame signal `$2CFD`, which the mapped
    Three-Blobs family consumes at `03:676B` to select alternate placement
    `03:67EC`.
  - Captured all seven frame-872 initializers, 105 scoped writes, and the
    normalized `03:6876@1` sample. The route never dispatches live
    `03:68A1`, so the hit and cumulative safety rule remain statically
    bounded rather than dynamically claimed.
  - Added `tools/Analyze-GrayBlobSemantics.ps1` and `dc_rom_gray_blobs`.
  - Verified byte-identical Debug/Release entry, call, write, and
    selected-state SHA-256 values
    `75aa6fc29520a22fe188cb81242b65e9a504d36b81756b83f9865e55cf89d8b0`,
    `d8d4381418800d61f2e06b4d458b1f3a9b37d8dc74081e1d6866703c700420e8`,
    `9b5c4d13a3bd3fcaa229ef2b23fa0094c7fa57af9ec5eb898ace6e19ded38193`,
    and
    `7b5c9ebeb29c3784227aa6ccf6c367bb9c5f57a27126777022c1142dfae0ca6a`.
  - Passed the focused Debug/Release regression and the complete 61-test
    Release suite.

- [x] Bounded BS-1's first target family and no-progress result:
  - Identified five six-hit objective targets at dynamic slots
    `$34/$35/$50-$52`, five repeatable support actors at `$53-$57`, and
    controller slot `$2C`.
  - Proved the target initializer seeds six hits, collision selector `$10`,
    completion selector `$81`, and RNG timer `$80-$AF`; its dormant timer
    refresh uses `$C0-$EF`.
  - ROM-bounded 50-unit nonterminal hits, a 200-unit sixth hit, shared
    remaining-target decrement at `$2D2E`, and the controller's `$2DBA` /
    `$53.1` completion latch.
  - Proved each support contact submits 10 stored units, repeats while the
    objective is nonzero, and releases after completion.
  - Dynamically bounded the forced-entry no-progress branch: no target or
    support contacts, `$2D2E` remains five, bonus/result fields remain zero,
    result export `04:456C` is entered once, and `04:4DC4` performs 240
    result-glyph ticks with 60 advances.
  - Statically separated the zero-progress `SORRY / YOU LOST YOUR / BONUS
    POINTS` selector from the scored `YOU GET / BONUS POINTS` and completion
    `PERFECT / SPECIAL POINTS` selectors.
  - Added `tools/Analyze-BonusStage1Semantics.ps1` and
    `dc_rom_bonus_stage1`.
  - Verified byte-identical Debug/Release census, entry, call, scoped-write,
    and selected-state SHA-256 values
    `e660ac5620fefbc65147496ec757893e9e599b60ad7635dcb0ee69b1f476dec4`,
    `4be82e8645661eaa9fdb132f932181b7cc3356e94cc03a7011a2aafcfeed2139`,
    `fa8e1cfeb714e0519f5b142c8fa2f4578780ca827e7877f47da0ccebd3a7cbad`,
    `f20e5d5b610a6be765bd3225be2c73ce8517313bf5f7ee4126428646421070bc`,
    and
    `bb5d4c02783e742240c88615b971669f0f64e87715734d358596a45f1f2df315`.
  - Passed the focused Debug/Release regression and the complete 73-test
    Release suite.

- [x] Dynamically bounded BS-1's scored-progress and perfect results:
  - Proved a single frame-1,459 `$2DB7=1` seed enters scored result
    `04:45F0`, advances handshake `$2D66` to 1, clears the accumulated bonus,
    and takes ordinary cleanup `04:46A4`.
  - Proved adding `$2DBA=1` selects perfect branch `04:464F`.
  - Dynamically bounded the perfect builder's 256 additions of 1,000 stored
    units, exact `$03E800` accumulator, and 512 transfers of `$01F4` through
    the main score trampoline.
  - Bounded 181 ticks / 46 glyph advances for scored progress and 874 ticks /
    219 advances for perfect, including perfect handshake transition 1 to 2.
  - Added `tools/Analyze-BonusStage1ResultSemantics.ps1` and
    `dc_rom_bonus_stage1_results`.
  - Verified byte-identical Debug/Release scored census and selected-state
    SHA-256 values
    `f9d1d29b90774a62b495bc57c00fbe6df18c676351441f92893ceced8771979b`
    and
    `6f06098ff55ee92e2dac4d4e5b8ebdea34c9fd4cb49f4a0cb8c08fa5629a746c`.
  - Verified Release perfect census and selected-state SHA-256 values
    `3d0c8e2de33d3fe6c48ea085205fb7e0d362a2d27c8b96ba0f02fabb4f2aaefb`
    and
    `f2e47ee8eea9e533d72855e097ae2744b76df3d89f2288f22ad3b59fc21c465c`.
  - Passed the focused Debug/Release regression and the complete 74-test
    Release suite.

- [x] Dynamically bounded BS-1's target contact and timer expiry:
  - Seeded collision bit `$3834.7` and independently forced `$3B35=1`
    immediately before the five-target steady update at frame 1,403.
  - Proved slot `$34` clears its collision bit, decrements its hit counter
    from six to five, adds 50 stored units to `$2DB7-$2DB9`, emits command
    `$36`, and decrements its ordinary timer.
  - Activated previously dormant RNG consumer `04:4805`: slot `$35` reaches
    zero and refreshes its timer to `$EB`, within the guarded `$C0-$EF`
    domain.
  - Proved expiry helper `04:4836` allocates a type-`$C3` actor at slot `$63`
    after the contact impact claims `$62`, copies the source position with a
    `$18` vertical offset, installs dispatch `04:488D`, and emits command
    `$46`.
  - Added `tools/Analyze-BonusStage1EventSemantics.ps1` and
    `dc_rom_bonus_stage1_events`.
  - Verified byte-identical Debug/Release census, entry, call, and
    scoped-write SHA-256 values
    `3b819a0a88a7169e2c3d7118f607ad0e380670a3b3acb921702c2b3b4afcd82c`,
    `31fb5bca73a6f8a25942ce56674592f1ccb1a7fb86810269a030dbdf2a0c6225`,
    `7a01d9bd957c3b1ac7f9f358b4e6e965c96d8206eebaf72d7f486bea8c2dcf2e`,
    and
    `8632ddac95070c188ea1e16626c34c1a0d8fb23f1cc9f7862e3c50fbd6018232`.
  - Passed the focused Debug/Release regression and the complete 75-test
    Release suite.

- [x] Bounded BS-2's first emitter and target family:
  - Identified the eight-record layout's transient setup actor, slot-`$2C`
    controller `04:4E8F`, paired emitter records at `$34/$35`, and shared
    allocator `04:4FBA`.
  - Proved the controller initializes objective `$2D2E` to `$1C` (28).
  - Proved the first emitter initializes five emissions, reloads its cadence
    to `$30`, and decrements the batch after each allocation.
  - Dynamically bounded a target allocation at slot `$54`: dispatch type
    `$C2`, copied position/shape/subtype, selectors `$10/$81`, type `$84`,
    entrance delay `$10`, one hit, and live dispatch `04:5051`.
  - Dynamically reproduced a genuine target contact at slot `$53`: 50 stored
    units, response selector `$35`, lane counter `$2D26` balanced `0->1->0`,
    and generic actor release.
  - Proved this contact leaves the global 28-count objective unchanged,
    separating target hits from the still-unmapped movement/resolution
    decrement.
  - Added `tools/Analyze-BonusStage2TargetSemantics.ps1` and
    `dc_rom_bonus_stage2_targets`.
  - Verified byte-identical Debug/Release census, entry, call, scoped-write,
    and selected-state SHA-256 values
    `0df2459325e5aaac657e588346b2ca5125d59f56e95f49a881e0eebf88e2996e`,
    `8a941d2af5bebdbf096c775b4dae77329d7cbb9ddcbff5cbb38b22e66a108420`,
    `f4b01ddc19c7289b390d1f12850eef4b620a5cb308fe1f2ae1d3d329ddba2913`,
    `7a1300ffb2144e1ddd92322ffed3ad1faaafcafb8c050ac69f950b95e41a337e`,
    and
    `138e1f0d90ba8107e655740306deac70f02f69b8bb114d2c9dbf16a9fb8bb882`.
  - Passed the focused Debug/Release regression and the complete 76-test
    Release suite.

- [x] Bounded BS-2's movement/resolution path, remaining actors, and results:
  - Activated both five-target emitters at slots `$34/$35`, proving subtype
    0/1 allocation at slots `$54/$55`, independent live counters
    `$2D26/$2D27`, 48-tick cadence reloads, and batch decrements from five to
    four.
  - Identified the exact 28-cell objective as an 8-by-5 perimeter: full
    top/middle/bottom rows plus the two endpoints of each intervening row.
  - Dynamically routed a valid cell `(0,0)` contact through `04:5260` and
    four `04:52ED` tile updates. The contact adds 50 stored units, the four
    resolved tiles add 100 each, and `04:52E9` decrements `$2D2E` once from
    28 to 27.
  - Bounded the remaining stage-specific layout initializers: slot `$50`
    stages the ball at `(63,205)` with continuation `04:5166`, while slot
    `$51` installs repeatable capture dispatch `04:5192` and its 10-unit
    ROM score contract.
  - Reproduced BS-2's unmodified zero-progress timeout as 240 glyph ticks
    and 60 advances without a special award.
  - Reproduced the targeted completion result. The zero-objective controller
    loads multiplier `$32` (50); the shared builder adds 1,000 stored units
    50 times and transfers the resulting 50,000 units in 100 chunks of 500.
  - Added `tools/Analyze-BonusStage2ResolutionSemantics.ps1` and
    `dc_rom_bonus_stage2_resolution`.
  - Verified byte-identical Debug/Release resolution census, entry, call,
    scoped-write, and selected-state SHA-256 values
    `e4104154585c0062c9f26cb31551d5e0f2015e240bea74868e61947c3d8707f5`,
    `70e149e081ad494ca9c61de285e14df6a7b8df2a1876ad5e19e3bb984162a1de`,
    `11797b82527289b89446716b765ef611e5932ccef55e25ee767718c279fa3843`,
    `d4b7325d26780c5918d0f5698fafbbb96c57cf614886056774724f25255f3701`,
    and
    `52c30a3f2635e9e5e590c03e28fa3289fdb2b5789952e5594f7d5e90725370f6`.
  - Verified Release timeout census/state SHA-256 values
    `c0aa885ddd4df8e8993ccf532f567123dea9963150650c0d77e8d6184c8058ad`
    and
    `189f9ba5c9dcb5b5a1ea46308e39ccbba5e97a8b3aa0b9ea29758c025766d1f7`,
    plus completion census/state values
    `eac2656f6d53a22600db65c2dd38279a7b19d7057a482021b407d46501249beb`
    and
    `152d7d5d51820be1c7e81daeeca689319898fe3d2d05667c1b17031fc42899dd`.
  - Passed the focused Debug/Release regression and the complete 77-test
    Release suite.

- [x] Bounded BS-3's first event family, completion, and timeout:
  - Identified controller `04:53AC`, three emitter records at `04:53CD`,
    six-target batches, 29-tick cadence, and independent live counters
    `$2D28-$2D2A`.
  - Bounded allocator `04:5424` and type-`$C2` target contact continuation
    `04:54E7`: each contact adds 100 stored units, decrements its lane count,
    selects response `$2D`, and releases the actor.
  - Bounded the three eight-hit objective records at `04:55DF`. Each ordinary
    hit adds 100 stored units and command `$26`; each terminal hit adds 1,000
    stored units and `04:56A6` clears the indexed objective flag
    `$2D2F-$2D31`.
  - Reproduced zero-progress timeout as 240 glyph ticks and 60 advances.
  - Reproduced completion multiplier 20, building a 20,000-unit special
    award in 20 additions of 1,000 and transferring it in 40 chunks of 500.
  - Added `tools/Analyze-BonusStage3Semantics.ps1` and
    `dc_rom_bonus_stage3`.
  - Preserved deterministic event, call, write, state, timeout, and
    completion artifacts in the ROM-backed regression.
  - Passed the focused Debug/Release regression.

- [x] Implemented and proved the first native replacement:
  - Added a real C game-logic library and native implementation of
    `00:F241`, restoring the six cached pre-integration position bytes for an
    indexed physics slot.
  - Added fork patch `0003-huc6280-native-replacement.patch` and a
    transactional runtime API. A replacement declares final registers,
    ordered writes with entry-relative master-cycle offsets, its body-cycle
    budget, and the outer RTS where emulation resumes.
  - The core commits validated writes through its normal memory path and
    advances hardware time at each write and at the body boundary, so
    existing call and write observers remain usable.
  - Locked the initial interrupt contract to mapped no-interrupt scopes.
    An exploratory all-call `F241` replacement correctly exposed longer
    interrupt-bearing variants, so they remain assembly until faithful
    mid-routine interrupt delivery is implemented.
  - Added ordinal-gated `--replace-hook BANK:PC@ORDINAL`. The first mapped
    `00:F241@1` call runs natively; later calls continue in assembly.
  - Shadow-tested the native call against restored assembly from the same
    pre-call savestate. They match final registers, caller/return path,
    162 body master cycles, all six ordered writes and relative timings, and
    complete post-call work RAM.
  - The native run preserves the established 1,800-frame gameplay, physics
    entry, bounded-call, and ordered-write SHA-256 values
    `1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353`,
    `9442322858ad18bfdf67ccb9a05aaf2312c512e168e2c7dbcd3ab858fed3cf79`,
    `92f638ae77cad9e2964cb2bafdca6b21a2aa50187abded37480ebd55146170a4`,
    and
    `6f272290712ed6432205e7858d6fa72102623b08dece9beeb5c4cee2d3ffbbce`.
  - Added pure native unit coverage and ROM-backed
    `dc_rom_native_replacement`.
  - Passed the focused Debug/Release replacement regressions, the
    dependency-free Debug suite, and the complete 79-test Release suite.

- [x] Bounded BS-4's first event family, completion, and timeout:
  - Identified controller `05:6A10`, three indexed eight-hit objectives at
    `05:6A31/6A46`, and their paired sources beginning at `05:6A7F`.
  - Dynamically reproduced ordinary and terminal objective contacts. Both
    add 100 stored units and command `$2E`; the terminal branch adds 1,000,
    clears the indexed flag `$2D2F-$2D31`, and releases the objective.
  - Bounded source continuation `05:6ACC` and allocator `05:6B16`. They
    create a type-`$C2` actor at slot `$66`, copy its position and objective
    index, install dispatch `05:6B7E`, and link it to source slot `$35`.
  - Reproduced linked-target contact `05:6C06`: it adds 100 stored units,
    selects response `$3B`, and enters the ROM continuation that allocates
    four orbiting children at `05:6D27`; the retained child-contact path at
    `05:6D7F` has the same 100-unit award.
  - Reproduced zero-progress timeout as 240 glyph ticks and 60 advances.
  - Reproduced completion multiplier 20, building a 20,000-unit special
    award in 20 additions of 1,000 and transferring it in 40 chunks of 500.
  - Added `tools/Analyze-BonusStage4Semantics.ps1` and
    `dc_rom_bonus_stage4`, with deterministic objective, emitter, target,
    timeout, and completion artifacts.
  - Passed the focused Debug/Release regression and the complete 80-test
    Release suite.

- [x] Bounded BS-5's first event family, completion, and timeout:
  - Identified controller `05:6E0E/6E16` and the 15 objective records at
    `05:7100/7112`, arranged as a five-by-three field.
  - Dynamically reproduced nonterminal and terminal contacts. Each adds 100
    stored units and command `$21`; the third visible-state contact adds 400,
    decrements `$2D2E`, and converts the target to its completed form.
  - Reproduced zero-progress timeout as 240 glyph ticks and 60 advances.
  - Reproduced completion multiplier 30, building a 30,000-unit special
    award in 30 additions of 1,000 and transferring it in 60 chunks of 500.
  - Added `tools/Analyze-BonusStage5Semantics.ps1` and
    `dc_rom_bonus_stage5`, with deterministic event, timeout, and completion
    artifacts.
  - Passed the focused Debug/Release regression and the complete 81-test
    Release suite.

- [x] Bounded BS-6's first event family, completion, and timeout:
  - Identified controller `05:71E3/71EB` and two indexed ten-hit objectives
    at `05:71F8/7228`.
  - Dynamically reproduced nonterminal and terminal contacts. Each adds 50
    stored units and command `$2E`; the tenth adds 2,000, decrements
    `$2D2E`, emits the destruction effect, and releases the objective.
  - Retained the objective state machine's type-`$C2` hazard allocator at
    `05:72E0`, including its RNG-derived cadence, as part of the ROM
    contract.
  - Reproduced zero-progress timeout as 240 glyph ticks and 60 advances.
  - Reproduced completion multiplier 15, building a 15,000-unit special
    award in 15 additions of 1,000 and transferring it in 30 chunks of 500.
  - Added `tools/Analyze-BonusStage6Semantics.ps1` and
    `dc_rom_bonus_stage6`, with deterministic event, timeout, and completion
    artifacts.
  - Passed the focused Debug/Release regression and the complete 82-test
    Release suite.

- [x] Activated and bounded BS-4's two linked-target RNG consumers:
  - Routed `05:6B88` 18 times through the authentic initial linked-target
    state and `05:6CB1` 22 times through the post-allocation state.
  - Proved both paths reload `$3B00,X` to `$20`, mask the RNG result with
    `$1F`, pass it to motion helper `01:C075`, and integrate through
    `01:C012`.
  - Reconstructed all 40 `00:E8B3` updates and verified the exact
    282-master-cycle, eight-write recurrence and returned high byte.
  - Added `tools/Analyze-BonusStage4RngSemantics.ps1` and
    `dc_rom_bonus_stage4_rng`, with byte-identical Debug/Release census,
    entry, call, scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regression and the complete 83-test
    Release suite.

- [x] Activated and bounded BS-6's paired hazard RNG consumers:
  - Reproduced the natural frame-1,510 hazard allocation from objective slot
    `$35` into the freed slot `$34`.
  - Proved `05:732E` transforms result `$A0` into minimum timer `$18`.
  - Proved `05:733B` transforms result `$C5` through table index/value 5
    and motion helper `01:C072` into signed velocity `$FFE8:$003B`.
  - Bounded allocator `05:72E0`: type `$C2`, live dispatch `05:734E`,
    collision selector `$13`, completion selector `$81`, command `$32`,
    12,924 master cycles, and 281 ordered writes.
  - Added `tools/Analyze-BonusStage6RngSemantics.ps1` and
    `dc_rom_bonus_stage6_rng`, with byte-identical Debug/Release census,
    entry, call, and scoped-write artifacts.
  - Passed the focused Debug/Release regression and the complete 84-test
    Release suite.

- [x] Activated and bounded the final maximum-score-ending RNG consumers:
  - Proved score saturation sets `$2CC0.7` and the session dispatcher takes
    the original `01:C239` transition into the bank-7 ending.
  - Corrected static aliases `07:68C7/68CF` to their live bank-qualified
    execution keys `07:48C7/48CF`.
  - Routed both consumers 23 times. `48C7` covers 15 of 16 geometry indices;
    `48CF` selects command `$2C` seven times and `$2D` sixteen times.
  - Bounded 184 ending-controller calls, 23 geometry-helper calls, 138
    type-`$8F` effect allocations, and all 62 shared RNG recurrences.
  - Added `tools/Analyze-MaximumScoreRngSemantics.ps1` and
    `dc_rom_maximum_score_rng`, with byte-identical Debug/Release entry,
    call, and scoped-write artifacts.
  - Isolated a framebuffer-only Debug/Release variance across frames
    1,555-1,680; work RAM, audio, and semantic traces remain identical.
  - Passed the focused Debug/Release regression and the complete 85-test
    Release suite.

- [x] Bounded the ordinary scored-but-incomplete result across BS-2 to BS-6:
  - Retained each stage's authentic nonzero objective initialization and
    seeded one stored bonus unit at the shared result boundary.
  - Proved all five routes enter `04:45F0`, take ordinary cleanup `04:46A4`,
    advance handshake `$2D66` to one, and clear `$2DB7-$2DB9`.
  - Reproduced the shared 181-tick, 46-advance presentation with the expected
    one-frame stage-loading offsets.
  - Proved none of the routes enters perfect-award builder `04:4A5B` or its
    frame-paced transfer.
  - Added `tools/Analyze-BonusScoredResultSemantics.ps1` and Release-only
    `dc_rom_bonus_scored_results`, with deterministic per-stage census and
    selected-state artifacts.
  - Passed the focused regression and the complete 86-test Release suite.

- [x] Activated the upper Wall-Pocket hand's RNG timer consumer:
  - Moved only fixed slot `$40`'s Y coordinate into the active camera window;
    the original visibility sweep activates it at frame 1,220.
  - Dynamically entered live state `02:5975` and previously inactive consumer
    `02:597D` at frame 1,221.
  - Proved RNG result `$73` masks to `$33`, builds timer `$0133`, and installs
    countdown continuation `02:5991`.
  - Bounded all 22 shared `00:E8B3` calls: 20 are 282-cycle, eight-write
    leaves and two retain the same eight direct writes around periodic
    interrupts.
  - Added `tools/Analyze-UpperWallPocketHandRngSemantics.ps1` and
    `dc_rom_upper_wall_pocket_hand_rng`, with byte-identical Debug/Release
    trace, call, scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 87-test
    Release suite.

- [x] Activated the upper Wall-Pocket hand's post-close RNG response:
  - Retained fixed slot `$40`, moved only its Y coordinate into the active
    camera window, and selected its exact `02:59F5` ROM continuation before
    the existing visibility sweep dispatched it.
  - Proved RNG result `$73` maps through `$B0 + (A & $3F)` to response
    `$E3`, advances the upper-pair continuation twice, and installs
    `02:5A00`.
  - Reproduced the `$E3`-tick continuation through zero at frame 1,447 and
    the reverse pass back to the delayed `02:597D` path at frame 1,450.
  - Added `tools/Analyze-UpperWallPocketHandResponseRngSemantics.ps1` and
    `dc_rom_upper_wall_pocket_hand_response_rng`, with byte-identical
    Debug/Release trace, call, scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 88-test
    Release suite.

- [x] Bounded the Gray-Blob movement-timer RNG consumer:
  - Used the existing maximum-score gameplay route's natural camera sweep
    to activate fixed slots `$50-$56` across frames 1,541-1,547.
  - Proved all seven first live `03:68A1` calls enter consumer `03:68B1`;
    results `$46/$10/$47/$30/$F6/$52/$70` produce movement timers
    `$46/$50/$47/$50/$56/$52/$50`.
  - Bounded each first live update at 633 master cycles and 20 writes and
    added normalized replay `03:68A1@1`.
  - Added `tools/Analyze-GrayBlobRngSemantics.ps1` and
    `dc_rom_gray_blob_rng`, with byte-identical Debug/Release trace, call,
    scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 89-test
    Release suite.

- [x] Bounded the Three-Blobs equal-state reset RNG consumer:
  - Aligned only global color bytes `$2D3A/$2D3B` with existing state zero
    at frame 1,200, preserving fixed controller slot `$57` and its authentic
    `03:65C2` equality gate.
  - Dynamically entered state-zero outcome `03:65DE`, submitted 10 stored
    score units, and reached reset consumer `03:6620`.
  - Proved RNG result `$5D` masks to 1 and installs the consecutive
    modulo-four states `1/2/3`; the shared latch clears after its eight-tick
    continuation.
  - Added normalized replays `03:661B@1` and `03:6620@1`.
  - Added `tools/Analyze-ThreeBlobsResetRngSemantics.ps1` and
    `dc_rom_three_blobs_reset_rng`, with byte-identical Debug/Release trace,
    call, scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 90-test
    Release suite.

- [x] Bounded the Little-Skull gray-release RNG consumer:
  - Preserved fixed slot `$46`, moved only its Y coordinate into the active
    camera window, and selected exact gray-state initializer `02:458A`.
  - Reintroduced the actor after that initializer restored its layout Y,
    allowing the original camera sweep and dispatcher to enter release
    continuation `02:45A4` and consumer `02:45C9`.
  - Proved RNG result `$75` selects pending value 2 at `$2D04`, restores the
    live actor fields, and installs ordinary continuation `02:4515`.
  - Added normalized replays `02:45A4@1` and `02:45C9@1`, reproducing
    768/606 master cycles and 23/18 ordered writes.
  - Added `tools/Analyze-LittleSkullRngSemantics.ps1` and
    `dc_rom_little_skull_rng`, with byte-identical Debug/Release entry, call,
    scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 91-test
    Release suite.

- [x] Bounded the Sorcerer zero-live-count RNG consumer:
  - Cleared only fixed group controller `$16`'s existing live count `$2D0F`
    at frame 1,200, retaining its authentic `02:57AD` dispatch.
  - Reproduced the 128-tick delay and entered `02:57BA/57C2` 18 times through
    the controller's original 131-frame reset cycle.
  - Proved each RNG low bit selects collision value `$28/$29`, reloads
    deployment count `$2D10=8`, clears `$2D0B` two frames later, and returns
    to `02:57AD`.
  - Added normalized replays `02:57AD@259`, `02:57BA@1`, and `02:57C2@1`.
  - Added `tools/Analyze-SorcererRngSemantics.ps1` and
    `dc_rom_sorcerer_rng`, with byte-identical Debug/Release entry, call,
    scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 92-test
    Release suite.

- [x] Bounded the Undead-Head post-trigger selector RNG consumer:
  - Preserved fixed slot `$4D`, moved only its Y coordinate into the active
    camera window, and selected exact ROM continuation `03:6248`.
  - Proved consumer `03:625D` maps RNG result `$73` to response value `$09`,
    marks the ball and actor, and stores continuation `$6279`.
  - Bounded the shared response handoff to state `$4D2D`.
  - Added normalized replays `03:6248@1` and `03:625D@1`, reproducing
    903/810 master cycles and 29/26 ordered writes.
  - Added `tools/Analyze-UndeadHeadSelectorRngSemantics.ps1` and
    `dc_rom_undead_head_selector_rng`, with byte-identical Debug/Release
    entry, call, scoped-write, and selected-state artifacts.
  - Passed the focused Debug/Release regressions and the complete 93-test
    Release suite.

- [x] Bounded the final three normal-table RNG consumers:
  - Used the Devil Shooter's natural frame-1,084 reset state with temporary
    automatic-input mode `$2254=1` to enter `02:5148`; result `$E7` maps
    through `$40 + (A & $3F)` to selector `$67` for fixed slot `$0E`.
  - Routed the Giant-Face mouth actor from instruction-aligned continuation
    `03:7821` into consumer `03:7834`; even result `$58` selects vector `$07`
    and signed ball velocity `(+12,+62)`.
  - Retained fixed Undead-Head slot `$4D`, moved it into the camera window,
    and selected launch continuation `03:6165`; consumer `03:6177` maps even
    result `$70` to vector `$17` and signed ball velocity `(-12,-62)`.
  - Proved all three shared `00:E8B3` calls retain the exact eight-write
    recurrence. The Devil-Shooter and Giant-Face calls are 282-cycle leaves;
    the Undead-Head call retains the same writes around one periodic
    interrupt.
  - Added normalized replays `03:7821@1` and `00:E8B3@1`.
  - Added `tools/Analyze-RemainingNormalRngSemantics.ps1` and
    `dc_rom_remaining_normal_rng`, with byte-identical Debug/Release census,
    entry, call, scoped-write, and selected-state artifacts.
  - Verified SHA-256 values
    `17b63292db225e089e87b0c8216ef49414a300dcecb71ea73db51fd25b4c7790`,
    `3352122bad7e6cebb1040d5a5299787a8cd8df98fc4808ef920c5b2ea5ad73fa`,
    `c2924c92ee7b63443c6be2a13ed11cbc800fd4f761dc779a411bc4dd13c39d00`,
    `9f0eb002eb61581bef1c9465c4f12782c3ce0716d60c7aadad330cc613cdc3f9`,
    and
    `88efd2fa575124ba3fd9167266d1fd9026547583c9487645869cfb96c45fa55d`.
  - Passed the combined Debug/Release regressions and the complete 94-test
    Release suite.

- [x] Reproduced BS-6's natural perfect outcome:
  - Selected BS-6 before the authentic stage initializer at frame 1,242,
    then drove both indexed objective actors through genuine terminal-contact
    helpers at frame 1,304.
  - Proved the two terminal paths award 4,100 stored units in total and
    decrement `$2D2E` from 2 to 0 without patching the objective count.
  - Proved controller `05:71EB` observes all-clear on frame 1,305, enters
    shared completion `04:44DB` with multiplier 15, and remains scheduled
    through 87 completion-animation returns.
  - Dynamically captured the original completion path writing `$2DBA=1` at
    `04:4560` on frame 1,457; the route has no result-boundary `$2DBA`
    patch.
  - Reproduced result export `04:456C`, perfect branch `04:464F`, transfer
    of the earned 4,100 units, construction of the 15,000-unit special
    award, its 30 transfers, both handshakes, and the complete
    400-tick/100-advance presentation.
  - Added `tools/Analyze-BonusStage6NaturalPerfectSemantics.ps1` and
    Release-only `dc_rom_bonus_stage6_natural_perfect`, with deterministic
    census, entry, and selected-state artifacts.
  - Verified SHA-256 values
    `fef4dc91446e524de66892be7692b88d7782cb0113cde447c569a3c8a7b1c08a`,
    `7e05d90d709b89945587846e1b4b1080dc5ab9d9212186c2af8dc56910b0f2e1`,
    and
    `d1dd5a31f20306e8aa776b599707d3c9bcbf65137730ffe2270a35c925a4ad4b`.
  - Passed the focused Release regression and the complete 95-test Release
    suite.

- [x] Reproduced BS-5's natural perfect outcome:
  - Used the same authentic early-selection boundary and retained the exact
    five-by-three target slots `$51-$5A/$62-$66`.
  - Routed all 15 original `05:7112` terminal contacts at frame 1,304,
    awarding 7,500 stored units and decrementing `$2D2E` through every value
    from 15 to 0 without patching the objective count.
  - Proved controller `05:6E16` enters shared completion `04:44DB` with
    multiplier 30 at frame 1,305 and the 87-step animation owns the
    `$2DBA=1` write at `04:4560`.
  - Reproduced transfer of the earned 7,500 units, construction of the
    30,000-unit special award, its 60 transfers, both handshakes, and the
    complete 437-tick/110-advance presentation.
  - Added `tools/Analyze-BonusStage5NaturalPerfectSemantics.ps1` and
    Release-only `dc_rom_bonus_stage5_natural_perfect`.
  - Verified census, entry, and selected-state SHA-256 values
    `5232409f1434cd2f8223eae6ea7303b02325e54205e6f895f3ab1f21894e8033`,
    `2ca1575931a1fa892e9755b1cf09d685c3195fced3b82f88780a21f3373e267b`,
    and
    `9ffa6901309a6fed1deac690038f1cef1eda42d2cd4906bf9b60b863f3d7d2aa`.
  - Passed the focused Release regression and the complete 96-test Release
    suite.

- [x] Reproduced BS-4's natural perfect outcome:
  - Selected BS-4 before the authentic stage initializer at frame 1,242 and
    retained indexed objective slots `$34/$52/$56`.
  - Routed all three original `05:6A46` terminal contacts at frame 1,304,
    awarding 3,300 stored units and clearing objective flags
    `$2D2F-$2D31` without patching those flags.
  - Proved controller `05:6A1E` enters shared completion `04:44DB` with
    multiplier 20 at frame 1,305 and remains scheduled through all 87
    completion-animation returns.
  - Dynamically captured the completion-owned `$2DBA=1` write at `04:4560`
    on frame 1,457; the route has no result-boundary latch seed.
  - Reproduced transfer of the earned 3,300 units, construction of the
    20,000-unit special award, its 40 transfers, both handshakes, and the
    complete 408-tick/102-advance presentation.
  - Added `tools/Analyze-BonusStage4NaturalPerfectSemantics.ps1` and
    Release-only `dc_rom_bonus_stage4_natural_perfect`.
  - Verified census, entry, and selected-state SHA-256 values
    `39f931765ba0dff6b110b5f0098f2a19411d409397b4f2a251c27a8136a6f90c`,
    `b3fee160fc71808724f51506053c1f484df59156ecfe5c0c593ce3a8c904de6e`,
    and
    `afdec0972b9d10c0a01aa5d001317c1bbe414dd9796b51366b6a3b253bd4e49a`.
  - Passed the focused Release regression and, across two contiguous test
    runs due to the command timeout, all 97 Release tests.

- [x] Reproduced BS-3's natural perfect outcome:
  - Selected BS-3 before the authentic stage initializer at frame 1,242 and
    retained objective slots `$51-$53`, their original dispatchers, and
    indexed controller flags `$2D2F-$2D31`.
  - Routed all three original `04:566C` terminal contacts at frame 1,447,
    followed by all three delayed `04:56A6` indexed finalizers at frame
    1,453, without patching the objective flags.
  - Preserved a natural lane-1 emitted-target contact at frame 1,440 and
    reproduced 3,400 earned units across that contact and the three terminal
    objective paths.
  - Proved controller `04:53BA` observes all-clear at frame 1,454, enters
    shared completion `04:44DB` with multiplier 20, and remains scheduled
    through all 87 completion-animation returns.
  - Dynamically captured the completion-owned `$2DBA=1` write at `04:4560`
    on frame 1,606; the route has neither objective-flag nor result-boundary
    latch seeding.
  - Reproduced transfer of the earned 3,400 units, construction of the
    20,000-unit special award, its 40 transfers, both handshakes, and the
    complete 408-tick/102-advance presentation.
  - Added `tools/Analyze-BonusStage3NaturalPerfectSemantics.ps1` and
    Release-only `dc_rom_bonus_stage3_natural_perfect`.
  - Verified census, entry, and selected-state SHA-256 values
    `dcba750ab8301e926c515a30c5678d014ea5ef8fc977da3d4f62384ff504c439`,
    `50de9c48aaa15a26c4484ba22eca9fd7f8b5a30f97909356343f157a93e96809`,
    and
    `89cba55bffa518834204b4ebdc2cb0afde1424c341e2ae3d3532ca6d1d9f7afc`.
  - Passed the focused Release regression and the complete 98-test Release
    suite with two isolated helper processes.

- [x] Reproduced BS-2's contact-driven perfect outcome:
  - Selected BS-2 before the authentic stage initializer at frame 1,242 and
    retained controller-owned objective count `$2D2E=28`.
  - Routed fixed slot `$53` through post-collision target continuation
    `04:5099` once for every valid cell in the ROM's 8-by-5 perimeter table.
    This preserves each 50-unit contact award, response, board mapper,
    subtype lane balance, actor release, and objective decrement while
    deliberately excluding the preceding long motion/collision envelope.
  - Dynamically entered mapper `04:5260` 28 times and tile resolver
    `04:52ED` 112 times. The original resolutions decrement `$2D2E` through
    every value from 27 to 0 without patching the count or tile state and
    award 12,600 stored units.
  - Proved controller `04:4E97` observes all-clear at frame 1,332, enters
    shared completion `04:44DB` with multiplier 50, and remains scheduled
    through all 87 completion-animation returns.
  - Dynamically captured the completion-owned `$2DBA=1` write at `04:4560`
    on frame 1,484; the route has no result-boundary latch seed.
  - Reproduced transfer of the earned 12,600 units, construction of the
    50,000-unit special award, its 100 transfers, both handshakes, and the
    complete 487-tick/122-advance presentation.
  - Added `tools/Analyze-BonusStage2ContactPerfectSemantics.ps1` and
    Release-only `dc_rom_bonus_stage2_contact_perfect`.
  - Verified census, entry, and selected-state SHA-256 values
    `51a3ceb1ef8ad214fe59f39f3a109b314659f2f3dd00afaaf2316e37fb80fc7c`,
    `a394ea8cc7a10db4b24b45ea357f53d5490d74138ca1106249e3941c94c523a7`,
    and
    `d28427026d37f15fe8aa39f37906a980091c6cadd1be54e0014d9122c533d0be`.
  - Passed the focused Release regression and the complete 99-test Release
    suite with two isolated helper processes.

- [x] Reproduced BS-1's natural perfect outcome:
  - Selected BS-1 before its authentic frame-1,241 initializer and retained
    the stage-owned objective count `$2D2E=5` and target slots
    `$34/$35/$50/$51/$52`.
  - Prepared only the five actors' collision bits and remaining hit counts
    at frame 1,303. Their original `04:47D5` terminal-contact paths each
    award 200 stored units and enter the shared destruction response.
  - Dynamically captured all five delayed `04:4828` decrements on frame
    1,309, reducing `$2D2E` from 5 to 0 without patching the objective count
    or accumulated score.
  - Proved controller `04:44CE` enters shared completion `04:44DB` with
    multiplier 10 on frame 1,310 and remains scheduled through all 87
    completion-animation returns.
  - Captured the completion-owned `$2DBA=1` write at `04:4560` on frame
    1,462 and reproduced both handshakes, transfer of the 1,000 earned units,
    construction and 20-chunk transfer of the 10,000-unit special award,
    and the complete 384-tick/96-advance presentation.
  - Added `tools/Analyze-BonusStage1NaturalPerfectSemantics.ps1` and
    Release-only `dc_rom_bonus_stage1_natural_perfect`.
  - Verified census, entry, and selected-state SHA-256 values
    `a64ab169a786eb3df1aa6ea4622a92e9af1fd6ccc9bfbb9e81d3177e3bfe0a3c`,
    `a66366020d8a9ffce74f9ccd608d6f173aad85adda82ee6f8d9b242ece57f016`,
    and
    `41901f52d3e610a775263296193c9819f4b753bc02b0e18a07ff200ed01eccad`.
  - Passed the focused Release regression and the complete 100-test Release
    suite with two isolated helper processes.

- [x] Extended BS-2's perfect route through the full contact envelope:
  - Preserved fixed dynamic slot `$53` and routed all 28 valid board cells
    through live target state `04:5079`, shared collision helper `04:5A12`,
    post-collision continuation `04:5099`, and board mapper `04:5260`.
  - Established that the routed collision branch performs exactly 144
    decrement-X writes at `04:5ABE`, equivalent to a `$90` backward scan,
    before mapping the target. Explicit inverse X seeds therefore resolve
    the intended `$60-$D0` board columns.
  - Captured a balanced 56-step increment/decrement Y sweep at
    `04:5ACB/5ADD` for every contact; each scan returns to its seeded
    `$3C/$4C/$5C/$6C/$7C` board row.
  - Reproduced all 28 `04:5099` contacts, 28 board mappings, 112 tile
    resolutions, 12,600 earned units, objective countdown, controller-owned
    perfect latch, and 50,000-unit special transfer from the full entry.
  - Added Release-only `dc_rom_bonus_stage2_full_contact_perfect` and
    extended `tools/Analyze-BonusStage2ContactPerfectSemantics.ps1` with a
    guarded full-envelope mode while preserving the earlier post-collision
    checkpoint.
  - Verified census, entry, and selected-state SHA-256 values
    `533a642e33244ce95f9ab9ea9b42f9635600c463346b0dc52928c07326222a5d`,
    `9c45ebf3663a8b85dd4446c3c2026664f86c4813fcdeb3fcd69b093604a30c2e`,
    and
    `3469ef08e3600de588b45d613dd0363f6dc27d01fa6763296594c416df73d34b`.
  - Passed the focused Release regression and the complete 101-test Release
    suite with two isolated helper processes.

- [x] Closed the Milestone 2 controls-and-physics gate:
  - Reused the unpatched 3,600-frame state-census input as one deterministic
    route rather than adding another recording mode.
  - Combined `02:5159`, `01:D466`, `01:C3CB`, `00:F1B4`, and `01:DD09` in
    one 16,328-call, 478,079-write ordered trace.
  - Followed held Button I through 96 Devil-Shooter charge calls, four
    released-input delays, the frame-1,185 `$3580=$F8` launch write, and the
    next frame's slot-0 `F1B4` integration.
  - Dynamically connected right control slot `$09`, left slot `$08`, and the
    combined left `$04/$06` pair to input masks `$01/$80/$82` and their
    `01:D48E` response state.
  - Captured three Button-II nudge pairs at frames 1,450/1,451,
    2,250/2,251, and 3,050/3,051. Each writes slot-0 `$3500/$3580` motion
    before a later mapped integration call.
  - Verified tilt lock `$2CC7` remains zero in all 3,600 frame-boundary
    work-RAM images and guarded its explicit ROM early-return gate.
  - Added `tools/Analyze-ControlsPhysicsRoute.ps1` and
    `dc_rom_controls_physics`.
  - Verified work-RAM, entry, call, and scoped-write SHA-256 values
    `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`,
    `1a7a40ad5aeb86b7612a17db10e789b0d169ce020670f23165f45de46f316077`,
    `48c4bf3c99233301cd8b3408116436f2b7960a7e0fd4a441ca3162c692f295ca`,
    and
    `7bca93abffaef5b85e855434c802fd03c1b75f6eca6eac9d0d9410eaf782846a`.
  - Passed the focused Release and Debug regressions, the six-test affected
    Release subset, and the complete 102-test Release suite with two isolated
    test processes.

- [x] Closed the Milestone 2 normal-table collision-boundary gate:
  - Reused the unpatched 3,600-frame state-census route and focused its two
    natural slot-`$62` Small-Soldier contacts at frames 1,479 and 1,852.
  - Identified `01:CBCF` as the confirmed-contact dispatcher. It sets
    `$3862=$90`, selects type-`$82` response `01:CF33`, updates slot-0 motion,
    and completes before the actor consumes the contact later in the frame.
  - Combined `01:CBCF`, `03:7F57`, `03:6966`, `03:7FB1`, `00:F55C`, and
    `00:ED18` in one 97,379-call, 1,163,259-write ordered trace.
  - Proved that both contacts enter the shared collision-table interpreter
    and actor response before clearing bit 7, adding 30 stored units, and
    latching audio command `$21`.
  - Followed the first hit from counter 2 to 1. The second reaches zero,
    adds the 50-unit completion award, changes `$2D16` from 5 to 4, and
    clears all 32 slot-`$62` actor arrays.
  - Added `tools/Analyze-CollisionBoundaryRoute.ps1` and
    `dc_rom_collision_boundary`.
  - Verified work-RAM, entry, call, scoped-write, and selected-state SHA-256
    values
    `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`,
    `c151ef79d4052f94b0740eb6232398d19eee5b0e8b1086f71990134f8b113d7b`,
    `239b72a47ddb1965af5c206136062a4e735dbcb475e798b1f9da29cc6752eeca`,
    `9c0fda84b35afa075465975fc8efb0320c9fc441bedd42d93b067e016b2dd426`,
    and
    `99cf6ab9fed4cfa976510fda30f77c20671e8f2134fc3b2842e3d90fd81d32d8`.
  - Passed the focused Release and Debug regressions, the six-test affected
    Release subset, and the complete 103-test Release suite with two isolated
    test processes.

- [x] Closed the Milestone 2 normal-table session-and-scoring gate:
  - Reused the frame-3,000 `$3380:$3300=$0300` drain seed and connected
    `01:C786`, `01:C7B0/C7BF`, `01:C7E5/C7EA/C80A`, `01:C812`,
    `02:4003`, `09:7266`, and `09:72FD` in one deterministic route.
  - Followed session bit `$53.1`, the frame-3,016 `$2D52` decrement from
    3 to 2, the zero `$2D32` ordinary-continuation decision, and the
    frame-3,287 category-1 table/ball reload.
  - Reconstructed stored score 1,020 from 33 pre-drain `00:F55C` calls and
    proved it remains unchanged throughout the transition.
  - Proved that the panel emits two fixed zero glyphs before converting the
    stored binary score, so stored 1,020 displays as 102,000 and saturation
    displays as 999,999,900. The separately converted remaining-ball digits
    change from 02 to 01.
  - Verified that no multiplier, extra-ball grant, alternate transition, or
    post-drain score mutation occurs on this ordinary route.
  - Added `tools/Analyze-SessionScoringRoute.ps1` and
    `dc_rom_session_scoring`.
  - Verified cross-configuration work-RAM, entry, call, and scoped-write
    SHA-256 values
    `229ffbff969db6067e2adb10b7f05523f048c7be1539fc2188499ca62f18ab5b`,
    `bd1bf1250380a6a7c7d87c92d5884e0190584619f1fed96ebd412413c96d8d47`,
    `31f9af5449deeef5395aec78f916dbe54b42d72436c98fd36413c8550173e25a`,
    and
    `19a322d5efee8660e00e93f759c3b5ff308de8f2727e85becc13ed7b915bf567`.

- [x] Published the Milestone 2 coverage manifest and closed the milestone:
  - Inventoried the official USA manual's frontend, controls, session,
    scoring, BP/BT/BM, Bonus Ball, numbered playfield chart 1-28, backup
    memory, all six bonus stages, and maximum-score ending.
  - Added the extra cartridge actors and native-bridge requirements needed
    by the implementation plan.
  - Classified 105 unique behavior units: 57 dynamically verified, 29
    statically ROM-mapped, and 19 explicitly deferred. No unknown or partial
    classification exists.
  - Added `docs/COVERAGE_MANIFEST.md`,
    `tools/Verify-CoverageManifest.ps1`, and `dc_coverage_manifest`.
  - Guarded every manual inventory ID, classification totals, completeness
    audit category, and named `dc_rom_*` evidence test.

- [x] Established the Milestone 3 background-decoding foundation:
  - Added downstream patch `0004-vdc-video-snapshot.patch` and a narrow
    read-only copy boundary for all 32,768 base-VDC words, 512 VCE palette
    entries, MWR geometry, and background X/Y scroll registers.
  - Added `PceRuntime::export_video_memory()`; it returns owned vectors and
    never exposes emulator pointers.
  - Added the core-owned HuC6270 BAT, 4-bit planar-tile, and 9-bit VCE-color
    decoder plus world-coordinate composition and exact crop helpers.
  - Documented the 64-by-32 circular streamed BAT and the next stream-commit
    mapping step in `docs/TABLE_BACKGROUND.md`.
  - Added `dc_rom_video_snapshot`, which follows the 3,600-frame table route
    through ten live decode checkpoints. Frame 900 guards VRAM, palette, and
    decoded-background SHA-256 values
    `f452889188abdf446a4caf85f5aac51e3afa1cd8ccc1c86ce6044562a0a7d900`,
    `be9d93e01ba3e5f26cfaaa6df5f6107bd05f41a8ec8f29d6f8ff8b88e14f710f`,
    and
    `f812ec7fdba43453dbd1dc926fdb8adf2d4ef400d7631b8ca7ebc49f18446c17`.
  - Identified `00:F400` as the 32-cell streamed BAT-row writer. Release-only
    `dc_rom_video_row_writer` guards 539 calls across four caller paths, exact
    VDC setup writes, 17,248 low/high data-port writes, caller counts, return
    site, and frame range; the live snapshot/golden route remains a separate
    cross-configuration test.
  - Passed the core/manifest/smoke/canonical/video/session affected Release
    subset and the core/smoke/canonical/video Debug subset. The canonical
    visual regression remains unchanged by the read-only bridge.

- [x] Mapped the streamed normal-table source geometry:
  - Classified `00:F2FE/F32D` as the downward first/second row pair and
    `00:F3B2/F3E1` as the upward first/second pair.
  - Proved all 539 commits preserve `anchor_y - camera_y = 480` and map a
    world row to circular BAT word
    `(((world_y - 480) & $FF) / 8) * 64`.
  - Bounded the tile-map source to physical bank `$14` at
    `$800A + world_y * 8` and packed palette nibbles to work RAM at
    `$2259 + world_y * 2`; the dynamic route covers every row from world Y
    168 through 720.
  - Added packed-BAT decoding across the finite 32-by-96-cell source
    envelope.
  - The initial frame-900 256-by-768 diagnostic is reproducible at SHA-256
    `f0df872362bcc231674151eecdd402752d1bc18dd9e333e5a95fb872dad54cee`.
    It was captured during the table fade-in and is not a visually valid
    full-playfield golden.
  - The frame-900 `(0,480,256,208)` packed-source crop matches the same
    decoder applied to the live BAT, with SHA-256
    `da841334896409730877e48a19d89cfa4bacfde153b3157e31c2b126c03bbb2d`.
    This is a decoder self-consistency check, not an independent emulator
    rendering comparison.
  - Post-fade frame-1200 diagnostics show the expected brickwork and colors.
    The background-only crop omits flippers, enemies, lights, and other
    sprite/gameplay-owned mechanisms that appear in the emulator composite.
  - Rows 0-687 of the post-fade reconstruction form a coherent provisional
    256-by-688 visible world. Rows 688-767 contain invalid or non-visible
    source/prefetch data, disproving the earlier assumption that all 96
    source rows were three complete 256-pixel visible bands.
  - Passed the core and video-snapshot regressions in Debug and Release and
    the Release-only 539-call row-writer regression.
  - Passed the complete 107-test Release suite with two isolated test
    processes.

- [x] Closed the Milestone 3 table-background reconstruction gate:
  - Added independent comparison against Beetle's final RGB565 framebuffer,
    including the core's quantization and the playfield's framebuffer-Y-30
    alignment.
  - Added the natural frame-1,600 checkpoint and a sequential isolated
    upper-table route. The latter holds only slot-0 ball Y through frame
    3,000, then lets the original camera converge to its -460 clamp and
    stream the upper region naturally.
  - Proved the accepted 256-by-668 world occupies source rows 20-687. Rows
    0-19 are the top guard, 688-720 are streamed lower prefetch, and
    721-767 are unused tail data.
  - Guarded stable lower, upper-middle, middle, and upper captures with
    source/crop SHA-256 values
    `e11ec46a715e311d584c19c4ddb78750b9e18e6a607f22db7d70de8dddf74a6f`,
    `750ba5022e7604636f97107a374df21262ede4a5aa2ac1e7cc64cb9b9decf25d`,
    `910a3372d4c516f6076bff49556dfd289915014c1cf652a8c8df921fa538bb44`,
    and
    `f2f8fda9090a1f98f1cf07ece0e775122c43257d38caef7f33f2d63c3ff81aec`.
    The accepted frame-1,200 256-by-668 crop is
    `3d5afa6f1f633e23b837c1f3ec3e0a19c8f00ca87abf2a7ff841f600dadc7e1e`.
  - Independently matched 44,901, 47,768, 49,432, and 44,247 background
    pixels out of each 53,248-pixel viewport. The complementary pixels are
    original sprite/presentation overlays.
  - Defined the exact overlay-oracle input boundary: active internal
    HuC6270 SAT words plus the already copied VRAM sprite patterns, VCE
    sprite palettes, and camera transform. Cabinet gameplay actors remain
    sourced from mapped work RAM rather than a second simulation.
  - Passed the focused video-snapshot regression in Debug and Release.

- [x] Added the first cabinet-window vertical slice:
  - Added explicit `--cabinet` and bounded `--cabinet-smoke-test` host modes
    without changing the desktop default.
  - Created a required portrait playfield plus independently recoverable,
    non-focusable DMD and backglass windows. Closing or losing an auxiliary
    no longer stops the playfield.
  - Decode and crop the accepted 256-by-668 table against live VRAM, VCE,
    and work RAM during normal sessions. Menus, zero-ball presentation, and
    `$53.4` bonus stages use the original emulated framebuffer.
  - Added a monitor-resolution seven-segment DMD driven by the `$2251`
    active score slot and `$2205/$220C/$2213` binary score arrays, preserving
    the verified times-100 display rule.
  - Added the black backglass fallback. User-selected PNG/JPEG/WebP loading
    was completed in the later backglass slice.
  - Added `cabinet::decode_cabinet_state()`, ROM-free state/switching unit
    coverage, `docs/CABINET_PREVIEW.md`, and the SDL dummy-driver
    `dc_rom_cabinet_smoke` regression.

- [x] Added persistent interactive cabinet display assignment:
  - Added the versioned `portable-data/cabinet.ini` boundary with strict
    parsing, escaped strings, bounded file size, and atomic replacement.
  - Persisted each role's display name, fallback window rectangle, current
    SDL display orientation, and borderless-fullscreen state.
  - Resolve an assignment by exact display identity first, greatest saved
    geometry overlap second, and the primary display last. Invalid and
    missing configuration falls back safely.
  - Added an F12 assignment screen that pauses emulation, color-identifies
    playfield/DMD/backglass, selects roles with 1/2/3, cycles live displays
    with Left/Right, toggles role fullscreen with F, and saves with Enter,
    Escape, or F12.
  - Added ROM-free configuration round-trip, rejection, atomic-file,
    monitor-reorder/loss, and geometry-clamping coverage. The dummy-driver
    cabinet smoke test remains configuration-write-free.
  - Passed the focused Debug core/CLI/cabinet-smoke regression and the
    focused Release core/CLI/emulation-smoke/cabinet-smoke/video-snapshot
    regression.

- [x] Added persisted PNG/JPEG/WebP backglass images:
  - Pinned SDL_image 3.4.4 at
    `bec9134a26c7d0f31b36d6083c25296e04cabff5` with only stb PNG/JPEG and
    libwebp 1.3.2 decoding enabled. All image encoders and unrelated format
    backends are disabled, and the dependency and bundled-component licenses
    are recorded.
  - Added the shared `render::load_backglass_image()` boundary. It accepts
    formats by file content rather than extension, rejects missing, empty,
    unsupported, larger-than-64-MiB, greater-than-16384-per-side, and
    greater-than-64-megapixel inputs, and owns the decoded SDL surface
    transactionally.
  - Added `--backglass <image>` for scripted selection and B in the F12
    assignment screen for the native file picker. A successful choice is
    loaded immediately and persisted as UTF-8; Delete clears it.
  - Backglass images use black aspect-preserving containment and the same
    integer-nearest plus controlled-linear sharp pipeline as other cabinet
    images. Decode, texture, and renderer-reset failures retain or restore
    the black fallback without stopping play.
  - Added real embedded PNG, JPEG, and WebP fixture coverage plus
    unsupported/missing-content rejection and orphan-CLI validation. Passed
    the focused Debug core/image/CLI/cabinet-smoke regression and the focused
    Release core/image/CLI/emulation-smoke/cabinet-smoke/video-snapshot
    regression.
  - Reinstalled and passed the distribution allowlist: SDL_image and libwebp
    remain statically linked, with only the executable, license, README, and
    third-party notices in the ROM-free package.

- [x] Added the copied live-camera SAT actor overlay:
  - Added downstream patch `0005-vdc-sprite-snapshot.patch` and extended the
    copied video boundary with all 256 internal post-DMA HuC6270 SAT words.
  - Added `render::composite_pce_sprites()` with 16/32-pixel widths,
    16/32/64-pixel heights, pattern alignment, both flips, transparent color
    zero, sprite palettes, two-bit sprite mode, clipping, and exact SAT-order
    sprite priority.
  - Preserved one raw opacity byte per packed background pixel and now applies
    the SAT foreground bit against it. The decoder also enforces the hardware
    maximum of 16 sprite halves per scanline, including the two-slot cost of a
    32-pixel-wide sprite.
  - Translated the live 208-line viewport through signed camera
    `$200B:$200C` into accepted table-world coordinates. The cabinet playfield
    now displays the copied active sprites without advancing a second actor
    simulation; score-panel sprites remain clipped for the DMD.
  - Added ROM-free pattern/flip/priority/rejection coverage. At settled frame
    1,200, the copied SAT hash is
    `26ad08bcac982e81b34e02f899e16d790f148562daaa17e5651b92e5f6248276`;
    46 visible sprites make 9,175 post-priority opaque writes and raise exact
    Beetle composite agreement from 44,901 to 51,778 of 53,248 pixels.
  - Passed the focused Debug and Release core/video-snapshot/cabinet-smoke
    regressions and reverified the four-file ROM-free distribution.

- [x] Added progressive persistent fixed-mechanism composition:
  - Added typed decoding for all 127 mapped actor slots across occupancy/type,
    flags, signed integer position, appearance, and live dispatch arrays.
  - Fixed slots `$0A-$61` learn exact post-limit SAT primitives only while
    their work-RAM anchor is in the live camera. Cached primitives remain
    tied to the actor's position and state signature and are rendered only
    outside the current 208-line SAT viewport.
  - Each learned primitive owns decoded pattern pixels and VCE palette
    indices. Later regional VRAM streaming cannot corrupt a retained
    off-camera mechanism, while palette colors remain live through fade-in.
  - Empty slots clear immediately. An unseen type, appearance, or dispatch
    change suppresses stale art until the original camera observes that
    mechanism again. The cache never advances actor state or gameplay.
  - Preserved background priority and sprite-pattern/palette decoding. The
    copied live SAT keeps the hardware 16-half scanline selection; retained
    actors learned on different camera visits do not suppress one another.
  - Added ROM-free typed-state and learn/persist coverage plus a natural
    3,600-frame ROM route proving that at least one fixed mechanism is
    learned and subsequently rendered off-camera. Focused Debug and Release
    core/video-snapshot/cabinet-smoke regressions pass.
  - This closes progressive persistence for visited mechanisms.

- [x] Mapped the original fixed-actor presentation metadata:
  - Located the complete post-DMA SAT source at VRAM words
    `$7800-$78FF`. Frame 1,200 records all 256 high-byte staging writes at
    physical bank `$08` sites `$40C6-$419E`.
  - Mapped `08:4036` as the actor/SAT scheduler and `08:40DF` as the
    descriptor renderer. Appearance `$3900,X` selects one of 69 pointer
    tables, phase `$3980,X` selects the descriptor, and each descriptor
    supplies signed offsets, pattern, palette, priority, flip, and size
    attributes with `$3800/$3D00/$3E00` modifiers.
  - Added a bounded native metadata decoder. At stable frame 1,200 it
    resolves 100 sprites for the fixed table and matches all 23 eligible
    live actor groups word-for-word against the copied SAT.
  - Added ROM-free descriptor, signed-offset, pattern-base, attribute-merge,
    and malformed-appearance coverage.
  - Proved that metadata alone cannot safely pre-seed unseen art: the
    non-SAT upper VRAM region `$4000-$77FF` changes from SHA-256
    `f84447542add674e39a660d57bde50277ef414c6ee860ab0e8c5d707e889edd3`
    at lower frame 1,200 to
    `c5f344d3130e20d40a37e5d055a557022290be8c87d97df397068850fbf7155d`
    at upper-middle frame 1,600.
  - Visual QA rejected a provisional metadata-only full-table seed before it
    reached the cabinet host. That result established the requirement for
    the sprite-pattern loader and region-correct local atlas completed in
    the next two slices.

- [x] Decoded and reproduced the camera-transition sprite-pattern streams:
  - Mapped `00:E41F` as the generic word-stream decoder. Commands `$01-$7F`
    copy that many little-endian words, commands `$81-$FF` repeat one word
    `(command & $7F)` times, `$80` repeats 256 times, and `$00` terminates.
  - Between frames 1,201 and 1,600, all writes into VRAM `$4000-$77FF`
    originate at `00:E449` (1,036 raw words) or `00:E467` (2,548 repeated
    words). Fourteen calls load physical-bank `$12` streams into `$5400` or
    `$7000`.
  - Added a bounded native decoder with ROM-free raw, repeat, and output-limit
    coverage. Replaying the 14 routed streams over frame-1,200 VRAM reproduces
    every frame-1,600 word in `$4000-$77FF` and exact SHA-256
    `c5f344d3130e20d40a37e5d055a557022290be8c87d97df397068850fbf7155d`.
  - Corrected the observation boundary from post-`PHY` `$E420` to true entry
    `$E41F`, so its stack/caller contract is valid. All 14 calls originate at
    `00:F661` inside selector `00:F606`.
  - `00:F606` selects one of 42 loader descriptors through appearance
    `$3900,X`, phase `$3980,X`, and pattern base `$4F:$50`. The observed
    transition ties slot `$5B`, appearance `$03`, descriptor `$F6FA` to
    `12:8030->$7000`, and slot `$0F`, appearance `$11`, descriptor `$F73B`
    phases 0/1/2/3 to `12:8AFD/8B80/8C5C/8D52->$5400`.
  - This closes the compressed stream format and observed selector path, but
    not unseen-state seeding.

- [x] Decoded all 42 `00:F606` selector entries into a local pattern atlas:
  - The pointer family collapses to eight upload descriptors plus executable
    special loader `$F768`, shared by appearances `$24-$29` as an explicit
    resident-pattern sentinel.
  - The eight descriptors resolve 35 phase assets through the bank `$10/$12`
    directories and decode 12,544 bounded words at their exact
    pattern-base-relative VRAM destinations.
  - Added a generic descriptor decoder for both `00:E41F` compressed streams
    and `00:F666-00:F6A5`'s mode-zero raw-word path through `00:E365`.
    None of the 42 static selectors uses mode zero; a ROM-free fixture guards
    its base resolution, phase offset, word count, and output limit.
  - The ROM-backed video snapshot guards all 42 appearance mappings, nine
    unique pointers, 35 uploads, 12,544 decoded words, destinations,
    encodings, and the six resident selectors.
  - Focused Debug core and ROM-backed video snapshot regressions pass. The
    final Release core and ROM-backed video snapshot regressions also pass.

- [x] Applied stream-backed atlas assets before first camera visits:
  - Separated `00:F606`'s loader destination base `$4F:$50` from bank-8's
    SAT pattern base `$51:$52`; the first no-visit diagnostic caught this
    distinction before the cache path was accepted.
  - The cache selects actor appearance and `$3980,X` phase, requires the
    upload to own every referenced 64-word sprite-pattern block, resolves
    pixels with the live sprite palette, and includes phase in its
    type/appearance/dispatch invalidation signature.
  - At settled lower frame 1,200, 16 of 17 eligible camera-active upload
    groups equal live VRAM byte-for-byte and all 23 eligible bank-8 metadata
    groups still equal the copied SAT. Three off-camera mechanisms are
    pre-seeded and contribute 1,266 priority-filtered pixels.
  - Golden-guarded accepted persistent composites now cover lower frame
    1,200 (`26e4dd2c...8061d16`), upper-middle frame 1,600
    (`bcffd95b...2a96efa`), and middle frame 1,800
    (`da770e96...850ff93`).
  - Debug and Release core, ROM-backed video snapshot, and cabinet-smoke
    regressions pass.
  - The same route now dynamically bounds resident loader `$F768` at frame
    874 with selector `A=0`, loading asset `$1222` at `$7C00`. `X=0` and
    slot-0 appearance/phase zero establish a scene-level initializer rather
    than an actor-local selector. Resident selectors 1-4 remain unmapped.

- [x] Classified and pre-seeded the remaining fixed-actor pattern families:
  - Decoded all five `$F768` choices
    `$1222/$1224/$1226/$1228/$122A` into five 1,008-word `$7C00` packs
    (5,040 words total). Selector 0 equals live frame-900 `$7C00` VRAM.
  - All six deterministic bonus-stage routes see only the earlier
    normal-table selector-0 call, so selectors 1-4 are not bonus-stage packs.
    Their scene association remains unmapped but is not a fixed-actor gate.
  - Independently classified all 24 frame-1,200 fixed groups using
    appearances `$24/$25/$29/$2A/$2B/$2C/$37/$41/$43`. None is owned by
    `$F768`, and every referenced 64-word block is disjoint from the exact
    half-open ranges of all known `$F606` and `$F768` uploads.
  - Generalized the exact range proof to every non-hidden fixed actor. At
    frame 1,200, eight groups are owned by their selected stream and 53 are
    disjoint from every known upload; zero groups remain unresolved.
  - The persistent cache now safely sources those static blocks from live
    VRAM before first camera visits. The 23 live-camera groups plus 38
    pre-seeded groups cover all 61 non-hidden fixed actors and write 10,980
    priority-filtered retained pixels.
  - Refreshed accepted persistent goldens for lower frame 1,200
    (`63d4e780...cfed752`), upper-middle frame 1,600
    (`66586645...1d6593b`), and middle frame 1,800
    (`d229dd58...33059eb`). Their differing coherent mechanism frames guard
    ongoing appearance/phase animation across table bands.
  - Focused core, ROM-backed video snapshot, and cabinet-smoke regressions
    pass in Debug and Release.

- [x] Added exact ball and full-table flipper presentation:
  - Proved slot `$00` and flipper/control slots `$04-$09` use absolute
    table-world coordinates. Their bank-8 descriptors decode one ball sprite
    plus two sprites for each of six flippers.
  - At frame 1,200, all five camera-active ball/flipper groups match
    contiguous copied-SAT words exactly. All seven actor groups use pattern
    blocks disjoint from every known upload, with zero unresolved sources.
  - Kept the moving ball live-SAT-only to prevent trails. Added slots
    `$04-$09` to the phase-aware persistent cache, making the upper, middle,
    and lower flipper pairs visible simultaneously while retaining live
    input-driven phase changes.
  - Golden-guarded lower frame 1,200 now caches 60 persistent actors, seeds
    42, and writes 12,836 retained pixels (`0b52e88e...aecfc2b`).
    Upper-middle frame 1,600 is `22e28684...7861e7c`; middle frame 1,800 is
    `c103b179...bcb588b`.
  - Focused core, ROM-backed video snapshot, and cabinet-smoke regressions
    pass in Debug and Release.

- [x] Retained the persistent normal-table dynamic enemies:
  - Generalized the bank-8 descriptor decoder to dynamic slots `$62-$7E`.
    At frame 1,200 its 14 sprites cover ten type-`$82`, appearance-`$1A`
    Small Soldiers and four type-`$82`, appearance-`$39` Tower insects.
  - All four camera-active dynamic groups match contiguous copied-SAT words
    exactly. All 14 groups use upload-disjoint pattern blocks, with zero
    unresolved sources.
  - Whitelisted only those two persistent enemy families in this slice.
    Empty slots and any reuse by another type, appearance, phase, or dispatch
    clear the cached visual immediately; the later transient-effect slice
    adds only the independently bounded impact family.
  - Golden-guarded lower frame 1,200 now caches 71 persistent actors, seeds
    52, and writes 14,696 retained pixels
    (`eb2e1147...10e55dcfd`). Upper-middle frame 1,600 is
    `2aeb44b0...693470835`; middle frame 1,800 is
    `bbba7ee9...8f8db34`.
  - The deterministic route guards exact dynamic family counts, active SAT
    equality at every captured frame, and changing metadata across the
    lower, upper-middle, and middle captures for all 14 enemies.
  - Focused core, ROM-backed video snapshot, and cabinet-smoke regressions
    pass in Debug and Release.

- [x] Classified and retained the shared table-world impact burst:
  - Restricted transient retention to type `$8F`, appearance `$10`, and the
    mapped activation/animation dispatch pair `02:4290/02:42B0`. The two
    dispatches are presentation-compatible; every other dynamic signature
    remains live-camera-only and invalidates reused-slot pixels.
  - At deterministic frame 2,100, slot `$71` is camera-active at phase 5.
    Its two bank-8 primitives match the copied SAT exactly, and its one actor
    group is upload-disjoint with zero unresolved pattern sources.
  - A cloned-cache probe moves the copied-camera boundary away without
    advancing gameplay and proves the live original actor remains visible in
    table-world space. Its accepted composite is
    `f075afa2...8c7c60` after the camera-band, coordinate-origin, palette,
    and off-camera-phase fixes.
  - Frames 2,400 and 2,700 prove the original 35-36-frame slot release leaves
    zero cached or rendered impact effects. ROM-free coverage also guards
    `$4290->$42B0` compatibility and arbitrary-dispatch slot reuse.
  - Focused core, ROM-backed video snapshot, and cabinet-smoke regressions
    pass in Debug and Release; the later alignment slice records the accepted
    frame-1,200/1,600/1,800 cabinet goldens.

- [x] Corrected camera-dependent actor alignment and flipper visibility:
  - Replaced nearest-anchor SAT association in production cabinet rendering
    with exact bank-8 ROM descriptor groups, eliminating cross-association
    among clustered Sorcerers and star/pentagram mechanisms.
  - Removed the blanket cache hole across the live camera band. Inactive
    retained actors may now occupy that band, while camera-observed actors
    are excluded from the cache layer and the copied live SAT is composited
    last as the authoritative presentation.
  - Work-RAM actor coordinates remain live during retained composition, so
    enemies continue moving after their region leaves the camera. Decoded
    pattern pixels survive unrelated VRAM streaming when the descriptor
    signature is unchanged.
  - Animated actors now keep bounded exact phase variants. Off-camera
    Sorcerers and dynamic enemies must resolve a ROM-owned current-phase
    visual or restore the matching learned phase; they never slide a frozen
    fallback image. Static mechanisms alone may keep their last aligned
    visual when a phase has no independent artwork.
  - The playable route now guards all six flippers at every post-fade
    checkpoint and verifies that the eight Sorcerer positions change between
    the frame-1,600 observed region and frame-1,800 retained region. The same
    route proves that all 16 Sorcerer walking phases are presented, and a
    ROM-free regression restores two distinct learned Sorcerer phase images
    outside the live camera band.
  - Normalized bank-8 actor Y from the VDC's 240-line presentation space
    into the stitched playfield by removing the 30-line score/status band.
    The route now proves that the background's best emulator match stays at
    exactly zero vertical offset through launch and camera transitions.
  - Cached sprites now retain decoded VCE palette indices rather than
    fade-time ARGB colors. Initial lower-view seeds may refresh until the
    camera first leaves that region, and composition resolves the current
    palette so upper/middle flippers do not remain black silhouettes.
  - The retained layer no longer applies one hardware scanline budget across
    sprites captured on different camera visits. The copied live SAT still
    enforces the original 16-half limit.
  - Refreshed the accepted lower/upper-middle/middle compositor goldens to
    `45901880...501785`, `de214ff8...68447e8`, and
    `b8680ea8...a4fe890`. They retain four off-camera flipper actors at every
    checkpoint while the current pair comes from the live SAT.
  - Debug and Release core, ROM-backed video snapshot, and cabinet-smoke
    regressions pass.

- [x] Began Milestone 4 with the second native physics routine:
  - Added pure C `dc_integrate_position`, matching `00:F1B4`'s two signed
    16.8 positions, signed 8.8 increments, six-byte pre-integration cache,
    and fractional-only movement marker.
  - Admitted the already bounded no-interrupt `00:F1B4@17` path. It
    declares the exact outer RTS, final registers/MPRs, 714 body master
    cycles, and 17 ordered writes with their original PCs and timings.
  - Independently replayed and admitted `00:F1B4@1` at frame 1,049. It
    integrates second position `$028F00` by nonnegative `$0010`, leaves the
    first position `$010E00` unchanged, performs 16 writes, and returns after
    702 cycles. Omitting the negative-sign scratch store makes it 12 cycles
    shorter than ordinal 17.
  - Independently replayed and admitted `00:F1B4@10` at frame 1,058. Its
    nonnegative `$00A2` increment moves the second position from `$0291D1`
    to `$029273`, crossing an integer boundary. The original comparison
    therefore exits with `A=$91`, `P=$80` before setting the fractional-only
    marker; the native transaction matches all 15 writes and 639 cycles.
  - Strengthened both host guards with exact entry registers, cached and
    integrated values, sign branches, and movement flags. The focused
    ordinal-1 shadow preserves the complete 1,156-call/21,738-write trace.
  - Native `F1B4@17` now runs immediately before native `F241@1`. Both
    checkpoints are restored and replayed through assembly; all 23 writes,
    876 combined body master cycles, and complete post-call work RAM match.
  - The established gameplay, routine-entry, bounded-call, and ordered-write
    SHA-256 values remain unchanged. ROM-free tests cover positive and
    negative integration, integer-step marking, indexed slots, and truncated
    snapshots.
  - Core, coverage-manifest, unsupported-ordinal CLI, and
    `dc_rom_native_replacement` regressions pass in Debug and Release.

- [x] Added the third bounded native physics routine:
  - Added pure C `dc_damp_and_accelerate_motion`, matching `01:DD09`'s
    signed truncating component damping and unsigned low-byte acceleration.
  - Admitted independently replayed `01:DD09@1`. Both signed-divider
    branches are skipped because its two motion components are zero; it adds
    `$8B=$10`, writes `$3500=$10` at body cycle 132 and `$3580=$00` at cycle
    168, and resumes assembly at outer RTS `01:DD6D`.
  - Independently replayed and admitted interrupt-free `01:DD09@2` at frame
    1,049. Axis one remains zero; axis two `$0010` runs through signed
    divisor `$80`, produces truncating quotient zero, then adds `$8B=$10`
    for output `$0020`. It returns after 2,409 cycles and 68 ordered writes.
  - The ordinal-2 transcript contains the complete signed-divider
    scratch/stack sequence but no `FCxx`/`FDxx` interrupt-handler passage.
    The focused shadow restores the same pre-call state and proves exact
    registers/MPRs, return path, timings, write order, and complete RAM.
  - Independently replayed and admitted interrupt-free `01:DD09@181` at
    frame 1,228. Axis two `$FF0D` divides through signed `$80`, restores its
    negative sign at `$FF0C`, then adds `$10` acceleration for `$FF1C`.
    Its exact 69-write transcript completes in 2,427 cycles and returns
    `A=$FF`, `P=$80`.
  - Initially rejected `01:DD09@52` after normalized replay exposed
    interrupt-owned stack/controller work through `00:E058/E169`. The later
    semantic `E905`/`DD09` checkpoint composition now admits this path.
  - Restored the same pre-call state and proved exact final registers/MPRs,
    two ordered writes and timings, 168 body master cycles, and complete
    post-call work RAM. The established gameplay, entry, bounded-call, and
    ordered-write hashes remain unchanged.
  - ROM-free tests cover positive and negative signed damping, signed
    divisors, acceleration, zero-component branch skips, invalid divisors,
    indexed slots, the bounded nonzero-divider input, and truncated
    snapshots. Semantic `01:DD09@all` later replaced the ordinal-only CLI
    admission.
  - Core, coverage-manifest, physics-oracle, unsupported-ordinal CLI, and
    `dc_rom_native_replacement` regressions pass in Release; the focused
    native replacement also passes in Debug.

- [x] Added the fourth bounded native physics routine:
  - Added pure C `dc_rotate_motion`, matching `01:D1A2`'s signed 8.8
    sign-magnitude multiplies, fractional truncation, and
    `u*c-v*s` / `u*s+v*c` output combination.
  - Audited the initially selected `D1A2@1` transcript and found three
    timer-interrupt handler passages (`FCxx`/`FDxx` writes). It was initially
    left emulated because a work-RAM-only transaction could not acknowledge
    and deliver those interrupts faithfully; the later checkpointed
    aggregate replacement now admits it.
  - Admitted the first independently replayed interrupt-free call,
    `01:D1A2@23`: identity coefficients `$0100/$0000`, inputs and outputs
    `$0000/$FFFF`, 5,076 body master cycles, 190 ordered writes, and outer
    RTS `01:D227`.
  - Fixed the core bridge so coincident writes and a write exactly at the
    body boundary do not invoke a spurious zero-cycle hardware sync. The
    canonical downstream patch reapplies cleanly to the pinned upstream
    revision.
  - Restored the same pre-call state and proved exact registers/MPRs, return
    path, write order/timing, cycle budget, complete post-call RAM, and
    unchanged gameplay, entry, call, and write hashes in Debug and Release.

- [x] Added the first bounded native collision routine:
  - Added pure C `dc_advance_collision_response`, matching `03:7FB1`'s
    event/phase response calculation, indexed countdown decrement, and
    countdown-reload phase advancement.
  - Independently replayed and admitted interrupt-free `03:7FB1@1` at
    frame 957. It maps event `$0F` and phase 0 to response `$1C`, decrements
    slot `$62`'s countdown from 4 to 3, performs three writes at body cycles
    39/93/114, and resumes at outer RTS `03:7FD1` after 126 cycles.
  - Independently replayed and admitted interrupt-free `03:7FB1@3` at
    frame 958. With the same slot, event, and phase, it decrements the
    countdown from 3 to 2 and returns response `$1C` after the same three
    writes and 126-cycle budget.
  - Independently replayed and admitted `03:7FB1@7` at frame 960. It takes
    the same slot/event response calculation from countdown 1 and phase 0,
    writes the transient zero countdown, reloads it to 4, advances phase to
    1, and returns `A=$04` after five writes at body cycles
    39/93/114/141/162.
  - The first shadow attempt exposed and fixed a branch-specific adapter
    error that returned response `$1C` instead of reload value `$04`.
  - The full 3,600-frame collision-boundary test now runs the native call,
    restores its pre-call state, and executes original assembly for shadow
    comparison. All census, work-RAM, entry, call, scoped-write, and
    selected-state hashes remain unchanged in Release; a focused 21,987-call
    Debug route also passes native-versus-assembly replay.
  - ROM-free tests cover indexed response calculation, the non-reload path,
    countdown reload, phase wrapping, and truncated snapshots. Unsupported
    7FB1 ordinals are rejected by the CLI. A direct replay of ordinal 2
    exposed 13 interrupt-owned writes and remains emulated. Separate
    3,600-frame follow-up and reload regressions preserve the Debug/Release
    census hashes and the complete 21,987-call, 91,295-write `7FB1` trace.

- [x] Added the second bounded native collision routine:
  - Added pure C `dc_select_collision_table_pass`, matching `03:6966`'s
    negative-state alternate-pass selection and early-return decision while
    keeping the ROM table pointer at the adapter boundary.
  - Independently replayed and admitted interrupt-free `03:6966@5` at frame
    941. Selector `$2A` resolves to table pointer `$6C2A`; fixed Front-Spider
    slot `$26` changes pass state `$81->$80` and returns at `03:6988`.
  - The native transaction matches output `A=$80`, `X=$26`, `Y=$81`,
    `P=$12`, four writes at body cycles 39/66/114/135, and the 165-cycle
    body budget before restored assembly replay.
  - The focused Debug and Release regression preserves the gameplay hash and
    complete 19,648-call/180,389-write `6966` hashes. ROM-free tests cover
    the early return, alternate pass, direct positive-state path, and
    truncated snapshots.
  - Extended the pure C boundary with
    `dc_advance_collision_table_record`, covering event/orientation updates,
    signed horizontal and vertical unit motion with carry/borrow, and the
    two-byte record-cursor advance.
  - Independently replayed and admitted interrupt-free `03:6966@1` at frame
    873. Selector `$2A` and record `$03` resolve `$6C2A->$6D56`; fixed
    Front-Spider slot `$26` changes pass `$80->$81`, event to `$02`,
    orientation `$0C->$8C`, cursor `0->1->2`, and horizontal low `$40->$41`.
  - The full-record transaction returns through `03:6A37` with
    `A=$00`, `X=$26`, `Y=$01`, `P=$10`, and `SP=$F5` after 663 cycles and
    12 exact writes. Its focused Debug and Release regression preserves the
    same complete interpreter artifacts.
  - Independently replayed and admitted interrupt-free `03:6966@2` from the
    adjacent frame-873 state. Selector `$2B` and record `$07` resolve
    `$6C2D->$6D9E`; fixed Front-Spider slot `$27` changes pass `$80->$81`,
    event to `$06`, orientation `$0C->$8C`, cursor `0->1->2`, and horizontal
    low `$D8->$D7`.
  - The reverse full-record transaction returns through `03:6A37` with
    `A=$00`, `X=$27`, `Y=$01`, `P=$10`, and `SP=$F5` after 672 cycles and
    12 exact writes. Focused Debug and Release regressions preserve the same
    complete interpreter artifacts, while ROM-free coverage now includes
    this exact selector/record pair.
  - Independently replayed and admitted interrupt-free `03:6966@3` from the
    next frame-873 state. Selector `$2C` and record `$05` resolve
    `$6C30->$6D7A`; fixed slot `$28` changes pass `$80->$81`, event to `$04`,
    orientation `$0C->$8C`, cursor `0->1->2`, and vertical low `$82->$83`.
  - The vertical full-record transaction returns through `03:6A37` with
    `A=$08`, `X=$28`, `Y=$01`, `P=$10`, and `SP=$F5` after 663 cycles and
    12 exact writes. Focused Debug and Release regressions preserve the same
    complete interpreter artifacts, while ROM-free coverage includes this
    exact selector/record pair.
  - Independently replayed and admitted interrupt-free `03:6966@4` from the
    remaining adjacent frame-873 state. Selector `$2D` and record `$05`
    resolve `$6C35->$6D7A`; fixed slot `$29` changes pass `$80->$81`, event
    to `$04`, orientation `$0C->$8C`, cursor `0->1->2`, and vertical low
    `$82->$83`.
  - The second vertical full-record transaction returns through `03:6A37`
    with `A=$08`, `X=$29`, `Y=$01`, `P=$10`, and `SP=$F5` after 663 cycles
    and 12 exact writes. Focused Debug and Release regressions preserve the
    same complete interpreter artifacts, while ROM-free coverage includes
    this exact selector/record pair.
  - Independently replayed and admitted interrupt-free `03:6966@6` at frame
    941. Selector `$2B` resolves to table pointer `$6C2D`; fixed Front-Spider
    slot `$27` changes pass state `$81->$80` and returns at `03:6988`.
  - The second alternate-pass transaction matches output `A=$80`, `X=$27`,
    `Y=$81`, `P=$12`, four writes at body cycles 39/66/114/135, and the
    165-cycle body budget. Its focused Debug and Release regression
    preserves the gameplay hash and complete `6966` traces, while ROM-free
    coverage includes the exact selector/slot pair.
  - Independently replayed and admitted interrupt-free `03:6966@7` at frame
    941. Selector `$2C` resolves to table pointer `$6C30`; fixed slot `$28`
    changes pass state `$81->$80` and returns at `03:6988`.
  - The third alternate-pass transaction matches output `A=$80`, `X=$28`,
    `Y=$81`, `P=$12`, four writes at body cycles 39/66/114/135, and the
    165-cycle body budget. Its focused Debug and Release regression
    preserves the gameplay hash and complete `6966` traces, while ROM-free
    coverage includes the exact selector/slot pair.
  - Independently replayed and admitted interrupt-free `03:6966@8` at frame
    941. Selector `$2D` resolves to table pointer `$6C35`; fixed slot `$29`
    changes pass state `$81->$80` and returns at `03:6988`.
  - The fourth alternate-pass transaction matches output `A=$80`, `X=$29`,
    `Y=$81`, `P=$12`, four writes at body cycles 39/66/114/135, and the
    165-cycle body budget. Its focused Debug and Release regression
    preserves the gameplay hash and complete `6966` traces, while ROM-free
    coverage includes the exact selector/slot pair.
  - Independently replayed and admitted interrupt-free `03:6966@9` at frame
    942. Caller `03:5890` supplies selector `$06`; table entry `$0B` resolves
    `$6B0A->$6DE6`, and slot `$17` changes pass `$80->$81`, event to `$0A`,
    orientation `$0D->$8D`, cursor `0->1->2`, and horizontal low `$F6->$F5`.
  - The alternate reverse full-record transaction returns through
    `03:6A37` with `A=$00`, `X=$17`, `Y=$01`, `P=$10`, and `SP=$F9` after
    672 cycles and 12 writes. Shadowing exposed and closed the full-record
    adapter's previously fixed `$21F5` saved-status address: it is now
    stack-relative and correctly writes `$21F9` for this caller.
  - Extended `dc_advance_collision_table_record` to distinguish an
    event-bearing high-bit header from a headerless continuation byte.
    Continuations preserve event/orientation, consume the current byte as
    motion, and advance the record cursor once rather than twice.
  - Independently replayed and admitted interrupt-free `03:6966@10` at frame
    942. Selector `$2A` resolves `$6C2A->$6D56`; slot `$26` resumes at cursor
    2, applies headerless `$08`, changes vertical low `$7C->$7D`, and advances
    the cursor once to 3.
  - The continuation returns through `03:6A37` with `A=$08`, `X=$26`,
    `Y=$02`, `P=$10`, and `SP=$F9` after 540 cycles and nine exact writes.
    Focused Debug and Release regressions preserve the complete interpreter
    artifacts.
  - Generalized the continuation transcript over validated table, motion,
    actor-field, and cursor addresses, then independently replayed and
    admitted interrupt-free `03:6966@12` at frame 942. Selector `$2C`
    resolves `$6C30->$6D7A`; slot `$28` resumes at cursor 2, applies
    headerless `$08`, changes vertical low `$83->$84`, and advances the
    cursor once to 3.
  - The second continuation returns through `03:6A37` with `A=$08`, `X=$28`,
    `Y=$02`, `P=$10`, and `SP=$F9` after 540 cycles and nine exact writes.
    Focused Debug and Release regressions preserve the complete interpreter
    artifacts.
  - Independently replayed and admitted interrupt-free `03:6966@14` at frame
    943. Caller `03:5890` supplies selector `$06`, which resolves to table
    pointer `$6B0A`; slot `$17` changes pass state `$81->$80` and returns at
    `03:6988`.
  - The alternate-selector early-return transaction matches output `A=$80`,
    `X=$17`, `Y=$81`, `P=$12`, and `SP=$F9`, four writes at body cycles
    39/66/114/135, and the 165-cycle body budget. Focused Debug and Release
    regressions preserve the gameplay hash and complete `6966` traces, while
    ROM-free coverage includes the exact selector/slot pair.
  - Independently replayed and admitted interrupt-free `03:6966@15` later
    in frame 943. Selector `$2A` resolves to table pointer `$6C2A`; fixed
    Front-Spider slot `$26` changes pass state `$81->$80` and returns at
    `03:6988`.
  - The repeated fixed-selector return exactly reuses the proven
    four-write/165-cycle contract. Its focused Debug and Release regression
    preserves the gameplay hash, complete `6966` traces, output registers,
    and complete post-call RAM against restored assembly.
  - Independently replayed and admitted interrupt-free `03:6966@16` later
    in frame 943. Selector `$2B` resolves to table pointer `$6C2D`; fixed
    Front-Spider slot `$27` changes pass state `$81->$80` and returns at
    `03:6988`.
  - The second repeated fixed-selector return exactly reuses the proven
    four-write/165-cycle contract. Its focused Debug and Release regression
    preserves the gameplay hash, complete `6966` traces, output registers,
    and complete post-call RAM against restored assembly.
  - Independently replayed and admitted interrupt-free `03:6966@17` later
    in frame 943. Selector `$2C` resolves to table pointer `$6C30`; fixed
    slot `$28` changes pass state `$81->$80` and returns at `03:6988`.
  - This third repeated return enters with status `$12`, while the earlier
    matching selector call enters with `$02`. The native guard now admits
    both exact variants, and its focused Debug and Release regression
    preserves all output, RAM, timing, write-order, and trace evidence.
  - Independently replayed and admitted interrupt-free `03:6966@18` later
    in frame 943. Selector `$2D` resolves to table pointer `$6C35`; fixed
    slot `$29` changes pass state `$81->$80` and returns at `03:6988`.
  - This fourth repeated return enters with status `$00`, while the earlier
    matching selector call enters with `$10`. The native guard now admits
    both exact variants, and its focused Debug and Release regression
    preserves all output, RAM, timing, write-order, and trace evidence.
  - Extended the continuation adapter through independently replayed,
    interrupt-free `03:6966@19` at frame 944. Selector `$06` resolves
    `$6B0A->$6DE6`; slot `$17` resumes at cursor 2, consumes headerless
    reverse-vertical byte `$10`, changes vertical low `$C4->$C3`, and
    advances the cursor to 3.
  - The alternate continuation returns through `03:6A37` with `A=$C4`,
    `X=$17`, `Y=$02`, `P=$10`, and `SP=$F9` after 549 cycles and nine exact
    writes. Focused Debug and Release regressions preserve the complete
    interpreter artifacts, while ROM-free coverage exercises the exact
    reverse-vertical continuation.
  - Extended the same helper through independently replayed, interrupt-free
    `03:6966@20` at frame 944. Selector `$2A` resolves `$6C2A->$6D56`;
    Front-Spider slot `$26` resumes at cursor 3, consumes headerless
    horizontal byte `$20`, changes horizontal low `$41->$42`, and advances
    the cursor to 4.
  - The horizontal continuation returns through `03:6A37` with `A=$00`,
    `X=$26`, `Y=$03`, `P=$10`, and `SP=$F9` after 540 cycles and nine exact
    writes. Focused Debug and Release regressions preserve the complete
    interpreter artifacts, while ROM-free coverage exercises the exact
    horizontal continuation.
  - Added independently replayed, interrupt-free `03:6966@21` later in frame
    944. Selector `$2B` resolves `$6C2D->$6D9E`; Front-Spider slot `$27`
    resumes at cursor 3, consumes headerless reverse-horizontal byte `$40`,
    changes horizontal low `$D7->$D6`, and advances the cursor to 4.
  - The reverse-horizontal continuation returns through `03:6A37` with
    `A=$00`, `X=$27`, `Y=$03`, `P=$10`, and `SP=$F9` after 549 cycles and
    nine exact writes. Focused Debug and Release regressions preserve the
    complete interpreter artifacts, while ROM-free coverage exercises the
    exact decrement path.
  - Added independently replayed, interrupt-free `03:6966@22` later in frame
    944. Selector `$2C` resolves `$6C30->$6D7A`; fixed slot `$28` resumes at
    cursor 3, consumes headerless vertical byte `$08`, changes vertical low
    `$84->$85`, and advances the cursor to 4.
  - The later vertical continuation enters with status `$12` and returns
    through `03:6A37` with `A=$08`, `X=$28`, `Y=$03`, `P=$10`, and
    `SP=$F9` after 540 cycles and nine exact writes. Focused Debug and Release
    regressions preserve the complete interpreter artifacts and ROM-free
    coverage exercises the exact cursor-3 path.
  - Added independently replayed, interrupt-free `03:6966@23` at the end of
    frame 944. Selector `$2D` resolves `$6C35->$6D7A`; fixed slot `$29`
    resumes at cursor 3, consumes headerless vertical byte `$08`, changes
    vertical low `$84->$85`, and advances the cursor to 4.
  - This second later vertical continuation enters with status `$00` and
    returns through `03:6A37` with `A=$08`, `X=$29`, `Y=$03`, `P=$10`, and
    `SP=$F9` after 540 cycles and nine exact writes. Focused Debug and
    Release regressions preserve the complete interpreter artifacts and
    ROM-free coverage exercises the exact slot-`$29` path.
  - Replaced ordinal admission as the primary `03:6966` scheduler with
    semantic `03:6966@all` admission. The adapter reads the selector pointer
    from the verified user ROM and accepts the `$81->$80` early-return
    contract for any selector and actor slot.
  - Extended Beetle's hook entry with conservative master-cycle distances to
    the next user/video event and next serviceable timer interrupt. Semantic
    calls reaching either horizon fall back to original assembly, covering
    interrupt-bearing calls without a maintained ordinal exception list while
    allowing harmless timer-divider ticks.
  - Generalized the ROM-driven headerless record step. Collision-list and
    motion cursors now select the table entry, motion stream, and effective
    delta without selector, actor-slot, caller, or ordinal conditions. The
    transaction covers both axes and exact low-byte carry/borrow timing.
  - Extended the bounded walker through `$C0` motion terminators, `$FE`
    collision-list wraps, `$FF` table termination, and headered records.
    Each traversed marker contributes its original cursor writes and exact
    accumulated cycle delay to the final transaction.
  - Retired the compiled selector/slot/ordinal proof matrix. The 21 focused
    ordinal tests remain restored-assembly regression coordinates, but all
    admitted `6966` execution now comes from the semantic early-return,
    headerless-step, or bounded-walker paths.
  - Added exact 17-instruction alternate-pass and 50-instruction one-axis
    headerless-record transcripts. The aggregate Debug and Release regression
    now executes 17,308 of 19,648 calls natively (88.1%) and preserves the
    established gameplay SHA-256, complete call SHA-256, and complete
    180,389-write SHA-256. Uncheckpointed interpreter paths still use
    automatic fallback.
  - Generalized the same instruction transcript to ordinary two-axis records
    without a high-byte carry or borrow. The resulting 55-checkpoint paths
    raise semantic coverage to 18,806 of 19,648 calls (95.7%) and compose
    20,113 of 20,867 Small-Soldier parent calls (96.4%). Release and Debug
    retain the exact gameplay/state-census, call, and ordered-write hashes.
- [x] Shifted Milestone 4 from the collision interpreter to bounded physics
  routines:
  - Replaced `F1B4`'s three state-specific admissions with one slot-zero
    semantic transaction covering both sign branches, all four integer
    comparisons, flags, exact writes, and variable cycle budgets.
  - Verified its state/timing formula against all 906 interrupt-free calls,
    then added path-sensitive 50-57-boundary transcripts that admit all
    1,156 route calls, including 250 original interrupt-bearing scopes.
  - Enabled the already state-generic six-store `F241` rollback across all
    calls through a 12-boundary transcript, admitting all 404.
  - Both semantic aggregate regressions preserve exact gameplay and complete
    call/write SHA-256 artifacts; the focused ordinal replays still pass.
  - Release core, both semantic aggregates, and the native-replacement shadow
    suite pass; Debug core and semantic `F1B4@all` also pass.
- [x] Ported shared signed-word negation `00:E837@all`:
  - Derived the exact complement/increment semantics from live `$2F:$30`,
    including the low-byte-wrap carry into `$30`.
  - Added exact 8/9-instruction transcripts for the 90-cycle three-write and
    102-cycle four-write paths.
  - Restored and shadow-compared ordinal 1's frame-1,064 wrap path against
    original assembly with exact register, RAM, cycle, and write-order parity.
  - Replaced all 894 state-census calls, including 24 interrupted scopes,
    with byte-identical Debug/Release census, call, and 3,122-write artifacts.
  - Profiled downstream `01:D09E` and initially deferred its variable
    long-math transcript until the bounded multiply leaves were native.
- [x] Ported unsigned fixed-point multiply round `00:E624@all`:
  - Modeled eight exact shift/add decisions across `$42:$43/$30`, including
    16-bit addition carry into the multiplier byte.
  - Generated 41-113 instruction checkpoints and 24-48 writes from live
    multiplier bits with exact `600 + 84 * admitted-bit-count` body timing.
  - Replaced all 5,371 route calls, including 1,592 interrupted scopes, with
    byte-identical Debug/Release census, call, and 181,868-write artifacts.
  - Shadowed interrupted ordinal 1 and adjacent interrupt-free ordinal 2
    against restored assembly with exact state, timing, and write parity.
- [x] Ported signed fixed-point multiply wrapper `00:E65C@all`:
  - Modeled both operand-normalization paths, optional high-byte round,
    mandatory low-byte round, fixed-point shift, and final sign restoration.
  - Composed exact nested JSR/RTS and PHX/PLX boundaries around one or two
    semantic `E624` children over a private staged RAM snapshot.
  - Replaced all 3,700 route calls, including 2,157 calls with original
    handler writes, with byte-identical Debug/Release census, call, and
    244,531-write artifacts.
  - Shadowed interrupt-free ordinal 3 against restored assembly with exact
    921-cycle, 35-write parity.
- [x] Ported flipper vector projection `01:D09E@all`:
  - Modeled the two signed 8.8 projected outputs from indexed motion and the
    live phase-selected coefficient pair.
  - Composed four semantic `E65C` children and one semantic `E837` child with
    exact parent scratch, addition, nested-stack, and checkpoint timing.
  - Increased the fixed checkpoint envelope from 512 to 2,048 while retaining
    the independent 512-write bound and savestate serialization.
  - Replaced all 540 calls, including 448 calls with original handler writes,
    with byte-identical Debug/Release census, call, and 168,532-write
    artifacts.
  - Shadowed interrupt-free ordinal 16 against restored assembly with exact
    5,328-cycle, 194-write parity.
- [x] Ported signed two-dimensional rotation `01:D1A2@all`:
  - Reused the pure signed 8.8 `dc_rotate_motion` result model over live
    input and coefficient state.
  - Composed four semantic `E65C` children and one semantic `E837` child
    with exact parent scratch, addition, indexed-store, nested-stack, and
    checkpoint timing.
  - Replaced all 354 current route calls, including 296 calls with original
    handler writes, with byte-identical Debug/Release census, call, and
    103,362-write artifacts.
  - Shadowed interrupt-free aggregate ordinal 23 against restored assembly
    with exact frame-1,241, 7,584-cycle, 273-write parity.
- [x] Ported flipper-radius projection `01:D612@all`:
  - Added pure C `dc_project_flipper_radius` for the signed 8.8
    `$31:$32 * $7D:$7E` result stored in `$C2:$C3`.
  - Composed one semantic `E65C` child with exact parent operand, result,
    nested-stack, and checkpoint timing.
  - Replaced all 62 calls, including 51 calls with original handler writes,
    with byte-identical Debug/Release census, call, and 6,271-write artifacts.
  - Shadowed interrupt-free ordinal 36 against restored assembly with exact
    frame-2,498, 2,079-cycle, 77-write parity.
- [x] Ported directional flipper-angle projection `01:D62E@all`:
  - Added pure C `dc_project_flipper_angle` for direct/wrapped angle delta,
    8.8 conversion, signed scaling, and direction-selected result negation.
  - Composed one semantic `E65C` child with exact branch, shift/rotate,
    scratch, output, nested-stack, and checkpoint timing.
  - Replaced all 62 calls, including 43 calls with original handler writes,
    with byte-identical Debug/Release census, call, and 5,062-write artifacts.
  - Shadowed interrupt-free ordinal 1 against restored assembly with exact
    frame-1,268, 1,407-cycle, 51-write parity.
- [x] Ported collision-motion adjustment `01:CFD8@all`:
  - Added pure C `dc_adjust_collision_motion` for both clamp directions and
    all four signed scaling modes.
  - Generated exact mode, clamp, shift/rotate, addition, scratch, flag, and
    interrupt checkpoints without nested child calls.
  - Replaced all 354 calls, including 41 calls with original handler writes,
    with byte-identical Debug/Release census, call, and 4,103-write artifacts.
  - Shadowed interrupt-free ordinal 29 against restored assembly with exact
    frame-1,297, 102-cycle, two-write parity.
- [x] Ported signed division `00:E905@all`:
  - Added pure C `dc_divide_signed_word` with explicit inherited-carry,
    signed normalization, 16-round remainder/quotient, and sign-restoration
    state.
  - Replaced all 752 calls, including 583 calls with original handler writes,
    with byte-identical Debug/Release gameplay, call, and 52,804-write
    artifacts.
  - Shadowed interrupt-free ordinal 516 against restored assembly with exact
    frame-1,564, 2,037-cycle, 59-write parity.
- [x] Generalized motion damping to semantic `01:DD09@all`:
  - Replaced the three fixed ordinal transcripts with a state-derived parent
    that conditionally composes one or two semantic `E905` children.
  - Replaced all 753 calls, including 669 calls with original interrupt
    handling, while preserving the established 61,291-write artifacts.
  - Retained ordinals 1, 2, and 181 as focused zero, positive-divider, and
    negative sign-restoration replay coordinates.
- [x] Completed interruptible near-target collision geometry
  `01:CCD5@all`:
  - Added exact instruction checkpoints for both signed-difference prefixes
    and both descriptor-offset branches.
  - Replaced all 66,523 route calls, including the 60,704 calls previously
    deferred at event/IRQ horizons.
  - Preserved the established Debug/Release census, complete call, and
    1,414,954-write artifacts byte-for-byte.
- [x] Split the native replacement subsystem:
  - Moved the `03:6966`, `03:7FB1`, `03:7F57`, `01:CC1A`, and `01:CCD5`
    adapters into `src/app/native_replacements_collision.cpp`.
  - Moved integration, rollback, signed arithmetic, damping, rotation, and
    flipper projections into `native_replacements_physics.cpp`.
  - Moved flipper input/phase/geometry, coroutine, nudge/session input, and
    actor scheduling into `native_replacements_controls.cpp`.
  - Moved RNG and score synthesis into `native_replacements_rules.cpp`,
    leaving `native_replacements.cpp` as the central registry.
  - Kept the public dispatcher unchanged, exposed only a private internal
    interface, and re-ran 27 subsystem ROM-backed regressions after the
    behavior-neutral moves.

## Next Actions

- [x] Add repository hygiene for ROMs, generated assets, traces, build products, and local configuration.
- [x] Extract the native replacement adapters and dispatcher from
  `src/app/main.cpp` behind one compiled module interface; reduce the file
  from 16,939 to 5,077 lines with no semantic change.
- [x] Split replacement-adapter growth into subsystem implementation
  files as those areas are modified; do not return transaction synthesis to
  `main.cpp`:
  - [x] Collision table, response, consumer, scan, and geometry.
  - [x] Physics and arithmetic composition.
  - [x] Controls and actor scheduling.
  - [x] RNG and scoring rules.
  - [x] Central registry.
- [x] Add the project license and third-party attribution structure.
- [x] Establish the C++20/CMake project and Windows CI build.
- [x] Pin and integrate the Beetle PCE core baseline.
- [x] Implement exact ROM hash validation.
- [x] Create the SDL3 application shell and emulator-facing `PceRuntime` boundary.
- [x] Display the emulated framebuffer in a resizable Windows window using the fixed sharp scaler.
- [x] Connect SDL keyboard/controller input and emulated audio.
- [x] Add versioned savestate serialization and recovery.
- [x] Add controller hot-plug/remapping and robust audio latency handling.
- [x] Add a canonical boot/input recording with RAM, framebuffer, and audio timing hashes.
- [x] Compare the canonical recording against Mesen2 and document any differences.
- [x] Exercise and document full backup-RAM reload behavior through the visible host.
- [x] Establish the best available independent canonical audio oracle.
- [x] Add the fork-owned execution-hook boundary and pass-through mode.
- [x] Add selective observe mode and deterministic routine-entry trace export.
- [x] Add a multi-hook registry and begin mapping canonical routine entries.
- [x] Add a dedicated gameplay recording that reaches the table and begin
  mapping active-game routine candidates.
- [x] Explain the Beetle/Mesen gameplay transition and collision-count deltas,
  and map the candidate routines' callers.
- [x] Capture ordered work-RAM writes, output registers/flags, return paths,
  and per-path cycle budgets for the initial session and collision routines.
- [x] Add savestate-normalized pre-call replay for the first physics scopes.
- [x] Resolve the mapped physics routines' fixed-point and indexed-slot
  semantics.
- [x] Extend the active-table census through the camera and first
  visibility/activation scope.
- [x] Resolve the 127-slot occupancy and camera-window activation contract.
- [x] Map the first score accumulator and deterministic award-addition scope.
- [x] Map the RNG seed, update recurrence, and first active consumers.
- [x] Map the accepted game-to-audio command path, descriptor lookup, and
  active producer identities.
- [x] Correlate gameplay audio commands with fixed and dynamic actor
  state-machine identities.
- [x] Bound the first dynamic table rule from allocation through hit scoring,
  audio feedback, completion, and slot release.
- [x] Bound the branching second-hit rule, including standard completion and
  subtype-specific persistent-state behavior.
- [x] Reconcile all fixed actor initializer identities with bounded
  gameplay-facing mechanisms or conservative compatibility roles.
- [x] Activate both selective normal-table modes and all six bonus-stage
  layouts.
- [x] Complete BS-3's first event-specific rule family and reproduce its
  completion and timeout outcomes.
- [x] Preserve the BS-3 mapping checkpoint, then pause further mapping
  expansion.
- [x] Extend the copied work-RAM boundary into transactional native
  invocation and define the first bounded no-interrupt delivery contract.
- [x] Shadow-test one already mapped, bounded native routine and demonstrate
  faithful end-to-end replacement while unported assembly continues.
- [x] Complete BS-4's first event-specific rule family and reproduce its
  completion and timeout outcomes.
- [x] Complete BS-5's first event-specific rule family and reproduce its
  completion and timeout outcomes.
- [x] Complete BS-6's first event-specific rule family and reproduce its
  completion and timeout outcomes.
- [x] Establish the bounded Milestone 2 bonus-stage baseline: all six stages
  have a first event family, timeout, completion, ordinary scored result, and
  perfect result.
- [x] Close the Milestone 2 controls-and-physics route.
- [x] Close the Milestone 2 collision-boundary route.
- [x] Close the Milestone 2 session-and-scoring route.
- [x] Publish the dynamic/static/deferred coverage manifest and close
  Milestone 2.
- [x] Add a copied VDC/VCE frame-boundary snapshot boundary.
- [x] Implement and test generic PCE BAT/tile/palette decoding,
  world-coordinate stitching, and camera cropping.
- [x] Decode and stitch the three normal-table backgrounds:
  - [x] Capture stable post-fade pattern and palette state for the upper,
    middle, and lower regions.
  - [x] Prove the exact visible world extent and separate visible,
    prefetch, panel, and unrelated source rows.
  - [x] Compare native background crops against independent emulator
    rendering evidence rather than a second invocation of the same decoder.
  - [x] Define the actor/sprite overlay inputs required for the final
    cabinet playfield composite.
- [x] Add the cabinet playfield, DMD, and backglass windows.
- [ ] Add display assignment and finish playfield composition:
  - [x] Switch bonus stages and non-gameplay presentation back to the
    emulated framebuffer.
  - [x] Persist display identities, geometry fallbacks, rotation, and
    fullscreen state.
  - [x] Add the interactive display-assignment screen.
  - [x] Load the user-selected PNG/JPEG/WebP backglass.
  - [x] Render the copied live-camera SAT actor layer in table-world
    coordinates.
  - [x] Preserve packed-background opacity and apply exact HuC6270
    background-vs-sprite priority and the scanline sprite limit.
  - [x] Decode mapped actor slots and retain exact fixed-mechanism visuals
    after they leave the live camera.
  - [x] Map the original fixed-actor descriptor renderer and validate native
    metadata against copied SAT groups.
  - [x] Decode the generic camera-region sprite-pattern stream and reproduce
    the lower-to-upper-middle pattern transition exactly.
  - [x] Decode all 42 `00:F606` selector entries, including the generic raw
    upload branch, into a bounded local pattern atlas.
  - [x] Apply stream-backed atlas entries before first camera visits and
    validate lower/upper-middle/middle persistent composites.
  - [x] Decode `$F768`'s resident `$7C00` assets, classify bank-8
    appearances outside `00:F606`, and pre-seed the remaining fixed
    mechanisms.
  - [x] Render every non-hidden fixed actor over the accepted table
    background with live appearance/phase changes.
  - [x] Keep the ball trail-free and map all six flipper/control slots into
    stable, animated table-world presentation.
  - [x] Retain and animate dynamic enemy slots `$62-$7E` across camera
    movement, with explicit slot-reuse invalidation.
  - [x] Classify transient SAT effects and retain only table-world effects.
  - [x] Validate upper/middle/lower actor activity on a deterministic
    playable route.
  - [x] Keep inactive retained actors visible inside the current camera band,
    resolve clustered actor geometry from exact ROM groups, and composite
    the live SAT last.
  - [ ] **Deferred at user request:** map the two-player/current-player RAM
    contract and add the P1/P2 discriminator to the DMD.
- [ ] Port gameplay routines behind the transactional replacement boundary:
  - [x] Replace and shadow-test bounded `00:F1B4@17` position integration.
  - [x] Replace and shadow-test interrupt-free `00:F1B4@1` nonnegative
    position integration.
  - [x] Replace and shadow-test interrupt-free `00:F1B4@10` integer-step
    early return.
  - [x] Replace ordinal admission for `00:F1B4` with semantic slot-zero
    integration; cover all 1,156 route calls through path-sensitive
    50-57-boundary transcripts.
  - [x] Retain bounded `00:F241@1` collision rollback as the reference slice.
  - [x] Schedule state-generic `00:F241@all` rollback with checkpointed
    mid-routine interrupt delivery; cover all 404 route calls natively.
  - [x] Replace and shadow-test bounded `01:DD09@1` zero-component
    acceleration.
  - [x] Replace and shadow-test interrupt-free `01:DD09@2` nonzero
    second-component damping and acceleration.
  - [x] Replace and shadow-test interrupt-free `01:DD09@181` negative
    divider and sign restoration.
  - [x] Add semantic `00:E905@all` signed division and compose it into
    semantic `01:DD09@all`; cover all 752 child and 753 parent calls with
    exact stack, scratch, timing, write, and original-interrupt parity.
  - [x] Replace and shadow-test interrupt-free `01:D1A2@23` signed
    two-dimensional rotation; initially keep interrupt-bearing `D1A2@1`
    emulated pending checkpointed child composition.
  - [x] Replace ordinal admission with semantic `01:D1A2@all`; compose four
    `E65C` children and one `E837` child with exact scratch, indexed output,
    stack, timing, write, and original-interrupt parity across all 354
    current route calls.
  - [x] Add semantic `01:D612@all` flipper-radius projection; compose one
    `E65C` child with exact scratch, result, stack, timing, write, and
    original-interrupt parity across all 62 calls.
  - [x] Add semantic `01:D62E@all` directional-angle projection; compose
    its direct/wrapped delta, five-step 8.8 conversion, one `E65C` child,
    conditional negation, and original-interrupt parity across all 62 calls.
  - [x] Add semantic `01:CFD8@all` collision-motion adjustment; cover both
    clamp directions and 3/16, 1/4, 1/2, and 1/16 signed scaling across all
    354 calls with exact write, timing, flag, and original-interrupt parity.
  - [x] Replace and shadow-test interrupt-free `03:7FB1@1` actor
    collision-response advancement.
  - [x] Replace and shadow-test interrupt-free `03:7FB1@3` follow-up
    countdown advancement.
  - [x] Admit interrupt-bearing `03:7FB1@2` through the aggregate
    instruction-checkpoint transcript.
  - [x] Replace and shadow-test interrupt-free `03:7FB1@7`
    countdown-reload and phase advancement.
  - [x] Replace `03:7FB1` ordinal admission with semantic response,
    countdown, phase, flag, write, timing, and interrupt checkpoints; cover
    all 21,987 census calls.
  - [x] Compose semantic `03:7F57@all` no-hit collision consumption from
    the existing `03:6966` and `03:7FB1` adapters; compose the checkpointed
    alternate-pass and one-axis children through both parent calls and cover
    18,954 of 20,867 calls with exact 362,060-write aggregate parity.
  - [x] Add semantic `00:F55C@all` ordinary score accumulation; cover all 40
    census calls through a 44-boundary transcript while retaining exact
    state, call, write, and original-interrupt parity.
  - [x] Add semantic `00:E8B3@all` RNG advancement; cover all 78 census
    calls through a 23-boundary transcript with exact scratch, seed, flag,
    and cycle parity.
  - [x] Add semantic `00:E837@all` signed 16-bit scratch negation; cover all
    894 census calls through 8/9-boundary, 90/102-cycle transcripts with
    exact flag, write, and original-interrupt parity.
  - [x] Add semantic `00:E624@all` unsigned multiply rounds; cover all 5,371
    census calls through generated 41-113-boundary transcripts with exact
    shift/add, carry, timing, write, and original-interrupt parity.
  - [x] Add semantic `00:E65C@all` signed fixed-point multiplication;
    compose one or two `E624` children with exact sign, nested-stack,
    timing, write, and original-interrupt parity across all 3,700 calls.
  - [x] Add semantic `01:D09E@all` flipper vector projection; compose four
    `E65C` children and one `E837` child with exact scratch, sum, stack,
    timing, write, and original-interrupt parity across all 540 calls.
  - [x] Add semantic `00:F4FC@all` actor camera-window classification; cover
    all 65,927 census calls through 2/18/31-boundary transcripts with exact
    scratch, flag, cycle, and original-interrupt parity.
  - [x] Add semantic `01:C574@all` generic actor-slot reset; cover all 132
    census calls through one generated 361-boundary loop transcript with
    exact stack, scratch, 32-field clear, cycle, and interrupt parity.
  - [x] Add semantic `01:D466@all` flipper input gating and common
    activation/coroutine handoff; cover 7,468 of 7,470 calls through
    3/21-boundary transcripts.
  - [x] Add semantic `01:CC1A@all` collision-scan vertical rejection; cover
    19,192 of 85,715 calls through exact 150/180/246/282-cycle, three- or
    six-write state-derived transcripts.
  - [x] Add the atomic semantic `01:CCD5@all` near-target geometry leaf;
    cover 5,819 of 66,523 calls through exact ROM-resolved 783-891-cycle,
    16/19-write transactions.
  - [x] Add path-sensitive instruction checkpoints to semantic
    `01:CCD5@all`; cover all 66,523 geometry calls while original interrupt
    handlers preempt and return inside the native transaction.
  - [x] Add semantic `01:D500@all` bidirectional flipper phase stepping;
    cover all 980 calls through 9/11-boundary transcripts with exact
    three-write timing and flags.
  - [x] Add semantic `01:D081@all` phase-indexed flipper geometry lookup;
    cover all 540 calls with exact ROM-table, stack, flag, 159-cycle native
    body, and original-handler parity.
  - [x] Add semantic `01:C5C9@all` shared actor-coroutine scheduling; cover
    all 2,462 calls with exact stack, delay, dispatch, flag, 102-192-cycle,
    and original-interrupt parity.
  - [x] Add semantic `01:C3CB@all` session/nudge gating; cover all 2,649
    calls through checkpointed 69-cycle session, 177-cycle idle, 207-cycle
    timer-reload, 762-cycle active-start, and 522-cycle active-finish paths,
    including exact bounded VDC-port writes.
  - [x] Add semantic `01:C49E@all` session-input gating; cover all 2,649
    calls through the checkpointed active-player 72-cycle no-Run-input return.
  - [x] Profile aggregate `01:DD09` viability; the initial 669-of-753
    interrupt blocker is resolved by the later semantic `E905` child and
    checkpointed parent composition.
  - [x] Replace and shadow-test interrupt-free `03:6966@5`
    collision-table alternate-pass early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@1`
    full collision-table record advancement.
  - [x] Replace and shadow-test interrupt-free `03:6966@2`
    reverse full collision-table record advancement.
  - [x] Replace and shadow-test interrupt-free `03:6966@3`
    vertical full collision-table record advancement.
  - [x] Replace and shadow-test interrupt-free `03:6966@4`
    second vertical full collision-table record advancement.
  - [x] Replace and shadow-test interrupt-free `03:6966@6`
    second collision-table alternate-pass early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@7`
    third collision-table alternate-pass early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@8`
    fourth collision-table alternate-pass early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@9`
    alternate-selector reverse full-record advancement.
  - [x] Replace and shadow-test interrupt-free `03:6966@10`
    headerless vertical continuation.
  - [x] Replace and shadow-test interrupt-free `03:6966@12`
    second headerless vertical continuation.
  - [x] Replace and shadow-test interrupt-free `03:6966@14`
    alternate-selector early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@15`
    repeated fixed-selector early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@16`
    second repeated fixed-selector early return.
  - [x] Replace and shadow-test interrupt-free `03:6966@17`
    third repeated fixed-selector early return with status `$12`.
  - [x] Replace and shadow-test interrupt-free `03:6966@18`
    fourth repeated fixed-selector early return with status `$00`.
  - [x] Replace and shadow-test interrupt-free `03:6966@19`
    alternate-selector reverse-vertical continuation.
  - [x] Replace and shadow-test interrupt-free `03:6966@20`
    headerless horizontal continuation.
  - [x] Replace and shadow-test interrupt-free `03:6966@21`
    headerless reverse-horizontal continuation.
  - [x] Replace and shadow-test interrupt-free `03:6966@22`
    later headerless vertical continuation with status `$12`.
  - [x] Replace and shadow-test interrupt-free `03:6966@23`
    second later headerless vertical continuation with status `$00`.
  - [x] Replace ordinal-by-ordinal `03:6966` scheduling with semantic
    alternate-pass admission and conservative hardware-event fallback;
    cover 4,868 of 19,648 route calls natively.
  - [x] Resolve ordinary headerless `03:6966` record steps from the ROM
    tables and per-slot cursors, including two-axis movement and high-byte
    carry/borrow; cover 7,437 of 19,648 route calls natively.
  - [x] Extend the bounded `03:6966` walker through `$C0`, `$FE`, `$FF`,
    and headered records; cover 7,476 of 19,648 route calls natively.
  - [x] Separate harmless timer-divider ticks from serviceable timer IRQs in
    the core scheduling horizon; cover 7,609 of 19,648 route calls natively.
  - [x] Add all 17 alternate-pass instruction checkpoints; cover 11,230 of
    19,648 route calls natively while original interrupt handlers remain
    exact.
  - [x] Add the generic 50-checkpoint, one-axis headerless record family;
    cover 17,308 of 19,648 route calls and compose 18,954 of 20,867
    Small-Soldier parent calls.
  - [x] Extend that state-based transcript to bounded two-axis headerless
    records; cover 18,806 of 19,648 route calls and compose 20,113 of 20,867
    Small-Soldier parent calls.
- [ ] **Milestone 4 deferred:** reproduce additional normal-table and
  bonus-stage rule-family variants as their routines are ported.
- [x] Extend replacement scheduling with faithful mid-routine interrupt
  delivery; `01:D081@all` is the first selected interrupt-bearing proof.

## Active Work

Milestone 4 is now active. Semantic `03:6966@all` scheduling replaces 18,806
of 19,648 deterministic-route calls by selector/slot state, while the
previous `03:6966@1` through `@23` admissions remain focused regression
coordinates for implemented full-record, continuation, and early-return
branches. Further expansion of this unusually large interpreter is frozen
while smaller routines are ported. Semantic `00:F1B4@all` integration and
`00:F241@all` rollback now replace all 1,560 calls across their position
pipeline. Semantic `03:7FB1@all` replaces all 21,987
collision-response calls across all live slots, including 1,042 original
interrupt-handler passages; `@1`, `@3`, and `@7` remain focused
decrement/reload fixtures, while `@2` is covered by the aggregate natural
route.
Semantic `03:7F57@all` composes every admitted collision-table transaction
and both response branches for 20,113 of 20,867 Small-Soldier no-hit calls.
Its alternate-pass, one-axis, and bounded two-axis paths compose 35/38,
68/71, or 73/76 parent checkpoints around the child transactions, including
both nested call/return boundaries. The single state-based adapter preserves
all 362,060 aggregate writes in Debug and Release.
Semantic `00:F55C@all` replaces all 40 ordinary score-addition calls through
a 44-boundary transcript with exact 24-bit arithmetic, nested call-stack,
display-dirty, and original-interrupt state. Semantic `00:E905@all` replaces
all 752 signed-divider calls through a carry-sensitive, 16-round transcript;
all 583 original interrupt-handler passages and 52,804 writes remain exact.
Semantic `01:DD09@all` composes that child and replaces all 753 damping
calls, including the 669 calls that were previously deferred for interrupts.
Semantic `00:E8B3@all` replaces all 78 shared RNG updates from live
seed/frame state through a 23-boundary transcript with exact eight-write and
282-cycle parity.
Semantic `00:F4FC@all` replaces all 65,927 actor camera-window checks from
live slot and camera state through 2/18/31-boundary empty, below, inside, and
above transcripts. All 4,858 original interrupt-bearing scopes remain exact.
Semantic `01:C574@all` replaces all 132 generic 32-array actor-slot resets
through one generated 361-boundary transcript. All 35 original
interrupt-bearing scopes and all 14,163 aggregate writes remain exact.
Semantic `01:D466@all` replaces 7,468 of 7,470 flipper/control-piece input
checks through exact 3/21-boundary no-input and common activation/coroutine
transcripts. The two remaining current-slot calls enter `D525` and emit
823/868 ordered writes, beyond the bridge's 512-write limit; later
held/release behavior outside this route also remains emulated.
Semantic `01:CC1A@all` replaces 19,192 of 85,715 central collision-scan
calls through exact state-derived positive/negative page and low-byte
vertical-separation rejection transcripts. The complete
85,715-call/1,784,119-write artifacts remain exact; near-target, pointer,
mask, and contact paths remain emulated.
Semantic `01:CCD5@all` replaces all 66,523 near-target geometry calls through
live-MPR ROM pointer resolution and exact path-sensitive
783/789/885/891-cycle transcripts. Original interrupt handlers preempt and
return at the declared instruction boundaries, and all 1,414,954 aggregate
writes remain exact.
Semantic `01:D500@all` replaces all 980 forward/reverse flipper phase steps
on the route through exact 9/11-boundary, three-write, 126/138-cycle
transcripts; endpoint and non-expiring timer paths outside the route remain
emulated.
Semantic `01:D081@all` replaces all 540 phase-indexed flipper geometry
lookups through a 14-checkpoint, exact five-write, 159-cycle ROM-table
transaction; 38 original interrupt-handler passages remain exact. The
downstream `D09E` vector, `D612` radius, and `D62E` directional-angle
projections are now native; the branch-heavy `D525` parent controller
remains emulated.
Semantic `01:C5C9@all` replaces all 2,462 shared actor-coroutine scheduler
calls. It derives immediate installation, delayed yield, delayed installation,
and return-page carry from live stack/slot state through exact 102-192-cycle
transactions. Branch-sensitive checkpoints let Beetle deliver original
interrupts inside the native transcript instead of falling back.
Semantic `01:C3CB@all` replaces all 2,649 session/nudge checks through
8/17/20/66/44-boundary transcripts for the exact 69-cycle session,
177-cycle timer-decrement idle, 207-cycle timer-reload, 762-cycle
active-start, and 522-cycle active-finish paths. The six active Button-II
calls retain their original base-VDC port writes and wait cycles through the
bounded replacement bus-write contract.
Semantic `01:C49E@all` replaces all 2,649 pause/high-score session checks
observed on the route through a seven-boundary transcript for the exact
72-cycle no-Run-input return; pause, high-score, and transition behavior
outside the route remains emulated.
Semantic `00:E837@all` replaces all 894 shared signed-word negations from
live `$2F:$30` through exact 8/9-boundary, three/four-write, 90/102-cycle
transcripts. All 24 original interrupt-handler passages remain exact.
Semantic `00:E624@all` replaces all 5,371 inner unsigned multiply rounds
through state-generated 41-113-boundary transcripts. All 1,592 original
interrupt-handler passages and 181,868 aggregate writes remain exact.
Semantic `00:E65C@all` composes one or two of those rounds with signed
operand normalization, nested stack boundaries, fixed-point result
selection, and sign restoration. All 3,700 wrapper calls execute natively,
including 2,157 calls with original handler writes; all 244,531 aggregate
writes remain exact.
Semantic `01:D09E@all` composes four signed multiply wrappers and one
negation into both flipper projection outputs. All 540 calls execute
natively, including 448 calls with original handler writes; all 168,532
aggregate writes remain exact.
Semantic `01:D1A2@all` composes the same child family into both indexed
signed-rotation outputs. All 354 current route calls execute natively,
including 296 calls with original handler writes; all 103,362 aggregate
writes remain exact.
Semantic `01:D612@all` composes one signed multiply into the controller's
`$C2:$C3` radius projection. All 62 calls execute natively, including 51
calls with original handler writes; all 6,271 aggregate writes remain exact.
Semantic `01:D62E@all` composes the directional angle delta and one signed
multiply into `$31:$32`. All 62 calls execute natively, including 43 calls
with original handler writes; all 5,062 aggregate writes remain exact.
Semantic `01:CFD8@all` covers both collision-motion clamp directions and all
four signed scaling modes. All 354 calls execute natively, including 41
calls with original handler writes; all 4,103 aggregate writes remain exact.
Semantic `00:E905@all` covers signed normalization, the carry-sensitive
16-round divider, and quotient sign restoration. All 752 calls execute
natively, including 583 calls with original handler writes; all 52,804
aggregate writes remain exact.
Semantic `01:DD09@all` composes that divider for live nonzero motion axes.
All 753 calls execute natively, including 669 calls with original handler
writes; all 61,291 aggregate writes remain exact.
The focused `01:D1A2@23` signed-rotation fixture,
`01:DD09@1` zero-component acceleration, `01:DD09@2` positive-divider
damping, `01:DD09@181` negative-divider damping, `00:F1B4@1` nonnegative
integration, `00:F1B4@10` integer-step
integration, `00:F1B4@17` negative-second integration, and `00:F241@1`
collision rollback execute through the
transactional copied-RAM boundary across the physics and collision routes,
then shadow against restored assembly with exact register, MPR, cycle,
write-order, and complete-RAM parity. Positive-state repeated passes and the
reverse-oriented `SMB0 $35` record class remain frozen with the collision
interpreter. Calls reaching the conservative user/video-event or
serviceable-timer-IRQ horizon—including ordinals 11 and 13—fall back
automatically. Unimplemented collision-interpreter semantic paths remain
emulated; checkpointed `DD09@all`, `D1A2@all`, and `7FB1@all` now cover
every observed damping, rotation, and response call.

The application boundary is now explicit: `main.cpp` owns process/runtime
orchestration and calls one native-replacement dispatch API, while the
replacement implementation units own the instruction-faithful adapters.
Collision, physics/arithmetic, controls/actor, and RNG/score synthesis now
occupy four subsystem implementation files behind a private internal
interface; `native_replacements.cpp` is the 234-line registry. The
extraction is behavior-neutral and preserves the existing pure C
`src/game_logic` boundary. The 27 directly affected subsystem ROM-backed
regressions pass in Release, representative Debug/Release semantic hashes
remain exact, and both application configurations compile cleanly.

Milestone 2 is complete. Milestone 3 now has the copied-video boundary,
directional `00:F400` stream map, packed-source decoder, accepted
256-by-668 background world, independently verified upper/middle/lower
crops, a copied post-DMA SAT boundary, a live-camera sprite decoder, and a
working three-window cabinet vertical slice. The previous 256-by-768
frame-900 artifact remains only a dark fade-state diagnostic. Display
identity, geometry fallback, orientation, fullscreen state, the F12
assignment screen, persisted PNG/JPEG/WebP backglass images, the
camera-visible SAT actor layer, background/sprite priority, typed actor-slot
views, decoded-pattern persistence for camera-visited fixed mechanisms, and
the exact bank-8 descriptor metadata decoder, generic `00:E41F` compressed
VRAM stream decoder, observed `00:F606` appearance/phase selector path,
exact frame-1,200-to-1,600 pattern reconstruction, and complete 42-entry
local pattern atlas are now complete. Stream-backed entries are applied to
bank-8 actor metadata before first camera visits. All five `$F768` scene
packs are decoded, and the 24 resident/non-selector fixed groups are proven
disjoint from every known upload. The proof now covers every fixed actor:
all 61 non-hidden groups have an exact pattern source, with 23 supplied by
the live camera and 38 pre-seeded at the lower checkpoint. Lower,
upper-middle, and middle persistent composites have refreshed deterministic
goldens and preserve live appearance/phase changes. `$F768` selectors 1-4
remain scene-unmapped but do not block fixed-actor composition. The ball is
now proven table-world and remains live-only, while all three
flipper pairs are retained with live phase metadata. Ten Small Soldiers and
four Tower insects use the same phase-aware table-world presentation, with
explicit family admission and slot-reuse invalidation. Type-`$8F`,
appearance-`$10` impact bursts now form the separately classified
table-world transient family: frame 2,100 guards their exact SAT metadata,
off-camera persistence, and upload-disjoint source, while later checkpoints
guard complete release without trails. All other dynamic effects remain
live-camera-only. The actor compositor now uses exact ROM descriptor groups
instead of nearest-anchor SAT association, retains inactive actors even when
their pixels overlap the live camera band, and draws the live SAT last. The
deterministic route guards all six flippers at every post-fade checkpoint and
proves the eight Sorcerers continue changing position after leaving their
region. Actor presentation Y is normalized past the 30-line status band,
the stitched background retains a zero-pixel vertical offset across launch,
and retained palette indices follow the settled VCE palette instead of
freezing fade-time colors. Two-player/DMD work is explicitly deferred again at the user's
request; Milestone 4 proceeds without that discriminator.
`$2251` remains proven only as the active score-array selector. The finite
dynamic/static/deferred scope remains frozen in
`docs/COVERAGE_MANIFEST.md`.

All six bonus stages have a bounded event family plus deterministic timeout,
completion, ordinary scored-result, and perfect regressions. All nine
bonus/ending-bank RNG consumers and all 12 normal-table consumers are
dynamically bounded. Retain frozen semantic `03:6966@all` scheduling and its
ordinal regression fixtures,
`01:DD09@1`,
`01:DD09@2`,
`01:DD09@181`,
semantic `00:F1B4@all` and `00:F241@all`, their focused ordinal fixtures,
and `00:F241@1` as the current
replacement vertical slice.

## Blockers

No external blocker prevents continuing Milestone 3 cabinet composition.
The accepted background source is rows 20-687 of the
packed 256-by-768 envelope; cabinet code must not display the guard, prefetch,
or tail rows. Camera-visited fixed mechanisms now retain decoded pattern
pixels independently of later regional VRAM changes while resolving colors
from the current VCE palette. The bank-8 actor renderer also
supplies exact geometry and descriptor metadata for unseen mechanisms. Its
generic compressed loader reproduces one complete camera transition
exactly, its selector family resolves to a bounded local atlas, and all
remaining fixed pattern blocks are proven upload-disjoint before static
VRAM seeding. No unresolved sprite-pattern source blocks normal-table
fixed-actor composition. `$F768` selectors 1-4 remain a future scene-mapping
question. Ball/flipper coordinate ownership and the two persistent dynamic
enemy families are closed, as is the first bounded table-world transient
family. The DMD discriminator is intentionally deferred at the user's
request; when resumed, it still requires a two-player route that
distinguishes the actual player-count/current-player fields from `$2251`'s
already proven score-array selection role. The longer state route exposes a
configuration-specific Beetle framebuffer variance beginning at frame 2,147;
work RAM, audio, camera scopes, and ordered writes remain identical. That
visual difference must be explained or normalized before this route becomes a
single cross-configuration framebuffer golden. The normalized replay boundary
removes frontend frame placement from deterministic no-interrupt call
comparison. Faithful checkpointed mid-routine interrupt delivery is now
available and proven by all 540 `01:D081` calls. Other interrupt-bearing
routines are no longer bridge-blocked, but each still needs an exact
instruction-boundary transcript for its implemented semantic branch.
`03:6966@11` enters through `03:69A2` / `00:FC26`, while `03:6966@13`
shifts interrupt-owned stores beginning near `03:69B7`; both remain emulated
until their corresponding semantic paths supply checkpoints. Checkpointed
`00:F1B4@all` and `00:F241@all` now cover every integration and rollback
call in their 1,560-call position pipeline.
`01:DD09@52` demonstrated that interrupt-owned stack and controller writes
can enter through `00:E058/E169` even when a natural trace does not visit the
previously catalogued `FCxx/FDxx` handler range. Semantic `00:E905@all` and
its `01:DD09@all` parent now supply those instruction checkpoints, so this
call and every other observed damping call execute natively.
The downstream flipper projection `01:D09E` is now fully native across all
540 calls. Its 5,328-33,471-cycle observed range is explained by original
interrupt handlers retained through the composed `E65C`/`E837` checkpoints.
The fixed checkpoint capacity is 2,048 for this bounded parent while the
independent write capacity remains 512.
The same write bound explains the two `01:D466` fallbacks: their current-slot
paths enter `D525` at frames 1,820 and 2,255 and emit 823/868 writes. They
remain part of the `D525` decomposition boundary rather than an unresolved
`D466` branch.
The shared signed multiply wrapper `00:E65C` is now fully native across all
3,700 route calls by composing the existing semantic `E624` transaction.
Its 921-27,306-cycle observed range is explained by original interrupt
handlers retained through exact instruction checkpoints. Adjacent helpers `E807`,
`E813`, `E825`, and `E85D` were not entered by the deterministic route and
remain emulated until a natural caller is reproduced.
Undead-Head launch consumer `03:6177` is dynamically bounded through fixed
slot `$4D`, but its 5,778-cycle combined-route scope contains a periodic
interrupt. Its RNG-owned writes are deterministic, while normalized replay
still detects shifted interrupt-owned writes. Keep it out of replacement
coverage until its native semantic path supplies checkpoints or a separately
proven no-interrupt invocation is reproduced.
The frame-1,300 forced-entry bonus route starts result coroutine `04:456C`
before a newly selected stage controller can finish shared completion
animation `04:44DB-4560`; result startup then stops stage-actor scheduling.
Selecting BS-1 and BS-3 through BS-6 earlier now leaves enough time for each
authentic initializer, objective controller, and completion path to write
`$2DBA` at `04:4560`. BS-2 now has the same completion-owned latch ordering
across all 28 cells from full live-target state `04:5079`, through collision
geometry and board resolution, to the result.
The maximum-score route has a second isolated presentation-only variance at
frames 1,555-1,680. Its work RAM, audio, and all ending/RNG semantic traces
are identical, so configuration-specific census hashes guard it without
blocking mapping.

## Decisions Locked for V1

- Windows 10/11 x64.
- Public GPL-3.0-or-later source with a user-supplied ROM; no ROM or extracted assets in releases.
- Exact USA ROM only.
- Beetle PCE remains as the hardware compatibility layer.
- Original gameplay behavior is the faithful default.
- Gameplay logic is ported incrementally behind a C ABI, using C for the
  first gameplay module and C++ for the host/transaction adapter.
- Sharp pixel scaling is always enabled; no alternative filters.
- Desktop mode retains original scrolling.
- Cabinet mode uses:
  - Full stitched table on the playfield monitor.
  - Bonus stages on the playfield monitor in place of the table.
  - Score on the DMD monitor.
  - User-selected static image on the backglass monitor.
- Initial cabinet input supports keyboard/controllers and semantic feedback hooks; physical DMD, DOF, and SSF support are deferred.

## Progress Update Template

When work advances, update:

1. The current phase, completed closure gates, and milestone status.
2. The `Completed` and `Next Actions` checklists.
3. `Active Work` with the current task and owner.
4. `Blockers` with a concrete resolution condition.
5. The date at the top of this document.
