# Devil's Crush Windows Port

This repository is the beginning of a hybrid Windows port of the TurboGrafx-16 game *Devil's Crush*. PC Engine hardware remains emulated while gameplay logic is progressively replaced with native C++.

See [PLAN.md](PLAN.md) for the approved architecture and [PROGRESS.md](PROGRESS.md) for current implementation status.

## ROM requirement

The game is not included. Supply the supported USA ROM when running the host:

- Size: 393,216 bytes
- MD5: `92fbe89d4c87bf283dbfbeac69a64e6e`
- SHA-256: `9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9`

ROM files and locally derived assets are ignored by Git and must not be distributed with the project.

## Prerequisites

- Windows 10 or 11, x64
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.30 or newer
- Internet access during the first full configure, to fetch pinned dependencies

SDL is pinned to release 3.4.12 with a verified source-archive hash. The host
uses one fixed two-pass sharp pixel scaler: exact nearest-neighbor integer
enlargement followed by a controlled final resolve to the monitor-sized,
aspect-correct output rectangle.

## Build

Core and tests only, without downloading dependencies:

```powershell
cmake --preset windows-core-only
cmake --build --preset windows-core-debug
ctest --preset windows-core-debug
```

Full SDL host:

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Validate a ROM without opening a window:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --validate-rom "D:\path\to\Devil's Crush.pce"
```

Run the current host:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --rom "D:\path\to\Devil's Crush.pce"
```

Run the cabinet preview:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --cabinet --rom "D:\path\to\Devil's Crush.pce"
```

Supply and persist a backglass image from the command line:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --cabinet --backglass "D:\cabinet\backglass.webp" --rom "D:\path\to\Devil's Crush.pce"
```

Cabinet mode opens a portrait playfield, a non-focusable score DMD, and a
non-focusable backglass. Menus and bonus stages use the sharply scaled
emulator framebuffer. A live normal session switches the playfield to the
accepted 256-by-668 table background, while the DMD reads the active score
directly from emulated work RAM. PNG, JPEG, and WebP backglasses are
aspect-contained against black with the fixed sharp scaler; invalid or
missing images retain the black fallback. Press F12 to pause into display
assignment: 1/2/3 selects the playfield, DMD, or backglass; Left/Right cycles
its monitor; F toggles that role's fullscreen state; B opens the backglass
picker; Delete clears the image; and Enter, Escape, or F12 saves and returns.
Closing an auxiliary window does not end play; closing the playfield does. See
[`docs/CABINET_PREVIEW.md`](docs/CABINET_PREVIEW.md) for the switching and
state contracts.

Run a headless ten-frame emulator check:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --smoke-test --rom "D:\path\to\Devil's Crush.pce"
```

Exercise the three-window cabinet host for ten frames with automatic exit:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --cabinet-smoke-test --rom "D:\path\to\Devil's Crush.pce"
```

Generate the deterministic 600-frame Beetle reference used for independent
Mesen2 comparison:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --record-canonical portable-data\canonical-v1.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

The ROM-free CSV records the fixed input script and per-frame SHA-256 values
for the first 8 KiB of PCE work RAM, the compact framebuffer, and the
float-stereo audio batch, together with framebuffer geometry and audio timing.
Recordings and other traces remain local and are excluded from Git.

The separate 1,800-frame gameplay recording preserves that Milestone 1
golden artifact while deterministically selecting New Game, entering the
table, launching the ball, operating both flippers, and applying four nudges:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --record-gameplay portable-data\gameplay-v1.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Debug and Release reproduce gameplay SHA-256
`1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353`.
The optional `--canonical-work-ram`, `--canonical-framebuffer`, and
`--canonical-audio` raw exports can also accompany this recording.

The separate 3,600-frame state-census route holds the launcher longer and
alternates flipper inputs so the ball traverses the scrolling table instead
of remaining in the launch lane:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-v1.csv `
  --observe-hook 00:F43D `
  --routine-trace out\traces\state-census-camera-entries.csv `
  --routine-call-trace out\traces\state-census-camera-calls.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

This route covers 208 horizontal ball coordinates, reaches vertical
coordinates 241-684, and activates the original scrolling camera. The
`00:F43D` camera/visibility scope completes 232 times and produces 205,563
ordered writes. Its entry, call, and write artifacts are byte-identical in
Debug and Release. The frame CSV has configuration-specific visual goldens
after frame 2,147, but work RAM, audio, and all bounded routine artifacts
remain identical; exact hashes and the mapped camera fields are recorded in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The nested actor-slot activation boundary can be recorded independently:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-actors.csv `
  --observe-hook 00:F4FC `
  --routine-trace out\traces\state-census-actor-entries.csv `
  --routine-call-trace out\traces\state-census-actor-calls.csv `
  --routine-replay 00:F4FC@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The route completes 519 ordered sweeps of all 127 slots plus 14 immediate
post-allocation refreshes. The regression verifies occupancy, the signed
camera-relative vertical window, bit-7 activation results, persistent and
dynamic slot ranges, interrupt-bearing calls, and the first normalized
replay. Exact path counts and artifact hashes are in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

Actor layout and live dispatch ownership use a smaller global write census:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-actor-layout.csv `
  --work-ram-write-range 3000-307F `
  --work-ram-write-range 3F00-3FFF `
  --work-ram-write-trace out\traces\state-census-actor-layout-writes.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

The analyzer decodes 88 fixed ROM layout records, distinguishes 59
initializer identities from the shared actor type values, verifies ball and
paired flipper/control ownership, and follows allocation and dispatch changes
in dynamic slots `$62-$71`.

The normal-table scene layout can also be bounded as one cross-frame call:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-scene-layout.csv `
  --observe-hook 02:4088 `
  --routine-trace out\traces\state-census-scene-layout-entries.csv `
  --routine-call-trace out\traces\state-census-scene-layout-calls.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

The call processes all 88 fixed records across frames 871-872. Its regression
verifies the banked-call path, exact record-field writes, and all 2,816
parallel-array clears. Category-selective layout entries remain explicitly
marked as inactive on this route.

The active game-to-audio command boundary is a compact replayable scope:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-audio-command.csv `
  --observe-hook 00:ED18 `
  --routine-trace out\traces\state-census-audio-command-entries.csv `
  --routine-call-trace out\traces\state-census-audio-command-calls.csv `
  --routine-replay 00:ED18@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The route accepts 66 synchronous commands from 21 caller paths and reaches 16
distinct driver descriptors. The regression verifies the `$2A` command latch,
the banked `45:4006` export, caller-to-command mapping, descriptor lookup,
`$2E00-$2FFF` channel-state writes, restored MPRs, and the frame-7 normalized
replay. Absolute-cycle alignment with the actor identity trace ties gameplay
commands to ball slot `$00`, 10 fixed layout records, and three dynamic
state-machine families. Exact command populations and artifact hashes are in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The first manual-named fixed family is the lower-table Tower:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-tower-family.csv `
  --observe-hook 03:60C3 `
  --routine-trace out\traces\state-census-tower-family-entries.csv `
  --routine-call-trace out\traces\state-census-tower-family-calls.csv `
  --routine-replay 03:60C3@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slot `$2D` is the Tower at world position `(144,538)`. Its route-active
hit awards 50 stored units, emits command `$33`, and advances the Tower state.
Neighboring slot `$2C` is the manual's “insects living in Tower” source: it
spawns the four `03:6016` children and owns their 200-unit group-clear award.

The Tower Guard / entrance surface closes the same family:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_tower_guard$" --output-on-failure
```

