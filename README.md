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

Run a headless ten-frame emulator check:

```powershell
.\out\build\windows-vs2022\Debug\devils-crush.exe --smoke-test --rom "D:\path\to\Devil's Crush.pce"
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

The current host boots the verified ROM through the pinned Beetle PCE core,
plays its float-stereo audio, accepts keyboard or the first available SDL
gamepad, and presents the emulated framebuffer through the fixed sharp scaler.
Gamepads can be connected, disconnected, or replaced while the host is
running. Optional SDL-format mappings are loaded from
`portable-data/gamecontrollerdb.txt`; F10 reloads the file after edits. F11
toggles borderless fullscreen; Escape exits. F5 atomically writes a versioned
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

ROM-required CTest coverage is local-only and disabled by default:

```powershell
cmake --preset windows-vs2022 `
  -DDC_ENABLE_ROM_TESTS=ON `
  -DDC_ROM_PATH="D:\path\to\Devil's Crush.pce"
ctest --test-dir out/build/windows-vs2022 -C Debug -L rom_required
```
