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

Milestone 2 adds a read-only HuC6280 routine-entry boundary keyed by physical
8 KiB bank and logical PC. Pass-through mode verifies that a known canonical
entry is reached while generating the same golden recording:

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
while retaining the canonical RAM/video/audio hash. Replace mode remains
disabled until mutation and cycle-accounting semantics are shadow-tested.

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
