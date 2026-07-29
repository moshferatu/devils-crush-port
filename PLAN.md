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
  - Remaining space receives a neutral dark frame.
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
   - Render the active player's score natively at full monitor resolution.
   - Show a P1/P2 discriminator only in two-player mode.
   - Continue updating during bonus stages.

3. **Backglass**
   - Persistently display a user-selected PNG, JPEG, or WebP.
   - Scale it with aspect-preserving containment and the same sharp scaling policy when pixel art is supplied.
   - Fall back to black when no image exists.

The full-table renderer will:

- Decode the three streamed table regions, tiles, sprites, palettes, compression, and animation metadata into a local cache derived from the ROM.
- Establish one world coordinate system for the complete table.
- Render actors from `GameSnapshot`, independent of the original camera.
- Preserve original camera-dependent activation and collision rules.
- Show inactive mechanisms in their correct persistent or dormant state without advancing additional gameplay logic.
- Validate each table-region crop against its equivalent emulated camera view.

Provide an interactive monitor-assignment screen and persist display identity, geometry fallback, rotation, and fullscreen state. Missing DMD or backglass monitors must not prevent play.

## Delivery Sequence

### 1. Emulated Windows baseline

- Establish CMake, SDL3, Windows CI, ROM exclusion, GPL notices, and the pinned Beetle fork.
- Run the unmodified game with emulated video, audio, input, backup RAM, and savestates.
- Implement monitor-resolution output and the fixed sharp scaler.
- Exit when canonical recordings match Beetle and Mesen2 results.

### 2. Instrumentation and hook bridge

- Add execution hooks, memory inspection, cycle accounting, trace export, and pass-through/observe/replace modes.
- Map ball, velocity, camera, score, RNG, actors, scenes, and audio queues.
- Continue the active bonus-stage mapping through BS-3's first
  event-specific rule family and its completion and timeout outcomes, then
  preserve that work as a deterministic checkpoint. **Completed
  2026-07-27.**
- After that checkpoint, pause expansion into the remaining bonus-stage
  mappings and implement the transactional native-invocation boundary,
  explicit interrupt policy, and native-vs-original shadow path.
  **Completed for bounded no-interrupt routines 2026-07-27; interrupt-bearing
  replacement remains a later bridge extension.**
- Prove the bridge with one already mapped, bounded routine: run the native
  and original implementations from the same pre-call state, compare their
  complete observable results, and then execute the native implementation in
  replacement mode while unported assembly continues normally. **Completed
  with `00:F241@1` 2026-07-27.**
- Resume the remaining normal-table and bonus-stage mappings after the first
  end-to-end replacement proof. **BS-4's first event family and its
  completion/timeout outcomes and BS-5's primary objective field and result
  paths were completed 2026-07-27. BS-6's objective pair and result paths
  are also complete, so every bonus stage now has a first bounded family.
  BS-4's two linked-target RNG consumers are now dynamically bounded, leaving
  four bonus-bank RNG consumers. BS-6's paired hazard RNG consumers are also
  dynamically bounded, leaving two bonus-bank consumers. Remaining bonus
  outcome variants and the final two maximum-score-ending RNG consumers were
  the active mapping checkpoint. The ending pair is now dynamically bounded,
  so all nine bonus/ending-bank RNG consumers have deterministic coverage.
  The ordinary scored-but-incomplete result is now dynamically bounded for
  all six stages. Normal-table consumer `02:597D` is now dynamically bounded
  through the upper Wall-Pocket hand's live timer setup, leaving nine
  normal-table RNG consumers. The upper Wall-Pocket hand's post-close
  response now dynamically bounds `02:59F5`, and the naturally activated
  Gray-Blob row bounds `03:68B1`, and the Three-Blobs equal-color reset
  bounds `03:6620`. The Little-Skull gray-release route now bounds
  `02:45C9`, and the Sorcerer zero-live controller bounds `02:57C2`, leaving
  four normal-table RNG consumers. The Undead-Head post-trigger selector now
  bounds `03:625D`, leaving three. A single combined route now bounds the
  final Devil-Shooter, Undead-Head launch, and Giant-Face mouth-release
  consumers at `02:5148`, `03:6177`, and `03:7834`, so all 12 normal-table
  RNG consumers have deterministic dynamic coverage. BS-2 now has a
  full-contact perfect route through its collision scan, post-collision
  continuation, and all 28 board cells, while BS-1 and BS-3 through BS-6
  have natural perfect routes.
  These routes drive the original objective resolution, controllers,
  completion animation, gameplay-owned `$2DBA` latch, result presentation,
  and both earned and special score transfers without seeding objective or
  result state. The deterministic state-census route now connects Button-I
  Devil-Shooter charge/release, both flipper inputs, and all three Button-II
  nudges to the mapped slot-0 motion and integration state; `$2CC7` remains
  clear throughout, closing the controls-and-physics gate. Two natural
  Small-Soldier contacts now connect the confirmed-contact dispatcher and
  ball response to collision-table processing, actor state, score, audio,
  group state, and complete actor release, closing the collision-boundary
  gate. The ordinary drain route now connects the live-ball threshold,
  session decrement, remaining-ball decision, selective table reload, stored
  score, and native score-panel digits. It proves the visible score is the
  binary stored score times 100 and closes the session-and-scoring gate. The
  finite dynamic/static/deferred coverage manifest classifies all 105
  behavior units, including every software behavior in the USA manual, and
  closes Milestone 2.**
