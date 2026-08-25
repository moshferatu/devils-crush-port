# Natural-Route Screening Ledger

Every native replacement has to be admitted from a *natural* route: the game
plays itself from power-on through an input-only script, nothing patches
emulated work RAM, and the resulting scope has its own caller and its own
return. This file records which routes have already been run and what each one
reached, so a later slice does not pay for a screen that has already been done.

The screening rule, stated once, because every row below applies it:

> A route earns a replacement only if it executes an instruction address that
> no earlier route reached **and** that address heads a transaction with an
> independent caller-and-return contract. Reaching a new *interior* of a
> routine that is already owned by a scheduler, collision, allocator,
> appearance, or actor-controller transaction is branch coverage, not a seam.
> Splitting an existing owner around its child or return is explicitly not
> allowed — see [RETAINED_ASSEMBLY_SEAMS.md](RETAINED_ASSEMBLY_SEAMS.md).

Each route is a `--record-gameplay-<name>` flag on the application. They all
run the ordinary visible startup and then drive the pad only; the differences
are the launcher release frame and the flipper/nudge cadence. Deterministic
means the assembly, deployed Release, and deployed Debug runs produce a
byte-identical recording, instruction profile, native-cycle census, and
abandonment census. Artifact hashes for each run are in
[PROGRESS_ARCHIVE.md](PROGRESS_ARCHIVE.md).

## Screened routes

All routes below are deterministic across configurations unless noted, and
none of them admitted a replacement.

