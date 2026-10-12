# Setup and configuration

Controls, settings and cabinet hardware for the Devil's Crush PC port. The
[README](../README.md) has the overview.

## Launcher

`devils-crush-launcher.exe` sits beside the game and is the easy way to set it
up: `devils-crush.ini` a tab at a time, and a Play button that saves
and starts the game, then comes back when the game closes.

| Tab | |
| --- | --- |
| Game | The ROM, and cabinet or desktop mode |
| Video | Smoothing, and for the cabinet: the playfield rotation, and which monitor each window is on and whether it is fullscreen |
| Audio | The music voice overlay |
| Cabinet | The nudge board and nudge strength, and the analog plunger |
| Keyboard | The keys for each PC Engine pad input, the launch keys, and each flipper's pad inputs |
| Misc. | A starting bonus stage, and dragging the ball with the mouse |

It is worked by mouse, keyboard or controller: Up and Down move, Left and
Right change a value, Enter opens a list or waits for a key to bind, Tab (or L1
and R1) turns the tabs, Ctrl+S saves, and Ctrl+Enter, F5 or Start plays. The
ROM can also be chosen with Browse or dropped on the window.

The launcher's icon and heading are the title screen's logo, decoded from the
ROM at build time (`tools/make_launcher_art.cpp`), since nothing of the ROM's
is kept in the repository. Without `Devil's Crush.pce` in the source folder,
or `-DDC_LAUNCHER_ART_ROM=<path>`, it builds without an icon and with a text
heading.

## Running

On a desktop, in a window:

```powershell
devils-crush.exe --no-cabinet --rom "D:\path\to\Devil's Crush.pce"
```

On a virtual pinball cabinet, across three screens (the default):

```powershell
devils-crush.exe --rom "D:\path\to\Devil's Crush.pce"
```

With no options the game plays as `devils-crush.ini` says, which is
what the launcher edits: `game.rom`, `game.cabinet`, `game.bonus_stage`,
`game.drag_ball`, `audio.music_voice_overlay`, `video.smoothing` and
`input.nudge_multiplier`. An option on the command line beats the file for
that run, and a file without them plays with the defaults below.

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

To put the ball where a table event needs it, add `--drag-ball`, or turn on
Drag Ball With Mouse in the launcher. The mouse pointer then shows over the
game, and while the left button is held on the playfield the ball is carried
under it, on the table and in the bonus stages, in desktop and cabinet modes.
The ball keeps the pointer's motion when the button is let go, so it can be
thrown.

A carried ball is out of play: nothing on the table moves it or is hit by it
until it is let go, so an event is set off by dropping the ball on it. On the
table the game's camera has to come with the ball, because the game only runs
what is on camera, and it comes at eight rows a frame: the ball rides the edge
of the camera's screen until the pointer's place is on it, which from one end
of the table to the other takes about a second. In desktop mode, holding the
ball at the top or bottom edge of the window scrolls the table the same way.
The ball can be taken straight out of the Devil Shooter without launching
it, and counts as launched once it is carried out of the shooter's lane. It
is a cheat for testing, so scores made with it are saved like any
others, and it is not available with `--capture-session`.

| Key | |
| --- | --- |
| F5 / F9 | Save and restore a quick state |
| F6 | Toggle playfield motion/artwork smoothing on high-refresh displays |
| R / Shift+R | Rotate the main playfield clockwise / counterclockwise by 90° |
| F10 | Reload `gamecontrollerdb.txt` after editing it |
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
| Space | Nudge |

The first available SDL gamepad works too, and extra mappings can be supplied
in `gamecontrollerdb.txt`. Keys are rebindable under `input.` in
`devils-crush.ini`, and which pad inputs work each flipper is
configurable as well — see
[docs/CABINET_PREVIEW.md](CABINET_PREVIEW.md#controls) for the full
mapping.

### Analog plunger

A cabinet's plunger is supported as one axis of an ordinary game controller, so
any board that reports one works. The settings are a device name, an axis, and
the two axis values the rod rests and bottoms out at, all under
`input.plunger.` in `devils-crush.ini`. Run the probe, pull the rod a few times, and
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
[docs/CABINET_PREVIEW.md](CABINET_PREVIEW.md#the-plunger) for the
measurements behind this.

## Cabinet Mode

Cabinet mode runs the game using 3 windows:

1. The playfield window renders the game in portrait mode. Individual table sections from the original game have been stitched together.
2. The DMD (Dot Matrix Display) window plays the head from the game's ending and cycles through the five high scores at startup, then displays the current score, bonus points, and ball once play begins.
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
[docs/CABINET_PREVIEW.md](CABINET_PREVIEW.md#deploying-to-a-cabinet)
for what the folder contains and how runtime state is kept per-machine.

## Saved data

Everything the host keeps between runs lives in the folder the game is started
from, which is the executable's own when it is opened from Explorer or through
the launcher. A front end that starts it from somewhere else should set its
working directory (its "Start in") to the game's folder.

| File | What it holds |
| --- | --- |
| `devils-crush.sav` | The cartridge's backup RAM, including the five numbered save slots the game writes from its own save screen |
| `devils-crush.hiscore` | The high-score table |
| `devils-crush.state` | The F5/F9 quick state |
| `devils-crush.ini` | The launcher's settings, display assignment, key bindings, and plunger calibration |
| `gamecontrollerdb.txt` | Your own controller mappings, if you add any |

Earlier builds kept these in a `portable-data` subfolder, with the settings
named `cabinet.ini`. The game and the launcher move them out on first start,
and remove the folder if nothing else is left in it.

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

[docs/BUILD_AND_TEST.md](BUILD_AND_TEST.md) covers the other presets, which
test suite to run when (the Debug ROM sweep takes hours and is not a build
check), the smoke tests, and the release staging layout.

## More documentation

| | |
| --- | --- |
| [PROGRESS.md](../PROGRESS.md) | Current implementation status, and an index of the port's own documents |
| [CABINET_PREVIEW.md](CABINET_PREVIEW.md) | Cabinet host behavior, controls, and the measurements behind them |
| [BUILD_AND_TEST.md](BUILD_AND_TEST.md) | Presets, test suites, and release staging |
| [INSTRUMENTATION.md](INSTRUMENTATION.md) | Recording, tracing, and oracle-comparison commands |
