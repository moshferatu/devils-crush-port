# Retained Assembly Seams

## Current priority: collision math calls and multiply transfers (September 9)

This user ranking supersedes the actor-timer ranking below. The live recording
was not supplied; reductions are measured on local replays.

| Rank | Address | Master cycles | Intervals | Implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 01:C9C3 | 16,854 | 790 | Shared live-stack JSR to E5EC |
| 2 | 01:C9D3 | 16,734 | 790 | Shared live-stack JSR to E85D |
| 3 | 00:E8A3 | 16,710 | 790 | Shared live-stack RTS |
| 4 | 00:E5AD | 16,248 | 1,332 | STA $30 using live A |
| 5 | 00:E5B8 | 16,224 | 1,332 | STA $2F using live A |
| 6 | 00:E5B2 | 16,200 | 1,332 | LDA $43 |
| 7 | 00:E5B4 | 16,200 | 1,332 | STA $30 using live A |
| 8 | 00:E5EF | 16,200 | 1,332 | STA $46 using live A |
| 9 | 00:E5B6 | 16,176 | 1,332 | LDA $42 |
| 10 | 00:E5FB | 16,152 | 1,332 | LDA $46 |
| 11 | 00:E5A0 | 16,128 | 1,332 | Flag-preserving STZ $42 |
| 12 | 00:E5A2 | 16,128 | 1,332 | Flag-preserving STZ $43 |

The nine zero-page operations share `replace_math_transfer_instruction`.
Each resumes independently, samples interrupts at 9 master cycles and
retires at 12, with stores at 12. LDA updates N/Z; STA and STZ preserve all
flags. Stores use live A after the intervening child calls. Exact PC,
MPR7=00, MPR1=F8, full work RAM and clear T are required. Decimal mode,
all register values and all event horizons are supported.

The two calls push C9C5/C9D5 high then low at cycle zero, sample at 18 and
complete at 21. The return consumes the live wrapping stack and resolves
the destination through the live MPRs. These three boundaries retain strict
>21-cycle guards on both event horizons. Existing full math walks retain
their dispatch priority; no core patch or arithmetic algorithm changed.

`math_transfer_fixture.hpp` covers 589,824 value/status combinations,
including flag preservation, live stores, mapping, T and horizon guards.
The shared call fixture now covers seventeen keys over every stack pointer
and status value; the shared return fixture covers 9,043,968 cases across
46 keys. Release and Debug fixtures pass.

`verify_math_transfer.cmake` uses the natural 3,600-frame twist route with
all twelve hooks, without RAM patches. It pins the ROM bytes, every hook's
coverage, and all five assembly hashes: frame CSV, scheduler calls, scoped
scheduler writes, full work-RAM writes and device writes. The nine transfers
each enter 344/344 times; C9C3 is 152/172, C9D3 150/172 and E8A3 152/172.
The trace contains 2,088 scheduler calls, 870,896 scoped writes, 12,470,985
RAM writes and 3,265,435 device writes.

The generated registry grows from 817 to 829 admitted routines and 865 to
877 dispatch-reachable keys, with 3,864,141 of 3,920,602 proven calls (98.6%).
The seam manifest has 288 non-overlapping ranges. Both registry and seam
checks pass.

Release validation passes: the five-trace assembly/native oracle (97.49 s),
the host registry, and both face/hand capture regressions. Debug validation
also passes: the five-trace oracle (697.16 s), face capture (323.65 s), hand
capture (315.78 s), and the shared fixtures. All five assembly/native artifacts
match exactly in both configurations.

Full-registry twist measurement, using the saved pre-change Release binary
and the current registry on the same natural route, reduces ranked residue
from 42,852 to 1,350 master cycles (41,502 removed, 96.8%). Nine transfer
sites reach zero; C9C3 retains 468 cycles, C9D3 462 and E8A3 420 at guarded
boundaries. Total interpreter residue falls by exactly the ranked delta.
Both runs retain frame SHA-256
`ca4a0762f74bc1b44cf5487a5c658cb0db5fae09cebbe61422632c88e6d7c8a3`
and zero abandonments. This before/after registry comparison is separate
from the five-artifact assembly oracle above.

