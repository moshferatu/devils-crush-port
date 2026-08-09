# Removing the emulator

The native-share percentage in [NATIVE_REGISTRY.md](NATIVE_REGISTRY.md)
measures one thing: how many master cycles are executed by native
replacements instead of by the emulated HuC6280. That is a good proxy for
"how much of the game logic have we ported", and a poor one for "can we
delete Beetle PCE". This document is about the second question.

## The three axes

Removing the emulator needs work on three fronts. The metric tracks one.

| Axis | What it means | Tracked by native share |
| --- | --- | --- |
| **CPU code** | Every routine runs as native C/C++ instead of emulated 6280 | Yes |
| **Device state** | The VDC and PSG are native models, not Beetle's | No |
| **Time** | Interrupt scheduling and cycle pacing are native | No |

The gap matters concretely. Today a "native" routine still drives emulated
hardware: `replace_sound_timer_driver` emits a write to `$0806`, the core
applies it with `WrMem`, and it lands in **Beetle's** PSG, which synthesises
the audio. Same for every VDC port write. At a hypothetical 100% native
cycle share the emulator would still be present, still own all device state,
and still own the clock.

So the percentage will plateau well short of "removable". Track the phases
below, not the number.

## The seam already exists

`PceRuntime` in
[`include/devils_crush/emulation/pce_runtime.hpp`](../include/devils_crush/emulation/pce_runtime.hpp)
is a pure virtual interface, and there are already two implementations:
`src/emulation/pce_runtime_beetle.cpp` and `src/emulation/pce_runtime_stub.cpp`.

The end state is a third — a native runtime satisfying the same contract with
no Beetle dependency. The part of the contract that has to hold is small:

```cpp
virtual EmulatedFrame run_frame(InputFrame) = 0;   // framebuffer + audio
virtual std::vector<std::byte> export_work_ram() const = 0;
virtual std::size_t backup_ram_size() const noexcept = 0;
virtual SaveState serialize() const = 0;
```

Everything else on the interface — execution hooks, replacement hooks,
routine-call observation, instruction profiling, write observers — is
transition scaffolding for the hybrid. A native runtime does not need any of
it, and it all gets deleted with the emulator.

## The acceptance test already exists

`--record-state-census` records, for each of 3,600 frames:

```
frame,input_p1,input_p2,work_ram_sha256,
framebuffer_width,framebuffer_height,framebuffer_sha256,
audio_sample_rate,audio_stereo_samples,audio_sha256
```

That is a complete oracle. It pins memory, video output and audio output,
frame by frame, against a fixed input script. A native runtime is finished
when the census still hashes to `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`.

Two consequences worth noting:

- Device models are directly verifiable. A native PSG is correct when
  `audio_sha256` matches; a native VDC when `framebuffer_sha256` matches. No
  new test harness is required for phase 2.
- The oracle covers one route. Code the census route never executes is not
  proven by it — see the unexercised arms of the sound driver noted in
  `native_replacements_sound.cpp`. Broader routes are worth adding before
  relying on the census as a completeness argument.

## Inventory: what still executes as emulated 6280

From a whole-ROM `--instruction-profile` of the deployed route
(1,292,378,139 master cycles). Clusters of contiguous addresses that are not
native entry points:

| Region | Master cycles | Share | What it is |
| --- | ---: | ---: | --- |
| `00:E2B7`–`E39C` | 584,758,197 | 45.25% | Frame-sync spin loop |
| `00:FC03`–`FCB3`, `00:FD07`–`FDF1` | 173,243,694 | 13.41% | Sound driver — **already ported** (see below) |
| bank `45` (`4000`–`4A9B`) | 45,583,803 | 3.53% | Music sequencer — 39% ported, `4015` tick and eight subroutines left |
| `00:E93C`–`EBD6` | 20,611,233 | 1.59% | VCE palette DMA |
| `00:E0EB`–`E292` | 15,342,522 | 1.19% | VBlank handler and pad reader |
| `00:E3C0`–`E47C` | 7,031,316 | 0.54% | VRAM tile upload |
| `00:F240`–`F4FB` | 5,761,776 | 0.45% | Tilemap upload, camera-sweep outer |
| `03:694A`–`6A37` | 5,274,582 | 0.41% | Gameplay tail |
| `02:586F`–`5903` | 4,055,142 | 0.31% | Gameplay tail |
| `01:CA84`–`CC04` | 3,974,346 | 0.31% | Collision tail |

