# Retained Assembly Seams

## Gray-Blob live update: 03:68BF (cabinet ranking)

The user's September 9 cabinet report is now the primary porting priority.
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

The next target from the supplied ranking is the side-worm waiting update at
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