Across the 31-route compatibility corpus, ranked residue totals
43,020 -> 1,350 cycles (41,670 removed, 96.9%). Twist contributes 41,502
cycles; spiral and alley each contribute 84 and reach zero. The other 28
routes have no residue at these sites before or after. Every frame hash and
abandonment count is unchanged, and every route's total interpreter delta
equals its ranked delta. The pre-existing single abandonments on higharc,
radial and sidestep are unchanged; the other 28 routes have zero. The corpus
is compatibility evidence, not a replacement for the user's priority list.

Evidence: `out/math-transfer-cost/manifest.csv`, `twist-addresses.csv`,
per-route before/after profiles and logs; `out/math-transfer-fixture-*.log`,
`out/math-transfer-tests-*.log`, `out/math-transfer-registry-check.log` and
`out/math-transfer-seams.log`.


## Previous priority: actor timers, music SEI and stream CLY (September 9)

The latest user ranking supersedes the Eyeball/countdown ranking below. The
live recording was not supplied; all measured reductions below are from local
replays, not estimates of the user's route.

| Rank | Address | Master cycles | Intervals | Implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 03:7E7E | 4,083 | 191 | Shared live-stack RTS |
| 2 | 03:66A3 | 3,150 | 110 | Full Three-Blobs prefix, then sampled DEC fallback |
| 3 | 03:7643 | 3,096 | 144 | Shared live-stack JSR to C018 |
| 4 | 03:7E6A | 2,952 | 192 | Indexed low-countdown LDA |
| 5 | 45:485D | 2,940 | 490 | SEI sampling the old I mask |
| 6 | 03:7E76 | 2,928 | 192 | High-countdown ORA using live A |
| 7 | 03:7E73 | 2,880 | 192 | Low-countdown STA using live A |
| 8 | 02:567F | 2,655 | 123 | Big-Star timer DEC |
| 9 | 03:66AA | 2,577 | 159 | Three-Blobs timer STA using live A |
| 10 | 03:66B2 | 2,577 | 159 | Three-Blobs toggle STA using live A |
| 11 | 00:E438 | 2,514 | 415 | Flag-preserving CLY |
| 12 | 03:66AD | 2,457 | 159 | Indexed Three-Blobs toggle LDA |

Eight actor instructions and CLY share `replace_ranked_timer_instruction`.
Exact PC, mapped code and clear T are required; actor RAM operations also
require full work RAM and MPR1=F8. Decimal mode, all X values and all event
horizons are supported. LDA/ORA/STA sample at 12 and complete at 15 master
cycles, with stores at 15; DEC samples at 18 and writes at 21. CLY samples at
3, completes at 6, and preserves every status flag. Countdown stores and ORA
consume live A after the intervening core borrow/decrement instructions.

The existing full Three-Blobs prefix at 66A3 retains priority and its
300-cycle timer guard. Declined prefixes now execute the single DEC through
a native checkpoint rather than the core capability. The old Release oracle
retains its frame, call, write and routine-replay hashes while coverage rises
from 1,089/1,107 to 1,107/1,107. The full prefix itself is unchanged.

SEI at 485D shares the 49E3 implementation: sample the old I mask at cycle 3,
retire with I set at 6, and preserve the old-mask sample through the existing
`samples_interrupt_flag_clear` checkpoint field. No core patch changed.
JSR at 7643 pushes 7645 high then low at cycle zero, samples at 18, and
completes at 21 to the live mapping of C018. RTS at 7E7E uses the live wrapping
stack and destination mapping. Both retain strict >21-cycle event guards.

`verify_ranked_timers.cmake` activates the Big-Star controller and two fixed
scheduler slots in a 3,600-frame assembly/native census. The 256-tick actor
counter includes a real low-byte borrow and expiry. All twelve hooks enter:
7E7E 386/446, 66A3 1,301/1,301, 7643 62/73, 7E6A/7E73/7E76 each 448/448,
485D 324/324, 567F 787/787, 66AA/66AD/66B2 each 43/43, and E438 19,040/19,040.
All five assembly/native artifacts match in both Release and Debug:

