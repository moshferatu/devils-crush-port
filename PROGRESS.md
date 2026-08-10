# Project Progress

Last updated: 2026-08-09

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
controls/actor, and rules/RNG/score synthesis now occupy four subsystem
implementation units behind a private internal interface;
`native_replacements.cpp` is the 555-line central registry. The extracted
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

Semantic `02:42F6@all` now covers the Dragon's-Egg group controller's
nonzero-count return. It reads shared live count `$2CF4` and reproduces the
load, branch, accumulator, flags, and 21-cycle return through two instruction
checkpoints. All 1,790 route calls execute natively; the complete
1,790-call/219-handler-write artifacts, including 17 interrupted calls,
remain byte-identical in Debug and Release. The count-zero award, latch
update, and reset path remains available as assembly fallback.

Semantic `02:4345@all` now covers all 9,857 fixed Dragon's-Egg hit checks.
Idle calls retain their exact 21-cycle, zero-write return. The frame-2,425
hit installs the 10-unit score arguments through a 57-cycle, two-write
handoff; after the original score/audio helpers, semantic `02:4357@all`
disables the egg and installs the `$39` impact/allocation parameters through
93 cycles and four writes. After the original burst and hatchling allocators,
semantic `02:4371@all` writes the activation byte in 21 cycles and returns to
the original scheduler. The complete 9,857-call/1,682-write artifacts remain
byte-identical in Debug and Release.

Semantic `02:4DC8/4DD9@all` now covers the Giant Skeleton's Eyeball
initializer and activation gate's dormant path. The fixed slot-`$2F`
initializer clears the transient flag/latch/state fields and installs its
steady script through the exact 258-cycle, ten-write helper transcript. The
gate reads the Dragon's-Egg completion latch `$2D20` and session flags `$53`,
then reproduces the two masks, branches, accumulator, flags, and 57-cycle
return through six instruction checkpoints. All 1,988 route gate calls plus
the one initializer execute natively, including 37 original interrupt-handler
passages; the complete 1,989-call/503-write expanded artifact remains
byte-identical in Release. Opening and final-animation paths remain available
as assembly fallback when their state becomes active.

Semantic `02:4FC1@all` now covers the three Mercy Stars' activation gate's
not-last-ball path. It reads normal-table mode `$2254`, decrements remaining
balls `$2D52`, and reproduces both loads, branches, accumulator, flags, and
54-cycle return through five instruction checkpoints. All 7,950 route calls
execute natively, including 163 original interrupt-handler passages; the
complete 7,950-call/2,173-write artifacts remain byte-identical in Debug and
Release. Non-normal-mode, last-ball score-limit, and activation paths remain
available as assembly fallback when their state becomes active.

Semantic `03:65C2@all` now covers the Three-Blobs result controller's two
mismatch returns. It reads live color states `$2D39-$2D3B` and reproduces
the load, one or two comparisons, accumulator, compare flags, and 42/63-cycle
return through three/five instruction checkpoints. All 2,650 route calls
execute natively, including 50 original interrupt-handler passages; the
complete 2,650-call/665-write artifacts remain byte-identical in Debug and
Release. Equal-state score and randomized-reset outcomes remain available as
assembly fallback.

Semantic `03:62B8@all` now covers the Giant Door's idle hit gate and the
geometry-delivered collision-clear/score-load prefix. Semantic `03:62CB@all`
reproduces the exact first-two-hit 27-cycle counter decrement/nonterminal
branch to original `03:62D0` and third-hit 33-cycle zero branch to native
`03:62D4`. That successor now reproduces the exact 57-cycle, four-write
50,000-point argument/stack handoff to the original score helper. Its
post-score `03:62E2` successor now reproduces the exact 243-cycle, nine-write
completion-state/coroutine handoff: it marks the Door complete, clears its
counter, and installs `03:62F0` through native-equivalent `01:C018/C5C9`
scheduling. That `03:62F0` successor now reproduces its exact 243-cycle,
eight-write completion-channel request and coroutine handoff, setting bit zero
of the indexed `$2D06` channel and installing `03:62FE`. That successor now
owns its exact 48-cycle clear-bit wait return through four instruction
checkpoints, including all five observed timer-interrupt passages. The later
`03:6308` set-bit successor now owns all four channel advances and their
coroutine reschedules: channels one through three take the exact 252-cycle,
eight-write path, while terminal channel four takes its exact 246-cycle,
eight-write handoff to the one-byte `03:6318` coroutine-return callback.
Semantic `03:7378@all` now consumes each released Knight channel through its
exact 363-cycle, 12-write activation transaction, clears bit zero, disables
the waiting surface, initializes motion, and installs `03:73A6`; later motion
and hit handling remain assembly-owned. The three carry-clear movement
fixtures, `03:73A6@2`, `03:73A6@4`, and `03:73A6@16`, now compose the exact
165-cycle collision-free table pass: ordinal two performs the 360-cycle,
nine-write timer-decrement/no-hit tick; ordinal four takes the 429-cycle,
11-write timer-reload/phase-advance/no-hit tick; and ordinal 16 takes the
438-cycle, 12-write phase-wrap/no-hit tick. The first checkpointed
interrupt fixture, `03:73A6@58`, preserves its 1,053-cycle, 22-write
timer-interrupt passage byte-for-byte. Collision-return fixture `03:73A6@60`
preserves its 1,233-cycle, 27-write timer-interrupt passage and `73AF` stacked
return boundary; the collision/multi-pass and remaining
interrupt-bearing movement records remain
assembly-owned. The dedicated terminal routes are byte-identical in Debug and
Release.

Semantic `03:73A6@all` subsequently proved the complete compatible subset:
all 64 collision-free, one-pass, no-hit calls on the controlled 129-call
route now execute natively, including the timer-decrement, timer-reload,
phase-wrap, and checkpointed `73D8`/`73AF` interrupt boundaries. Collision/
multi-pass and hit records conservatively return to assembly; the aggregate
census, call, and ordered-write artifacts remain byte-identical.

Semantic `02:445B@all` now covers the hidden Big-Star feature controller's
idle gate. It reads shared Little-Skull latch `$2CFA` and reproduces the
load, not-taken nonzero branch, accumulator, flags, and 21-cycle return
through two instruction checkpoints. All 2,650 route calls execute natively,
including 28 calls containing original interrupt handlers; the complete
2,650-call/364-write artifacts remain byte-identical in Debug and Release.
The unexercised pending-feature path retains its Big-Star direction, actor,
score, and audio updates in assembly.

Semantic `03:6339@all` now covers the Walled Star's idle hit gate. It reads
indexed collision state `$3800,X` and shared access latch `$2DBC`, reproducing
both loads, both taken branches, accumulator, flags, and the 54-cycle return
through four instruction checkpoints. All 2,650 route calls execute natively,
including 49 calls containing original interrupt handlers; the complete
2,650-call/659-write artifacts remain byte-identical in Debug and Release.
Wall-damage and active post-destruction access paths remain in assembly.

Semantic `03:60C3@all` now covers all 2,646 fixed-Tower hit checks. Idle
calls preserve the two-checkpoint, 27-cycle return; the routed hit clears
indexed collision state and reaches the shared score helper after 63 native
cycles. Score and audio remain emulated, semantic `03:60E0@all` then advances
Tower progress in a 156-cycle handoff to the interrupt-bearing shared
dispatcher, and semantic `03:611C@all` composes the native coroutine
scheduler across all four transition calls. The complete 2,646-call/538-write
outer artifacts remain byte-identical in Debug and Release.

Semantic `03:60A6@all` now covers the adjacent Tower Guard's closed-selector
update. It derives selector `$84` from the live nonzero `$2D4D` gate counter,
preserves accumulator, Y, flags, indexed `$3D00,X` write timing, and the
48-cycle return through four instruction checkpoints. All 2,650 route calls
execute natively, including 58 calls containing original interrupt handlers;
the complete 2,650-call/3,403-write artifacts remain byte-identical in Debug
and Release. Counter zero is classified as selector `$8E` but remains in
assembly with the unexercised open BS-5 entrance.

Semantic `03:7CAF@all` now covers the hidden fixed
collision/scene-response controller's dormant gate. It derives the
nonnegative scene branch from `$2CC0`, the zero-response branch from scratch
`$CC`, and the exact accumulator, flags, and 45-cycle return through four
instruction checkpoints. All 2,650 route calls execute natively, including
54 calls containing original interrupt handlers; the complete
2,650-call/721-write artifacts remain byte-identical in Debug and Release.
Negative-scene handling and the nonzero response state machine remain in
assembly.

Semantic `03:7B27@all` now covers all 2,650 adjacent hidden fixed-scheduler
calls. In addition to the exact 48/87-cycle suppressed and waiting paths,
all 332 timer expiries natively reload `$3A5C` and prepare the first
`$02/$30/$08` submission in a 156-cycle prefix. Semantic `03:7B4E@all` and
`03:7B5D@all` each execute 332 exact 54-cycle, three-write continuations for
selectors `$03` and `$07` after the original banked-call wrapper returns.
The wrapper and exported `08:4030` routine remain assembly. The complete
2,650-call/46,618-write artifacts and state census remain byte-identical in
Debug and Release; only negative-scene handling is unobserved and emulated.

Semantic `03:7B7A@all` now covers all 2,650 paired hidden fixed-marker
animator calls. It retains the exact 48/93-cycle suppressed and waiting
paths, while all 332 expiries natively reload `$3A5D`, increment and
modulo-eight wrap `$3ADD`, select the live phase in Y, and install the first
`$1E:16` descriptor. The uninterrupted routed prefixes consume 231 cycles,
or 240 on the 41 wrap ticks, and return at the original `PHY` before the
drawing helper. Semantic `03:7BBB@all` and `03:7BCD@all` then install
`$1E:18` and `$1E:1A` through 332 exact 36-cycle, two-write continuations
apiece. Semantic `03:7BE0/7BE8/7BEC@all` replaces all 996 marker-wrapper
prologues, post-draw `PLA` continuations, and `PLY` epilogues through exact
33/12/12-cycle segments. The phase-table selection and live stack bytes are
state-derived. Semantic `02:5955@all` replaces all 997 routed five-bit
phase-stride additions to `$2F:$30`, including the 996 marker calls, through
15 checkpoints, eight writes, and 198 uninterrupted master cycles. The
remaining `01:C05D` drawing submission stays in assembly. The complete
2,650-call/88,480-write artifacts and state census remain byte-identical in
Debug and Release; only negative-scene handling is unobserved and emulated.

Semantic `03:74D8@all` now covers all 2,648 routed Giant-Face controller
calls. A clear wake-request bit takes the exact three-boundary, 27-cycle
idle return with no native writes. The 23 interrupt-bearing calls retain all
289 handler-owned writes. Wake and transformation request handling remains
in assembly.

Semantic `03:767E@all` now covers all 873 routed Giant-Face hidden-form
animator calls. Animation modes other than one take the exact
three-boundary, 27-cycle idle return with no native writes. The eight
interrupt-bearing calls retain all 97 handler-owned writes, and the complete
5,608-call Giant-Face family artifact remains byte-identical in Debug and
Release. Mode one's active timer, phase, and drawing state machine remains
in assembly.

Semantic `03:76FA@all` now covers all 867 routed Giant-Face crown collision
checks. A nonnegative indexed collision byte takes the exact two-boundary,
27-cycle no-hit return with no native writes. The seven interrupt-bearing
calls retain all 86 handler-owned writes; the 300-point hit path remains in
assembly.

Semantic `03:7726@all` now covers all 1,214 routed Giant-Face mouth checks.
A zero `$2D21` mouth state plus nonnegative indexed collision byte takes the
exact four-boundary, 48-cycle closed/no-hit return with no native writes. The
22 interrupt-bearing calls retain all 288 handler-owned writes; the open-mouth
capture and 300-point hit paths remain in assembly.

Semantic `02:5A4B@all` now covers all 2,144 Dragon's-Head collision checks.
It preserves the exact 27-cycle idle return and performs the routed hit's
42-cycle indexed collision-latch clear before handing shared score, audio,
bonus, and appearance helpers to assembly. Semantic `02:5A6E@all` then
performs both routed 27-cycle, one-write counter decrements natively. The
complete 2,144-call/866-write outer artifacts remain byte-identical in
Debug and Release, including all 17 idle interrupt passages and both
hit-helper interrupt schedules. Counter one and the eighth-hit
mouth-opening branch deliberately remain in assembly.

Semantic `03:78C7@all` now covers all 3,468 fixed-Bumper checks. It preserves
the exact two-boundary, 27-cycle idle return and executes each of the 11
routed hits through a 165-cycle, four-write collision-clear, phase-advance,
and handler-dispatch prefix. Shared score, audio, appearance, and scheduler
helpers remain in assembly. Semantic `03:78F5@all` performs all nine routed
primary progress increments through exact 36-cycle, one-write transcripts,
and `03:7912@all` performs both routed secondary damage updates through exact
114-cycle, three-write transcripts. The complete 3,468-call/3,127-write
outer artifacts remain byte-identical in Debug and Release. The inactive
ninth-hit destruction award and six-child release remain in assembly.

Semantic `02:5116@all` now covers all 125 fixed Devil Shooter collision-state
checks. It derives indexed state from `$3800,X` and preserves the exact
accumulator, flags, and two-boundary 27-cycle idle or 21-cycle catch branch.
The catch resumes assembly for the occupied latch and shared `00:C087`
ball-position/audio helper, including its interrupt. Semantic
`02:52B8@all` then owns the complete 228-cycle, nine-write post-helper
continuation: it writes the catch presentation/state fields and composes
native `01:C5C9` to install dispatch `02:52C3`. The complete
125-call/116-write outer artifacts and the focused one-call/nine-write
continuation artifacts remain byte-identical in Debug and Release.
Semantic `02:5159@all` now also replaces the four exact 96-cycle, zero-write
released-Button-I returns for fixed Shooter slot `$0E`; held charge, occupied,
alternate-mode, and launch paths remain assembly-backed.
Semantic `02:52E3@all` now replaces 137 horizon-safe normal charge-position
calls across the main Shooter's two natural callers, preserving its exact
five stores and 243/261/279/297-cycle timelines; event/IRQ scopes and the
capped-charge coordinate path remain assembly-backed.
Semantic `02:533B@all` now replaces 11 horizon-safe charge-retraction calls
across the Shooter catch and launch-controller callers, preserving its exact
four-/five-store 234/273/291/309/327-cycle transcripts; the remaining
event/IRQ horizons remain assembly-backed.

Semantic `02:54B9@all` now covers all 1,631 left Skeleton Insect trigger
checks. It preserves the exact two-boundary, 21-cycle clear-trigger return
for 1,630 calls, then composes `00:F55C` and `01:C5C9` through the full
978-cycle, 23-write trigger. The routed path submits the 4,000-point award,
sets the four-tick opening timer, and installs dispatch `02:54D0`. All nine
calls containing original interrupt handlers and the complete
1,631-call/150-write artifacts remain byte-identical in Debug and Release.

Semantic `02:5551@all` now covers all 336 right Skeleton Insect trigger
checks. It preserves the exact two-boundary, 21-cycle clear-trigger return
for 335 calls, then composes `00:F55C` and `01:C5C9` through the full
978-cycle, 23-write trigger. The routed path submits the 4,000-point award,
sets the four-tick opening timer, and installs dispatch `02:5568`. All six
calls containing original interrupt handlers and the complete
336-call/101-write artifacts remain byte-identical in Debug and Release.

Semantic `02:55F9@all` now covers all 2,403 center Skeleton-Insect trigger
checks. It derives the Tower latch from `$2CFC` and preserves the exact
accumulator, flags, and two-boundary 21-cycle idle or 27-cycle trigger
branch. The routed trigger resumes the original shared drawing helper, then
semantic `02:5604@all` owns the complete 1,152-cycle, 30-write continuation:
it composes native `00:F55C`, reproduces the immediate `01:C5EA` delay
installer, and composes native `01:C5C9` for the 10,000-point award,
presentation-bit changes, and delayed `02:561A` opening loop toward
`02:5625`. The continuation shadows exactly at frame 3,353. All 23
interrupt-bearing idle calls and the trigger's assembly-owned helper
interrupt remain exact; the complete
2,403-call/391-write artifacts stay byte-identical in Debug and Release.

Semantic `03:63F1@all` now covers the Devil's Coffin's ordinary return,
Shooter-session type refresh, and both routed collision entries. It derives
session bit `$53.3` and indexed collision state `$3800,X`, preserves the
exact accumulator and flags, optionally refreshes type `$83`, starts the
one-tick opening timer, and composes the native `01:C5C9` scheduler to
install `03:640A`. Its five/seven/21/23-boundary,
zero/one/eight/nine-write, 51/66/270/285-cycle transcripts replace all 643
route calls, including all 13 calls containing original interrupt handlers;
the complete 643-call/202-write artifacts remain byte-identical in Debug and
Release.

Semantic `03:640A@all` now covers the Devil's Coffin opening pass's
timer ticks, timer-expiry advances, and final coroutine handoff. It derives
the indexed timer, appearance, and collision marker, then composes the
already-proven `01:C5C9` scheduler to install dispatch `03:6432`. Exact
two/eleven/28-checkpoint transcripts preserve one/four/12 writes and
33/132/366-cycle timing. All 18 routed opening calls execute natively, while
the complete 18-call/52-write artifacts remain byte-identical in Debug and
Release.

Semantic `03:6445@all` now covers the complete Devil's Coffin closing pass.
In addition to the timer and nonterminal appearance paths, its final closure
clears `$53.3`, restores actor type `$82`, runs initializer `03:63E1`, and
composes the already-proven `01:C5C9` scheduler to reinstall steady dispatch
`03:63F1`. Exact two/nine/34-checkpoint transcripts preserve
one/four/15 writes and 33/111/429-cycle timing. All 18 routed closing calls
execute natively, while the complete 18-call/58-write artifacts remain
byte-identical in Debug and Release.

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

Semantic `01:CC1A@all` now covers the central collision scan's four early
vertical-separation rejections and composes semantic `CCD5` through the
post-geometry horizontal, negative-vertical, positive-page, and same-page
limit rejections. A pure classifier derives each bounds result from the
staged child output; the host carries all child ROM-resolved writes and
interrupt checkpoints through the parent JSR/RTS boundary, then reproduces
the exact `$2D/$2F` subtractions, comparisons, flags, and return. The
in-range continuation now derives the sub-cell coordinates and side flag,
then composes semantic `E569` and `CC98` through their cross-bank return.
All 85,715 calls execute natively.
The complete 85,715-call/1,784,119-write artifacts remain byte-identical in
Debug and Release, with call SHA-256
`c01321d866af5636ce12db1174a9a3428d08ce273cf62820fcbf446dcab1a313`
and write SHA-256
`a0366616f0bbd85b22d5a4a72e8d03e09a8866525c4c90d50c51e2ea7d035881`.

Semantic `01:CCD5@all` now covers the near-target collision-geometry leaf
used by `CC1A`. It resolves both ROM pointer
directories through the live MPR map, selects the phase descriptor, and
reproduces the signed offset, bounds, scratch pointers, stack write, flags,
and all four 783/789/885/891-cycle shapes. Its path-sensitive instruction
transcript replaces all 66,523 calls, including the previously deferred
interrupt-bearing scopes; all 66,523 calls and 1,414,954 writes remain
byte-identical in Debug and Release.

Semantic `00:E569@all` now covers the unsigned-byte scaler used by all 874
remaining in-range `CC1A` mask paths. A pure eight-round model records each
admitted multiplier bit and high-byte carry; the host reproduces the saved-X
stack transaction, shifts, conditional additions/increments, flags, and
`471 + 24 * add-count + 12 * carry-count` base timing. All 874 calls and
10,729 ordered writes remain byte-identical, including original interrupt
passages. Interrupt-free ordinal 2 shadows restored assembly at frame 1,072
with exact 519-cycle and 10-write parity.

Semantic `01:CC98@all` now covers the final pointer and collision-mask tail
entered by all 874 in-range `CC1A` paths. A pure model resolves the two
pointer-table indices, eight-byte mask row, and column bit; the host
reproduces all shifts, 16-bit pointer addition, flags, writes, and the
carry-set contact or carry-clear no-contact return. All 874 calls and 9,617
ordered writes remain byte-identical in Debug and Release. Interrupt-free
ordinal 2 shadows restored assembly at frame 1,072 with exact 378-cycle and
nine-write parity.

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
| 3. Cabinet preview | In progress | Three-window cabinet mode shows the stitched playfield, score DMD, native animated-logo backglass, assigned displays, and playfield-hosted bonus stages |
| 4. Native gameplay port | In progress | Physics, collisions, RNG, scoring, table rules, bonus stages, and session transitions execute natively |
| 5. Final cabinet world model | Not started | Desktop and cabinet profiles share the same native gameplay state without divergence |
| 6. Release hardening | Not started | Tested, documented, ROM-free Windows x64 package is ready for release |

Status values used in this file are `Not started`, `In progress`, `Blocked`, and `Complete`.

## Completed

- [x] Extend the bounded Knight movement parent through its timer-expiry and
  phase-wrap branches: `03:73A6@2` preserves the collision-free
  timer-decrement/no-hit tick, `03:73A6@4` its timer-reload/phase-advance/no-hit
  sibling, and `03:73A6@16` its phase-wrap/no-hit sibling. The native
  `CMP #4` and downstream no-hit `CLC` now retain their carry/zero behavior;
  all three dedicated Release regressions retain byte-identical census, call,
  and ordered-write artifacts.
- [x] Add the first checkpointed Knight-movement interrupt fixture:
  `03:73A6@58` preserves its 1,053-cycle, 22-write movement call and the
  handler's `73D8` stacked return boundary. Its dedicated Release regression
  retains byte-identical census, call, and ordered-write artifacts.
- [x] Compose the Knight collision-helper return into the movement parent:
  `03:73A6@60` now preserves the child `03:6988` RTS, parent `03:73AF`
  carry branch, and their 1,233-cycle, 27-write timer-interrupt passage.
  Its dedicated Release regression retains byte-identical census, call, and
  ordered-write artifacts.
- [x] Promote the collision-free Knight movement parent to semantic
  `03:73A6@all`: all 64 compatible calls on the controlled 129-call route
  now run natively with byte-identical aggregate census, call, and ordered-
  write artifacts. Collision/multi-pass and hit calls remain conservative
  assembly fallbacks.
