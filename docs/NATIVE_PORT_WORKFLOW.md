# Native Port Iteration Workflow

This workflow keeps routine replacement faithful while making the normal
iteration loop fast enough to port related code as a unit. It applies to
Milestone 4 native gameplay work; it does not weaken the release or milestone
acceptance criteria in [PLAN.md](../PLAN.md).

## Current priority: bonus hazard countdown and return ranking (September 9)

This user-supplied ranking supersedes the Side-Worm/multiply ranking below.
The live recording was not supplied; the measurements here use separate local
replays, with targeted bonus activation to exercise the ranked code.

| Rank | Address | Master cycles | Intervals | Implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 05:7398 | 435,252 | 20,516 | Shared live-stack RTS |
| 2 | 05:7393 | 311,694 | 20,530 | Live ORA high countdown |
| 3 | 05:7387 | 311,454 | 20,530 | Live LDA low countdown |
| 4 | 05:7376 | 311,154 | 20,550 | Live LDA actor activity |
| 5 | 05:7390 | 311,046 | 20,530 | Live STA low countdown |
| 6 | 02:4FE1 | 285,570 | 13,458 | Shared live-stack RTS |
| 7 | 05:7379 | 248,952 | 20,550 | Live BPL activity gate |
| 8 | 05:738A | 248,298 | 20,530 | Live BNE borrow gate |
| 9 | 02:4FC1 | 203,910 | 13,458 | Live LDA fallback after full Mercy-Star owner |
| 10 | 05:7396 | 124,536 | 20,530 | Live BEQ expiry gate |
| 11 | 05:738F | 124,236 | 20,530 | Live DEC accumulator |
| 12 | 04:5197 | 117,033 | 5,525 | Shared live-stack RTS |

All twelve ranked sites have native coverage. Nine independent instructions
share `replace_bonus_countdown_instruction`; three returns extend the existing
live-stack RTS implementation. The bank-05 hazard gates use live N/Z, loads
use live RAM and X, DEC consumes live A, STA preserves flags, and ORA combines
live A with the current high countdown. The unranked high-byte decrement at
05:738C remains in the core, including on low-byte underflow. Each port resumes
at the real next instruction so an interrupt can separate any two operations.

LDA/STA/ORA sample at cycle 12 and finish at 15; STA writes at 15. DEC samples
at 3 and finishes at 6. Branches sample at 3/9 and finish at 6/12. These single
instructions retain a checkpoint at every event horizon, require T clear,
exact PC/code mapping and a full mapped RAM snapshot, and permit decimal mode.
Returns retain the shared strict greater-than-21 event guards and wrapping
live-stack semantics. The existing full Mercy-Star gate keeps priority.

The ROM-free fixture adds 3,538,944 operand/status cases across all byte values
and actor indices, with mapping, short-RAM, horizon, write-offset and register
preservation checks. Shared return coverage rises to forty sites and 7,864,320
stack/target/status cases. The new two-route assembly oracle pins every ranked
opcode and compares frame state, scheduler calls and scoped writes, all RAM
writes, and all device writes. Its bonus-stage-6 route includes a real low-byte
borrow; the second route forces 150 Mercy-Star table-mode declines while
retaining 6,255 full gate walks.

Registry membership rises from 775 to 786, reachable keys from 823 to 834, and
classified seam ranges from 235 to 247. The existing Mercy-Star semantic oracle
still admits 7,950/7,950 full walks with unchanged hashes.

Paired full-registry replays against the saved pre-change executable remove
30,072 of 31,794 target cycles on the bonus-stage-6 route (94.6%), leaving
1,722. The second targeted route removes 5,190 of 5,463 cycles (95.0%), leaving
273. Combined target residue falls from 37,257 to 1,995 cycles: 35,262 removed
(94.6%). Both frame hashes are unchanged and both routes have zero abandonments.
In each route, the total interpreter-cycle delta equals the target-address
delta. All nine newly ported non-return instructions have zero residue;
remaining residue is at guarded returns. Detailed results are in
`out/bonus-countdown-cost/active.csv` and `active-addresses.csv`.

The expanded BS-6 oracle also exercises 04:5197 on 501 calls (449 native),
besides 179 entries at each bank-05 site (159 native returns). All five
artifacts match assembly. The second oracle admits all 6,405 Mercy-Star entries,
137/150 independent returns, and 3/3 bank-04 returns; its five artifacts also
match assembly. Remaining declined returns retain the strict event guards.

All 31 standard corpus routes preserve frame hashes and abandonment counts.
They have no residual cycles at these twelve addresses before or after this
round, so this sweep is compatibility evidence; the targeted bonus replays
above measure coverage gain. The saved baseline has one abandonment each on
higharc, radial and sidestep, unchanged after the ports; all other routes have
zero. The paired corpus report is `out/bonus-countdown-cost/manifest.csv`.

The older Mercy-Star Debug test initially rejected the frame hash because it
still expected a different Debug-only artifact. An independent assembly-only
Debug census reproduces the Release/native frame SHA-256
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
call SHA-256 `b10fe937699858b1c55ee67cb579f98eeb7ca55b96e5ba7c2fb404cee7c53b4a`,
and write SHA-256 `ff4cb68930bc10a4e0f59b05a1e8e733ed6554aaa5b6bb874d77744410a3f91a`.
The test now uses that common frame expectation. The full gate implementation
was not changed; its 7,950 calls and 2,173 scoped writes remain identical.
Evidence: `out/bonus-countdown-mercy-debug-assembly.*`.

Release validation passes all seven selected checks: the shared exhaustive
fixture, both new bonus oracles, the existing full Mercy-Star oracle, the face
and hand captures, and host registry. The expanded bank-04 coverage oracle also
passes (34.59 seconds). Debug passes the six selected checks: shared fixture,
both new bonus oracles, full Mercy-Star, face and hand. The first Debug batch
logged the stale Mercy-Star expectation described above; its corrected rerun
passes in 158.08 seconds. The new Debug oracles pass in 311.74 and 592.06 seconds.

Logs: `out/bonus-countdown-existing-Release.log`,
`out/bonus-countdown-tests-{Release,Debug}.log`,
`out/bonus-countdown-expanded-Release.log`, and
`out/bonus-countdown-mercy-corrected-Debug.log`. Final Release/Debug builds,
registry generation/check, the 247-range seam verification and whitespace
checks pass. No prior port was reverted.

## Previous priority: Side-Worm and multiply residue ranking (September 9)

This latest user ranking supersedes the actor ranking below. No accompanying
input recording or total residue was supplied; local replays measure coverage
and compatibility separately from that live session.

| Rank | Address | Master cycles | Intervals | Shared implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 02:5CC8 | 51,780 | 3,412 | Live ORA session latch |
| 2 | 02:5CC5 | 51,564 | 3,412 | Live LDA session latch fallback |
| 3 | 00:E5AF | 48,702 | 2,286 | JSR E5BC |
| 4 | 00:E824 | 48,582 | 2,286 | Live-stack RTS |
| 5 | 00:E5BB | 48,534 | 2,286 | Live-stack RTS |
| 6 | 00:E5EC | 48,414 | 2,286 | JSR E813 fallback |
| 7 | 00:E5F1 | 48,414 | 2,286 | JSR E825 |
| 8 | 00:E5F8 | 48,414 | 2,286 | JSR E59F |
| 9 | 00:E5EB | 48,366 | 2,286 | Live-stack RTS |
| 10 | 00:E836 | 48,366 | 2,286 | Live-stack RTS |
| 11 | 02:5CCB | 41,352 | 3,412 | Live BNE session gate |
| 12 | 01:DA3E | 31,728 | 2,630 | Live-stack PLY |

All twelve sites are covered. The larger 5CC5 waiting-parent and E5EC signed
multiply walks retain priority; their declines use independent LDA/JSR
fallbacks. Four calls share the existing wrapping-stack JSR implementation:
high then low saved-PC bytes at offset zero, sample at 18, completion at 21,
and destination bank from the live MPR. Four returns share the existing atomic
RTS and return-observer declaration. Both calls and returns require both event
horizons strictly beyond 21 master cycles.

The Side-Worm gate reads $2D00 and $2D01 separately, preserving the live A for
ORA and live Z for BNE. LDA/ORA sample at 12 and finish at 15; BNE samples at
3/9 and finishes at 6/12. PLY reads the live wrapping stack, changes only Y,
N/Z and SP, samples at 9 and finishes at 12. These single instructions keep
their checkpoints at every event horizon. New gate/PLY and call paths require
T clear, mapped code/stack RAM and a complete RAM snapshot; decimal is allowed.

The combined twist oracle also exposed an existing E5BC transcript bug. Its
first multiply half selected second-half instruction and resume PCs from a
stale local PC: appending a checkpoint only updated a copy. Each half is now
selected explicitly. The multiply fixture verifies that every checkpoint
resumes at the next recorded instruction, across all 4,096 operand cases.
Arithmetic, write values and cycle offsets are unchanged.

The shared fixture covers 1,572,864 gate/stack operand/status cases, thirteen
JSR sites across all stack/status values, and 7,274,496 return cases, plus
mapping, RAM-size and event-horizon guards. The existing waiting-parent oracle
retains 1,760/1,760 full walks and its pinned hashes. Registry membership rises
from 765 to 775, reachable keys from 813 to 823, and classified ranges from
223 to 235.


The combined `verify_ranked_math_gates.cmake` oracle uses the natural 3,600-frame
`twist` route. All five artifacts match assembly: frame state, 2,088 scheduler
calls with 870,896 writes, all 12,470,985 RAM writes and all 3,265,435 device
writes. It pins the ROM bytes at every ranked site. LDA, ORA and BNE each
admit 686/686 entries; PLY admits 455/455. E5EC admits 313/344 entries with
its full owner retained; the other math calls/returns admit 253-265 of 292,
with strict event-horizon fallbacks.

Release validation passed the combined oracle, shared exhaustive fixture,
face and hand captures, audio wrapper, host registry and existing Side-Worm
waiting-parent oracle (7/7, 128.82 seconds). Both multiply semantic fixtures
also pass in Release and Debug (2/2 each).

Paired registry measurements against the saved pre-change executable reduce
`twist` residue at these twelve addresses from 84,024 to 5,982 master cycles:
78,042 removed (92.9%). Its frame hash is unchanged and both runs have zero
abandonments. All three gate instructions and PLY have zero remaining
interpreted intervals; the eight math call/return sites retain horizon
fallbacks. The total interpreter-cycle reduction equals the target-address
reduction. Per-address results are in `out/math-gates-cost/twist-addresses.csv`.

All 31 paired corpus routes preserve frame hashes and abandonment counts.
Their combined target residue falls from 88,794 to 6,024 master cycles:
82,770 removed (93.2%). Each route's total interpreter-cycle delta equals
its target-address delta. This saved baseline has one abandonment each on
`higharc`, `radial` and `sidestep`, unchanged after the ports; all other routes
have zero. Results are in `out/math-gates-cost/manifest.csv`.

Debug validation passed the shared fixture, combined oracle, face and hand
captures, and audio wrapper (5/5, 773.99 seconds). Including the two multiply
fixtures, this round passes 9 Release and 7 Debug checks. Logs are
`out/math-gates-tests-{Release,Debug}.log` and
`out/math-gates-multiply-tests-{Release,Debug}.log`. Registry generation/check
and the 235-range seam manifest validation also pass.

## Previous priority: combined actor residue ranking (September 9)

The latest user-supplied ranking supersedes the collision-call ranking below.
Port related instructions together. The input recording and total residue were
not supplied; corpus measurements are separate from the user's live session.

| Rank | Address | Master cycles | Intervals | Shared implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 02:45A9 | 50,808 | 2,400 | Live-stack RTS |
| 2 | 02:4571 | 38,901 | 1,841 | Live-stack RTS |
| 3 | 02:45A4 | 36,576 | 2,400 | Live latch LDA fallback |
| 4 | 02:59F4 | 35,001 | 1,653 | Live-stack RTS |
| 5 | 02:4484 | 29,988 | 1,420 | Live-stack RTS |
| 6 | 02:56AB | 29,685 | 1,401 | Live-stack RTS |
| 7 | 00:E60A | 29,532 | 1,396 | Live-stack RTS |
| 8 | 03:61B2 | 28,119 | 1,331 | Live-stack RTS |
| 9 | 02:4567 | 28,053 | 1,843 | Live indexed LDY |
| 10 | 02:456A | 27,789 | 1,843 | Live indexed LDA |
| 11 | 02:5674 | 25,992 | 1,224 | Live-stack JSR fallback |
| 12 | 02:5721 | 25,848 | 1,224 | Live-stack RTS |

All twelve addresses are covered in this round. Eight returns share the existing
atomic RTS implementation, including live wrapping stack reads, destination MPR
resolution, return-observer declarations, and strict event horizons beyond 21
master cycles. The three loads preserve live indices and update only their target
register and N/Z; each samples at cycle 12 and completes at 15. The JSR pushes
5676 high byte then low byte at offset zero, samples at 18 and ends at 21. Loads
and JSR reject T mode and require mapped code and RAM; decimal mode is supported.

The existing full 45A4 gray-release and 5674 rotation walks retain priority.
Their declined states now use the independent load/call without repeating a
cached actor classification. The Big-Star semantic oracle retains its pinned
hashes and 199 full walks, adding 219 JSR fallbacks (418/490 entries overall).
Registry membership increases from 755 to 765, reachable keys from 803 to 813,
and classified seam ranges from 211 to 223.

The shared fixture now checks 6,488,064 return target/status/stack cases, nine
JSR sites across all stack pointers and status bytes, and 1,179,648 skull load
index/value/status cases, plus mapping, RAM-size and event-horizon guards.
`verify_ranked_actor_seams.cmake` activates actors in a controlled census to
exercise all twelve sites. Its complete frame-state, 12,922,900 RAM-write and
3,245,139 bus-write transcripts match assembly. This is a controlled scenario,
not a replay of the user's latest session.

The controlled scenario also preserves the saved pre-change scheduler-call
hashes. These differ from assembly because the pre-existing full 5674 atomic
walk hides its internal 5721 return from the C5C9 observer. The new test pins
the assembly and native observer baselines separately; it does not claim
assembly-equivalent scheduler traces. Machine and write traces match exactly.

Paired registry measurements against the saved pre-change Release executable
reduce the twelve-address total in the controlled scenario from 58,488 to
7,215 master cycles: 51,273 removed (87.7%). The frame hash and abandonment
count are unchanged; this scenario has one pre-existing abandonment. Total
interpreter-cycle reduction equals the sum of reductions at the twelve sites.
The three loads have no remaining interpreted intervals on this route; return
and call residue remains where the strict event horizons require the core.
Artifacts are in `out/ranked-actors-active-cost/`, including `addresses.csv`.

All 31 paired corpus routes preserve frame hashes and abandonment counts;
`higharc` retains one pre-existing abandonment. Only `twist` reaches these
sites in that corpus: 24,615 to 4,476 cycles across 02:56AB, 00:E60A, 02:5721
and 02:5674, removing 20,139 cycles (81.8%). Other corpus routes have zero
residue at these twelve addresses both before and after. Each route's total
interpreter-cycle delta equals its target-address delta. Results are recorded
in `out/ranked-actors-cost/manifest.csv`.

Release validation passed 8/8 tests, including the combined actor oracle,
exhaustive fixture, face and hand captures, audio wrapper, host registry,
and existing Big-Star and gray-release semantic oracles. Debug passed 5/5:
the fixture, combined actor oracle, face and hand captures, and audio wrapper.
Logs are `out/ranked-actors-tests-Release.log` (150.42 seconds) and
`out/ranked-actors-tests-Debug.log` (802.90 seconds). Registry generation
and the 223-range seam manifest checks also pass.

## Previous priority: collision-call cabinet ranking (September 9)

This earlier user-supplied ranking is retained as iteration history.
Use the replay corpus for compatibility and target measurements, not to
replace this ranking. No total residue or input recording accompanied it.

| Rank | Address | Master cycles | Intervals |
| --- | --- | ---: | ---: |
| 1 | 02:4421 | 6,957 | 329 |
| 2 | 02:441E | 4,983 | 329 |
| 3 | 02:5F73 | 4,080 | 192 |
| 4 | 00:FA67 | 3,024 | 141 |
| 5 | 03:7C34 | 2,208 | 104 |
| 6 | 00:E438 | 1,962 | 327 |
| 7 | 45:49E3 | 1,776 | 296 |
| 8 | 45:4B85 | 1,683 | 39 |
| 9 | 45:485D | 1,122 | 187 |
| 10 | 03:66A3 | 1,110 | 38 |
| 11 | 00:EB1A | 1,104 | 92 |
| 12 | 03:66AA | 927 | 57 |

The first three ranked addresses were handled in preceding rounds. This round
reduces the guarded residue at 00:FA67; the next target is 03:7C34, subject
to any newer user-supplied ranking.

## Tile-upload branch event horizon: 00:FA67

The fourth address in the latest ranking is already native away from events.
Its guard now requires both event horizons to exceed 12 master cycles rather
than the entire 18/24-cycle BBR0. The zero-page phase advances to 9, samples,
and completes at 12. When no event occurs in that phase, the next core
advance and the native checkpoint both reach the final sample at 15/21,
then complete at 18/24. The final sample replaces the earlier interrupt
latch before interrupt dispatch. No intermediate service boundary is added.
Events at or before 12 still require the core's original advance sequence.
PC, code/RAM mappings, RAM size, T flag, and collision BBS1 guards remain.

The existing exhaustive bit-branch fixture checks both event horizons for
every operand. The canonical oracle now admits 766/832 tile branches
(previously 741/832), while collision admission remains 373/404. Registry
membership remains 755 routines / 803 reachable keys, with 211 seam ranges.

The canonical route's tile residue falls from 1,908 cycles / 91 intervals to
1,368 / 66: 540 cycles removed (28.3%). All 31 paired registry routes remove
540 cycles each, reducing the corpus total at this address from 59,148 to
42,408 cycles (16,740 removed). Every frame hash and abandonment count is
unchanged; higharc retains its one pre-existing abandonment. Total interpreter
cycle deltas equal the tile-address deltas on every route. The comparison uses
the previous binary with its original guard, not exclusion of the whole port.
This measures the corpus; it does not replay the user's 141-interval session.
Evidence: `out/tile-horizon-cost/manifest.csv` and the paired profiles/logs.

Release passes all six selected checks (63.50 seconds); Debug passes all five
(601.64 seconds). Both include the exhaustive shared fixture, pinned bit-branch
assembly oracle with frame/call/RAM/hardware-write parity, face and hand
captures, and audio-wrapper interrupt regression; Release also checks the
host registry. Logs are `out/tile-horizon-tests-{Release,Debug}.log`.
Registry generation, the 211-range seam check, and `git diff --check` pass.
The complete test suite was not rerun.

## Tower-Insect source scheduler call: 02:5F73

This round follows the latest ranking's next address, 02:5F73 (4,080 reported
master cycles / 192 intervals). The previously unadmitted family helper
restricted this JSR to actor slot 2C and fixed the destination bank to 01.
It now delegates to the tested shared call implementation: preserve A/X/Y/P,
push return address 5F75 in hardware order through the wrapping live stack,
and reach C018 in the bank mapped by MPR6. Stack writes precede the cycle
charge; interrupt sampling is at 18 master cycles and completion at 21.
Live PC, code/RAM mapping, full RAM, clear T, and strict event-horizon guards
preserve fallback. Decimal mode is valid and the opcode needs no actor-slot
restriction. The other child-spawner continuations retain their own guards.

The shared fixture now exercises eight call sites, adding all 65,536
stack-pointer/status combinations for 5F73 and both event-horizon checks.
The higharc oracle pins JSR C018 and observes both 5F73 and scheduler C5C9.
Assembly and native frames, call registers/timing, and all 818,325 ordered
writes match across 1,635 observed calls. It accepts 163 of 192 source calls;
the other 29 retain event-horizon fallback.

Registry admission rises to 755 routines with 803 reachable keys. The seam
manifest has 211 non-overlapping ranges. Evidence is recorded under
`out/source-call-cost/`, `out/source-call-tests-{Release,Debug}.log`, and
`tests/verify_tower_source_scheduler_call.cmake`.

All 31 registry before/after route hashes and abandonment counts match.
Only higharc exercises 5F73: target residue falls from **4,080 to 657 master
cycles**, removing **3,423 cycles (83.9%)**; 29 guarded call intervals remain.
Each route's total interpreter-cycle delta equals its target delta. Higharc
has one pre-existing abandonment in both modes, also present before this
round; unchanged counts must not be described as zero abandonments. These
are replay measurements, not a claim to have replayed the cabinet session.
Release passes all 7 checks in 58.21 seconds, including the new oracle, shared
fixture, previous collision-call oracle, host deployment, both corruption
captures, and audio-interrupt regression.

Debug passes all 6 checks in 603.38 seconds: the shared fixture, new source
call and previous collision-call oracles, both corruption captures, and
audio-interrupt regression.

The next address in the user's ranking is 00:FA67. The current round does not
claim to remove the other supplied residues or replay the cabinet recording.

## Collision selector and table call: 02:441E / 4421

The native selector load reads $3C80,X using live X, updates A/N/Z, and
hands off at 4421 after 15 master cycles. The previously unadmitted JSR
helper now has live-PC, code/RAM-mapping, RAM-size, and clear-T guards.
It pushes return address 4423 high-byte first through the wrapping live
stack, then reaches 6966 in the bank selected by MPR3 after 21 cycles.
Both stack writes occur before the instruction's cycle charge, matching
the core; the old helper incorrectly timestamped them at cycle 15.
The JSR retains core fallback when either event horizon is at most 21.
Each instruction has its own interrupt checkpoint; the callee remains an
independent registry entry. Decimal mode is valid for both instructions.