Fixed slot `$2E` at `(132,510)` uses `03:60A6` to select closed completion
ID `$84` while the Tower's five-step counter `$2D4D` is nonzero, and `$8E`
when it reaches zero. All 2,650 routed updates remain closed. The ROM-bounded
open path selects bonus stage 5, command `$41`, and resets the Tower gate.

The mirrored lower-table bumpers form the next named fixed family:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-bumper-family.csv `
  --observe-hook 03:78C7 `
  --routine-trace out\traces\state-census-bumper-family-entries.csv `
  --routine-call-trace out\traces\state-census-bumper-family-calls.csv `
  --routine-replay 03:78C7@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slots `$14/$15` at `(190/74,604)` share a nine-phase hit dispatcher.
Each observed hit awards 10 stored units and emits command `$20`; terminal
destruction adds 200 and releases six type-`$83` children. The route records
11 hits but stops at phases 3 and 8, so the terminal behavior is ROM-bounded
and explicitly retained as an inactive path.

The two Front Spiders beneath the bumpers have a separate safety rule:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-front-spider.csv `
  --observe-hook 03:6490 `
  --routine-trace out\traces\state-census-front-spider-entries.csv `
  --routine-call-trace out\traces\state-census-front-spider-calls.csv `
  --routine-replay 03:6490@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slots `$26/$27` select left/right collision IDs and safety state. The
route records two right-side hits that set launcher-safety flag `$2D14` and
one left-side hit that sets trap-door flag `$2D13`; every hit awards 40
stored units, emits command `$34`, and runs the same appearance cycle.

The two outer Side Spiders are a distinct one-hit family:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_side_spiders$" --output-on-failure
```

Fixed slots `$28/$29` at `(40/240,642)` share dispatch `03:651F` and select
side-shoot collision IDs `$2C/$2D`. The route records 2,796 collision-free
updates, split equally by side. Exact ROM signatures retain their 10,000-point
use award, command `$34`, transition latch `$2D32`, and disappearance cycle.

Those safety flags also trigger the three fixed Skeleton Insects:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-skeleton-insects.csv `
  --observe-hook 02:54A3 --observe-hook 02:54B9 `
  --observe-hook 02:54D0 --observe-hook 02:5500 `
  --observe-hook 02:5543 --observe-hook 02:5551 `
  --observe-hook 02:5568 --observe-hook 02:5590 `
  --observe-hook 02:55D3 --observe-hook 02:55D6 `
  --observe-hook 02:55D9 --observe-hook 02:55F9 `
  --observe-hook 02:561A --observe-hook 02:5625 `
  --observe-hook 02:5630 `
  --routine-trace out\traces\state-census-skeleton-insects.entries.csv `
  --routine-call-trace out\traces\state-census-skeleton-insects.calls.csv `
  --routine-replay 02:54B9@1 `
  --routine-replay 02:5551@1 `
  --routine-replay 02:55F9@1 `
  --work-ram-write-range 2CFC-2CFC `
  --work-ram-write-range 2D13-2D14 `
  --work-ram-write-range 3000-307F `
  --work-ram-write-range 3F00-3FFF `
  --work-ram-write-trace out\traces\state-census-skeleton-insects.actors.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Slots `$31/$33` consume the left/right Front-Spider latches and each award
40 stored units. Slot `$32` consumes Tower latch `$2CFC` and awards 100.
The second right-side latch occurs after slot `$33` has changed state and
does not re-award. These target routines emit no audio command; the source
Spider or Tower owns the same-frame feedback.

The eight upper-table Sorcerers and their hidden controller are bounded from
deployment into their Big-Star orbit:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-sorcerers.csv `
  --observe-hook 02:578E --observe-hook 02:57A4 `
  --observe-hook 02:57A7 --observe-hook 02:57AD `
  --observe-hook 02:57F8 --observe-hook 02:5808 `
  --observe-hook 02:586F --observe-hook 02:5886 `
  --observe-hook 02:58C2 `
  --routine-trace out\traces\state-census-sorcerers.entries.csv `
  --routine-call-trace out\traces\state-census-sorcerers.calls.csv `
  --routine-replay 02:57AD@1 `
  --routine-replay 02:5886@1 `
  --routine-replay 02:58C2@1 `
  --work-ram-write-range 2D0B-2D10 `
  --work-ram-write-range 2D40-2D40 `
  --work-ram-write-range 3000-307F `
  --work-ram-write-range 3F00-3FFF `
  --work-ram-write-trace out\traces\state-census-sorcerers.actors.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Controller slot `$16` stages fixed slots `$17-$1E` at 44-frame intervals.
All eight reach orbit dispatch `02:58C2`, leaving live count `$2D0F=8` and
deployment count `$2D10=0`. The route does not hit them; exact ROM signatures
retain the two 10-unit hits, command `$29`, 50-unit destruction award, color
cycle, and group regeneration as inactive behavior.

The far-right ball service is now identified and bounded as the Devil
Shooter:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-devil-shooter.csv `
  --observe-hook 02:5116 `
  --observe-hook 02:5159 `
  --routine-trace out\traces\state-census-devil-shooter-entries.csv `
  --routine-call-trace out\traces\state-census-devil-shooter-calls.csv `
  --routine-replay 02:5116@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slot `$0E` catches the ball at frame 1,064, positions it at
X=`$010E`, and emits command `$25`. Button I then drives 96 charge updates
through frame 1,180; the first four emit `$3D`. Release produces velocity
`$F8` and command `$2B` at frame 1,185. The bounded service cycle makes no
score write.

The four presentation/control actors surrounding that service are bounded
separately:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_devil_shooter_companions$" --output-on-failure
```

Fixed slots `$0A-$0D` provide the Shooter's four-/five-frame session
animations, charge-driven vertical position, and three-frame charge
appearance. The regression covers all 5,231 routed calls and eight normalized
state samples; these companions own no score or audio rule.

