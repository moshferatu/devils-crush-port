# Devil's Crush PC Port

A PC port of the TurboGrafx-16 pinball game *[Devil's Crush](https://en.wikipedia.org/wiki/Devil%27s_Crush)*, built to be played either on a desktop or a virtual pinball cabinet setup with 3 screens.

Being able to play the game on a cabinet was the original motivation for the project.

TurboGrafx-16 / PC Engine hardware is still partially emulated, and the game's own logic is being progressively replaced with native C++.

## ROM (Required)

The game is not included. You will need to supply the supported ROM yourself:

| | |
| --- | --- |
| Region | USA |
| Size | 393,216 bytes |
| MD5 | `92fbe89d4c87bf283dbfbeac69a64e6e` |
| SHA-256 | `9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9` |

Currently, all other ROM files are unsupported.

ROM files and locally derived assets are ignored by `git` and are not distributed with the project.

## Running

On a desktop, in a window:

```powershell
devils-crush.exe --no-cabinet --rom "D:\path\to\Devil's Crush.pce"
```

On a virtual pinball cabinet, across three screens (the default):

```powershell
devils-crush.exe --rom "D:\path\to\Devil's Crush.pce"
```

Music voice overlay is also enabled by default during play. Add
`--no-music-voice-overlay` to disable it. The existing `--cabinet` and
`--music-voice-overlay` flags still explicitly enable their features.
Diagnostic recordings retain their reference defaults; `--emulated-psg`
also disables the default overlay because it requires the native PSG.

To start a session directly in one of the six bonus stages, add
`--bonus-stage` with a stage number from 1 through 6. This works in both
desktop and cabinet modes and uses the game's normal bonus-stage transition:

```powershell
devils-crush.exe --bonus-stage 3 --rom "D:\path\to\Devil's Crush.pce"
```

| Key | |
| --- | --- |
| F5 / F9 | Save and restore a quick state |
| F6 | Toggle playfield motion/artwork smoothing on high-refresh displays |
| F10 | Reload `portable-data/gamecontrollerdb.txt` after editing it |
| F11 | Toggle playfield borderless fullscreen |
| F12 | Open cabinet display assignment |
| Escape | Quit (outside the display assignment screen) |

The flag `--emulate-only` can be used to execute the original assembly instead, should that be necessary.

## Controls

| Input | |
| --- | --- |
| Left Shift, Left | Left flipper |
| Right Shift, Right | Right flipper |
| Enter | Launch the ball in the shooter; start, pause, and confirm everywhere else |
| X | Nudge |

The first available SDL gamepad works too, and extra mappings can be supplied
in `portable-data/gamecontrollerdb.txt`. Keys are rebindable under `input.` in
`portable-data/cabinet.ini`, and which pad inputs work each flipper is
configurable as well — see
[docs/CABINET_PREVIEW.md](docs/CABINET_PREVIEW.md#controls) for the full
mapping.

### Analog plunger

A cabinet's plunger is supported as one axis of an ordinary game controller, so
any board that reports one works. The settings are a device name, an axis, and
the two axis values the rod rests and bottoms out at, all under
`input.plunger.` in `cabinet.ini`. Run the probe, pull the rod a few times, and
it prints the four numbers ready to paste in:

```powershell
devils-crush.exe --cabinet-plunger-probe
```

There is no plunger until one is named (`input.plunger.axis=-1`), because a
plunger and a thumbstick look identical from the host's side. The endpoints do
default to a Cleveland Software Design PinOne's, which puts its plunger on the
Z axis, so a PinOne needs only `input.plunger.axis=2` — but confirm it with the
probe, because a board's own firmware decides which end of its travel reads as
drawn. If `rest` and `drawn` are the wrong way round, every shot dribbles
instead of launching.

The rod drives the shooter's draw distance, not the launch button, so the
cartridge works out each shot's velocity exactly as it always did.
`--cabinet-plunger-check` demonstrates it: four draws produce four launch
velocities. See
[docs/CABINET_PREVIEW.md](docs/CABINET_PREVIEW.md#the-plunger) for the
measurements behind this.

## Cabinet Mode

Cabinet mode runs the game using 3 windows:

1. The playfield window renders the game in portrait mode. Individual table sections from the original game have been stitched together.
2. The DMD (Dot Matrix Display) window cycles through the five high scores at startup, then displays the current score, bonus points, and ball once play begins.
3. The backglass window displays the game's artwork.

### Screen Assignment

Press F12 to pause into display assignment:

| Key | |
| --- | --- |
| 1 / 2 / 3 | Select the playfield, DMD, or backglass |
| Left / Right | Cycle that role's monitor |
| F | Toggle that role's fullscreen state |
| B | Open the backglass picker |
| Delete | Clear the backglass image |
| Enter, Escape, F12 | Save and return |

### Deploying to a cabinet

`tools/Publish-Cabinet.ps1` builds the `windows-cabinet` preset and stages a
self-contained folder that can be synced to a cabinet on its own, without the
workspace:

```powershell
.\tools\Publish-Cabinet.ps1
```

See
[docs/CABINET_PREVIEW.md](docs/CABINET_PREVIEW.md#deploying-to-a-cabinet)
for what the folder contains and how runtime state is kept per-machine.

## Saved data

Everything the host keeps between runs lives in `portable-data/` beside the
executable:

| File | What it holds |
| --- | --- |
| `devils-crush.sav` | The cartridge's backup RAM, including the five numbered save slots the game writes from its own save screen |
| `devils-crush.hiscore` | The high-score table |
| `devils-crush.state` | The F5/F9 quick state |
| `cabinet.ini` | Display assignment, key bindings, and plunger calibration |

High scores save themselves. The cartridge only ever kept its table by spending
one of five save slots on it by hand — pause, Select for High Scores, Button I,
pick a slot — because a HuCard had two kilobytes of battery-backed RAM to spend
carefully. A PC does not, so the host writes the table whenever it changes and
puts it back at boot, leaving the five original slots for the games they were
meant for. Delete `devils-crush.hiscore` to go back to the ROM's own five
1,000,000-point `AAA` entries.

## Building from source

You need Windows 10 or 11 on x64, Visual Studio 2022 with the Desktop
development with C++ workload, CMake 3.30 or newer, and internet access during
the first full configure to fetch the pinned dependencies.

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-release
```

The host lands in `out\build\windows-vs2022\Release\devils-crush.exe`. To check
a ROM without opening a window:

```powershell
devils-crush.exe --validate-rom "D:\path\to\Devil's Crush.pce"
```

[docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) covers the other presets, which
test suite to run when (the Debug ROM sweep takes hours and is not a build
check), the smoke tests, and the release staging layout.

## Documentation

| | |
| --- | --- |
| [PLAN.md](PLAN.md) | The approved architecture |
| [PROGRESS.md](PROGRESS.md) | Current implementation status |
| [docs/CABINET_PREVIEW.md](docs/CABINET_PREVIEW.md) | Cabinet host behavior, controls, and the measurements behind them |
| [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) | Presets, test suites, and release staging |
| [docs/PORT_HISTORY.md](docs/PORT_HISTORY.md) | What has been bounded, replaced, and proven, in order |
| [docs/INSTRUMENTATION.md](docs/INSTRUMENTATION.md) | Recording, tracing, and oracle-comparison commands |
| [docs/NATIVE_PORT_WORKFLOW.md](docs/NATIVE_PORT_WORKFLOW.md) | The validation cadence used for each port iteration |
| [docs/NATIVE_REGISTRY.md](docs/NATIVE_REGISTRY.md) | The admitted native routines and their measured coverage |
| [docs/ROUTINE_MAP.md](docs/ROUTINE_MAP.md) | Mapped ROM routines, their evidence, and artifact hashes |
| [docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md) | Which gameplay routes have been screened for a native seam, and what each reached |
| [docs/RETAINED_ASSEMBLY_SEAMS.md](docs/RETAINED_ASSEMBLY_SEAMS.md) | Code deliberately left emulated, and why |
| [docs/PROGRESS_ARCHIVE.md](docs/PROGRESS_ARCHIVE.md) | The full historical progress log through 2026-08-23 |