- [x] Extend the semantic Knight hit gate through its terminal prefix:
  a forced fourth hit at `03:73D8@1848` now preserves the exact 75-cycle
  latch-clear/counter-zero/carry-set return at `03:73E8`. The movement parent
  deliberately retains the carry-set destruction continuation in assembly.
  Its `03:73FE@all` successor now natively prepares the 100-unit award and
  takes the exact 51-cycle banked `01:C02A` score handoff. Following the
  assembly-owned score/release helper, `03:740A@all` additionally owns the
  exact 51-cycle queue-channel acknowledgement prefix and resumes original
  `03:7415` for the common helper. Its `02:41A0@all` entry now owns the exact
  48-cycle four-store scratch-clear prefix and resumes original `02:41A8`;
  its `02:41A8@all` successor then owns the exact 21-cycle command setup;
  `02:41AD@all` then owns the exact 123-cycle saved-index/table-coordinate
  setup and resumes original `02:41C2`; its `02:41C2@all` successor then owns
  the exact 21-cycle `JSR $41F5` stack frame and resumes the shared child.
  Its terminal-only `02:41F5@all` entry then owns the exact 48-cycle pointer
  copy through `$2D:$2E`, resuming at original `02:420B`. Its terminal-only
  successor owns the exact 21-cycle `JSR $C024` stack frame. Its terminal-only
  `01:C024@all` successor then owns the exact 12-cycle jump to `$C5AB`.
  Its terminal-only `01:C5AB@all` successor then owns the exact 6-cycle scan
  initializer, and its terminal-only `01:C5AD@all` successor then owns the
  exact 15-cycle slot-four occupancy read and resumes at the original branch.
  Its terminal-only `01:C5B0@all` successor then owns the exact 6-cycle
  nonzero not-taken branch and resumes at the original scan increment. Its
  terminal-only `01:C5B2@all` successor then owns the exact 6-cycle increment
  and N/Z update, resuming at the original range comparison. Its
  terminal-only `01:C5B3@all` successor then owns the exact 6-cycle limit
  comparison, resuming at the original loop branch. Its terminal-only
  `01:C5B5@all` successor then owns the exact 12-cycle taken loop branch and
  resumes at the shared scan read. Its terminal-only `01:C5B9@all` successor
  then owns the exact 6-cycle `$80` reservation-byte load after the shared
  scan finds its first free slot, resuming at the original indexed store. The
  terminal-only `01:C5BB@all` successor then owns the exact 15-cycle indexed
  reservation store and resumes at the original actor setup. The later actor
  setup's terminal-only `01:C5BE@all` successor then owns the exact 12-cycle
  cleared `$55` mode-byte load and resumes at the original branch. The branch
  path's terminal-only `01:C5C0@all` successor then owns the exact 12-cycle
  taken cleared-mode branch and resumes at the original carry-clear return.
  Its terminal-only `01:C5C7@all` successor then owns the exact 6-cycle
  carry clear and resumes at the shared return. That return and the shared
  worker's later actor/coroutine body remain assembly-backed. After the
  terminal return, `02:420E@all` owns the exact 6-cycle nonnegative `BMI`
  fall-through and resumes at the original actor setup. Its terminal-only
  `02:4210@all` successor then owns the exact 12-cycle `$2D` pointer-low-byte
  load and resumes at the original indexed store. Its terminal-only
  `02:4212@all` successor then owns the exact 15-cycle indexed pointer-low-byte
  store and resumes at the original actor setup. Its terminal-only
  `02:4215@all` successor then owns the exact 12-cycle `$2E` pointer-high-byte
  load and resumes at the original indexed store. Its terminal-only
  `02:4217@all` successor then owns the exact 15-cycle indexed pointer-high-byte
  store and resumes at the original actor setup. Its terminal-only
  `02:421A@all` successor then owns the exact 12-cycle `$26` scratch clear and
  resumes at the original position-field setup. Its `02:421C@all` successor
  then owns the exact 12-cycle `$2B` mode load and resumes at the original
  signed branch. Its `02:421E@all` successor then owns the terminal
  nonnegative 12-cycle signed branch and resumes at the original position
  load. Its `02:4222@all` successor then owns the exact 6-cycle carry clear
  before the terminal position arithmetic. Its `02:4223@all` successor then
  owns the exact 15-cycle indexed base-position load before the original
  mode-byte add. Its `02:4226@all` successor then owns the exact 12-cycle
  mode-byte add before the original actor-field store. Its `02:4228@all`
  successor then owns the exact 15-cycle indexed position store before the
  remaining actor setup. Its `02:422B@all` successor then owns the exact
  15-cycle indexed high-position load before the original scratch-byte add.
  Its `02:422E@all` successor then owns the exact terminal 12-cycle
  cleared-scratch high-byte add before the original indexed store. Its
  `02:4230@all` successor owns that exact 15-cycle indexed high-position
  store. Its terminal-only `02:4233@all` successor then owns the exact
  12-cycle high-position scratch clear. Its terminal-only `02:4235@all`
  successor then owns the exact 12-cycle high-byte mode load. Its terminal-only
  `02:4237@all` successor then owns the exact 12-cycle nonnegative signed
  branch. Its terminal-only `02:423B@all` successor then owns the exact
  6-cycle carry clear. Its terminal-only `02:423C@all` successor then owns
  the exact 15-cycle indexed high-position base load. Its terminal-only
  `02:423F@all` successor then owns the exact 12-cycle high-position scratch
  add. Its terminal-only `02:4241@all` successor then owns the exact 15-cycle
  indexed high-position actor-field store. Its terminal-only `02:4244@all`
  successor then owns the exact 15-cycle indexed companion actor-field load.
  Its terminal-only `02:4247@all` successor then owns the exact 12-cycle
  companion-field scratch add. Its terminal-only `02:4249@all` successor then
  owns the exact 15-cycle indexed companion actor-field store. Its terminal-only
  `02:424C@all` successor then owns the exact 6-cycle companion actor-field
  accumulator clear. Its terminal-only `02:424D@all` successor then owns the
  exact 15-cycle indexed companion actor-field zero store before the remaining
  setup. Its terminal-only `02:4250@all` successor then owns the exact
  15-cycle indexed second companion actor-field zero store. Its terminal-only
  `02:4253@all` successor then owns the exact 15-cycle indexed actor-attribute
  load. Its terminal-only `02:4256@all` successor then owns the exact 6-cycle
  actor-attribute high-nibble mask. Its terminal-only `02:4258@all` successor
  then owns the exact 6-cycle actor-attribute mode-bit merge before the
  remaining setup. Its terminal-only `02:425A@all` successor then owns the
  exact 15-cycle indexed actor-attribute store. Its terminal-only `02:425D@all`
  successor then owns the exact 6-cycle actor-field constant load before the
  remaining setup. Its terminal-only `02:425F@all` successor then owns the
  exact 15-cycle indexed actor-field constant store. Its terminal-only
  `02:4262@all` successor then owns the exact 6-cycle second actor-field
  constant load before the remaining setup.
  `03:7418@all` then owns the exact 36-cycle
  surface-release prefix and its timer-IRQ handoff to native `03:7420@all`.
  That successor composes its exact 33-cycle banked-call wrapper with the
  existing 159-cycle coroutine scheduler, preserving the terminal 192-cycle,
  seven-write dispatch. Score accumulation and post-dispatch continuation stay
  assembly-backed. Release regressions
  `dc_rom_knight_terminal_prefix`, `dc_rom_knight_terminal_score_prefix`,
  `dc_rom_knight_terminal_acknowledgement_prefix`, and
  `dc_rom_knight_terminal_release_helper_prefix`,
  `dc_rom_knight_terminal_release_helper_setup_prefix`, and
  `dc_rom_knight_terminal_release_helper_table_setup_prefix`,
  `dc_rom_knight_terminal_release_helper_call_prefix`, and
  `dc_rom_knight_terminal_release_helper_pointer_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_call_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_jump_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_scan_initializer_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_scan_read_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_scan_nonzero_branch_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_scan_increment_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_scan_range_compare_prefix`,
  `dc_rom_knight_terminal_release_helper_shared_scan_loop_branch_prefix`,
  `dc_rom_knight_terminal_release_helper_allocator_load_prefix`,
  `dc_rom_knight_terminal_release_helper_allocator_store_prefix`,
  `dc_rom_knight_terminal_release_helper_allocator_mode_load_prefix`,
  `dc_rom_knight_terminal_release_helper_allocator_mode_branch_prefix`,
  `dc_rom_knight_terminal_release_helper_allocator_return_carry_clear_prefix`,
  `dc_rom_knight_terminal_release_helper_post_return_mode_branch_prefix`,
  `dc_rom_knight_terminal_release_helper_post_return_pointer_low_load_prefix`,
  `dc_rom_knight_terminal_release_helper_post_return_pointer_low_store_prefix`,
  `dc_rom_knight_terminal_release_helper_post_return_pointer_high_load_prefix`,
  `dc_rom_knight_terminal_release_helper_post_return_pointer_high_store_prefix`,
  `dc_rom_knight_terminal_release_helper_post_return_scratch_clear_prefix`,
  `dc_rom_knight_terminal_scheduler`, and
  `dc_rom_knight_terminal_release_prefix` prove byte-identical census, call,
  and ordered-write SHA-256 artifacts
  `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `a4ac7d7692fca47e030823d339961c2952ed83097b72d2203c97225726774aa6`, and
  `21189884146f10cfe9567127daac6ab96602c2cb1bf14a96874ccf1713abb0b5` for
  the hit gate, and `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `2a461b3c1ba2fcc3cfae51c84bd1b2415c13a8f63aa730c87f617aac74dd52b9`, and
  `4e62b85c9cab18feb840a61b64220579c7319a201a71d1f26f1c230b467a6d3c` for
  the score handoff, and `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `aecb7ba43fb1b687c5dd97fddb479e65625700ea9769ec97740dc7cffcd43483`, and
  `6019ada362a26dc590e99929eaecbb360cad545ae72f401dc21c63285aae8688` for
  the acknowledgement prefix, and
  `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `0461b3880057e3389bfa2bccf6a830468626af05e64d17487d17e29729662ee2`, and
  `d70def79e1b9a9a254ecca17951a0e1eec328f2e4f997cb11bdf2ca29249bcd0` for
  the release-helper prefix, and
  `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `61f9e088ffef26b31b9217546b61d63c043e8064592791edb2bfca007fdc2dc6`, and
  `2ae2b3609f6ccec3d25fdfc902949853e91986fcb2d7379d69552b46d83d5346` for
  the release-helper setup prefix, and
  `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `7dbd3b550fe6b13ab5fc207eeced1ac08f4e8692a482fab95c489e099e67519b`, and
  `5141d9cc48662da37968c0aa785ca34fba6902ea52fad52fd354becd975a6eb7` for
  the terminal scheduler, and
  `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `90f7abb310002e88b4822a68fb324de747d14c3e3b3278ace125bfa1b870b234`, and
  `dc120b5ea8e1f7377d44b9d57b76fc7def58d294a224808fc53ad4691f0a873b` for
  the release prefix. The pointer-copy prefix additionally preserves the
  terminal route's census/call/write SHA-256 values
  `5bee19b564a4e54444f444fdaf156fcec1385fbca952abf8b531b910eeb75243`,
  `9584d8d808a3896f382524de2a68fab5dc4ccbce0978f5a8a870594876b99a9e`, and
  `83b7539cc3d10b658572c721f14484a1a490b87e1e4089d849f6118c9187d676`.
  The following shared-call frame preserves the same census and its call/write
  SHA-256 values `fd88f68464e3c2e4edcd260514156502433a90aa09164b66700e1d7f79362d7d`
  and `347f7b6b0aea1af064dc08d81b2df665df820c8737e6902d2a1f9b1289ce3f91`.
  The shared-entry jump preserves the same census and its call/write SHA-256
  values `09f30c6f356257a699ed0722cee40a1d3d7cb51a2c161b8f37b03e046df16935`
  and `a3ae6fedc5e16e5994e008038d5b614c9f2810db5261f5d0df6764ca1f06cf5d`.
  The first shared scan read preserves the same census and its call/write
  SHA-256 values `be670143b988e155b53e512c71dd21aea33df170c93bd1b2c1509766fc247ffa`
  and `8d3e38d3fd311eb367de6991007ccd88e97ae7dd147a2409025a1d4e4f5f6d52`.
  The first nonzero shared scan branch preserves the same census and its
  call/write SHA-256 values `5d70ba2ccf962ecad19da18542c993fd4ed2d093a99571b87912c0ea59268cea`
  and `3a8cde3d85b7cc50689e4dc852278fe5a2134b14136638f422055cbe9aa0a950`.
  The first shared scan increment preserves the same census and its call/write
  SHA-256 values `67a520562e7d42444b3e21954dc663a6a0aab3c0c8fafbbe762759a4bcd6c63a`
  and `23f9aaaca91dbdb601ef32fe348ff79c7d51187ff28d5c1f5a66a983714a58f0`.
  The first shared scan range comparison preserves the same census and its
  call/write SHA-256 values `940976e7e02e216d0680bda4df6e5115f38b0d1230eda7ebae0ccaddf1757a87`
  and `97434d0e6594e5671b0dabfd19a5c07e7426fa320048cc7660f9884e4bcab836`.
  The first shared scan loop branch preserves the same census and its
  call/write SHA-256 values `1167163402dbd8d5377bebb59589b675c9971ad82dd76d62719130df0cee56be`
  and `f68e62f638a06924a5ee2707a94006a7476999e710733b69a63b10d8fb3f8d69`.
- [x] Replaced the backglass's static presentation with a native animated
  Devil's Crush title logo and restored the DMD to score-only presentation:
  - Mapped and decoded the five original compressed ROM graphic streams,
    two title palettes, eye palette, and four eye-sprite descriptors.
  - Reproduced the settled 64-frame palette/eye cycle without reading or
    cropping the emulated framebuffer and without a live VRAM/SAT dependency.
  - Presented the animation continuously on the backglass, retained the
    configured static image as a failure fallback, kept the DMD on its native
    score path throughout, and retained the PCE pixel aspect through the fixed
    sharp scaler.
  - Extended the native crop through the complete `Crush` lettering and
    tightened its right edge before the title screen's trademark glyphs.
  - Changed the cabinet playfield's unused surround from RGB `8,8,12` to
    explicit RGB `0,0,0` for true-black OLED letterboxing.
  - Added exact ROM-backed frame goldens and passed core, title-logo, and
    dummy-driver three-window cabinet smoke coverage.
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
    source slot `$2C` at `(140,526)` uses `BSR 02:5F78` to create the four
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
  - Replaced all 1,748 collision-free `02:4C91@all` calls with exact
    66-cycle timer wait, 135-cycle appearance advance, and 144-cycle
    appearance reset transcripts. The native adapter preserves all timer and
    appearance writes and defers collision activation to assembly.
  - Verified byte-identical entry, call, write, and selected-state SHA-256
    values
    `fb473f9da4c43e0eea2abcfed06c5510b3d2c6a457bcbd6e791c5b91be6cb057`,
    `48a1a89f272e80679e859d6ecdc34d4102d864985fba5ba64bdde12d0642e64b`,
    `1d70d3313ad65639066692b2721f1df8d5ffc4d041e5625a3f84fceaf3dcd4fa`,
    and
    `6692ff8cb8ddb9927b57e2844dea70ea8c11afc48a3ac18707350690b83c5d26`.
  - Passed ROM-free core tests and the existing owning-family Release
    regression, then the required Debug family checkpoint; the established
    entry, call, write, and selected-state artifacts remain byte-identical.
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
- [x] Composed the post-geometry collision-scan bounds:
  - Added a pure classifier for the horizontal, negative-vertical,
    multi-page, same-page-limit, and in-range outcomes after `CCD5`.
  - Composed the full semantic `CCD5` write/checkpoint transcript through
    `CC1A`'s horizontal, negative-vertical, positive-page, and same-page
    limit rejections, including exact parent stack, subtractions,
    comparisons, flags, and timing.
  - Raised `CC1A@all` admission from 19,192 to 84,841 of 85,715 calls
    (99.0%) while preserving all state, call, and 1,784,119-write hashes.
  - Shadowed route ordinal 5 against restored assembly at frame 1,054 with
    exact 1,005-cycle and 22-write parity.
- [x] Ported unsigned-byte scaler `00:E569@all`:
  - Added a pure eight-round shift/add model with per-round admission and
    high-byte-carry state.
  - Replaced all 874 calls through path-generated checkpoints while
    preserving every original interrupt passage and all 10,729 writes.
  - Shadowed interrupt-free ordinal 2 at frame 1,072 with exact 519-cycle,
    10-write parity.
- [x] Ported final collision-mask lookup `01:CC98@all`:
  - Added a pure live-pointer/index/mask model and exact instruction
    transcript for both contact outcomes.
  - Replaced all 874 calls while preserving every original interrupt passage
    and all 9,617 ordered writes.
  - Shadowed interrupt-free ordinal 2 at frame 1,072 with exact 378-cycle,
    nine-write parity.
- [x] Composed the final in-range collision-scan path:
  - Added the pure side-flag, sub-cell, rounded-span, and multiply-input
    model for `CC41-CC97`.
  - Composed `CCD5`, `E569`, and `CC98` through the parent, including the
    physical-bank identity of the cross-bank multiply return checkpoint.
  - Raised `CC1A@all` admission from 84,841 to all 85,715 calls while
    preserving the complete 1,784,119-write artifact.
  - Shadowed in-range ordinal 19,176 at frame 1,553 with exact 2,277-cycle,
    52-write contact parity.
- [x] Ported the Dragon's-Egg active-count controller `02:42F6@all`:
  - Added a pure C classifier for the nonzero active return and deferred
    count-zero completion sequence.
  - Replaced all 1,790 route calls through a two-checkpoint, 21-cycle,
    zero-write transcript while preserving 17 interrupt passages and all 219
    handler-owned writes.
  - Shadowed ordinal 1 at frame 873 with exact 21-cycle, zero-write parity.
- [x] Completed the routed Dragon's-Egg hit path with semantic
  `02:4345@all`, `02:4357@all`, and `02:4371@all`:
  - Retained the pure C classifier for indexed collision state and replaced
    all 9,857 gate calls through exact 21-cycle idle or 57-cycle score-setup
    transcripts.
  - Replaced the one routed disable/impact setup in 93 cycles and four
    writes, then the post-allocation activation in 21 cycles and one write.
  - Kept shared score/audio, burst allocation, hatchling allocation, and
    coroutine scheduling in assembly while preserving all 1,682 aggregate
    writes and the complete outer call hash.
  - Shadowed ordinal 1 at frame 873 with exact 21-cycle, zero-write parity
    and post-allocation ordinal 1 at frame 2,425 with exact outer parity.
- [x] Ported the Giant Skeleton's Eyeball initializer and dormant gate
  `02:4DC8/4DD9@all`:
  - Added a typed fixed-slot initializer classifier and replaced the sole
    frame-872 setup through its exact 258-cycle, ten-write transcript and
    final shared-script interrupt boundary.
  - Added a pure C classifier for the Dragon's-Egg completion latch,
    final-animation session flag, and dormant return.
  - Replaced all 1,988 route calls through a six-checkpoint, 57-cycle,
    zero-write transcript while preserving 37 original interrupt passages
    and all 493 handler-owned writes.
  - Shadowed ordinal 1 at frame 873 with exact 57-cycle, zero-write parity.
- [x] Ported the Mercy Stars' not-last-ball gate `02:4FC1@all`:
  - Added a pure C classifier for normal-table mode, remaining-ball state,
    selected-player stored score, score-limit return, and activation.
  - Replaced all 7,950 route calls through a five-checkpoint, 54-cycle,
    zero-write transcript while preserving 163 original interrupt passages
    and all 2,173 handler-owned writes.
  - Shadowed ordinal 1 at frame 873 with exact 54-cycle, zero-write parity.
- [x] Ported the Three-Blobs mismatch controller `03:65C2@all`:
  - Added a pure C classifier for first-pair mismatch, third-value mismatch,
    and the deferred equal-state outcome.
  - Replaced all 2,650 route calls through three/five-checkpoint,
    42/63-cycle, zero-write transcripts while preserving 50 original
    interrupt passages and all 665 handler-owned writes.
  - Shadowed ordinal 1 at frame 873 with exact 42-cycle, zero-write parity.
- [x] Ported the Giant Door's idle hit gate `03:62B8@all`:
  - Added a pure C classifier for indexed collision state, idle return, and
    the deferred triggered-hit path.
  - Replaced all 676 route calls through a two-checkpoint, 27-cycle,
    zero-write transcript while preserving seven original interrupt
    passages and all 88 handler-owned writes.
  - Shadowed ordinal 1 at frame 1,228 with exact 27-cycle, zero-write parity.
- [x] Ported the Giant Face hidden-form animator's idle gate `03:767E@all`:
  - Added a pure C classifier for the shared animation mode and the
    mode-one active path.
  - Replaced all 873 route calls through a three-checkpoint, 27-cycle,
    zero-write transcript while preserving eight original interrupt
    passages and all 97 handler-owned writes.
  - Shadowed ordinal 1 at frame 1,222 and preserved the complete
    5,608-call/812-write Giant Face family artifacts in Debug and Release.
  - Left mode one's active timer, phase, and drawing path in assembly.
- [x] Ported the Giant Face controller's idle request gate `03:74D8@all`:
  - Added a pure C classifier for shared request bit zero and the deferred
    wake-request path.
  - Replaced all 2,648 route calls through a three-checkpoint, 27-cycle,
    zero-write transcript while preserving 23 original interrupt passages
    and all 289 handler-owned writes.
  - Shadowed ordinal 1 at frame 942 and preserved the complete
    5,608-call/812-write Giant Face family artifacts in Debug and Release.
  - Left wake and transformation request handling in assembly.
- [x] Ported the Giant Face crown's no-hit collision gate `03:76FA@all`:
  - Added a pure C classifier for the indexed collision byte and deferred
    crown-hit path.
  - Replaced all 867 route calls through a two-checkpoint, 27-cycle,
    zero-write transcript while preserving seven original interrupt passages
    and all 86 handler-owned writes.
  - Shadowed ordinal 1 at frame 1,223 and preserved the complete
    5,608-call/812-write Giant Face family artifacts in Debug and Release.
  - Left the 300-point score, impact, and audio hit path in assembly.
- [x] Ported the Giant Face mouth's closed/no-hit gate `03:7726@all`:
  - Added a pure C classifier for mouth state, indexed collision state, and
    the deferred open-mouth and hit paths.
  - Replaced all 1,214 route calls through a four-checkpoint, 48-cycle,
    zero-write transcript while preserving 22 original interrupt passages
    and all 288 handler-owned writes.
  - Shadowed ordinal 1 at frame 1,213 and preserved the complete
    5,608-call/812-write Giant Face family artifacts in Debug and Release.
  - Left open-mouth capture and the 300-point score, impact, and audio hit
    paths in assembly.
- [x] Ported the hidden Big-Star feature idle gate `02:445B@all`:
  - Added a pure C classifier for the shared Little-Skull latch, idle
    return, and deferred pending-feature path.
  - Replaced all 2,650 route calls through a two-checkpoint, 21-cycle,
    zero-write transcript while preserving 28 original interrupt passages
    and all 364 handler-owned writes.
  - Shadowed ordinal 1 at frame 873 with exact 21-cycle, zero-write parity.
- [x] Ported the Big-Star active pending gate `02:570A@all`:
  - Added a pure C classifier for `$2D04`'s two pending direction bits;
    unrelated high bits retain the original clear-and-return behavior.
  - Replaced 489 of 490 controlled active-window calls through seven
    checkpoints and the 81-cycle `$2D04` clear. Its sole pending-left child
    `02:5722@all` now also replaces the fixed-slot phase-zero through phase-six
    velocity/cadence helper: 240 cycles for phase advances and 252 cycles for
    lower-phase reset or phase-six saturation; all other helper phases remain
    assembly fallback.
  - The lower-reset Release artifacts are byte-identical; its required Debug
    checkpoint has census
    `84638ea265c2f353cf112b9927bc95b5ece558c3e411336a57dcd7b84b839bf7`
    with matching entry/call/write artifacts.
    A separate controlled `$2D18 == 4` Release regression covers the
    latch-clear tail opposite its normal increment-and-latch exit.
  - The independently controlled phase-four Release artifacts are
    byte-identical; the required Debug checkpoint has census
    `700587830ac9cc82e1f19ac5ee54506410241cc87ef1c42c4e92ea629807b5a8`
    with matching entry/call/write artifacts.
  - Independently controlled phase-five and phase-six Release regressions
    freeze the 240-cycle advance and 252-cycle saturation transcripts.
    The phase-six Debug census is
    `a31bfb402382bea7a77e13c7bac7c66164162d2887589ede746b217a5ecda6e7`;
    its entry/call/write artifacts match Release and assembly.
  - Shadowed ordinal 2 at frame 1,222 with exact 81-cycle, one-write parity
    and the child at frame 1,221 with 240-cycle, six-write parity; froze the
    focused Release census/call artifacts.
- [x] Ported the Devil-Shooter side-companion dormant leaves
  `02:53DD/5456@all`:
  - Added a pure C classifier for the two fixed companion slots and session
    bit `$53.3`; active-loop states remain assembly fallback.
  - Replaced 929 of 1,055 left and 926 of 1,085 right calls through the
    exact 24-cycle zero-write return. The 285 calls within a user-event or
    serviceable timer-IRQ horizon remain assembly-backed.
  - Shadowed ordinal 10 for each leaf at frames 2,145/2,141 and froze the
    byte-identical focused Release census, call, and ordered-write artifacts.
- [x] Ported the Devil-Shooter charge-position leaf `02:5397@all`:
  - Added a pure C classifier for fixed slot `$0B`, charge counter `$2D15`,
    and its carry-aware 16-bit vertical-coordinate result.
  - Replaced 873 of 1,260 calls with the exact 75-cycle, two-write
    transcript. The remaining 387 calls near a user-event or serviceable
    timer-IRQ horizon remain assembly-backed.
  - Extended the owning Release companion regression; it shadows ordinal 10
    at frame 949 and preserves byte-identical census, call, and ordered-write
    artifacts.
- [x] Ported the Devil-Shooter charge-appearance leaf `02:5408@all`:
  - Added a pure C classifier for fixed slot `$0C`, shared charge counter
    `$2D15`, cached charge `$3E0C`, and its bounded three-frame appearance.
  - Replaced 1,015 of 1,288 calls through exact 42-cycle cached, 105-cycle
    advance, and 114-cycle reset transcripts. The 273 calls at a user-event
    or serviceable timer-IRQ horizon, including all 22 interrupt-bearing
    variants, remain assembly-backed.
  - Extended the same Release companion regression with an ordinal-10
    frame-949 42-cycle zero-write replay; census, call, and ordered-write
    artifacts remain byte-identical.
- [x] Ported the Devil-Shooter right-companion active timer leaf `02:545D@all`:
  - Added a pure C classifier for fixed slot `$0D`, indexed timer `$3B0D`,
    and its five-frame appearance `$398D`.
  - Replaced 219 of 267 calls through exact 33-cycle timer wait, 102-cycle
    reload/advance, and 111-cycle reload/advance/reset transcripts. The 48
    user-event or timer-IRQ horizon calls, including both observed
    interrupt-bearing variants, remain assembly-backed.
  - Extended the same Release companion regression with an ordinal-10
    frame-952 33-cycle one-write replay and refreshed its focused call/write
    artifacts while retaining the byte-identical state census.
- [x] Ported the Devil-Shooter left-companion active timer leaf `02:53E4@all`:
  - Reclassified the shared code entry to admit only fixed left slot `$0A`;
    right/other contexts and all user- or timer-IRQ horizons remain assembly
    fallback.
  - Preserved the left four-frame appearance limit and exact 33/102/111-cycle
    wait/advance/reset paths, including stores at `53E4/53EB/53EE/53F8`.
  - Extended the owning Release companion regression: 214 of 272 calls now
    execute natively and replay `02:53E4@3` has exact frame-941 33-cycle,
    one-write parity while the state census remains byte-identical.
- [x] Ported the Devil-Shooter left active controller `02:53C8@all`:
  - Composed the fixed-slot `$0A` timer child with its authentic JSR/RTS and
    nonzero `$53 & $09` tail, preserving exact 99/168/177-cycle
    wait/advance/reset transcripts and the `$01FB/$01FA = $53/$CA` stack
    writes.
  - Requires a full no-event/no-timer-IRQ window, leaving transfer, alternate,
    and horizon-bound controller paths in assembly.
  - The Release companion regression now replaces 180 of 266 controller calls
    plus 34 of the 92 remaining direct timer calls; replay `02:53C8@3` pins
    the frame-945 99-cycle, three-write path with byte-identical census.
- [x] Ported the mirrored Devil-Shooter right active controller `02:5441@all`:
  - Generalized the composed controller adapter for fixed slot `$0D`, its
    five-frame timer child, `02:544A` return, and `$01FB/$01FA = $54/$43`
    authentic JSR stack writes.
  - Replaces 174 of 267 bounded controller calls, while its timer leaf covers
    45 of the 93 remaining direct calls; replay `02:5441@3` pins the
    frame-945 99-cycle, three-write transcript without changing the census.
- [x] Ported the Big-Star rotating wait path `02:5674@all`:
  - Added a pure C classifier for fixed slot `$4B`, pending-direction latch
    `$2D04`, signed angular velocity `$2D00:$2D01`, timer `$3A4B`, and
    session flags `$20CB`.
  - Replaced 199 of 490 active-window controller calls through the exact
    222-cycle, five-write nested-pending-gate/timer-decrement path. The
    classifier keeps stopped, pending-direction, timer-expiry, transfer-mode,
    and event/IRQ-horizon calls in assembly.
  - Extended the Release Big-Star family regression with an ordinal-4
    frame-1224 replay and refreshed census, call, and ordered-write artifacts.
- [x] Ported the Walled Star's idle hit gate `03:6339@all`:
  - Added a pure C classifier for indexed collision state, shared access
    latch, idle return, and the two deferred active paths.
  - Replaced all 2,650 route calls through a four-checkpoint, 54-cycle,
    zero-write transcript while preserving 49 original interrupt passages
    and all 659 handler-owned writes.
  - Shadowed ordinal 1 at frame 873 with exact 54-cycle, zero-write parity.
- [x] Completed the fixed Tower hit path with semantic `03:60C3@all`,
  `03:60E0@all`, and `03:611C@all`:
  - Kept the idle two-checkpoint, 27-cycle return and added the routed
    63-cycle collision-clear handoff, covering all 2,646 gate calls.
  - Retained shared score/audio and interrupt-bearing dispatcher helpers in
    assembly while moving Tower progress and transition state into the
    156-cycle `60E0` continuation.
  - Composed the native coroutine scheduler for all four `611C` calls
    through exact 135/213-cycle transcripts; ordinal 1 replays with exact
    135-cycle, five-write parity.
  - Preserved the complete 2,646-call/538-write outer artifact and froze the
    five-call continuation artifact with call SHA-256
    `7e282adac47c3b913dc254b24cb364e41a12ac7c216f5d9944bac4a1e38306b8`
    and write SHA-256
    `eb7ffe66312e2c613cb842a6e755f8facba6574218e069a93be809ec2d27ba04`.
- [x] Completed the Dragon's Head hit gate and routed countdown:
  - Added a pure C classifier for indexed collision state, the idle return,
    and the triggered path.
  - Expanded semantic `02:5A4B@all` to all 2,144 route calls through exact
    27-cycle idle or 42-cycle collision-clear transcripts while preserving
    shared score/audio/bonus/appearance helpers in assembly.
  - Added semantic `02:5A6E@all` for both routed nonterminal counter
    decrements. Ordinal 1 shadows frame 2,076 with exact 27-cycle,
    one-write parity; counter one still falls back to the original
    mouth-opening path.
  - Preserved the complete 2,144-call/866-write outer artifact and froze the
    two-call continuation artifact with call SHA-256
    `564b3e42385479f383e4a93b46d568f58242867b4f7f9593810a37ece7fa0b3c`
    and write SHA-256
    `03b84e95c2335cb54cdfab0673164dc6a1816e7b16d12cf7509ba53e81dc2931`.
- [x] Completed both fixed Bumpers' routed hit path:
  - Added a pure C classifier for indexed collision state, the idle return,
    and the triggered path.
  - Expanded `03:78C7@all` to all 3,468 route calls through exact 27-cycle
    idle or 165-cycle, four-write collision-clear/phase-dispatch transcripts.
  - Added `03:78F5@all` for all nine 36-cycle, one-write primary progress
    increments and `03:7912@all` for both 114-cycle, three-write secondary
    damage updates.
  - Preserved shared score/audio/appearance/scheduler helpers and the
    inactive ninth-hit destruction/release branch in assembly.
  - Preserved the complete 3,468-call/3,127-write outer artifacts and froze
    the 11-call/953-write continuation artifacts at call SHA-256
    `1225992b04220cdea996a4a1a11e3f67be24ef5dd5a42e0ae1d6039ded66b5ce`
    and write SHA-256
    `99674df0a29df6a45f2eb4e32c8d3f8dd0fe423fb30b699c6828825af7cb7b29`.
    The second `03:7912` call shadows frame 3,342 with exact 114-cycle,
    three-write parity.
- [x] Completed the fixed Devil Shooter hit gate and catch continuation:
  - Expanded semantic `02:5116@all` to all 125 route checks through exact
    two-checkpoint, zero-write 27-cycle idle and 21-cycle catch branches.
  - Retained the occupied latch and shared `00:C087` ball-position/audio
    helper in assembly, preserving its original interrupt passage.
  - Added semantic `02:52B8@all` for the exact 228-cycle, nine-write
    catch-state and coroutine continuation through native `01:C5C9`.
  - Shadowed `02:5116@1` and `02:52B8@1`; the latter matches registers,
    cycles, both JSR stacks, write order, and complete RAM at frame 1,064.
- [x] Added the Devil-Shooter released-input delay gate `02:5159@all`:
  - Classifies only fixed slot `$0E` in normal-table mode with Button I clear
    for the active player and a nonnegative collision state.
  - Replaces all four frames 1,181--1,184 returns through the exact
    eight-checkpoint, 96-cycle, zero-write transcript; held charge and launch
    behavior remain assembly-backed.
  - Extended the owning `dc_rom_devil_shooter` regression with normalized
    replay `02:5159@97`; Release and the required Debug family checkpoint
    preserve its existing census, trace, call, and ordered-write artifacts.
- [x] Added the Devil-Shooter charge-position leaf `02:52E3@all`:
  - Classifies fixed Shooter slot `$0E`, carries its three-byte coordinate,
    derives charge `$00`--`$26`, and selects appearance values 0--4.
  - Replaces 137 of 252 calls through five exact stores and the 243-, 261-,
    279-, or 297-cycle normal transcripts; capped-charge and event/IRQ
    horizons remain assembly-backed.
  - Extended `dc_rom_devil_shooter` with normalized replay `02:52E3@1` and
    refreshed the Release entry, call, and ordered-write artifacts; the
    required Debug family checkpoint preserves the matching behavior.
- [x] Added the Devil-Shooter charge-retraction leaf `02:533B@all`:
  - Classifies fixed Shooter slot `$0E`, subtracts two vertical units,
    derives the remaining charge, and preserves the conditional appearance
    write or final zero-charge return.
  - Carries the low-byte charge subtraction borrow into the high-byte clamp
    comparison, keeping clamp states on the original assembly path.
  - Replaces 11 of 32 calls through exact four-/five-store 234-, 273-,
    291-, 309-, or 327-cycle transcripts; all event/IRQ horizons and the
    clamp path remain assembly-backed.
  - Extended `dc_rom_devil_shooter` with normalized replay `02:533B@2` and
    refreshed the Release entry, call, and ordered-write artifacts; the
    required Debug family checkpoint preserves the matching behavior.
- [x] Completed the left Skeleton Insect trigger gate `02:54B9@all`:
  - Retained the 1,630 exact 21-cycle, zero-write clear-latch returns.
  - Composed the native score accumulator and coroutine scheduler for the
    frame-3,466 trigger's 4,000-point award, four-tick opening timer, and
    `02:54D0` dispatch.
  - Shadowed ordinal 1,631 with exact 978-cycle, 23-write parity while
    preserving all nine interrupt passages and the complete 150-write
    artifact.
- [x] Ported the Tower Guard's closed selector `03:60A6@all`:
  - Added a pure C classifier for the live five-step gate counter, closed
    selector `$84`, and deferred open selector `$8E`.
  - Replaced all 2,650 route calls through a four-checkpoint, 48-cycle,
    one-write transcript while preserving 58 original interrupt passages
    and the complete 3,403-write artifact.
  - Shadowed ordinal 1 at frame 873 with exact one-write parity; the
    unexercised zero-counter/open entrance remains in assembly.
- [x] Ported the fixed collision-response dormant gate `03:7CAF@all`:
  - Added a pure C classifier for negative-scene, dormant, and active
    response paths from `$2CC0` and `$CC`.
  - Replaced all 2,650 route calls through a four-checkpoint, 45-cycle,
    zero-write transcript while preserving 54 original interrupt passages
    and the complete 721-write artifact.
  - Shadowed ordinal 1 at frame 873 with exact zero-write parity; negative
    scenes and the nonzero response state machine remain in assembly.
- [x] Ported the complete routed fixed-scheduler path
  `03:7B27/7B4E/7B5D@all`:
  - Added a pure C classifier for inactive scenes, presentation suppression,
    non-expiring timer decrements, and active timer-expiry ticks.
  - Replaced all 2,650 outer calls, including 332 exact 156-cycle timer
    reload/first-submission prefixes and 332 entries apiece at the two
    54-cycle continuation hooks.
  - Preserved the original banked-call wrapper, exported `08:4030` routine,
    stable ordinal-1 replay, state census, and complete 46,618-write artifact;
    only the unobserved negative-scene path remains in assembly.
- [x] Ported the routed fixed-marker animator through all three descriptor
  setups and its stack wrapper with
  `03:7B7A/7BBB/7BCD/7BE0/7BE8/7BEC@all`:
  - Reused the shared pure C classifier for inactive scenes, presentation
    suppression, non-expiring timer decrements, and active timer-expiry
    ticks.
  - Replaced all 2,650 outer calls, including 332 exact 231/240-cycle timer,
    phase-wave, and first-descriptor prefixes plus 332 entries apiece at the
    two exact 36-cycle descriptor continuation hooks.
  - Replaced all 996 executions apiece of the phase-table/stack prologue,
    post-first-helper `PLA`, and post-second-helper `PLY` through exact
    33/12/12-cycle segments.
  - Replaced all 997 routed `02:5955` phase-stride additions, including all
    996 marker-owned calls, through exact 198-cycle, eight-write,
    15-checkpoint transcripts while retaining `01:C05D` drawing submission
    in assembly.
  - Shadowed ordinal 1 at frame 873 with exact 48-cycle, zero-write parity;
    preserved the remaining shared drawing helper, Debug/Release state
    censuses, and the complete 88,480-write artifact.
- [x] Split the native replacement subsystem:
  - Moved the `03:6966`, `03:7FB1`, `03:7F57`, `01:CC1A`, `01:CC98`, and
    `01:CCD5` adapters into `src/app/native_replacements_collision.cpp`.
  - Moved integration, rollback, signed arithmetic, damping, rotation, and
    flipper projections into `native_replacements_physics.cpp`.
  - Moved flipper input/phase/geometry, coroutine, nudge/session input, and
    actor scheduling into `native_replacements_controls.cpp`.
  - Moved normal-table rule, RNG, and score synthesis into
    `native_replacements_rules.cpp`, leaving `native_replacements.cpp` as
    the central registry.
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
- [x] Render the native ROM-derived animated title logo continuously on the
  backglass and retain the score-only DMD throughout.
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
  - [x] Compose semantic `01:CCD5@all` into `01:CC1A@all` through all
    post-geometry bound rejections; cover 84,841 of 85,715 parent calls with
    exact restored-assembly replay and aggregate parity.
  - [x] Add semantic `00:E569@all` unsigned-byte multiplication for all 874
    remaining collision-mask calls with exact stack, shift/add, carry,
    timing, write, and original-interrupt parity.
  - [x] Add semantic `01:CC98@all` final collision-mask lookup for all 874
    in-range paths with exact live-MPR reads, 16-bit pointer construction,
    flags, timing, writes, and original-interrupt parity.
  - [x] Compose semantic `00:E569@all` and `01:CC98@all` through the final
    in-range `01:CC1A@all` path; cover all 85,715 parent calls with exact
    cross-bank checkpoints, restored-assembly replay, and aggregate parity.
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
  - [x] Add semantic `01:DD6E@all` ordinary ball-drain threshold; cover all
    1,744 session-route calls through exact 84-cycle carry-clear and
    78-cycle carry-set transcripts, including the injected frame-3,000 drain.
  - [x] Add semantic `01:C80A@all` ordinary remaining-ball presentation;
    cover its complete 318-cycle, thirteen-write timer-install/decrement
    transcript on the session route.
  - [x] Compose semantic `01:C80A@all` through `01:C7EA@all` for the
    zero-transition-latch ordinary continuation; cover its complete
    345-cycle, thirteen-write parent transcript.
  - [x] Compose semantic `01:C7EA@all` through `01:C7E5@all` for the
    nonzero-ball ordinary decision; cover its complete 366-cycle,
    thirteen-write route. Its zero-ball sibling composes that 27-cycle
    load/taken branch with native `01:C7F4` into the exact 345-cycle,
    thirteen-write zero-ball scheduler transaction, while the alternate
    nonzero-`$2D32` sibling composes its 21-cycle load/fall-through with the
    following `01:C7EF` 228-cycle scheduler setup into the exact 249-cycle,
    nine-write transaction; its nonzero-ball `01:C7E5@all` parent composes
    that alternate branch into the exact 270-cycle, nine-write outer scope.
    `01:C7F4@all` now owns the following 318-cycle,
    thirteen-write zero-ball scheduler setup and resumes at `01:C5E9`; the
    installed `01:C7F9@all` callback additionally owns 254 common
    non-expiring delay ticks through exact 123-cycle, five-write composed
    scopes, while its one `01:C7FC@all` expiry callback owns the exact
    201-cycle, eight-write scheduler-install tail; the following state-machine
    body reaches native `01:C804@all` on its clear latch, composing an exact
    345-cycle, thirteen-write presentation gate; its nonzero-latch return and
    remaining state-machine body remain assembly-backed. Native `01:C812@all`
    now also owns its zero-count 237-cycle, eight-write state-flag/scheduler
    setup while retaining its nonzero reload path in assembly. The
    alternate `01:C7EF@all` tail likewise owns its 228-cycle, nine-write
    latch-clear/scheduler setup and resumes at `01:C5E9`; its later body
    remains in assembly. Its controlled Debug and Release route is byte-identical:
    Debug census SHA-256 is
    `000f85c840f1ae52475746d8e8af723c9509a24b37a28baa4e9c4b895c814b7d`,
    and native call/write SHA-256 values are
    `80feb43474cb780365d2d36701379863e05de79a35c193aa6a61120d2ed16771` and
    `773ab75800ad5b961c131313e5499bf04735c62e0471890cd63f1ad80aada1bf`.
    The alternate scheduler's paired Debug replay has census SHA-256
    `78576500728927e8bd6fd5595743b04ea54f1662e092040191a4efcd9e0e36af`
    and native call/write SHA-256 values
    `0da1d358166e9079b5f156af8797caa34743778f470855502183d9d8802c6e42`
    and `cca4b5ac2294bbfdcdb7e0b57c6265a7be22609273d05c75fcd8886b11c61b1a`.
    The alternate-latch controlled route is byte-identical
    in Debug and Release: Debug census SHA-256 is
    `78576500728927e8bd6fd5595743b04ea54f1662e092040191a4efcd9e0e36af`,
    with the same call and ordered-write hashes as Release.
    The paired Debug audit of the later clear-latch `C804` and zero-count
    `C812` leaves is also byte-identical: its census, call, and ordered-write
    SHA-256 values are
    `baf530269ea4effe1c42aaead83c7f1f18b953e992d890c0dc08a4906a3a851c`,
    `fc7e46a8258045229faea9a87070da05f358a0197b07701c3981f7feec95ad52`,
    and `d4bf75fdea28df80ea85151ef0d4c53e10dc8767ed0dd6009e1b3fe405727de6`.
  - [x] Reprofile the preceding session/ball-count and panel boundaries after
    `01:C7E5@all`; retain `01:C786`, `01:C7B0/C7BF`, and direct-VDC
    `09:72FD` assembly-backed until a separately composable gameplay leaf is
    captured.
  - [x] Add semantic `02:42F6@all` Dragon-Egg controller gating; cover all
    1,790 nonzero-count route calls through the checkpointed 21-cycle return.
  - [x] Complete semantic `02:4345@all` Dragon-Egg hit gating for all 9,857
    route calls, then replace routed disable/impact setup at `02:4357@all`
    and post-allocation activation at `02:4371@all`.
  - [x] Add semantic `02:4DD9@all` Giant-Skeleton-Eyeball gating; cover all
    1,988 dormant route calls through the checkpointed 57-cycle return.
  - [x] Add semantic `02:4FC1@all` Mercy-Star gating; cover all 7,950
    not-last-ball route calls through the checkpointed 54-cycle return.
  - [x] Add semantic `03:65C2@all` Three-Blobs result gating; cover all
    2,650 mismatch route calls through checkpointed 42/63-cycle returns.
  - [x] Add semantic `03:62B8@all` Giant-Door idle-hit gating; cover all 676
    route calls through the checkpointed 27-cycle return.
  - [x] Add semantic `02:445B@all` Big-Star feature idle gating; cover all
    2,650 route calls through the checkpointed 21-cycle return.
  - [x] Add semantic `03:6339@all` Walled-Star idle-hit gating; cover all
    2,650 route calls through the checkpointed 54-cycle return.
  - [x] Complete semantic `03:60C3@all` Tower hit gating for all 2,646
    route calls, then replace Tower progress at `03:60E0@all` and compose
    the scheduler across all four `03:611C@all` transition calls.
  - [x] Complete semantic `02:5A4B@all` Dragon's-Head hit gating for all
    2,144 route calls through checkpointed 27-cycle idle or 42-cycle
    collision-clear paths, then replace both routed nonterminal counter
    decrements at `02:5A6E@all`.
  - [x] Complete semantic `03:78C7@all` Bumper hit gating for all 3,468
    route calls through checkpointed 27-cycle idle or 165-cycle
    collision/phase-dispatch paths, then replace all nine primary progress
    updates at `03:78F5@all` and both secondary updates at `03:7912@all`.
  - [x] Complete semantic `02:5116@all` Devil-Shooter gating for all 125
    route calls through checkpointed 27-cycle idle or 21-cycle catch
    branches, then replace the post-helper `02:52B8` continuation through
    its exact 228-cycle state/scheduler transcript.
  - [x] Add semantic `02:54B9@all` left Skeleton-Insect trigger gating;
    cover all 1,631 route calls through the checkpointed 21-cycle return
    and 978-cycle score/scheduler trigger.
  - [x] Add semantic `02:5551@all` right Skeleton-Insect trigger gating;
    cover all 336 route calls through the checkpointed 21-cycle return and
    978-cycle score/scheduler trigger.
  - [x] Add semantic `02:55F9@all` center Skeleton-Insect trigger gating;
    cover all 2,403 route calls through the checkpointed 21-cycle return or
    27-cycle handoff, then replace the post-drawing `02:5604` continuation
    through its exact 1,152-cycle score/delay/scheduler transcript.
  - [x] Complete semantic `03:63F1@all` Devil's-Coffin return,
    type-refresh, and collision-entry gating; cover all 643 route calls
    through checkpointed 51/66/270/285-cycle paths.
  - [x] Add semantic `03:640A@all` Devil's-Coffin opening timer gating;
    cover all 18 route calls through the checkpointed 33-cycle decrement,
    132-cycle appearance-advance, and 366-cycle scheduler-handoff paths.
  - [x] Add semantic `03:6445@all` Devil's-Coffin closing timer gating;
    cover all 18 route calls through the checkpointed 33-cycle decrement,
    111-cycle nonterminal appearance-decrement, and 429-cycle final
    scheduler-handoff paths.
  - [x] Add semantic `03:60A6@all` Tower-Guard closed-selector updates;
    cover all 2,650 route calls through the checkpointed 48-cycle write.
  - [x] Add semantic `03:7CAF@all` fixed collision-response dormant gating;
    cover all 2,650 route calls through the checkpointed 45-cycle return.
  - [x] Expand semantic `03:7B27@all` with `03:7B4E/7B5D@all`; cover all
    2,650 routed scheduler calls while retaining the three original
    banked-call-wrapper invocations.
  - [x] Expand semantic `03:7B7A@all` with
    `03:7BBB/7BCD/7BE0/7BE8/7BEC@all`; cover all
    2,650 routed fixed-marker calls through timer, phase-wave, and all three
    descriptor setups plus all 996 wrapper executions.
  - [x] Add semantic `02:5955@all`; cover all 997 five-bit phase-stride
    additions, including the 996 marker calls, while retaining the
    `01:C05D` drawing submission.
  - [x] Add semantic `03:767E@all`; cover all 873 routed Giant-Face
    hidden-form animator idle returns while retaining mode one's active
    animation in assembly.
  - [x] Add semantic `03:74D8@all`; cover all 2,648 routed Giant-Face
    controller idle returns while retaining wake and transformation request
    handling in assembly.
  - [x] Add semantic `03:76FA@all`; cover all 867 routed Giant-Face crown
    no-hit returns while retaining the 300-point hit path in assembly.
  - [x] Add semantic `03:7726@all`; cover all 1,214 routed Giant-Face mouth
    closed/no-hit returns while retaining open-mouth and collision paths in
    assembly until naturally exercised.
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
- [x] Replace the Upper Wall-Pocket hand initializers:
  - Composed `02:5972@all` with the existing `01:C5C9` coroutine scheduler
    for fixed mirrored slots `$40/$41`.
  - Replaced both route calls with their exact 192-cycle, seven-write setup
    transcript and live dispatch `02:5975`; the delayed RNG, collision,
    animation, and response states remain assembly fallback.
  - Release core and owning-family ROM replay preserve the existing entry,
    call, write, and selected-state artifacts:
    `defcf166fa1b1bf77b4e415f57d082f06f60d73d8f73afbbad036b46713062d9`,
    `98595fe928d45a59437f77f804b662a67f1860c2358c649c109b48d2d420ed92`,
    `1f7ce0a73fc4e1c5c9dcc3953563ea48def6dfa37b482d9e827a57cbe23f3490`,
    and `e4521a0c6afa7d98deba96c4f21032e2ff7e702629371a2ac07b9c097bd3dc0b`.
- [x] Replace the Upper Wall-Pocket hand delayed countdown:
  - Replaced all 299 traced non-expiry `02:5991@all` calls for fixed slot
    `$40`, including 292 75-cycle decrements, the 90-cycle high-byte borrow,
    and six original interrupt-handler passages.
  - The independently timed close-state expiry paths remain assembly fallback;
    the traced one-tick-zero opening handoff is completed below.
  - The owning hand-RNG regression continues to observe every countdown call;
    its current compact parent/countdown artifacts are recorded with the live
    setup replacement below.
  - Passed Release and required Debug owning-family checkpoints, plus the
    ROM-free core suite in both configurations.
- [x] Replace the Upper Wall-Pocket hand live timer setup:
  - Replaced the traced fixed-slot-`$40` `02:5975@all` parent, composing the
    shared `00:E8B3` RNG recurrence and `01:C5C9` coroutine scheduler.
  - Preserved the exact 33-write, 1,323-cycle transcript, including the
    `02:5982` parent-level IRQ boundary, all handler writes, RNG result `$73`,
    timer `$0133`, and `02:5991` dispatch.
  - The owning regression now records the non-duplicated native parent and
    countdown scopes (300 calls/407 writes), preserving trace/call/write/state
    SHA-256 values
    `917e2973508c58a15a7c573149a808ecf723e69e05aa75010dc3ea40817eb36e`,
    `cf195dceb46c10ceceb195f202c4ed3871c127ee8d28fbad4058819fca4ca395`,
    `ac951f417d75a00928fb1dd255030856345542357f0315795f74af753ebbe92d`,
    and `664cdeb5e46aa4e1f8e2986f9cd915d9dca00456a8c268546e897abe52ebcba5`.
  - Passed Release and required Debug owning-family checkpoints, plus the
    ROM-free core suite in both configurations.
- [x] Replace the Upper Wall-Pocket hand traced zero-timer handoff:
  - Forced only the first routed slot-`$40` countdown frame from `$0001` to
    `$0000`, then captured its `02:5991 -> 01:C5C9 -> 02:59A5` continuation.
  - Composed the existing coroutine scheduler natively, preserving the exact
    20-write, 918-cycle transcript and its periodic IRQ passage.
  - Added `dc_rom_upper_wall_pocket_hand_expiry`, which pins the 174-call,
    235-write controlled replay. Its Release/Debug census SHA-256 values are
    `9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed` and
    `6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`;
    shared trace/call/write/state hashes are
    `0e5676d007ede57f34e878ce69b920fb8d3e112b070cd025b0f12ad38785235d`,
    `32cdc094ab4f750ed568fe93598a73b401ef0688e087d8ab66ee453835227744`,
    `0ff5459dd6105bc7a05a7313cc2de3f59c7e7db765de6ca60c59351485adf41e`, and
    `638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab`.
  - Passed the Release fixture in 9.71 seconds and the required Debug family
    checkpoint in 219.32 seconds, plus ROM-free core tests in both builds.
- [x] Replace the Upper Wall-Pocket hand opening continuation:
  - Replaced the trace-backed fixed-slot-`$40` `02:59A5@all` scene-clear
    transition, including collision/state/type writes, `$59C4` continuation
    submission, and the native shared scheduler's four-to-three-tick result.
  - Added `dc_rom_upper_wall_pocket_hand_opening`, which pins the exact
    16-write, 456-cycle no-interrupt scope and its combined 174-call parent
    route. Release census/trace/call/write/state SHA-256 values are
    `9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed`,
    `9df5f42ab0789c93b6127e0d40dd029b59e6b892a5b38a5fa69c61389cb0d24d`,
    `6796cbd0ac56579f11033fa2a96aca50d6a437f1eaf82d8486af89263fe5b1af`,
    `959c00ca63264aeb7e49b95a29d5f46c62b93c112b9a3cf607996c97735e597a`, and
    `638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab`.
  - Passed the Release owning regression in 9.52 seconds and the Release
    ROM-free core suite. Per `docs/NATIVE_PORT_WORKFLOW.md`, defer Debug until
    the next hand-continuation sibling or the family-close checkpoint.
- [x] Replace the Upper Wall-Pocket hand opening follow-up:
  - Replaced the traced fixed-slot-`$40` positive-state `02:59C7@all` branch,
    preserving the `$82` type write, one-tick timer, `$59DF` scheduler
    continuation, and all nine ordered writes.
  - Added `dc_rom_upper_wall_pocket_hand_opening_followup`, which composes
    `02:5991`, `02:59A5`, and `02:59C7`. Release/Debug census SHA-256 values
    are `9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed`
    and `6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`;
    shared trace/call/write/state hashes are
    `b7d9ad263c05ebcbe441dd829947f36a30571ad693a4dc5e73798b62fc27ca05`,
    `8096c9e92792beabce21640fce8a0eeb516577c91ce1b185e515723cbb08ad26`,
    `f9b54e63be8bda6487d9499589586156d0c7bc5570dbdb3cb204f2ee2bd0c1f4`, and
    `638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab`.
  - Passed Release core/owning regression (8.88 seconds) and the required
    Debug family checkpoint (187.29 seconds); both Debug and Release core
    suites pass.
- [x] Replace the Upper Wall-Pocket hand scheduled opening expiry:
  - Replaced fixed-slot-`$40` `02:59E0@all`, the actual scheduler entry after
    `$59DF` is placed on the stack. Its five observed calls preserve four
    timer/appearance paths and the appearance-two RNG, randomized-delay, and
    `$5A07` scheduler handoff terminal path: 33 ordered writes total, with the
    terminal call taking exactly 825 master cycles.
  - Added the Release per-slice gate
    `dc_rom_upper_wall_pocket_hand_opening_expiry`, initially composed with
    `02:5991`, `02:59A5`, and `02:59C7`; the next sibling extends that same
    family gate and adds its required Debug checkpoint.
  - Passed the Release owning regression in 8.87 seconds and the Release
    ROM-free core suite. Per `docs/NATIVE_PORT_WORKFLOW.md`, defer Debug until
    the next sibling or the family-close checkpoint.
- [x] Replace the Upper Wall-Pocket hand post-opening scheduler:
  - Replaced fixed-slot-`$40` `02:5A08@all`, the actual scheduler target after
    the `$5A07` return address prepared by the terminal opening-expiry path.
    It preserves the `$5A0A` JSR stack return and the seven-write, 192-cycle
    `01:C018`/`01:C5C9` transfer that installs `$5A0B`.
  - Extended `dc_rom_upper_wall_pocket_hand_opening_expiry` across
    `02:5991 -> 02:59A5 -> 02:59C7 -> 02:59E0 -> 02:5A08`. Release/Debug
    census SHA-256 values are
    `9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed` and
    `6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`;
    shared trace/call/write SHA-256 values are
    `1022784e50f4a7855690e288dc3ad51e2f42806fd9bcc78288f7f23bfb16be59`,
    `d71bb60b3596138eef1a5132b9502c1657ac483af7e89a673bbf5f32d63e4b0c`, and
    `5fdf45323d0455d86bc0df31033cd203604aef8290d404062fa88dfb802ea562`.
  - Passed Release core/owning regression (8.78 seconds), plus the required
    Debug core/owning family checkpoint (187.69 seconds).
- [x] Replace the Upper Wall-Pocket hand post-opening state:
  - Replaced fixed-slot-`$40` `02:5A0B@all`, which writes the one-tick
    `$3A40` timer, preserves the `$3080` collision mark, and composes the
    `$5A1A` scheduler return that installs `$5A1B`. Its single natural call
    preserves all 24 ordered writes and the exact 1,170-cycle transcript,
    including the timer IRQ through native scheduler checkpoints.
  - Extended `dc_rom_upper_wall_pocket_hand_opening_expiry` across
    `02:5991 -> 02:59A5 -> 02:59C7 -> 02:59E0 -> 02:5A08 -> 02:5A0B`.
    Release/Debug census SHA-256 values are
    `9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed` and
    `6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`;
    shared trace/call/write SHA-256 values are
    `2ad3bfd2b4f3b87786c7bba3c65809e08d8c683230deb0dae18316a104c035ae`,
    `ca2d781786e8f820a290b4e2d934b875c4ec701001d3027557323dfa92f13694`, and
    `1fbc49257075fc0fbc4b80594a0424ef1d125acb6d8d65ac164a2e6085a25d7a`.
  - Passed Release core/owning regression (9.69 seconds) and the early Debug
    core/owning checkpoint required for the new timer-IRQ transcript
    (195.40 seconds).
- [x] Replace the Upper Wall-Pocket hand post-opening countdown:
  - Replaced all nine fixed-slot-`$40` `02:5A1B@all` dispatches: six
    33-cycle timer decrements, two 96-cycle reload/collision-counter paths,
    and the final 81-cycle zero-counter handoff to the existing `$5975`
    live/RNG tail. The handoff deliberately resumes assembly before that
    tail's interrupt-bearing work begins.
  - Extended `dc_rom_upper_wall_pocket_hand_opening_expiry` across
    `02:5991 -> 02:59A5 -> 02:59C7 -> 02:59E0 -> 02:5A08 -> 02:5A0B ->
    02:5A1B`. Release/Debug census SHA-256 values remain
    `9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed` and
    `6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`;
    the expanded shared trace/call/write SHA-256 values are
    `0c42eb69a906861a5f2e6335aa08c89a72b0a5a61bd411b5f849faf04fd4a9cb`,
    `40ead7d76cf9de8f1621ed2e31f1b4363f153f5e31e8ccb8f11a1dfac5f2ce89`, and
    `fa415070ae64c33cf45ecb5dd700a2f3e2ae873658c1f2569eb34ef7518b3232`.
  - Passed the Release core suite and owning regression. Per the documented
    cadence, defer Debug replay until another sibling or the family-close
    checkpoint; this slice adds no new native interrupt transcript.
- [x] Replace the Upper Wall-Pocket hand `$2D33` dormant guard:
  - Extended `02:59A5@all` with the fixed-slot-`$40` nonzero-latch path:
    exact `LDA $2D33` / taken-branch return through `$59F4`, 27 cycles, and
    no game-owned writes for all 298 observed calls.
  - Added `dc_rom_upper_wall_pocket_hand_guard`, asserting only `$2D33` at
    frame 1,222. Release/Debug census SHA-256 values are
    `c12f290cd61c254e573b362ab572cb1afb19a5ddaabc27ca77bcdd19e51c71d8` and
    `5ffb267191ede4e70121638ae0198463a25e95b71582d82bb6e84457bb80248e`;
    shared trace/call/write/state hashes are
    `3e2722d448c4e22c57bb4fb6846f4e5331ff062b55a3344c2afc95b312d57b9d`,
    `d114365a0d52b054fa80394cd83a2a9d86041847c069f709d567dff02f7039d2`,
    `edb384280b11fa4cab1e6527ac1ee928d13c9dd7b2753af938d76715eef38a83`, and
    `eccef7f1968b9576f34ba291712d1e67d52cbf428c1c8c74cc12855c6d5c1df0`.
  - Passed Release core/owning replay (7.84 seconds) and the required Debug
    core/owning new-branch checkpoint (165.19 seconds).
- [x] Replace the Upper Wall-Pocket hand negative-state close loop:
  - Extended `02:59C7@all` with the fixed-slot-`$40` high-state path. It
    clears `$3840`, returns through `$59BF`, submits the four-tick delay, and
    composes `01:C5C9` to dispatch the next `02:59C7` follow-up.
  - Added `dc_rom_upper_wall_pocket_hand_close`, which observes one
    interrupt-bearing 1,089-cycle, 27-write close loop and its two 261-cycle,
    nine-write positive-state follow-ups. Release/Debug census SHA-256 values
    are `7a3466734aa3856e9158828057ce31f7a9854d95a9ce8e9b6768b372b3bcb9df`
    and `45a7a4ef3c92924574d97666aa03cfae4f36d837e2a74efc3eec71c6ecd4fba9`;
    shared trace/call/write/state hashes are
    `2fbfe2597e236fc6f6249f9388f61999514985c88b339cfd9c9a4a4232542096`,
    `7b7516225b1a6fe7c1612c42c386816c69cef5ca04c294b76e720e623796fb4e`,
    `4514ec8143334011d1856f0163c460ce1d454601e2763307e2a1fd08cb8d352b`, and
    `d9608965ca1ec3f589fe9e048c9598e8a77a9a18a09d1fdf7a1ea5bd973595f2`.
  - Passed Release core/owning replay (7.55 seconds) and the required Debug
    core/owning new-branch checkpoint (168.64 seconds).
- [x] Replace the Four-Star Wall-Pocket collision gate:
  - Extended `02:4A93@all` with the fixed lower-left pocket's exact
    `LDA $383E` / taken `BMI $4A9B` collision prefix and its controlled
    zero-rotation selector decision: the non-matching access continues at
    `$4ABB` after 144 master cycles and the matching arrow continues at
    `$4ACF` after 171 cycles. The matching path includes its `$4ACA`
    `STZ $2CFA` latch clear and `$4ACD` `$41` audio-command load; the paths
    retain thirteen and fifteen instruction checkpoints respectively.
  - Added matching-arrow continuations `02:4AD2@all` and `02:4AE5@all`:
    after the original shared-audio return, they compose the native 100-unit
    score transaction (including its timer IRQ), `$3A3E=4` timer, `$3980=4`
    appearance write, and shared scheduler handoff that installs `$4AEB`.
    The shared audio and `$406F` appearance helpers remain in assembly.
  - Added `dc_rom_four_star_wall_pocket_matching`, which injects the
    matching selector on collision frame 1,234. Its Release/Debug census
    SHA-256 values are respectively
    `285273abaf31a916d0fbebaaea2131b09f35ae595918ed53a066d7d2045f169b`
    and `d1fc741494aaa41541494ce0c5b1c201990d175f0e4abd6cfa4ddfefa9aab0a1`;
    Its Release trace/call/write/state SHA-256 values are
    `0f55ad5c69a31bdadcd3ebf2392fb24005bc4cf463e86f2b0e5c0c06b0ab05ad`,
    `e0e62f03a8be06ccda672042d8dda45b1b60656f4cd18a22ee450c5003627a49`,
    `e354a6f21119dd9544d407c2e8c82883dfac774ee1ac4502e7470f99b0d5a65c`, and
    `6c5946171ec29f2bf46eb93dbe307aa3492fb10fe80361fdaf7075c8225fd49c`.
  - Passed Release core/owning replay (7.75 seconds) and the required Debug
    core/owning family checkpoint (176.05 seconds); all captured artifacts
    match in both configurations.
  - Added `tools/Analyze-NativeFallbackHotspots.ps1` and its documented
    observer-set workflow. Its first natural Four-Star pilot ranks unported
    `02:4AEB` at 506 calls, 40,368 master cycles, and 857 ordered writes
    (50,652 weighted cost), making it the measured next family candidate.
  - Added the focused `dc_rom_four_star_wall_pocket_collision` regression;
    the one controlled collision plus 1,167 subsequent controller entries
    preserve Release/Debug census SHA-256 values
    `53432539e70810d69ef8b3cce45cd81300749ea021f27f4fd71e8587ccfbe28e`
    and `d7a897a7c019c8b618176c9b92371d08f82f602b3ba17cd5fede2e8dcf0c000c`.
    The shared trace/call/write/state hashes are
    `414300f98d17d9dfcbb29723f818de3114848e7b44c7ca87d359153de24c3526`,
    `a4e5c8e36845ab70dd128a3b2e814dfdff5cc2ae406d00336c1494b0ee13f808`,
    `f822e87f6a423a9de0d00f10ae6bea73bde54c314c734d13c740f86886ccb9fc`, and
    `a019a78ef19fc7d72e11b329aaf90bb390e50cf6728a56aa897c1a974664069a`.
  - Passed Release and Debug core tests; original-versus-native artifact
    comparisons are byte-identical in both configurations.
- [x] Replace the Four-Star Wall-Pocket scheduled animation continuation:
  - Semantic `02:4AEB@all` now covers the naturally observed lower-left
    pocket's four-tick countdown and phase-advance paths. The adapter retains
    the exact 60-cycle/one-write and 96-cycle/three-write transcripts, and
    separately reproduces instruction-boundary IRQ delivery through the
    decrement checkpoint. Appearance six enters the measured 942-cycle,
    22-write scene transition at `$4B00`, which deliberately remains
    assembly-backed.
  - Extended `dc_rom_four_star_wall_pocket_matching` to observe and replace
    all 506 scheduled calls. Its Release census and selected-state artifacts
    remain unchanged; the expanded trace, call, and write artifacts are
    `6105daa60a740fc1d9f9a1b9e421a1dc9c87fe980e8aec4b804214a12087dea2`,
    `9abc6c7b5a8d2518f5ecc5481bc693917b056659e9d9f0eec95e155e4ee30e24`,
    and
    `4e983a7998075dfa71ddd478cdcab4ed8995ce7c7960ced84ccbc0bd15ff59a3`.
    The focused Release replay passed in 7.88 seconds; the required new-branch
    Debug family checkpoint also passed in 156.09 seconds.
- [x] Replace the Walled-Star access pocket steady controller:
  - A fresh parent/continuation profile ranked `02:4B3F` at 905 natural
    calls, 95,532 master cycles, 1,791 ordered writes, and 117,024 weighted
    cost. The higher-cost `01:C05D` fixed-marker result was correctly scoped
    out as the shared emulated video-drawing jump table.
  - Semantic `02:4B3F@all` reuses the independently proven four-tick
    timer/appearance tail for the collision-free fixed-slot-`$11` path. All
    905 calls execute natively through exact 66-cycle waits, 135-cycle
    advances, and 144-cycle resets, including the original interrupt
    checkpoints. The collision score/opening/bonus-stage transition remains
    assembly fallback pending a dedicated transition transcript.
  - Added the explicit pure-C Walled-Star access classifier alias and
    extended `dc_rom_walled_star_access`; Release core and owning-family ROM
    replay passed with all existing artifacts unchanged in 8.57 seconds.
- [x] Replace both Screen-2 Side-Star access steady controllers:
  - A fresh sibling-family profile ranked lower-left `02:4BD5` at 1,277
    natural calls, 132,906 master cycles, 2,494 ordered writes, and 162,834
    weighted cost; upper-right `02:4C0A` added 905 calls, 94,947 cycles,
    1,786 writes, and 116,379 weighted cost.
  - Semantic `02:4BD5/4C0A@all` reuses the independently proven four-tick
    tail for all 2,182 collision-free fixed-slot `$10/$12` calls. Exact
    66-cycle waits, 135-cycle advances, 144-cycle resets, and instruction
    checkpoints remain intact; the scoring, transformation, and bonus-stage
    entrance collision branches deliberately remain assembly fallback.
  - Added the pure-C Side-Star access classifier alias and extended
    `dc_rom_side_star_access`; Release core and owning-family ROM replay
    passed with all existing artifacts unchanged in 8.81 seconds. The
    required Debug family checkpoint then passed both Walled-Star and
    Side-Star replays in 190.39 and 172.06 seconds respectively.
- [x] Replace the upper-right Side-Star entrance-animation leaf `02:4C33@all`:
  - A controlled `$3812=$80` collision route exposes nine fixed-slot `$12`
    entrance calls. Six are 60-cycle, one-write timer decrements and two are
    96-cycle, three-write timer reload/appearance advances; the ninth is the
    309-cycle stage-selection transition.
  - Semantic `02:4C33@all` covers the eight nonterminal calls and falls back
    for appearance six. Its normalized replay at frame 1,221 preserves exact
    60-cycle, one-write parity.
  - Added the Release-only
    `dc_rom_side_star_bonus_entrance_animation_semantic` regression. Census,
    entry, call, and ordered-write artifacts are byte-identical to assembly.
- [x] Replace the queued-Knight collision helper:
  - Fresh comparison profiles measured Front-Spider parent `03:6490` at
    2,645 calls / 2,324,265 weighted cost and the passive Knight parent
    `03:7378` at 10,600 calls / 1,982,160 weighted cost. The latter exposed
    compact collision child `03:73D8` at all 10,600 entries.
  - Semantic `03:73D8@all` now handles the fixed Knight slots `$3A-$3D` when
    their collision byte is nonnegative. It preserves the exact 33-cycle
    load/branch/return transcript and all three IRQ checkpoints. A bit-seven
    nonterminal hit now also natively clears its collision latch, decrements
    the counter, and hands the original `03:7BF6` collision/audio/score
    helper its exact JSR frame. A terminal fourth hit likewise natively clears
    the latch and decrements to zero through its carry-set `03:73E8` return;
    the subsequent destruction state machine remains assembly-owned.
  - Added the pure-C classifier and `dc_rom_knight_hit_gate_semantic`.
    Release core and focused ROM replay preserve census/call/write SHA-256
    values `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
    `6d8c143b2d3519b08ab769316c6754d14d7b8217e4c1d31609c76b085ecfb3c1`,
    and `9d7282f06841f7e4dcbd40d6ccfeb507647c85afe351251be26d2c04832f31da`.
  - A forced release/hit route sets only `$383A=$80` at frame 1,402. Its
    10,642-call `03:73D8@all` replay executes the 96-cycle nonterminal-hit
    prefix while retaining the 150-write helper transaction in assembly; the
    census/call/write SHA-256 artifacts are
    `116338aeb088606d937f58c7aa04d853c2284e2137c60d323fe0e1125e416890`,
    `6bb57b4cbce1bd9dac52677950a2b430513a873df2942774eb43550ea44f8ced`, and
    `62b0d5380d9719cd86cc9f6b98b710a64d15bea4fdb88a23b77a6e46ab0f2acd`.
    The composed waiting-parent slice below completes the required Knight
    family checkpoint.