The manual's Devil's Coffin / return-gate target is bounded separately:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_devils_coffin$" --output-on-failure
```

Fixed slot `$0F` at `(221,257)` uses initializer `03:63E1` and steady
dispatch `03:63F1`. The routed ball contacts it at frames 1,244 and 1,297;
each contact runs appearance `0→1→2→3→2→1→0`. Completion clears the
Shooter-linked session bit `$53.3`, changes type `$83` to `$82`, and returns
to steady state. The family owns no score or audio-command write.

Two lower-table major targets share command `$2E` but have different rules:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-dragon-skull.csv `
  --observe-hook 02:5A4B `
  --observe-hook 02:5EEC `
  --routine-trace out\traces\state-census-dragon-skull-entries.csv `
  --routine-call-trace out\traces\state-census-dragon-skull-calls.csv `
  --routine-replay 02:5A4B@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slot `$13` is the Dragon's Head. Two routed hits each award 50 stored
units, emit `$2E`, and advance its eight-hit opening counter from 8 to 6.
Fixed slot `$30` is the Giant Skeleton; its routed hit awards 40, emits the
same `$2E`, and leaves its repeatable dispatcher active. The analyzer also
guards the Dragon's inactive eighth-hit mouth-opening path.

The six Dragon's Eggs beneath Dragon's Head and their dynamic hatchling are
bounded as one source/child family:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-dragon-eggs.csv `
  --observe-hook 02:42EB `
  --observe-hook 02:42F6 `
  --observe-hook 02:433F `
  --observe-hook 02:4345 `
  --observe-hook 02:4379 `
  --observe-hook 02:441E `
  --routine-trace out\traces\state-census-dragon-eggs-entries.csv `
  --routine-call-trace out\traces\state-census-dragon-eggs-calls.csv `
  --routine-replay 02:42F6@1 `
  --routine-replay 02:4345@1 `
  --work-ram-write-range 2CF4-2CF7 `
  --work-ram-write-range 2CFB-2CFB `
  --work-ram-write-range 2D20-2D20 `
  --work-ram-write-range 3000-307F `
  --work-ram-write-range 3F00-3FFF `
  --work-ram-write-trace out\traces\state-census-dragon-eggs-state.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slots `$20-$25` increment a hidden slot-`$1F` controller count.
The routed slot-`$22` hit awards 10 stored units, selects impact command
`$39`, and creates a same-position hatchling at `02:441E`. The hatchling hit
four frames later awards 200 units, selects `$34`, and frees its slot. The
unreached all-six completion branch remains ROM-bounded.

The all-six completion consumer is bounded separately:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_giant_skeleton_eyeball$" --output-on-failure
```

Fixed slot `$2F` at `(225,535)` is the manual's Giant Skeleton's Eyeball,
positioned inside Giant Skeleton slot `$30`. Its steady dispatch `02:4DD9`
waits on `$2D20` bit 0, the signal set by the Dragon's-Egg controller when
all six eggs are gone. The standard route leaves that signal clear, producing
1,988 dormant calls; exact ROM signatures retain the linked mouth-opening
animation and reset states.

The lower-table 3 Mercy Stars are bounded as a last-chance family:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_mercy_stars$" --output-on-failure
```

Fixed slots `$5F-$61` at `(132/148/164,660)` share steady dispatch
`02:4FC1`. The route's three-ball state keeps all 7,950 updates dormant.
ROM signatures retain their exact last-ball and under-500,000-point gate,
1,000-point use award, table transition, and Three-Blobs safety activation.

The Three Blobs above the central face own the remaining fixed `$44`
producers:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-three-blobs.csv `
  --observe-hook 03:66A3 `
  --observe-hook 03:66D3 `
  --routine-trace out\traces\state-census-three-blobs-entries.csv `
  --routine-call-trace out\traces\state-census-three-blobs-calls.csv `
  --routine-replay 03:66A3@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slots `$58/$59/$5A` own global color states `$2D39-$2D3B`.
Six internal states select four visual IDs. The route records 16 scoreless
collision advances and command-`$44` attempts; two occur in frame 1,546 and
coalesce into one per-frame audio export. The all-state-4 result is
ROM-bounded but not activated by the route.

The adjacent fixed slot `$57` owns the equality/result controller:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-three-blobs-controller.csv `
  --observe-hook 03:65BC `
  --observe-hook 03:65C2 `
  --routine-trace out\traces\state-census-three-blobs-controller-entries.csv `
  --routine-call-trace out\traces\state-census-three-blobs-controller-calls.csv `
  --routine-replay 03:65C2@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The route records one initialization and 2,650 steady updates: 2,565 exit
after the first unequal pair, 35 after the third-value comparison, and 50
include periodic interrupts. It never enters an equal-state result branch;
the 100-unit all-red award, `$53.7` flag, and randomized reset remain guarded
by exact ROM signatures. The normalized mismatch sample is a 42-cycle,
zero-write leaf.

The seven Gray Blobs in the upper chute are statically bounded and linked to
the Three-Blobs family:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_gray_blobs$" --output-on-failure
```

Fixed slots `$50-$56` form the guide's seven-piece upper row and share
initializer `03:6876` and live dispatch `03:68A1`. The census initializes all
seven at frame 872 but never camera-dispatches the upper chute. Exact ROM
signatures retain the 64-95-tick RNG interval, four-tick appearance toggle,
`$03E8` score submission (100,000 displayed points), default command-`$2D`
impact burst, and one-frame `$2CFD` hit signal. The Three-Blobs movement code
consumes that signal and selects its alternate placement at `03:67EC`.

The shared type-`$8F` impact-burst family is bounded from its source calls
through allocation, one-shot activation, animation, and slot release:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-impact-burst.csv `
  --observe-hook 02:41EC `
  --observe-hook 02:41FF `
  --observe-hook 02:4290 `
  --routine-trace out\traces\state-census-impact-burst-entries.csv `
  --routine-call-trace out\traces\state-census-impact-burst-calls.csv `
  --routine-replay 02:4290@1 `
  --work-ram-write-range 3000-307F `
  --work-ram-write-range 3F00-3FFF `
  --work-ram-write-trace out\traces\state-census-impact-burst-actors.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Seven effects are spawned by Small Soldiers, Tower insects, Dragon's Egg
slot `$22`, and its `02:441E` hatchling. Each copies its source position,
emits exactly one source-selected `$2D`, `$34`, or `$39` command, follows
the ten-step `02:42B0` animation, and frees its dynamic slot. The effect
itself does not award score.

That first audio-owned dynamic family is bounded independently:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-two-hit-target.csv `
  --observe-hook 03:6016 `
  --routine-trace out\traces\state-census-two-hit-target-entries.csv `
  --routine-call-trace out\traces\state-census-two-hit-target-calls.csv `
  --routine-replay 03:6016@5 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The Tower-insect children are four dynamic type-`$82` actors using a two-hit
counter. Each hit awards 10 stored score units and emits audio command `$29`;
the second hit also awards 50, clears all 32 slot arrays, and frees the actor.
The route records 6,494 updates, seven hits, three completions, and one
deterministic no-hit replay.