The fixture covers 262,144 selector/status/slot combinations and 65,536
stack/status combinations, plus mapping and event-horizon guards. The weave
oracle pins the six ROM bytes and compares frames, call registers/timing,
and 21,092 ordered writes across 658 calls with assembly. It accepts all
329 selector loads and 285 of 329 JSRs; event horizons retain the remaining
44 calls in the core. The smaller state census also matches all three
artifacts, with 5/5 loads and 4/5 calls accepted.

Both addresses are admitted to the normal registry: 754 routines, 803
reachable keys. The seam manifest has 209 non-overlapping ranges. Release
passes all 6 checks in 59.17 seconds, including the fixture, ROM oracle,
host deployment, both corruption captures, and audio-interrupt regression.

Registry comparisons preserve all 31 route hashes and abandonment counts.
Target residue falls from **32,040 to 2,313 master cycles** across the corpus,
removing **29,727 cycles (92.8%)**. The affected routes are sweep
(19,884 to 1,245), weave (12,084 to 1,068), and alley (72 to 0). The selector
load has no remaining interpreter intervals; the residual call intervals
retain event-horizon fallback. Every route's total interpreter-cycle delta
equals its target delta. These are replay measurements, not an extrapolated
reduction of the user's unprovided cabinet recording.

Debug passes all 5 checks in 557.51 seconds: the fixture, collision-call
oracle, both corruption captures, and audio-interrupt regression.

Evidence: `out/collision-call-cost/`,
`out/collision-call-tests-{Release,Debug}.log`, and
`tests/verify_collision_table_call.cmake`.

## Previous priority: Three-Blobs cabinet report (September 9)

That report superseded the earlier Gray-Blob / side-worm
priority below. Baseline: 196,506 non-wait interpreter master cycles across
14,984 intervals and 2,321 addresses; 750 of 751 admitted routines deployed,
with C500 claimed by the host and zero transcript abandonments.

Use this address ranking for the next round, grouping related instructions
when implementing and measuring a path:

| Rank | Address | Master cycles | Intervals |
| --- | --- | ---: | ---: |
| 1 | 03:66A6 | 5,874 | 519 |
| 2 | 45:49E3 | 4,410 | 731 |
| 3 | 03:7E7E | 4,035 | 191 |
| 4 | 03:66A3 | 3,342 | 102 |
| 5 | 03:7E76 | 2,904 | 192 |
| 6 | 03:7E6A | 2,880 | 192 |
| 7 | 03:7E73 | 2,880 | 192 |
| 8 | 02:5009 | 2,772 | 180 |
| 9 | 02:5004 | 2,700 | 180 |
| 10 | 45:4B85 | 2,493 | 57 |
| 11 | 00:FA67 | 2,430 | 111 |
| 12 | 03:7E79 | 2,322 | 192 |

Start with 03:66A6 and its 03:66A3 prefix (9,216 reported cycles combined).
The existing first-movement helper has a 300-cycle timer-horizon guard tied
to a known twist-route device timing discrepancy. Audit instruction timing
and continuation coverage before changing that guard. Then inspect 45:49E3
and the related 03:7E6A-7E7E path (15,021 listed cycles combined).

The replay corpus remains compatibility evidence; it does not override this
priority. This report has no accompanying input recording, so replay savings
must be labeled by route. Do not infer a before/after improvement by comparing
this session's total with the earlier, different cabinet session.

## Three-Blobs timer branch: 03:66A6

This round implements the first address in the latest cabinet ranking. The
existing 66A3 prefix uses a core DEC fallback within its 300-cycle timer
horizon; execution then reaches BNE at 66A6. The new independent branch
reads the live zero flag, takes 12 master cycles to reach 66B5 when nonzero,
or 6 to reach 66A8 on expiry, and samples interrupts three cycles before
completion. It preserves all other registers, flags, mapping, and RAM.
Live PC and code-bank guards plus a clear-T guard retain core fallback.
Decimal mode is valid. The longer prefix and its timing guard are unchanged.

The status/horizon fixture covers 77,312 cases. The pinned-ROM census oracle
accepts all 1,107 branch calls and compares frame, full RAM-write, and full
bus-write artifacts against assembly and fixed hashes. Registry admission
rises to 752 routines (802 dispatch-reachable); the execution-seam manifest
has 207 non-overlapping ranges.

Registry before/after comparisons preserve all 31 route hashes and abandonment
counts. At 66A6, residue falls from **14,928 to 0 master cycles** across the
corpus, including **1,374 to 0** on the timing-sensitive twist route. Each
route's total interpreter-cycle delta equals its 66A6 delta. These are corpus
measurements, not a replay of the user's cabinet session. Release passes all
7 checks in 120.22 seconds, including the new oracle, existing prefix oracle,
host deployment, both corruption captures, and audio-interrupt regression.
Debug passes all 5 checks in 847.76 seconds: the fixture, new full-trace
oracle, both corruption captures, and audio-interrupt regression.

Evidence: `out/three-blobs-branch-cost/`,
`out/three-blobs-branch-tests-{Release,Debug}.log`, and
`tests/verify_three_blobs_timer_branch.cmake`. This change targets 66A6;
remaining interpreter residue at 66A3 is not counted as removed.

## Gray-Blob live update: 03:68BF (cabinet ranking)

The earlier September 9 cabinet report drove this completed iteration.
Its leading addresses are 03:68BF (17,577 cycles), 03:68D9 (17,505), and
03:68D1 (12,603). The synthetic 31-route aggregate is retained for
compatibility checks, not used to override this live-play ranking.

The old Gray-Blob helper required three non-expiring timers, then incorrectly
modeled a phase decrement at 6905 and an RTS at 690D. The verified ROM instead
has RTS at 6905; the phase countdown begins at 68FD on a separate path after
movement. The new update follows the actual ROM, performs appearance expiry
(reload 4 and toggle sprite bit 0), loads live collision state, and decrements
the movement timer. It returns through the live stack at 6905 or hands off at
68DE for movement expiry / 6906 for collision response. It never decrements the
phase timer on the steady path. All instructions retain their interrupt
samples, cycle costs, register flags, and ordered RAM writes. Entry guards
require live PC, RAM/code mapping, full RAM, and clear T; decimal mode is valid.

03:68BF is now admitted through the normal registry, replacing its auxiliary
hook. This also makes `--registry-exclude 03:68BF` a real before/after control.
The legacy Gray-blob-wait entry counter remains available in the report.

Validation and measurement:

- Exhaustive fixture: 1,048,576 timer/status/collision/slot combinations,
  including timer wrap, appearance expiry, collision selection, stack wrap,
  mapping guards, and the actual 6905 return. The phase timer is deliberately
  set to 1, which the old classifier rejected.
- ROM oracle: the established Gray-Blob activation route (gameplay with
  `--work-ram-patch 1300:2CC0:80`) accepts all 492 calls. All frame, full RAM-write,
  and bus-write hashes match assembly. The opcode bytes and RTS are pinned.
- Registry comparison on that activation route: 68BF-6905 interpreter residue
  falls from **52,911 to 690 cycles**, removing **52,221 cycles (98.70%)**.
  Non-wait residue falls from 8,120,112 to 8,067,891, with identical frame hashes
  and zero abandonments. The remaining 690 cycles belong to movement-expiry
  and post-movement code beyond the ported prefix. The three leading Gray-Blob
  addresses have zero residual interpreter intervals in the after profile.
- Release: 6/6 checks pass in 54.46 seconds, including the fixture, ROM
  oracle, host deployment, corrupted-face capture, hand-bus capture, and
  audio-interrupt regression. Debug: the fixture, oracle, both corruption
  captures, and audio-interrupt regression pass 5/5 in 475.80 seconds.
- Evidence: `out/gray-blob-update-cost/activated.{before,after}.*`,
  `out/gray-blob-tests-Release.log`, `out/gray-blob-tests-Debug.log`, and
  `out/gray-blob-cost-{0,1,2}.log`.

All 31 compatibility-route hashes and abandonment counts match. Only `twist`
exercises this target materially: 34,875 to 1,728 cycles (33,147 removed).
This coverage gap is why the cabinet report takes precedence over the corpus
aggregate when choosing the next port.

The previous ranking proposed the side-worm waiting update next at
02:5CC5 / 5CC8 / 5CCB (48,288 reported cycles combined). The remaining supplied
leaders are 02:56AB (12,534), 00:E60A (11,388), 02:59F4 (11,370), and
00:E5EB / E5EC / E836 (10,056 each). Measured improvement
above is for the activation replay, not an extrapolated reduction of the
user's unprovided input recording.

## Actor queue asset call and reload: 01:C624 / C627

This iteration targets C624-C630, the largest remaining primary-ranking
range at 78,726 interpreter cycles. C624 now performs the original JSR
F605 with the live stack: it pushes C626 in hardware order, preserves
registers and mapping, samples interrupts at cycle 18, and completes at 21.
Both event horizons must exceed 21 cycles. The asset loader remains a
separate entry with its own execution contract.

C627 independently reads $3080,X from the live return state, tests bits 5
and 2, and hands off at C654, C632, or C63E. It does not repeat C606's
classifier or reload X from $4D, since the helper can have changed actor
state and registers. The three paths cost 33, 39, and 45 cycles; every
instruction samples interrupts three cycles before completion. HuC6280
immediate BIT updates N/V/Z; other status bits and unrelated state are
preserved. Short RAM, foreign
MPR1/MPR6, mismatched PC, and T mode retain the core path. The first-queue
suffix C632-C63D and the original C654 RTS are unchanged.

The fixture adds 196,608 live attribute/status/slot cases with $4D deliberately
different from X. It checks all three decisions, flags, timing, absence of
writes, and entry guards. C624 joins the shared call fixture across every
stack pointer/status combination, including wrapped pushes and both event
horizons. The isolated gameplay oracle pins 14 ROM bytes at offset 9764,
accepts C624 4/4 and C627 35,353/35,353, and matches the five pinned assembly
artifacts (state, scheduler calls, call writes, RAM writes, device writes).
Registry admission is now 750 routines from 801 reachable keys; 204 seam
ranges remain classified.

Across all 31 routes, target residue falls from 78,726 to 2,418 cycles,
a reduction of 76,308 (96.93%). Non-wait residue falls from 3,838,305 to
3,761,997 cycles (1.99%); total interpreter cycles fall from 18,080,821,839
to 18,080,745,531. Frame-sync waiting stays at 18,076,983,534 cycles and
executed interpreter addresses fall from 3,516 to 3,511. All route hashes
and abandonment counts match; every total delta equals its target delta.
This measures removed interpreter work, not host execution speed.

Five targeted checks pass in both Release (57.63 seconds) and Debug
(474.22 seconds): the expanded scheduler fixture, new assembly oracle,
face capture, hand-bus capture, and audio-interrupt regression. Logs:
`out/actor-queue-reload-tests-Release.log` and
`out/actor-queue-reload-tests-Debug.log`. Both face replays retain the
corrected hash; the three original corruption capture files remain unchanged.

Evidence: `out/actor-queue-reload-cost/manifest.csv`,
`out/actor-queue-reload-cost-summary.log`,
`out/actor-queue-reload-cost/after-interpreter-residue.md`, and
`out/actor-queue-reload-registry-check.log`.
Next priority is palette E994-E9A8 (75,516 cycles), followed by the other
largest removable ranges in the primary ranking.

## Actor scan cursor admission: 01:C500

The largest primary-ranking range, C500-C50E, already had a native walker
used by the cabinet flipper hook but lacked ordinary registry admission.
Admitting C500 removes all 8,664,411 measured interpreter cycles in that
range. Across 31 routes, non-wait residue falls from 12,502,716 to 3,838,305
cycles (69.30%); total interpreter cycles fall from 18,089,486,250 to
18,080,821,839. Frame-sync waiting remains 18,076,983,534 cycles. Executed
interpreter addresses fall from 3,523 to 3,516. Every route state hash and
abandonment count matches, and each total cycle delta equals its cursor
delta. These are interpreter-residue measurements, not host speedup claims.

The existing seven-instruction suffix resets $2056, $2CBA, $2CBC, and
$204D, sets $2CBB to $7F, and resumes at C510 after 81 master cycles.
Each instruction retains its interrupt sample three cycles before completion.
The walker now also checks the live PC and MPR1/MPR6 mappings. D/T modes
and short RAM continue to defer to the core. C4DE still stops at C500 after
writing the enable mask, preserving the cabinet input boundary.

Live cabinet play, captured-session replay, and the launch flipper probe
explicitly claim C500 before registry installation when they restore input.
The host callback restores flipper input and then invokes the same cursor
walker. Initial regression runs caught a duplicate-hook collision; the
explicit claims resolve it. The host smoke test now requires the named
C500 claim, so ordinary admission cannot silently occupy the cabinet key.

The cursor fixture checks 65,536 register/status cases, output flags,
checkpoint/write timing, and PC/mapping/RAM guards. The isolated gameplay
oracle pins the 16 ROM bytes at offset 9472, accepts 1,703/1,703 entries,
and matches all five pinned assembly artifacts: state, scheduler calls,
call writes, RAM writes, and device writes. The registry contains 748
admitted routines and 799 reachable keys. The seam manifest retains 204
ranges and classifies the cursor body as guarded while naming host ownership.

Validation passes: seven Release checks (67.15 seconds) and six Debug checks
(494.69 seconds), covering the cursor fixture/oracle, host registry, face
capture, hand-bus capture, and audio-interrupt regression; Release also
checks the original C4DE preamble. The launch probe passes all four camera
and input configurations. A final Debug host check verifies the explicit
C500 claim. Logs: `out/actor-scan-cursors-tests-Release-final.log`,
`out/actor-scan-cursors-tests-Debug.log`,
`out/actor-scan-cursors-launch-probe.log`, and
`out/actor-scan-cursors-host-Debug-final.log`. Both face replays retain the
corrected hash; the three original capture files remain unchanged.

Evidence: `out/actor-scan-cursors-cost/manifest.csv`,
`out/actor-scan-cursors-cost-summary.log`,
`out/actor-scan-cursors-cost/after-interpreter-residue.md`, and
`out/actor-scan-cursors-registry-check.log`.
The next primary-ranking targets are scheduler C624-C630 (78,726 cycles)
and palette E994-E9A8 (75,516 cycles). Continue prioritizing expected
removable cycles from the complete ranking.

## Audio filter and command setup: 00:ECFD / ED18 / ED23 / ED2C

This iteration targets the two largest non-host residue clusters together:
ECFD-ED07 (91,590 cycles) and ED18-ED34 (84,462 cycles). The filter stores
A in $2A, rejects command FF, and tests the live $2CC7 tilt latch. With a
nonzero latch it accepts only commands 31, 27, and 47. Its rejected paths
hand off at the original ED03/ED17 RTS boundaries. ED18, ED23, and ED2C
set the wrapper selector to 06, 00, or 09, set bank $45 in $30, and JMP
ED58. The existing ED58 wrapper retains live stack and mapper ownership.

Every transcribed instruction has its own interrupt checkpoint, sampled
three master cycles before completion. RAM writes carry the original
instruction and completion offset. Short RAM, foreign MPR1/MPR7, mismatched
PC/bank, and T mode defer to the interpreter. No caller frame is synthesized
and no mapper change is moved across the ED58 handoff.

The new fixture checks 196,608 command/status/filter cases, all three setup
entries across all status values, flags, timing, writes, and mapping guards.
The isolated gameplay oracle pins the 58 ROM bytes at offset 3325 and all
five existing gameplay artifacts (state, scheduler calls, call writes, RAM
writes, and device writes). Admission is 26/26 ECFD, 26/26 ED18, 1/1 ED23,
and 4/4 ED2C. The registry now contains 747 admitted routines and 799
reachable keys; the seam manifest contains 204 classified ranges.

The 31-route incremental registry comparison removes all 176,052 interpreter
cycles at the targeted paths. Non-wait residue falls from 12,678,768 to
12,502,716 cycles (1.39%); executed addresses fall from 3,542 to 3,523.
All state hashes and abandonment counts match, and every total cycle delta
equals its target delta. The 18,076,983,534 frame-wait cycles are unchanged.
Evidence: `out/audio-command-setup-cost/manifest.csv`, the before/after
interpreter-residue reports alongside it, and
`out/audio-command-setup-cost-summary.log`. This is a registry comparison;
the isolated oracle separately supplies the pinned assembly comparison.

Release and Debug each pass all five targeted checks: the new exhaustive
fixture and assembly/native oracle, the audio-wrapper interrupt regression,
the corrupted-face session, and the upper-hand bus session. Logs are
`out/audio-command-setup-tests-{Release,Debug}.log` (61.96 and 491.41 seconds).
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`;
the original capture inputs, state, and hiscore hashes are unchanged.
Both builds, registry generation/check, seam validation, and diff checks pass.
The full suite was not run.

Prioritize expected removable cycles in the primary ranking on the next
iteration. Audit the 8,664,411-cycle C500-C50E host range first: its cursor
suffix already has a native walker and the cabinet host installs it after
restoring flipper input, but it has no ordinary registry admission evidence.
The host label alone is not proof that this cost is unavoidable. Preserve
the input boundary when evaluating admission. The next non-host clusters
are scheduler C624-C630 (78,726 cycles) and palette E994-E9A8 (75,516);
investigate those before another small independent RTS.

## Actor-flag return: 01:C8E1

The actor-flag check after the C5C9 scheduler yield now admits its final RTS
independently. The oracle pins JSR C5C9 / LDA $3C80,X / AND #01 / BEQ +1 /
RTS as `20c9c5bd803c2901f00160` at ROM offset 10455. The predecessors do
not change the interrupt mask, so the existing atomic return transcript applies.
The return reads the live stack, wraps both stack indices and the saved
address, resolves its destination through live MPRs, preserves A/X/Y/P,
and reports an absorbed return at cycle zero without RAM writes.
Both event horizons must exceed 21 master cycles. Unsupported code/stack
mappings and short RAM views defer to the core.

The shared fixture covers 25 returns and 4,915,200 target/status/stack cases,
plus both horizons and mapping/RAM guards. `dc_rom_actor_flag_return`
compares five pinned alley artifacts, including countdown call observations,
RAM writes, and device writes; its isolated replay admits 101/118 returns.
The standard gameplay route does not enter this return. The registry contains
743 admitted routines and 795 dispatch-reachable keys; the seam manifest
contains 204 classified ranges.

Across all 31 scheduler-return routes, enabling this additional registry entry
reduces its interpreter cost from 10,104 to 1,641 master cycles: 8,463 cycles
removed (83.76%). Alley, lanes, stagger, and twist remove 2,121, 2,079, 2,121,
and 2,142 cycles respectively; the other routes have no target delta.
Non-wait interpreter cost falls from 12,687,231 to 12,678,768 cycles (0.07%).
Every frame hash and registry abandonment count matches, frame-wait cycles
remain 18,076,983,534, and the total interpreter delta equals the target delta.
This is an incremental registry comparison; the isolated oracle supplies
the pinned assembly comparison. Evidence is in
`out/actor-flag-return-cost/manifest.csv` and
`out/actor-flag-return-cost/{before,after}-interpreter-residue.md`.

Release and Debug each pass all five targeted checks: the shared return
fixture, actor-flag oracle, audio-wrapper regression, corrupted-face session,
and upper-hand bus session. Logs are
`out/actor-flag-tests-{Release,Debug}.log` (94.75 and 751.40 seconds).
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`;
the original capture inputs, state, and hiscore hashes are unchanged.
Both builds, registry generation/check, seam validation, and diff checks pass.
The full suite was not run.

## Frame-delay return: 00:E333

The frame-delay loop now admits its final RTS independently. The oracle
pins JSR E321 / DEC $2C / BNE E32C / RTS as `2021e3c62cd0f960` at ROM
offset 812. The loop still executes in the core; DEC and BNE do not change
the interrupt mask, so the existing atomic return transcript applies.
The return reads the live stack, wraps both stack indices and the saved
address, resolves its destination through live MPRs, preserves A/X/Y/P,
and reports an absorbed return at cycle zero without RAM writes.
Both event horizons must exceed 21 master cycles. Unsupported code/stack
mappings and short RAM views defer to the core.

The shared fixture covers 24 returns and 4,718,592 target/status/stack cases,
plus both horizons and mapping/RAM guards. `dc_rom_frame_delay_return`
compares five pinned gameplay artifacts, including scheduler call observations,
RAM writes, and device writes; its isolated replay admits 21/21 returns.
The registry contains 742 admitted routines and 794 dispatch-reachable keys.
The former E32C-E34C core range is split around the guarded return, yielding
203 classified ranges.

Across all 31 scheduler-return routes, enabling only this additional registry
entry reduces its interpreter cost from 13,671 to zero master cycles (100%),
or 441 per route. Non-wait interpreter cost falls from 12,700,902 to
12,687,231 cycles (0.11%). Every frame hash and registry abandonment count
matches, frame-wait cycles remain 18,076,983,534, and the total interpreter
delta equals the target delta. This is an incremental registry comparison;
the isolated oracle supplies the pinned assembly comparison. Evidence is in
`out/frame-delay-return-cost/manifest.csv` and
`out/frame-delay-return-cost/{before,after}-interpreter-residue.md`.