- Script all behaviors documented in the [USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf).
  **Milestone 2 inventory and classification completed 2026-07-28; deferred
  variants remain assigned to Milestones 4 and 6.**
- Exit when every required state is reproducible, routines can be
  shadow-tested, and at least one faithful routine has run successfully in
  replacement mode. **Completed 2026-07-28.**

### 3. Cabinet preview

- Decode and stitch the three table backgrounds.
  - The copied frame-boundary VDC/VCE snapshot boundary, HuC6270 BAT/planar
    tile/VCE-color decoder, world-coordinate compositor, crop helper, and
    first live table goldens were completed 2026-07-28.
  - `00:F400` and its four directional caller paths are now dynamically
    guarded, and their ROM/work-RAM source-address and circular-BAT mapping
    is established.
  - The first 256-by-768 export used frame 900 while the table palette was
    still fading in and treated the entire 96-row source envelope as visible
    world data. It is a deterministic diagnostic, not a completed
    playfield.
  - Stable upper, middle, and lower captures, both camera clamps, and
    independent Beetle-composite comparisons now establish the accepted
    256-by-668 world at source rows 20-687. Rows 0-19 are the top guard,
    688-720 are streamed lower prefetch, and 721-767 are unused tail data.
    The background task and its sprite/actor overlay-input definition were
    completed 2026-07-28; sprites and gameplay-owned mechanisms remain a
    separate cabinet-renderer layer.