- [x] Compose the queued-Knight waiting parent:
  - A fresh six-entry screen reprofiled the four initializers, steady parent,
    and native collision child. Parent `03:7378` remains the clear payoff at
    10,600 calls and 1,982,160 weighted cost; the four initializers total
    only 1,662 weighted cost.
  - Semantic `03:7378@all` now composes only the fixed-slot `$3A-$3D`
    waiting branch: clear indexed Giant-Door request bit plus nonnegative
    collision state. It preserves the exact 123-cycle queue/channel reads,
    child-call stack writes, nested `RTS`, and carry-clear outer return.
    Door activation, movement, and the four-hit path remain assembly fallback.
  - Added pure-C classification and
    `dc_rom_knight_waiting_parent_semantic`. Release core and focused ROM
    replay preserve census/call/write SHA-256 values
    `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
    `fcc85209b476ba21441502ef04ae63c9511045399c9720cd188dc5de49b68f6c`, and
    `a43918c5cb8f86a9a2c458e2dc9ec4687f0eb33b3550be3a1f5592258854c793`.
  - The required Debug core/family checkpoint also passes: its census is
    `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
    and its call/write artifacts match Release. The durable regression covers
    both configurations; this completes the Knight family batch.
- [x] Replace the shared Front-/Side-Spider appearance helper:
  - A composed observer set separated `03:65A4` from Front-Spider collision
    parent `03:6490`, collision interpreter `03:6966`, and the same helper's
    Side-Spider callers. It measured 5,441 calls, 293,640 master cycles,
    8,167 ordered writes, and 391,644 weighted cost.
  - Semantic `03:65A4@all` now covers fixed Spider slots `$26-$29` through
    exact 33-cycle timer waits, 102-cycle phase advances, and 111-cycle phase
    resets. Its instruction checkpoints preserve the native timer writes and
    original timer-interrupt passages; both collision parents remain assembly
    fallback for their selector, interpreter, score, and transition work.
  - Added the pure-C classifier and `dc_rom_spider_appearance_semantic`.
    Release core and focused ROM replay preserve census/call/write SHA-256
    values `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
    `7e987ec3f4faa07710a6d528d2e108ca4b5157cf8dd0f9183c701a1eb3583356`, and
    `d7c55c4dde46c9c844669f67e5a3e932d411fa8aac206d6e1d9bc284fccac210`.
    Debug replay is deferred until a Spider parent composition or family
    checkpoint under the documented cadence.
- [x] Replace the paired Small-Soldier source waits:
  - A fresh Walled-Star screen rejected `03:6366` as an internal branch
    already subsumed by native `03:6339`. The separate Small-Soldier profile
    then measured left/right source waits `03:7D85/7E23` at 2,457 calls each,
    with a combined 131,439 weighted cost.
  - Semantic `03:7D85/7E23@all` now covers fixed source slots `$34/$35`
    whenever their corresponding `$2D16/$2D17` group count is nonzero. Both
    use the exact 21-cycle absolute-load/branch/return transcript and retain
    their timer-interrupt passages; zero-count completion, child deployment,
    and collision dispatch remain assembly fallback.
  - Added the pure-C classifier and `dc_rom_small_soldier_source_gates_semantic`.
    Release core and focused ROM replay preserve census/call/write SHA-256
    values `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
    `15b059014d358901b21d75b349df894df0fc9e981f1f038ded740e622ce779db`, and
    `e9918a499097dea8cc28d1315f6e25d6adf6012ece1914b236d26ff33451993f`.
    Debug replay is deferred until a Small-Soldier child or family checkpoint
    under the documented cadence.