Release and Debug each pass all five targeted checks: the shared return
fixture, frame-delay oracle, audio-wrapper regression, corrupted-face
session, and upper-hand bus session. Logs are
`out/frame-delay-tests-{Release,Debug}.log` (60.01 and 505.10 seconds).
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`;
the original capture inputs, state, and hiscore hashes are unchanged.
Both builds, registry generation/check, seam validation, and diff checks pass.
The full suite was not run.

## Parent ascending returns: 09:70BB and 09:71E1

Both ascending animation phases now admit their final RTS independently of
parent-body execution. The oracle pins the shared CMP #06 / BEQ +1 / RTS
sequence (`c906f00160`) at ROM offsets 78007 and 78301. Neither predecessor
changes the interrupt mask. The return reads the live stack, wraps its stack
indices and saved address, resolves the destination through live MPRs, preserves
A/X/Y/P, and reports the absorbed return at cycle zero without RAM writes.
Both event horizons must exceed 21 master cycles; unsupported code or stack
mappings and short RAM views defer to the core.

The shared fixture covers 23 returns and 4,521,984 target/status/stack cases,
plus both horizons and mapping/RAM guards. `dc_rom_parent_ascending_returns`
compares five pinned gameplay artifacts, including scheduler call observations,
RAM writes, and device writes. Its isolated replay admits 22/23 and 15/19
returns respectively. The registry contains 741 admitted routines and 793
dispatch-reachable keys; the seam manifest retains 201 classified ranges.

Across all 31 scheduler-return routes, enabling these two entries in the
current registry reduces their interpreter cost from 27,342 to 3,255 cycles:
24,087 cycles removed (88.10%), or 777 per route. Non-wait
interpreter cycles fall from 12,724,989 to 12,700,902 (0.19%). Frame hashes,
registry abandonment counts, and frame-wait cycles match on every route; the
total interpreter delta equals the target delta. This measures the incremental
registry change; the isolated oracle supplies the pinned assembly comparison.
Evidence is in `out/parent-ascending-cost/manifest.csv` and
`out/parent-ascending-cost/{before,after}-interpreter-residue.md`.

Release and Debug each pass all five targeted checks: the shared return
fixture, ascending-return oracle, audio-wrapper regression, corrupted-face
session, and upper-hand bus session. Logs are
`out/parent-ascending-tests-{Release,Debug}.log`; builds, registry generation,
and seam validation also pass. Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the original capture inputs, state, and hiscore hashes are unchanged.
The full suite was not run.

## Attribute transaction return: 01:DA3F

The attribute update now admits its final RTS independently after the core
restores Y with PLY at DA3E, including early-exit callers. The oracle pins
PLY / RTS as `7a60` at ROM offset 14910. The return reads the live stack,
wraps both stack indices and the saved return address, resolves its target
through live MPRs, preserves A/X/Y/P, and reports an absorbed return at
cycle zero without RAM writes. Both event horizons must exceed 21 master
cycles; unsupported mappings and short RAM views defer. PLY does not
restore I, so the existing atomic return transcript applies.

The shared fixture covers 21 returns and 4,128,768 target/status/stack
cases, plus both event horizons and mapping/RAM guards.
`dc_rom_attribute_return` compares five pinned gameplay artifacts,
including scheduler call observations, RAM writes, and device writes.
Its isolated replay admits 2/2 returns. The registry contains 739 admitted
routines and 791 dispatch-reachable keys; the seam manifest retains 201 ranges.

Across all 31 registry routes, excluding only 01:DA3F for the before run,
target interpreter cost falls from 17,718 to 2,640 master cycles (85.10%),
removing 15,078 cycles. Every frame-trace hash and registry abandonment count
matches, and each total cycle delta equals the target delta. Non-wait
interpreter work falls from 12,740,067 to 12,724,989 cycles (0.12%);
frame-sync waiting remains 18,076,983,534 cycles. This measures the registry
increment, not fresh parity between the entire registry and assembly.
Evidence: `out/attribute-return-cost/manifest.csv`, the before/after residue
reports, and `out/attribute-return-cost-summary.log`.

Release and Debug each pass all five targeted checks: the shared return
fixture, attribute return oracle, audio interrupt regression, corrupted-face
session, and upper-hand bus session. Total times are 61.80 s Release /
513.72 s Debug. Logs: `out/attribute-return-tests-{Release,Debug}.log`.
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the three original capture inputs retain their checksums.
Registry generation/check and the seam check pass. The full suite was not run.

## Mapped RNG return: 00:E8E6

The shared RNG helper now admits its final RTS independently, reading the
live caller frame without assuming which actor requested a random number.
The oracle pins LDA $222B / RTS as `ad2b2260` at ROM offset 2275. The return
preserves A/X/Y/P and MPRs, wraps the stack indices and saved return address,
resolves the destination through live MPRs, and reports an absorbed return
at offset zero without RAM writes. Both event horizons must exceed 21 master
cycles; unsupported mappings and short RAM views defer. The preceding LDA
does not restore I, so this return uses the existing atomic transcript.

The shared fixture covers 20 returns and 3,932,160 target/status/stack cases,
plus both event horizons and mapping/RAM guards. `dc_rom_rng_return` compares
all five pinned alley artifacts, including countdown call observations.
The ordinary gameplay route does not enter this helper; alley independently
admits 46/53 returns. The registry contains 738 admitted routines and 790
dispatch-reachable keys; the seam manifest retains 201 ranges.

Across all 31 registry routes, excluding only 00:E8E6 for the before run,
target interpreter cost falls from 19,122 to 2,679 master cycles (85.99%),
removing 16,443 cycles. Every frame-trace hash and registry abandonment count
matches, and each total cycle delta equals the target delta. Non-wait
interpreter work falls from 12,756,510 to 12,740,067 cycles (0.13%);
frame-sync waiting remains 18,076,983,534 cycles. This measures the registry
increment, not fresh parity between the entire registry and assembly.
Evidence: `out/rng-return-cost/manifest.csv`, the before/after residue reports,
and `out/rng-return-cost-summary.log`.

Release and Debug each pass all five targeted checks: the shared return
fixture, RNG oracle, audio interrupt regression, corrupted-face session,
and upper-hand bus session. Total times are 96.09 s Release / 751.59 s Debug.
Logs: `out/rng-return-tests-{Release,Debug}.log`. Both face replays retain
SHA-256 `a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the three original capture inputs retain their checksums. Final Release
and Debug builds, registry generation/check, and the seam check pass.
The full suite was not run.

## Scripted actor countdown returns: 09:703C and 09:7174

The upper-table scripted actor countdowns now hand off to independently
admitted native live-stack RTSs. The final CMP #$06 / BEQ / RTS sequence is
pinned as `c906f00160` at ROM offsets 77880 and 78192. Both returns preserve
A/X/Y/P and MPRs, wrap stack indices and the saved return address, resolve
the destination through live MPRs, and report the absorbed return at offset
zero without RAM writes. Both event horizons must exceed 21 master cycles;
unsupported mappings and short RAM views defer. The preceding comparison
and branch do not restore I, so these returns use the atomic transcript.

The shared fixture covers 19 returns and 3,735,552 target/status/stack cases,
plus timer/user horizon and mapping/RAM guards. The new
`dc_rom_script_countdown_returns` oracle compares all five pinned gameplay
artifacts, including scheduler call observations. Independent admissions are
30/30 at each return. The registry contains 737 admitted routines and 789
dispatch-reachable keys; the seam manifest retains 201 ranges.

Across all 31 registry routes, excluding only these two hooks for the before
run, target interpreter cost falls from 39,060 to zero master cycles (100%),
removing 1,260 cycles per route. Every frame-trace hash and registry abandonment
count matches, and each total cycle delta equals the target delta. Non-wait
interpreter work falls from 12,795,570 to 12,756,510 cycles (0.31%);
frame-sync waiting remains 18,076,983,534 cycles. This measures the registry
increment, not fresh parity between the entire registry and assembly.
Evidence: `out/script-countdown-cost/manifest.csv`, the before/after residue
reports, and `out/script-countdown-cost-summary.log`.


Release and Debug each pass all five targeted checks: the shared return
fixture, scripted countdown oracle, audio interrupt regression, corrupted-face
session, and upper-hand bus session. Total times are 63.72 s Release / 531.89 s
Debug. Logs: `out/script-countdown-tests-{Release,Debug}.log`.
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the three original capture inputs retain their checksums. Final Release
and Debug builds, registry generation/check, and the seam check pass.
The full suite was not run.


## Small-Soldier delayed-spawn returns: 03:7D81 and 03:7E1F

The two delayed-spawn paths now use the shared live-stack RTS independently
of their existing waiting-path owners. Both returns follow DEC $3B00,X / BEQ,
pinned as `de003bf00160` at ROM offsets 32124 and 32282. The return preserves
A/X/Y/P and MPRs, wraps stack indices and the saved address, resolves the
destination through live MPRs, and reports the absorbed return at offset zero
without RAM writes. Both event horizons must exceed 21 master cycles;
unsupported mappings and short RAM views defer. The preceding branches do
not restore I, so these returns use the atomic transcript.

The shared fixture covers 17 returns and 3,342,336 target/status/stack cases,
plus strict timer/user horizons and mapping/RAM guards. The new
`dc_rom_small_soldier_returns` oracle compares all five pinned gameplay
artifacts, including scheduler call observations. Independent admissions are
163/192 at 03:7D81 and 166/192 at 03:7E1F. The registry contains 735 admitted
routines and 787 dispatch-reachable keys; the seam manifest retains 201 ranges.

Across all 31 registry routes, excluding only these two hooks for the before
run, target interpreter cost falls from 45,243 to 16,326 master cycles:
28,917 removed (63.91%). Every frame-trace hash and registry abandonment count
matches, and each total cycle delta equals the target delta. Non-wait
interpreter work falls from 12,824,487 to 12,795,570 cycles (0.23%);
frame-sync waiting remains 18,076,983,534 cycles. This measures the registry
increment, not fresh parity between the entire registry and assembly.
Evidence: `out/small-soldier-cost/manifest.csv`, the before/after residue
reports, and `out/small-soldier-cost-summary.log`.


Release and Debug each pass the shared return fixture and all four targeted
replay checks: Small-Soldier returns, audio interrupt regression, corrupted-face
session, and upper-hand bus session. Replay checks take 62.12 s Release /
486.50 s Debug. Logs: `out/small-soldier-tests-Release.log`,
`out/small-soldier-tests-Debug.log`, and `out/small-soldier-fixture-{Release,Debug}.log`.
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the three original capture inputs retain their checksums. Registry and seam
checks pass. The full suite was not run.


## Parent descending returns: 09:7071 and 09:71A9

The two parent descending paths now use the shared live-stack RTS at 7071 and
71A9. Both follow DEC $3980,X / BEQ, pinned as `de8039f00160` at ROM offsets
77932 and 78244. The return preserves A/X/Y/P and MPRs, wraps stack indices
and the saved address, resolves the destination through live MPRs, and reports
the absorbed return at offset zero without RAM writes. Both event horizons
must exceed 21 master cycles; unsupported mappings and short RAM views defer.
The preceding branch does not restore I, so the transcript remains atomic.

The shared fixture now covers 15 returns and 2,949,120 target/status/stack
cases, plus strict timer/user horizons and mapping/RAM guards. The new
`dc_rom_parent_descending_returns` oracle compares all five pinned gameplay
artifacts, including scheduler call observations. Independent admissions are
35/35 at 09:7071 and 26/35 at 09:71A9. The registry contains 733 admitted
routines and 785 dispatch-reachable keys; the seam manifest retains 201 ranges.

Across all 31 registry routes, excluding only these two hooks for the before
run, target interpreter cost falls from 45,570 to 5,859 master cycles:
39,711 removed (87.14%), or 1,281 per route. Every frame-trace hash and registry
abandonment count matches, and each total cycle delta equals the target delta.
Non-wait interpreter work falls from 12,864,198 to 12,824,487 cycles (0.31%);
frame-sync waiting remains 18,076,983,534 cycles. This measures the registry
increment, not fresh parity between the entire registry and assembly.
Evidence: `out/parent-descending-cost/manifest.csv`, the before/after residue
reports, and `out/parent-descending-cost-summary.log`.

Release and Debug each pass all five targeted checks: the shared return fixture,
parent descending oracle, audio interrupt regression, corrupted-face replay,
and upper-hand bus replay (62.10 s Release / 527.44 s Debug). Logs:
`out/parent-descending-tests-Release.log` and `out/parent-descending-tests-Debug.log`.
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the three original capture inputs retain their checksums. Registry and seam
checks pass. The full suite was not run.

## Parent holding returns: 09:7099 and 09:71C4

The two parent holding paths now use the shared live-stack RTS at 7099 and
71C4. Both follow DEC $3B00,X / BEQ, pinned as `de003bf00160` at ROM offsets
77972 and 78271. The return preserves A/X/Y/P and MPRs, wraps stack indices
and the saved address, resolves the destination through live MPRs, and reports
the absorbed return at offset zero without RAM writes. Both event horizons
must exceed 21 master cycles; unsupported mappings and short RAM views defer.
The preceding branch does not restore I, so the transcript remains atomic.

The shared fixture now exercises thirteen returns across 2,555,904
return target/status/stack cases, plus horizon and mapping guards. The standalone
gameplay oracle admits 29/32 entries at 7099 and 29/36 at 71C4 and compares
frame state, scheduler calls, caller writes, RAM writes and hardware writes to
the pinned assembly baseline. Registry generation admits 731 routines, with
201 classified seam ranges.

Across all 31 routes, isolated full-registry A/B runs excluding these returns
reduce their interpreter cost from 44,268 to 6,510 cycles (85.29%). Non-wait
interpreter work falls from 12,901,956 to 12,864,198 cycles (0.29%). Every route
retains identical frame hashes and abandonment summaries, and its total cycle
reduction equals the target reduction. Frame-sync waiting stays at
18,076,983,534 cycles. This measures the registry increment, not fresh parity
between the entire registry and assembly. Evidence:
`out/parent-holding-cost/manifest.csv`, the before/after residue reports, and
`out/parent-holding-cost-summary.log`.

Release and Debug each pass all five targeted checks: the shared return fixture,
parent holding oracle, audio interrupt regression, corrupted-face replay and
upper-hand bus replay (62.37 s Release / 498.74 s Debug). Logs:
`out/parent-holding-tests-Release.log` and `out/parent-holding-tests-Debug.log`.
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`,
and the three original capture inputs retain their checksums. Registry and seam
checks pass. The full suite was not run.

## Parent decay returns: 09:704E and 09:7186

Both sibling upper-table parent actors now return natively after their existing
7043/717B decay gates. The shared RTS reads the live F8 stack, wraps stack
indices and the saved address, and resolves the destination through the live
mapper. It preserves A/X/Y/P and mapper registers, writes no RAM, and reports
the absorbed return at offset zero. Both event horizons must be strictly beyond
21 cycles; unsupported code/stack mappings and short RAM views retain core
execution. The preceding BEQ does not restore the interrupt mask, so these
returns use the atomic transcript without the audio PLP/RTS checkpoint.

The shared fixture covers eleven returns and 2,162,688 target/status/stack
cases, plus both horizon guards and invalid mapping/RAM cases. The new
parent-decay oracle pins both identical DEC/LDA/ORA/BEQ/RTS sequences
(de003bbd00341d8034f00160), admits 48/48 standalone entries for each return,
and compares frame state, scheduler calls, ordered caller writes, RAM writes
and hardware writes against the pinned gameplay assembly baseline. Registry
generation admits 729 routines; the seam manifest retains 201 classified ranges.

Across all 31 routes, isolated full-registry A/B runs excluding these two
returns reduce their interpreter cost from 62,496 to zero (100%). Non-wait
interpreter work falls from 12,964,452 to 12,901,956 cycles (0.48%). Every route
retains identical frame hashes and abandonment summaries, and its total cycle
reduction equals the target reduction. Frame-sync waiting remains
18,076,983,534 cycles. This is incremental registry evidence, not a fresh
whole-registry assembly comparison. Evidence: `out/parent-decay-cost/manifest.csv`,
the before/after residue reports, and `out/parent-decay-cost-summary.log`.

Release and Debug both pass the shared return fixture, parent-decay parity
oracle, audio wrapper interrupt regression, corrupted-face replay and upper-hand
bus replay. The initial oracle run rejected an incorrectly calculated ROM file
offset; correcting the test offsets produced passing dedicated reruns (38.97 s
Release / 340.76 s Debug). The other four checks passed in their original runs.
Logs: `out/parent-decay-tests-Release.log`, `out/parent-decay-tests-Debug.log`,
`out/parent-decay-oracle-Release.log` and `out/parent-decay-oracle-Debug.log`.
Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`;
the three original capture inputs retain their checksums. Registry generation,
seam verification and `git diff --check` pass. The full suite was not run.

## Mercy-Star return: 02:4F92

The Mercy-Star handler now returns natively at 4F92 after the 4F89 load,
4F8B mask and 4F8D branch. The shared RTS transcript consumes the live F8
stack frame, wraps both stack indices and the saved address, and resolves
the destination through the live mapper. It preserves A/X/Y/P and mapper
registers, performs no RAM writes, and retains the return observation at
offset zero. Both event horizons must be strictly beyond its 21 cycles;
unsupported code/stack mappings or a short RAM view retain core execution.
The preceding branch does not restore the interrupt mask, so this return
uses the existing atomic path; the audio PLP/RTS checkpoint remains specific
to 45:4B85.

The shared fixture covers nine returns and 1,769,472 target/status/stack
cases, including address wrap, live mapper resolution and both horizon
guards. The alley oracle pins the preceding instructions and RTS bytes
(a5532902d0034cc84d60), admits 63/63 standalone entries, and compares frame
state, countdown call observations, ordered caller writes, RAM writes and
hardware writes against the pinned assembly baseline. The registry now
admits 727 routines with 201 classified seam ranges.

Across all 31 replay routes, isolated full-registry A/B runs excluding only
02:4F92 before the change reduce this return from 34,224 to 3,858 interpreter
cycles: 30,366 removed (88.73%). Non-wait interpreter work falls from
12,994,818 to 12,964,452 cycles (0.23%). Every route retains identical frame
hashes and abandonment summaries, and its total cycle reduction equals the
target reduction. Frame-sync waiting remains 18,076,983,534 cycles. This is
incremental registry evidence, not a fresh whole-registry assembly comparison.
Evidence: `out/mercy-star-cost/manifest.csv`, the before/after residue reports,
and `out/mercy-star-cost-summary.log`.