| Route | Cadence | What it reached | Verdict |
| --- | --- | --- | --- |
| `lanes` | 24-frame single holds on a 96-frame cycle, nudge bursts, 5 Button-II | Giant Door `03:6314` (returns via `01:C654`), Knight prefix `03:73D0`, `03:751E`, `03:7696`, `02:4D2D/4F75` | All owner interiors |
| `crossfire` | Overlapping holds, 32-frame cadence, 6 nudges | Giant-Skeleton Eyeball final animation `02:4F3D`, `02:4F6C-4F8D` | Already-bounded continuation |
| `orbit` | Separated 48-frame windows, rotating nudges | `02:515C/5161` (offsets of owned `02:5159` coroutine), `02:4F40/4F6C`, `03:6001/6013` | Mapped owners only |
| `alternate` | Long hold, 24-frame asymmetric cadence, 6 Button-II | Giant-Skeleton Eyeball `02:4F3D` (224 calls) incl. the single `02:4F76` transition | Confirms the documented continuation boundary |
| `sweep` | Asymmetric left/right cadence, 8 spaced Button-II | Active flipper `01:D4A7-D4B2`, `02:4F3D` | Re-enters the existing flipper owner / scheduler |
| `stagger` | 144-frame offset holds, broad release gaps | 3,626 addresses | Inside the lanes/sweep/state-census union |
| `pulse` | 4-frame taps on a 180-frame cadence, long neutral intervals | `02:51C3/51C5` (interior of the owned Devil-Shooter coroutine) plus 64 bank-`45` music rows | Coroutine interior |
| `rally` | 137-frame right-biased recovery, short left saves | Skeleton-Insect `02:54D0` (12 calls) and `02:5500` (187), all `01:C51D -> 01:C520` | Established countdown/idle paths |
| `slalom` | 211-frame wide left/right saves, opposite-side nudges | 3,375 addresses | Inside the prior union |
| `switchback` | Long hold, 233-frame alternating saves | 3,376 addresses | Inside the lanes/pulse/rally/stagger/sweep union |
| `pendulum` | 257-frame long travel, mirrored saves | 3,242 addresses | Inside the discovery union |
| `nudge` | 313-frame sparse saves, rebound-led | Remaining-ball/reload chain `01:C7E5/C812 -> 02:4003`, Giant-Skeleton and Devil-Shooter owners, Giant-Face crown hit `03:761E` | Documented owners |
| `spiral` | Earlier release, 359-frame mixed flipper/nudge | 3,920 addresses | Inside the discovery union |
| `ricochet` | 419-frame brief staggered saves | `02:5228-523C` — 202 36-cycle returns to `02:5245`, one scheduler path via `01:C5E9` | Branches of the owned `02:5223` controller |
| `volley` | 503-frame sustained alternating banks | 2,846 addresses | Inside the discovery union |
| `whirlpool` | 521-frame offset double-save windows | 2,852 addresses | Inside the discovery union |
| `cascade` | Release at frame 1,064, 607-frame neutral descent | `02:5228-523C` again | Already rejected by `ricochet` |
| `weave` | Delayed release, 613-frame alternating recovery | Dragon's-Egg dispatch `02:438A-438E` (1,757 calls to `01:C654`), collision-interpreter/banked-wrapper `03:6989-6994` | Neither group has its own caller and return |
| `higharc` | Hold through frame 1,460, 701-frame isolated saves | 208 additions, all mapped loader / Giant-Skeleton / Devil-Shooter / flipper / scheduler interiors | Also exercised the safe `00:E255 -> 00:E25F` MPR-mismatch fallback |
| `corner` | Release at frame 1,480, 743-frame right-first corner save | +72 rows: position retry/bounds `01:CDF8-CEF8` and continuation `01:D066-D080` | Existing collision-response ownership |
| `alley` | Hold through frame 1,580, 811-frame paired left recoveries | +1,127 rows: `02:4D2D` (96 calls, `01:C51D -> 01:C5E9`) and `03:670A` (102 calls, returns via `03:6965`) | Existing owners |
| `twist` | Release at frame 1,340, 887-frame long alternating saves | 9,236 addresses, 656 outside the union; natural Big-Star active path `02:5674/570A` | No standalone transaction, but see the fidelity note below |
| `sidestep` | Release at frame 1,275, 947-frame asymmetric side saves | `03:78CC-78F8` — one fixed-Bumper hit continuation, `01:C51D` in, `03:7C33` out | Scheduler-owned continuation |
| `rebound` | Release at frame 1,235, 1,009-frame long right rebound | 8,515 addresses | Strict subset of `alley` and `corner` |
| `bankshot` | Release at frame 1,310, 1,091-frame asymmetric bank saves | 8,242 addresses | Exactly the `sidestep` address set |
| `skew` | Release at frame 1,400, 1,201-frame wide left return | Scheduler-owned `01:D47B` continuation, one-write `01:D000` local continuation | Neither has an independent contract |
| `helix` | Release at frame 1,515, 1,237-frame right-first wide saves | 8,403 addresses | Strict subset of `alley` |
| `vector` | Release at frame 1,330, 1,271-frame staggered banks | 8,266 addresses | Strict subset of `alley` |
| `playfield` | Hold through frame 1,525, 1,277-frame wide saves | +383 rows: allocator/activation `02:41EC-42E0`, actor records `02:526A/5557-55D2`, controllers `03:6023/64A5/761E/7C43` | Existing scheduler / collision owners |
| `radial` | Release at frame 1,445, 1,301-frame long-left/right/short-left | 8,944 addresses | Strict subset of `alley`; one deterministic `00:E241 -> 00:E250` MPR-mismatch abandonment, reproduced in both configurations |

The built-in 1,800-frame gameplay plan was screened on the same terms: 509
Devil-Shooter `02:5159` arrivals and a three-arrival `02:5226-523C` launch
continuation that feeds the already-owned `02:5223` controller. No transaction.

### The `twist` fidelity result

`twist` is the one route that found a real defect rather than only coverage.
Its deployed recording diverged from assembly in **audio only** at frame 1,826,
with work RAM and framebuffer identical. Two separate timer-interrupt seams
were responsible, and both are now guarded by conservative horizons:

- `02:570A` (Big-Star idle) falls back to assembly whenever its 81-cycle body
  reaches a serviceable timer-IRQ horizon. 280 no-interrupt calls stay native;
  10 interrupt-bearing calls go to the core.
- `03:66A3` (Three-Blobs first-movement prefix) falls back on a timer horizon
  of 300 master cycles or less. 998 of 1,107 calls stay native.

With both horizons in place the fully deployed 3,600-frame `twist` recording is
byte-identical to assembly for audio, work RAM, and video. Keep `twist` as the
audio-fidelity probe for future device-timing work.

## What screening has established

The canonical 3,600-frame route and all 30 input-only variants above have
exhausted the table-rule heads that can be reached from a scripted pad stream.
The residue is documented device, block-transfer, compound-branch,
setup/scheduler, music, and caller-owned continuation seams. Adding another
scripted cadence in the same style is very unlikely to pay: the last several
routes each landed strictly inside the union of the earlier ones.

## Player-session capture, and why it replaced route scripting

The way past that horizon was to stop scripting and record a person playing.

- `--capture-session <dir>` runs the ordinary desktop or cabinet host and
  saves `initial.state` plus exact per-frame pad and plunger inputs, post-frame
  work-RAM SHA-256 fingerprints, the host mode, and the profile/census
  artifacts.
- `--replay-session <inputs.csv>` restores that state and replays the pad
  stream through the normal recording, registry, profile, and trace machinery.
  It halts at the first mismatching work-RAM fingerprint and refuses captures
  without the checkpoint or with physical-plunger movement, rather than
  producing an approximate replay.

`player-session-4` is the first capture that paid out. Its 6,605 frames replay
byte-identically and reach `00:E85D-E8B2`, a helper no scripted route had ever
entered — 665 natural calls over frames 3,614-4,542, every one with the
contract `01:C9D3 -> 00:E85D -> 00:E8A3`. Narrowing it gave the call-free,
eight-pass restoring **unsigned square root** at `00:E85D-E8A4`, which is now
registry-admitted with all 6,605 per-frame work-RAM hashes preserved.

`player-session-3` (a longer 59,248-frame v1 desktop capture) predates the
per-frame checkpoint, so it is broad route coverage only, not admission
evidence. Its leading residue is the already-classified TIA and compound-branch
seams, VDC acknowledgements, bank-`09` palette/score-panel fragments, bank-`45`
music fallbacks, and actor/collision branch interiors.

`player-session-5` is an 11,167-frame cabinet-mode v3 capture. Its default
registry replay preserves every work-RAM fingerprint and exactly reproduces its
7,778-address profile (`a8cf3988190362968f6e444dacfdb29d5e75fd41b5c6e4cbe57b395175a188e9`).
It newly exposes the short actor scan at `01:C539`, but its 943 outer calls
(942 from `01:C1D3`, one from `00:EEC1`) run through dynamic handlers and
queue filing before their `01:C573` RTS: 21,675--379,866 cycles and up to
1,734 writes per call. This is an existing composite actor owner, not a new
atomic routine; any future extension must retain the existing dispatch and
queue seams rather than replacing the whole scope.

On 2026-08-25, a source-clean current Release build no longer reproduced that
baseline: `player-session-5` first differed at frame 2,401 and the earlier
v3 `player-session-4` first differed at frame 144. They remain useful for
prioritization only.

`player-session-6` replaces that certification boundary. Its 31,971-frame
cabinet-mode v3 replay preserves every work-RAM fingerprint; the captured and
replayed instruction profiles are byte-identical with SHA-256
`26d754f8905c7bfbcedcad2e08a937078496c650093b8fc2c0fe44cc79b5635c`.
The default registry executes 7,218,542,577 of 11,477,133,123 master cycles
natively. Its focused `01:C539` trace has 1,744 calls (1,743 from `01:C1D3`,
one from `00:EEC1`), all returning through `01:C573`; together they span
24,228--369,078 cycles and 425--8,885 work-RAM writes per call. The 117,359,643
cycles and 2,379,778 writes confirm that this is still a composite actor loop,
not an atomic replacement candidate.