| Artifact | SHA-256 |
| --- | --- |
| Frame census | `1b96bbd4ed757318f0825a338f2d54155f165361e389d29f155dde63f5233a2e` |
| Scheduler calls | `e2d03614156b764e743fd50bd6ffa573f1fc9008b029c1e36ac8739592fceb06` |
| Scheduler writes | `4609f780f8e248b4e8d55fc7ea4be9ae1f731872f5dc095041b4c4fb71956d46` |
| Work-RAM writes | `790683ae44c0edee30fbc6c266a7a7c1425c1533121127be919ffd81ee186bfb` |
| Device writes | `e1c4253c527b577f124f2c4d1fa58fdf6caf6c4e3e3f49fb93d8c49e0c1d5e47` |

The oracle compares 3,074 scheduler calls, 1,398,843 scoped writes,
12,778,378 full work-RAM writes and 4,240,033 device writes.
The shared fixture verifies 4,128,768 timer/CLY cases, 65,536 independent
countdown OR operand pairs, both SEI addresses across all statuses and short
horizons, fifteen JSR keys, and 8,847,360 cases across 45 RTS keys.

The targeted full-registry comparison in `out/ranked-timers-cost/active.csv`
reduces ranked residue from **75,171 to 2,661 master cycles (96.5%)**.
Every opcode site reaches zero; only guarded JSR/RTS residue remains. Both
runs preserve frame hash
`ba83a3bc15b9f62227800a33c289d692e4a41476fddfdcfceaceb9db558e8eae`
and zero abandonments; the total interpreter-cycle reduction equals the
ranked reduction. This measures before/after compatibility, not assembly
parity for the full registry. The saved pre-change binary fails at frame
1,482 on the artificially forced actor's later animation; that baseline log
is retained as `active-expiry.before.log`. The measurement therefore adds
`1400:3BC6:FF` identically to both runs to extend its countdown. The separate
assembly oracle above retains the real expiry without that extension.

The generated registry grows from 806 to **817 admitted routines** and from
854 to 865 dispatch-reachable keys. The seam inventory passes with 276
nonoverlapping ranges. The 31 full-registry compatibility routes in
`out/ranked-timers-cost/manifest.csv` reduce ranked residue from **153,195 to
510 cycles (99.7%)**, with every frame hash and abandonment count unchanged.
All total interpreter deltas equal their ranked deltas. Remaining residue is
only the guarded 03:7643 call on nine routes; 22 routes reach zero. The saved
baseline's one abandonment each on higharc, radial and sidestep is unchanged;
the other 28 routes retain zero. The corpus validates compatibility and does
not replace the user's live ranking as the porting priority.

Release passes the shared instruction fixture, the new five-artifact oracle,
the existing Three-Blobs prefix oracle, the host registry, and both original
face/hand captures. Debug passes the shared fixture, the new five-artifact oracle
(811.75 seconds), and both face/hand captures. Registry and seam checks and
`git diff --check` pass.


## Previous priority: Eyeball, countdown and music interrupt ranking (September 9)

This user-supplied ranking supersedes the bumper/sprite ranking below. The
live recording was not supplied; the measurements use separate local replays.

| Rank | Address | Master cycles | Intervals | Implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 02:4E32 | 28,176 | 1,328 | Shared live-stack RTS |
| 2 | 03:7C34 | 6,816 | 320 | Shared live JSR to C018 |
| 3 | 02:432C | 5,427 | 255 | Shared live-stack RTS |
| 4 | 02:4E24 | 5,103 | 329 | Full Eyeball idle walk, then indexed LDA fallback |
| 5 | 45:4B85 | 4,926 | 110 | Existing sampled RTS now crosses event horizons |
| 6 | 45:49E3 | 4,686 | 773 | SEI with distinct sampled and retired I masks |
| 7 | 02:4E29 | 4,188 | 329 | Live zero-page session flag LDA |
| 8 | 02:4E2D | 4,158 | 329 | BEQ using live Z |
| 9 | 03:7DE0 | 4,059 | 191 | Shared live-stack RTS |
| 10 | 02:4324 | 3,888 | 256 | Countdown STA using live A |
| 11 | 02:431B | 3,840 | 256 | Countdown indexed LDA |
| 12 | 02:4327 | 3,840 | 256 | Countdown indexed ORA using live A |