Release and Debug each pass all five targeted checks: the shared return
fixture, Mercy-Star parity oracle, audio wrapper interrupt regression,
corrupted-face replay and upper-hand bus replay (94.45 s / 707.97 s).
Logs: `out/mercy-star-tests-Release.log` and
`out/mercy-star-tests-Debug.log`. Both face replays retain SHA-256
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`;
the three original capture inputs retain their checksums. Registry generation,
seam verification and `git diff --check` pass. The full suite was not run.

## Audio mapper return: 00:ED85

The three-bank audio wrapper now returns natively at ED85 after its existing
ED75 continuation restores the caller's mapper shadows and X. The independent
RTS reads the live F8 stack, wraps both stack indices and the saved address,
and resolves the destination through the restored live mapper. Its 21-cycle
transcript preserves A/X/Y/P and the caller's return observation at offset
zero. Both event horizons must be strictly beyond the instruction; unsupported
code/stack mappings or a short RAM view decline to the interpreter.

The shared fixture now covers eight return entries and 1,572,864
return target/status/stack cases. The mapper-return oracle pins the PLX/RTS
ROM bytes fa60 and admission at 28/32 standalone gameplay entries. Frame
state, scheduler calls, ordered caller writes, RAM writes and hardware writes
must match the pinned assembly baseline. The registry admits 726 routines
and the seam manifest retains 201 classified ranges.

All 31 registry A/B routes preserve frame-state hashes and abandonment
counts with only ED85 excluded on the before side. Its interpreter cost
falls from 37,608 to 5,814 master cycles, removing 31,794 (84.54%). Each
route's delta is confined to ED85; non-wait interpreter work falls from
13,026,612 to 12,994,818 (0.24%). Frame-sync waits are unchanged. This is
an incremental registry comparison, not fresh whole-registry assembly parity.
Evidence is in out/audio-mapper-cost/manifest.csv, its before/after rankings
and out/audio-mapper-cost-summary.log.

Release and Debug each pass five targeted checks: the shared fixture, mapper
return oracle, prior audio return/IRQ oracle, face capture and hand-bus
capture. Total times are 59.30 and 489.16 seconds; logs are
out/audio-mapper-tests-{Release,Debug}.log. Both face replays retain the
corrected a263ebb4...b35a hash, and the original capture inputs are unchanged.
Registry/seam validation and git diff --check pass. The full suite was not run.

## Audio wrapper return: 45:4B85

The audio wrapper now independently admits its final RTS through the shared
live-stack return helper. PLP at 45:4B84 remains interpreted so its old-mask
interrupt sampling completes before the return sees the restored status.
The RTS requires matching live PC and code mapping, the F8 work-RAM stack,
a full RAM view, and both event horizons strictly beyond 21 master cycles.
It preserves registers and status, wraps both stack reads and the return
address, and retains the caller's return observation at offset zero.
An explicit checkpoint samples interrupts at cycle 18 and completes at 21.
This is required after PLP clears I with an already-pending IRQ; without
that sample, the old-mask latch survives for an extra opcode. The pinned
stagger registry regression covers the resulting frame-2277 RAM mismatch.

The shared fixture covers 1,376,256 return target/status/stack cases across
seven entries, including wraparound and both horizon guards. The new
verify_audio_wrapper_return.cmake oracle pins ROM bytes 2860 and admission
at 8/26 standalone gameplay returns. Frame state, scheduler call traces,
ordered caller writes, all RAM writes and hardware writes must match the
pinned assembly baseline. The registry now admits 725 routines; the seam
manifest retains 201 classified ranges.

All 31 current-registry A/B routes preserve frame-state hashes and
abandonment counts when only 45:4B85 is excluded on the before side.
Return interpreter cost falls from 60,558 to 51,840 master cycles:
8,718 removed (14.40%), with each delta confined to that address.
Non-wait interpreter work falls from 13,035,330 to 13,026,612 (0.067%);
frame-sync waits are unchanged. PLP remains interpreted, and most RTS
cost remains behind the event guards. This measures the increment within
the registry, not fresh whole-registry assembly parity. Evidence is in
out/audio-wrapper-cost/manifest.csv, its before/after rankings and
out/audio-wrapper-cost-summary.log.

Release and Debug each pass four targeted checks: the shared return fixture,
the audio return oracle (including the stagger regression), corrupted-face
capture and hand-bus capture, in 60.60 and 471.89 seconds respectively.
Logs are out/audio-wrapper-tests-{Release,Debug}.log. Both face replays
retain the corrected a263ebb4...b35a hash; the original capture inputs are
unchanged. Registry/seam validation and git diff --check pass. The full
suite was not run.

## Release submission call and return: 02:51D2 and 02:51D5

The release-countdown tail was a 67,662-cycle interpreter cluster across the
31-route corpus. Its JSR $5475 and caller RTS now use the shared native call
and return helpers. The JSR writes the real $51D4 caller frame, high byte
then low byte with page-wrapped SP, and hands off to a separate live entry
at 02:5475. It samples at master cycle 18 and completes at 21. The RTS reads
the live stack, wraps the return address, and preserves the return observer
at offset zero. Both require matching live PC/code mapping, work-RAM stack
mapping, a full RAM view and both event horizons strictly beyond 21 cycles.
The call also declines transfer mode. The child remains independently owned;
no submission body or caller continuation is absorbed.

The shared fixture now covers 393,216 call stack/status combinations and
1,179,648 return target/status/stack combinations, plus both horizon guards
and invalid PC, mapping and RAM cases. The alley-route assembly oracle
verify_release_submission_tail.cmake pins ROM bytes 20755460 and admits
59/68 calls and 310/340 returns. Frame state, observed countdown/submission
calls, ordered caller writes, all RAM writes and hardware writes match
assembly byte-for-byte. The registry admits 724 routines and the seam
manifest validates 201 classified ranges.

All 31 current-registry A/B routes preserve frame-state hashes and
abandonment counts when only 51D2 and 51D5 are excluded on the before side.
Their interpreter cost falls from 67,662 to 5,943 master cycles: 61,719
removed (91.22%), with every cycle delta confined to those entries.
Non-wait interpreter work falls from 13,097,049 to 13,035,330 (0.47%);
frame-sync waits are unchanged. This isolates the increment within the
registry, not fresh whole-registry assembly parity. Evidence is in
out/release-submission-cost/manifest.csv, its before/after rankings and
out/release-submission-cost-summary.log.

Release and Debug each pass five targeted checks: the shared instruction
fixture, release submission assembly oracle, existing coroutine-call oracle,
corrupted-face capture and hand-bus capture (94.60 and 694.66 seconds).
Logs are out/release-submission-tests-{Release,Debug}.log. Both face replays
retain the corrected a263ebb4...b35a hash and all original capture inputs
are unchanged. Registry/seam validation and git diff --check pass. The full
suite was not rerun.

## Music active-voice test: 45:4095 and 45:4097

The highest remaining non-host cluster after the interrupt-vector port was
the music loop's AND #$01 / BEQ at 45:4095-4097 (174,816 interpreter master
cycles across 31 routes). Both instructions now run as independent native
entries. AND updates A and N/Z and resumes at 4097 in six master cycles;
BEQ uses the live Z flag and resumes at 409C in twelve cycles or 4099 in
six. Each samples interrupts three cycles before completion. Live PC and
bank-45 code mapping must match, T must be clear, and both event horizons
must strictly exceed the instruction cost. Other registers, the stack and
RAM are preserved. Near-event and transfer-mode cases retain the core opcode.

The shared instruction fixture checks 131,072 accumulator/status cases,
including both branch arms and both event horizons from 0 through 13,
plus invalid PC, code mapping and routine key. The dedicated
verify_music_voice_gate.cmake pins the ROM bytes 2901f003 and admits
10,625/11,097 AND entries and 10,374/11,097 branches. Its frame-state,
scheduler-call, ordered caller-write, RAM-write and hardware-write hashes
match the existing assembly baselines. The registry admits 722 routines;
the seam manifest validates 201 classified ranges.

All 31 current-registry A/B routes preserve frame-state hashes and
abandonment counts when only 4095 and 4097 are excluded on the before side.
Their interpreter cost falls from 174,816 to 6,516 master cycles: 168,300
removed (96.27%). Every cycle delta is confined to those two entries.
Non-wait interpreter work falls from 13,265,349 to 13,097,049 (1.27%);
frame-sync waits are unchanged. This isolates the increment within the
registry, not fresh whole-registry assembly parity. Evidence is in
out/music-voice-gate-cost/manifest.csv, its before/after rankings and
out/music-voice-gate-cost-summary.log.

Release and Debug each pass four targeted checks: the instruction fixture,
music voice gate assembly oracle, corrupted-face capture and hand-bus
capture (31.07 and 299.83 seconds respectively). Logs are
out/music-voice-gate-tests-{Release,Debug}.log. Both face replays retain
the corrected a263ebb4...b35a hash; the original capture inputs are unchanged.
Registry/seam validation and git diff --check pass. The full suite was
not rerun.

## Choose by remaining execution cost

The `out/corrupted-face` capture exposed a correctness bug in the resident
pattern loader at `00:F768`. At frame 3235 it enters with A=01, X=36 and Y=05.
The original `PHX / ASL A / TAY` saves actor 36 and selects descriptor 02;
the native code had saved A and used incoming Y, selecting the wrong graphics
and restoring actor 01. This corrupted the face and the actor scheduler.
The native implementation now reproduces all three instructions, their flags
and individual interrupt checkpoints. Both loader JSRs also write their stack
frames at instruction entry, matching the original ordered write traces.
No admitted routine or earlier return port was removed.

Release and Debug replay all 4,366 captured frames against the original loader with
the rest of the registry deployed. Frame RAM/video/audio fingerprints,
call registers/timing and ordered call RAM writes match; all three loader
entries are admitted 3/3 and there are zero abandonments. The corrected replay
hash is `9842d199628660ed77dbc2a9e8d777ddcf76e6a927d517a4d709ac873143a0ae`.
The face remains intact and enemies continue moving in the exported frames.
`verify_corrupted_face_session.cmake` preserves the capture and compares to
the original loader rather than asserting the captured, corrupted hashes.
CTest includes it when the local capture is available. A ROM-free fixture
checks 65,536 selector/status/stack cases and the face caller restoration.
The existing higharc regression now compares against actual assembly,
including call and ordered write traces, and is enabled in both configurations.
Both capture and higharc replays pass in Release and Debug; the Debug pair
takes 278.50 seconds. Both builds pass the ROM-free fixture. Four related
Release regressions (VRAM upload, VDC word transfer, shared scheduler return
and coroutine scheduler) also pass. Both executables are rebuilt; registry
generation remains current at 703 entries. The full CTest suite was not rerun.

The independent hand-setup mismatch at `02:5975` is now corrected. The native
prefix stops at `02:5983`, letting the core execute absolute `ADC $0080`
against the live mapping (normally VDC status). A new native `02:5986`
continuation consumes the resulting accumulator and flags, writes both timer
bytes, and composes the existing scheduler. The RNG RTS now takes 21 master
cycles and CLC takes six; their formerly compensating timing errors are gone.
The semantic helper accepts an explicit bus value instead of reading RAM
at offset 0080; its unit case deliberately makes those values differ.

The 4,366-frame capture matches the original hand setup with the rest of the
registry active: complete RAM/video/audio frame fingerprints, call timing
and registers, and ordered call RAM writes. Both native sections run 2/2,
with zero abandonments. The corrected replay hash is
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`.
This supersedes the earlier face-only replay hash because hand timers now
match the original hardware reads. The portable hand/RNG route retains its
existing trace/write hashes and admits the new suffix 1/1. The registry
contains 704 entries; the live read is a new explicit core seam (197 ranges).
`verify_upper_hand_bus_session.cmake` is included when the capture exists.
Release passes nine focused checks (core, both session comparisons, hand
RNG/expiry/opening/close, coroutine scheduler and shared scheduler return).
Debug passes core, both session comparisons (304.00/308.09 seconds), and the
hand/RNG regression (139.96 seconds). Its old Debug-only census expectation
was replaced with the Release hash after a fresh Debug assembly run produced
that same hash. Both executables are rebuilt. Registry and all 197 seam
ranges validate; the full suite and 31-route cost corpus were not rerun.

The extra assembly run exposed a separate `02:5991` countdown trace
mismatch: native `ORA $3B80,X` set N/Z but failed to update A. The native
output and the ORA/branch interrupt checkpoints now carry the combined timer
bytes in A. The hand/RNG call trace matches the original assembly hash
`9b2e47e80213caec538e613fc90bad2478ea03dafa19693b765db665d6e2b4d9`
in Release and Debug, correcting 26 output-register rows. A fresh Release
assembly expiry run also matches the corrected call hash
`95ccea8fcae9e51e81c4e2424c9703ee2f94895496075373f61fad4dd78a7d9b`.
All other existing artifact hashes remain unchanged. All nine Release hand
regressions pass after correcting those two call-trace expectations.
Artifacts: `out/hand-debug-assembly.csv*`, `out/hand-bus-release-tests.log`,
`out/hand-bus-debug-tests.log`, and `out/hand-bus-debug-rng-tests.log`.
The temporary frame-3200 checkpoint instrumentation remains removed.

The current priority is the larger return and scheduler boundaries, following
the user's request to pivot from smaller guarded bodies. The shared scheduler
RTS at `08:5976` was the largest remaining return (436,170 interpreted cycles).
It now reads the live caller stack through a standalone 21-cycle native
transcript, declining at either close event horizon. The leading host row
`01:C500-C50E` is already cabinet-owned input work; `01:CB68` and `00:FA67`
are compound branch seams, not returns.

The corpus `out/coverage-scheduler-return-corpus-v1` removes 374,976 actual
interpreted master cycles (2.34% of the prior non-wait residue), reducing
16,013,649 to 15,638,673. The return itself falls from 436,170 to 61,194
cycles, an 85.97% reduction. All 31 assembly/registry state-baseline pairs
and abandonment counts match the sequencer-gate corpus. Existing whole-
registry versus assembly differences remain; this is isolated return parity,
not a new claim of whole-registry parity. The remaining return executions
are classified as guarded and stay in the primary ranking.

The isolated gameplay replay admits 575/670 returns in Release and Debug and preserves
full frame state, call/stack traces, all RAM writes and all bus writes. A
new offset-zero observer guard prevents reporting the already-observed entry
RTS twice. All ten existing Release absorbed-return/coroutine regressions
pass. Release and Debug fixtures pass 196,608 return-target/status/stack cases
and both horizon guards. The registry now contains 703 entries, with 196
classified seam ranges. The full CTest suite and interactive crash capture
have not been rerun. Both executables are rebuilt with the new admission.

The live return at `01:C4C8` now shares the atomic live-stack transcript with
`08:5976`. It verifies its own code mapping and the F8 stack mapping, preserves
registers and flags, and declines at either close event horizon. The obsolete
automatic core-hook entry is removed because the registry now owns this RTS.
The registry contains 705 entries; C4C8 remains visible as a guarded seam.
The exhaustive fixture covers 393,216 return-target/status/stack cases across
both addresses, plus mapping, RAM-size and event-horizon guards.

Release and Debug gameplay admit 712/860 C4C8 returns and match original
assembly frame fingerprints, call registers/timing, ordered call writes,
all RAM writes and all bus writes. The Debug comparison takes 284.99 seconds.
With the full registry, isolated before/after profiles on gameplay, higharc
and nudge preserve frame fingerprints and abandonment counts (0, 1 and 0).
Higharc C4C8 interpreter cost falls from 6,075 to 1,245 master cycles; nudge
falls from 6,075 to 1,476. Total interpreter cost falls by exactly those
4,830 and 4,599 cycles. Gameplay already absorbs all these returns in its
parent and is unchanged. Evidence is in `out/run-return-cost/` and
`out/run-return-cost.log`; this is a three-route comparison, not a rerun of
the full 31-route corpus. Both Release capture comparisons pass with the
new return deployed. The final Debug pentagram comparison also passes all
4,366 frames (276.35 seconds). Both executables are rebuilt; registry
generation and all 197 seam ranges validate. The full CTest suite was not rerun.

The delay scheduler at `01:C5EA-C603` now has a standalone native entry.
It stores the delay, consumes the live JSR frame, increments the continuation
with 16-bit carry, installs both actor fields and executes the indirect JMP.
Every instruction has an interrupt checkpoint. RAM/code mapping, actor-slot
and transfer-mode guards retain the core fallback; this jump does not report
an absorbed RTS. The ROM-free fixture covers 196,608 continuation/status/stack
cases across all 128 RAM actor slots, including stack and address wrapping.

Release compares the original assembly on gameplay (2/2 calls) and higharc
(23/23): frame fingerprints, call registers/timing, ordered call writes,
all RAM writes and all bus writes match. The portable gameplay regression
also pins the exact scheduler ROM bytes. With the full registry, gameplay,
higharc and nudge remove 294, 3,258 and 5,439 actual interpreted master cycles
respectively. No interpreted instructions remain in C5EA-C603 on these three
routes. Total interpreter costs fall by exactly those amounts; state hashes
and abandonment counts (0, 1, 0) are unchanged. This is 8,991 cycles across
three routes, not an extrapolation to the older 31-route corpus.

Both Release pentagram comparisons pass all 4,366 frames with the delay
entry deployed, retaining replay hash
`a263ebb43cf47f94e37011461ffd3c3657a3f94005c5523a746db13869a1b35a`.
Release also passes the existing coroutine scheduler regression and exhaustive
return/delay fixture. Evidence: `out/coroutine-delay-cost/`,
`out/coroutine-delay-cost.log`, `out/coroutine-delay-release-tests.log`,
and `out/coroutine-delay-higharc.*`. The registry contains 706 entries and
the seam manifest has 198 ranges. The full suite and 31-route corpus were
not rerun.

Debug also passes the pinned scheduler assembly comparison (294.39 seconds)
and the full pentagram loader comparison (296.24 seconds), with zero capture
abandonments. Both configurations pass the final exhaustive fixture, including
the mapping, transfer-mode and short-RAM guards. Both executables are rebuilt.
Debug evidence is in `out/coroutine-delay-debug-tests.log`.

The selection-controller RTS at `08:5A69` now shares the atomic live-stack
return implementation. Its parent still stops at the return boundary; the
standalone entry preserves registers/flags, checks the live stack and code
mappings, and declines when either event horizon is within 21 master cycles.
The exhaustive fixture now covers 589,824 return/status/stack cases across
three addresses, as well as both horizon guards. The gameplay assembly
comparison admits 517/550 returns and preserves frame fingerprints, both
parent and return call traces, ordered call writes, every RAM write and all
bus writes.

The fresh 31-route comparison in `out/selection-return-cost/` deploys the
current registry on both sides, excluding only `08:5A69` on the before side.
All state hashes and abandonment counts match. Its interpreter cost falls
from 358,050 to 21,483 master cycles: 336,567 removed, or 94%. Each route
removes exactly 10,857 cycles, with no interpreter-cost changes outside the
return. Total non-wait residue falls from 15,376,059 to 15,039,492 (2.19%).
This isolates the selection return against the current corrected registry;
it does not claim fresh whole-registry versus original-assembly parity.
The paired manifest and before/after primary rankings are retained there;
`out/selection-return-cost-summary.log` records the aggregate.

Release passes the new pinned assembly comparison, the selection-controller
regression, both pentagram comparisons and the exhaustive fixture. The capture
retains its corrected replay hash and zero abandonments. Evidence is in
`out/selection-return-release-tests.log`. The registry now contains 707 entries;
all 198 seam ranges validate. The full CTest suite was not rerun.

Debug also passes the selection-return assembly comparison (322.61 seconds),
the full pentagram regression (322.29 seconds) and the exhaustive fixture.
The capture hash remains unchanged. Both executables are rebuilt; Debug
evidence is in `out/selection-return-debug-tests.log`.

The interrupt-chain vector at `00:E003` now uses the guarded native jump
helper, handing off to E179 after 12 master cycles with its sample at 9.
Both event horizons must exceed 12; invalid PC, code mapping or transfer
mode retain assembly. The expanded fixture covers 262,144 stack/status
cases across four jumps. The interrupt-chain oracle pins the new ROM bytes
and 129/130 admissions while preserving all five existing assembly traces.
Registry generation admits 720 routines and the seam manifest verifies
200 ranges. Across all 31 isolated current-registry A/B routes, E003
interpreter cost falls 135,336 -> 2,916 master cycles (97.85%); non-wait
interpreter work falls 13,397,769 -> 13,265,349 (0.99%). Frame-state hashes
and abandonment counts match, and every delta stays at E003. See
`out/interrupt-vector-cost/manifest.csv` and its before/after primary
rankings. This is incremental registry evidence, not whole-registry
assembly parity.

The actor countdown return at `02:4F75` now uses the shared native RTS
helper with live stack reads, wrapped return targets, caller observations
and strict 21-cycle event guards. The fixture covers 983,040 return cases
across five entries, plus horizon, mapping, RAM-size and PC rejection.
The alley-route assembly oracle admits 188/223 returns and pins matching
frame, caller and write artifacts. Registry generation now admits 719
entries, and the manifest validates 199 ranges after separating the
countdown prefix from its independently guarded RTS.

All 31 paired current-registry routes preserve frame hashes and abandonment
counts. Countdown-return interpreter cost falls from 122,982 to 15,063
master cycles (87.75% removed), reducing non-wait interpreter work from
13,505,688 to 13,397,769 (0.80%). The delta is confined to 02:4F75; this
comparison isolates the increment within the registry. Evidence is in
`out/countdown-return-cost/manifest.csv` and its primary rankings.

Release and Debug each pass the countdown-return assembly oracle, expanded
fixture, preceding bit-branch oracle and face/hand capture regressions
(five checks each). Logs are `out/countdown-return-tests-{Release,Debug}.log`.
Both face replays retain the corrected `a263ebb4...b35a` hash, and the
original capture inputs are unchanged. Registry generation, all 199 seam
ranges and `git diff --check` pass. The full suite was not rerun.

The preceding increment addressed collision `01:CB68` and tile-upload
`00:FA67`, now execute natively when both event horizons exceed the entire
18/24-cycle bit-branch opcode. The final interrupt sample is equivalent
while no event or mask change intervenes; near events the core preserves
the two original samples. CB68 is BBS1, correcting the old BBS2 label.
The fixture covers 131,072 operand/status combinations, both horizons for
every operand, and invalid mappings, PCs and RAM. The assembly oracle
admits 373/404 collision entries and 741/832 tile entries, preserving five
pinned frame-state, scheduler-call and write artifacts. The registry
contains 718 entries and the seam manifest retains 198 classified ranges.

All 31 paired current-registry routes preserve frame-state hashes and
abandonment counts, with every interpreter cycle delta confined to the
two branches. Their cost falls from 1,120,458 to 112,980 master cycles
(89.92% removed); total non-wait interpreter work falls from 14,513,166
to 13,505,688 (6.94%). Evidence and refreshed primary rankings are under
`out/bit-branches-cost/`, with totals in `out/bit-branches-cost-summary.log`.
This isolates the increment within the current registry, not fresh
whole-registry assembly parity.

Release and Debug each pass the bit-branch oracle, expanded fixture,
preceding VRAM-stream oracle and face/hand captures (five checks each).
Logs are `out/bit-branches-tests-{Release,Debug}.log`. The corrected face
replay hash and original capture inputs are unchanged. Registry generation,
all 198 seam ranges and `git diff --check` pass; the full suite was not rerun.

The preceding VRAM stream boundaries `00:E479` and `00:E47C` execute natively. The
loop BRA shares the 12-cycle unconditional-branch helper; the RTS shares
the 21-cycle live-stack return helper. Both preserve their strict event
horizon guards. Fixtures cover 196,608 branch stack/status combinations
and 786,432 return target/status/stack cases. The new assembly oracle
observes the decoder caller and return, admits 6,321/6,773 branches and
20/20 returns, and preserves five pinned frame, call and write artifacts.
The registry now contains 716 entries.

All 31 paired current-registry routes preserve state and abandonment counts,
with only the two new entries excluded on the before side. Target interpreter
cost falls from 162,351 to 14,916 master cycles (90.81% removed); every
cycle delta stays at those addresses. Non-wait work falls from 14,660,601
to 14,513,166 (1.01%). Evidence and primary rankings are under
`out/vram-stream-tail-cost/`, with totals in
`out/vram-stream-tail-cost-summary.log`. This isolates the increment within
the current registry; it is not fresh whole-registry assembly parity.

Release and Debug each pass the new oracle, expanded fixture, existing
interrupt-chain oracle and face/hand captures (five checks each). Logs are
`out/vram-stream-tail-tests-{Release,Debug}.log`. Original capture files
and the corrected replay hash are unchanged. Registry generation, all
198 seam ranges and `git diff --check` pass; the full suite was not rerun.

The preceding interrupt-chain jumps `00:FC03` and `00:FC89` execute as separate native
12-cycle instructions, preserving registers and stack state with interrupt
sampling at cycle 9. Both event horizons must exceed 12 cycles. The fixture
covers 131,072 stack/status combinations plus horizon and mapping guards;
the assembly oracle admits 126/130 and 123/123 entries and preserves all five
pinned frame, call and write artifacts. The registry now has 714 entries.

All 31 current-registry routes preserve state and abandonment counts with
only those two entries excluded on the before side. Interpreter cost at the
jumps falls from 268,068 to 7,512 master cycles (97.20% removed), with no
cost changes elsewhere. Total non-wait work falls from 14,921,157 to
14,660,601 (1.75%). Evidence and refreshed primary rankings are under
`out/interrupt-chain-cost/`, with totals in
`out/interrupt-chain-cost-summary.log`. This measures the isolated increment
within the registry, not fresh whole-registry assembly parity.

Release and Debug each pass the new oracle, expanded fixture and face/hand
capture regressions. See `out/interrupt-chain-tests-{Release,Debug}.log`.
The original capture files and corrected replay hash are unchanged. Registry
generation, all 198 seam ranges and `git diff --check` pass; the full suite
was not rerun.

The preceding scheduler continuation calls `01:C69B`, `01:C6A6` and `01:C6BF` extend the
same native JSR implementation to five calls, preserving its stack order,
21-cycle completion and strict event-horizon guards. The expanded fixture
covers 327,680 stack/status combinations; the assembly oracle enables all
five calls with unchanged frame, call, RAM-write and bus-write hashes. New
admissions are 1/1, 42/48 and 1/1; the registry now has 712 entries.

