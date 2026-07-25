# Mesen2 Canonical Comparison

## Oracle

- Mesen 2.1.1, official Windows release.
- Release archive SHA-256:
  `23ccc2bc060b663c68dad3a8c5d6da7d23a50f872d04f135bafa2b04ff7d5cbe`.
- Extracted `Mesen.exe` SHA-256:
  `d2eb03c2590c648bf329f127ebcfefd70130e7690a9e2ccdba8616faea1fe96b`.
- Beetle PCE revision:
  `ae99235c2139c176c1a8d0fde2957bf701d3cab0`.
- ROM SHA-256:
  `9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9`.

The comparison uses Mesen's headless `--testRunner` mode. Its isolated PCE
configuration enables a TurboTap with five two-button pads to match the
host's pinned libretro options and disables frame skipping. Both branches
run the same 600-frame input sequence. Two independent Mesen captures check
the oracle's work RAM, normalized write-event sequence, CPU frame-boundary
state, and framebuffer for repeatability.

## Work-RAM result

Comparison performed on 2026-07-24:

| Measurement | Result |
| --- | ---: |
| Frames | 600 |
| Bytes compared per frame | 8,192 |
| Total byte observations | 4,915,200 |
| Different byte observations | 341 (0.006938%) |
| Average different bytes per frame | 0.5683 |
| Maximum differences in one frame | 10 |
| Distinct differing RAM offsets | 15 |
| Byte-exact frames | 304 |

The differing offsets and number of affected frames are:

| Offset | Frames | Offset | Frames |
| ---: | ---: | ---: | ---: |
| `0x01F6` | 288 | `0x01F2` | 8 |
| `0x01EF` | 7 | `0x002F` | 6 |
| `0x01F1` | 6 | `0x01ED` | 4 |
| `0x01F3` | 3 | `0x0030` | 3 |
| `0x01F0` | 3 | `0x01F4` | 3 |
| `0x01EE` | 3 | `0x01F5` | 2 |
| `0x0037` | 2 | `0x0038` | 2 |
| `0x01F7` | 1 |  |  |

All 341 observations are explained by startup scratch storage:

| Classification | Different byte observations |
| --- | ---: |
| Zero-page boot scratch | 13, last seen on frame 6 |
| Inactive/free stack bytes | 328 |
| Active stack bytes | 0 |
| Other work RAM | 0 |

The zero-page differences are the source pointer and temporary bytes used by
the boot decompressor: `0x002F`/`0x0030` and `0x0037`/`0x0038`. Write tracing
identifies their final writers at physical ROM addresses `0x000473`,
`0x000479`, `0x00045A`, and `0x00045F`. The stack-page differences occupy
`0x01ED` through `0x01F7`; comparison against Mesen's frame-boundary stack
pointer finds none in the live stack.

The initial unnormalized result also contained 1,188 persistent observations
at `0x00B4` and `0x0D5C`. Their traced writer subtracts backup-RAM header
words. Mesen initializes its 2 KiB PCE backup RAM header as
`HUBM 00 A0 10 80`, whereas pinned Beetle initializes
`HUBM 00 88 10 80`; the resulting word subtraction is exactly the observed
`0x1F` versus `0x07`. The Mesen capture now strictly validates all eight
header bytes and changes only byte offset 5 from `0xA0` to the Beetle
baseline `0x88` before frame 1. This normalizes an emulator initialization
policy, not game execution.

Report format v5 records the backup-RAM normalization, every differing
offset's first/last frame and values, the work-RAM classification, write
events, and CPU frame-boundary state. The two Mesen runs reproduce work RAM,
the normalized write-event sequence, and CPU boundaries exactly. There is no
unexplained semantic work-RAM divergence in the canonical window.

## Framebuffer result

Comparison performed on 2026-07-24:

| Measurement | Result |
| --- | ---: |
| Beetle geometry | 256x240 |
| Mesen geometry | 292x242 |
| Mesen normalization | source `(18,3)`, one black bottom row |
| Presentation alignment | Beetle frame `n` to Mesen frame `n` |
| Frames compared | 600 |
| Byte-exact normalized frames | 600 |
| Pixel observations | 36,864,000 |
| Different pixel observations | 0 |
| Average different pixels per frame | 0 |
| Maximum differences in one frame | 0 |

Both recorders emit a 16-byte `DCF1` header (`DCF1`, then little-endian
32-bit width, height, and frame count) followed by RGB888 pixels. Mesen uses
an explicit RGB cube quantized through the same RGB565 expansion as the host,
removing its normal composite-palette difference. The comparison then
searches a bounded set of canonical frames for one fixed spatial and temporal
alignment. Mesen's 292x242 Lua surface normalizes to Beetle's
configured scanline range by removing 18 pixels on each horizontal side and
three rows at the top, then supplying the one black bottom row that lies
outside Mesen's surface.

Earlier captures varied between zero and two frames of apparent presentation
phase because Mesen's frame skipping was not pinned off. With
`PcEngine.DisableFrameSkipping = true`, both independent raw Mesen
framebuffer streams are byte-identical and all 600 normalized frames match
Beetle pixel for pixel at zero temporal shift.

## Audio result

Mesen's Lua API does not expose audio, but its unchanged native core exports
an existing deterministic `WaveRecord`/`WaveStop` path. The checked-in
builder applies a narrow patch to the managed 2.1.1 headless runner so it
starts and stops that recorder around the 600-frame Lua script. It verifies
that the generated runner retains the official `MesenCore.dll` byte for byte
(SHA-256
`65f69a7bdb25c35fdca77c9c73e71ef3e8ec233380204d94cfc0179933048c74`).

Both branches record PCM16 stereo at 44.1 kHz:

| Measurement | Result |
| --- | ---: |
| Beetle stereo frames | 442,264 |
| Mesen stereo frames | 442,262 |
| Duration difference | -2 samples (-0.045351 ms) |
| Best waveform alignment | 9 samples (0.204082 ms) |
| 10 ms RMS-envelope correlation | 0.981922894 |
| Mesen repeat WAVs | Byte-exact |

The aligned raw waveform correlation is `0.386539938`; sample-exact output is
not expected from two independent PSG implementations and resamplers. The
near-identical duration and high amplitude-envelope correlation establish
canonical audio timing and audible-event progression without treating one
emulator's synthesis artifacts as required game behavior.

Work RAM, framebuffer, audio timing, and scene progression now have no
unexplained semantic difference in the canonical window.

## Reproduction

Build the Debug host, obtain the pinned Mesen executable, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Compare-Mesen2.ps1 `
  -MesenPath "D:\tools\Mesen-2.1.1\Mesen.exe" `
  -AppPath out\build\windows-vs2022\Debug\devils-crush.exe `
  -RomPath "D:\path\to\Devil's Crush.pce"
```

Generated ROM-derived streams and `comparison.json` are written below
`out/traces/mesen2` and remain excluded from source control.

To build the audio-capable managed runner around the unchanged official core,
use the extracted 2.1.1 source and application directories:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Build-AudioRunner.ps1 `
  -MesenSourceDirectory "D:\tools\Mesen2-2.1.1-source" `
  -MesenAppDirectory "D:\tools\Mesen-2.1.1"
```

Then perform two independent Mesen WAV captures and the cross-core timing
comparison:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\mesen2\Compare-MesenAudio.ps1 `
  -MesenAudioRunnerDirectory `
    out\tools\mesen2-audio-runner\bin\win-x64\Release `
  -AppPath out\build\windows-vs2022\Debug\devils-crush.exe `
  -RomPath "D:\path\to\Devil's Crush.pce"
```