- Add playfield, score DMD, backglass, display assignment, and bonus-stage
  switching.
  - The first `--cabinet` vertical slice now creates non-focusable DMD and
    backglass auxiliaries, renders the accepted full-table background during
    normal sessions, restores the emulated framebuffer for menus and bonus
    stages, displays the live score, and degrades to playfield-only operation
    when an auxiliary closes or fails.
  - Display identities, window geometry fallbacks, current monitor rotation,
    and fullscreen state are now persisted atomically. The F12 assignment
    screen pauses emulation, color-identifies the three roles, cycles each
    role across live displays, and saves the result. Exact identity,
    saved-geometry overlap, and primary-display fallback make monitor reorder
    or loss deterministic.
  - PNG, JPEG, and WebP backglass loading is now complete through the pinned
    static SDL_image 3.4.4 stack. `--backglass` supplies a scripted choice;
    B in the F12 assignment screen opens the native picker; Delete restores
    the black fallback. Images are content-validated, bounded, persisted,
    and aspect-contained through the fixed sharp scaler.
  - The copied post-DMA HuC6270 SAT now supplies the live camera's sprite
    overlay in table-world coordinates with hardware background priority and
    the 16-sprite-half scanline limit.
  - Typed views now decode all 127 mapped work-RAM actor slots. Fixed
    mechanisms resolve exact bank-8 ROM descriptor groups rather than
    proximity-associating nearby SAT entries. Work-RAM coordinates and
    phases remain live off-camera, while the copied SAT is composited last
    and stays authoritative for camera-observed actors. Retained primitives
    own decoded pattern pixels and palette indices, so unrelated
    camera-region VRAM cannot corrupt them while the current VCE palette
    still completes the table fade correctly.
  - Bank `$08`'s actor renderer, 69 appearance tables, phase-selected
    descriptors, and `$7800-$78FF` SAT staging boundary are mapped. The
    native metadata decoder matches every eligible fixed-actor group in the
    stable lower-table SAT. Generic stream decoder `00:E41F` and the 14
    physical-bank `$12` streams used by the lower-to-upper-middle transition
    reproduce VRAM `$4000-$77FF` exactly. Selector `00:F606` ties that
    transition to actor slot, appearance, phase, descriptor, and asset
    directory state. All 42 selector entries now decode into a local atlas:
    eight upload descriptors provide 35 compressed phase assets, while
    appearances `$24-$29` share the `$F768` no-per-phase sentinel. All five
    scene-resident `$F768` packs are decoded; selector 0
    (`$1222->$7C00`) exactly matches live normal-table VRAM, but none of its
    `$7C00` blocks owns those fixed actors' descriptor patterns. The separate
    raw-upload code path is also bounded and ROM-free tested.
    Stream-backed entries and upload-disjoint static VRAM blocks are now
    applied lazily to bank-8 actor metadata. At settled lower frame 1,200,
    all 61 non-hidden fixed actors have an exact source: 23 are represented
    by the live camera and 38 are pre-seeded across the rest of the table.
    Appearance/phase changes re-decode those actors each frame, and accepted
    lower, upper-middle, and middle persistent composites have deterministic
    goldens. Scene association for `$F768` selectors 1-4 is no longer a
    fixed-actor composition gate.
  - Shared type-`$8F` impact bursts are now the first classified table-world
    transient. Their `$4290->$42B0` activation/animation pair, appearance
    `$10`, original phase/lifetime, two exact bank-8 primitives, and
    upload-disjoint pattern source are guarded at frame 2,100. All other
    dynamic effects remain live-camera-only by default, and cleared or reused
    slots invalidate retained pixels immediately.
  - Prioritize a populated, animated, playable stitched table before cabinet
    UI refinements:
    - [complete] Keep the live ball trail-free and all six flipper/control
      slots visible and animated at their absolute table-world positions.
    - [complete] Track persistent dynamic enemies in slots `$62-$7E` across
      camera movement and retain their animation phases without duplicating
      or trailing reused slots.
    - [complete] Classify transient SAT effects separately from persistent
      enemies and mechanisms; retain only the mapped table-world impact
      family and keep every other effect live-camera-only.
    - [complete] Validate fixed mechanisms, all six flippers, ten Small
      Soldiers, and four Tower insects in upper, middle, and lower table
      bands on a deterministic playable route.
    - [complete] Remove the live-camera cache hole and nearest-actor
      association so Sorcerers, star/pentagram mechanisms, and flippers stay
      aligned and visible as the camera changes regions.
    - [complete] Normalize actor presentation Y past the 30-line score band,
      keep the stitched background at one zero-offset world origin across
      launch/camera transitions, and resolve retained palette indices against
      the current VCE palette instead of freezing fade-in silhouettes.
    - [complete] Preserve a bounded set of exact per-phase visuals for
      animated actors. Off-camera enemies select the ROM-owned or learned
      visual matching their live phase; only static mechanisms may retain a
      prior aligned visual when a phase has no independent artwork. Never
      rebuild an actor from another region's live VRAM.
    - [deferred at user request] Map the two-player/current-player RAM
      contract, then add the P1/P2 DMD discriminator.
- Initially use emulated logic and render active actors from observed RAM.
- Exit when the entire table is visible and equivalent camera crops match the emulator.