All twelve sites have native coverage. Six live actor instructions share
`replace_eyeball_countdown_instruction`: exact PC, mapped code/RAM, complete
RAM and clear T are required; decimal mode and every X are supported. Each
instruction samples three cycles before retirement and accepts all horizons.
Indexed LDA/STA/ORA take 15 cycles (STA writes at 15); zero-page LDA takes 12;
BEQ takes 6 or 12. The existing full 4E24 idle walk retains priority. Its old
oracle now admits 1,377/1,377 entries: 1,066 full walks and 311 load fallbacks,
with every original state, routine, call and write hash unchanged.

The three actor RTS entries share the existing strict >21-cycle guards and
live wrapping stack/mapper handling. JSR at 7C34 pushes 7C36 high then low at
cycle zero, samples at 18, and retires at 21 to the live mapping of C018.
The audio RTS at 4B85 already had a checkpoint for the status restored by PLP;
that checkpoint now permits all event horizons. SEI at 49E3 samples the old
I mask at 3, then retires with I set at 6 using
`samples_interrupt_flag_clear`. A pending interrupt therefore sees the old
sample while stacking the retired status. The full music walks retain their
existing boundary before an unmasked SEI.

`eyeball_countdown_fixture.hpp` exercises 2,752,512 actor operand/accumulator/
status cases and 2,048 SEI status/horizon cases. The shared fixture now covers
fourteen JSR keys and 44 RTS keys (8,650,752 return target/status/stack cases).
Both Release and Debug fixtures pass.

`verify_eyeball_countdown.cmake` pins all twelve ROM opcodes and activates the
Eyeball plus two fixed scheduler countdowns. It covers the low-byte borrow,
expiry and subsequent animation. All five isolated assembly/native artifacts
match: frame state, scheduler calls, scoped writes, full RAM writes and device
writes. The route observes 1,817 scheduler calls, 946,403 scoped writes,
11,941,811 RAM writes and 3,410,099 device writes. Admission counts are
235/262 at 4E32, 13/16 at 7C34, 224/255 at 432C, 263/263 at 4E24,
84/84 at 4B85, 2,594/2,594 at 49E3, 53/53 at both 4E29 and 4E2D,
332/382 at 7DE0 and 256/256 at each countdown load/store/ORA.

The full-registry targeted measurement holds the two artificially installed
actors in their countdowns by extending their high timers before expiry. This
avoids a forced later animation state that the saved pre-change full registry
cannot execute (it stops at frame 1414 without those extensions). The isolated
assembly oracle separately covers expiry without extending the timers.
Targeted residue falls from **199,572 to 10,746 master cycles**, removing
**188,826 (94.6%)**. All six actor instructions and both music sites reach zero;
the remaining cost is the guarded JSR/RTS entries. Before/after frame hashes
match, both runs have zero abandonments, and the total interpreter reduction
equals the reduction at the twelve targets. These full-registry hashes are
compared with the saved full-registry baseline, not with assembly.

Evidence: `out/eyeball-countdown-cost/active.csv`, `active-addresses.csv`,
the per-route profiles and the Release/Debug test logs. Registry generation
admits **806 routines out of 854 reachable keys**, with 3,838,307 of 3,894,653
proven calls (98.6%). The seam manifest verifies **269 non-overlapping ranges**.
Release checks pass for the new oracle, original Eyeball idle oracle, audio
wrapper return (including the pending-interrupt stagger route), host registry
and original face/hand captures. The 31 standard compatibility routes preserve
every before/after frame hash and abandonment count. Ranked residue falls from
**138,114 to 3,138 cycles (97.7%)**, with only guarded 03:7C34 calls remaining.
The total interpreter delta equals the target delta on every route. Higharc,
radial and sidestep each retain their one pre-existing abandonment; the other
28 routes retain zero. Debug checks also pass for the new five-artifact
assembly oracle, audio return/stagger regression and original face/hand
captures. Including the shared fixtures, seven selected Release checks and
five selected Debug checks pass. The new oracle takes 97.50 seconds in Release
and 798.83 seconds in Debug; full RAM/device trace export accounts for the
longer Debug run. Registry/seam checks and the final whitespace check pass.

## Previous priority: bumper, face timer and sprite overflow ranking (September 9)

This user-supplied ranking supersedes the bonus countdown ranking below. The
live recording was not supplied; the measurements here use separate local
replays, with targeted bumper, face and sprite-overflow activation.