The manual's ten Small Soldiers at `03:7F57` use a branching second-hit rule:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-branching-two-hit.csv `
  --observe-hook 03:7F57 `
  --routine-trace out\traces\state-census-branching-two-hit-entries.csv `
  --routine-call-trace out\traces\state-census-branching-two-hit-calls.csv `
  --routine-replay 03:7F57@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Five hits across ten actors each award 30 units and emit `$21`. A standard
second hit adds 50 and frees the actor, while subtype `$0F` enters holding
dispatch `7F81`. The regression covers 20,867 updates and the first normalized
no-hit replay.

The two Side Stars that generate those actors and the deferred holding-state
exit are bounded separately:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-small-soldier-sources.csv `
  --observe-hook 03:7D4D --observe-hook 03:7D6F `
  --observe-hook 03:7D85 --observe-hook 03:7DE9 `
  --observe-hook 03:7E0D --observe-hook 03:7E23 `
  --observe-hook 03:7E87 --observe-hook 03:7F81 `
  --routine-trace out\traces\state-census-small-soldier-sources.entries.csv `
  --routine-call-trace out\traces\state-census-small-soldier-sources.calls.csv `
  --routine-replay 03:7D85@1 --routine-replay 03:7E23@1 `
  --routine-replay 03:7E87@1 --routine-replay 03:7F81@1 `
  --work-ram-write-range 2D16-2D17 `
  --work-ram-write-range 3000-307F `
  --work-ram-write-range 3F00-3FFF `
  --work-ram-write-trace out\traces\state-census-small-soldier-sources.actors.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slots `$34/$35` spawn the lower-left and upper-right five-member
groups in five timed pairs. Counts `$2D16/$2D17` rise to 5 and each falls to
4 on the two observed completions. Slot `$69` holds for eight `03:7F81`
updates, then receives its deferred 50-unit award and is freed. The inactive
all-five source rule awards 200 units and regenerates the group.

The central Giant Face and its crown/mouth components are bounded as one
four-slot family:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-giant-face.csv `
  --observe-hook 03:7428 --observe-hook 03:742B `
  --observe-hook 03:742E --observe-hook 03:74D8 `
  --observe-hook 03:766C --observe-hook 03:767E `
  --observe-hook 03:76F7 --observe-hook 03:76FA `
  --observe-hook 03:7718 --observe-hook 03:7726 `
  --routine-trace out\traces\state-census-giant-face.entries.csv `
  --routine-call-trace out\traces\state-census-giant-face.calls.csv `
  --routine-replay 03:74D8@1 --routine-replay 03:767E@1 `
  --routine-replay 03:76FA@1 --routine-replay 03:7726@1 `
  --work-ram-write-range 2258-2258 `
  --work-ram-write-range 2D0C-2D0E `
  --work-ram-write-range 2D21-2D21 `
  --work-ram-write-range 3036-3039 `
  --work-ram-write-range 3836-3839 `
  --work-ram-write-range 3A36-3A39 `
  --work-ram-write-range 3AB6-3AB9 `
  --work-ram-write-range 3E36-3E39 `
  --work-ram-write-range 3EB6-3EB9 `
  --work-ram-write-range 3F36-3F39 `
  --work-ram-write-range 3FB6-3FB9 `
  --work-ram-write-trace out\traces\state-census-giant-face.state.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Slots `$36-$39` own the persisted form/request controller, hidden animator,
mouth, and crown collision surface. The route leaves form 0 inactive across
5,608 calls. ROM signatures retain the six transformation requests,
300-point crown/closed-mouth hit with command `$20`, and the open-mouth
10,000-point capture that emits `$41` and selects bonus stage 4.

The two helmet channels that issue the Giant Face wake request are captured
separately:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-giant-face-helmet.csv `
  --observe-hook 02:4A5C --observe-hook 02:4A64 `
  --observe-hook 02:4C78 --observe-hook 02:4C91 `
  --routine-trace out\traces\state-census-giant-face-helmet.entries.csv `
  --routine-call-trace out\traces\state-census-giant-face-helmet.calls.csv `
  --routine-replay 02:4C91@1 `
  --work-ram-write-range 2258-2258 `
  --work-ram-write-range 2D0C-2D0D `
  --work-ram-write-range 2D16-2D17 `
  --work-ram-write-range 302A-302B `
  --work-ram-write-range 30AA-30AB `
  --work-ram-write-range 382A-382B `
  --work-ram-write-range 392A-392B `
  --work-ram-write-range 39AA-39AB `
  --work-ram-write-range 3A2A-3A2B `
  --work-ram-write-range 3C2A-3C2B `
  --work-ram-write-range 3CAA-3CAB `
  --work-ram-write-range 3D2A-3D2B `
  --work-ram-write-range 3DAA-3DAB `
  --work-ram-write-range 3E2A-3E2B `
  --work-ram-write-range 3EAA-3EAB `
  --work-ram-write-range 3F2A-3F2B `
  --work-ram-write-range 3FAA-3FAB `
  --work-ram-write-trace out\traces\state-census-giant-face-helmet.state.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Fixed slots `$2A/$2B` are the left/right helmet openings. Their 1,748 routed
idle calls run matching timer/appearance cycles without a collision. The
ROM-bounded activation sets Giant Face wake bit 0, awards 300 points with
command `$20`, emits return command `$25`, and routes the ball through the
selected side.

The four upper-table bonus Stars are bounded as the manual's Wall Pockets:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_four_star_wall_pockets$" --output-on-failure
```

Fixed slots `$3E/$3F/$42/$43` map the lower-left, lower-right, upper-left,
and upper-right pockets to BS-1/5/3/6 and Big-Star selectors VI/IV/VIII/II.
The route records 1,008 collision-free `02:4A93` calls. Exact ROM signatures
retain the normal 1,000-point access that advances the eight-state Sorcerer
color and the stopped-arrow match that awards 10,000 points and selects the
pocket's bonus stage.

The hands guarding the upper two Wall Pockets are bounded separately:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_upper_wall_pocket_hands$" --output-on-failure
```

Mirrored slots `$40/$41` at `(72,55)/(232,55)` sit directly above upper
pockets `$42/$43`. Shared byte `$2D33` gates their delayed type-`$83`
collision surface and two-step open/close cycle. The census initializes both
hands at frame 872 but never camera-dispatches live state `02:5975`; exact ROM
signatures retain the collision wait and random ball response. The hand
family owns no score or audio command.

The Little Skulls, Big Star, Side Worms, and Undead Head now form one bounded
eight-direction feature network:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_big_star_feature_network$" --output-on-failure
```

