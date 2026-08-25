# Hybrid PCE Emulation and Native Game-Logic Port

## Summary

- Build a Windows x64 C++20/SDL3 application around an exact pinned baseline
  of [Beetle PCE](https://github.com/libretro/beetle-pce-libretro), providing
  HuC6280, memory, timing, VDC/VCE graphics, PSG audio, controller, and
  backup-memory emulation. Move that baseline into a project-owned fork
  before applying execution hooks. The combined Windows application is
  GPL-3.0-or-later after auditing the MSVC compatibility sources compiled
  from Beetle.
- Use [Mesen2](https://github.com/SourMesen/Mesen2) as an independent debugger and behavioral oracle.
- Initially run the game completely through emulation, then replace gameplay
  routines incrementally with native C/C++ while retaining emulated hardware,
  original graphics/audio drivers, menus, and compatibility glue.
- V1 supports only the existing USA ROM: MD5 `92fbe89d4c87bf283dbfbeac69a64e6e`. The user supplies this ROM and it remains required at launch.
- Preserve original gameplay by default. Native gameplay modules provide extension points for later physics, rules, scoring, and cabinet enhancements.
- Render every window at its monitor's current pixel resolution using one fixed sharp pixel-art scaling pipeline. No alternative filtering modes are included in v1.

## Hybrid Architecture and Interfaces

### Emulated compatibility layer

Fork Beetle PCE behind a narrow API:

```cpp
class PceRuntime {
public:
    RomResult loadRom(const std::filesystem::path&);
    void reset();
    EmulatedFrame runFrame(const InputFrame&);
    MemoryView workRam();
    CpuState cpuState() const;
    FramebufferView framebuffer() const;
    AudioBatch takeAudio();
    SaveState serialize() const;
    bool deserialize(const SaveState&);
};
```

The fork adds:

- Bank-qualified HuC6280 execution hooks.
- Read/write access to work RAM, VRAM, palettes, CPU registers, MPR state, and PSG commands.
- Frame, interrupt, controller-poll, and routine-entry callbacks.
- Bounded routine-call observers that capture callers, outer returns,
  input/output CPU state, absolute cycle budgets, and ordered work-RAM writes
  from the routine, nested callees, and interrupts.
- Deterministic savestates for native-vs-original differential tests.
- A manifest recording the upstream commit and local patches.

The following remain emulated in v1:

- HuC6280 scheduling, interrupts, timers, bank switching, and memory bus.
- VDC/VCE tile, sprite, palette, raster, and scrolling behavior.
- PSG and the original music/SFX driver.
- ROM bootstrap, title/options screens, legacy password/save UI, and low-level display/audio routines.

### Progressive native routine replacement

Address hooks by physical ROM bank plus logical PC.

```cpp
struct RoutineKey {
    uint8_t romBank;
    uint16_t logicalPc;
};

enum class HookMode {
    PassThrough,
    Observe,
    Replace
};

struct NativeCallResult {
    CpuRegisters registers;
    uint32_t emulatedCycles;
    ReturnBehavior returnBehavior;
    std::vector<GameEvent> events;
};
```

Each native module operates on typed views over emulated work RAM, retaining compatibility with original rendering and unported assembly.

Port these subsystems:

- Ball integration, velocity, gravity, flippers, launcher, and nudge/tilt.
- Collision lookup, priority, triggers, and response impulses.
- RNG and original seed/call ordering.
- Packed-BCD scoring, bonuses, multipliers, ball transformations, and extra balls.
- Main-table targets, enemies, gates, entrances, reset rules, and persistent state.
- All six bonus stages and their success, perfect, failure, and timeout paths.
- Player turns, ball count, fast/slow modes, drains, last chance, and maximum-score ending.

Boot, menu, presentation, and audio assembly remains unless a future enhancement requires replacing it.

A shadow harness restores the same pre-call savestate before each branch, runs
original assembly and native code sequentially, then compares registers, RAM
writes, cycles, events, and resulting frames. Beetle/libretro is
single-instance, so parallel comparisons must use isolated helper processes
or separately loaded core images rather than two ordinary `PceRuntime`
objects in one process.

### Enhancement-facing API

```cpp
class NativeGameLogic {
public:
    NativeCallResult invoke(
        RoutineKey,
        CpuRegisters,
        GameStateView,
        const LogicOptions&);
};

struct LogicOptions {
    FidelityMode fidelity = FidelityMode::Original;
};

struct GameSnapshot {
    Scene scene;
    PlayerState players[2];
    BallState ball;
    TableWorldState table;
    BonusState bonus;
    CameraState originalCamera;
};

struct FrameEvents {
    std::vector<ScoreEvent> score;
    std::vector<AudioCommand> audio;
    std::vector<SceneEvent> scene;
    std::vector<CabinetFeedbackEvent> cabinet;
};
```

`CabinetFeedbackEvent` exposes semantic flipper, nudge, bumper, target, drain, bonus, and extra-ball events. DOF/SSF output remains deferred.

## Rendering and Cabinet Behavior

### Fixed sharp pixel scaling

- Query each SDL window's drawable pixel size and recreate its backbuffer whenever resolution, DPI, orientation, or display assignment changes.
- Borderless fullscreen windows use the selected monitor's desktop resolution.
- The original PCE resolution is only a logical coordinate system and source texture. It is never presented as a fixed 256x240-sized output.
- Use one non-configurable sharp pixel pipeline:
  - Preserve logical pixel boundaries and pixel-center alignment.
  - Scale first to the largest useful integer multiple with nearest sampling.
  - Resolve any remaining non-integer fit into the monitor-sized destination with one controlled linear pass over the already enlarged texture, avoiding uneven pixel widths and conventional blur.
  - Disable mipmapping and texture smoothing outside this controlled final pass.
- Preserve aspect ratio:
  - Desktop and bonus scenes occupy the largest correct PCE-aspect rectangle.
  - The full table fits completely by height.
  - Remaining space receives a pure RGB 0,0,0 black frame.
- Do not provide nearest-only, conventional bilinear, CRT, scanline, smoothing, stretch, or crop options in v1.
- Render DMD text, setup UI, and framing directly at monitor resolution.
- Support landscape and portrait 1080p, 1440p, and 4K displays. See the [SDL3 window documentation](https://wiki.libsdl.org/SDL3/SDL_CreateWindow).

### Desktop profile

- Use the emulated VDC framebuffer, including original scrolling, menus, status screens, and bonus stages.
- Upscale the framebuffer to the current window or monitor resolution with the fixed sharp scaler.
- Native logic replacements continue feeding the RAM and queues expected by original rendering and audio code.

### Cabinet profile

Use three independently positioned borderless SDL windows:

1. **Playfield**
   - During menus and non-gameplay screens, show the upscaled emulated framebuffer.
   - During normal play, show a native full-table renderer.
   - During bonus stages, replace the full table with the sharply upscaled emulated bonus-stage framebuffer.
   - Restore the full table when the bonus stage ends.

2. **DMD**
   - Render the score as the dominant, unlabelled value, with smaller Ball
     and Bonus fields sharing a second native text row below it.
   - Use the cartridge's original 8-by-8 alphanumeric font for every label
     and value, decoded from its resident ROM-to-VRAM stream at startup.
   - Show a P1/P2 discriminator only in two-player mode.
   - Continue updating during bonus stages.

3. **Backglass**
   - Persistently display a native ROM-derived animation of the title-screen
     Devil's Crush logo.
   - Preserve a user-selected PNG, JPEG, or WebP as a fallback if native logo
     decoding or rendering fails.
   - Scale both paths with aspect-preserving containment and the same sharp
     scaling policy.

The full-table renderer will:

- Decode the three streamed table regions, tiles, sprites, palettes, compression, and animation metadata into a local cache derived from the ROM.
- Establish one world coordinate system for the complete table.
- Render actors from `GameSnapshot`, independent of the original camera.
- Preserve original camera-dependent activation and collision rules.
- Show inactive mechanisms in their correct persistent or dormant state without advancing additional gameplay logic.
- Validate each table-region crop against its equivalent emulated camera view.

Provide an interactive monitor-assignment screen and persist display identity, geometry fallback, rotation, and fullscreen state. Missing DMD or backglass monitors must not prevent play.

## Delivery Sequence

Each milestone below states its scope and its exit gate only. Current status is
in [PROGRESS.md](PROGRESS.md); the dated record of how each gate was met is in
[docs/PORT_HISTORY.md](docs/PORT_HISTORY.md) and
[docs/PROGRESS_ARCHIVE.md](docs/PROGRESS_ARCHIVE.md).

### 1. Emulated Windows baseline — complete

- Establish CMake, SDL3, Windows CI, ROM exclusion, GPL notices, and the pinned Beetle fork.
- Run the unmodified game with emulated video, audio, input, backup RAM, and savestates.
- Implement monitor-resolution output and the fixed sharp scaler.
- **Exit:** canonical recordings match Beetle and Mesen2 results.

### 2. Instrumentation and hook bridge — complete

- Add execution hooks, memory inspection, cycle accounting, trace export, and pass-through/observe/replace modes.
- Map ball, velocity, camera, score, RNG, actors, scenes, and audio queues.
- Implement the transactional native-invocation boundary, the explicit
  interrupt policy, and the native-vs-original shadow path, then prove the
  bridge end-to-end with one bounded routine before resuming mapping.
- Script every behavior documented in the [USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
  and classify all of it in [docs/COVERAGE_MANIFEST.md](docs/COVERAGE_MANIFEST.md).
- **Exit:** every required state is reproducible, routines can be shadow-tested,
  and at least one faithful routine runs in replacement mode.

### 3. Cabinet preview — in progress

- Decode and stitch the three table backgrounds into one world coordinate
  system. The accepted world is 256-by-668 at source rows 20-687 of the packed
  256-by-768 envelope; rows 0-19 are the top guard, 688-720 are streamed lower
  prefetch, and 721-767 are unused tail. See
  [docs/TABLE_BACKGROUND.md](docs/TABLE_BACKGROUND.md).
- Add the playfield, score DMD, backglass, display assignment, and
  bonus-stage windows described under **Cabinet profile** above. See
  [docs/CABINET_PREVIEW.md](docs/CABINET_PREVIEW.md).
- Render every mapped actor over that background from copied SAT and
  ROM-derived descriptor metadata, retaining fixed-mechanism visuals after the
  camera leaves them.
- Prioritize a populated, animated, playable stitched table over cabinet
  polish; use emulated logic and observed RAM until Milestone 4 replaces it.
- **Exit:** the entire table is visible, equivalent camera crops match the
  emulator, and missing auxiliary monitors degrade gracefully.

### 4. Native gameplay port — in progress

- Keep application orchestration separate from instruction-faithful
  replacement adapters. Adapters live in subsystem implementation files;
  `native_replacements.cpp` is the central registry.
- Port RNG, physics, collision, table actors and rules, bonus stages, and
  session transitions.
- For each routine: observe, document, shadow-test, then replace, following
  [docs/NATIVE_PORT_WORKFLOW.md](docs/NATIVE_PORT_WORKFLOW.md). Admission
  requires a natural route or player capture that gives the scope its own
  caller and return — see [docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md).
- Maintain [docs/NATIVE_REGISTRY.md](docs/NATIVE_REGISTRY.md) as the manifest
  of admitted routines, and
  [docs/RETAINED_ASSEMBLY_SEAMS.md](docs/RETAINED_ASSEMBLY_SEAMS.md) as the
  manifest of code deliberately left emulated.
- **Exit:** all listed gameplay subsystems are native and remaining ROM
  execution is limited to approved compatibility glue.

### 5. Final cabinet world model — not started

- Complete the typed `TableWorldState`.
- Render every table region and mechanism correctly while preserving original simulation behavior.
- Add versioned native save containers holding Beetle state, ROM/core identifiers, and native extension state.
- **Exit:** desktop and cabinet profiles use identical native gameplay state without divergence.

### 6. Release hardening — not started

- Finish remapping, volume, pause, high scores, frontend launch/exit, logging, display recovery, and portable Windows packaging.
- Publish a source-complete GPL-3.0-or-later Windows x64 release containing no ROM, extracted assets, box art, traces, or generated disassembly.

## Tests and Acceptance Criteria

### Native-port iteration cadence

Milestone 4 routine work follows [the native-port workflow](docs/NATIVE_PORT_WORKFLOW.md).
Each slice requires an incremental Release build, ROM-free core coverage in
both configurations, and one owning-family Release ROM regression. Related
sibling routines should share that family route and artifact rather than add
duplicate full-route tests. The full Release suite is the family, milestone,
and cross-family checkpoint; it runs in parallel in under three minutes, so it
is not narrowed.

Persist authoritative assembly calls with `--routine-replay-corpus-out` when
a family route is first recorded. During adapter development, replay one or
more `.dcrr` fixtures with `--routine-replay-corpus` and semantic `@all`
replacement hooks; this restores directly at the call and compares registers,
return state, relative cycles, ordered writes, and post-call work RAM without
traversing the route. Re-run the owning-family route for aggregate admission,
artifact, and coverage proof before closing the slice.

Debug ROM regressions are deliberately exceptional. The ROM-free Debug tests
carry what a Debug build catches that a Release build cannot, and they cover
the native models where such a fault would live. A Debug ROM route adds only
the claim that an artifact is byte-identical across configurations, so it is
required for one owning family when a new artifact golden is first recorded,
when shared transaction/checkpoint/interrupt/save-state machinery changes, or
when an unexplained Release artifact movement raises undefined behavior as a
suspect. The full Debug sweep is a release-checkpoint and diagnostic tool.

This cadence changes validation scheduling only: every admitted replacement
still requires original-assembly parity for its complete observable behavior,
and a Debug route that does run must reproduce the Release artifact exactly.

- Verify ROM hashes, asset decompression, table stitching, palettes, display transforms, sharp-scaler math, and save versioning.
- In pure-emulation mode, compare replay RAM hashes, framebuffer CRCs, audio timing, and scene transitions against Mesen2.
- For every native routine, compare:
  - Registers and flags.
  - RAM writes and ordering.
  - RNG and score state.
  - Collision IDs and ordering.
  - Cycle budget.
  - Audio and scene events.
- Require zero unexplained semantic differences between all-emulated and native-replacement faithful modes.
- Cover both speeds, both player counts, all table regions and mechanisms, all six bonus stages and outcomes, tilt, password/data load, five saves, drains, extra balls, last chance, known stuck-ball behavior, and the `999,999,900` ending.
- Confirm identical simulation at 60, 120, and 144 Hz output refresh rates.
- Validate sharp scaling at common desktop and cabinet resolutions:
  - Backbuffer equals actual monitor pixels.
  - Content occupies the maximum aspect-correct area.
  - Logical pixels remain crisp and consistently aligned.
  - No conventional blur, stretch, crop, mipmapping, or unintended smoothing appears.
- Cabinet acceptance requires:
  - Entire table visible without scrolling.
  - Equivalent crops matching original views.
  - Bonus stages replacing the playfield view.
  - Score remaining live on the DMD throughout frontend and gameplay states.
  - Native animated title logo remaining live on the backglass.
  - Auxiliary windows never stealing controls.
  - Monitor reorder or loss degrading gracefully.

## Assumptions and Constraints

- The USA ROM is the only supported v1 image and must be available at launch.
- Beetle PCE remains permanently as the hardware compatibility layer.
- Sharp pixel scaling is the only supported v1 scaler and is always enabled.
- V1 gameplay defaults remain original; altered physics, balance, or rules are deferred.
- Cabinet output targets three LCD monitors. Physical DMDs, DOF, SSF, online services, remastered art, and Japanese-ROM support are deferred.
- The combined application uses GPL-3.0-or-later terms because Beetle's MSVC build includes `mednafen/msvc_compat.h` under GPL-3.0-or-later.
- Conduct an IP review before publicly releasing the reverse-engineered port.