| Rank | Address | Master cycles | Intervals | Implementation |
| --- | --- | ---: | ---: | --- |
| 1 | 03:7ADB | 21,393 | 1,005 | Live indexed bumper timer DEC |
| 2 | 03:7A7F | 15,267 | 1,005 | Live indexed bumper activity LDA |
| 3 | 03:7A71 | 15,195 | 1,005 | Live indexed bumper collision input LDA |
| 4 | 03:76C5 | 15,078 | 694 | Live indexed DEC fallback after full face wait |
| 5 | 08:40F6 | 12,858 | 602 | Live mapped indirect sprite pointer-high LDA |
| 6 | 08:40F1 | 12,834 | 602 | Live mapped indirect sprite pointer-low LDA |
| 7 | 08:40FB | 12,786 | 602 | Live mapped indirect sprite entry-count LDA |
| 8 | 08:4106 | 12,738 | 602 | Shared live-stack sprite overflow RTS |
| 9 | 03:7A82 | 12,192 | 1,005 | Live N activity BPL |
| 10 | 03:7A77 | 12,180 | 1,005 | Live carry collision BCC |
| 11 | 03:7ADE | 10,692 | 1,005 | Live Z timer BNE |
| 12 | 08:40DF | 9,246 | 602 | Live indexed descriptor LDY fallback after full emitter |

All twelve ranked sites have native coverage. Eleven instructions share
`replace_bumper_sprite_instruction`; the overflow return extends the shared
RTS implementation. The existing full face wait and sprite emitter retain
priority. Their instruction fallbacks read live registers and RAM, without
assuming a fixed actor slot, reconstructing an earlier result, or skipping
the following branch. The sprite loads resolve the current zero-page pointer
and Y through the live MPR mapping, including 16-bit address wrapping and the
supported ROM's bank mirrors. Device, backup-RAM and unmapped reads decline.

Indexed loads sample at cycle 12 and finish at 15. Indexed DEC samples at 18
and writes at 21; indirect indexed LDA samples at 18 and finishes at 21.
Branches sample at 3/9 and finish at 6/12. Each instruction retains a checkpoint
at all event horizons and requires exact PC/code mapping, mapped full work RAM
and T clear; decimal mode is permitted. The overflow RTS retains the existing
strict greater-than-21 event guards and live wrapping stack behavior.

The ROM-free fixture checks 3,670,016 indexed operand/status combinations and
1,179,648 indirect pointer/mapping cases, including every 16-bit pointer,
address wrapping, RAM aliases, ROM mirrors and device rejection. It checks
register preservation, live branch flags, decrement writes and interrupt
sampling. The shared return fixture now covers 41 sites and 8,060,928
target/status/stack cases. Both Release and Debug fixtures pass.

The new two-route oracle pins all twelve opcodes and compares five complete
artifacts against assembly: frame state, scheduler calls, scoped call writes,
all RAM writes and all device writes. The bumper route uses the existing
terminal activation at frame 3341 and reserves 63 SAT entries for four frames
to exercise cursor-overflow exits. Each bumper instruction enters 1,202 times;
each indirect load enters 128 times, with 111/128 native overflow returns.
The emitter admits 75,757/75,757 calls, retaining its full path for 75,629 and
using the descriptor-load fallback for 128. The face route admits 872/872
entries, preserving 639 full waits and adding 233 decrement fallbacks. The
older face wait oracle retains all its state, call, write and replay hashes;
only its admission count changes. The existing full emitter and bumper
terminal regressions also pass unchanged.

Registry membership rises from 786 to 796, reachable keys from 834 to 844,
and classified seam ranges from 247 to 259.

Paired full-registry replays against the saved pre-change executable reduce
target cycles on the bumper/overflow route from 116,166 to 357, removing
115,809 (99.7%). The face route removes all 5,037 target cycles. Combined,
121,203 target cycles fall to 357: 120,846 removed (99.7%). The remaining
cycles are exclusively at guarded 08:4106 returns; every new non-return
instruction has zero residue when exercised. Both frame hashes are unchanged,
both routes have zero abandonments, and each route's total interpreter-cycle
delta equals its target-address delta. These are before/after full-registry
comparisons; the isolated assembly oracle is separate evidence.