Fixed slots `$46/$47` and `$49/$4A` are the two Little-Skull pairs;
`$45/$48` control pair counts and regeneration, while central slot `$4B`
owns the Big Star. A Little-Skull hit awards 2,000 points, creates the default
command-`$2D` impact, and feeds direction bits into the Big Star's signed
speed states `+64,+32,+16,0,-16,-32,-64`. Its zero-based selector covers all
eight Roman numerals: I is Undead Head slot `$4D`, III/VII are Side Worms
`$4F/$4E`, II/IV/VI/VIII are the four Wall Pockets, and V is the special
Warlock condition. The census initializes all eleven records but leaves the
feature actors camera-inactive; only 2,650 clear-latch calls are routed.

The final hidden normal-table records are bounded as compatibility and
presentation controllers:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_fixed_system_controllers$" --output-on-failure
```

Slots `$5C-$5E` install steady dispatches `03:7B27`, `03:7B7A`, and
`03:7CAF`. Slots `$5C/$5D` share an eight-frame cadence: the first submits
three selectors through the common banked-call wrapper, while the second
renders three markers through phase table `0,1,2,3,4,3,2,1`. Slot `$5E`
owns a collision/scene-response branch, but that branch remains dormant on
the census route and shared byte `$2D38` stays zero. This closes the
gameplay-facing or compatibility-role reconciliation of all 88 fixed layout
records without assigning unsupported table-mechanism names to hidden system
glue.

The upper-left Giant Door (the detailed guide's Wooden Gate) is bounded
independently:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_giant_door$" --output-on-failure
```

Fixed slot `$4C` at world position `(64,254)` initializes a three-hit counter
and steady dispatch `03:62B8`. The standard route records 676 no-hit calls,
including seven interrupt-bearing samples. Exact ROM signatures retain the
5,000-point hit award, 50,000-point third-hit completion award, and the
four-channel post-destruction handshake.

The four Knights waiting behind that gate are bounded as the downstream
family:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_knights$" --output-on-failure
```

Fixed slots `$3A-$3D` index gate channels `$2D06-$2D09` and share steady
dispatch `03:7378`. All 10,600 routed calls remain gate-blocked. Exact ROM
signatures retain the four-hit rule, command `$26`, 5,000-point nonterminal
hits, 10,000-point final hit/destruction result, and indexed acknowledgment
back to the Giant Door controller.

The upper-left Walled Star is also bounded:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_walled_star$" --output-on-failure
```

Fixed slot `$5B` at `(32,302)` uses steady dispatch `03:6339`. The route
records 2,650 no-hit calls. ROM signatures retain four 5,000-point wall hits
with command `$2E`, a fifth-collision 20,000-point terminal award, and the
post-destruction access/reset states; the complete wall lifecycle therefore
adds 40,000 points.

The access pocket exposed behind that wall is bounded independently:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_walled_star_access$" --output-on-failure
```

Fixed slot `$11` at `(38,308)` uses steady dispatch `02:4B3F`. Its 905 routed
calls remain unhit. Exact ROM signatures retain the 10,000-point award, the
`$2DBC/$2D53` handshake with the outer wall, and RNG selection of one bonus
stage from 1 through 6.

The two Screen-2 Side-Star access surfaces are bounded separately from their
Small-Soldier source controllers:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_side_star_access$" --output-on-failure
```

Fixed slots `$10/$12` at `(38,372)/(238,308)` use steady dispatches
`02:4BD5/4C0A`. The route records 1,277/905 unhit calls and normalized
replays for both shared idle paths. Exact ROM signatures retain their
8,000-point awards, Giant-Face transformation request, and the upper-right
bonus-stage-6 entrance.

BS-1 and BS-3 through BS-6 now have natural perfect-outcome regressions;
BS-2 has the equivalent full-contact proof through its collision scan and
all 28 board cells:

```powershell
ctest --test-dir out\build\windows-vs2022 -C Release `
  -R "^dc_rom_bonus_stage(1|3|4|5|6)_natural_perfect$|^dc_rom_bonus_stage2_full_contact_perfect$" `
  --output-on-failure
```

Genuine terminal objective contacts reduce each stage-owned count to zero.
The original controllers then run their full completion animations, write
the perfect latch themselves, transfer the earned units, and build and
transfer the stage-specific special awards. The regressions do not seed the
objective count, accumulated score, or perfect latch. Exact ordering and
artifact hashes are documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The same route reaches a compact score-accumulation boundary:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-score.csv `
  --observe-hook 00:F55C `
  --routine-trace out\traces\state-census-score-entries.csv `
  --routine-call-trace out\traces\state-census-score-calls.csv `
  --routine-replay 00:F55C@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The 40 calls add unsigned 16-bit awards to a selected little-endian 24-bit
score and produce a final stored value of 1,280. The regression verifies the
direct write order, award set, 9,999,999 stored-unit saturation constant, and
the first call's normalized replay. See
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md) for the field layout and exact
trace hashes.

The RNG boundary uses the same route and additionally traces its free-running
seed:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-rng.csv `
  --observe-hook 00:E8B3 `
  --routine-trace out\traces\state-census-rng-entries.csv `
  --routine-call-trace out\traces\state-census-rng-calls.csv `
  --routine-replay 00:E8B3@1 `
  --work-ram-write-range 222A-222B `
  --work-ram-write-trace out\traces\state-census-rng-state.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

The regression verifies the once-per-frame seed tick, all 78 exact RNG
updates, both active actor-initialization consumers, and the first normalized
replay. It also performs an exact cartridge-wide census of all 21
`JSR C081` consumers: 12 normal-table sites and nine bonus-stage sites. Their
immediate contracts comprise 12 masked timer/offset transforms, six binary
selectors, and three multi-way selectors. Nineteen remain dynamically
inactive, but their call sites and output domains are now ROM-guarded.
The recurrence, consumer table, and artifact hashes are documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