- [x] Replace the Sorcerer controller stable-color gate:
  - A fresh Sorcerer profile rejected broad orbit `02:58C2` as a mixed
    collision/drawing/interrupt parent despite its 22,113,279 weighted cost.
    It instead identified direct controller branch target `02:57E9` at 2,648
    calls and 145,128 weighted cost.
  - Semantic `02:57E9@all` handles fixed hidden controller slot `$16` when
    shared color `$2D40` equals cached `$3980,X`. Its exact 42-cycle
    load/compare/branch return retains original interrupt passages; the
    color-update store and `02:593F` helper remain assembly fallback.
  - Added the pure-C classifier and Release-only
    `dc_rom_sorcerer_color_gate_semantic`. Release core and focused ROM replay
    preserve native census/call/write SHA-256 values
    `e4826889df5db0c7585e889ab2df1db3e8401be39db0b909ebea6be0950278ce`,
    `902b5143e8de9aabf2accc6954f6e870bbee820e5ca0fd0edf933fd651962a64`, and
    `6ca2df71d6b3cd024e9dbf997f7e590083aeb18772abc79961b36a15aa753ca4`.
    Debug replay is deferred until a Sorcerer sibling or family checkpoint.
- [x] Replace the Tower-insect animation leaf:
  - A fresh Tower-insect profile rejected mixed controller `03:6016` (6,494
    calls; 6,819,600 weighted cost) because it composes collision, scoring,
    audio, and child lifetime. It selected direct `BSR` target `03:6054`
    (6,750 calls; 1,612,386 weighted cost) instead.
  - Semantic `03:6054@all` accepts dynamic insect slots `$64/$67/$6A/$6D`,
    selects the ROM frame-table value from cadence/phase state, and executes
    exact 147-cycle timer waits or 183-cycle reload/phase advances. Collision,
    scoring, audio, and release behavior remain assembly fallback.
  - Added pure-C classification and Release-only
    `dc_rom_tower_insect_animation_semantic`. Release core and focused ROM
    replay preserve native census/call/write SHA-256 values
    `7c99d041007e2a735b2bf8e6c7c870615c15bb0e79dee9330f1414da2187853c`,
    `9d3beee15ee6ada8ea7b15b414745437e71876b1ef5be14e5f184c188b7ddf76`, and
    `a0d6ad02b991511ecfde741d752b4c11b5da61afe9a27cdd02d90b397a4c2f2e`.
    The paired source-gate slice completes the documented Debug family checkpoint.