Detailed measurements: `out/bumper-sprite-cost/active.csv` and
`out/bumper-sprite-cost/active-addresses.csv`. Release passes the shared fixture,
both new oracles, existing face wait, full emitter, bumper terminal, host
registry, and the original face and hand captures. The new oracles pass in
99.95 and 100.68 seconds. Logs: `out/bumper-sprite-tests-Release.log`,
`out/bumper-sprite-face-hand-Release.log`, and
`out/bumper-sprite-fixture-Debug.log`.


The 31-route full-registry corpus preserves every frame hash and abandonment
count. Target residue falls from 8,022 to zero across alley (1,578), lanes
(3,342), stagger (1,836) and twist (1,266); the other 27 routes have zero target
residue before and after. All gains are at 03:76C5. The saved baseline has one
abandonment each on higharc, radial and sidestep, unchanged after these ports;
the other routes have none. Report: `out/bumper-sprite-cost/manifest.csv`.
The original face and hand captures also pass Debug in 283.54 and 284.38 seconds.
Both new Debug oracles pass all five assembly/native artifact comparisons:
bumper/overflow in 813.10 seconds and face timer in 764.18 seconds. The four
Debug ROM checks pass together in 1,047.75 seconds; with the shared fixture,
all five selected Debug checks pass. Final log: `out/bumper-sprite-tests-Debug.log`.

## Previous priority: bonus hazard countdown and return ranking (September 9)

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

## Interrupt return vector: 00:E003

The final JMP in the sound interrupt return chain now runs natively and
hands off at E179. It uses the existing single-instruction jump helper:
12 master cycles, an interrupt sample at 9, and both event horizons
strictly greater than 12. The live PC, bank-zero code mapping and clear
transfer flag are required. Registers, status, stack and RAM are preserved.

The fixture now covers 262,144 stack/status combinations across four
jump entries, plus both event horizons, invalid PC, key and mapping.
The expanded interrupt-chain oracle pins the E003 opcode and admits
129/130 vector jumps alongside FC03 and FC89. Frame-state, scheduler
caller observations, ordered caller writes, all RAM writes and hardware
writes retain their existing assembly hashes. The registry admits 720
routines and the manifest validates 200 classified ranges. The manifest
also separates the following reset initialization from this return jump.

All 31 current-registry A/B routes preserve frame-state hashes and
abandonment counts when only E003 is excluded on the before side. Its
interpreter cost falls from 135,336 to 2,916 master cycles (132,420 removed,
97.85%). Every cycle delta is confined to E003. Non-wait interpreter work
falls from 13,397,769 to 13,265,349 (0.99%); frame-sync waits are unchanged.
This measures the isolated increment within the registry, not fresh
whole-registry assembly parity. Evidence is in
`out/interrupt-vector-cost/manifest.csv`, its before/after rankings and
`out/interrupt-vector-cost-summary.log`.

Release and Debug each pass four targeted checks: the expanded fixture,
interrupt-chain assembly oracle, corrupted-face capture and hand-bus
capture. Logs are `out/interrupt-vector-tests-{Release,Debug}.log`.
Both face replays retain the corrected `a263ebb4...b35a` hash; the original
capture inputs are unchanged. Registry/seam validation and `git diff --check`
pass. The full suite was not rerun.

## Actor countdown return: 02:4F75

The countdown's RTS now enters the shared native live-stack return helper.
It reads both caller bytes with stack-page wrapping, adds one with word
wrapping, and selects the target bank from the live MPRs. Registers and
status remain intact. Both event horizons must exceed 21 master cycles;
near events, or with an invalid PC, code/stack mapping or short RAM, the
original core executes the return. The offset-zero absorbed-return record
preserves the caller observer without adding an interrupt checkpoint.

The expanded fixture covers 983,040 return target/status/stack cases across
five entries, plus both event horizons and invalid inputs. The alley-route
assembly oracle pins the RTS byte and admits 188/223 returns. All five
frame-state, caller, ordered caller-write, RAM-write and hardware-write
artifacts match assembly. The ordinary gameplay route does not exercise
this return. Registry generation admits 719 routines; the seam manifest
splits this return from its countdown prefix and validates 199 ranges.