The sound-driver row is not unported work. That code is native, proven and
byte-exact; it runs emulated on 48% of entries only because the replacement
hook is gated on `!ReplacementActive` and a timer interrupt usually preempts
another suspended transcript. **Do not fix that in the emulator.** It is a
limitation of the hybrid harness, it ports nothing, and it disappears on its
own when the CPU does.

## Phases

### Phase 1 — finish the CPU code

Remaining real logic, in rough order of value:

1. **Bank `45` music sequencer** (45.6M, of which 15.3M is done). The last
   large block of ordinary logic. The PSG write channel and the bus-write
   census built for the timer driver applied unchanged, as expected. The bank
   turned out to be sixteen small subroutines rather than a monolith, so it is
   going in slices: the sample/envelope group is native, and the per-frame
   tick at `45:4015` (13.8M), the prologue `45:4142` (7.4M) and the
   step-limit handler `45:457A` (1.7M) are next. See
   [NATIVE_FALLBACK_HOTSPOTS.md](NATIVE_FALLBACK_HOTSPOTS.md#the-music-sequencer-and-screening-a-bank-instead-of-a-family).
2. **Video upload paths** (33.4M): palette DMA `00:E93C`–`EBD6`, VRAM tiles
   `00:E3C0`–`E47C`, tilemap `00:F240`–`F452`. These write the VDC ports,
   which the write channel already carries.
3. **VBlank handler and pad reader** (15.3M). Needs the write channel widened
   again to reach the joypad port at `$1000` — the same bounded-register move
   made for the PSG.
4. **Gameplay tails** (~20M spread thin, no single address above 0.14%).
5. **Boot and reset path.** Off the census route's hot path but required.
6. **The frame-sync spin loop** (584.8M). Trivial code, but it is where the
   native frame boundary ends up living — do it with phase 3, not before.

### Phase 2 — native device models

- **PSG.** Accept `$0800`–`$0809`, model the six channels, DDA output, noise
  and LFO, and produce the `AudioView` float samples. Acceptance:
  `audio_sha256`. This is the larger of the two, because Beetle's PSG
  includes resampling to 44.1kHz that has to be matched bit-for-bit.
- **VDC.** Accept the VDC register file, maintain VRAM, the post-DMA SAT,
  palette and scroll, and produce a `VideoMemorySnapshot`. Acceptance:
  `framebuffer_sha256`. Much of this is already done: `src/render/` decodes a
  snapshot into pixels natively today. What is missing is the state machine
  that *maintains* the snapshot in response to port writes, plus the raster
  timing that decides when each line is sampled.

### Phase 3 — time and interrupts

- Timer reload and IRQ generation (`$0C00`/`$0C01`), interrupt mask and
  acknowledge (`$1402`/`$1403`).
- VDC/VBlank interrupt timing against the raster.
- Cycle pacing. Native transcripts currently declare `body_master_cycles` so
  Beetle can schedule around them; something must still model time to the
  same precision, because the game's behaviour depends on where the timer
  interrupt lands relative to the frame. This is the least obvious phase and
  the one most likely to be underestimated.
- The spin loop becomes the native frame boundary.

### Phase 4 — delete the CPU

Once every routine is native and phases 2–3 hold, there is no 6280 to
emulate. Bank mapping (`MPR`, `TAM`/`TMA`) collapses into ordinary ROM
addressing in native code. `RoutineReplacement`, `ReplacementCheckpoint`, the
hook machinery, the write channel, the replacement-nesting limitation and
most of `PceRuntime` all go away together.

## Open questions

- **Cycle model fidelity.** Phase 3 is the real risk. The census is
  cycle-sensitive: the `CLI` delayed-effect bug found in the sound driver
  moved a nested interrupt by nine master cycles and changed the census hash.
  A native time model has to be at least that precise.
- **Route coverage.** One 3,600-frame route is a strong oracle but not a
  complete one. Additional censused routes — particularly one that plays
  looping music, which the current route never does — should exist before
  phase 2 is declared correct.
- **Audio resampling.** Matching `audio_sha256` means matching Beetle's
  resampler exactly, or changing the oracle. Worth deciding early which.
- **Savestates.** `serialize`/`deserialize` are part of the contract and
  currently wrap Beetle's format.
