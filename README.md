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
  --replace-hook 00:F4FC@all `
  --routine-trace out\traces\state-census-actor-entries.csv `
  --routine-call-trace out\traces\state-census-actor-calls.csv `
  --routine-replay 00:F4FC@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The route completes 519 ordered sweeps of all 127 slots plus 14 immediate
post-allocation refreshes. The regression verifies occupancy, the signed
camera-relative vertical window, bit-7 activation results, persistent and
dynamic slot ranges, interrupt-bearing calls, and the first normalized
replay. The semantic replacement admits all 65,927 calls through
2/18/31-instruction empty, below-window, and upper-window transcripts,
including 4,858 original interrupt-handler passages. Exact path counts and
artifact hashes are in
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md).

The adjacent actor lifecycle reset can also run natively with
`--observe-hook 01:C574 --replace-hook 01:C574@all`. One generated loop
transcript clears all 32 parallel fields for any legal slot and replaces all
132 state-census calls, including 35 interrupt-bearing calls. Debug and
Release preserve the complete 132-call/14,163-write artifacts.

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
  --replace-hook 00:F55C@all `
  --routine-trace out\traces\state-census-score-entries.csv `
  --routine-call-trace out\traces\state-census-score-calls.csv `
  --routine-replay 00:F55C@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The 40 calls add unsigned 16-bit awards to a selected little-endian 24-bit
score and produce a final stored value of 1,280. The regression verifies the
direct write order, award set, 9,999,999 stored-unit saturation constant, and
the first call's normalized replay. It also enables semantic
`00:F55C@all`: ordinary unsaturated additions below the current high score
reproduce all eight writes, result flags, and the 558-cycle body. The
44-boundary transcript spans the outer addition and both nested helpers, so
all 40 calls execute natively while original interrupt handlers still run at
their original boundaries. The complete 40-call/446-write artifacts remain
exact in Debug and Release. See
[`docs/ROUTINE_MAP.md`](docs/ROUTINE_MAP.md) for the field layout and exact
trace hashes.

The RNG boundary uses the same route and additionally traces its free-running
seed:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-state-census portable-data\state-census-rng.csv `
  --observe-hook 00:E8B3 `
  --replace-hook 00:E8B3@all `
  --routine-trace out\traces\state-census-rng-entries.csv `
  --routine-call-trace out\traces\state-census-rng-calls.csv `
  --routine-replay 00:E8B3@1 `
  --work-ram-write-range 222A-222B `
  --work-ram-write-trace out\traces\state-census-rng-state.csv `
  --rom "D:\path\to\Devil's Crush.pce"
```

Semantic `00:E8B3@all` derives the complete fixed RNG transaction from the
live seed and frame counter: six scratch writes, two seed writes, the
returned high byte, final arithmetic flags, and 282 body master cycles.
Its 23-boundary transcript admits all 78 calls; Debug and Release preserve
the exact census, 78-call/703-write routine artifacts, and 3,771 selected
seed writes.

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

Replacement mode is available for mapped semantic routines and focused
regression calls. Checkpointed replacements may cross event/IRQ horizons:
Beetle samples interrupts at declared original instruction boundaries, runs
the original handler, and resumes the remaining native transcript after a
state-matching `RTI`. Native game logic includes semantic position
integration `00:F1B4@all`, semantic collision rollback `00:F241@all`, and
their focused `@1`, `@10`, and `@17` integration/rollback fixtures, plus
semantic signed division `00:E905@all`, semantic damping/acceleration
`01:DD09@all` with focused `@1`, `@2`, and `@181` fixtures, semantic signed
rotation `01:D1A2@all` with focused fixture `@23`, full collision-table
record advancement `03:6966@1`,
`03:6966@2`, `03:6966@3`, `03:6966@4`, and `03:6966@9`,
headerless collision-table continuations `03:6966@10` and `03:6966@12`,
plus alternate-selector reverse-vertical continuation `03:6966@19` and
horizontal continuations `03:6966@20` and `03:6966@21`,
later vertical continuations `03:6966@22` and `03:6966@23`,
alternate-pass selection `03:6966@5` through `03:6966@8` and
`03:6966@14` through `03:6966@18`, plus semantic `03:6966@all`
alternate-pass replacement across all selectors and actor slots, semantic
actor collision-response scheduling `03:7FB1@all`, and its focused
`03:7FB1@1`, `03:7FB1@3`, and `03:7FB1@7` decrement/reload fixtures, plus
semantic Small-Soldier no-hit consumption `03:7F57@all`, ordinary score
accumulation `00:F55C@all`, RNG advancement `00:E8B3@all`, and flipper
input gating `01:D466@all`, flipper phase stepping `01:D500@all`,
phase-indexed flipper geometry `01:D081@all`, shared actor-coroutine
scheduling `01:C5C9@all`, session/nudge gating `01:C3CB@all`, and idle
session-input gating `01:C49E@all`, plus central collision-scan vertical
rejection `01:CC1A@all` and near-target geometry `01:CCD5@all`. This command
replaces the four simultaneous physics
calls, observes the full physics set, and restores each pre-call savestate
to compare the native result with assembly:

```powershell
.\out\build\windows-vs2022\Release\devils-crush.exe `
  --record-gameplay portable-data\native-replacement.csv `
  --observe-hook 00:F1B4 `
  --observe-hook 00:F241 `
  --observe-hook 01:D1A2 `
  --observe-hook 01:DD09 `
  --replace-hook 00:F1B4@17 `
  --replace-hook 00:F241@1 `
  --replace-hook 01:D1A2@23 `
  --replace-hook 01:DD09@1 `
  --routine-trace out\traces\native-replacement-entries.csv `
  --routine-call-trace out\traces\native-replacement-calls.csv `
  --routine-replay 00:F1B4@17 `
  --routine-replay 00:F241@1 `
  --routine-replay 01:D1A2@23 `
  --routine-replay 01:DD09@1 `
  --rom "D:\path\to\Devil's Crush.pce"