All 31 current-registry A/B routes preserve frame-state hashes and
abandonment counts when only 02:4F75 is excluded on the before side.
Target interpreter cost falls from 122,982 to 15,063 master cycles
(107,919 removed, 87.75%). Every cycle delta stays at this RTS. Non-wait
interpreter work falls from 13,505,688 to 13,397,769 (0.80%); frame-sync
wait cost is unchanged. Evidence is in `out/countdown-return-cost/manifest.csv`,
its before/after primary rankings and `out/countdown-return-cost-summary.log`.
This isolates the increment within the current registry; it is not fresh
whole-registry assembly parity. Near-event residual work remains visible
in the primary ranking.

Release and Debug each pass the countdown-return assembly oracle, expanded
fixture, preceding bit-branch oracle and face/hand capture regressions
(five checks each). Logs are `out/countdown-return-tests-{Release,Debug}.log`.
Both face replays retain the corrected `a263ebb4...b35a` hash, and the
original capture inputs are unchanged. Registry generation, all 199 seam
ranges and `git diff --check` pass. The full suite was not rerun.

## Collision and tile-upload bit branches: 01:CB68 and 00:FA67

The two largest non-host interpreter seams now have guarded native entries.
CB68 is `BBS1 $77, $CB93` (the old manifest incorrectly called it BBS2);
FA67 is `BBR0 $26, $FA6E`. Both read live work RAM, preserve all registers
and flags, and hand off at the selected arm. The existing enclosing walks
still stop before these instructions so each gets a fresh event horizon.

The core performs a zero-page load with an interrupt sample at master
cycle 9 and completion at 12, followed by the branch sample at 15 or 21
and completion at 18 or 24. The native entry requires both event horizons
to exceed the complete opcode cost. With no intervening event, RAM write
or interrupt-mask change, the final sample produces the same latch. A
single checkpoint preserves that final sample without exposing an
interruptible boundary in the middle of an opcode. Near either event,
the original core executes both samples. Invalid live PCs, code/RAM
mappings, short RAM and transfer mode also decline.

The expanded fixture covers 131,072 operand/status combinations, both
event horizons from 0 through 25 for every operand, and invalid inputs.
The assembly oracle pins both ROM instructions and preserves all five
frame-state, scheduler-call, ordered call-write, RAM-write and hardware-write
artifacts. Canonical admissions are 373/404 at CB68 and 741/832 at FA67.
The registry contains 718 entries; the manifest retains 198 ranges and
classifies these two addresses as guarded.

All 31 paired current-registry routes preserve frame-state hashes and
abandonment counts when only CB68 and FA67 are excluded on the before
side. Target interpreter cost falls from 1,120,458 to 112,980 master
cycles (1,007,478 removed, 89.92%), with every cycle delta confined to
these addresses. Non-wait interpreter work falls from 14,513,166 to
13,505,688 (6.94%). Evidence is in `out/bit-branches-cost/manifest.csv`,
its before/after primary rankings and `out/bit-branches-cost-summary.log`.
This isolates the increment within the current registry; it is not fresh
whole-registry assembly parity. Event-adjacent residual work remains
visible in the primary ranking.

Release and Debug each pass the bit-branch assembly oracle, expanded
fixture, preceding VRAM-stream oracle and face/hand capture regressions
(five checks each). Logs are `out/bit-branches-tests-{Release,Debug}.log`.
Both face replays retain the corrected `a263ebb4...b35a` hash and all three
original capture inputs are unchanged. Registry generation, all 198 seam
ranges and `git diff --check` pass. The full suite was not rerun.

## VRAM stream loop and return: 00:E479 and 00:E47C

The stream decoder's BRA E438 and final RTS now have standalone native
entries. E479 shares the unconditional-branch helper with the interrupt-chain
JMPs: 12 master cycles, interrupt sampling at cycle 9, clear transfer mode
and a matching live bank-zero code mapping. Both event horizons must exceed
12 cycles. It hands off at E438 so the next record uses a fresh entry.

E47C uses the shared live-stack return helper. It reads the page-wrapped
caller frame, preserves registers and flags, and emits the offset-zero
return observation. Both event horizons must exceed 21 cycles, and the
live stack and code mappings must match. The fixture now covers 196,608
branch stack/status combinations and 786,432 return target/status/stack
cases, including both event horizons and invalid mappings.