- [x] Replace the Tower-insect source child-count gate:
  - The corrected physical-bank family profile observed source countdown
    `02:5F4A`, direct child-count gate `02:5F5F`, and allocator `02:5F78`
    beside the dynamic controller and animation leaf. It selected `02:5F5F`
    as the composable sibling: 2,504 calls and 67,395 weighted cost.
  - Semantic `02:5F5F@all` admits only fixed source slot `$2C` while live
    child count `$2D2B` is nonzero, preserving its 21-cycle load/branch/return
    transcript. The zero-count 20,000-point group-clear branch, source
    countdown, and child allocator remain assembly fallback.
  - Added pure-C classification and Release-only
    `dc_rom_tower_insect_source_gate_semantic`. Release core and focused ROM
    replay preserve native census/call/write SHA-256 values
    `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
    `d0caafb2f34cd597e0fa24e2f1a179bf44897dc5fd1aa3fc63ffbcdc186f3c05`, and
    `338ff7769d0f05f93aab446eaac4c27066232aa9daf86cac5cebf033622da6f9`.
    The combined Debug Tower-insect family replay passes both native leaves.
- [x] Replace the Four-Star Wall-Pocket steady controller:
  - Reused the ROM-shared collision gate and four-tick timer/appearance tail
    at `02:4A93` for fixed slots `$3E/$3F/$42/$43`.
  - Replaced all 1,008 collision-free calls with exact 66-cycle wait,
    135-cycle appearance advance, and 144-cycle appearance reset
    transcripts; collision access, Big-Star score/color state, and
    bonus-stage transition remain assembly fallback.
  - Preserved the existing entry, call, write, and selected-state artifacts:
    `eafb2abd72e818a258eee861904f0abb097cb1703c885a66316b08a318fad3d9`,
    `2f86b84a3997f8a84239a554ecb34523fc94c564f337ff446b8df61f91b1d64b`,
    `d4fe55cf0b4b975eecd160accace5b3f0e753bf05b99b2e6a5c7b7d375f256b8`,
    and
    `6bb4604bcffd88960c62c76ae98bc5a11984a4711bef456da014b3d31cf1de4e`.
  - Passed ROM-free core tests, the Release owning-family regression, and the
    required Debug family checkpoint.
- [x] Replace the Walled Star's post-destruction idle prefix:
  - A controlled dispatcher profile at fixed slot `$5B` isolates `03:638F`
    while both access latches `$2D53/$2D32` are clear. All 2,390 calls resume
    at the original `03:6399` RTS through the exact 42-cycle, zero-write
    two-load/two-branch transcript; 36 timer-interrupt passages remain
    emulator-owned through four instruction checkpoints.
  - The Release owning regression preserves census/call/write SHA-256 values
    `432af3cde4152def8c9afc6845b9575c8812859e76330e8de1f3dd61c9a24ee6`,
    `6bdbc72c548309243846bb601536361693c162bd6689dbec1532f6a034f78cf6`, and
    `1df166a843eb05bb8f258d942b6c0999b14dee6c5c723276dab8702b55b96b10`.
    Debug repeats the exact call/write artifacts and preserves its matching
    census hash `7de49230f4654595da7eb268945929c9cbceabcadb03de21cddd376dc285d1ac`.
    The live acknowledgement, wall-clear animation, and bonus routing stay
    assembly-backed.

## Active Work

Current owner: Claude. Current slice: **the Sorcerer tick is native**, taking
the deployed registry to **41.02%** of all master cycles (530,161,950 of
1,292,371,131) across 2,333,873 native calls and 353 admitted routines.

`02:586F`-`02:5903` was the largest block of unported gameplay left on the
census route and the only block of its size with no native entry anywhere
inside it. It is the Sorcerer's per-frame tick -- the deployment and staging
heads at `02:57F8` and `02:5808` were already native, and this is the three
states that follow, for the eight Sorcerers in slots `$17`-`$1E`. One routine
in three pieces, split by the two calls it makes to the collision-table
interpreter at `03:6966`: the staggered deployment delay on the sixteen-bit
countdown in `$3B00,X`/`$3B80,X`, the arrival state that reseeds the table
cursors when the interpreter reports its terminator, and orbit, where all eight
sit for the rest of the route. Five entry points, admitted on every call,
3,156,294 cycles gross across 41,144 entries and `+2,595,360` net.

`02:5896` is hookable on its own but is deliberately not an entry point: the
walk from `02:5893` branches into it, so registering it would leave a routine
the deployed registry never enters, and the CLI treats a hook that is never
entered as an error.

**The screen was the cleanest the in-window ranking has produced.** Ninety-nine
instructions across seven banks write the addresses this family reads. Exactly
one of them ever writes inside one of the family's call windows -- `03:69D9`,
the interpreter's own event store, which is this family's callee -- and the
other ninety-eight land inside a window on 0.0% of their executions. Not "close
to zero": zero. Two slices ago the same method gave eighteen callees at
99.3-100% and three hundred others at 0.00%. That is now the expected shape of
the answer, and a middle band is the signal that something needs looking at.
The corollary is worth stating: a window that contains a call contains its
callee's writes, so a callee scoring high is evidence the window is drawn
correctly. What matters is that nothing else is in there.

**The shadowing bill now looks like a constant.** 560,934, 17.8% of gross --
the lowest of the last four slices, and low enough to see the mechanism through
the noise. Five measurements exist: 15.6%, 18.1%, 21.0%, 19.7%, 17.8%.
Underneath them the per-event numbers barely move. The driver costs 574 to 577
native cycles per displaced entry every time, and one entry is displaced for
every 2,900 to 3,300 master cycles a transcript holds the core, so the bill is
close to `gross / 3,000 x 575` -- about 19%. Two caveats keep it an estimate.
The spread is three points either side, a hundred thousand cycles on a slice
this size. And the displacement rate is 1.7 times the naive figure: the timer
fires every 5,068 master cycles, so a transcript covering N cycles should
overlap N/5,068 of them, not N/3,000. Part of that is the shadow window being
wider than the transcript, but that accounts for about a fifth of the excess.
The rest is unexplained. Use the rule to decide whether a region is worth
starting; keep measuring the bill with `--registry-exclude` before reporting
what a slice delivered.

Verification is the two-run diff against pure assembly: 240,657 work-RAM writes
covering the family's whole output -- `$2D10`, `$3080,X`, `$3880,X`, `$3980,X`,
the cooldown pair, `$3C00,X` and the table cursors -- byte-identical hooked and
unhooked, plus the state census. This family writes no device port. Regression:
`dc_rom_sorcerer_tick_semantic`.

Next: the region is 4,055,142 cycles down to 910,530, and about 87% of that
remainder is the leaf-contract call tax -- two `JSR`s to the interpreter, two to
the bank-01 wrapper at `$C018`, and three `RTS` instructions. Composing the
interpreter would reach it and would also reach the 5,274,582 still emulated at
`03:694A`-`6A37`, but `03:6966` is an atomic body rather than a checkpointed
transcript and already declines 2,850 of its own 34,259 entries at an interrupt
horizon; splicing it into a longer parent puts that uncheckpointed stretch
inside the parent. Turning the interpreter into a transcript first is the
larger and more valuable move, and it is a rewrite of a 2,600-line semantic
replacement rather than a new walk. Otherwise the remaining phase-1 groups are
the palette shadow buffer at `00:E94C`-`EBD6` (3.8M) and the sequencer's note
handlers at `45:4517`-`4A9B` (3.2M). The pad reader `00:E195` (6.9M) and the
interrupt dispatcher `00:E058` stay blocked on device *reads* and are phase-2
work with the joypad and the VDC.

Previous slice: **the sweep composes the collision scanner**, taking the
registry to 40.82%. `01:CB48` stopped stopping in front of `JSR $CC1A`: it
transcribes the call, runs the scanner's own replacement on a staged work RAM,
splices its checkpoints and writes at the cycle the call landed on, transcribes
the `RTS` at `01:CC40`, and hands back at `01:CB61`. 83,905 of the scanner's
85,715 calls compose, at 42 master cycles each -- 3,524,010 gross, exactly the
tax, the one figure that was safe to predict. `01:CB5E` fell from 1,795,419
cycles to 17,622 and `01:CC40` from 1,797,309 to 19,776, and `01:CB48` became
the largest single native address on the route at 7.96%. The 1,810 calls that
do not compose are the hit path, which leaves the scanner from the middle of
itself at `CCD2` or `CCD4` with no `RTS` to transcribe, so the walk truncates
its own vectors back and ends in front of the call as before. Two things a
future composition needs: hand the child a staged work RAM carrying the
parent's writes, and treat the splice as an attempt that rolls back.

Previous slice: **the collision frame is native**, taking the registry to
40.60%. `01:CA84`-`CDF7` is the code on either side of the overlap scanner
`01:CC1A`: the playfield-tile probe `01:CD74`, the ball/table gate `01:CA84`,
the sweep setup and tail around `01:CB48`, the single-slot probe `01:CBAB` and
the confirmed-contact dispatcher `01:CBCF`. Nineteen entry points, admitted on
every call, `+3,232,335` net. It found that **the inventory row had been drawn
around the wrong addresses**: `01:CD74` was 2,643,867 cycles, two thirds of the
family, and had no row of its own because it sits outside the `CA84`-`CC04`
span the inventory quoted -- when a row looks small for what it is called,
re-derive its span from the profile. It also produced the sharper shared-state
test now in the workflow: rank each foreign writer by the fraction of its
executions landing inside an exact call window, and callees separate from
preemptors with no judgement call. For this family the split was absolute --
eighteen arithmetic instructions at `00:E56E`-`00:E847` between 99.3% and 100%,
every other writer at exactly 0.00%. Its shadowing bill was 21.0% of gross,
higher than the tilemap's 18.1% on a fifth *less* native time, which settled
that total native time sets the bill's scale and nothing more.

Previous slice: **the background tilemap upload is native**, taking the
registry to 40.35%. `00:F261`-`F4F1` is the other half of the graphics driver's
port work: where `00:E41F` streams a prepared word list into VRAM, this decides
which row of the scrolling tile table has come into view and pushes it. Ten
entry points, admitted on every call, `+4,243,104` net. It found that **an
instruction the core builds from two `LastCycle`s cannot be a checkpoint**:
`BBR` and `BBS` are `LD_ZP` wrapping `JR`, so the machine crosses them in four
clock steps where a checkpoint crosses in two, and those steps are where the
core tests `next_event`. A coarser step ran the video `EventHandler` late,
shifting a VDC event far enough to move a timer interrupt one instruction early
four times in 3,600 frames -- twice in assembly *downstream* of the walk, which
is the tell that a walk is perturbing the core's event phase rather than its
own output. `00:F423` goes back to the core at a cost of 365,664 cycles. It
also established that shared-state windows must be bracketed on **real call
boundaries** from an `--observe-hook` trace, not inferred from the writes: the
VRAM slice's trick works only when the sole way into a region is a call to its
head, and inferring windows here ran them thirty times too long.

Previous slice: **the VRAM and VCE upload paths are native**, taking the registry
to 40.02%. `00:E3C0`-`E47C` is one idiom three times over -- point a VDC or VCE
write pointer at a base address, then push words through the data ports until a
counter runs out -- and nothing in it reads a device register, so the whole 7.0M
row was phase-1 work. Six entry points, admitted on every call, `+6,591,648` net.
It was the first family whose transcripts **chunk**: one decoder call is 167,000
master cycles and 3,400 port writes against a 512-write channel, so each routine
takes a second entry point at the head of its loop and a transcript ends in front
of the backward branch that closes it. The core executes that branch, lands on
the loop head and calls the replacement again; nothing in the core changed to
allow it. Two properties make it safe: every chunk must transcribe at least one
unit of work, or the core re-enters identically for ever, and the leak is the
branch itself. That slice also established measuring shared state rather than
reading the call graph for it.

Previous slice: **the VDC interrupt handler's body is native**, taking the
registry to 39.51%. `00:E058` reads the VDC status register and dispatches on
it, so the head is phase-2 work, but nothing downstream of that read needs the
device again. Six entry points shared one walk -- the two arms the dispatcher
jumps to, the two addresses the VBlank arm's calls come back to, and the
secondary handler's prologue and epilogue -- admitted on every one of 13,932
calls, 1,350,720 cycles. Two findings came with it. **An instruction that
changes `I` cannot be summarised by a checkpoint that publishes its result**:
the first draft covered the `PLP` one instruction short of the `RTI`, and
swallowed a timer interrupt that assembly services between the two, because the
core samples the IRQ at `LastCycle` -- before `PLP` pops -- while a checkpoint
carries one register set. The walk stops in front of the `PLP` instead. And **do
not assume a handler stays masked because the interrupt sequence set `I`**: bank
`45` ends three critical sections with an unconditional `CLI`, so 78 of the
route's interrupt entries land inside these six walks, all at or after
`00:E123`.

Previous slice: **the bank-switching call wrappers are native**, taking the
registry to 39.41%. Every cross-bank call in the game goes through one of five
small wrappers: the caller leaves a physical bank in `$30` and an entry offset
in `$2F`, and a wrapper maps the bank into an MPR slot, jumps through the
callee's `$40xx` table, and unmaps it on the way out. Nine entry points shared
one walk, admitted on every one of 52,911 calls, 4,849,479 cycles. They were the
first transcripts in the port to change the bank mapping, which needed no new
capability. Two findings came with it. **A device read is a harder blocker than
a device write**: the plan for this region said to widen the write channel to
reach the joypad at `$1000`, and the blocker was the read side, which no
allowlist reaches — 7.5M of the row is phase 2. And **the shadowing bill scales
with total native time, not per-call length**: 52,911 transcripts of 36 to 162
cycles cost the driver 1,326 entries and 756,342 cycles, 15.6% of gross and the
largest bill any slice has paid.

Previous slice: **the palette upload `00:EB30` is native**, taking the registry
to 39.10%. It runs once a frame, walking the thirty-two dirty flags at
`$2CCC`-`$2CEB` and pushing sixteen colours per dirty block through the VCE at
`$0404`/`$0405`; admitted 3,152 of 3,181 entered, 6,870,516 cycles. Its
whole-palette arm stays emulated — one `TIA` moving 1,024 bytes is 1,024
writes against a 512-write channel — but that arm is 29 calls of 3,599. Two
findings came with it. Widening the write channel is **two edits, and the
second one is in the fork**: adding `$0400`-`$0405` to
`is_replaceable_hardware_port` let the writes through, and the transcript then
ran three master cycles long per colour because `AdvanceReplacementTo` tested
only `logical_address <= 0x0003`; `IOWrite` steals a CPU cycle for the VDC
*and* the VCE, so both belong in that test. Every write value and every write
order was already right when the clock was wrong, which is the whole argument
for diffing traces rather than end state. And **whether the routine runs masked
decides the shadowing cost**: `00:EB30` spans three timer periods and cost the
driver nothing, because a VCE upload has to land inside the blanking interval,
so all 3,599 calls arrive with `I` already set. The palette group is finished
for the hybrid at 18.2M of 20.6M; what is left is unreachable rather than
unported.

Previous slice: **the palette shadow buffer's three leaf maintainers are
native** — `00:E9B6`, the block interpolator (672 calls, 6,548,946 cycles);
`00:EBA5`, the block stager (1,121 calls, 2,629,866); and `00:E93C`, the
sixteen-bit compare the fade drivers branch on (8,969 calls, 758,370) — taking
the registry to 38.56%. All three are admitted on **every** call, because they
are leaves in the strong sense: no `JSR`, no device port, and nothing another
thread of control writes, so there is no horizon to miss and no snapshot to go
stale. Two findings came with it. A lumped inventory row hides both the target
and the blockers: the elimination plan listed `00:E93C`-`EBD6` as one 20.6M
"VCE palette DMA", and it is six routines with nothing in common but an address
range. And two divisions decide whether a candidate can be admitted at all,
more cheaply than the disassembly — the obvious next slice was `45:4015`, the
music sequencer's per-frame tick, and it is not portable: its voice loop
reaches `45:4148` (`LDA $F5,X`, the timer driver's channel bank byte) on all
3,604 of `4142`'s calls, so the shared-state guard applies to a 12,667-cycle
body against a 5,068-cycle timer period, and a builder for it would decline
3,599 calls out of 3,599. Bank 45 is finished for the hybrid at 55%.

Previous slice: **the bank-45 music sequencer's command interpreter,
portamento stepper, sample/envelope group and PSG register programmer are
native**, taking the deployed registry to **37.85%** of all master cycles
(489,149,301 of 1,292,370,213) across 2,257,771 native calls and 294 admitted
routines. The bank is 25,029,471 of 45,586,602 cycles native, 55%.

Bank `45` is the other half of the audio path: the timer driver streams
samples, the sequencer decides what they are, walking its voices once per frame
from `45:4015` and programming the PSG register file directly. It was the
largest emulated block outside the spin loop after the driver landed.

Screening it needed a different instrument, and the instrument is the
transferable part. Rather than an observer set of sibling routines, the
whole-ROM instruction profile was read three ways: by bank (46.1M over 1,078
addresses — flat, hottest address 0.042%, so no inner loop to take), by
scanning the executed addresses for `JSR`/`BSR` opcodes (sixteen call targets,
twelve of them leaves — the bank is a set of small subroutines, not a
monolith), and by walking control flow from each target without following
calls, which attributes the bank's cycles to routines. That converted an
intimidating 1,078-address region into a sixteen-row table in about five
minutes, off an artifact already on disk, and it is what turned this from a
10,000-line transcript into an incremental slice.

Ten routines are native. Seven are the sample/envelope group the voice walk
calls once per voice per frame: `45:48B4` (the shared command decoder),
`45:4883` and `45:48F6` (which inline it), `45:49A2`, `45:49B6`, `45:49F9`
(which drives `$0800` and `$0804`) and `45:4211`. The eighth is `45:4142`, the
PSG register programmer: it walks the six channels pushing period, amplitude,
balance and noise out to the register file, uploads a 32-byte waveform through
`$0806` when a voice's waveform changes, and programs the LFO at
`$0808`/`$0809`. The last two are `45:4228`, the portamento stepper, and
`45:457A`, the command interpreter — the only part of the bank that reads the
music data itself. One shared walk serves all ten; each stops in front of its
own `RTS` and hands the return back to the core, and any call the walk makes is
inlined into the same transcript. That is why `48B4` is admitted only 72 times
on its own — the other 25,896 calls to it are made from inside a `4883` or
`48F6` transcript and never reach the hook — and why `4569`, `4522`, `46D7`
and the forty command handlers are not separate entries at all.

`457A` is where the bank stops being arithmetic. It scans the voice's stream
through `$D0`/`$D1`; a byte under `$80` is a note, `$C0`-`$DD` copies an
instrument record out of `$5AA9`, and `$80`-`$BF` is a command dispatched
through a table of `address - 1` values at `$42BA` by pushing the address and
executing `RTS` — so the handler runs with the caller's return address still
on the stack and its own `RTS` continues the scan. Reaching a note falls into
`4622`, which calls `46D7` to convert it to a period, and for a percussion
voice whose note lands in the sample table that ends at `4776`: remap MPR6 to
the sample's bank, copy the eight-byte header into the timer driver's
per-channel zero page, silence the PSG channel, and start the timer. `4228`
shares that whole tail, reached by a `JMP` rather than a `JSR`.

The remaining 20,557,131 cycles are the per-frame tick at `45:4015` (13.8M),
which calls everything above, plus `45:4936` and the track loader `45:4BCC`.
The tick was screened after this slice landed and cannot be ported while the
emulator is here — see the current slice above for the arithmetic.

**Findings worth carrying forward.**

*A transcript that shares state with an interrupt handler needs a freshness
guard.* `$F5`/`$F7`/`$F9` are the timer driver's per-channel bank bytes — the
only bytes in this bank's working set another thread of control writes. The
driver sets one when it maps a channel's data bank and clears it when the
channel falls idle; `4142` reads all three (and clears one to release a
channel), and `49F9` reaches them through `$20F5,Y`. A transcript is computed
once from a snapshot taken before it runs, so those reads are only sound if no
timer interrupt is delivered while it runs. The walk declines unless the
interrupt mask is already set at entry — `4142` brackets each voice in
`PHP`/`SEI`/`PLP`, so a masked entry stays masked for the whole body — or the
body finishes inside `master_cycles_until_timer_interrupt`. That is a
correctness boundary, not a horizon heuristic. It admits `4142` at 1,792 of
3,604, and applying it to the already-shipped `49F9` dropped that from 22,664
to 22,622: forty-two calls that had been admitted on a read that was not
provably fresh. Expect more of this as the port moves closer to the interrupt
handlers; phase 3 of the elimination roadmap dissolves it, because native time
and interrupts leave no snapshot to go stale.

*`JSR` writes its return address before it charges its cycles.* Nearly every
store lands on its instruction's final cycle, because the core's addressing
macros are `GetXX(EA); ADDCYC(n); LastCycle(); WrMem(...)`. `JSR` is
`PUSH; PUSH; ADDCYC(6); LastCycle()`, so its two stack writes land on the
instruction's **first** cycle — 21 master cycles earlier than the same-shaped
`PHP` two lines away in the same file. The first slice got this wrong and
nothing caught it: the state census passed, the device-write trace passed, and
both Release and Debug passed. What caught it was widening the work-RAM trace
range to include the stack at `$21E0`-`$21FF` and diffing against a
pure-assembly run, which showed a constant +21 cycle skew in every write from
frame 148 of 3,600 onward.

*`PLP` defers the mask, and only the mask.* `CLI` and `SEI` both apply their
flag after the core has sampled the interrupt, so their checkpoint publishes
the old mask and the change lands on the next one. `PLP` has the same shape —
`ADDCYC(3); LastCycle(); P = POP()` — and the obvious generalisation, defer
the whole popped byte, is wrong. The sample decides *whether* the interrupt is
taken and needs the old mask; but if it is taken, the core pushes `P` after
the pop, so the checkpoint has to carry the popped flags already. The
interpreter's `PLP` at `46AE` restores a carry set eighty instructions
earlier, and the timer interrupt landed on it twice in 3,600 frames, pushing
`$00` where the machine pushes `$41`. The correct split is to publish the
popped flags with the *old* mask bit.

*A leaf transcript cannot express every routine.* `457A`'s `$82`/`$92` command
pops four bytes and jumps to `$40EE`, discarding the routine's own frame. A
transcript that must stop in front of an `RTS` has no way to say that, so
those 45 calls decline; 35 more decline on command handlers this route never
reaches, which were left untranscribed rather than written blind. Both would go
away if `45:4015` were ported, because at that level the escape is ordinary
internal control flow — it is a property of the hybrid's contract, not of the
code. The tick turned out not to be portable, so they stay.

That comparison is now the standard shape for this kind of slice, and the
stack belongs in the range — it has now caught both of the cycle-level faults
above, on the strength of a handful of bytes each. The regression compares
three artifacts against a **pure-assembly run of the same route**: 5,458,568
work-RAM writes covering every voice record, the zero-page command pointer,
the driver's per-channel state and the stack; 699,251 PSG and timer writes;
and the census (`9036a9de...`). All byte-identical. The trace range was
widened for this slice as well as the hook list, because the sample trigger
writes nine driver bytes the previous ranges did not cover. Regression:
`dc_rom_music_sequencer_semantic`.

The other thing this slice is worth remembering for: after landing `4228`,
the instruction profile showed it never reaches the sample trigger on this
route, so about two hundred lines of the shared tail were still unexercised.
`457A` had to follow immediately rather than shipping unverified code. Check
what the route actually ran, not only that it matched.

Previous slice: **the timer sound driver at `00:FC00` is
native**, taking the deployed registry to **36.22%** of all master cycles
(467,299,485 of 1,290,049,560) across 2,159,816 native calls and 284 admitted
routines. This was the first slice outside gameplay logic: the scope widened to
sound and video deliberately, so that the emulator dependency can eventually be
removed entirely rather than only the game-specific parts.

The handler is the game's PSG sample player. It is a flat state machine over
`00:FC00`-`00:FE48` with no subroutine calls: three channels walked at X = 4,
2, 0, each streaming five-bit DDA samples out of a sequence bank mapped into
MPR6 and pushed through `$0806`. Earlier screening had filed this region as
"timer IRQ and sound driver (`00:FC00`-`FDFF`, bank `45`)" and rejected it for
driving hardware registers. Both halves were wrong: bank `45` is the *music
sequencer*, a separate unported component at `$4000`-`$4CB8`, and the hardware
registers turned out to be a bounded, cheap channel to open.

Three things were needed, all reusable and all written up in
[docs/NATIVE_PORT_WORKFLOW.md](docs/NATIVE_PORT_WORKFLOW.md):

- The transaction write channel now carries device registers as well as work
  RAM: the PSG file `$0800`-`$0809`, the timer `$0C00`-`$0C01`, and the
  interrupt controller `$1402`-`$1403`. Unlike the VDC ports these steal no
  cycles, so a transcript prices their stores like any other instruction.
- `--bus-write-range` / `--bus-write-trace` were added, because device writes
  are invisible to a work-RAM census. The state census would catch a wrong
  value reaching `$0806` on its own, since it hashes the audio, but only as a
  changed hash somewhere in 3,600 frames; the bus trace localises it to a
  single write, value, cycle and instruction. The regression asserts all
  954,241 PSG, timer and interrupt-acknowledge writes byte-identical to
  assembly.
- `CLI` opens the interrupt window one instruction late — the core samples the
  interrupt before `OP_CLEARF` clears the flag — so the `CLI` checkpoint must
  publish the *old* mask. Publishing the new one let a nested timer interrupt
  arrive nine master cycles early, which was the entire difference between a
  matching and a non-matching census.

The walk stops at the epilogue heads `00:FC7C` and `00:FD70` (plus `00:FC8C`
for a reentrant interrupt) rather than running to the `RTI`. From the `CLI` at
`00:FC32` the handler is interruptible, and its epilogue reads `$DA`, the rerun
request a preempting handler sets; a transcript is computed once from one
snapshot and cannot observe that write, so the handler would never take its
`JMP $FC03` chain exit into the sequencer.

Hooked alone the driver is admitted at 254,434 of 254,434 entries. With the
whole registry deployed it is admitted 127,455 of 254,990, 50.0% — the rest
arrive while another checkpointed transcript is suspended, and the core does
not consult the replacement hook while one is active. **That no-nesting rule,
not any unported code, is the largest single lever left on the percentage:
104,378,772 cycles.** It is a limit of the hybrid harness rather than of the
port, and the decision below is to leave it alone. Evidence: Release census
`9036a9de...` and Debug
census `a95d0c34...` unchanged, device-write trace `fec8e78f...`, channel-record
write trace `c4195b4e...`, regression `dc_rom_sound_timer_driver_semantic`.

One caveat worth carrying forward: the walk implements every address in the
handler, but the census route only executes 168 of them. The track-advance tail
`00:FDF4`-`00:FE48`, the portamento branch at `00:FCB6`, the hold path at
`00:FCDC` and the two V-flag arms are transcribed from the disassembly and
never exercised, because the route silences an expiring channel at `00:FDE1`
rather than advancing it. They are not proven by any census. A route that plays
looping music would exercise them and is the obvious next thing to census.

**Decision: do not "fix" the replacement no-nesting rule.** The driver runs
emulated on 48% of entries because the core gates the replacement hook on
`!ReplacementActive` and a timer interrupt usually preempts another suspended
transcript. Making that nest would mean adding a replacement-frame stack to
`HuC6280` — emulator scaffolding that exists only for the hybrid transition
and gets deleted with the emulator. It ports no new instruction: that code is
already native and already proven. It would raise the headline percentage
without advancing the goal, so it was deliberately left alone. The native path
is fully exercised anyway by the isolated hook run, which admits 254,434 of
254,434 with a byte-identical census.

That prompted a broader look at what removing the emulator actually needs,
written up in [docs/EMULATOR_ELIMINATION.md](docs/EMULATOR_ELIMINATION.md).
Two things from it are worth knowing here. The native-share percentage tracks
only one of three axes — CPU code, device state, and time — and will plateau
well short of "Beetle is removable", because a native routine today still
writes into Beetle's PSG and VDC and runs on Beetle's clock. And the
acceptance test for the endgame already exists: the state census hashes the
framebuffer and the audio as well as work RAM, so a Beetle-free `PceRuntime`
is finished when `--record-state-census` still yields `9036a9de...`.

Previous slice: the camera-visibility sweep at
`00:F4F2` is native, taking the deployed registry to **30.41%** of all master
cycles and 81.4% of the portable pool. `00:F4FC` -- which decides whether bit 7
of a slot's attribute byte, the bit both `01:C510` and `08:407C` gate on, is
set this frame -- has been native for a long time; what was left was the loop
around it, and it took the same shared-walk treatment as the SAT driver. Four
entry points: the initializer at `00:F4F2` and the three `RTS` instructions the
child returns through (`00:F523` off-camera clear, `00:F545` inside-camera
clear, `00:F54E` below-camera set). One new wrinkle: `00:F4FC` is also called
from outside this loop, fourteen times on the route, and those arrivals reach
the same three `RTS` addresses with a different frame under the stack pointer.
The walk reads the frame and declines rather than assume the caller, which is
why this is the first of these walks admitted at less than 100% -- 66,432 of
66,446. Its regression traces the attribute bytes at `$3080-$30FE` as well as
the stack, because they are the sweep's actual product: 1,125,975 writes
matching assembly exactly.

Four whole loops are now native instruction for instruction -- the actor loop,
the collision sweep, the camera sweep, and the SAT submission. Outside the
frame-sync spin, 94% of what the machine still emulates is the sound driver;
the entire rest is 17,195,439 cycles, 1.33%, with no single address above
0.14%. The remaining bank-`00` clusters were screened and are out of scope:
`00:E93C-EBD6` (20.6M) is VCE palette DMA, `00:E3C0-E47C` (7.0M) and
`00:F25F-F452` (5.5M) are VRAM/tilemap uploads, and `00:E0EB-E232` (11.4M) is
the VBlank handler whose pad reader writes the hardware page at `$1000`,
outside the proven write channel.

Previous slice: the satellite-attribute submission driver
at `08:4036-40C2` is native, taking the deployed registry to **30.07%** of all
master cycles and 80.5% of the portable pool. It had been rejected twice
before, for a real reason: it calls `08:40DF` twenty-one times a frame at ~944
cycles each, so a whole-routine transaction would blow past both the 512-write
cap and every interrupt horizon. The way through was to stop treating a
replacement as covering a routine. The hook fires at every instruction where no
replacement is active, so what matters is the set of addresses control
*arrives* at between transactions -- here the head at `08:4036` and the four
addresses a `JSR $40DF` returns to (`08:4063`, `08:4075`, `08:4088`,
`08:4097`). One shared walk starts at any of them and runs until the next
`JSR`, which the machine executes and which re-enters the native emitter. It is
shared rather than five transcripts because the stopping points chain: draining
one queue empty falls into the next, so an arrival at `08:4063` can end
anywhere from `08:4060` to `08:40C3`. Entry counts change accordingly --
`08:4088` sees 3,492 arrivals alone but 2,567 deployed with the others. Three
modelling notes: the walk shadows the two queue cursors it both writes and
re-reads; `PHY` charges its cycles before it pushes, so its write lands at the
instruction's end, the opposite of the `JSR`/`BSR` convention below; and a
sprite count of `$40` or more skips the tail clear, which this route never
produces, so the walk stops at `08:40A6` rather than run an unexercised
transcript. Bank `08`'s SAT path now runs natively from its first instruction
to its last, leaving only the four `JSR` instructions and the closing `RTS`.
Everything still emulated outside the spin loop and the sound driver is
21,582,381 cycles, 1.67% of the machine, with no single address worth more than
0.14%.

Previous slice: the main actor loop and the collision
sweep are now native instruction for instruction. `01:C51E` (`BSR $C52C`),
`01:C520` (`JSR $C606`), and `01:C654` (the queue filer's `RTS`) close the
11.4M cycles of pure stack traffic that were the loop's whole remainder, and
`01:CB61`/`01:CB6B` do for the collision sweep what `01:C523` did for the actor
loop -- the `01:CB48` walk absorbs the tail for every slot it rejects, but a
slot that reaches the vertical scan at `01:CC1A` leaves the transaction there,
so the branch and advance that follow are separate arrivals. All five run every
arrival: 171,734 each for the linkage trio, 84,735 and 400,892 for the sweep
tail. The deployed registry moves from 28.22% to 29.49% of all master cycles
and from 77.0% to 80.4% of the portable pool.

The linkage transcripts settle a question the contract's own comment had left
looking closed. "The RTS itself remains emulated, preserving stack and
caller-return behavior" describes the convention for a whole-routine
replacement, not a limit on transcripts: `output_registers` carries the stack
pointer, the stack page is ordinary work RAM, and the nested collision
compositions had been writing `JSR` frames all along. Pricing them needs three
facts from the core -- `JSR` and `BSR` push before the first `ADDCYC`, so both
stack bytes carry the instruction's *start* offset where a read-modify-write
carries its end; `JSR`/`RTS` are 21 master cycles and `BSR` is 24; and
`RMB`/`SMB` cost 21 rather than the 18 an ordinary zero-page read-modify-write
costs. The regression scopes its trace to `21FC-21FD`, the stack bytes the loop
uses at its usual `$FD` stack pointer, so a mismodelled push order, value, or
timestamp shows up directly in 1,068,240 traced writes; a wider `21F0-21FF`
trace also matches assembly exactly at 3,722,042 writes but is too large to
keep. Release census `9036a9de...` and Debug census `a95d0c34...` are unchanged
from the assembly baselines, for each hook alone and for the whole registry.

Also corrected on the way through: bank `45` is the PSG sound driver, not a
video bank. It only runs in interrupt context, mapped in by the `TAM #$40` at
`00:FC10`, and on a 384KB image Mednafen maps physical banks 64-127 to ROM bank
`((x & 0x0F) + 32)`, so `45` is ROM bank `25` rather than a mirror. Folding its
46,078,773 cycles in with the timer handler's 249,525,441 makes the sound
driver 91% of everything still emulated outside the frame-sync spin loop.