All 31 paired current-registry routes preserve state and abandonment counts.
Excluding just the three new calls on the before side measures 61,026 to
8,064 interpreter master cycles (52,962 removed, 86.79%), with no changes
outside those addresses. Non-wait work falls from 14,974,119 to 14,921,157
(0.35%). Evidence and refreshed primary rankings are under
`out/scheduler-followup-cost/`; totals are in
`out/scheduler-followup-cost-summary.log`. This comparison isolates the new
calls; it does not establish fresh whole-registry assembly parity.

Release and Debug each pass the expanded call oracle, exhaustive fixture
and both face/hand capture regressions. Logs are
`out/scheduler-followup-tests-{Release,Debug}.log`. The original capture
files are unchanged. Registry generation, 198 seam ranges and
`git diff --check` pass; the full suite was not rerun.

The preceding two-call increment is recorded below for comparison.
Scheduler call seams `01:C693` and `01:C696` now have standalone native JSR
transcripts. They push the original return words in hardware order at cycle
zero, wrap the live stack pointer within its page, and hand off to the existing
`C5EA` and `C5C9` owners after 21 master cycles. Both event horizons must exceed
21 cycles; code/RAM mappings and transfer mode are guarded. The fixture covers
every stack pointer and status value for both calls and checks the horizon,
mapping, PC and RAM-size guards. `dc_rom_coroutine_calls` pins frame state,
call registers/timing, ordered call writes, every RAM write and all bus writes
against assembly, with 1/1 and 54/60 admissions. The registry has 709 entries.

All 31 current-registry routes match when excluding only these two calls on
the before side. State hashes and abandonment counts remain unchanged, with
no interpreter-cost changes outside the target addresses. Their cost falls
from 73,281 to 7,908 master cycles (89.21% removed); total non-wait work falls
from 15,039,492 to 14,974,119 (0.43%). The paired manifest and refreshed primary
rankings are in `out/coroutine-calls-cost/`; the aggregate is recorded in
`out/coroutine-calls-cost-summary.log`. This isolates the new calls against
the current registry, without claiming fresh whole-registry assembly parity.

Release passes five focused checks: the call and delay assembly oracles,
the exhaustive fixture, and both pentagram regressions. Debug passes the call
oracle, fixture and both pentagram regressions. Both configurations preserve
the corrected 4,366-frame capture hash and zero abandonments. The original
capture files are unchanged. Logs are `out/coroutine-calls-release-tests.log`,
`out/coroutine-calls-debug-tests.log` and
`out/coroutine-calls-capture-{Release,Debug}.log`. Registry generation,
all 198 seam ranges and `git diff --check` pass. The full suite was not rerun.

The higher
`00:E479-E47C` cluster contains 126,924 branch cycles and only 35,427 return
cycles; its aggregate should not be mistaken for a single expensive RTS.
Keep the close-horizon `08:5976` residual visible; the atomic guard is still
required by this implementation.

The standalone sequencer gate slice admits `45:404F`, reusing the existing
`404F-4070` calculation after an enclosing native call declines. It stops
at `4072`, `4081` or `408F`, preserving the following sequencer ownership.
The corpus `out/coverage-sequencer-gate-corpus-v1` reduces actual non-wait
interpreter residue from 16,107,579 to 16,013,649 master cycles: 93,930
removed (0.58%), with 15 fewer interpreted addresses. All 31 state-baseline
pairs and abandonment totals match the PSG horizon corpus. Existing
whole-registry versus assembly differences remain.
Release and Debug admit 19/19 entries and match the existing assembly hashes
for frame state, selected RAM writes, call/stack writes and all 699,251 PSG
and timer writes. Both builds pass 196,608 gate arithmetic/branch cases,
12,032 PSG horizon/stack cases and 196,608 sequence-end combinations.

Registry regeneration now preserves the mode-2 VBlank evidence through an
explicit admission marker and a dispatch key in the generator's supported
form. It also retains the historically verified `$4143` continuation whose
arrivals disappeared after the PSG horizon slice, and refreshes `$4142`'s
recorded count. The registry has 702 entries; all 196 seam ranges validate.
Both executables are rebuilt. The full CTest suite and interactive crash
capture were not rerun.

Next investigate `00:ECFD-ED07` (91,590 cycles) and its adjacent guarded
continuation `00:ED18-ED34` (84,462). Higher-ranked core return and scheduler
costs remain visible in the primary ranking.

The PSG horizon slice (`out/coverage-psg-horizon-corpus-v1`) preserves the
`45:4142-4146` prefix when the shared driver-state read at `$4148` would
miss the timer horizon. The native walk stops before that read and retains
the existing final guard for earlier reads. Actual non-wait interpreter
residue falls from 16,173,678 to 16,107,579 master cycles: 66,099 removed
(0.41%), with five fewer interpreted addresses. All 31 assembly/registry
state-baseline pairs and abandonment totals remain unchanged. The existing
whole-registry versus assembly differences remain.

The music-family replay now admits all 3,604 PSG entries; its 42 former
standalone `$4143` arrivals disappear. Release and Debug match the existing
assembly hashes for frame state, selected RAM writes, call/stack writes,
and all 699,251 PSG/timer writes. Both builds pass 12,032 timer-horizon/stack
cases and the existing 196,608 sequence-end combinations. The music replay
is now enabled in Debug as well as Release, with separate output paths.
Both executables are rebuilt; 701 routines and 196 seam ranges remain.
The full CTest suite and interactive crash capture were not rerun.

Next investigate the guarded sequencer body `45:404F-4070` (93,930 cycles).
Higher-ranked core return and scheduler costs remain in the primary ranking;
the remaining PSG shared-state read still belongs to the live core path when
the timer horizon is too close.

The VBlank mode-2 slice (`out/coverage-vblank-mode-two-corpus-v1`) ports
`00:E09E-E0CD` and enables the existing dispatch/control-shadow handoffs on
that mode. Actual non-wait interpreter residue falls from 16,340,988 to
16,173,678 master cycles: 167,310 removed (1.02%), with 36 fewer interpreted
addresses. The gain includes the dispatcher and common tail that previously
declined along with mode 2. All 31 assembly/registry state-baseline pairs and
abandonment totals are unchanged. The existing whole-registry versus assembly
differences remain; the isolated replacement has its own parity proof.

The alley and spiral routes admit 203/203 and 187/187 mode-2 entries. Release
compares complete frame state, all ordered RAM writes and timestamped device
writes; Debug uses the existing VBlank family's RAM ranges while retaining
full frame fingerprints and device-write coverage. Both configurations pass
the two-route replay (163.26 seconds in Release, 772.48 seconds in Debug) and
65,536 displacement/status cases, including byte wrapping, ADC overflow,
stack wrapping, instruction checkpoints and mapping/buffer guards. The
existing Release VBlank regression and five sorcerer regressions also pass.
The registry now admits 701 routines; all 196 seam ranges validate. The full
CTest suite and interactive crash capture were not rerun in this slice.

Next investigate the guarded PSG-programmer entry `45:4142-414A` (111,051
interpreted cycles) and its timer-state horizon before considering another
admission. Higher-ranked core return and scheduler boundaries remain visible
in the primary ranking.

The VDC attribute-body slice (`out/coverage-vdc-attribute-corpus-v1`) ports
`01:DA0E–DA3B` after the two live VDC reads, stopping before PLY/RTS.
Actual non-wait interpreter residue falls from 16,468,422 to 16,340,988
master cycles: 127,434 removed (0.77%), with 21 fewer interpreted addresses.
All 31 state-baseline pairs and abandonment totals match the preceding
actor-initialization corpus. Existing whole-registry/assembly differences
remain; this is not a claim of whole-registry parity.

The isolated gameplay test admits 2/2 entries and matches all four hashes
(state/frame, calls, RAM writes, timestamped bus writes) in Release and Debug.
Final CTests pass in 11.30 and 136.47 seconds respectively. Both builds also
pass 65,536 accumulator/attribute combinations, status/mapping/buffer guards,
and the existing VDC initializer, row, and word-transfer tests. Three Release
sorcerer regressions pass. The full CTest suite and user crash capture were
not rerun in this slice. Both executables are rebuilt; the registry now has
700 admitted routines and the seam manifest has 196 ranges.

Next substantive candidate: the VBlank mode-specific VDC write branch
`00:E09E–E0CD` (98,280 interpreted cycles), extending the existing VBlank
family while preserving its interrupt/stack boundaries. Higher-ranked core
return and scheduler boundaries remain visible in the primary ranking.

The actor-initialization slice (`out/coverage-actor-initialization-corpus-v1`)
ports four RAM bodies across `01:C655–C6BF`, preserving every C5EA/C5C9 JSR
as a separately owned scheduler boundary. Actual non-wait interpreter residue
falls from 16,590,567 to 16,468,422 master cycles: 122,145 removed (0.74%),
with 39 fewer interpreted addresses. The original range's remaining 134,307
cycles are scheduler calls, not another unimplemented RAM body. All 31
assembly/registry state-baseline pairs and abandonment totals are unchanged;
the existing whole-registry versus assembly differences remain.

Release and Debug isolated gameplay replays admit all four entries (1, 1,
48, and 1 calls), with identical state, call/write, and bus artifacts. Both
configurations pass 262,144 slot/value/segment combinations plus status,
mapping, buffer, CLX flag-preservation and invalid-entry guards. Three Release
sorcerer regressions pass. Both executables are rebuilt; 699 routines are
admitted and 195 seam ranges validate. The initial test accidentally used
the old executable while linking was pending, causing a command-line rejection
and executable lock; relinking after that process exited resolved it. The
isolated `out/actor-initialization-release-v2` replay and subsequent CTests
pass. No changes were reverted.

Next prioritize `01:DA0E–DA3B` (within the 155,280-cycle DA0E–DA3F range):
the selected-register reads actually precede this entry at DA06/DA0B. The
remaining body packs an attribute and writes the VDC. Preserve those earlier
reads and the PLY/RTS boundary while porting the arithmetic/write sequence.

The release-countdown slice (`out/coverage-release-countdown-corpus-v2`) ports
the full `02:51AF–51CF` body, including expiry, while retaining the 51D2 JSR
and 51D5 RTS. It fixes the dormant absolute $3980 store and completion-time
writes, removes X/SP restrictions, and bounds the full byte-wide ROM lookup.
Actual non-wait interpreter residue falls from 16,673,268 to 16,590,567 master
cycles (82,701 removed, 0.50%), with 15 fewer interpreted addresses. All 31
assembly/registry baseline pairs and abandonment totals are unchanged; the
existing whole-registry/assembly differences remain, not newly claimed parity.

Release and Debug isolated alley replays admit 340/340 calls, including 68
expiry events, with identical state, call/RAM-write and hardware-bus artifacts.
Both configurations pass 65,536 timer/state combinations plus the existing
65,536 appearance/stack cases. Three Release sorcerer regressions also pass.
Both executables are rebuilt, with 695 admitted routines. The first sweep
attempt caught a duplicate auxiliary registration before gameplay; the old
auxiliary hook was removed after graduation into the registry, then the entire
sweep passed. No changes were reverted. Next investigate the actual actor
initialization/continuation body in `01:C655–C6BF`, preserving scheduler calls
as separate boundaries instead of treating the whole range as unportable.

The VDC initializer slice (`out/coverage-vdc-initializer-corpus-v1`) ports the
complete `00:E271–E290` setup loop, retaining only its E292 caller return in
the core. Actual non-wait interpreter residue falls from 16,860,756 to
16,673,268 master cycles: 187,488 removed (1.11%), with 17 fewer interpreted
addresses. The 1,953 remaining cycles in this routine are the core RTS, not
a missing loop branch. All 31 assembly/registry state-baseline pairs and
abandonment totals remain unchanged; the pre-existing 31 whole-registry
versus assembly differences are not resolved by this change.

Release and Debug isolated replays admit all three calls and match frame
state, complete call/RAM-write traces, and hardware-bus writes. Both builds
pass 65,536 status/stack cases plus mapping/buffer guards and the existing
VDC transfer tests. Three focused Release sorcerer regressions pass, both
executables are rebuilt, and the registry now admits 694 routines. Next
prioritize the actual `02:51AF` release-countdown body (150,363 cycles), whose
disabled expiry continuation needs corrected absolute addressing and store
timing, rather than the larger but RTS-only `08:5A69` entry.

The appearance submission slice (`out/coverage-appearance-submission-corpus-v1`)
generalizes the existing `02:5475/5481` native arithmetic and stack handling to
all mapped callers, rather than admitting another core-only descriptor. The
former actor/depth guards left 159,222 interpreted cycles in the route corpus.
Non-wait residue falls from 17,019,978 to 16,860,756 cycles (0.94% overall;
97.81% of the former 5475–54B6 range), with 26 fewer interpreted addresses.
All 31 state baselines and abandonment totals are unchanged; the existing
whole-registry versus assembly differences remain, not newly resolved.
The alley regression exercises 68 deeper SP=F9 callers among 250 appearances,
and compares frame state, complete calls/RAM writes, and hardware writes.
The helper has 65,536 direct appearance/stack cases covering pointer carries,
overflow flags, intermediate writes, and mapping guards. Release and Debug
gameplay/alternate/alley replays pass, as do the focused sorcerer regressions;
both executables are rebuilt. Its following 54A3
actor controller is still retained. Next prioritize an actual larger body,
such as the E271 VDC initializer, over isolated core-coverage accounting.

The collision-vector seam (`out/coverage-collision-vector-corpus-v1`) installs
`01:CBF9` as an exact core RTS capability, preserving its synthetic vector
stack and leaving the enclosing collision transcript unfinished. All 31 route
state baselines and abandonment totals remain unchanged. Release and Debug
gameplay/alternate comparisons match state, enclosing vector-call/write traces
and device writes. The descriptor preserves all registers in 65,536 status/SP
combinations. Focused sorcerer regressions pass; both executables are rebuilt.

Reported non-wait residue changes from 17,698,350 to 17,019,978 cycles
(678,372 cycles) solely because this instruction is now admitted core coverage.
The original RTS opcode body still executes: this is not a C++ handler port or
a measured speedup. The registry remains at 693 entries and the 31 existing
assembly/whole-registry differences remain. Next prefer a controller such as
`02:5475-54B6` over additional core-only accounting changes.

The session-gate continuation (`out/coverage-session-gate-corpus-v1`) removes
all 227,004 remaining interpreted cycles at `01:C49E-C4A2` in the 31-route
corpus. Total non-wait residue falls from 17,925,354 to 17,698,350 cycles
(1.27%), and executed residue addresses from 3,738 to 3,735. Both sides of
all route baselines remain unchanged; the 31 pre-existing whole-registry
assembly differences are not resolved by this slice. The existing 693-entry
registry is unchanged: this extends a controller's missing branches rather
than adding leaf entries.

Release natural/forced-blocked/forced-pause replay compares full census,
call/write and device-bus traces; Debug additionally verifies pause/resume.
The pause fixture reaches `C4AE` and returns to normal play through frame
1,800. Direct tests exercise flag/input combinations and every JSR stack
pointer, including wrap. Idle-gate and sorcerer regressions pass. Release
and Debug executables are rebuilt. The child audio routine, subsequent wait
loop and early-exit RTS retain independent ownership.

The primary queue is now `registry-interpreter-residue.md` from
`Invoke-NativeCoverageSweep.ps1`. Each registry recording with
`--instruction-profile <path>` also writes `<path>.interpreter.csv`, containing
only actual interpreted intervals. The sweep merges these files, excludes the
frame-sync wait, and ranks every remaining seam, including `guarded`, `core`
and `host` classifications. Those labels explain ownership; they do not remove
a path from consideration. Check whether host-owned work is already native in
cabinet play before treating a recording-only omission as a new port.

Use the whole-ROM, unregistered-entry and unclassified-candidate reports as
secondary diagnostics. They include native transaction costs or exclude
expensive classified paths and therefore cannot measure the remaining work.
Prefer completing a hot controller's branches and child handoffs over adding
low-cost leaves merely to increase the admitted-routine count. Compare actual
non-wait interpreter cycles before and after on the same routes, alongside
state, call and ordered-write fidelity checks.

For example, completing `02:5159`'s collision-marked release and full-charge
dispatch paths reduced higharc's deployed non-wait interpreter residue from
1,075,794 to 868,071 master cycles (19.3%), with unchanged assembly and registry
state hashes. All 1,629 directly observed controller calls and 20,975 ordered
call writes remained byte-identical to assembly. The routine count stayed 620.

The audio stack wrapper (`45:4B7B`, `4B82`) is now a two-entry port with a
real child-call boundary, not a composed guess at the child's stack. Its
31-route interpreted range fell from 204,714 to 79,734 cycles (124,980 removed,
61.1%). `out/coverage-audio-stack-corpus-v2` retains all prior assembly and
registry state baselines and abandonment counts. Total non-wait interpreter
residue is 17,925,354 cycles versus 18,053,088 in
`out/coverage-eyeball-animation-corpus-v1`; the 127,734-cycle total improvement
also includes 2,754 cycles from the intervening Eyeball expiry arm. Do not
attribute that entire delta to the audio wrapper. The registry is 693 entries.

An initial generic PLP checkpoint passed the short isolated replay but failed
the full-registry gameplay baseline at frame 1064; that failed evidence remains
in `out/coverage-audio-stack-corpus-v1`. The corrected version stops at PLP,
which samples the old interrupt mask before restoring the saved one. PLP and
RTS at these addresses are still interpreted, not deployed native-core
capabilities. Release and Debug tests compare 1,800 frames of pixels, audio,
RAM, video memory, ordered writes and child/return CPU boundaries, then a
separate gameplay route's full census, enclosing calls and hardware writes.
16,384 supported stack/status cases cover wrap and SEI sampling. Ten focused
Release checks pass, including the three sorcerer crash regressions and bank
wrapper/music-exit tests. Both executables were rebuilt. This is not a claim
of whole-registry assembly parity: the corpus's 31 pre-existing A/B differences
remain unchanged, and the known strict-stack regression was not rerun here.

The next controller slice, `08:56D3-577E`, moves the three scene-selection
pages out of the interpreter. Its 24 admitted entries are the head and real
child-return continuations of one controller, not independent leaf ports.
Each native segment pushes the original child-call frame and stops at the
callee; frame waits, actor passes and asset uploads retain their existing
owners. The next continuation reads fresh RAM after the child returns.

Across the same 31 routes, this removed 1,679,208 non-wait interpreter cycles:
26,992,269 became 25,313,061 (6.2% less residue). The 70 previously interpreted
controller addresses disappeared from that corpus's residue, with every
assembly and registry baseline unchanged. On the unpatched admission route,
all 4,270,592 ordered RAM writes and 5,330 child-entry register/timing snapshots
match assembly, as do frame-by-frame RAM, video and audio. Separate controlled
menu-choice tests cover the optional child, early exit and repeated third-page
visits. Evidence is under `out/coverage-scene-selection-corpus-v1` and
`out/scene-selection-controller-v1`; the checked-in scene-selection regressions
retain the assembly goldens.

The following slice extends that controller through the adjacent scene
bootstrap and attract countdown, `08:55A6-56D0`. Its 23 new admitted entries
are genuine child-return boundaries, covering setup, the start-input loop,
actor creation, the 16-bit countdown and timeout/restart. Bank-09 setup
children are resolved through the live MPRs; the actor slot returned by
`08:5977` is read fresh, not captured from a particular run. The initial VDC
stores preserve the bus's stolen-cycle interrupt sampling. No child body,
frame wait or bulk upload is made part of an oversized native transaction.

Across the same 31 accepted routes, this removes another 1,303,116 non-wait
interpreter cycles: 25,313,061 becomes 24,009,945 (5.1% less residue), with
120 previously interpreted addresses removed. Every assembly/registry
baseline and every abandonment count remains unchanged. The natural
assembly comparison includes all 4,270,592 RAM writes, 1,019,098 VDC writes,
6,288 child-entry snapshots, and frame RAM/video/audio. A separate controlled
timeout verifies restart and the full countdown, including low-byte borrow,
against 1,318,305 assembly RAM writes. Both new regressions and the core tests
pass in Debug and Release; the focused 12-test Release set also includes the
existing selection and sorcerer gates. The final Release executable is rebuilt.
Evidence: `out/scene-bootstrap-controller-v1` and
`out/coverage-scene-bootstrap-corpus-v1`.

The next slice removes the `00:E362-E39C` graphics-transfer cluster from the
high-impact queue. The address-fill loop at `E34C` now owns its closing BNE;
the uncompressed word uploader at `E365` resumes at its existing `E37E` loop
head in bounded 32-word segments. Each original instruction keeps its own
checkpoint and each VDC store keeps its real bus timing. Source reads resolve
through live MPRs, including ROM mirrors and RAM, while unsupported device
reads decline without committing a partial transcript. The final RTSs stay
with the core. No bridge capacity or interrupt/stack machinery was changed.

Admission grows from 667 to 670: two word-transfer entries plus the promotion
of the already-native `E34C` auxiliary capability into an admitted row. Its
old auxiliary installation must not remain alongside the registry hook;
the cabinet smoke test verifies the resulting deployment. The legacy
`VDC-address` capability counter is consequently zero, with the work now
credited to the routine row instead of lost from native coverage.

Across all 31 accepted routes, actual non-wait interpreter residue drops
from 24,009,945 to 22,863,639 master cycles: 1,146,306 removed (4.8%),
and 25 interpreted addresses eliminated. Every route improves by 36,222 to
47,994 cycles. All accepted assembly/registry state hashes and all registry
abandonment counts remain unchanged. Evidence is in
`out/coverage-vdc-word-transfer-corpus-v2`; the earlier `-v1` directory retains
the duplicate-hook installation failure caught before the completed sweep.