```

The four native transactions declare final registers, 215 ordered writes
and their entry-relative timing, 6,120 combined body master cycles, and their
outer RTS boundaries. Gameplay, entry, call, and write artifacts remain
byte-identical to the assembly baseline. Other variants remain in assembly
until their semantic paths and checkpoint transcripts are independently
proven.

`01:D081@all` is the first interrupt-bearing aggregate proof. Its 14
instruction checkpoints allow all 540 geometry calls to execute natively,
including 38 calls interrupted by the original ROM handler. The resulting
state census, complete call trace, and all 3,214 writes remain byte-identical
in Debug and Release.

A focused 1,800-frame collision-table regression replaces `03:6966@5`,
restores its frame-941 pre-call state, and compares the alternate-pass early
return with assembly. Selector `$2A` resolves to table pointer `$6C2A`;
Front-Spider slot `$26` changes pass state `$81->$80`. The transaction
matches four ordered writes at body cycles 39/66/114/135, output
`A=$80`, `Y=$81`, `P=$12`, and outer RTS `03:6988` after 165 body master
cycles. Debug and Release preserve the gameplay and complete
19,648-call/180,389-write interpreter hashes.

A companion collision-table regression replaces `03:6966@1` and restores
its frame-873 pre-call state. Selector `$2A` and record `$03` resolve table
pointer `$6C2A` to motion record `$6D56`; Front-Spider slot `$26` changes
pass `$80->$81`, event to `$02`, orientation `$0C->$8C`, cursor `0->1->2`,
and horizontal low `$40->$41`. The native transaction matches all 12 writes,
663 body master cycles, output registers, and outer RTS `03:6A37`. Debug and
Release preserve the same complete interpreter artifacts.

A second full-record regression replaces `03:6966@2` from the adjacent
frame-873 state. Selector `$2B` and record `$07` resolve table pointer
`$6C2D` to motion record `$6D9E`; Front-Spider slot `$27` changes pass
`$80->$81`, event to `$06`, orientation `$0C->$8C`, cursor `0->1->2`, and
horizontal low `$D8->$D7`. The reverse-motion branch matches all 12 writes,
672 body master cycles, output registers, and outer RTS `03:6A37`. Debug and
Release preserve the same complete interpreter artifacts.

A third full-record regression replaces `03:6966@3` from the next
frame-873 state. Selector `$2C` and record `$05` resolve table pointer
`$6C30` to motion record `$6D7A`; fixed slot `$28` changes pass `$80->$81`,
event to `$04`, orientation `$0C->$8C`, cursor `0->1->2`, and vertical low
`$82->$83`. The vertical-motion branch matches all 12 writes, 663 body
master cycles, final `A=$08`, and outer RTS `03:6A37`. Debug and Release
preserve the same complete interpreter artifacts.

A fourth full-record regression replaces `03:6966@4` from the remaining
adjacent frame-873 state. Selector `$2D` and record `$05` resolve table
pointer `$6C35` to the shared motion record `$6D7A`; fixed slot `$29`
changes pass `$80->$81`, event to `$04`, orientation `$0C->$8C`, cursor
`0->1->2`, and vertical low `$82->$83`. The transaction matches all 12
writes, 663 body master cycles, final `A=$08`, and outer RTS `03:6A37`.
Debug and Release preserve the same complete interpreter artifacts.

A second alternate-pass regression replaces `03:6966@6` and restores its
adjacent frame-941 pre-call state. Selector `$2B` resolves table pointer
`$6C2D`; Front-Spider slot `$27` changes pass `$81->$80`. The transaction
matches four ordered writes at body cycles 39/66/114/135, output `A=$80`,
`Y=$81`, `P=$12`, and outer RTS `03:6988` after 165 body master cycles.
Debug and Release preserve the same complete interpreter artifacts.

A third alternate-pass regression replaces `03:6966@7` and restores the
next frame-941 pre-call state. Selector `$2C` resolves table pointer `$6C30`;
fixed slot `$28` changes pass `$81->$80`. The transaction matches four
ordered writes at body cycles 39/66/114/135, output `A=$80`, `Y=$81`,
`P=$12`, and outer RTS `03:6988` after 165 body master cycles. Debug and
Release preserve the same complete interpreter artifacts.

A fourth alternate-pass regression replaces `03:6966@8` and completes the
frame-941 quartet. Selector `$2D` resolves table pointer `$6C35`; fixed slot
`$29` changes pass `$81->$80`. The transaction matches four ordered writes
at body cycles 39/66/114/135, output `A=$80`, `Y=$81`, `P=$12`, and outer
RTS `03:6988` after 165 body master cycles. Debug and Release preserve the
same complete interpreter artifacts.

An alternate-selector full-record regression replaces `03:6966@9` at frame
942. Caller `03:5890` supplies selector `$06`, which resolves table entry
`$0B` at `$6B0A` to motion record `$6DE6`; slot `$17` changes pass
`$80->$81`, event to `$0A`, orientation `$0D->$8D`, cursor `0->1->2`, and
horizontal low `$F6->$F5`. The reverse-motion transaction matches all 12
writes, including the stack-relative `$21F9` saved-status store, 672 body
master cycles, and outer RTS `03:6A37`. Debug and Release preserve the same
complete interpreter artifacts.

A headerless-continuation regression replaces `03:6966@10` at frame 942.
Selector `$2A` resolves `$6C2A->$6D56`; Front-Spider slot `$26` resumes at
cursor 2, consumes the `$08` vertical step without rewriting event or
orientation, changes vertical low `$7C->$7D`, and advances the cursor once
to 3. The native transaction matches nine writes, 540 body master cycles,
final `A=$08` / `Y=$02`, and outer RTS `03:6A37`. Debug and Release preserve
the same complete interpreter artifacts.

A second continuation regression replaces `03:6966@12` from the same frame.
Selector `$2C` resolves `$6C30->$6D7A`; fixed slot `$28` resumes at cursor
2, consumes headerless `$08`, changes vertical low `$83->$84`, and advances
the cursor once to 3. It matches the same nine-write/540-cycle transaction
shape with selector- and slot-specific addresses. Debug and Release preserve
the complete interpreter artifacts.

An alternate-selector pass regression replaces `03:6966@14` at frame 943.
Caller `03:5890` supplies selector `$06`, which resolves to table pointer
`$6B0A`; slot `$17` changes pass state `$81->$80`. The early-return
transaction matches four ordered writes at body cycles 39/66/114/135,
output `A=$80`, `Y=$81`, `P=$12`, and outer RTS `03:6988` after 165 body
master cycles. Debug and Release preserve the complete interpreter
artifacts.

A repeated fixed-selector pass regression replaces `03:6966@15` later in
frame 943. Selector `$2A` again resolves to table pointer `$6C2A`;
Front-Spider slot `$26` changes pass state `$81->$80`. The independently
restored call matches the existing four-write/165-cycle transaction shape,
output registers, and outer RTS `03:6988`. Debug and Release preserve the
complete interpreter artifacts.

A second repeated fixed-selector pass regression replaces `03:6966@16`
later in frame 943. Selector `$2B` resolves to table pointer `$6C2D`; the
other Front-Spider slot `$27` changes pass state `$81->$80`. It matches the
same four-write/165-cycle transaction shape, output registers, and outer RTS
`03:6988` against independently restored assembly. Debug and Release
preserve the complete interpreter artifacts.

A third repeated fixed-selector pass regression replaces `03:6966@17`
later in frame 943. Selector `$2C` resolves to table pointer `$6C30`; fixed
slot `$28` changes pass state `$81->$80`. This call enters with status `$12`
rather than the earlier matching selector call's `$02`, and the native guard
admits both independently proven variants. Its four writes, 165 cycles,
output registers, and outer RTS `03:6988` match restored assembly in Debug
and Release.

A fourth repeated fixed-selector pass regression replaces `03:6966@18`
later in frame 943. Selector `$2D` resolves to table pointer `$6C35`; fixed
slot `$29` changes pass state `$81->$80`. This call enters with status `$00`
rather than the earlier matching selector call's `$10`, and the native guard
admits both independently proven variants. Its four writes, 165 cycles,
output registers, and outer RTS `03:6988` match restored assembly in Debug
and Release.

An alternate-selector continuation regression replaces `03:6966@19` at
frame 944. Selector `$06` resolves `$6B0A->$6DE6`; slot `$17` resumes at
cursor 2, consumes headerless reverse-vertical motion byte `$10`, changes
vertical low `$C4->$C3`, and advances the cursor to 3. The native transaction
matches nine writes, 549 body master cycles, final `A=$C4` / `Y=$02`, and
outer RTS `03:6A37`. Debug and Release preserve the complete interpreter
artifacts.

A horizontal-continuation regression replaces `03:6966@20` at frame 944.
Selector `$2A` resolves `$6C2A->$6D56`; Front-Spider slot `$26` resumes at
cursor 3, consumes headerless motion byte `$20`, changes horizontal low
`$41->$42`, and advances the cursor to 4. The native transaction matches
nine writes, 540 body master cycles, final `A=$00` / `Y=$03`, and outer RTS
`03:6A37`. Debug and Release preserve the complete interpreter artifacts.

A reverse-horizontal continuation regression replaces `03:6966@21` later
in frame 944. Selector `$2B` resolves `$6C2D->$6D9E`; Front-Spider slot
`$27` resumes at cursor 3, consumes headerless motion byte `$40`, changes
horizontal low `$D7->$D6`, and advances the cursor to 4. The native
transaction matches nine writes, 549 body master cycles, final `A=$00` /
`Y=$03`, and outer RTS `03:6A37`. Debug and Release preserve the complete
interpreter artifacts.

A later vertical-continuation regression replaces `03:6966@22` later in
frame 944. Selector `$2C` resolves `$6C30->$6D7A`; fixed slot `$28` resumes
at cursor 3, consumes headerless motion byte `$08`, changes vertical low
`$84->$85`, and advances the cursor to 4. This call enters with status
`$12`; its nine writes, 540 body master cycles, final `A=$08` / `Y=$03`,
and outer RTS `03:6A37` match restored assembly in Debug and Release.

A second later vertical-continuation regression replaces `03:6966@23` at
the end of frame 944. Selector `$2D` resolves `$6C35->$6D7A`; fixed slot
`$29` resumes at cursor 3, consumes the same headerless motion byte `$08`,
changes vertical low `$84->$85`, and advances the cursor to 4. This call
enters with status `$00`; its nine writes, 540 body master cycles, final
`A=$08` / `Y=$03`, and outer RTS `03:6A37` match restored assembly in Debug
and Release.

The collision-table scheduler is no longer limited to those individual
ordinal fixtures. `--replace-hook 03:6966@all` reads the selector-pointer
table from the verified user ROM and natively handles both the four-write
alternate-pass early return and ordinary headerless record steps. The latter
resolve the collision-list and motion cursors generically, then apply either
or both signed axes with exact high-byte carry/borrow timing. The
alternate-pass path supplies all 17 original instruction checkpoints, so
Beetle can run an asserted interrupt through the original ROM handler and
resume the transaction. Uncheckpointed paths still finish before the next
user/video event or serviceable timer interrupt, otherwise original assembly
runs normally. Harmless timer-divider ticks do not force fallback. The
bounded walker also
follows `$C0` motion terminators, applies `$FE`
collision-list wraps, recognizes `$FF` table termination, and executes
headered records with their accumulated cursor-write timing. Generic
forward/reverse one-axis headerless records add exact 50-instruction
transcripts; ordinary two-axis records without a high-byte carry or borrow
extend the same transcript to 55 instructions. Across the 1,800-frame route
this replaces 18,806 of 19,648 calls (95.7%) while
preserving the exact gameplay, complete call trace, and all 180,389 ordered
writes. Ordinals remain useful regression coordinates, but are no longer the
admission mechanism for these semantic paths; the former compiled
selector/slot/ordinal proof matrix has been retired. Further expansion of
this interpreter is frozen while smaller gameplay routines are ported.

A focused 1,800-frame integration regression separately replaces
`00:F1B4@1`, restores its frame-1,049 pre-call state, and compares its
nonnegative path with assembly. It integrates `$028F00 + $0010`, leaves the
other axis unchanged, and declares 16 ordered writes over 702 body master
cycles. Debug and Release preserve the gameplay and complete
1,156-call/21,738-write integration trace hashes.

A second focused integration regression replaces `00:F1B4@10` and restores
its frame-1,058 pre-call state. Its nonnegative increment crosses the second
position from `$0291D1` to `$029273`, so assembly exits before setting the
fractional-only `$3C80` marker. The native transaction matches the early
return's `A=$91`, `P=$80`, 15 ordered writes, and 639 body master cycles.
Debug and Release preserve the same gameplay and complete integration-trace
hashes.

The semantic integration regression derives sign-extension stores,
position-store timing, comparison exits, flags, and cycle budgets from the
live slot-zero state rather than an ordinal. Path-sensitive 50-57-boundary
transcripts execute all 1,156 calls natively, including 250 original
interrupt-bearing scopes. The adjacent six-store `00:F241@all` rollback uses
a 12-boundary transcript and executes all 404 calls natively. Both preserve
the byte-identical gameplay and their complete
call/write traces; the original ordinal fixtures remain focused
restored-assembly checks.

Semantic `00:E905@all` reproduces the signed divider's inherited carry,
operand normalization, fixed 16-round unsigned core, nested stack traffic,
and quotient sign restoration. All 752 gameplay calls run natively,
including 583 calls with original interrupt-handler writes. Debug and
Release preserve call SHA-256
`20d7b35e6e17c63980db6b22e4ed425000d5a4576f1bfa5bd9b36fda86bf335d`
and write SHA-256
`59822abdd58d01fb652cd197fa0555fa40c4f21b57dbe51591adb5e0360fb81d`.

Semantic `01:DD09@all` composes that divider for either nonzero motion
component, subtracts the signed quotient, and applies `$8B/256` acceleration
to the second component. All 753 calls now run natively, including the 669
calls previously deferred for interrupt delivery. Debug and Release
preserve call SHA-256
`e285eb631a38539fcca0bed8c7e7d84a6f7dbbaa0613ee8009467b50c0f03327`
and write SHA-256
`ef4285d7a05ee530edb4563e88b4af8e0fb23deaee2188bc40828ff48b8571d8`.
Focused ordinals 2 and 181 retain their exact 2,409-cycle/68-write positive
and 2,427-cycle/69-write negative restored-assembly replays.

The separate 3,600-frame collision-boundary regression replaces
`03:7FB1@1`, restores its frame-957 pre-call state, and compares the native
result with assembly. That transaction declares three writes, 126 body
master cycles, and outer RTS `03:7FD1`; every established collision census,
work-RAM, entry, call, scoped-write, and selected-state hash remains
unchanged.

A focused 3,600-frame collision-response regression separately replaces
`03:7FB1@3`, restores its frame-958 pre-call state, and compares the next
non-reload decrement with assembly. It declares three writes at body cycles
39/93/114, moves the slot-`$62` countdown from 3 to 2, returns `A=$1C`, and
completes in 126 body master cycles. Debug and Release preserve their
established census hashes plus the complete 21,987-call/91,295-write
`7FB1` artifacts. Ordinal 2 is not admitted: its independently replayed
transcript includes a periodic interrupt.

A focused 3,600-frame collision-response regression separately replaces
`03:7FB1@7`, restores its frame-960 pre-call state, and compares the native
reload branch with assembly. It declares five writes at body cycles
39/93/114/141/162, returns `A=$04`, advances phase 0 to 1, and completes in
162 body master cycles. Debug and Release preserve their established census
hashes plus the complete 21,987-call/91,295-write `7FB1` artifacts.

The aggregate `03:7FB1@all` regression uses that same routine for every live
slot and derives the result flags as well as the 126-cycle decrement or
162-cycle reload transaction from state. Its 11/14 instruction checkpoints
cover all 21,987 calls, including 1,042 calls that execute an original ROM
interrupt handler mid-routine. Debug and Release retain the exact
state-census, call-trace, and write-trace hashes.

The outer `03:7F57@all` aggregate reuses those state-based primitives rather
than adding occurrence-specific handlers. Its admitted no-hit branch accepts
any semantic `03:6966` transaction, stages its writes into a private snapshot,
then derives either `03:7FB1` response branch from that resulting state.
Both nested JSR stack pairs, the caller's final collision-byte test, write
offsets, and total timing are composed around the child transactions. The
alternate-pass child produces 35/38-boundary parent transcripts, the
one-axis headerless child produces 68/71, and the bounded two-axis child
produces 73/76. Together they admit 20,113 of 20,867 calls (96.4%).
Debug and Release preserve the complete 20,867-call SHA-256
`3ab64128aab89d056216f79d0d3f041e186eb109048aefe82785e2875f41ec4f`
and 362,060-write SHA-256
`3b2eb1b5dbd54c1b08cf6a509820e47c2320faf544b4d15824f5426f414c6544`.

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

Semantic `01:D466@all` derives behavior from live `$2CEE & $3B80,X` state.
The zero result returns in 36 cycles with no writes. The common non-current
slot activation marks the piece active, reproduces the nested stack/scratch
writes, and installs coroutine dispatch `01:D48E` in nine writes over 282
cycles. The two remaining current-slot calls enter `D525` and produce
823/868 ordered writes, beyond the bridge's 512-write limit; later
held/release variants stay in assembly. Three checkpoints cover the
zero-input exit and 21 cover the nested
activation/scheduler path, including event/IRQ-adjacent calls. Debug and
Release admit 7,468 of 7,470 calls (99.97%) while preserving call
SHA-256
`5230b181b1cb23394d0a540346418e98244f25e0ea2a50fce136b0b007fe2947`
and write SHA-256
`b5cec75fa9f5d055fd5ed869df3ef6dabe578d9b7cd9b8481b6d701a81a209b8`.

Semantic `01:D500@all` handles the shared flipper phase step after its
indexed countdown reaches one. It clears and reloads `$3A00,X`, then
increments or decrements `$3980,X` according to `$3C80,X` orientation.
Forward and reverse paths reproduce their three writes and direction-specific
flags over 138 or 126 cycles. Their 11/9-boundary transcripts admit all 980
route calls; endpoint and non-expiring timer states outside the route remain
in assembly. Debug and Release preserve call SHA-256
`8d3708e4fe36ddd9745d77af68c3a6d575acef3ce29e23c70e72dc30eca9056f`
and write SHA-256
`29274f5c8a62a9fc9d41f7796f343ac55167fcafdf71448dc6e9f76bcfd31965`.

Semantic `01:CC1A@all` handles the central collision scan's earliest
vertical-separation rejection. Four state-derived positive/negative page
and low-byte paths reproduce the internal JSR stack writes, scratch state,
binary flags, and 150/180/246/282-cycle timing. They admit 19,192 of 85,715
route calls while preserving all 1,784,119 scoped writes. Near-target,
pointer, mask, and contact paths continue in assembly. Debug and Release
preserve call SHA-256
`c01321d866af5636ce12db1174a9a3428d08ce273cf62820fcbf446dcab1a313`
and write SHA-256
`a0366616f0bbd85b22d5a4a72e8d03e09a8866525c4c90d50c51e2ea7d035881`.

Semantic `01:CCD5@all` handles the near-target geometry leaf reached by the
remaining scan calls. It follows the live MPR mapping into the verified ROM
pointer directories, selects both phase geometry pointers, and emits the
exact signed-offset, limit, stack, and scratch transaction. Atomic
event/IRQ admission replaces 5,819 of 66,523 calls through
783/789/885/891-cycle, 16/19-write paths. Debug and Release preserve all
1,414,954 writes; instruction checkpoints are the next expansion.

Semantic `01:D081@all` handles the flipper/ball interaction's shared
geometry lookup. It masks `$78` to five phase bits and loads two 16-bit
vectors from the verified user-ROM tables at `01:D2AE` and `01:D2EE`,
preserving Y through the original stack write. Its five-write transaction
returns after a 159-cycle native body with exact accumulator and flag state.
Fourteen instruction checkpoints let Beetle run the original interrupt
handler during 38 affected calls, so all 540 calls are native. Debug and
Release preserve call
SHA-256
`32d52a16cff5b2f20d1b3e8174e75a4ca097648253ce0726970fb92707a96d08`
and write SHA-256
`b13407d580d6428cf3f7fc0460040953d4cb7136faa9305c2339ed99a8336934`.

Semantic `00:E837@all` handles the shared signed 16-bit scratch-word
negation used by the flipper projection and rotation paths. It complements
`$2F:$30`, increments the low byte, and conditionally carries into the high
byte through exact 8/9-boundary, three/four-write, 90/102-cycle transcripts.
All 894 route calls now run natively, including 24 original interrupt
passages. Debug and Release preserve call SHA-256
`5871cb89939cddaaa4d7b3f9ed74dcb78b04cf80ccc9264c672d750c412ac849`
and write SHA-256
`2904482e2d30a8800ad5a13e26e11b3b0f12adc0d9ac6bca0ecc729b6252d6bd`.
The adjacent `01:D09E` projection is now native through the composed
transaction documented below.

Semantic `00:E624@all` handles the fixed eight-bit unsigned multiply round
inside that pipeline. It shifts `$42:$43/$30`, conditionally adds
`$2D:$2E`, and propagates 16-bit carry into `$30`. The host generates
41-113 instruction checkpoints directly from the eight live multiplier
bits, preserving exact `600 + 84 * admitted-bit-count` timing. All 5,371 route calls
now run natively, including 1,592 original interrupt passages. Debug and
Release preserve call SHA-256
`d704b3698ce86f82d504ad4b7693b40f425a035d056483fd473bebba672b022c`
and write SHA-256
`a073ff2ee296a0bd34a76bee12fcf8bcd15f5c4b2407c7698feabd16e08653d4`.

Semantic `00:E65C@all` composes that child into the shared signed 8.8
multiply wrapper. It normalizes both words at `$2D:$2E` and `$2F:$30`,
skips or runs the high-byte round from live multiplier state, always runs
the low-byte round, shifts the product back to 8.8, and restores the
combined sign. The generated parent transcript includes every nested
JSR/RTS and PHX/PLX stack boundary, both child transcripts, and the exact
optional sign paths. All 3,700 route calls now run natively, including 2,157
calls with original interrupt-handler writes. Debug and Release preserve
call SHA-256
`bd816aa5d64221cdb21835e6d27fe81b640f45c32e549490bedf5535a9adc695`
and write SHA-256
`c21901978bb16c29d33af18899cdb2494aee21b76da7560a43f6405841ad0ce4`.

Semantic `01:D09E@all` composes four signed multiplications plus one
`E837` negation into the flipper vector projection. It produces
`$C0:$C1 = first*c + second*s` and
`$BE:$BF = -first*s + second*c` from the indexed motion pair and the
phase-selected coefficients at `$7D-$80`. The path-sensitive parent
transcript includes all five nested stack transactions and child
checkpoints. All 540 route calls now run natively, including 448 calls with
original interrupt-handler writes. Debug and Release preserve call SHA-256
`b2f33cee152dc71d55b6da09a446510faeecdca93010f4284ed24ab46a0e1cae`
and write SHA-256
`51fc462de6afc7355c005bbde18879a4fa0fa6390554ddf097ff3f06c9b44826`.

Semantic `01:D1A2@all` uses the same bounded child family for signed
two-dimensional rotation. From live `u=$C1:$C0`, `v=$BF:$BE`,
`c=$7E:$7D`, and `s=$80:$7F`, it stores `u*c-v*s` in
`$3480:$3400,X` and `u*s+v*c` in `$3580:$3500,X`. Its path-sensitive
parent transcript composes four `E65C` multiplications and one `E837`
negation with exact scratch, indexed output, stack, and interrupt timing.
All 354 current route calls now run natively, including 296 calls with
original interrupt-handler writes. Debug and Release preserve call SHA-256
`dac3fcbd9d547af534494dbb2d0e85508209afce444689831d0fd9525d88578c`
and write SHA-256
`e0465a8fee8556dc82827845a87a1dc575a7d70f9d9aa81e9c9d75dca6cdf523`.

Semantic `01:D612@all` scales the live signed 8.8 flipper radius
`$31:$32` by coefficient `$7D:$7E` and stores the result in `$C2:$C3`.
Its parent transcript composes one complete semantic `E65C` child with
exact operand setup, result stores, nested stack writes, and interrupt
checkpoints. All 62 calls run natively, including 51 calls with original
interrupt-handler writes. Debug and Release preserve call SHA-256
`20c409aa3cda48b3dbc0bf8c5e1e0a82319f7aa8efdeb2c70374ae884811bb52`
and write SHA-256
`73c5f1848187c576bbb48595f7d3fe95b03c70cee7c67cd18994b71c9038308f`.

Semantic `01:D62E@all` derives a direct or wrapped flipper-angle delta from
`$3180,X/Y` and `$3C80,Y`, converts it to 8.8 with the original five
shift/rotate pairs, scales it by `$9E:$9F` through one semantic `E65C`
child, and applies the direction-selected sign into `$31:$32`. All 62 calls
run natively, including 43 calls with original interrupt-handler writes.
Debug and Release preserve call SHA-256
`fd133773c9c76ba17d3143052aaf88efc3a4cbbfe887b72dfd076f5f74e9eafb`
and write SHA-256
`23a2558bb00f772c4509ec470d8f8b5b642d95b2ac4c343c7f64ae1c870735eb`.

Semantic `01:CFD8@all` applies the collision mode selected by `$78 & $60`
to `$BF:$BE`: ordinary positive/negative clamping, signed 3/16 or 1/4
scaling, one-half scaling, or one-sixteenth scaling. All 354 calls run
natively, including 41 calls with original interrupt-handler writes. Debug
and Release preserve call SHA-256
`a3ec557a3eddcd91aa5b58918fb52ab8d038e36a7e2ea13bda6d602149bb842f`
and write SHA-256
`0f9ec94cd10cc59a8f0cbc9d70bfdd06c8ff2a3bea9623f76c6e662dc00b30ec`.

Semantic `01:C5C9@all` handles the shared actor-coroutine scheduler used by
many table controllers. It consumes the current JSR return from the stack,
optionally decrements `$3D80,X`, and either yields or installs the following
instruction in `$3F00,X/$3F80,X`. Immediate, delayed, and return-page-carry
branches are derived from live state rather than callers or call ordinals.
Its delayed-yield path supplies eight instruction-boundary checkpoints;
installation paths supply 12-15 depending on countdown and page carry. All
2,462 calls now remain native across exact 102-192-cycle transactions while
Beetle executes original interrupt handlers. Debug and Release preserve call SHA-256
`924221bbff7b244e91850fa206ad8ce70c2e86d3cf894824dd5e6b7b09da3bd9`
and write SHA-256
`1ef7a9e9aa5007f01a0138660f948ab77d5bc67292d9e5a076a8284d1eebc1aa`.

Semantic `01:C3CB@all` covers the neighboring gate's session-state,
ordinary idle, timer-reload, active-nudge start, and phase-finish branches.
The first three paths retain their exact 69/177/207-cycle behavior. The
three active starts use a 66-boundary, 762-cycle transcript; their three
phase finishes use a 44-boundary, 522-cycle transcript. A strict
physical-`$FF`, logical-`$0000-$0003` transaction allowance sends their base
VDC port writes through Beetle's normal bus path and accounts for every VDC
wait cycle. Together the 8/17/20/66/44-boundary transcripts admit all 2,649
calls.
Debug and Release preserve call SHA-256
`eb17f2e9bc2e4fac07823be555379dfb75c21e5c0f4ef4161084ed7588198905`
and write SHA-256
`9e7bf6564ef1c61afac2ad63cdcbf2f8742318711b051744ffab24f552c6c40d`.

Semantic `01:C49E@all` covers the immediately following no-Run-input return.
It resolves the active player, tests `$2018,Y & $08`, and returns exact
`A/Y/N/Z` state after 72 cycles without writes. Its seven-boundary transcript
admits all 2,649 calls; pause, high-score, alternate-session, and
initialization paths outside this route remain in assembly. Debug and Release
preserve call SHA-256
`1e5052965bd3252e116ee6cd4b72ea5213cab371e9e0c6faad083af9915e381e`
and write SHA-256
`09e04a8271fd92ad23977712ed8511a70fccac3457b3101cbda6671d5a0ec402`.

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
