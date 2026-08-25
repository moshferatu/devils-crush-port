# Instrumentation

Every recording, tracing, and oracle-comparison command the port uses. None of
this is needed to play the game — see the [README](../README.md) for that — but
these routes produce the deterministic artifacts that
[PORT_HISTORY.md](PORT_HISTORY.md), [ROUTINE_MAP.md](ROUTINE_MAP.md), and
[NATIVE_REGISTRY.md](NATIVE_REGISTRY.md) quote.

Recordings and other traces stay local and are excluded from Git.

## Canonical and gameplay recordings

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

## State-census routes

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
[`ROUTINE_MAP.md`](ROUTINE_MAP.md).

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
[`ROUTINE_MAP.md`](ROUTINE_MAP.md).

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
[`ROUTINE_MAP.md`](ROUTINE_MAP.md).

## The routine-entry registry

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
in [`ROUTINE_MAP.md`](ROUTINE_MAP.md).

## Observing the gameplay route

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
frames. See [`ROUTINE_MAP.md`](ROUTINE_MAP.md) for the exact
classification and hashes.

The apparent Mesen `+1/+1/+26` entry deltas are one frame of table-activation
phase, not additional gameplay calls: after the shared four-call bootstrap,
Beetle frame `n` matches Mesen frame `n-1` exactly in per-frame call count,
A/X/Y/SP, and stable MPRs. See
[`ROUTINE_MAP.md`](ROUTINE_MAP.md) for callers and mapped state
fields. Across the 21,368 aligned session/collision calls, 18,075 bounded
scopes match exactly in cycle budget and ordered routine-owned writes. The
remaining 3,293 move a periodic interrupt between neighboring scopes; after
separating the interrupt stack/handler writes, their return paths, outputs,
and routine-owned write streams also match.

## Work-RAM write census and seeding

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

## The Beetle fork and the Mesen2 oracle

The pinned upstream Beetle source is reproducibly modified by the ordered
project-owned patch series documented in
[`third_party/beetle-pce-fork/MANIFEST.md`](../third_party/beetle-pce-fork/MANIFEST.md).

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
[MESEN2_PARITY.md](MESEN2_PARITY.md) for the pinned oracle,
latest comparison results, and normalization rationale.

Mesen's native core has a WAV recorder that its headless command line does
not expose. `tools\mesen2\Build-AudioRunner.ps1` applies a managed-runner-only
patch to the official 2.1.1 source and verifies that `MesenCore.dll` remains
unchanged. `tools\mesen2\Compare-MesenAudio.ps1` then records two independent
Mesen WAVs and reports duration, alignment, envelope correlation, and oracle
repeatability against the host's canonical WAV. Exact commands and current
results are in [MESEN2_PARITY.md](MESEN2_PARITY.md).

## Backup-RAM persistence

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
[BACKUP_RAM_VALIDATION.md](BACKUP_RAM_VALIDATION.md) for the
procedure and result.