`dc_rom_vdc_word_transfer` locks the isolated family to the original
assembly's full 4,270,592 RAM writes, 1,019,098 VDC bus writes, frame states,
and the uploader's entry/exit register and cycle tuples. It proves all 2,048
address-fill iterations and both halves of the natural 64-word transfer.
The ROM-free `dc_vdc_word_transfer_tests` additionally checks 672 transfer
cases, seven address-fill cases and rejection guards: long multi-segment
transfers, 16-bit borrowing (including an initial zero count), Y/source-page
wrap, odd continuation indices, live RAM/ROM bank crossings, preserved flags,
exact write/sample offsets and fixed channel bounds. Both tests pass in
Release and Debug. The focused Release set also retains the existing VRAM,
scene-bootstrap/selection, sorcerer and cabinet deployment checks.

Both executables are rebuilt. The final RAM-mapping guard tightening is
rechecked on `gameplay`, `crossfire` and `higharc` in
`out/coverage-vdc-word-transfer-final-v1`, as well as the owning-family tests.
Assembly/native artifacts and the strict registry comparison are retained in
`out/vdc-word-transfer-v1`. The strict comparison still has the same
pre-existing census difference and complete registry stack-write hash as the
preceding slice; its failure was not weakened or blessed as assembly parity.

The following slice completes the selection actor controller adjacent to
`08:5A2E`: the recurring `5A3E` input callback and old/new glyph entries
`5A6A`, `5A7A`, and `5A86`. Four real transaction entries, not four isolated
leaves, cover both input directions, signed decrement wrap, increment wrap,
glyph-width selection, and the carry-sensitive tile-address calculation.
Native segments stop at original child calls or the tail JMP into `5B01`.
The existing initializer, tile writer, coroutine scheduler and core RTS keep
their ownership. Decimal/T flags and unsupported RAM/code mapping decline
before any state changes. Admission grows from 670 to 674.

`dc_rom_selection_actor_controller` supplies its own real joypad route, so
active selection no longer depends on a user's manual playthrough. It runs
800 frames each in assembly and native configurations, for normal and wide
glyph layouts. All 499 callback entries, 45 old-glyph entries, 47 new-glyph
entries and 45 address-builder entries are native in each case. Both wrap
directions are mandatory coverage. Each frame compares SHA-256 evidence of
ordered timestamped RAM/VDC writes, child register/MPR/cycle snapshots,
complete RAM/video memory, pixels and audio; frozen assembly aggregate
hashes prevent both sides drifting together. The wide case changes layout
only at a frame seam and exercises row/column carries. No ROM bytes or
input-memory bytes are patched. An additional 43,008 input/choice/limit/slot
combinations, all 256 stack positions, and D/T/mapping/short-RAM guards test
the production transcript directly.

All 15 focused Release regressions pass, including the existing scene,
initializer, VDC-transfer, sorcerer and cabinet deployment checks. The new
controller regression and core tests also pass in Debug. Both executables
are rebuilt with all 674 admitted routines deployed.

Across all 31 accepted routes, non-wait interpreter residue falls from
22,863,639 to 21,380,289 master cycles: 1,483,350 removed (6.5%). Each route
loses 47,850 interpreted cycles. All accepted assembly/registry state hashes
and abandonment counts remain unchanged. The complete evidence and refreshed
ranking are in `out/coverage-selection-actor-corpus-v1`; preserving these
per-configuration baselines does not imply full-registry assembly parity.

The natural 1,800-frame CLI recording also retains its original state and
all 4,270,592 RAM writes and 1,019,098 VDC writes. Evidence and the rerun of
the unchanged pre-existing strict registry parity failure are under
`out/selection-actor-controller-v1`. No crash fix or regression gate was
reverted. That ranking selected the `00:ED4F-ED85` banked controller for the
next slice; the leading `01:C500-C50E` profile row is cabinet-owned,
not new porting work.

The audio-command bank wrapper now has native entry and return segments at
`00:ED58` and `00:ED75`, bringing the admitted registry to 676. The first saves
X and all three MPR shadows, maps three consecutive banks, and makes the real
`JSR E262`; the second restores the caller's live stack frame and stops at the
core-owned `ED85` RTS. The driver body and interrupt/return machinery keep
their existing owners. This resolves the mapper/partial-stack deferral at the
actual wrapper boundaries rather than guessing a frame at the interior `ED5E`.

`dc_rom_bank_call_wrappers_semantic` now includes all 72 natural calls at each
new entry. Its original 3,600-frame assembly census and 6,389,776 ordered
stack/mapper/vector writes are unchanged. `dc_rom_audio_bank_wrapper` adds
65,536 stack-pointer/bank combinations and an 800-frame real-joypad A/B replay,
both isolated and composed with the existing bank wrappers. Every frame's
pixels, audio, RAM, video memory, ordered RAM/VDC writes and restored MPR/
register boundary match assembly in Release and Debug. No assembly golden
was replaced to admit these entries.

The complete 31-route sweep in `out/coverage-audio-bank-wrapper-corpus-v1`
reduces actual non-wait interpreter residue from 21,380,289 to 20,794,209
master cycles: 586,080 removed (2.7%), with a reduction on every route and
27 fewer interpreted addresses. All accepted state hashes and abandonment
counts remain unchanged. All 17 focused Release tests pass; the new wrapper
regression and core tests also pass in Debug. Both executables are rebuilt.
The next coherent
non-host controller selected from that refreshed queue was the VDC row-stream uploader
`00:EF2A-EF6E` (463,977 cycles); its existing row back-edge is a natural bounded
transaction boundary. The larger `CBF9`, `CB68` and `FA67` single-instruction
rows still require their precise core event/return contracts, not an ordinary
bulk transcript.

The strict full-registry parity rerun in `out/audio-bank-wrapper-v1` retains
the pre-existing census mismatch and the same registry stack-write SHA-256
`6130a0d537c2e73a1bcff723338339ed1cc08bffb045e713a6ac69f5edd55fb1`.
Passing isolated wrapper parity must not be reported as full-registry parity.

The VDC row-stream uploader `00:EF2A-EF6C` is now native, bringing admission
to 677 routines. A segment owns one complete 32-word row including both
pointer advances and the X-count back-edge; `EF6E` RTS remains in the core.
This resolves the old nested-arrival/enclosing-return objection without
increasing the bridge's write or checkpoint capacity. Both pointers advance
by `$40`, but one is a byte source and the other a word-addressed destination.
The model resolves the source through live MPRs and staged RAM, preserves
CLA/CLY flags, and declines device reads or unsupported D/T/mapping states.

`dc_vdc_word_transfer_tests` now includes 8,448 row boundary combinations,
all X values and source low bytes, RAM aliases, ROM/MPR crossings, destination
carry/wrap, exact write offsets and interrupt samples, plus a complete
zero-count/256-row stream and negative guards. `dc_rom_vdc_row_stream`
compares a fresh assembly/native 1,800-frame replay and fixed assembly goldens:
all four row calls, five row/return observations, 4,270,592 RAM writes and
1,019,098 VDC writes. The replay evidence is in `out/vdc-row-stream-v1`.

The complete 31-route sweep in `out/coverage-vdc-row-stream-corpus-v1`
reduces actual non-wait interpreter residue from 20,794,209 to 20,330,883
master cycles: 463,326 removed (2.2%), exactly 14,946 per route and 35 fewer
interpreted addresses. All accepted assembly and registry state baselines
and all abandonment counts remain unchanged. All 19 focused Release tests
pass; the row boundary tests, assembly/native replay and core tests also pass
in Debug. Both executables have been rebuilt with the 677-entry registry.
The separate strict full-registry parity rerun still has the pre-existing
census mismatch and unchanged stack-write hash `6130a0d5...dd55fb1`; no parity
assertion or accepted baseline was relaxed.

After the row-stream slice, the non-host controller queue started with the
music command region `45:4569-45F2` (421,638 cycles), followed by the guarded nudge-input
region `01:C3CB-C3E0` (388,470). The former already has an inlined command
model: investigate why those deployed arrivals remain interpreted and prove
its real dispatch/return boundaries before adding ownership. The larger
single-instruction return/compound-branch seams remain separate core-contract
work, and the leading `01:C500-C50E` row is already cabinet-native host work.

The music command investigation found a real whole-parent refusal at
`45:4356`: the `$82/$92` sequence-end command clears the voice, discards two
stack frames with four PLAs, and jumps to the tick continuation `40EE`.
Treating that as an unsupported leaf return lost the entire command/tick
prefix. The model now executes the exact pops and jump, ends the transcript
at `40EE`, and lets the existing owner resume with live state. The `$86`
publishing variant at `4350` shares the exit. No return is fabricated or
absorbed, no bridge capacity changes, and no new registry entry is necessary.

The full music regression's original census, RAM-write, PSG/timer-write,
call and call-write goldens remain unchanged; it now accepts all 3,599 tick
entries. Three child entries disappear from that composed test because their
callers no longer decline. `dc_rom_music_command_exit` preserves their
independent admission: 2,276/2,299 command calls, 1,142/1,142 portamento
calls and 22,647/22,664 voice-commit calls. Its fresh assembly/native replay
compares 32,391 `40EE` register/timing boundaries, all 12,647,397 RAM writes,
699,251 PSG/timer writes and the complete frame census to fixed assembly
goldens. Unsupported commands and existing interrupt guards still decline.
The 196,608 direct command/stack/voice cases include each stack pointer and
X value, live caller bytes and wrapping pops, preserved status, exact samples
and store timing, and explicit absence of an absorbed-return declaration.
Evidence is retained under `out/music-command-v1`.

Across all 31 routes in `out/coverage-music-command-exit-corpus-v1`, actual
non-wait interpreter residue drops from 20,330,883 to 19,401,741 master
cycles: 929,142 removed (4.57%), with 81 fewer interpreted addresses. Every
route improves by 7,041 to 49,344 cycles; all accepted assembly/registry
state hashes and all abandonment counts are unchanged. Completing the exit
recovers both the command prefix and previously rejected tick work, rather
than merely saving the handler's own 75 cycles. Admission remains 677.
All 19 focused Release tests pass, including the prior upper-table/sorcerer
regressions. Debug also passes the full independent assembly/native replay,
all direct boundary cases and core tests. Both executables are rebuilt.
The separate strict registry parity rerun retains its known
census mismatch and identical complete stack-write hash `6130a0d5...dd55fb1`.
The next selected controller was the nudge-input gate `01:C3CB-C3E0`
(388,470 cycles). Its session-bit and tilt-lock returns are now native,
including the live caller return. Timer expiry also reaches the existing
active start/finish model, using the newly cleared counter rather than the
entry counter. No new registry entry was needed: admission remains 677.

The new direct tests cover 65,408 combinations of session flags and nonzero
locks, live/wrapping stacks and flags, plus 512 active timer-reload cases.
Unpatched nudge, twist and lanes replays compare full frame states, call
register/timing records, ordered call writes and VDC writes to assembly.
They accept all 2,654 / 2,654 / 2,653 controller calls respectively. A separate
1,800-frame controlled replay exercises a reload start, 200 reload finishes
and 400 tilt-lock calls, requiring native admission for each and comparing
every frame's RAM/video/audio and every ordered RAM/bus write with the core.

That rare-state replay exposed older active-path errors at interrupt
boundaries: CLI cleared its sampled I mask too early, and CLX incorrectly
set N/Z. Both are fixed; SEI's old-mask sample and VDC's stolen-cycle sample
are explicit too. Rejected active models now remain rejected rather than
falling through to the idle-return model.

The lanes baseline required a reviewed correction: only frame 1461's RAM
and frames 1461-1464/1469's audio change; pixels and all later state remain
unchanged. The new full-registry census is byte-identical to the same
registry with only C3CB left in assembly. `dc_rom_nudge_registry_parity`
also proves identical ordered stack and VDC/PSG writes between those two
modes, and pins the corrected census (`16886029...a0eaea7`). The original
assembly baseline is unchanged. This is not a relaxation of whole-registry
parity: the separate strict stack-page test still reports its older census
mismatch and unchanged stack-write hash `6130a0d5...dd55fb1`.

The final 31-route audit in `out/coverage-nudge-controller-audit-v3` reduces
actual non-wait interpreter residue from 19,401,741 to 19,013,487 master
cycles: 388,254 removed (2.0%). Every assembly recording and every registry
recording matches the reviewed manifest, with only the independently proven
lanes correction above replacing an old registry baseline. All abandonment
counts remain unchanged. The initial baseline-enforcing sweep stopped on
lanes; the final diagnostic sweep was separately checked against all 31 rows
after that correction, not accepted just because recording succeeded.

All 14 focused Release tests pass, including the prior sorcerer/upper-table
crash regressions and the mixed-registry lanes comparison. Debug passes the
direct cases, core tests and controlled assembly/native replay too. Both
executables are rebuilt. Natural and mixed-registry evidence is retained in
`out/nudge-controller-v1`; the rare-state fixture is reproducible through
`dc_rom_nudge_controller_rare_states` without another user capture.

The selective scene-record loader `02:40A8-40CE` is now ported at its initial
entry and actual `40BD` child-return continuation. It scans twelve-byte
records, skips non-category-1 records, advances the pointer with carry, and
stages the original JSR stack writes before handing off to `40F6` or `C08A`.
Neither child is inlined, and the final `40CE` RTS stays in the core. The
scan cap declines atomically without enlarging either bridge channel.

The baseline-enforcing 31-route sweep in
`out/coverage-selective-controller-corpus-v1` reduces actual non-wait
interpreter cycles from 19,013,487 to 18,710,133: 303,354 removed (1.6%),
with 19 addresses gone. All assembly and registry state hashes and all
abandonment counts remain unchanged; no baseline was adjusted. The 546
remaining cycles in this family are its final core-owned RTS. The separate
pre-existing whole-registry assembly differences are not resolved by this slice.

The admission regression independently compares alternate, twist and the
existing seeded drain fixture with assembly, including frame state,
enclosing call/write artifacts and all VDC writes. Each reaches the initial
entry once and all seven loader returns. Direct tests cover 196,608
selected/skipped/terminal cases over every pointer low byte and stack pointer,
plus full-table scanning, 16-bit pointer wrap, live source mapping, CLA flags,
D/T rejection and overlong-list/device-source fallback. Admission grows from
677 to 679. Both executables are rebuilt. All 13 focused Release checks pass,
including the existing scene, sorcerer and upper-table regressions. Debug
passes the core/direct tests and all three assembly/native replay comparisons
too; its trace-heavy replay takes approximately 14 minutes.

The bank-09 parent animation startup at `09:705C-70C0` now has seven native
entries spanning its descending, holding and ascending phases. Each real
motion/audio child or coroutine handoff ends the segment. The existing
`70C1` scheduler owner remains unchanged, and the ordinary returns at `7071`,
`7099` and `70BB` now admit independent guarded live-stack returns. This parent-body step grew admission from 679 to 686.

`dc_rom_upper_parent_animation` compares two unpatched routes with assembly:
all 92 enclosing calls, their ordered writes, complete frame state and every
`0000-1FFF` device write. `dc_rom_upper_parent_animation_terminal` forces the
last ascending counter transition in the live actor to reach the otherwise
unvisited audio exit and its existing scheduler owner. It changes two RAM
bytes, not ROM code; both executions produce identical artifacts. The direct
test covers 197,632 counter/comparison/stack cases, including full flag checks,
wrapped PHX/JSR/PLX stacks, live post-child X restoration and D/T guards.

The baseline-enforcing sweep in `out/coverage-upper-parent-corpus-v2` removes
219,201 actual interpreted cycles (1.17%): 18,710,133 becomes 18,490,932, and
37 instruction addresses disappear from the residue. The family's remaining
58,590 cycles are its three core-owned RTS instructions. All 31 assembly and
registry baselines and all abandonment counts are unchanged. The pre-existing
whole-registry assembly differences remain; this is not a claim to have fixed
those separate differences. No baseline or prior crash fix was changed.

Both executables are rebuilt. All 13 focused Release checks pass, including
the score-panel, sorcerer and upper-table tile-scheduler regressions. Debug
passes core/exhaustive tests and both natural-route assembly comparisons
(approximately eight minutes of traced replay). The controlled terminal
fixture was verified in Release.

The sibling at `09:7194-71E6` is now also ported through five separately
replay-proven boundaries, bringing admission to 691 routines. Its descending
and ascending semantics share explicit address relocation with the first
parent; its shorter hold phase has a distinct scheduler handoff. The three
RTS instructions remain core-owned, as does the unchanged final `71E7` call.
Natural gameplay/alternate replays and a controlled final-counter transition
compare complete census, enclosing calls/writes and device writes. The combined
direct controller test covers 329,472 cases. See
`tests/verify_sibling_parent_animation.cmake` for executable evidence.

The baseline-enforcing `out/coverage-sibling-parent-corpus-v1` sweep passes all
31 routes with no changed assembly/registry state baselines or abandonment
counts relative to `coverage-upper-parent-corpus-v2`. Actual non-wait interpreter
residue falls from 18,490,932 to 18,314,790 cycles: 176,142 removed (0.95%),
with 29 executed addresses removed. The sibling's remaining 58,590 cycles are
the three retained RTS instructions. The pre-existing 31 whole-registry
assembly differences are unchanged, not newly blessed or resolved.

Fourteen focused Release checks pass: the core/direct tests, both parents'
natural replay tests, the original parent's terminal fixture, score-panel
poll, host registry, three sorcerer regressions, upper-table actor scheduling,
and registry/seam/baseline manifests. Both executables were rebuilt. This is
focused regression coverage, not a new full-suite or strict-stack parity claim.
Debug core/direct checks and all three sibling replay routes also pass; the
trace-heavy Debug replay took 594.58 seconds. The final Release and Debug
executables both contain the 691-entry registry.

The next animation slice extends the existing `02:4F3D` admission through
`4F69`: timer expiry/reload, frame increment/wrap, ROM frame selection, dirty
marking and phase borrow. The prior wait fast path remains first; the new
instruction-checkpointed fallback stops at the existing `4F6C` continuation.
The `4F3A` child call and downstream return/scheduler owners are unchanged.
Admission remains 691 routines, with 224/224 calls on the established patched
Eyeball route instead of 122/224. Direct tests cover 131,328 combinations;
the new assembly/native replay compares complete state and ordered RAM/device
writes. The old regression's census, trace, call and write hashes are unchanged.

`out/coverage-eyeball-animation-corpus-v1` passes all 31 baseline-enforced
routes: actual non-wait interpreter residue falls from 18,314,790 to
18,053,088 cycles (261,702 removed, 1.43%), with 18 executed addresses removed.
Assembly and registry state hashes and abandonment counts are unchanged from
the sibling-parent corpus. The 31 pre-existing whole-registry differences
remain unchanged; this is not a whole-registry parity claim.
Twelve focused Release checks pass (core/direct, both Eyeball replays, host
registry, three sorcerer regressions, upper-table scheduling, and three
manifest/baseline checks). Debug core/direct and the full Eyeball A/B replay
also pass; the latter took 334.81 seconds. Both executables are rebuilt with
the refreshed 691-entry proof metadata. No crash fixes or state baselines
were reverted. The related `02:4F6C-4F8D` terminal continuation still accounts
for 175,884 interpreted cycles and is a candidate for the next bounded slice.

The countdown continuation now handles expiry through `4F81`, stopping at
`4F83` before the child call. Mapping and D/T guards are explicit. Direct
Release/Debug tests cover 196,864 animation/countdown cases. The updated
Release Eyeball A/B replay requires full countdown admission and an observed
expiry; census, enclosing calls, ordered RAM writes and device writes match.
Sorcerer tick/color/live-controller regressions, host registry, registry and
seam checks pass. Both executables are rebuilt. No new Debug ROM replay or
full 31-route sweep was run for this follow-up.

`out/coverage-eyeball-expiry-v1` checks gameplay, alternate and twist against
the committed baselines, with unchanged state hashes and abandonment counts.
Only 210 interpreted cycles were removed across these three routes: this
closes the rare expiry arm, not the bulk of the remaining continuation cost.
The child calls and post-call scheduling from `4F83` still need separate
ownership/resumption work; the earlier 175,884-cycle range must not be
reported as eliminated.

The full Release audit also exposed two stale test-wiring/count expectations,
corrected without changing their behavioral goldens, and one unresolved
**pre-existing** strict registry/assembly parity failure:
`dc_rom_registry_stack_page_writes`. A diagnostic replay excluding all 24 new
scene-controller entries has exactly the same census and complete stack trace
as the expanded registry. The first assembly/registry stack difference is write
23 at frame 1, `45:4B75` (cycle 6,468 versus 6,450), before scene selection.
The diagnostic artifacts are `prior-registry.census.csv` and
`prior-registry.stack.csv` under `out/scene-selection-controller-v1`.
Do not relax that parity test or describe the full suite as green; preservation
of the accepted per-configuration corpus is a distinct, narrower guarantee.
The bootstrap slice reran that strict comparison: both the registry census
and complete stack-write hash still match the prior registry exactly. Its
artifacts are `strict-registry.*` under `out/scene-bootstrap-controller-v1`.

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
family, or state machine. Run it in parallel, but use the latest completed
CTest timing when estimating the wait: the growing regression suite and its
trace-heavy tests no longer fit the old three-minute estimate.

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
  Mark its reason in the test and in the slice's commit message.
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
ranks them by cycles, and marks the ones the registry already enters. The
entry-only view is accounting, not proof that every listed instruction still
runs in the interpreter: `tools/native_execution_seams.csv` also classifies
core opcode boundaries, cabinet handoffs, and guarded-native fallbacks that
are deliberately not registry rows. Use `-CandidateOnly` to exclude all three
classes when selecting a new family; maintain the CSV (and its static test)
when a new explicit hook or guarded family is introduced. Exclude
the frame-sync spin loop — `00:E323`/`00:E325` is 45% of all master cycles and
is the game waiting on the VBlank counter, not work to port.

### Persistent table-route coverage sweep