The next distinct high-cost owner, `01:C969`, is also not ready for direct
admission. Its 33,498 observed calls split into 7,301 returns at `01:C971`,
26,117 at `01:C9DE`, and 80 full returns at `01:CA83`; together they cover
18,540,300 master cycles and 517,416 writes. The active arm calls `00:E5EC`
twice, `00:E85D`, `00:E79C` twice, `00:E728`, and `00:E65C` three times before
the full exit. `E85D` and `E65C` are already native, but the others remain
assembly-backed. A narrow transcript through the guard and the `C9DE` range-out
return matched focused original calls at ordinals 6,176, 6,177, 30,155 and
30,186, yet the default deployed session first diverged at frame 4,888. The
prefix was rejected and reverted: preserve this parent/child schedule until
that cross-registry interaction is understood.

`05:6D27` is the first independently bounded session-6 residue to clear the
screen. Its 7,728 calls all arrive from `05:C565` and return through `05:C568`.
The 7,719 live orbit arms return at `05:6D7E` after exactly 450 master cycles
and ten ordered writes: phase `$3E00,X`, four zero-page coordinate temporaries,
and the four position bytes for the child. The remaining nine calls branch to
the enclosing `05:6D7F` controller and deliberately decline before staging a
write. The initial transcript charged zero-page instructions at nine master
cycles and diverged at the first live frame (10,711); the corrected 12-cycle
timing preserves every one of the 31,971 replay fingerprints, and
`dc_rom_bonus_stage4` passes in Release.

The next leaf, `00:E5BC`, is the eight-bit partial loop of the unsigned-word
multiply, not another caller-owned transaction. All 2,423 session-6 entries
come from `00:E5AF`, return through `00:E5B2`, and stop on the `00:E5EB` RTS.
It shifts the `$42:$43` accumulator and `$30` multiplier once per half-step,
then conditionally adds `$2D:$2E`; its uninterrupted body is exactly
600 + 72 x population-count(initial `$30`) master cycles. The checkpointed
replacement stages those 24 shift writes and up to 16 add writes at the
original instruction boundaries. Forced replacement covers all 2,423 natural
calls, focused replay verifies call 1 at frame 4,888 (744 cycles, 28 writes),
and the default 596-entry registry preserves every session-6 fingerprint while
contributing 2,000,280 native master cycles. The ROM-free semantic fixture
exercises all 256 multiplier patterns across a representative 16-bit operand
grid and refuses decimal/T mode.

`02:4FC4` is the side gate's bounded deadline comparison. All 15,108 session-6
entries arrive through the actor coroutine and take one of two no-write returns:
the ordinary `02:4FE1` RTS or the state-change hand-off at `02:4FE2`. The native
transcript preserves every branch and subtract flag through the three-byte
`$2205,Y - $00:13:88` comparison, including the two earlier exits at `$2254`
and `$2D52`. Its semantic fixture covers both deadline boundaries and decimal/T
refusal; the deployed 597-entry registry preserves all 31,971 session-6 hashes
and contributes 1,858,284 native master cycles.

**Practical consequence:** prefer a fresh player capture over another scripted
route when looking for the next admission, and capture it with the current
executable so the per-frame fingerprints exist.

## Adding a route

1. Add the `--record-gameplay-<name>` cadence; change controller bits only,
   never emulated work RAM.
2. Record it in assembly, deployed Release, and deployed Debug. All four
   artifacts must agree byte-for-byte before the screen means anything.
3. Diff its instruction profile against the union of the routes above. If it
   adds nothing, stop and record the row.
4. For each new address, run a focused assembly observer and read the caller
   and the return. Only an independent caller-and-return contract is a
   candidate; anything else is an interior and stays assembly-backed.
5. Add the row to the table above and note the artifact hashes in the slice's
   commit message.