Milestone 2 adds a HuC6280 routine-entry registry keyed by physical 8 KiB
bank and logical PC. Pass-through mode verifies that known canonical entries
are reached while generating the same golden recording:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --record-canonical portable-data\canonical-hooked.csv `
  --pass-through-hook 00:E462 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Observe mode atomically writes a deterministic routine-entry CSV containing
the frame, core master-cycle timestamp, A/X/Y/SP/status, and all eight MPRs:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --record-canonical portable-data\canonical-observed.csv `
  --observe-hook 00:E462 `
  --routine-trace out\traces\routine-00-E462.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

The selected startup probe produces 11,403 entries in frames 1–7. Debug and
Release reproduce trace SHA-256
`585b2221987701f24b9f6a271d48a7bedec3ce01fb08f3889d196cfe1a3191a4`
while retaining the canonical RAM/video/audio hash.

Replacement mode is now available for explicitly mapped, ordinal-gated
no-interrupt calls. The first native game-logic routine is the C
implementation of collision rollback `00:F241`. This command replaces its
first gameplay call, observes the full physics set, and restores that same
pre-call savestate to compare the native result with assembly:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-gameplay portable-data\native-replacement.csv `
  --observe-hook 00:F1B4 `
  --observe-hook 00:F241 `
  --observe-hook 01:D1A2 `
  --observe-hook 01:DD09 `
  --replace-hook 00:F241@1 `
  --routine-trace out\traces\native-replacement-entries.csv `
  --routine-call-trace out\traces\native-replacement-calls.csv `
  --routine-replay 00:F241@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The native transaction declares final registers, six ordered writes and
their entry-relative timing, 162 body master cycles, and the outer RTS.
Gameplay, entry, call, and write artifacts remain byte-identical to the
assembly baseline. Interrupt-bearing call variants remain in assembly until
the bridge gains faithful mid-routine interrupt delivery.

Repeat either hook option to register multiple distinct addresses. A
multi-address observe run writes the v2 trace format, preserving global
instruction order across all selected routines:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --record-canonical portable-data\canonical-mapped.csv `
  --observe-hook 00:E195 `
  --observe-hook 08:5977 `
  --observe-hook 08:59B5 `
  --observe-hook 08:59CE `
  --observe-hook 08:59E7 `
  --routine-trace out\traces\canonical-routines.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

The current evidence and confidence levels for these addresses are recorded
in [`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The gameplay route can be observed with the same registry:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --record-gameplay portable-data\gameplay-mapped.csv `
  --observe-hook 00:E195 `
  --observe-hook 01:C3CB `
  --observe-hook 01:C49E `
  --observe-hook 03:6966 `
  --routine-trace out\traces\gameplay-routines.csv `
  --routine-call-trace out\traces\gameplay-calls.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

This produces 23,167 globally ordered entries and trace SHA-256
`d66b6fd13beba8a36370a7508bad968f72570bfad7d947b8442376279bf928b0`
without changing the gameplay recording. The bounded call trace records the
caller and return path, input/output registers, absolute and frame-local
master clocks, and body cycle budget for all 23,167 completed calls. Its
companion `.writes.csv` records all 235,173 ordered in-scope work-RAM writes,
including nested callees and interrupt handlers. Debug and Release reproduce
call SHA-256
`920fb4f8132b58ded27e65eb0a4b0c88def481fcb5f29286f8029f3594057b38`
and write SHA-256
`84c398603f50b279ad670eed0d06d887006f085aa1d5aa7d4a8de4ad8178264c`.

The independent Mesen gameplay oracle is
`tools\mesen2\gameplay_routines.lua`. It records the same four entries,
derives each caller from the HuC6280 stack, brackets the mapped outer return
sites, captures output state and ordered writes, and exports frame-boundary
work RAM. Capture both cores twice and run the cross-core checks in one
isolated operation with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Capture-GameplayRoutines.ps1 `
  -MesenPath "D:\tools\Mesen-2.1.1\Mesen.exe" `
  -AppPath out\build\windows-vs2022\Release\devils-crush.exe `
  -RomPath "D:\path\to\Devil's Crush.pce"
```

To recheck existing Beetle and Mesen traces without recapturing them:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Compare-GameplayRoutines.ps1 `
  -BeetleTracePath out\traces\beetle-gameplay-routines.csv `
  -MesenTracePath out\traces\mesen-gameplay-routines.csv `
  -BeetleCallPath out\traces\beetle-gameplay-calls.csv `
  -BeetleWritePath out\traces\beetle-gameplay-calls.csv.writes.csv `
  -MesenCallPath out\traces\mesen-gameplay-calls.csv `
  -MesenWritePath out\traces\mesen-gameplay-writes.csv `
  -RomPath "D:\path\to\Devil's Crush.pce"
```

The same launcher has a `physics` profile for the four first motion
candidates:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Capture-GameplayRoutines.ps1 `
  -MesenPath "D:\tools\Mesen-2.1.1\Mesen.exe" `
  -AppPath out\build\windows-vs2022\Release\devils-crush.exe `
  -RomPath "D:\path\to\Devil's Crush.pce" `
  -Profile physics `
  -OutputDirectory out\traces\mesen2-physics
```

It performs two isolated Mesen captures, verifies all four artifacts are
repeatable, and runs a caller-context cross-core comparison. The route produces
549 byte-semantic exact leaf scopes and 259 leaf scopes whose pre-call RAM
already differs because the cores schedule actor motion in different output
frames. See [`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md) for the exact
classification and hashes.

The apparent Mesen `+1/+1/+26` entry deltas are one frame of table-activation
phase, not additional gameplay calls: after the shared four-call bootstrap,
Beetle frame `n` matches Mesen frame `n-1` exactly in per-frame call count,
A/X/Y/SP, and stable MPRs. See
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md) for callers and mapped state
fields. Across the 21,368 aligned session/collision calls, 18,075 bounded
scopes match exactly in cycle budget and ordered routine-owned writes. The
remaining 3,293 move a periodic interrupt between neighboring scopes; after
separating the interrupt stack/handler writes, their return paths, outputs,
and routine-owned write streams also match.

Candidate discovery can selectively record writes to one or more logical
work-RAM address ranges. The trace includes the frame and absolute cycle,
executing physical bank/logical PC, logical write address, physical 8 KiB
work-RAM offset, and value:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-gameplay portable-data\gameplay-census.csv `
  --work-ram-write-range 0000-007F `
  --work-ram-write-range 2C00-2FFF `
  --work-ram-write-range 3000-3FFF `
  --work-ram-write-trace out\traces\gameplay-write-census.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Recordings can also seed explicit work-RAM bytes immediately before a chosen
frame. This is a diagnostic mutation boundary for reaching otherwise dormant
original-ROM transitions; it is separate from CPU writes and every seed is
embedded in the recording header:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-seeded.csv `
  --work-ram-patch 3600:3FFF:A5 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The runtime imports only an exact-size 8 KiB copy. The CLI accepts logical
addresses `$2000-$3FFF`, rejects frame 0, frames outside the selected
recording, and duplicate frame/address locations, and leaves unseeded
recordings byte-identical. `dc_rom_work_ram_patch` guards the first seeded
artifact in Debug and Release.

The first physics census identified four active routine boundaries. Their
combined bounded capture is:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-gameplay portable-data\gameplay-physics.csv `
  --observe-hook 00:F1B4 `
  --observe-hook 00:F241 `
  --observe-hook 01:D1A2 `
  --observe-hook 01:DD09 `
  --routine-trace out\traces\gameplay-physics-entries.csv `
  --routine-call-trace out\traces\gameplay-physics-calls.csv `
  --routine-replay 00:F1B4@17 `
  --routine-replay 00:F241@1 `
  --routine-replay 01:D1A2@1 `
  --routine-replay 01:DD09@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

Debug and Release produce 2,717 entries/calls and 171,993 scoped writes with
byte-identical trace hashes documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md). Each `--routine-replay`
argument selects a one-based entry ordinal. The recorder captures an opaque
pre-instruction Beetle savestate and work-RAM image, records the original
bounded call, restores the checkpoint, and requires the replayed registers,
caller, return, body-cycle budget, ordered writes, relative write timing, and
post-call work RAM to agree. Beetle rebases the frame-local timestamp when a
mid-frame savestate resumes, so only the clock origin is normalized.