Use `Invoke-NativeCoverageSweep.ps1` when choosing the next family rather
than relying on a single recent play session. It runs all 31 deterministic
table input routes by default, records each route both assembly-only and with
the admitted registry. It preserves both profiles and logs, writes their
state-hash manifest (including an explicit A/B comparison), and merges the
registry-enabled profiles into both a ranked entry-only residue report and a
second ranking of unclassified port candidates.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Invoke-NativeCoverageSweep.ps1 `
  -App .\out\build\windows-vs2022\Release\devils-crush.exe `
  -Rom ".\Devil's Crush.pce" `
  -OutputDirectory .\out\coverage\table-routes-001
```

The output directory must not already contain artifacts. That keeps a failing
run's assembly control, registry result, and logs available for diagnosis
instead of letting a later run silently overwrite its evidence. For a quick
family check, pass one or more names such as `-Route higharc,playfield`; omit
`-Route` for the complete corpus. `Analyze-InstructionProfile.ps1` also now
accepts several `-ProfilePath` values directly when a custom route set needs a
merged ranking. After accepting a sweep, use its `manifest.csv` as the next
run's reviewed baseline; the checked-in
`tests/native_coverage_table_routes.csv` is used by default, so either the
assembly or registry state hash changing is a hard failure. Pass
`-SkipBaselineCheck` only while deliberately generating a reviewed replacement
baseline, or `-BaselineManifest` to check a candidate manifest.
`-RequireAssemblyRegistryMatch` is available for a fully assembly-faithful
configuration, but is intentionally not assumed by the current broad registry
sweep: its A/B differences are preserved as diagnostic evidence rather than
silently treated as a new baseline.

The entry-only report keeps the full cost accounting, including core and host
seams. The companion `registry-candidate-residue.md` removes those classified
boundaries and guarded families, so a large fallback inside an existing
Devil-Shooter or music replacement is investigated by its guard state instead
of being misidentified as a wholly unported routine.

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

### Read the registry's own admission rates before ranking anything

The instrument below ranks what is still emulated, and the reflex on reading it
is to look for a routine nobody has ported. Check first whether the routine is
already there and switched off. `--native-cycle-census` prints `native_calls`
against `entries` for every admitted address, and a row that reads `837 of
7,339` is a finished, proven transcript builder being refused five times in
six — the work is written, the evidence exists, and the cycles are on the floor.

Bank `45` was 4,430,103 emulated master cycles ranked across three clusters
that all looked like unported code, and not one instruction of it needed
writing: three defects in the walk's own guards accounted for the lot, and
fixing them was worth `+3,758,556` against a largest-remaining-routine of
roughly 1.3M. **Sort the census by `native_calls / entries` ascending and read
the top of that list before reading the residue at all.**

Two things stop that from being a to-do list on its own. A low rate is only
worth chasing if the row also carries cycles: the `knight_terminal_release_-
helper_*_prefix` family sits at the very top at `0 of 4,387` and is not a
defect at all — those are single-ordinal slices proven on a patched route,
whose preconditions the natural route never meets, and they report
`native_master_cycles` of zero to say so. And a partial row is sometimes a
deliberate hold-back. Rank by `entries - native_calls` weighted by the routine's
own cycle cost, then count the declines to tell a hold-back from a bug; the two
are indistinguishable from the outside.

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
had answered wrong from the profile.

**Do not check the target against the registry, and do not skip a crossing
because nothing owns the far side.** This file said the opposite for two
slices, and the collision-tail slice measured it: crossing `01:CB7B` into
`01:CF33`, which at the time was not registered and was still interpreted
instruction, is worth 6,678 cycles over 318 calls — exactly 21 each, exactly
what every other crossing pays. The reasoning behind the old rule confused two
different things. A crossing converts the `JSR` *itself*, and that conversion
happens whatever is on the far side; what a registered callee adds is the
callee's own body, which was never this edit's to claim. The mistake was
expensive in the other direction too: it is why the sorcerer tick's two calls
to `$C018` sat in the "unresolved target" column for a slice instead of being
crossed. There is no such thing as a worthless crossing.

**But do not price a survivor from its profile row either.** The sweep names
the addresses correctly; the row does not tell you how many of those executions
are walk-stops. `02:58DA` is in `is_stop_instruction` and profiled 598
executions worth 12,558, and crossing it recovered none of them: every one of
those 598 was interpreted *residue*, the core running on through the routine
after an interrupt abandoned a transcript upstream, never reaching the stop at
all. The same region shows it plainly — `02:58C7` through `02:58E3` all carry
interpreted counts inside a routine whose entry installs 18,152 times. Residue
is a different slice with a different fix, which is a new entry point, not a
crossing. Take the sweep's address list, cross the lot, and let the census say
what it was worth.

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

**`patches/0021-huc6280-abandonment-reason.patch` finishes the report, and
`--abandonment-census` is where to read it.** `0020` said *that* a transcript
was given up; `0021` says where the emulator picked the routine up afterwards
and why the core gave up, and the host names the transcript by encoding the
routine key in the token it mints. Over the census route that is 904 sites
summing to exactly the 21,657 the core counts. Two results from it are worth
more than the instrument.

**Every abandonment on this route is the machine disagreeing, never the
transcript.** Not one is a mispriced advance and not one is an exhausted
transcript — the two other endings are reported and never seen. And every
disagreement is `P`, and every one of those is the **B flag**, which is not a
register bit on this processor: `PHP` pushes `P | B_FLAG` and `PLP` pops the
byte back whole, so any such pair sets it in the live register, and any
interrupt clears it again because that push forces it off. Nothing reads it.
So before spending a slice on where abandonments *land*, check what they are —
21,657 of them turned out to be one bit that no execution can depend on.
`patches/0022-huc6280-break-flag-resume.patch` stops comparing it and the route
abandons nothing at all.

**Two things that came out of landing it are worth more than the patch.** The
first is that **an abandonment inflates the reported native share, and nobody
had noticed for as long as abandonments were normal.** The host credits a
native call when it *offers* one, so an abandoned transcript was credited its
whole body while the emulator delivered part of it — and the machine, going on
to re-interpret the rest of that span, arrived at registered addresses *inside*
it and each of those was credited its own body on top. Removing the
abandonments took the reported figure *down*, from 53.28% to 52.90%, while the
emulator interpreted 21,644 fewer instructions. A number that falls when the
port improves is worth stopping over: it means the number was measuring
something else.

**The second is that a transcript cannot model the B flag, so nothing should
record it.** A transcript models `PHP` and `PLP` exactly and therefore carries
B forward; it cannot know when an interrupt will clear it. While abandonment
was normal that never showed, because the transcripts that would have disagreed
were thrown away first. With them completing, the declared register reached the
call traces and the `absorbed_return_trace` family caught it immediately —
those compare an emulated route against a replaced one *inside the same run*
rather than against a stored hash, which is why they are the regressions worth
having. `observable_status` in the runtime drops the bit on the way out of the
machine and on the way into the core.

**Re-baselining on that scale needs a confinement argument, not a diff count.**
220 stored expectations moved. What makes that safe is not that the suite goes
green afterwards — it would go green around almost any mistake — but that
regenerating one artifact either side of the change gives exactly one differing
column across 10,605 rows, `status`, and exactly one differing value, `10` to
`00`. Establish that on a representative artifact of each kind before rewriting
anything, and treat a diff that touches a second column as a fault rather than
a baseline.

**And do not read a residue address as belonging to the routine it sits in.**
The bank `03` interpreter's 2,433,327 emulated cycles were recorded here as
`03:6966`'s to recover. `03:6966` abandons 1,721 times; `02:58C2` abandons
5,034 and `03:7F57` 4,515, both of them *inside* `03:6966`'s body, because both
compose it. An interior address is reached by whoever was running when the
transcript covering it was given up, and the census is the only thing that
says who that was.

**The correction to that last one is worth more than the caution was.** After
the B flag stopped being compared, the same range fell from 2,433,327 emulated
cycles to 21,180 — and nobody ported anything in that bank. Residue *was*
abandonment. Three rows recorded elsewhere as ordinary unported code went with
it: `03:603E` at 137,607 was filed as an unported parent and is 84; `03:7F80`
at 94,878 was filed as an untranscribed arm and is 63; `02:58DA` at 12,558 had
been crossed, returned nothing, and no longer executes at all. So when a
profile row is expensive and no walk explains it, suspect where a transcript
resumed before inventing a parent for it.

**The instruction profile over-reports every row by about one per cent, and it
is not noise.** `01:DC95`'s residue profiled at 1,002,069 and the registry
gained 993,525 when it was ported. The gap appears on every row because the
profile measures elapsed master cycles between instruction boundaries, so an
interrupt taken at a boundary is charged to the instruction in front of it. At
the extreme this is the whole row: `00:E321` is `LDA <$4B`, twelve master
cycles, 3,568 executions — 42,816 cycles of instruction against a row of
1,180,680, because it is the boundary the VBlank interrupt is taken on. **Rank
with the profile; cost with the static instruction model.** A row that prices
much higher than its opcodes can account for is telling you about a boundary,
not about work to port.

**A routine reached by `JMP` is still an entry, and its absorbed return is the
interesting case.** `01:DC95` has one `JSR` site and seven `JMP` sites, and
2,340 of its 2,895 arrivals come through the `JMP` at `01:C770`. On those the
frame its `RTS` reads was pushed by whoever called its *caller*: the routine
never had a frame of its own. The pointer arithmetic is unchanged — still
$2100+S+1 and +2 — which is exactly what makes it worth a regression rather
than an assumption, because a tail call leaves the stack looking ordinary and
reading it wrong returns to a live address instead of faulting. It is the ninth
shape in `verify_absorbed_return_trace`.

**The static instruction model prices a candidate exactly, and it is cheap
enough to do before writing anything.** Multiply each address's opcode cost by
its execution count from the profile and add them up. For `01:C721`-`01:C770`
that came to 1,122,531 master cycles, and the registry gained 1,122,531 — to the
cycle, where the profile's own total for the same range was 10,752 higher for
the reason above. Doing this first also tells you what the arms cost
individually, which is how you decide whether an arm the format cannot express
is worth handing back.

**The admitted-registry generator reads the dispatch chain, so write the keys
out literally.** `Generate-NativeRegistry.ps1` matches
`entry.routine == RoutineKey{ .physical_bank = 0xNN, .logical_pc = 0xNNNN }` in
`native_replacements.cpp`, or a `constexpr RoutineKey kRoutine{…}` guard in a
family source, and nothing else. A `switch` or a helper predicate registers the
routine correctly at runtime and is then rejected as unreachable by `-Check`.
Match the pattern rather than teaching the tool a second one: that check is what
stops a routine being admitted to the registry that the dispatcher would never
hand a replacement for.

**A crossing is not a technique that gets exhausted.** The sweep for walks
ending in front of a call returned nothing for several slices, and that was
recorded as the technique being finished. It was finished only for the code that
had been ported. The substep loop added eight crossings the day it was written,
because a crossing is simply what a walk does when it meets a call — so the
supply of them is the supply of unported code, and a `JSR` row in the profile is
a port waiting to happen rather than an edit waiting to be made.

**`docs/NATIVE_REGISTRY.md` says it is generated and it is not, any more.**
Running `Generate-NativeRegistry.ps1 -ReportPath docs/NATIVE_REGISTRY.md`
rewrites the whole report from the prose stored in the script, and that prose
has fallen behind the checked-in file — on this slice the regeneration was a net
loss of about 340 lines, including several paragraphs of slice history that
exist only in the document. Edit the document by hand and mirror the same change
into the script's `$lines.Add(...)` block, which is what keeps the two from
drifting further. The per-routine table rows *are* derived, so take those from a
report generated to a scratch path and paste them in. Only `-Check` and the
default table write are safe to run at the repository.

**A call into a routine the same family owns is still a crossing, and it is
worth more than one into somebody else's.** `00:F08E`'s four `JSR`s go to
`00:F0C0`, `00:F104`, `00:F148` and `00:F17E`, all inside the same slice. Making
the call rather than stopping in front of it converts the `JSR` itself, and
because the target is an entry the same walk declares, the body on the far side
is converted in the same slice. Crossing plus absorbing the returns is what
lets a family of five routines leave nothing in its range interpreted.

**When every arrival is admitted, that is the shared-state argument, not a
separate one.** The deadline this port has to meet is that a transcript's reads
all happen before the next serviceable interrupt, and the adapter enforces it by
declining a body that reaches the horizon. So `entries: N of N` with zero
declined is a measurement that no walk in the family ever spanned an interrupt —
which is a stronger statement than an enumeration of what the handlers write,
and much cheaper to obtain. Enumerate handler writes when the count is short of
N; when it is N of N, say so and move on.

**The cheapest hand-off is a fall-through into a registered address.** A walk
that meets a call can cross it, which converts the `JSR` and buys 21 cycles for
the price of pushing a return frame. A walk whose last instruction simply falls
into a routine the registry already owns pays nothing at all: the transcript
stops, the machine is handed a program counter, and whatever owns that address
picks the body up — no pushed frame, no converted call, no jump to transcribe.
`01:C4DE`-`01:C50E` ends on `STX <$4D` and the next address is the registered
actor scan at `01:C510`. **So when ranking candidates, look at what runs
immediately before an address already in the registry**; adjacency to a ported
routine is worth as much as a call into one and costs less to claim.

**Hand an arm back because its target is outside the transcript, not because the
branch has never gone that way.** Both are arms this route has never taken, and
they are not the same. `01:C4ED` reaches instructions nothing else in the walk
covers, so declining it leaves the core to be right about them by construction.
The two branches to `01:C4FC` reach an address the walk already transcribes —
the fall-through from `01:C4FA` arrives there 322 times — so following one lands
on ground already proven, and the only untranscribed thing about the arm is
which way the flag went. Declining that second kind buys no safety and loses the
tail of the walk.

**A port can swallow an instruction hook, and nothing will say so.** Porting
`01:C4DE`-`01:C50E` silently broke the cabinet's shooter input gate, which hung
an observe hook on `01:C500` to put the flipper bits back into `$2CEE` between
the preamble's `STY $2CEE` and the four addresses that read it. A routine that
runs as a native transcript never presents its interior instructions to the
interpreter — the walk leaves `01:C4DE` and hands back a program counter of
`01:C510` — so the hook fired on every frame before the slice and on none after
it. Nothing failed: `install_execution_hook` still succeeded, because `01:C500`
is not a registry entry and so collided with nothing. It simply never ran again,
and the flippers went dead during the launch.

**An address is hookable only if the interpreter still arrives at it, which for
a ported range means its entry and whatever it hands back to, and nothing in
between.** Neither was available here — `01:C4DE` is before the write, and
`01:C510`, `01:C52C` and `01:D466`, every address between the write and the
reads, are registry entries themselves, and a key carries one hook. So the host
claims the preamble back through `install_native_registry`'s `claimed_routines`
and runs it interpreted: 775,224 master cycles, 0.06% of the route, for a
flipper that answers 100 frames sooner on every launch.

Two things follow for the next slice. **Before porting a range, grep the host
for hooks inside it**, because the collision check only covers entries. And
**a probe that measures a host behaviour has to deploy the registry**, or it
measures a machine nobody plays: `--cabinet-launch-flipper-probe` kept
reporting the gate working for as long as it ran without one.

Two things about `0020` are worth carrying. The half-fix tried first — withdrawing
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

**`--observe-hook` cannot be pointed at any of those addresses except the head.**
An observed call is opened at the hook and closed by its return, and an entry
that is a return address never returns — control leaves it through the next call
instead. The capture never closes, and it grows without bound: three observe
hooks on the main session loop's entries, with *no* replacement installed at
all, reached 79 GB of resident memory halfway through the census route and then
failed the recording. Nothing was wrong with the port; the shape of the request
was wrong. Use `--work-ram-write-range` with `--work-ram-write-trace` for these
families. It is the better oracle anyway, since it says what was written and
when rather than what the trace believed a call was.

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

**When composition will not fit, a crossing still collects half the tax.** A
crossing is composition's first step without its second: push the return
address, charge the 21 cycles, and end the transcript on the callee's first
instruction instead of walking into it. That converts the `JSR` without needing
the callee to fit inside the parent's horizon or channels, which is the case
composition cannot serve. The main session loop is the extreme version — ten
crossings, and the callees are the frame's whole actor scan and motion pass, so
splicing any of them in was never on the table. It took the range from
1,259,148 emulated master cycles to 21.

The corollary is that **a crossing does not port what it crosses**, and that is
a feature rather than a limitation. `01:C154` calls the frame-sync wait, where
the machine is supposed to sit for 45% of the route. Crossing it makes the call
natively and hands the interpreter `00:E321` to spin in, which is exactly right:
the 21 cycles were work, and the spin never was.

The way out for the rest is **composition**: instead of stopping in front of the `JSR`,
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
joined `BBR` as an instruction the format could not express, and took the same
remedy: stop in front of it, register the address after it, and let the core
run the instruction itself. In the driver epilogue that cost 1,526,046 cycles
of a 26.7M slice — until the format was changed to say both. See *The format
could not express it, so the format was wrong* below.

The two exceptions are worth holding together, because they are found
differently. `BBR` is a *timing* defect — the transcript's own artifacts are
exact and the divergence is downstream. `SEI` is a *state* defect — the
divergence is right there in the transcript's own cycle stamps, and the stack
page names the instruction. If a walk's writes are correct in value and order
but uniformly late from some point onward, look for an interrupt the machine
did not take, and look at the mask first.

#### Fix every `SEI` in the span, and make the stop conditional

The note above was written from one instance and read as being about one
instance. Bank `45` had **six** — `4153`, `4168`, `47DF`, `482D`, `485D`,
`49E3` — every one of them transcribed with the deferred set, and the one
that was eventually caught was caught only because an unrelated change started
admitting the walks that reach it. The others had been latent for slices. So
when this rule bites once, **sweep the ported span for the opcode** rather than
patching the address in front of you: `$78` is one byte and the disassembler
prints it.

The cost is not what the driver epilogue's figure suggests, because the stop
can be conditional. The ambiguity only exists when the `SEI` has a window to
close: reached with `I` already set, the deferred byte and the true byte are
the same byte and the walk can transcribe it normally. Testing the walk's
current mask at the instruction turned six unconditional stops into stops that
fire on the arrivals that need them — and the arrivals that do not are the
majority, because `4142` brackets each channel in `PHP`/`SEI`/`PLP` and most
callers arrive masked. Keep the case in the switch for the masked path;
deleting it sends every masked arrival to `default` and refuses the transcript,
which is how this was first got wrong.

#### The format could not express it, so the format was wrong

Two slices in a row worked around the `SEI` rule, and then the residue ranking
put the cost of the workaround at the top of the table. `00:FC81` — one `SEI`
— was **1,509,498 emulated master cycles**, more than any other single
instruction outside the frame-sync wait and more than any unported *routine*.
Add `00:FD75` and bank `45`'s six and the rule was costing 1,597,272 cycles,
13% of everything the deployed registry still emulated.

That is the signal to stop working around a format and change it. The
ambiguity was never "one byte cannot say two things" — it was that the
checkpoint had one *field* where the hardware has two moments. `LastCycle`
sampling and the opcode's writeback are a cycle apart, and the checkpoint
already models that separation for time: `interrupt_sample_master_cycle_offset`
exists precisely because the sample happens before the boundary. It just did
not model it for the mask. `patches/0023-huc6280-interrupt-flag-sample.patch`
adds `samples_interrupt_flag_clear`, and the core applies the clear mask across
`DcSampleInterrupt` and the retired one immediately after.

Three properties made it a small change rather than a risky one, and they are
worth copying the next time a format looks like the obstacle:

- **Additive with the old behaviour as the default.** A checkpoint that leaves
  the field zero takes byte-for-byte the path it took before, so all 404
  admitted routines were unaffected by construction rather than by testing.
- **Refused rather than trusted at the boundary.** The host throws on a
  checkpoint that claims the clear sample without setting the mask, so the one
  way to state the new field wrongly cannot reach the core.
- **Proved where a wrong answer would land.** Both readings of an `SEI`
  boundary differ only in the byte an interrupt pushes, so the oracle is the
  stack page: `--work-ram-write-range 2100-21FF` over the whole route,
  4,205,705 writes, identical. The state census cannot see it — a pushed byte
  is popped again long before a frame boundary — which is the same reason the
  census missed the four-byte fault in the slice before this one.

One corroboration is worth knowing about because it is nearly free. The
boundary-decision count printed by the census — 66,291,003 — did not move.
That figure counts the questions the machine asked, not the work it did, so a
change to *how* an interrupt boundary is answered that leaves the count alone
has not moved any boundary. It caught nothing here, but it is the cheapest
available check that a change of this kind is confined to the answer.

What the fix does **not** do is make every `SEI` free. Removing the stop means
the walk carries on through the mask set, and a walk that carries on holds its
`interrupt_window_open` state across the boundary — which is genuinely still
open, since the interrupt can be taken there. In bank `45` that made the
staleness guard and the chunk bound conservative over a much longer span, and
`45:4142` dropped from 3,544 admitted arrivals of 3,604 to 3,216 while
`45:485E` stopped being entered at all, for a net gain of only 129,942 cycles.
The driver epilogue has no such interaction — its split at the `SEI` is needed
for an unrelated reason, so the transcript ends there anyway — which is why
this landed there and not in bank `45`. **A format fix removes the obstacle;
it does not decide the walk.**

#### The mask a body runs under is not the mask it was entered with

A guard that exempts a walk from some promise "because the machine is masked"
must follow the mask instruction by instruction. Reading it once from
`entry.registers.status` is the same answer only for a body that never changes
it, and a body that never changes it is not the interesting case.

Bank `45` is the worst kind of example. Its per-frame tick is called from
inside the VBlank handler, so it is entered with `I` set — and it does not stay
that way, because `4837`, `4872`, `47EE` and `49F2` each end a critical section
with an unconditional `CLI` rather than restoring the caller's mask. The
sequencer's guard read the entry byte, concluded no interrupt could land during
the body, and exempted the whole walk from the staleness test it exists to
apply. The timer driver then ran inside that body and the transcript read its
own snapshot afterwards.