Previous slice: `01:C523` (actor-loop tail) and
`08:40C3` (satellite attribute-table tail clear) are now instruction-boundary
transcripts and run every arrival -- 446,976 and 4,334 hooked on their own --
taking the deployed registry from 26.15% to 28.22% of all master cycles,
77.0% of the portable pool. `01:C523` closes the ~8.2M-cycle ceiling the sweep
slice recorded as structurally emulated: the `01:C510` walk absorbs that tail
for every slot it skips, but a dispatched slot leaves the transaction at the
handler, so the increment after the handler returns is a separate arrival.
`08:40C3` carries a 48-iteration budget because eight VDC port writes an
iteration reach the core's 512-write cap before `Y` does, and resumes at its
own loop head; it touches no work RAM, so the state census -- which hashes the
rendered framebuffer every frame -- is its regression. The Release census
(`9036a9de...`) and the Debug census (`a95d0c34...`) are byte-identical to
assembly. What is left of the actor loop is 11,508,888 cycles of pure stack
traffic: two `BSR`/`JSR` pairs and the two `RTS` instructions the contract
leaves emulated.

Current owner: Claude. Current slice: the actor dispatch pair, `01:C52C`
(jump-table vector load) and `01:C606` (sprite-queue registration), are now
instruction-boundary transcripts and run every arrival -- 171,734 each on the
3,600-frame state-census route -- taking the deployed registry from 23.60% to
26.15% of all master cycles, 74.3% of the portable pool. They were found by
ranking a `--replace-registry` instruction profile with the registry entry
rows dropped, which is the only reading of that profile that means "still
emulated". `01:C52C` resumes on the first instruction of whichever handler the
jump table names, so its `return_instruction` is computed per call rather than
fixed. The Release census (`9036a9de...`), the Debug census (`a95d0c34...`),
and both routines' scoped work-RAM write traces are byte-identical to
assembly. What is left of the actor loop is the six instructions of stack and
cursor bookkeeping around each dispatch, plus the `RTS` the contract leaves
emulated.

Current owner: Claude. Current slice: the two bank-01 sweep heads,
`01:C510` (actor loop) and `01:CB48` (collision sweep), are now
instruction-boundary transcripts and run every arrival -- 176,861 and 87,968
on the 3,600-frame state-census route -- taking the deployed registry from
21.35% to 23.60% of all master cycles, 67.1% of the portable pool. Both had
looked like partial coverage (136,509 of 321,839 and 89,762 of 260,683);
counting the declines showed all of them were the atomic interrupt-horizon
floor, with zero classifier, decimal, counter, or overrun rejections, so
neither needed new game logic. The Release census (`9036a9de...`), the Debug
census (`a95d0c34...`), and both routines' scoped work-RAM write traces are
byte-identical to assembly.

Current owner: Claude. Current slice: semantic `08:40DF@all` replaces the
satellite sprite attribute-table emitter for all 73,818 calls on the
3,600-frame state-census route, taking the deployed registry from 13.75% to
21.35% of all master cycles -- 60.7% of the portable pool once the emitter's
own 100,468,839 cycles join the gameplay pool in the denominator. It is a
full instruction-boundary transcript: 21 prologue checkpoints and up to 61
per emitted entry, so interrupts are sampled and delivered where the original
samples and delivers them, and the core charges the VDC's stolen wait cycle
once rather than twice. The Release and Debug censuses, all 73,818 call
records, all 1,477,731 scoped work-RAM writes, and all 4,205,700 interrupt
stack writes are byte-identical to assembly. A zero entry count, a cursor
overflow, and a bit-five attribute-high merge remain declined by the
classifier and assembly-backed.

Current owner: Codex. Current slice: semantic `03:73A6@all` now replaces all
64 compatible collision-free, one-pass, no-hit Knight movement calls on the
controlled 129-call route, including timer-decrement, timer-reload,
phase-wrap, and checkpointed `73D8`/`73AF` interrupt boundaries. Collision/
multi-pass and downstream hit-helper records remain assembly-backed;
`03:73D8@all` now supplies both the nonterminal 96-cycle `7BF6` handoff and
the terminal 75-cycle carry-set prefix, whose `03:73FE@all` successor owns
the exact 51-cycle 100-unit banked score handoff, whose `03:740A@all`
successor owns the exact 51-cycle queue acknowledgement prefix, and whose
`02:41A0@all` common-helper entry owns the exact 48-cycle scratch-clear
  prefix, whose `02:41A8@all` successor owns the exact 21-cycle command setup,
  whose `02:41AD@all` successor owns the exact 123-cycle saved-index/table-
  coordinate setup and resumes original `02:41C2`, whose successor owns the
  exact 21-cycle `$41F5` call frame, whose terminal-only child successor owns
  the exact 48-cycle `$2D:$2E` pointer-copy prefix and resumes at `02:420B`,
  whose successor owns the exact 21-cycle `$C024` call frame, whose child
  successor owns the exact 12-cycle jump to `$C5AB`, whose successor owns the
  exact 6-cycle scan initializer, whose terminal-only `01:C5AD` successor
  owns the exact 15-cycle slot-four occupancy read and resumes at the original
  branch, whose terminal-only `01:C5B0` successor owns the exact 6-cycle
  nonzero not-taken branch and resumes at the original scan increment, whose
  terminal-only `01:C5B2` successor owns the exact 6-cycle increment and N/Z
  update and resumes at the original range comparison, whose terminal-only
  `01:C5B3` successor owns the exact 6-cycle limit comparison and resumes at
  the original loop branch, whose terminal-only `01:C5B5` successor owns the
  exact 12-cycle taken loop branch and resumes at the shared scan read.
Its later `03:7418@all` successor owns the exact 36-cycle release/scheduler
handoff.
The terminal `03:7420@all` successor composes its 33-cycle banked wrapper
with the exact 159-cycle coroutine scheduler, preserving its seven-write
terminal dispatch.
The Tower's armed
final-gate prefix `03:60F9@all` now
replaces only fixed slot `$2D` with qualifying latch `$2CFC=1` and final gate
`$2D4D=1`. Its exact 54-cycle transcript preserves A across the decrement,
commits the zero gate at cycle 42, and branches to original `03:612B`. The
scheduled no-hit continuation `03:6136@all` now replaces all 245 observed
calls through the exact 21-cycle indexed-load/not-taken-branch transcript,
resuming at original `613B` for `RTS`. Its cycle-15 checkpoint preserves the
frame-3413 timer IRQ's stack return PC `6139`. A real geometry-delivered
Tower collision now also runs `6136`'s exact 75-cycle sign-bit prefix: it
sets `$3C80=1` and hands off at original `01:C021`, retaining the shared
timer/audio/scene dispatcher in assembly. The dedicated Release collision
regression has byte-identical census, call, ordered-write, and selected-state
artifacts, while the existing BS-5 entrance census, trace, call,
ordered-write, and selected-state artifacts remain byte-identical. The
related Debug family audit repeats both controlled routes: the original
no-hit route retains its 1/1 and 245/245 native counts, and the real collision
route has 1/1 terminal and stage-selection entries. Both retain exact
original-vs-native census, trace/call, ordered-write, and selected-state
hashes. The Giant Door's formerly unprofiled sign-bit collision branch is now
bounded by a real geometry route: a ball placement at frame 1,228 naturally
delivers `$384C=$90` twice (frames 1,316 and 1,338). Semantic `03:62B8@all`
executes the exact 48-cycle collision-clear and 50-unit score-load prefix,
then resumes original `03:62C4` for the alias-sensitive score operands. All
2,373 calls in the controlled Release route execute natively, and its owning
regression preserves byte-identical census, call, ordered-write, and
selected-state artifacts. A second placement at frame 1,340 naturally delivers
the third collision at frame 1,372; semantic `03:62CB@all` owns all three
counter-decrement branches, resuming the first two at original `03:62D0` and
the terminal branch at native `03:62D4`. Its exact 57-cycle 50,000-point
argument/stack prefix hands the original score helper back to assembly; the
later destruction, Knight release, and completion handshake remain
assembly-backed. The matching Debug
original-versus-native completion route preserves the same four artifacts
exactly.
The Walled Star's post-destruction dispatcher `03:638F@all` now additionally
owns its fixed-slot clear-latch leaf. With both `$2D53` and `$2D32` zero, it
replays the 42-cycle two-load/two-branch prefix and hands the original RTS at
`03:6399` back to assembly. The acknowledgment, animation, and bonus-routing
branches remain assembly-backed: a controlled `$2D53=1` entry takes one
912-cycle, 20-write transition and reaches 878 mixed `01:C018` descendants
with 262,468 aggregate writes, so it is not a composable parent prefix.
The ordinary session drain threshold `01:DD6E@all`
now executes natively for the live slot-0 Y comparison against `$02D0`.
The sibling zero-ball branch at `01:C7E5@all` now composes its exact 27-cycle
`$2D52` load/taken branch with the native `01:C7F4` 318-cycle scheduler setup,
forming the exact 345-cycle, thirteen-write outer transaction and resuming at
`01:C5E9`; the following scheduler-driven game-over body remains assembly-backed.
All 1,744 session-route calls retain byte-identical Release census, call,
and ordered-write artifacts; the exact-threshold drain at frame 3,000 also
retains its 78-cycle carry-set return. The ordinary remaining-ball
presentation `01:C80A@all` is also native: it installs its 16-tick
coroutine, consumes its first tick, and returns through the exact 318-cycle,
thirteen-write transcript. Its parent `01:C7EA@all` now reads the zero shared
transition latch and composes that child after the exact 27-cycle branch,
covering the 345-cycle ordinary continuation while deferring nonzero-latch
transitions. Outer decision `01:C7E5@all` now adds the nonzero remaining-ball
test and its exact 21-cycle fall-through, completing the observed 366-cycle
ordinary branch while deferring zero-ball and alternate-latch paths. Both are
now bounded, composed scheduler transactions; their later bodies remain
assembly-backed. The
alternate nonzero-`$55` session threshold, the preceding transition/ball-count
decisions, and later table-reload and panel paths remain assembly-backed
pending their own bounded transcripts. A follow-up assembly-only profile
retained the nested continuation observers and established that preceding
`01:C786`, `01:C7B0`, and `01:C7BF` are not prefix-only leaves: their ordinary
paths enter shared `00:ECFD` before the `C5EA/C5C9` coroutine handoff and
carry 83/114/114 scoped writes, including interrupt-owned work. They remain
assembly-backed until that helper has a composable transcript or a local
natural branch is captured. The panel's dominant `09:72FD` form is likewise
kept emulated because its otherwise small transcript writes VDC ports
directly, outside the work-RAM gameplay transaction boundary. A fresh
standalone screen likewise rejects `01:C5EA`: its stack-rewriting delay
installer continues into the installed coroutine, so 73 ordinary-route
observer scopes range from 831 cycles/22 writes to 423,162 cycles/11,369
writes. Preserve the exact inline `C5EA` compositions already used by native
parents, but keep the direct boundary assembly-backed. The remaining
`00:ECFD` dependency has also been re-profiled: it is the mapped audio driver,
not a gameplay helper. Its 71 controlled-route calls span 314,442 cycles and
7,869 writes over 16 command IDs, including the session's `$22` and `$27`
commands; retain it and its `C786/C7B0/C7BF` callers in assembly. The Giant-Face
mouth closed/no-hit gate
`03:7726@all`, mirrored helmet controller `02:4C91@all`, Four-Star
Wall-Pocket controller `02:4A93@all`, and the two Upper Wall-Pocket hand
initializers `02:5972@all`, traced live timer setup `02:5975@all`, delayed
timer `02:5991@all`, its trace-backed `$0001`-to-zero scheduler handoff, and
the scene-clear opening continuation `02:59A5@all`, its positive-state
follow-up `02:59C7@all`, and the actual scheduled opening expiry
`02:59E0@all`, its actual post-opening scheduler `02:5A08@all`, and its
post-opening state `02:5A0B@all`, and its scheduled post-opening countdown
`02:5A1B@all` are complete, including the `$2D33` dormant guard at
`02:59A5@all` and the negative-state close loop at `02:59C7@all`. The
Four-Star Wall-Pocket controller also has its collision gate plus captured
ordinary and matching-arrow access decisions at `02:4A93@all`; matching
continuations `02:4AD2@all` and `02:4AE5@all` now cover its post-audio score,
opening timer/appearance, and scheduler handoff. The shared audio and
appearance helpers remain assembly fallback. Its scheduled `$4AEB` animation
continuation now executes natively for the observed countdown and
phase-advance paths; the measured appearance-six scene transition remains a
942-cycle, 22-write assembly fallback. The next bounded candidate
should be selected from a new profile observer set rather than assumed from
this closed slice. The Walled-Star access controller `02:4B3F@all` now adds
its 905 collision-free four-tick calls to the native set; its collision score,
opening, wall handshake, and bonus-stage selection remain assembly fallback.
The sibling Screen-2 Side-Star access controllers `02:4BD5/4C0A@all` add
their 1,277/905 collision-free calls through the same timer/appearance tail;
their score, Giant-Face transformation, and upper-right bonus-stage entrance
branches remain assembly fallback. A controlled lower-left Side-Star collision
at frame 1,210 spans 1,902 cycles and 44 writes across the score/effect,
Giant-Face request, animation, and scheduler sequence, so its continuation
also remains assembly-backed. A controlled Walled-Star access collision
at frame 1,220 enters a 1,182-cycle, 31-write allocation/score/animation/
scheduler transaction, so it remains assembly-backed rather than becoming a
parent composition target. A controlled upper-right Side-Star collision at
frame 1,220 starts the distinct persisted-form-zero bonus-stage-6 entrance
as a 1,188-cycle, 30-write score/animation/scheduler transaction, so that
collision transaction remains assembly-backed. Its eight subsequent
`02:4C33@all` timer/appearance calls now execute natively through exact
60-cycle decrement and 96-cycle reload/advance transcripts; only the ninth
stage-selection transition remains assembly-backed. A fresh comparison profile now selected
the Knight collision helper `03:73D8@all`, and the fresh follow-up parent
screen now composes its 10,600 fixed-slot `$3A-$3D` waiting calls at
`03:7378@all` through the exact checkpointed 123-cycle nested-call
transcript. Door activation and terminal Knight destruction remain assembly
fallback; the nonterminal hit's 96-cycle clear/decrement/`7BF6` handoff is
now semantic at `03:73D8@all`. A composed Front-/Side-Spider profile now adds shared helper
`03:65A4@all`: all 5,441 fixed-slot `$26-$29` timer calls execute through
its checkpointed wait/advance/reset paths, while both collision parents stay
assembly fallback. A fresh Release re-screen confirms that enabling the
current `03:6966@all` and `03:65A4@all` children preserves the baseline
census (51,294/54,149 collision entries and all 5,441 appearance entries
native), but the two Spider parents still require a parent-relative nested
interrupt/timing transcript. An inexact no-hit composition was discarded;
`03:6490/651F` remain assembly fallback. A subsequent profile rejected duplicate Walled-Star child
`03:6366` and selected paired Small-Soldier source waits `03:7D85/7E23@all`:
all 4,914 fixed-slot `$34/$35` nonzero-group calls execute through their
checkpointed 21-cycle transcripts, while zero-count completion, deployment,
and child collision behavior remain assembly fallback. The next candidate
should be selected from a new profile observer set rather than assumed from
this bounded family slice. A fresh Sorcerer screen rejected its broad orbit
parent as mixed collision/drawing work but added direct controller color gate
`02:57E9@all`: all 2,648 hidden-slot-`$16` entries run through the checkpointed
42-cycle stable-color return while the mismatch/update helper remains assembly
fallback. The next candidate should again come from a new observer set.
The Tower-insect profile then selected direct animation leaf `03:6054@all`:
all 6,750 dynamic-slot `$64/$67/$6A/$6D` calls now execute through its
checkpointed frame-table, timer, and phase-advance transcripts, while mixed
parent `03:6016` and its collision/scoring/lifetime paths remain assembly
fallback. Its corrected physical-bank sibling screen then selected fixed-source
gate `02:5F5F@all`: all 2,504 slot-`$2C` nonzero-child-count returns execute
through their checkpointed 21-cycle transcript, while source countdown,
spawning, and group-clear remain assembly fallback. Their combined Debug
family checkpoint now passes with both native leaves enabled.
The next Small-Soldier child-deployment screen rejects `03:7EFA` as a small
mixed collision/scheduling parent whose major children already have native
coverage. A fresh Three-Blobs movement screen instead selects direct
appearance/collision gate `03:67B3@all`: all 1,965 fixed-slot `$58-$5A`
entries now begin natively. Its 1,950 collision-free calls execute the
checkpointed 93-cycle color lookup and appearance-store path; the 15
collision-active calls execute the exact 66-cycle prefix then hand off at
the original collision dispatcher, leaving its scoring/audio/state path in
assembly. Both mixed movement parents remain assembly fallback. The owning
Release replay and original-artifact hashes pass; the next candidate should
again come from a new bounded observer set. A Tower-source re-screen keeps `02:5F4A/5F78`
assembly-backed: their 145-countdown/four-allocation scope is too small and
allocation-coupled to justify a parent composition around native `02:5F5F`.
An unrelated Giant-Skeleton screen finds high raw fallback cost at `02:5EEC`,
but every call depends on mixed physical-bank child `01:DAF8` before the
random appearance/collision gate. Both remain assembly until that child has
its own classified, instruction-timed boundary; the measured decision is
recorded in `docs/NATIVE_FALLBACK_HOTSPOTS.md`.
Native-port iterations now follow
`docs/NATIVE_PORT_WORKFLOW.md`:
batch siblings sharing a family route, use Release family parity per slice,
and reserve Debug ROM replays for family or broader checkpoints.
The naturally exercised Little-Skull gray initializer `02:458A@all` now ports
its sole strict slot-`$46` setup at frame 1,221: all eleven ordered writes,
the 291-cycle scheduler transaction, and the hand-off to `02:45A4` retain the
existing Little-Skull RNG route's exact Release artifacts. The gray-release,
RNG, collision, and regeneration continuations remain assembly-backed unless
they meet their own bounded replacement contracts.
The immediate observed gray-release continuation `02:45A4@all` now also runs
natively at frame 1,223: it composes its state restore, odd-RNG pending-bit
selection, and coroutine installation of `02:4515` through the exact
768-cycle, 23-write transaction. Alternate RNG parity, collision, and
regeneration paths remain assembly-backed.
The downstream ordinary Little-Skull continuation `02:4515@all` now ports
all 22 naturally observed entries for live slots `$46`, `$47`, `$49`, and
`$4A`: its three-write 105-cycle wait and ten-write 303-cycle expiry handoff
to the existing coroutine scheduler are exact in Release. The existing
instruction-boundary transcript now preserves the one slot-`$49` timer IRQ
between the position and appearance stores, so no focused calls remain in
assembly. The focused Release semantic regression replays the 303-cycle
expiry branch and freezes the complete census, call, and ordered-write
artifacts.
The active Big-Star pending gate `02:570A@all` now covers 489 of 490 calls in
a controlled visible-window profile. Its pending-left helper `02:5722@all`
runs natively for fixed-slot phases zero through six, and mirrored
pending-right helper `02:5738@all` runs for phases zero through four; all
other helper states remain assembly-backed.
The Devil-Shooter companion re-screen then selected its two 24-cycle dormant
leaves `02:53DD/5456@all`. Their Release-only semantic regression preserves
the full family census/call/write artifacts while replacing 1,855 event-
horizon-safe calls; 285 near-event or timer-IRQ calls conservatively remain
in assembly. The separate fresh sibling profile then selected charge-position
leaf `02:5397@all`: its exact two-write 75-cycle transcript now replaces 873
of 1,260 calls while 387 near-horizon calls remain in assembly. A fresh
expanded companion profile then admitted the variable charge-appearance leaf
`02:5408@all`: 1,015 of 1,288 calls now execute natively through its exact
42/105/114-cycle cached/advance/reset branches, while 273 near-horizon calls
remain in assembly. A new child-level companion screen then selected the
right active timer leaf `02:545D@all`: 219 of 267 calls run natively through
its 33/102/111-cycle wait/advance/reset paths, while 48 horizon calls remain
in assembly. A fresh active-window screen then selected the Big-Star rotating
wait in `02:5674@all`: 199 of 490 controller calls now run natively through
its nested latch clear, timer decrement, and session-bit-clear transcript;
the 291 non-admitted controller calls continue to expose their child gate,
where `02:570A@all` replaces 290 no-pending entries. The remaining Big-Star
timer expiry, drawing, stopped, and direction-helper paths remain assembly.
A later controlled stopped-selector screen confirms `02:56BA` is not a
callable candidate: it is an unentered internal selector-V branch whose
nonmatching case jumps back into the mixed controller and matching case
continues through session/timer writes.
A fresh expiry screen confirms that `02:56EA` and shared `01:C0A5/C0A8` are
original drawing submissions, so their 1.66M weighted units are retained
within the emulated graphics-driver boundary rather than miscounted as
gameplay fallback. A second fresh Giant-Skeleton re-screen keeps
`02:5EEC/01:DAF8` assembly-backed: all 2,139 parent/child pairs remain mixed
coordinate/collision paths with no independently bounded state-derived leaf.
A fresh Devil-Shooter left-active re-screen resolved the earlier all-call
failure: `02:53E4` is a shared entry, so the adapter admits only fixed slot
`$0A` and rejects non-left contexts plus user/timer-IRQ horizons. Its normal
`02:53C8` parent composes 180 of 266 fixed-slot, nonzero-flag controller calls
through exact 99/168/177-cycle transcripts; the leaf handles 34 of the
remaining 92 direct entries. Mirrored `02:5441` now applies the same bounded
composition for 174 of 267 right-controller calls, leaving `02:545D` to cover
45 of 93 direct leaf entries. In each case the remaining alternate or
horizon-bound paths retain assembly execution. A fresh Three-Blobs second-phase
re-screen also keeps `03:66D3` assembly-backed: all 720 calls are complete
1,008--24,168-cycle motion/collision/scheduler transactions with 24--422
writes, rather than independently returnable prefixes. The next candidate
should be selected from a new cross-family bounded observer set. That screen now
admitted the upper-chute Gray-Blob initializer `03:6876@all`: its exact
372-cycle, 15-write typed setup transcript now runs natively for all seven
Release layouts. Its final authentic `01:C5E6` interrupt-sampling checkpoint
preserves the original scheduler timing for the four calls formerly blocked
by the conservative pre-entry horizon. The owning semantic regression
preserves the complete census, call, and ordered-write artifacts. The same
natural upper-chute route now also admits all seven first-live `03:68A1@all`
prefixes: each exact 462-cycle native transaction clears the initial latches,
composes RNG, stores its movement timer, and hands off at original `01:C018`.
The complete 633-cycle/20-write calls retain their byte-identical Release
census, call, and ordered-write artifacts; later live movement, collision,
hit, impact, and score behavior remains assembly-backed. The same next family
extension now ports the Giant-Skeleton-Eyeball setup `02:4DC8@all`: its sole
fixed-slot frame-872 invocation runs through the exact 258-cycle, ten-write
script transcript and final interrupt boundary alongside all 1,988 native
`02:4DD9@all` dormant calls. The same lower-table family now adds all three
Mercy-Star setup calls at `02:4F9F@all`: fixed slots `$5F-$61` execute the
exact 318-cycle, eleven-write dormant-state/script-installation transcript
with the final `01:C5E6` scheduler checkpoint, alongside the existing 7,950
native `02:4FC1@all` dormant-gate calls. Activation, scoring, transport, and
release remain assembly fallback. The same fixed Devil-Shooter family now
adds initializer `02:50EE@all`: its sole `$0E` setup call runs through the
exact 351-cycle, 15-write timer/position/appearance/script transcript with
the final `01:C5E6` scheduler checkpoint. Collision/catch, charge, and launch
continue through their existing bounded native or assembly paths. The
adjacent direct retraction leaf `02:533B@all` now covers the fixed Shooter's
four-/five-store charge decay from both catch and launch callers, while its
mixed launch parent `02:5208` remains assembly-backed. The adjacent
Three-Blobs layout initializer `03:664A@all` now ports all three fixed
`$58-$5A` calls through the exact 396-cycle, 16-write state/appearance/script
transaction and final `01:C5E6` scheduler checkpoint. Alternating movement
and collision behavior retain their current bounded native-or-assembly split.
The fixed Dragon's Head layout initializer `02:5A31@all` now likewise ports
its sole `$13` setup call through the exact 291-cycle, eleven-write
appearance/collision/script transaction and final `01:C5E6` scheduler
checkpoint. Its eight-hit score/audio gate and mouth-opening paths retain
their existing native-or-assembly split. The adjacent Giant Skeleton layout
initializer `02:5EE4@all` now ports its sole `$30` setup call through the
exact 213-cycle, eight-write appearance/script transaction and the same final
`01:C5E6` scheduler checkpoint. Its collision and animation controller at
`02:5EEC` remains assembly-backed because it is still mixed with its
physical-bank child `01:DAF8`. The corresponding Three-Blobs controller
initializer `03:65BC@all` now ports its sole fixed `$57` setup call through
the exact 207-cycle, eight-write automatic-safety-clear and scheduler
transaction. The equality, scoring, and randomized-reset paths at `03:65C2`
retain their existing native-or-assembly split. The fixed Giant Door layout
initializer `03:6294@all` now ports its sole `$4C` setup call through the
exact 345-cycle, fifteen-write handshake-reset/counter/script transaction and
final `01:C5E6` scheduler checkpoint. Its three-hit collision, score, and
destruction paths at `03:62B8` retain their existing native-or-assembly split;
the geometry-delivered first-hit prefix resumes at original `03:62C4`.
The adjacent Walled Star layout initializer `03:6319@all` now ports its sole
fixed `$5B` setup call through the exact 321-cycle, twelve-write wall-state,
collision-selector, and scheduler transaction with the final `01:C5E6`
checkpoint. Its wall-damage, scoring, and access paths at `03:6339` retain
their existing native-or-assembly split.
The Walled-Star access pocket initializer `02:4A44@all` now ports its sole
fixed `$11` setup call through the exact 273-cycle, nine-write
appearance/collision/script transaction and final `02:4CE4` checkpoint. Its
collision, score, opening, and random bonus-stage transition retain the
existing `02:4B3F` native-or-assembly split.