The physics regression also runs
`tools\Analyze-PhysicsSemantics.ps1`. It verifies the defining ROM instruction
signatures, confirms that all recorded calls address indexed slot `X=$00`,
checks `F241`'s six routine-owned rollback writes, and guards the signed
16.8/8.8 field interpretation documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The Milestone 2 controls-and-physics closure regression reuses the longer
state-census route and combines `02:5159`, `01:D466`, `01:C3CB`, `00:F1B4`,
and `01:DD09` in one ordered trace. It follows Button-I charge through the
Devil Shooter's `$3580=$F8` launch and subsequent integration, verifies
right/left flipper state transitions, follows three Button-II nudges into
slot-0 motion and later integration, and confirms that tilt lock `$2CC7`
remains clear for all 3,600 frames. The exact route is documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The Milestone 2 collision closure regression follows two natural contacts
with Small-Soldier slot `$62`. It connects confirmed-contact dispatcher
`01:CBCF` and the type-`$82` ball response to `03:6966`, `03:7FB1`, the
actor's two-hit counter, both score awards, command `$21`, group counter
`$2D16`, and complete actor release. The combined Debug/Release regression is
`dc_rom_collision_boundary`; its exact ordered route is documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The session-and-scoring closure regression follows one ordinary drain from
slot 0's signed Y threshold through the `$2D52` live-ball decrement,
remaining-ball decision, category-1 table reload, and score-panel redraw.
It reconstructs stored score 1,020 from 33 accumulator calls, proves the
panel displays stored score times 100, and observes the remaining-ball digits
change from 02 to 01 without a multiplier, extra-ball grant, alternate
transition, or score mutation. The same display boundary corrects the
saturated visible score to 999,999,900. The combined Debug/Release regression
is `dc_rom_session_scoring`; its exact ordered route is documented in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

Milestone 2 is scope-frozen by
[`docs/COVERAGE_MANIFEST.md`](docs/COVERAGE_MANIFEST.md). Its 105 unique
behavior units classify the complete USA-manual software inventory and the
additional cartridge/bridge requirements as dynamically verified, statically
ROM-mapped, or explicitly deferred. `dc_coverage_manifest` guards the IDs,
classification totals, manual completeness audit, and registered evidence
tests.

Milestone 3 begins with a copied, read-only video boundary. The runtime can
export all 32,768 HuC6270 VRAM words, all 512 raw VCE palette entries, the
memory-width register, and background scroll registers without retaining
emulator-owned pointers. The core rendering module decodes BAT entries,
8-by-8 four-plane tiles, shared background color zero, and native 9-bit PCE
colors; it also composes decoded regions in signed world coordinates and
extracts exact camera crops.
The current format findings and stream-boundary work are tracked in
[`docs/TABLE_BACKGROUND.md`](docs/TABLE_BACKGROUND.md).

`dc_rom_video_snapshot` drives the established 3,600-frame table route through
eleven live decode checkpoints. Its first table checkpoint guards separate VRAM,
palette, and decoded-background SHA-256 goldens, while the ordinary core test
locks planar bit order, subpalette behavior, dormant BAT cells, region seams,
negative world origins, and crop bounds. Release-only
`dc_rom_video_row_writer` identifies `00:F400` as the streamed 32-cell
BAT-row writer and guards all 539 calls, their four callers, exact VDC
setup/data-port counts, return site, and frame range.
The four callers are now classified as upward/downward one- or two-row
stream pairs. Their dynamic coordinate contract resolves physical ROM bank
`$14` tile-map words and packed work-RAM palette nibbles across a
32-by-96-cell source envelope. The first full-source export used the
frame-900 fade-in palette and incorrectly treated all 96 rows as visible
table data. It remains a deterministic decoder diagnostic, not a completed
playfield. Stable post-fade captures now establish the accepted 256-by-668
background at source rows 20-687: rows 0-19 are a top guard, 688-720 are
streamed lower prefetch, and 721-767 are unused tail data. Representative
upper, middle, and lower crops are independently checked against Beetle's
final RGB565 framebuffer; exact background pixels match while the remaining
pixels retain sprite and presentation overlays. The copied video boundary now
also includes the active internal HuC6270 SAT. Cabinet mode decodes its
patterns, palettes, sizes, flips, and sprite-to-sprite priority into table
world coordinates for the live camera viewport. The packed background decoder
retains raw pixel opacity so behind-background sprite priority is exact, and
the renderer enforces the 16-sprite-half scanline limit while moving
score/status presentation to the DMD. Typed work-RAM views now decode all 127
actor slots. Fixed mechanisms now resolve exact bank-8 ROM descriptor groups
instead of proximity-associating nearby SAT entries and continue rendering
from live work-RAM positions after the camera moves away without advancing a
second simulation. Retained visuals own decoded pattern pixels, so later
camera-region VRAM streaming cannot corrupt their geometry; stored VCE
palette indices resolve against the current palette so fade-in colors settle
normally. Actor Y is normalized past the 30-line score/status band before
entering table space. The retained
layer is drawn first, camera-observed actors are omitted from it, and the
copied live SAT is drawn last. This keeps inactive mechanisms inside the
camera band visible while preserving the SAT as the live oracle. Animated
actors retain bounded exact phase variants: an off-camera enemy either
resolves its current ROM-owned phase or restores the learned visual for that
same phase. It never moves a frozen fallback image, and another region's
VRAM is never sampled as a replacement. Static mechanisms may retain a prior
aligned visual only when their phase has no independent artwork. Bank `$08`'s
original actor renderer and descriptor tables provide exact world geometry
for unseen fixed mechanisms, with all 23
eligible frame-1,200 groups matching the copied SAT. Deterministic first-visit
seeding now has a native `00:E41F` compressed-stream decoder that reproduces
the complete frame-1,200-to-1,600 pattern transition. Observed selector
`00:F606` now has a complete local appearance/phase atlas: its 42 entries
share eight upload descriptors containing 35 phase assets plus one
resident-pattern sentinel for appearances `$24-$29`. The decoder also covers
the selector's bounded raw-word branch. Stream-backed first-visit seeding is
now active in cabinet mode. It keeps loader destination `$4F:$50` separate
from SAT pattern base `$51:$52`, requires the selected upload to own every
pattern block used by the bank-8 descriptor, and keys retained visuals by
animation phase as well as actor identity. At settled lower frame 1,200,
eight fixed groups use selected streams and 53 are proven disjoint from
every known upload. The 23 live-camera groups plus 38 pre-seeded groups
therefore cover all 61 non-hidden fixed actors and render 10,980 retained
priority-filtered pixels. Lower, upper-middle, and middle full-table
composites have deterministic animation goldens. All five `$F768` choices decode into 1,008-word
scene packs at `$7C00`; the normal-table initializer selects
`A=0`, `$1222->$7C00`, and the decoded words equal live VRAM. An
independent classifier shows that pack owns none of the 24 resident or
out-of-selector fixed groups, and that every such group is disjoint from all
known upload ranges. Their static live-VRAM blocks are therefore safely
pre-seeded as well. `$F768` selectors 1-4 remain scene-unmapped but do not
block normal-table fixed-actor composition.