Track it as a flag that starts from the entry mask and is updated wherever a
deferred status change lands — one place, if every mask change goes through
`append_checkpoint`. With the `SEI` rule above in force the flag only ever
opens, which is what makes it cheap to reason about: nothing inside a
transcript can mask the machine, because a walk stops in front of anything that
would.

#### The stop list and the execute list have to agree

A family that stops in front of an `RTS` at depth zero and executes one deeper
carries two lists of the same addresses, and they can drift apart silently. In
bank `45` seven returns — `4141`, `4243`, `4270`, `429E`, `42B2`, `42B9`,
`46D6` — were in the stop list and missing from the execute list, so a walk
that inlined a call ending at any of them fell through to `default` and gave
the whole transcript up. That single omission was **9,830 refusals on the
census route, six times every other cause in the bank put together**, and it
was invisible from the outside: the routine was admitted, entered, and simply
declined most of the time.

An address in one list and not the other is a routine that can be entered and
never left. Derive one list from the other, or assert they match.

#### Measure why a walk declines; do not reason about it

Four rounds of plausible reasoning about bank `45`'s refusal rate — the chunk
margin, the interrupt horizon, the driver-state guard, the MPR mapping — were
all wrong, and each cost a build and a 3,600-frame run to disprove. A
throwaway counter at every `return std::nullopt`, keyed by entry address and
stopping program counter, named the real cause in one run and named the second
one in the run after that.

It is twenty lines and it does not need to be good code. Add it, run once,
delete it. The temptation is to skip it because the next hypothesis feels
obvious; the record here is nought for four.

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

Two transcripts have now been written against the silicon's timing instead of
the core's — the `JSR`s at `02:53C8` and `02:5441`, whose pushes were declared
at offset 6, where the hardware puts them, rather than 0, where this core does.
Both were caught by the whole-route stack comparison and neither by anything
else, which is why that comparison is now `dc_rom_registry_stack_page_writes`
rather than a step someone is expected to remember.

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

**A check this good should not depend on remembering to run it.** Everything
above was already written down when a third stack-attribution fault was found:
`02:53C8` and `02:5441` credited their entry `JSR`'s two pushes at offset 6
rather than 0, 708 wrong lines in 4,205,699, deployed and passing for months.
The rule it violated has its own section two headings up. What was missing was
not the knowledge or the technique but the standing invariant, so the whole-route
comparison is now `dc_rom_registry_stack_page_writes`: record the route twice,
emulated and with every admitted routine deployed, and require the two traces
over `$2100-21FF` to be byte-identical. It runs in 33 seconds inside a suite
that takes 191, because it shares the critical path with nothing.

Keep it a comparison rather than a digest. Both times this defect class was
caught, the pinned per-routine hash covering the same routine had quietly been
refreshed to *agree* with the mistake — the two comments now stacked at the top
of `verify_devil_shooter_companion_dormant_semantic.cmake` are the record of it.
A digest says the registry still does what it did. A comparison against
assembly says the registry still does what the original does, and only the
second one is the claim the port is making.

**A replay is not a comparison.** `--routine-replay` verified `02:53C8@3` on
every Release run while the defect was live. It restores a savestate and re-runs
the frame with the same configuration, so when a routine is replaced both sides
of its comparison are the same transcript agreeing with itself. It proves a
transcript is independent of the state it was reached through — which is worth
proving — and nothing whatever about fidelity.

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

### A state-generic transcript supersedes a pinned one, and the pinned test proves it

Early families proved a transcript by reproducing one observed register state:
the walk refuses unless `A`, `X`, `Y`, `SP` and `P` match what the fixture
recorded, and it is admitted on the one arrival that matches. A hundred and six
routines in this registry are still of that kind. When a later, state-generic
family reaches the same address, the pinned one is not a competitor to be
ordered around — it is superseded, and the right move is to delete it.

`01:C024` is the clean case. It is a row of bank `01`'s jump table, `JMP $C5AB`,
and `replace_knight_terminal_release_helper_shared_jump_prefix` owned it gated
on `A=$42, X=$3A, Y=0, SP=$F4, P=0`: **1 of 64 arrivals**. The transcript it
produced was byte-identical to the one the general row produces, because a `JMP`
reads nothing, writes nothing and moves no flag — there is no state for a gate
to be about. Deleting the pinned function raised the address to 64 of 64.

**The proof that a supersession is safe is the pinned test's own hashes.** Do
not write a new one. That fixture already drives the exact route the pinned
transcript was built from — here, seventeen work-RAM patches to force the knight
terminal path — and it already pins the census, the call trace and the write
trace. Change its expected admission count and nothing else, and re-run it:

- **Hashes unchanged, count risen.** The general transcript does on all
  arrivals what the pinned one did on one. That is the whole claim, and it is
  now measured on the arrivals the pinned one refused.
- **Any hash moved.** The two transcripts are not the same transcript. Stop;
  the general one is wrong on some state the pinned gate was excluding, and
  the gate was load-bearing after all.

Sixty-three additional native arrivals here moved not one byte of any artifact.

**Watch for the shadowing, because nothing will tell you.** Dispatch is a chain
of `if (entry.routine == RoutineKey{...}) { return ...; }`, so the *first*
matching key owns the address outright — a `std::nullopt` from it falls back to
the interpreter, not to the next block. Add a family that overlaps an older one
and the older one silently stops running, wherever it sits in the file. Two
things surface it: `tools/Generate-NativeRegistry.ps1` merges the two evidence
sources into one row and marks it `+1`, and its partial-coverage count moves.
Read that summary after adding any family that spans a range rather than an
address.

### Price a jump table by profiling its range, not by collecting the rows other notes mention

`01:C000`-`01:C0CB` was carried in the progress log for several slices as "`01:C012`
and `01:C018`, 29,000 cycles for two transcribed jumps" — a poor rate, honestly
priced, and postponed accordingly. Both figures were right and the conclusion
was wrong. Those are simply the two rows that other notes in that file had cause
to name, because they are the trampolines in front of two heavily used routines.
Profiling the range found **eighteen** live rows worth 101,352 cycles, and the
table runs a hundred and seventy bytes further than the note said.

A dispatch table has no call graph to read: every row is reached by an indirect
mechanism — a request number, an index, a bank-call wrapper — so nothing in the
surrounding code names its rows, and the survey habits that work on routines
(read the callers, follow the `JSR`s) return nothing. **Grep the profile for the
address range instead**, which takes one command and prices the whole table
including the rows nobody has ever mentioned. The same applies to any structure
entered by computed address: vector tables, state-machine dispatch arrays, the
actor handler table.

### The analyzer groups contiguous addresses; a routine is joined by control flow

The candidate ranking builds approximate routines out of runs of contiguous
*executed* addresses, so a gap in the profile ends a run. A branch forward over
code the route never takes is exactly such a gap, and it splits one routine into
two candidates that are each a fragment of it.

`09:7237` is ten instructions and 327,924 master cycles, forty-two per cent of
what was left in bank `09`. It never appeared in a ranking. It branches over an
untaken countdown at `7249`-`725B`, so the analyzer offered `09:71EA`-`7247` and
`09:725E`-`72EA` — the first ending in the middle of the poll and starting in the
middle of an unrelated animation stepper, the second beginning at the poll's own
join point. The progress log carried both as separate recommendations, and neither
of them is a routine.

**Disassemble a candidate's range before pricing it, and price what the
disassembly says the routine is.** Two cheap habits catch this. A run that
starts at an address nothing calls, or ends at one that is not a return or a
jump, is a fragment — check the addresses immediately before and after it in the
profile. And a run whose *first* instruction is a plain load rather than
something a caller would arrive at is usually the far side of a branch. The fix
costs one `Disassemble-Rom.py` call over the union of the neighbouring runs,
which is what the "read a whole bank by its call graph" note already asks for
and what this file has otherwise been treating as optional.

### Check the core's opcode semantics, not the processor's documentation

`BIT #$01` at `09:7261` decides whether the score panel redraws. On a 65C02 the
immediate addressing mode of `BIT` is the documented exception that affects only
`Z` and leaves `N` and `V` alone. This core has one `BIT` macro shared across
every addressing mode — `P &= ~(Z|V|N); P |= ZNTable[x&A]&Z; P |= x&(V|N)` — so
immediate `BIT` clears `N` and `V` from the operand's top two bits like every
other form.

The transcript has to match the core, not the datasheet, because the core is
what the census was recorded from. Getting it wrong is not silent: the host
compares registers at the resume point and would have abandoned the walk. But it
would have abandoned it on every call, and an abandoned transcript still reports
as installed — the number that moves is `Registry abandonments`, which is easy
to read past when the deployment line says 433 of 433.

**Read the opcode out of `huc6280.cpp` and `huc6280_ops.inc` for any instruction
whose flag behaviour is mode-dependent or unusual**: `BIT`, `TSB`/`TRB`, the
`T`-flag arithmetic, the block moves, `BBRi`/`BBSi`. It is a grep, and it costs
less than reading an abandonment count backwards. The same source settles
timing, which is the other half: `JR` charges `ADDCYC(3)` on the taken arm and
`ADDCYC(1)` on the untaken one, and calls `LastCycle()` on both, so a taken
branch is 12 master cycles sampling at 9 and an untaken one is 6 sampling at 3.

### An oracle for a routine that produces a decision

The write-range comparison this file recommends everywhere assumes the routine's
product is bytes. `09:7237`'s product is a *choice*: which of four gates stopped
it, and what flags it returned. It writes one byte, thirty-three times in 3,600
frames. A write comparison over everything it touches would have been nearly
empty, and would have passed just as happily if the walk returned the wrong `N`
and `V` on all 2,616 calls that write nothing at all — which, given the `BIT`
above, was the live risk.

**Make the routine-call trace the primary artifact when the routine's output is
control flow.** It carries `output_a`, `output_status`, `output_sp`,
`body_master_cycles` and `return_instruction_pc` per call, so comparing an
emulated trace against a replaced one compares the arm and the register state
directly, for every call, at no extra cost — the trace is already being recorded
to check the absorbed return. `return_instruction_pc` alone distinguishes the
arms: 2,616 calls end at `7265` and 33 at `72EA`, and a gate read from the wrong
address moves that split before it moves anything else.

Keep the write channels underneath it, chosen for what the *decision* enables
rather than for what the routine writes. Here that means the VDC base ports:
the redraw the poll admits is a tile stream through `$0000`-`$0003`, so
`--bus-write-range` catches a dirty bit consumed one call early or late, which no
work-RAM census can see.

### A recommendation ages against the measurement it was built on

The progress log recommended widening bank `45`'s driver-state guard as "the
largest recoverable block", worth 476,490 master cycles in call sites plus
6,516 of 7,724 declined entries at `45:4044`. Every number in that sentence was
correct when it was written and none of them survived the slice that chunked the
music tick — the same slice, in the same file, a few sections above. Measured
before starting: `45:4044` admits 1,413 of 1,535, and the bank's emulated
remainder is 834,150 rather than the 4,430,103 the per-bank table still carried.
The recommendation was pointing at a pool the file itself had already taken.

The per-bank table is what made it hard to catch. Six of its eight rows matched
a fresh derivation exactly, because the slice that wrote them updated only the
bank it moved and carried the rest forward. A table that is right in six places
reads as current in all eight.

**Re-derive the per-bank remainder before acting on a recommendation built on
it**, the same way this file already insists on re-deriving the `JSR` bucket. It
is one command against the instruction profile. And when a slice updates one row
of a derived table, either re-derive every row or say in the table which ones
were measured — a stale row is worse than a missing one, because a missing row
sends the next reader to measure.

### A guard on another thread's memory is about the read's cycle, not the body's length

Bank `45`'s sequencer reads three bytes the timer driver writes from its own
thread of control. A transcript computes from a snapshot, so such a read is only
sound if the walk reaches it before the driver can next run, and the host
guarded that by refusing any transcript whose **finished body** exceeded the
interrupt horizon. The guard's own comment said the *read* had to be reached in
time. The code compared the whole body, so a walk that read the byte at cycle
300 and ran on to 12,000 was refused for something that could only happen 4,700
cycles after the read it was worried about.

**What the other thread can invalidate is a read, and a read is over at the
boundary of the instruction that made it.** Record that cycle — priced exactly
the way a store's is, at the next checkpoint — and compare it instead. Keep the
*last* such read rather than the first: once the body crosses the horizon the
core may deliver the interrupt at a checkpoint, so a read after that point is
stale however early the first one was.

Two things about the shape are worth keeping. First, the correction was worth
+3,159 master cycles, roughly a thousandth of what it had been recommended for,
because chunking had already made the bodies short — **a guard that reads as
over-conservative may simply be unreachable**, and the way to find out is to
measure it rather than to argue about it. Second, the proof was not a new test:
the family's existing two-run comparison passed unchanged with twenty-two more
chunks admitted. A guard change that is correct moves the admitted counts and
nothing else.

**And nothing lifts a guard of this kind.** The core abandons a transcript on a
register mismatch at the resume point; it never compares work RAM. An interrupt
handler that runs, rewrites the byte and returns with its registers restored is
invisible to the abandonment machinery — which is exactly what a timer-driven
sound driver does. A transcript's reads of another thread's memory can only ever
be guarded by the host.

### Identify a routine by the addresses it names, not by its control flow

`09:60C7` was picked out of a profile as the largest contiguous emulated run in
an untouched bank, with nothing said anywhere about what it did. Its control
flow answers nothing: fifty-nine instructions, no branch, six indexed writes
through a pointer it builds itself. Its *constants* answer everything. It builds
`$27B9 + block * 32`, and `$27B9` is the palette shadow — the buffer one
already-ported routine clears with a `TII` and another uploads with a `TIA`. It
finishes by writing `$2CCB`, which is the flag a third already-ported routine
clears after that upload. So it is colour cycling, and it belongs to a family the
port already had five members of and a name for.

**Grep the port's own sources and docs for a routine's constants before reading
its logic.** After a few dozen slices the addresses that matter have been named
somewhere, and a base address plus a flag address identifies a routine faster
and more reliably than tracing what it does to them. It also puts the new
routine in the right family, which is what decides its oracle: a producer that
writes a buffer the port has already documented can be compared against that
buffer, and the regression writes itself.

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

## Persisted routine-replay corpus

The normalized replay boundary is persistent now. Add one or more
`--routine-replay BANK:PC@ORDINAL` arguments to an authoritative assembly
family recording and name a directory with `--routine-replay-corpus-out`.
Multiple ordinals of the same routine are deliberately allowed: one route can
capture the distinct states a sibling batch needs.

```powershell
.\devils-crush.exe --record-gameplay out\family.csv `
  --observe-hook 00:F241 `
  --routine-call-trace out\family.calls.csv `
  --routine-replay 00:F241@1 --routine-replay 00:F241@2 `
  --routine-replay-corpus-out out\family-corpus `
  --rom "Devil's Crush.pce"
```

Corpus export is refused if the target has an explicit replacement or the
registry is deployed. The `.dcrr` entry is therefore an assembly oracle, not a
native adapter certifying itself. It contains the opaque pre-call machine
state, entry input and horizons, complete expected call and ordered writes,
and pre/post work RAM. The stable little-endian container is checksummed and
bound to both the supported ROM SHA-256 and exact Beetle revision.

During implementation, replay those calls directly with one semantic hook:

```powershell
.\devils-crush.exe `
  --routine-replay-corpus out\family-corpus\00-F241-1.dcrr `
  --routine-replay-corpus out\family-corpus\00-F241-2.dcrr `
  --replace-hook 00:F241@all --rom "Devil's Crush.pce"
```

The host restores at each call, requires the requested native adapter to run
exactly once, and compares output registers, return state, relative cycles,
ordered work-RAM writes, and complete post-call work RAM. A decline cannot
pass by falling back to assembly. `dc_rom_routine_replay_corpus` proves cold
capture, verified cache reuse, assembly replay, and native replay for two
`00:F241` calls; its cached path takes 0.08 seconds in Release and 0.21 seconds
in Debug on the development machine.

This is the inner edit loop, not the closure proof. Run the owning-family
Release route again before admitting a replacement so aggregate call coverage,
interrupt placement, artifact hashes, and the canonical census remain guarded.

The first family selected with this loop was the shared actor-slot allocator
at `01:C5AB`. Six cacheable uninterrupted calls cover its distinct natural
scan lengths, and two forced fixtures cover alternate reservation and pool
exhaustion, while the owning 3,600-frame route verifies all 44 calls and 72
interrupt-handler passages. That split keeps iteration fast without mistaking
the corpus for interrupt-placement or aggregate-coverage proof.

The next selected family, unsigned divider `00:E7D7`, reinforced the same
split. One uninterrupted cached call covers fast direct replay, while the
owning route proves all 105 calls, zero through six subtraction stores, and 82
interrupt-bearing scopes. The pure model includes the ROM's zero-divisor
behavior even though the natural route does not request it.

Its sibling at `00:E6DC` uses the same proof shape at byte width. Ordinal 3 is
the cached direct fixture because it is the last of three calls in frame 874;
using ordinal 1 would correctly replay the requested call and then observe the
two later replacements in the same frame. The owning route proves all 46 calls,
13 interrupt-bearing scopes, and 1,108 ordered writes. This is why a direct
fixture should be the last suitable call in its frame when an `@all` hook is
used for replay.

The initial Small-Soldier collision tail at `03:7F1A` demonstrates the other
useful kind of reuse: parameterize a proven caller-level composition when two
states differ only in field addresses, branch seams, stack return bytes, and
the outer return. Its steady sibling `03:7F57` kept the same child models and
regression unchanged. An uninterrupted ordinal-4 corpus supplied the fast
inner loop; the full route remained responsible for the interrupt-bearing
scopes and the ten deliberately unhandled scheduler transitions.

Exported bank-7 gate `07:4003` demonstrates why candidate boundaries should
include a public trampoline when it is part of every call. The export is only
`JMP $4634`, but beginning there owns another 12 cycles per call and avoids
leaving a hot entry seam interpreted. One canonical corpus covers the ordinary
path; a four-exit corpus plus a separately clean arming corpus covers every
branch without using an interrupt-bearing call as a normalized fixture.

Flipper release entry `01:D4B3` demonstrates the inverse accounting case: a
new parent can be semantically complete while adding only its prefix to the
deployed cycle total. Its 252-cycle body calls `01:C5C9`, whose 159-cycle body
and 21-cycle return were already native. Absorbing the complete parent and its
return therefore adds 273 - 180 = 93 cycles per call, not 273. Always use the
registry A/B cycle census for the claimed gain; a standalone observer's body
cost double-counts any child the deployed registry already owns.

It also demonstrates why a static contiguous range is not necessarily one
routine. `D4B3` stages a JSR frame that `C5C9` consumes, then the scheduler's
RTS returns directly to the original actor-scan caller. The following `D4C6`
bytes are a separately scheduled continuation, not fall-through work owned by
the `D4B3` transaction.

That continuation demonstrates how to port a parent whose child is too large
to compose. `D4C6` and `D4C9` do not inline `D500` or `CBAB`; each native seam
only stages its JSR frame and crosses into the existing registry owner. The
return address is itself admitted, so execution resumes natively after the
child completes. This preserves the collision route's bounded chunks while
recovering every instruction in the small parent. The canonical parent can
therefore contain a 1,197-write call without any individual replacement
exceeding the bridge's 512-write channel.

The matching `D48E/D491/D494` leading pass confirms that the seam pattern is
reusable within one state machine. Its phase and collision children stay
independently credited, while `D494` owns only the local post-collision
decision. The measured registry delta is the sum of those previously
interpreted seams, not either enclosing observer scope.

Release prefix `D458-D463` shows the same seam at a scheduler boundary. Own the
51-cycle state prefix plus the 21-cycle `JSR`, stage return `D465`, and hand
execution to the admitted `C5C9` entry. The child consumes that frame and
absorbs its own final return; the new parent gains exactly 72 cycles per call
without duplicating the scheduler's 180 already-native cycles.

Hardware-facing leaf `08:5B01` shows how to decompose an oversized producer
without hiding device behavior. Keep its six VDC stores in the ordinary
ordered-write channel, checkpoint every instruction, and validate the whole
`0000-0003` bus stream in addition to RAM and call artifacts. The VDC applies
its stolen cycle through the bus at each store; checkpoint reconciliation
keeps that cost inside the original instruction timing. This turns a parent
whose calls exceed 512 RAM writes into a bounded 283-call child without
pretending hardware writes are ordinary memory.

Guarded idle entry `08:5E42` shows when partial semantic coverage is the
honest boundary. The canonical route proves 2,649 two-zero returns, but never
enters the state-changing renderer beyond the prefix. Model the idle predicate
in pure C, checkpoint and absorb that complete return, and decline either
active byte. A hot measured path can be fully removed without treating an
unobserved continuation as understood.

Boot initializer `08:584C` shows the matching split when the wall is in the
middle of a routine. Its `TII $3000,$3001,$0FFF` is one instruction and 4,095
ordered writes, so neither a 512-write transcript nor instruction-boundary
chunking can own it. Cross both calls before it, stop the clear prefix on the
TII, and register the first address after it for the VDC tail and final return.
That moves only 183 cycles, but makes the capacity decision explicit and lets
one outer-call artifact prove that the emulated instruction stayed in exactly
the same place.

`00:E4E3` shows a different oversized-call shape: its backward branch lands on
the routine's own registered entry after every compressed record. Treat that
re-entry as the transaction boundary. The outer observer remains deliberately
nested, so exact outer and inner call/write artifacts prove that record-level
transactions preserve the whole call even though direct replay is practical
only for the terminal record (a nonterminal replay invokes more than one
replacement by design).

For global screening, `Analyze-InstructionProfile.ps1 -UnregisteredOnly
-ExcludeFrameSyncWait` now reports only deployed non-entry residue outside the
`00:E321-E32B` wait. Pair it with the native-cycle census's
`entries - native_calls` screen: specialized or guarded registry entries can
hide the next coherent family from a non-entry-only ranking.