The assembly oracle checks the BRA/RTS ROM bytes and observes E41F and E47C.
It admits 6,321/6,773 branch entries and 20/20 returns, with identical pinned
frame-state, call, ordered call-write, all-RAM-write and hardware-write
artifacts. Loop entries are counted through replacement admissions rather
than opened as subroutine observations. The registry contains 716 entries;
the seam manifest still contains 198 ranges.

All 31 paired current-registry routes preserve frame-state hashes and
abandonment counts with only E479 and E47C excluded on the before side.
Their interpreter cost falls from 162,351 to 14,916 master cycles (147,435
removed, 90.81%), with every cycle delta confined to those addresses.
Total non-wait interpreter work falls from 14,660,601 to 14,513,166 (1.01%).
Evidence is in `out/vram-stream-tail-cost/manifest.csv`, its before/after
primary rankings and `out/vram-stream-tail-cost-summary.log`. This measures
the isolated increment within the current registry, not fresh whole-registry
assembly parity.

Release and Debug each pass the new assembly oracle, expanded fixture,
existing interrupt-chain oracle and face/hand capture regressions (five
checks each). See `out/vram-stream-tail-tests-{Release,Debug}.log`. Both face
replays retain the corrected `a263ebb4...b35a` hash, and the original capture
files are unchanged. Registry generation, all 198 seam ranges and
`git diff --check` pass. The full suite was not rerun.

## Interrupt-chain jumps: 00:FC03 and 00:FC89

Both absolute jumps now execute natively: FC89 hands off to FC03, and FC03
hands off to E003. Each remains a separate 12-master-cycle instruction with
an interrupt sample at cycle 9. Registers, flags, mappings and the live stack
are preserved; no RAM write or absorbed-return notification is emitted.
The replacement requires the matching live PC, bank-zero MPR7 and clear
transfer mode, and declines unless both event horizons exceed 12 cycles.
The old seam descriptions incorrectly called these instructions SEI; the
ROM oracle checks their actual JMP bytes.

The fixture covers 131,072 stack/status combinations across both entries,
both event horizons and invalid code mappings/PCs/keys. The canonical
assembly comparison admits 126/130 FC03 entries and 123/123 FC89 entries.
Its pinned frame-state, scheduler-call, ordered call-write, all-RAM-write
and hardware-write artifacts are identical to assembly. The four deferred
entries remain visible as guarded interpreter work. The registry contains
714 entries, and the manifest still contains 198 ranges.

All 31 paired current-registry routes preserve frame-state hashes and
abandonment counts when only these two addresses are excluded on the before
side. Their interpreter cost falls from 268,068 to 7,512 master cycles
(260,556 removed, 97.20%); every cycle delta stays at the target addresses.
Non-wait interpreter work falls from 14,921,157 to 14,660,601 (1.75%).
Evidence is in `out/interrupt-chain-cost/manifest.csv`, its before/after
primary rankings and `out/interrupt-chain-cost-summary.log`. This isolates
the new entries within the current registry; it is not a fresh whole-registry
comparison against assembly.

Release and Debug each pass the assembly oracle, expanded fixture, face
capture and upper-hand bus capture (four checks each). Logs are
`out/interrupt-chain-tests-{Release,Debug}.log`. Both face replays retain
the corrected `a263ebb4...b35a` hash, and the original capture is unchanged.
Registry generation, all 198 seam ranges and `git diff --check` pass.
The full test suite was not rerun.

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
DA06/DA0B still read the actual device buffer. DA3E PLY remains core-owned;
DA3F now admits an independent guarded RTS, including early-exit callers.

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

The zero-page and relative-branch phases of these HuC6280 instructions each
sample interrupts, but the CPU cannot service an interrupt between them.
A transcript must preserve device-event timing without inventing that boundary.

At `00:FA67`, the guarded native BBR0 now accepts events after the 12-cycle
zero-page phase. Earlier events retain the core instruction; canonical
coverage is 766/832. See the current tile-upload horizon evidence above.
The existing core paths at `00:F423`, `01:C6E3`, and `01:C756`, and the
outer `00:FAA5` row boundary, retain their own constraints.

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
`7071`, `7099`, and `70BB` now admit independent guarded live-stack returns.

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
the three ordinary RTS instructions at `71A9`, `71C4`, and `71E1` now admit independent guarded live-stack returns.

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