Cabinet work now prioritizes a fully populated, animated stitched playfield.
The ball remains exact and trail-free through the live SAT, while all six
flippers are retained at their absolute table-world positions with live
phase changes. Dynamic slots `$62-$7E` now retain the ten Small Soldiers and
four Tower insects across camera movement with live animation metadata.
Only those persistent type-`$82` families are eligible: empty slots or reuse
by an impact/effect family clear the cached visual immediately. Lower,
upper-middle, and middle full-table goldens guard the resulting population
and animation. The separately mapped type-`$8F` impact burst persists for its
original table-world lifetime without admitting other transient families.
Camera-transition regressions keep all six flippers resolved and prove the
eight upper Sorcerers continue moving after their region leaves the camera
while presenting all 16 exact walking phases. They also prove the stitched
background's best emulator match remains at a zero-pixel vertical offset
through launch and camera movement.
Two-player/current-player mapping and P1/P2 DMD presentation are currently
deferred at the user's request.

The pinned upstream Beetle source is reproducibly modified by the ordered
project-owned patch series documented in
[`third_party/beetle-pce-fork/MANIFEST.md`](third_party/beetle-pce-fork/MANIFEST.md).

For byte-level oracle comparison, add
`--canonical-work-ram out\traces\beetle-wram.bin`. The optional binary is
exactly 600 consecutive 8 KiB work-RAM snapshots in frame order. The
optional
`--canonical-framebuffer out\traces\beetle-framebuffer.rgb` records a
16-byte `DCF1` geometry header followed by 600 RGB888 frames.
`--canonical-audio out\traces\beetle-audio.wav` records the same run as
44.1 kHz PCM16 stereo. The checked-in
Mesen2 comparison launcher performs two isolated Mesen captures, strictly
normalizes its backup-RAM header to Beetle's initialization policy, disables
frame skipping, and records work RAM, memory-write events, CPU frame
boundaries, and RGB888 output. It classifies remaining RAM observations,
normalizes Mesen's uncropped framebuffer to the host output, checks oracle
repeatability, and produces a JSON difference report:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Compare-Mesen2.ps1 `
  -MesenPath "D:\tools\Mesen-2.1.1\Mesen.exe" `
  -AppPath out\build\windows-vs2022\Debug\devils-crush.exe `
  -RomPath "D:\path\to\Devil's Crush.pce"
```

The launcher copies Mesen into the ignored output directory before creating
its isolated settings and save-data home. Generated trace artifacts remain
ignored. See
[docs/MESEN2_PARITY.md](docs/MESEN2_PARITY.md) for the pinned oracle,
latest comparison results, and normalization rationale.

Mesen's native core has a WAV recorder that its headless command line does
not expose. `tools\mesen2\Build-AudioRunner.ps1` applies a managed-runner-only
patch to the official 2.1.1 source and verifies that `MesenCore.dll` remains
unchanged. `tools\mesen2\Compare-MesenAudio.ps1` then records two independent
Mesen WAVs and reports duration, alignment, envelope correlation, and oracle
repeatability against the host's canonical WAV. Exact commands and current
results are in [docs/MESEN2_PARITY.md](docs/MESEN2_PARITY.md).

Exercise backup-RAM persistence across a complete Beetle game reload without
touching the normal `devils-crush.sav`:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe `
  --backup-ram-test portable-data\backup-ram-test.sav `
  --rom "D:\path\to\Devil's Crush.pce"
```

This writes a deterministic 2 KiB test pattern atomically, unloads and reloads
the ROM, imports the file, and verifies every byte. The opt-in `rom_required`
CTest label runs the smoke test, canonical golden recording, and this reload
test in both Debug and Release configurations.

The original game's save and Data Load screens have also been validated
visibly across two complete Release-host processes. See
[docs/BACKUP_RAM_VALIDATION.md](docs/BACKUP_RAM_VALIDATION.md) for the
procedure and result.

The current host boots the verified ROM through the pinned Beetle PCE core,
plays its float-stereo audio, accepts keyboard or the first available SDL
gamepad, and presents desktop or cabinet output through the fixed sharp scaler.
Gamepads can be connected, disconnected, or replaced while the host is
running. Optional SDL-format mappings are loaded from
`portable-data/gamecontrollerdb.txt`; F10 reloads the file after edits. F11
toggles playfield borderless fullscreen; F12 opens cabinet display assignment;
Escape exits outside that assignment screen. F5 atomically writes a versioned
quick state to `portable-data/devils-crush.state`, and F9 restores it after
checking the file signature, checksum, ROM identity, and pinned core revision.
The game's 2 KiB battery-backed RAM is loaded from and atomically saved to
`portable-data/devils-crush.sav`.

Quick states intentionally bind to this exact ROM and Beetle revision rather
than attempting unsafe cross-version migration. Beetle restores emulated PSG
state and audio timing but not its frontend resampler history, so the host
clears already queued audio when loading a state.

Audio remains synchronized to emulation rather than monitor refresh. The host
limits stale queued audio to 150 ms, drops an overrun after a long stall, and
reopens the default playback stream after device loss or format changes
without stopping emulation.

## Release staging check

Create and verify the current ROM-free Release layout:

```powershell
cmake --install out/build/windows-vs2022 `
  --config Release `
  --prefix out/distribution/devils-crush
cmake `
  -DDISTRIBUTION_DIR="$PWD/out/distribution/devils-crush" `
  -P cmake/VerifyDistribution.cmake
```

The verifier requires exactly `devils-crush.exe`, `LICENSE`, `README.md`, and
`THIRD_PARTY_NOTICES.md`, and independently rejects the supported ROM hash.
CI performs this check for every Release build. This staging layout is not yet
the final source-complete public package.

ROM-required CTest coverage is local-only and disabled by default:

```powershell
cmake --preset windows-vs2022 `
  -DDC_ENABLE_ROM_TESTS=ON `
  -DDC_ROM_PATH="D:\path\to\Devil's Crush.pce"
ctest --test-dir out/build/windows-vs2022 -C Debug -L rom_required
```