The scheduler-selected active Side-Worm continuation `02:5D4D` was re-screened
after the new `02:5D4A@all` timer initializer. Its 39 calls alternate between
an apparent 69-cycle, two-write turn that still invokes shared `01:C07E` and
longer turns that immediately enter the mixed `02:5DFC` coordinate/collision
helper. It remains assembly-backed; no safe standalone continuation exists at
that boundary.

The appearance-six terminal call of the native Side-Star entrance animation
`02:4C33@all` was also re-traced. Although its enclosing scope is only 309
cycles and ten writes, its `01:C018` path reaches `01:C5C9` after 150 cycles
with changed registers and enters a 1,926-cycle mixed scheduler/controller
scope. It remains the intentional ninth-call assembly fallback rather than
an invalid direct composition of the ordinary coroutine scheduler.

A fresh Mercy-Star last-ball screen sets only `$2D52` to one at frame 1,200.
The three fixed `02:4FC1` activations immediately span 471, 1,128, and 471
cycles with mixed release/presentation/scheduler work; their downstream
`01:C018` scopes range from 822 to 3,333 cycles. The later selected
`02:50C1` continuation is a 5,625-cycle, 133-write animation/state scope.
Mercy-Star activation, award, transport, safety, and release remain
assembly-backed; the existing native dormant gate is the correct boundary.
The separately forced Giant-Face mouth-open state `$2D21=1` reaches
`03:7726/7749` once at frame 1,213. Its 27-cycle gate prefix joins a
1,122-cycle, 40-write capture/score/actor-script transaction, whose
`01:C5C9` descendant takes another 1,395 cycles and 27 writes. It does not
reach the later `03:7821/7834` RNG-release route from this state alone.
Open-mouth capture remains assembly-backed; the closed/no-hit gate remains
the correct native boundary.
The complementary closed-mouth collision probe sets only `$3838=$80` for
fixed slot `$38`. Its frame-1,213 `03:7726` call reaches `03:7730` after the
42-cycle gate prefix, then spans a 15,414-cycle, 236-write shared
score/impact/audio and actor/display transaction (15,456 cycles for the
enclosing gate). A later `01:C5C9` scheduler entry occurs 64,023 cycles after
the gate and takes 1,896 cycles with 36 writes. Collision hits remain
assembly-backed; the native contract is still only the closed/no-hit leaf.
The Giant-Face wake probe sets only `$2D0C.0` at frame 1,200. Fixed slot
`$36` reaches `03:74D8/74E0` once for a 261-cycle, nine-write
mode-one/script-installation transaction; first active animator `03:767E`
arrives at frame 1,222, followed by a 3,675-cycle, 77-write `03:76C5`
drawing/animation scope. A guarded exact-write replacement trial diverged on
frame 1,200, so the scheduler transfer remains an assembly boundary. The
native request-free `03:74D8` leaf is unchanged.
The natural Dragon-Egg hatchling in dynamic slot `$64` was also re-screened.
Its four ordinary `02:441E` updates span 1,164--1,986 cycles and 22--38
writes through the shared collision/drawing helper; the fifth is a
19,965-cycle, 384-write score/impact/release transaction returning through
`01:C594`. No standalone timer leaf remains, so the hatchling controller
continues through assembly after the existing Egg-owned prefixes.
The Giant Door active-hit probe confirms that `$384C` is a collision-pipeline
surface rather than a durable request byte: controlled `$80` and
selector-preserving `$90` patches around its frame-1,229 dispatch are both
normalized to the normal `$10` selector before `03:62B8` reads them. All 676
observed calls retain their native 27-cycle idle leaf, while the score,
three-hit counter, and completion handshake must wait for a real collision
delivery or a later pipeline injection point. No invalid parent composition
was added.
The mirrored Giant-Face helmet collision was then captured by setting either
slot `$2A` or `$2B`'s true collision bit (`$382A/$382B=$80`) at frame 1,200. Its
frame-1,222 `02:4C91` parent is a 3,171-cycle, 76-write score/audio/wake and
ball-return transaction, so it remains assembly-backed. Its direct post-score
entry `02:4CFB@all` is now native: the exact 396-cycle, 15-write continuation
reloads the active left or right helmet timer, restores slot-0 to `$0086,$0124`
or `$009E,$0124`, sets phase four, and composes the original coroutine scheduler
to install `$4D0B`. Focused Release replays and owning regressions cover both
channels; the existing normal helmet family regression also passes.
The scheduled helmet follow-up `02:4D0B@all` now also natively covers both
channels' exact 60-cycle timer waits and 96-cycle timer reload/phase advances.
The left route admits 92 of 93 entries and intentionally leaves its phase-six
animation transition in assembly; the mirrored right route admits all 353
observed calls. Focused Release replay preserves the initial one-write wait
at frame 1,518 and the complete route artifacts.
The retained left phase-six call was then traced through direct successor
`02:4D20`: at frame 1,614 it consumes 18,474 cycles and 296 writes across
drawing, scheduler, actor, and animation work (18,507 cycles for enclosing
`02:4D0B`). It remains assembly-backed; no further local continuation was
exposed by that transaction.
The mirrored Side-Star layout initializers `02:4A54/4A4C@all` now port the
fixed `$10/$12` setup calls through their exact 273-cycle, nine-write
appearance/collision/script transcripts and final `02:4CE4` checkpoints.
Their existing `02:4BD5/4C0A` collision, score, transformation, and entrance
paths retain their current native-or-assembly split.

The queued-Knight layout initializers `03:734E/7353/735A/7361@all` now port
all four fixed `$3A-$3D` setup calls. Their typed queue-index mapping preserves
the exact 276-288-cycle, eleven-write selector/counter/script transcripts and
the final `01:C5E6` scheduler checkpoint; later release, collision, and
four-hit behavior retains the existing native-or-assembly split.

The shared Side-Spider layout initializer `03:64FA@all` now ports both fixed
`$28/$29` setups through their exact 342-cycle, fourteen-write
side-parameter/collision/script transcript and final `01:C5E6` checkpoint.
The collision, scoring, and transition parent `03:651F` remains assembly-backed.

The shared Front-Spider layout initializer `03:646B@all` now ports both fixed
`$26/$27` setups through the exact same 342-cycle, fourteen-write
side-parameter/collision/script shape, including its Front-Spider-specific
dispatch target and final `01:C5E6` checkpoint. The collision, scoring, and
transition parent `03:6490` remains assembly-backed.

The shared Giant-Face helmet initializer `02:4C78@all` now ports both fixed
channel layouts `$2A/$2B` through their exact 240-cycle, eight-write
script/timer/appearance transcript, returning at the original `02:4CF1` RTS
boundary. Collision-marked entries and all later activation behavior remain
assembly-backed.

The direct mirrored Giant-Face helmet outer initializers `02:4A5C/4A64@all`
now port the fixed selector store plus shared setup as exact 273-cycle,
nine-write transcripts for slots `$2A/$2B`. Their collision-marked states
continue through the complete original wrapper and active path; the new
Release semantic regression independently guards both outer entries.

The four fixed Little-Skull layouts now execute shared initializer
`02:44E4@all` natively. Its pair-constrained adapter preserves the 8-bit
live-count increments and each exact 390-cycle, fourteen-write actor and
coroutine transcript, with a final `01:C5E6` checkpoint. The collision,
regeneration, gray-release, scoring, and Big-Star paths remain assembly-owned
outside their already mapped gates; a dedicated Release semantic regression
guards all four setup calls.

The two Little-Skull pair-controller layouts now execute shared initializer
`02:4485@all` natively. Its pair-constrained adapter clears each live-count
and regeneration byte, schedules the original `02:449A` controller through
an exact 285-cycle, ten-write transcript, and preserves the final
`01:C5E6` checkpoint. A separate Release semantic regression covers both
fixed pair-controller entries.

The Little-Skull hidden transition controller now executes initializer
`02:4455@all` natively. The fixed-slot adapter clears feature latch `$2CFA`
and schedules the existing gate through the exact 207-cycle, eight-write
transcript with its final `01:C5E6` checkpoint. A dedicated Release semantic
regression guards the sole startup entry.

The fixed Big-Star initializer `02:565C@all` now executes natively. Its
slot-constrained adapter preserves the exact pending-bit, stopped-direction,
zero-velocity, and speed-state setup as a 285-cycle, twelve-write transcript
with the final `01:C5E6` checkpoint. The existing direction/rotation gates
and all event-owned behavior remain unchanged; a focused Release semantic
regression guards the startup entry.

The mirrored fixed Side-Worm initializers `02:5C4B/5C7C@all` now execute
natively. Their slot-constrained adapters preserve the left/right state,
selector, collision, appearance, and `02:5CC5` dispatch writes as exact
204/210-cycle, ten-write direct-return transcripts. A focused Release
semantic regression independently replays both startup entries. Their shared
stopped-selector gate `02:5CCD@all` now also covers all 1,760 calls in a
controlled fixed-selector visible-window fixture: 1,739 clean 36-cycle,
zero-write returns and 21 interrupt passages reproduce byte-identical census,
entry, call, and ordered-write artifacts through four exact instruction
checkpoints. Matching-selector activation and all collision, movement,
scoring, and outcome paths remain assembly-owned.

The fixed Undead-Head initializer `03:61A0@all` now executes natively for
slot `$4D`. Its exact 192-cycle, seven-write shared coroutine-scheduler
handoff installs `03:61A3` and returns through the original `01:C5E9`
boundary. Its direct stopped-selector gate `03:61AB@all` now also covers the
controlled visible-window return: all 770 fixed-slot calls preserve the
three-checkpoint 27-cycle, zero-write `LDA`/`CMP`/branch transcript, including
eight original interrupt passages. The selector-zero activation, collision,
RNG, scoring, and bonus behavior remain in assembly; focused Release semantic
regressions guard both the startup and selector gate.

The fixed Devil's-Coffin initializer `03:63E1@all` now executes natively for
its startup and both post-cycle resets. Its slot-constrained adapter preserves
the 249-cycle, nine-write collision-normalization and scheduler transcript,
including direct and coroutine caller stack shapes. A focused Release
semantic regression guards all three observed calls; the Coffin's collision,
opening, closing, and session behaviors retain their existing boundaries.

The six fixed Dragon's-Egg layouts now execute natively at `02:433F@all`.
Their typed `$20-$25` adapter derives the shared `$2CF4` live-count increment
from work RAM and preserves each exact 213-cycle, eight-write scheduler
handoff to `02:4345`, including the caller-relative stack writes. The focused
Release regression guards all six layouts; the group-completion, collision,
score, hatchling, and reset behaviors retain their existing boundaries.

The hidden Dragon's-Egg group-controller initializer `02:42EB@all` now
executes natively for slot `$1F`. Its exact 228-cycle, eight-write setup
clears group-reset byte `$2CF7` and installs the existing `02:42F6`
live-count controller through the original scheduler boundary. A focused
Release regression guards this one-time call; completion, collision, score,
hatchling, and reset behavior retain their existing native-or-assembly split.

Semantic `03:66A3@all` now ports all 1,107 observed Three-Blobs
first-movement calls. The 535 direct no-zero paths write all three live
countdown decrements and resume at `03:66DE`; their exact six instruction
checkpoints preserve both taken and fall-through branch timing, including the
249 formerly event-horizon-adjacent calls. The 272 timer-expiry paths
preserve the decrement, four-tick reload, and phase-bit toggle through
instruction checkpoints, then resume the original mixed motion/collision tail
at `03:66B5`. Forty-five y-expiry paths write the timer/x/y decrements and
resume at `03:66BF` before the original reset/coroutine path. All six x-expiry
paths write the timer/x decrements and resume at `03:66E2` before the original
reset/scheduler tail. The focused Release regression preserves exact census,
call, and ordered-write artifacts across the complete route.

Semantic `03:676B@all` now ports the shared Three-Blobs scene-clear idle leaf.
Both alternating movement phases reach this child after their position work.
When group latch `$2CFD`, scene latch `$2D3C`, and the actor collision latch
are clear, the adapter reproduces the exact 147-cycle path through the shared
color-to-appearance store at `03:67BD` and returns through `03:67E5`. Its
instruction-timed latch and appearance checkpoints now admit 1,950 of 1,965
calls, including formerly event-horizon-adjacent idle calls; suspended, moving,
and collision-active states retain assembly execution. Its focused Release
census, call, and ordered-write artifacts are byte-identical to assembly.

Semantic `03:66DE@all` now composes the first-movement direct continuation:
the nested `03:676B` scene-clear idle leaf plus its caller's `JSR` stack frame
and child return to the outer `RTS`. It admits 1,034 of 1,041 calls through
the exact 189-cycle, three-write transcript, while mixed non-idle continuations
retain assembly execution. The focused Release census, call, and ordered-write
artifacts are byte-identical to assembly.

Semantic `03:67C0@all` now ports every focused Three-Blobs direct final
collision-gate call. For fixed slots `$58-$5A`, its 27-cycle no-hit leaf
reloads `$3800,X`, derives the original N/Z flags, and takes the outer return
through two exact checkpoints. The 15 collision-active calls now also execute
the 69-cycle latch-clear, command-`$44`, and original-audio-trampoline prefix
natively, including its return stack frame; the original audio, color-state
advance, and appearance update remain assembly-backed after that handoff. The
dedicated Release regression preserves the exact census, call, and ordered-
write artifacts and replays ordinal one at frame 1,229 with 27 cycles and no
writes.

The hidden fixed-system scheduler initializer `03:7B1F@all` now executes
natively for slot `$5C`. Its exact 213-cycle, eight-write setup restores the
one-tick timer and installs `03:7B27` through the original scheduler boundary.
A focused Release regression guards this one-time call; steady selector
submission remains within its existing native-or-assembly split.

The paired hidden fixed-marker initializer `03:7B6D@all` now executes natively
for slot `$5D`. Its exact 234-cycle, nine-write setup restores marker mode
`$04`, the one-tick timer, and the original scheduler handoff to `03:7B7A`.
A focused Release regression guards this one-time call; phase-wave marker
presentation remains within its existing native-or-assembly split.

The final hidden fixed-response initializer `03:7CA7@all` now executes
natively for slot `$5E`. Its exact 219-cycle, nine-write setup clears shared
response state `$2D38` and collision scratch `$CD`, then installs `03:7CAF`
through the original scheduler boundary. A focused Release regression guards
this one-time call; the conservative collision/scene response remains within
its existing native-or-assembly split.