### 4. Native gameplay port

- Begin from the replacement-mode routine proven during Milestone 2 and use
  it as the reference vertical slice for later ports. **Milestone 4 began
  2026-07-28 with faithful `00:F1B4@17` position integration beside the
  existing `00:F241@1` rollback replacement. Those focused fixtures now
  support semantic `00:F1B4@all` integration across positive/negative and
  fractional/integer-step states plus semantic `00:F241@all` rollback.
  Automatic event/IRQ fallback admits 214 of 1,156 integration calls and 242
  of 404 rollback calls. The current slice also includes the
  bounded `01:DD09@1` zero-component acceleration path, its interrupt-free
  `01:DD09@2` positive nonzero-divider path, the interrupt-free negative
  divider at `01:DD09@181`, and interrupt-free `01:D1A2@23` signed rotation,
  plus `03:6966@1` forward, `03:6966@2` reverse-horizontal, and
  `03:6966@3` and `03:6966@4` vertical full collision-table record
  advancement, plus `03:6966@9`'s alternate-selector reverse-horizontal
  full record and `03:6966@10` and `03:6966@12` headerless vertical
  continuations plus `03:6966@19`'s alternate-selector reverse-vertical
  continuation, `03:6966@20`'s horizontal continuation, and
  `03:6966@21`'s reverse-horizontal continuation plus `03:6966@22`'s
  later vertical continuation and `03:6966@23`'s matching second
  later vertical continuation as restored-assembly fixtures,
  `03:6966@5` through `03:6966@8` collision-table alternate-pass selection
  plus `03:6966@14`'s alternate-selector and `03:6966@15` through
  `03:6966@18`'s repeated fixed-selector early returns. The ordinal fixtures
  now support semantic `03:6966@all` scheduling: selector pointers come from
  the verified user ROM, any slot whose pass state is `$81` takes the native
  four-write early return, and ordinary `$80->$81` headerless record steps
  resolve their collision-list and motion cursors from ROM. The generic
  transaction handles either or both signed axes, including low-byte
  carry/borrow into the high byte. Core-provided user/video-event and
  serviceable-timer-IRQ horizons conservatively fall back to assembly while
  allowing harmless timer-divider ticks. The bounded walker also follows
  `$C0` motion terminators, applies `$FE` table wraps, recognizes `$FF`
  termination, and handles headered records. This replaces 7,609 of the
  route's 19,648 calls (38.7%) without an ordinal allowlist; further work on
  this unusually large interpreter is frozen while smaller routines are
  ported,
  semantic `03:7FB1@all` actor collision-response scheduling across any
  slot, response, countdown, and phase. Its ordinary decrement and reload
  branches replace 13,460 of 21,987 census calls (61.2%); the existing
  `@1`, `@3`, and `@7` coordinates remain focused restored-assembly
  fixtures. Calls near an interrupt—including focused `03:7FB1@2`—fall
  back automatically. `01:DD09@52` and unimplemented `03:6966` semantic
  paths remain emulated.**
- Port RNG, physics, collision, table actors/rules, bonus stages, and session transitions.
- For each routine: observe, document, shadow-test, then replace.
- Maintain a manifest showing which active-game ROM ranges remain emulated.
- Exit when all listed gameplay subsystems are native and remaining ROM execution is limited to approved compatibility glue.

### 5. Final cabinet world model

- Complete the typed `TableWorldState`.
- Render every table region and mechanism correctly while preserving original simulation behavior.
- Add versioned native save containers holding Beetle state, ROM/core identifiers, and native extension state.
- Exit when desktop and cabinet profiles use identical native gameplay state without divergence.

### 6. Release hardening

- Finish remapping, volume, pause, high scores, frontend launch/exit, logging, display recovery, and portable Windows packaging.
- Publish a source-complete GPL-3.0-or-later Windows x64 release containing no ROM, extracted assets, box art, traces, or generated disassembly.

## Tests and Acceptance Criteria

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
  - Score remaining on the DMD.
  - Backglass remaining unchanged.
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