The Sorcerer direct-tail investigation is complete with no replacement
admitted. A fresh Release cross-family observer set retained `02:58C2`,
`02:5EEC`, `01:DAF8`, `03:6490`, and `03:651F` in assembly because each still
combines collision, drawing, or parent-relative interrupt timing. Splitting
the natural Sorcerer route initially ranked `02:5903` (18,152 calls;
2,579,169 weighted units), but the caller/return transcript and ROM-byte
check proved it is the shared one-byte `RTS` at the orbit-path boundary.
Its measured variants therefore aggregate caller work rather than an isolated
leaf; the surrounding orbit controller remains explicitly emulated.
The next sibling screen, staggered deployment `02:586F`, has 1,189 apparent
84-cycle, one-write countdown passes among 1,240 calls, but its remaining
deployment and interrupt-bearing states are caller-relative. A guarded trial
of the countdown shape diverged at frame 941, so no adapter was admitted and
the complete state remains assembly-backed.
Its direct predecessor `02:5808@all` is now native for all eight fixed
Sorcerer slots `$17-$1E`. The slot-and-route-index-bounded adapter preserves
the exact 642-cycle, 22-write orbit-field, live-count, staggered-delay, and
`01:C5E6` scheduler transcript; the complete Release family census, call,
ordered-write, and selected-state artifacts remain byte-identical. The
owning Sorcerer regression replays `02:5808@1` at frame 873.
The preceding route-indexed staging initializer `02:57F8@all` is now native
for the same eight fixed slots. Its exact 249-cycle, ten-write transcript
installs route and collision state before the original scheduler transfers to
`02:5808`; the owning regression replays `02:57F8@1` at frame 871 alongside
the existing deployment replay.
The complete Sorcerer family checkpoint now passes in both Release and Debug,
including the native counts and normalized replays for both setup adapters.
The adjacent Tower-insect source initializer `02:5F22@all` now executes
natively for its sole fixed `$2C` frame-872 call. Its exact 192-cycle,
seven-write transaction installs the original `$5F24` continuation and
preserves the `01:C5E6` scheduler checkpoint; later spawn countdown,
allocation, collision, and group-clear behavior remains assembly-backed. The
expanded Tower-insect source regression passes in Release, and the complete
Debug Tower-insect family checkpoint now replays `02:5F22@1`, `03:60B3@1`,
`03:6054@1`, and `02:5F5F@1` with byte-identical census and updated complete call/write
artifacts.
The adjacent fixed Tower initializer `03:60B3@all` now executes natively for
its sole frame-872 slot `$2D` call. Its exact 249-cycle, ten-write progress,
phase, guard, and scheduler transaction is covered by the expanded Release
Tower-family replay and final Debug family checkpoint, which replays
`02:5F22@1`, `03:60B3@1`, `03:6054@1`, and `02:5F5F@1`; the busy child
controller and collision paths remain assembly-backed.
The mirrored fixed Bumper initializers `03:789A/78A4@all` now execute natively
for their sole frame-871 slots `$14/$15` calls. Their side-specific exact
342/336-cycle, fourteen-write transactions establish side/type, collision,
phase, and appearance state before handing off to the existing `03:78C7`
dispatcher through the original scheduler boundary. The expanded owning
Bumper regression replays both entries alongside `03:78C7@1` and preserves
the full steady hit, score, and actor-identity semantic route in Release and
the final 202.17-second Debug family checkpoint.
The right Bumper's real terminal child release at `03:79DE@all` now executes
natively for all six slot `$71-$76` child initializers after the frame-3,342
slot-`$15` hit. Its exact 402-cycle, seventeen-write transaction copies parent
position and appearance, installs type `$83`, and preserves the first child's
in-flight frame interrupt through an instruction-boundary checkpoint. The new
`dc_rom_bumper_terminal` regression advances only the authentic phase before
contact and proves native census, call, and ordered-write artifacts are
byte-identical to the assembly baseline.
The child-controller sibling `03:7A71` was also screened at that real terminal
frame: all six entries immediately compose the shared collision-table walker
and later motion work, so it remains assembly-backed rather than duplicating
the existing `03:6966` ownership boundary.
The paired fixed Small-Soldier source initializers `03:7D4D/7DE9@all` now
execute natively for their sole frame-872 slots `$34/$35` calls. Their exact
327/333-cycle, fourteen-write side-specific transactions install collision
IDs, side, source state, subtype, child budget, and the original scheduler
handoff to delayed spawn states `03:7D6F/7E0D`. The expanded owning source
regression replays both entries while retaining delayed spawning, allocation,
source waiting, and child completion under their established ownership in
Release and the final 180.52-second Debug family checkpoint.
The staged Giant-Face controller setup `03:7428/742B/742E@all` now executes
natively for the frame-872/873 scheduler stages and the frame-941 empty-form
finalization of fixed slot `$36`. It preserves the 192/192/306-cycle,
7/7/12-write transactions that install the original `742B`, `742E`, and
`74D8` scheduler states. Persisted forms one through six still fall back to
the original restoration branch. The expanded Giant-Face regression replays
all three direct entries with the established request, animator, crown, and
mouth gates.
The paired Small-Soldier delayed-spawn waits `03:7D6F/7E0D@all` now execute
natively whenever their source timers remain nonzero and clear of the user or
timer interrupt horizon. The exact 33-cycle, one-write decrement/return path
replaces 155 left and 161 right calls; expiry, allocation, and 70 timing
boundary calls remain assembly-owned. The source regression retains its
existing child-spawner and completion replay before independently replaying
both native waits.
The fixed Giant-Face hidden animator initializer `03:766C@all` now executes
natively for its sole frame-872 slot `$37` call. Its exact 255-cycle,
ten-write setup establishes timer, phase, and form state before the existing
`03:767E` animator gate; active animation modes remain assembly-owned.
The fixed Giant-Face crown initializer `03:76F7@all` now executes natively
for its sole frame-872 slot `$39` call. Its exact 192-cycle, seven-write
scheduler transaction installs the existing `03:76FA` collision gate; hit
handling remains assembly-owned.
The fixed Giant-Face mouth initializer `03:7718@all` now executes natively
for its sole frame-872 slot `$38` call. Its exact 243-cycle, nine-write
collision-clear and scheduler transaction installs the existing `03:7726`
mouth gate; capture and hit handling remain assembly-owned.
The stopped Undead-Head controller parent `03:61A3@all` now composes its
two clear session-transition reads with the existing selector return for
fixed slot `$4D`. All 770 camera-window calls preserve the exact 63-cycle
no-write path and all original interrupt passages through six instruction
checkpoints; transition and activation paths remain assembly-owned.
The paired Side-Worm stopped-parent routine `02:5CC5@all` now composes its
two clear session-transition reads with the existing selector return for
fixed slots `$4E/$4F`. All 1,760 camera-window calls preserve the exact
72-cycle no-write path and 35 original interrupt passages through six
instruction checkpoints; transition and activation paths remain
assembly-owned. The permanent Release replay and Debug ROM-free core
checkpoint both pass.
The Giant-Skeleton Eyeball opening wait `02:4DF7@all` now executes natively
for its fixed slot `$2F` while opening-state timer `$3A2F` exceeds one. The
controlled completion-latch route replaces all seven exact 33-cycle,
one-write decrements and preserves byte-identical artifacts; its first and
second expiry paths remain assembly-owned for their animation, audio, and
scheduler work. The expanded Eyeball Debug family checkpoint also passes;
its controlled census is `eb5b0e905cf6a88aa9317329d6167b95fe19d683f2d598373a3312837c43f89c`
and its entry/call/write artifacts match Release.
The following scheduled steady return `02:4E24@all` now replaces 1,066 of
1,377 observed calls. It admits only fixed slot `$2F` with a nonsigned
collision byte and clear final-animation flag, then reproduces the 51-cycle,
zero-write local return. The 311 calls at a conservative event horizon and
the 22 collision/animation continuations remain assembly-owned. Release and
Debug census, entry, call, and ordered-write artifacts are exact, and the
normalized `02:4E24@2` replay passes in both configurations.
The final-animation wait `02:4F3D@all` now also replaces 122 of 224 calls
for fixed slot `$2F` when its timer and phase both remain above one and clear
of every conservative event horizon. It preserves the 102-cycle, two-write
main-timer/phase decrement and local return. Expiry, phase-transition, and
the other 102 horizon-bound calls remain assembly-owned. Release and Debug
artifacts are exact, and normalized replay `02:4F3D@2` passes in both builds.
A follow-up Eyeball continuation screen retains `02:4F76` in assembly: its
sole natural entry is a 2,388-cycle, 64-write drawing/coroutine/scheduler
transition with an interrupt passage, not an independently bounded leaf.
A separate controlled Giant-Face crown-hit screen likewise retains active
helper `03:761E` in assembly: its one hit call spans 10,473 cycles and 109
writes (16,839 cycles/262 writes for the enclosing gate), crossing original
impact allocation and score/audio behavior rather than exposing a direct leaf.
The separate active hidden-form screen now ports `03:76C5@all` for fixed slot
`$37`: 639 of 872 calls execute the exact 27-cycle, one-write non-expiry timer
wait through two instruction checkpoints. Its 233 expiry or conservative
horizon calls retain the original phase, drawing, and scheduler behavior; the
focused Release census, entry, call, and ordered-write artifacts are exact and
normalized replay `03:76C5@2` passes at frame 1,224. The required Debug
Giant-Face family checkpoint also passes with census
`b56d786a79b31e51e5ffef5ebc40be80f46be13a1d930b11c7372be76a4eca4e` and
matching entry/call/write artifacts.
The mirrored Big-Star pending-right helper `02:5738@all` now executes for its
controlled fixed-slot `$4B` phase-zero through phase-four calls. It
reproduces exact 228-cycle, six-write phase-zero hold and phase-one-to-zero
`$40:$00`/cadence-three selections, phase-two-to-one/`$20:$00`/cadence-six,
and phase-three-to-two/`$10:$00`/cadence-eight, each with normalized replay
at frame 1,221. The verified phase-four reset writes phase three, selects zero
velocity/cadence, and follows its separate 252-cycle tail to set the pending
latch to one unless `$2D18` equals four, where it clears the latch through the
other 252-cycle tail. The new phase-zero and phase-one focused Release
artifacts are byte-identical, and the required Debug family checkpoint for
phase one preserves matching entry/call/write artifacts with census
`ed64e39a4c427c98052151f3c4f7e0410945cbf0ca809ee2be06d14179633e65`.
The phase-zero Debug checkpoint has census
`493a1ee77abf53ab229a95d5ef50ba4067617b72556845324ffd9dfac4c362af` with
matching Release entry/call/write artifacts.
The phase-four Release artifacts are likewise byte-identical; its required
Debug checkpoint has census
`3684061b116d8626b85b21bcdc3b550cf61e52fd506bed136cbfcc6c91f5c325`
with matching entry/call/write artifacts.
The Tower-insect source countdown `02:5F4A@all` now replaces 141 of 145
natural calls when fixed source slot `$2C` has a timer above one. It preserves
the exact 33-cycle, one-write decrement wait and instruction checkpoints for
the two interrupt-bearing calls; timer expiry and spawning remain assembly
fallback. The required Debug Tower-insect checkpoint preserves matching
entry/call/write artifacts and census
`a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`.
A controlled Four-Star appearance-six probe set fixed slot `$3E`'s indexed
appearance `$39BE` at frame 1,235 and reached `02:4B00` once. Its 942-cycle,
22-write original-helper and scheduler transaction is not an independently
bounded leaf, so the transition remains assembly-backed.
A follow-up activation screen now ports the Side-Worm's first active
coroutine handoff `02:5D4A@all`: the fixed left slot `$4E` preserves its
192-cycle, seven-write `01:C018`/scheduler transaction and installs `02:5D4D`
at frame 1,997. Its only repeated direct helper, `02:5DFC`, remains a
pointer-driven
coordinate/collision transaction (20 calls, 13,584 cycles, and 329 writes),
so active motion/collision behavior stays assembly-backed. A separate
Dragon's-Head terminal screen also remains assembly-backed: setting the
existing frame-2,076 hit counter to one one frame early exposes one
25,248-cycle, 356-write cross-subsystem completion transaction at `02:5A4B`,
not an isolated leaf.
The complete direct-setup initializer batch plus the Side-Worm selector gate
has been revalidated in Release: all 25 focused ROM regressions, the owning
build, and ROM-free core suite
pass. Its required Debug family checkpoint also passes: the owning Side-Worm
initializer replay `02:5C4B@1` preserves the one native entry, frame-872
204-cycle/10-write transcript, and the Release call/write hashes. The known
configuration-specific framebuffer census is
`a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`;
the Debug ROM-free core suite passes as well.

A fresh ordinary live-ball sibling profile now explicitly rejects the broad
`01:C765` handler and shared `01:DC95` post-motion helper as the next slice:
their 2,340/2,895 calls contain mixed integration, collision, pointer-driven
geometry, drain, and presentation work. The remaining `01:C696` ball
dispatch is a shared `C5C9/C5EA` scheduler/coroutine entry rather than a
local gameplay leaf. No replacement was added from this screen; its measured
call, cycle, write, and return populations are recorded in
`docs/NATIVE_FALLBACK_HOTSPOTS.md`. The next candidate must come from a new
bounded sibling observer set.

A second ordinary contact-response observer set now likewise rejects
`01:CBCF`, `01:CF33`, and interior `01:D524`. Their 270/520/980 calls consume
5,064,033/7,933,113/2,000,790 master cycles and 159,371/251,090/50,335
ordered writes. The apparently compact `CF33` form enters mixed `CF89`, whose
228 internal-entry scopes return through post-motion `DCD7`/`DD08`; its
sibling `CE94` crosses collision or arithmetic work. `D524` remains part of the
previously bounded-but-broad `D525` controller. No independent state-derived
return exists in this natural family, so no replacement was registered; the
next candidate must again come from a new sibling observer set.

A controlled fixed-response screen now sets only collision scratch `$20CC`
at frame 1,200 and retains the active `03:7CAF` branch in assembly. Its one
8,949-cycle, 225-write transition issues original audio command `$08`, updates
response/countdown state, and reschedules through `01:C5E9`; the next frame's
continuation issues command `$01`. The two `00:ECFD` audio-driver child scopes
take 5,799/10,830 cycles and 149/269 writes, so the branch remains a combined
collision/audio/ball-response scene transaction rather than an extension of
the native 45-cycle dormant gate. No replacement was registered.

A separate Tower-Guard open-selector probe set only gate `$2D4D` to zero and
fixed collision byte `$382E` to `$80` at frame 1,200. The route continues to
run all 2,650 selector updates but never enters `03:612B`; the Guard collision
is not the BS-5 transition source. The native selector leaf is unchanged and
no parent composition was registered. The fixed-target scanner `01:CC1A`
does select slot `$2E` twice per frame. Its inner `CC98` trace proves that its
descriptor accepts ball X=`$84-$A3`, Y=`$0206-$022D` and returns through
`$CCD2`; those hits return `A=$08`, `Y=$04` to generic `01:CBCF`, not to the
entrance.

The actual Tower terminal route is now captured: at frame 3,353, slot `$2D`
with qualifying latch `$2CFC=1` and gate `$2D4D=1` enters `60F9`, decrements
the gate, and branches to `612B`. That 228-cycle prefix clears `$382D` and
schedules `6136`. Its controlled route does not deliver a sign-bit Tower
collision: the 714-cycle frame-3,413 call is a timer-interrupt-bearing no-hit
return, not stage selection. A separate geometry-delivered probe places the
ball at the Tower contact surface at frame 3,354; collision staging then writes
`$382D=$80` naturally and the frame-3,355 `6136` call takes the real
stage-selection branch. Its local 75-cycle flag/store/`C021` prefix is native,
while the shared dispatcher remains assembly-backed.
`dc_rom_tower_bonus_entrance` now freezes this controlled route in Release:
the terminal and scheduler-prefix entries each occur once, `6136` occurs 245
times with its later collision-gated continuation, and the census, ordered
call/write, and selected Tower-state artifacts remain exact.
Semantic `03:60F9@all` now replaces the single armed final-gate pass through
an exact four-checkpoint, 54-cycle transcript. It retains the latch-loaded
accumulator across `DEC $2D4D`, records the zero gate write at cycle 42, and
hands control to the original `03:612B`; the same Release entrance regression
proves its complete artifact remains byte-identical.
The refreshed `03:6136` original trace corrects that earlier rejection: its
frame-3,413 timer IRQ saves `PC=6139` immediately after the indexed collision
load. Semantic `03:6136@all` therefore owns all 245 observed no-hit calls
through cycle-15 and cycle-21 checkpoints, then resumes original `613B` for
`RTS`. The separately captured geometry-delivered sign-bit call owns its
75-cycle prefix through the original `01:C021` dispatcher handoff.

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
Semantic `01:CC1A@all` replaces all 85,715 central collision-scan calls
through exact state-derived vertical-separation transcripts, composed
`CCD5` post-geometry bounds, and the final in-range coordinate/multiply/mask
path. The complete 85,715-call/1,784,119-write artifacts remain exact.
Semantic `01:CCD5@all` replaces all 66,523 near-target geometry calls through
live-MPR ROM pointer resolution and exact path-sensitive
783/789/885/891-cycle transcripts. Original interrupt handlers preempt and
return at the declared instruction boundaries, and all 1,414,954 aggregate
writes remain exact.
Semantic `00:E569@all` replaces all 874 unsigned-byte products nested by the
remaining collision-mask paths. Its live-bit-generated transcript preserves
the saved-X stack transaction, conditional high-byte increments, flags,
timing, all 10,729 writes, and original interrupt passages.
Semantic `01:CC98@all` replaces the final lookup tail on all 874 of those
paths. It resolves both table pointers and the mask byte through the live MPR
map, applies the ROM column bit, and returns through exact 378/384-cycle,
nine-write contact/no-contact transcripts. All 9,617 aggregate writes and
original interrupt passages remain exact.
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
Semantic `02:42F6@all` replaces all 1,790 Dragon's-Egg group-controller
checks observed on the route through a two-boundary transcript for the exact
21-cycle nonzero-count return. All 17 original interrupt-handler passages and
219 aggregate writes remain exact; the count-zero award, latch update, and
reset sequence outside the route remains emulated.
Semantic `02:4345@all` replaces all 9,857 Dragon's-Egg hit checks through
exact 21-cycle idle or 57-cycle score-setup transcripts. Semantic
`02:4357@all` owns the routed 93-cycle, four-write egg-disable and
impact/allocation setup, and semantic `02:4371@all` owns the 21-cycle,
one-write post-allocation activation. Shared score/audio, burst allocation,
hatchling allocation, and scheduling remain assembly; all 1,682 aggregate
writes remain exact.
Semantic `02:4DC8/4DD9@all` replaces the fixed Giant-Skeleton-Eyeball
initializer and all 1,988 activation checks observed on the route. The
initializer uses its exact 258-cycle, ten-write script transcript and final
interrupt boundary; the gate retains its six-boundary 57-cycle dormant
return. All 37 original interrupt-handler passages and the expanded 503-write
aggregate remain exact; opening and final-animation states outside the route
remain emulated.
Semantic `03:6876@all` now replaces all seven Gray-Blob layout initializers.
Its 15-write setup transcript uses the final `01:C5E6` checkpoint to admit
the four calls that had previously remained at the conservative event/IRQ
horizon, preserving the same Release census, call, and ordered-write
artifacts.
Semantic `03:68A1@all` now replaces all seven naturally activated Gray-Blob
first-live prefixes on the maximum-score route. For slots `$50-$56` with the
initial one-tick phase timer, it clears transient/collision latches, composes
the native RNG recurrence, stores the original 64--95-tick movement interval,
and hands off at `01:C018`. Every complete 633-cycle, 20-write scope (and the
focused Release census, call, and ordered-write artifacts) remains exact;
later movement, collision, hit, impact, and scoring paths stay assembly-backed.
Semantic `02:57AD@all` now replaces all 2,648 live Sorcerer-controller
branches on the standard route. Its exact 27-cycle `$2D0F` load and taken
branch hands off to the existing native `02:57E9` color gate; the complete
Release census, call, and ordered-write artifacts remain exact. Zero-live
delay, RNG selection, and redeployment behavior remain emulated.
Semantic `02:4FC1@all` replaces all 7,950 Mercy-Star activation checks
observed on the route through a five-boundary transcript for the exact
54-cycle not-last-ball return. All 163 original interrupt-handler passages
and 2,173 aggregate handler writes remain exact; normal-mode activation and
score-limit states outside the route remain emulated.
Semantic `03:65C2@all` replaces all 2,650 Three-Blobs controller checks
observed on the route through three/five-boundary transcripts for the exact
42/63-cycle mismatch returns. All 50 original interrupt-handler passages and
665 aggregate handler writes remain exact; equal-state score and randomized
reset outcomes remain emulated.
Semantic `03:62B8@all` replaces all 676 baseline Giant-Door hit checks through
a two-boundary transcript for the exact 27-cycle idle return, plus both
geometry-delivered sign-bit calls through the exact 48-cycle collision-clear
and score-load prefix to original `03:62C4`. Semantic `03:62CB@all` adds the
two observed nonterminal 27-cycle counter decrements before original `03:62D0`
and the 33-cycle terminal zero branch to `03:62D4`; the completion body remains
emulated after native `03:62D4` contributes its exact 57-cycle, four-write
score-helper handoff and native `03:62E2` contributes its exact 243-cycle,
nine-write completion-state/coroutine handoff to `03:62F0`; native `03:62F0`
then contributes its exact 243-cycle, eight-write channel-request/scheduler
handoff to `03:62FE`, whose native clear-bit wait leaf contributes its exact
48-cycle zero-write return across all 378 observed polls, including five
timer-interrupt passages. All seven baseline interrupt-handler passages and 88
aggregate handler writes remain exact.
Semantic `02:445B@all` replaces all 2,650 hidden Big-Star feature-controller
calls observed on the route through a two-boundary transcript for the exact
21-cycle clear-latch return. All 28 original interrupt-handler passages and
364 aggregate handler writes remain exact; pending Little-Skull direction,
actor, score, and audio work outside the route remains emulated.
Semantic `03:6339@all` replaces all 2,650 Walled-Star hit checks observed on
the route through a four-boundary transcript for the exact 54-cycle idle
return. All 49 original interrupt-handler passages and 659 aggregate handler
writes remain exact; collision damage and active post-destruction access
outside the route remain emulated.
Semantic `03:60C3@all` replaces all 2,646 fixed-Tower hit checks through
exact 27-cycle idle or 63-cycle collision-clear transcripts. Shared score
and audio helpers remain emulated; semantic `03:60E0@all` executes the
156-cycle Tower-progress handoff around the original interrupt-bearing
dispatcher, and semantic `03:611C@all` composes the native scheduler for all
four transition calls. The complete outer 538-write artifact remains exact.
Semantic `02:5A4B@all` replaces all 2,144 Dragon's-Head hit checks through
exact 27-cycle idle or 42-cycle collision-clear transcripts. Shared score,
audio, bonus, and appearance helpers remain emulated; semantic
`02:5A6E@all` performs both routed 27-cycle, one-write nonterminal counter
decrements. All original interrupt passages and the complete 866-write
outer artifact remain exact; counter one and the eighth-hit mouth-opening
path continue through assembly.
Semantic `03:78C7@all` replaces all 3,468 fixed-Bumper hit checks through
exact 27-cycle idle or 165-cycle, four-write collision-clear/phase-dispatch
transcripts. Shared score, audio, appearance, and scheduler helpers remain
emulated; semantic `03:78F5@all` executes all nine routed 36-cycle,
one-write primary progress increments and semantic `03:7912@all` executes
both routed 114-cycle, three-write secondary damage updates. The complete
3,127-write outer artifact and 11-call/953-write continuation artifact
remain exact; ninth-hit destruction and child release continue in assembly.
Semantic `02:5116@all` replaces all 125 fixed Devil-Shooter hit checks
observed on the route through exact two-boundary 27-cycle idle and 21-cycle
catch transcripts. The catch hands the occupied latch and shared `00:C087`
ball-position/audio helper to assembly, then semantic `02:52B8@all`
executes the 228-cycle, nine-write catch-state and coroutine continuation
natively. The helper's original interrupt-handler passage and the complete
116-write outer artifact remain exact.
Semantic `02:54B9@all` replaces all 1,631 left Skeleton-Insect trigger
checks observed on the route. Its two-boundary transcript preserves the
exact 21-cycle clear-latch return for 1,630 calls; the 978-cycle, 23-write
trigger composes the native score accumulator and coroutine scheduler for
the 4,000-point award, four-tick opening timer, and `02:54D0` dispatch. All
nine original interrupt-handler passages and the complete 150-write
artifact remain exact.
Semantic `02:5551@all` replaces all 336 right Skeleton-Insect trigger checks
observed on the route. Its two-boundary transcript preserves the exact
21-cycle clear-latch return for 335 calls; the 978-cycle, 23-write trigger
composes the native score accumulator and coroutine scheduler for the
4,000-point award, four-tick opening timer, and `02:5568` dispatch. All six
original interrupt-handler passages and the complete 101-write artifact
remain exact.
Semantic `02:55F9@all` replaces all 2,403 center Skeleton-Insect trigger
checks observed on the route through exact two-boundary 21-cycle idle and
27-cycle trigger transcripts. The trigger hands the shared drawing helper
to assembly, then semantic `02:5604@all` executes the 1,152-cycle, 30-write
score, presentation, delay, and coroutine continuation natively. All 23
idle interrupt-handler passages, the drawing helper's trigger interrupt,
and the complete 391-write artifact remain exact.
Semantic `03:63F1@all` replaces all 643 Devil's-Coffin steady checks through
five/seven/21/23-boundary transcripts for the exact 51-cycle idle,
66-cycle `$83` type-refresh, and 270/285-cycle collision-entry paths. Both
routed contacts start the one-tick opening timer and compose the native
coroutine scheduler to install `03:640A`. All 13 original interrupt-handler
passages and the complete 202-write artifact remain exact.
Semantic `03:640A@all` replaces all 18 Devil's-Coffin opening-pass calls
through two/eleven/28-boundary transcripts for the exact 33-cycle timer-
decrement, 132-cycle appearance-advance, and 366-cycle scheduler-handoff
paths. The complete 52-write artifact remains exact.
Semantic `03:6445@all` replaces all 18 Devil's-Coffin closing-pass calls
through two/nine/34-boundary transcripts for the exact 33-cycle timer-
decrement, 111-cycle nonterminal appearance-decrement, and 429-cycle final
closure/scheduler-handoff paths. The complete 58-write artifact remains
exact.
Semantic `03:60A6@all` replaces all 2,650 Tower-Guard selector updates
observed on the route through a four-boundary transcript for the exact
48-cycle closed-selector write. All 58 original interrupt-handler passages
and the complete 3,403-write artifact remain exact; the unexercised
zero-counter `$8E` selector and BS-5 entrance continue through assembly.
Semantic `03:7CAF@all` replaces all 2,650 hidden fixed
collision/scene-response controller calls observed on the route through a
four-boundary transcript for the exact 45-cycle dormant return. All 54
original interrupt-handler passages and the complete 721-write artifact
remain exact; negative-scene handling and the nonzero response state machine
continue through assembly.
Semantic `03:7B27@all` replaces all 2,650 adjacent fixed-scheduler calls.
The suppressed/waiting paths retain exact 48/87-cycle transcripts; all 332
timer expiries use a 156-cycle timer-reload/first-submission prefix, followed
by 332 exact 54-cycle entries apiece at semantic `03:7B4E@all` and
`03:7B5D@all`. The original banked-call wrapper and exported routine retain
their interrupt-bearing assembly behavior, and the complete 46,618-write
artifact remains exact. Only negative-scene handling remains emulated.
Semantic `03:7B7A@all` replaces all 2,650 paired fixed-marker calls. The
suppressed/waiting paths retain exact 48/93-cycle transcripts; all 332 timer
expiries use a 231/240-cycle cadence-reload, modulo-eight phase-wave, and
first-descriptor prefix, followed by 332 exact 36-cycle entries apiece at
semantic `03:7BBB@all` and `03:7BCD@all`. Semantic
`03:7BE0/7BE8/7BEC@all` handles all 996 phase-table/stack prologues,
post-draw accumulator restores, and Y-restoring epilogues through exact
33/12/12-cycle segments. Semantic `02:5955@all` replaces all 997 routed
five-bit phase-stride additions through exact 15-checkpoint, eight-write,
198-cycle transcripts. All 75 original interrupt-handler passages and the
complete 8,978-write helper artifact remain exact. The remaining `01:C05D`
drawing submission retains its assembly behavior, and the complete
88,480-write marker artifact remains exact. Only negative-scene handling
remains emulated.
Semantic `03:74D8@all` replaces all 2,648 routed Giant-Face controller calls
through exact three-boundary, 27-cycle request-bit-clear transcripts. All 23
original interrupt-handler passages and 289 handler-owned writes remain
exact. The complete 5,608-call/812-write Giant Face family artifact is
unchanged; wake and transformation requests remain emulated.
Semantic `03:767E@all` replaces all 873 routed Giant-Face hidden-form
animator calls through exact three-boundary, 27-cycle idle transcripts. All
eight original interrupt-handler passages and 97 handler-owned writes remain
exact. The complete 5,608-call/812-write Giant Face family artifact is
unchanged; animation mode one's active timer, phase, and drawing path remains
emulated.
Semantic `03:76FA@all` replaces all 867 routed Giant-Face crown collision
checks through exact two-boundary, 27-cycle no-hit transcripts. All seven
original interrupt-handler passages and 86 handler-owned writes remain exact.
The complete 5,608-call/812-write Giant Face family artifact is unchanged;
the 300-point score, impact, and audio hit path remains emulated.
Semantic `03:7726@all` replaces all 1,214 routed Giant-Face mouth checks
through exact four-boundary, 48-cycle closed/no-hit transcripts. All 22
original interrupt-handler passages and 288 handler-owned writes remain
exact. The complete 5,608-call/812-write Giant Face family artifact is
unchanged; open-mouth capture and the 300-point score, impact, and audio hit
paths remain emulated.
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
Collision, physics/arithmetic, controls/actor, and rules/RNG/score synthesis now
occupy four subsystem implementation files behind a private internal
interface; `native_replacements.cpp` is the 555-line registry. The
extraction is behavior-neutral and preserves the existing pure C
`src/game_logic` boundary. The 49 directly affected subsystem ROM-backed
regressions pass in Release, representative Debug/Release semantic hashes
remain exact, and both application configurations compile cleanly. The
documented native-port iteration cadence now makes the Release owning-family
regression the per-slice behavioral gate; Debug family replay remains required
at the defined family and milestone checkpoints.

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
freezing fade-time colors. The backglass now renders the mapped title logo
through a native ROM-derived 64-frame animation while the DMD retains its
score-only presentation. Only the two-player DMD
discriminator remains deferred at the user's request; Milestone 4 proceeds
without that discriminator.
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
  - Native animated Devil's Crush logo on the backglass monitor, with a
    user-selected static image as a failure fallback.
- Initial cabinet input supports keyboard/controllers and semantic feedback hooks; physical DMD, DOF, and SSF support are deferred.

## Progress Update Template

When work advances, update:

1. The current phase, completed closure gates, and milestone status.
2. The `Completed` and `Next Actions` checklists.
3. `Active Work` with the current task and owner.
4. `Blockers` with a concrete resolution condition.
5. The date at the top of this document.
