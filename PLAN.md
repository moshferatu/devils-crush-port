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
  **Completed for bounded no-interrupt routines 2026-07-27. Instruction-
  checkpointed mid-routine interrupt delivery completed 2026-07-29.**
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
    stages, displays a dominant live score above current-ball/bonus status on
    the DMD and a native ROM-derived
    animated logo on the backglass, and
    degrades to playfield-only operation when an auxiliary closes or fails.
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

- Keep application orchestration separate from instruction-faithful
  replacement adapters. **Completed initial extraction 2026-07-30: the
  11,857-line adapter/dispatcher block now lives behind
  `app/native_replacements.hpp`, reducing `src/app/main.cpp` from 16,939 to
  5,077 lines without changing replacement behavior. Further adapter growth
  is assigned to subsystem-specific implementation files rather than
  `main.cpp`. Completed 2026-07-30: collision, physics/arithmetic,
  controls/actor, and rules/RNG/score adapters now occupy subsystem
  implementation files, while `native_replacements.cpp` is the 555-line
  central registry.**
- Begin from the replacement-mode routine proven during Milestone 2 and use
  it as the reference vertical slice for later ports. **Milestone 4 began
  2026-07-28 with faithful `00:F1B4@17` position integration beside the
  existing `00:F241@1` rollback replacement. Those focused fixtures now
  support semantic `00:F1B4@all` integration across positive/negative and
  fractional/integer-step states plus semantic `00:F241@all` rollback.
  Path-sensitive 50-57-boundary integration transcripts admit all 1,156
  calls, including 250 original interrupt-bearing scopes, while a
  12-boundary transcript admits all 404 rollback calls. The current slice
  also includes the
  bounded `01:DD09@1` zero-component acceleration path, its interrupt-free
  `01:DD09@2` positive nonzero-divider path, the interrupt-free negative
  divider at `01:DD09@181`, and the focused interrupt-free
  `01:D1A2@23` signed-rotation fixture,
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
  carry/borrow into the high byte. The alternate-pass branch now carries all
  17 instruction checkpoints and retains original interrupt-handler
  passages; uncheckpointed paths use core-provided user/video-event and
  serviceable-timer-IRQ horizons to fall back conservatively while allowing
  harmless timer-divider ticks. The bounded walker also follows
  `$C0` motion terminators, applies `$FE` table wraps, recognizes `$FF`
  termination, and handles headered records. Exact 50-boundary transcripts
  cover the generic forward/reverse one-axis headerless family, and the same
  state-based transcript extends to 55 boundaries when both axes move
  without a high-byte carry or borrow. This replaces 18,806 of the route's
  19,648 calls (95.7%) without an ordinal allowlist; further work on
  this unusually large interpreter is frozen while smaller routines are
  ported,
  semantic `03:7FB1@all` actor collision-response scheduling across any
  slot, response, countdown, and phase. Its ordinary decrement and reload
  branches replace all 21,987 census calls through 11/14-boundary
  transcripts, including 1,042 calls containing original interrupt-handler
  passages. The existing `@1`, `@3`, and `@7` coordinates remain focused
  restored-assembly fixtures; `@2` is the aggregate interrupt-bearing
  coordinate. Semantic `03:7F57@all` composes any admitted
  `03:6966` transaction and either `03:7FB1` response branch with the
  caller's local no-hit exit. Full-record writes are staged into a private
  snapshot before response derivation. The checkpointed alternate-pass,
  one-axis, and bounded two-axis compositions include both parent JSR/RTS
  pairs and replace 20,113 of 20,867 Small-Soldier consumer calls (96.4%)
  from live state, while
  preserving the exact complete 20,867-call/362,060-write artifacts. Semantic
  `00:F55C@all` also handles ordinary
  unsaturated score additions below the current high score. Its 44-boundary
  transcript replaces all 40 census calls with exact 24-bit result, nested
  call-stack, flag, write, timing, and original-interrupt parity. Semantic
  `00:E8B3@all` advances the free-running RNG seed from
  live seed/frame state. Its 23-boundary transcript replaces all 78 census
  calls with an exact eight-write, 282-cycle transaction. Semantic
  `00:F4FC@all` classifies every occupied actor against the signed camera
  window and updates its visibility flag. Its 2/18/31-boundary transcripts
  replace all 65,927 census calls, including 4,858 original
  interrupt-handler passages, through exact 27-333-cycle transactions.
  Semantic `01:C574@all` clears any legal actor slot across all 32 parallel
  state arrays. Its generated 361-boundary loop transcript replaces all 132
  census calls, including 35 original interrupt-handler passages, through an
  exact 99-write, 3,627-cycle transaction.
  Semantic `01:D466@all`
  evaluates the global input and indexed flipper-piece mask, then handles
  both the 36-cycle no-input exit and common 282-cycle activation/coroutine
  handoff. Its 3/21-boundary transcripts replace 7,468 of 7,470 calls
  (99.97%) with exact register, flag, call, and write parity. The two
  remaining current-slot calls enter `D525` and emit 823/868 ordered writes,
  so they remain emulated under that controller's bounded-decomposition
  limit. Semantic `01:D500@all`
  advances forward and reverse flipper phases when their indexed timer
  expires. Its 9/11-boundary transcripts replace all 980 route calls through
  exact three-write, 126/138-cycle transactions.
  Semantic `01:D081@all` resolves the flipper/ball interaction's two
  phase-indexed geometry vectors from the verified user ROM. Its 14-boundary
  transcript now replaces all 540 calls, including 38 interrupted calls,
  through an exact five-write, 159-cycle native body while Beetle executes
  the original interrupt handlers.
  Semantic `01:C5C9@all` replaces the shared actor-coroutine scheduler from
  live stack, slot, and delay state. Its immediate-install,
  decrement-and-yield, and decrement-to-install branches now carry
  instruction-boundary transcripts and replace all 2,462 calls through
  exact 102-192-cycle transactions while Beetle executes any original
  interrupt handlers.
  Semantic `01:C3CB@all` covers the session-state, ordinary
  timer-decrement/no-Button-II, timer-reload, active-nudge start, and
  phase-finish paths, replacing all 2,649 calls through
  8/17/20/66/44-boundary transcripts. The active paths use a bounded
  replacement bus-write contract for the base VDC ports and retain their
  exact VDC wait cycles. Semantic `01:C49E@all`
  handles the adjacent active-player no-Run-input return, replacing all 2,649
  calls with its seven-boundary, zero-write, 72-cycle transcript while pause,
  high-score, and session-transition paths remain emulated. Semantic
  `02:42F6@all` handles the Dragon's-Egg group controller's nonzero-count
  return from live count `$2CF4`. Its two-boundary, zero-write, 21-cycle
  transcript replaces all 1,790 current route calls, including 17 original
  interrupt-handler passages, while the count-zero award and reset sequence
  remains emulated. Semantic `02:4345@all` handles all 9,857 Dragon's-Egg
  hit checks. Idle calls retain their two-boundary, zero-write, 21-cycle
  return; the routed hit installs the 10-unit score arguments through an
  exact 57-cycle, two-write handoff. Shared score/audio work remains
  emulated, semantic `02:4357@all` then disables the egg and installs the
  `$39` impact/allocation parameters through 93 cycles and four writes, and
  semantic `02:4371@all` activates the fixed slot after the original burst
  and hatchling allocators. The complete outer 1,682-write artifact and all
  original interrupt passages remain exact.
  Semantic `02:445B@all` handles the hidden Big-Star feature controller's
  clear `$2CFA` latch return. Its two-boundary, zero-write, 21-cycle
  transcript replaces all 2,650 current route calls, including 28 original
  interrupt-handler passages, while the pending Little-Skull direction and
  scoring update remains emulated. Semantic `03:6339@all` handles the
  Walled Star's ordinary no-hit return from indexed collision state
  `$3800,X` and shared access latch `$2DBC`. Its four-boundary, zero-write,
  54-cycle transcript replaces all 2,650 current route calls, including 49
  original interrupt-handler passages, while wall damage and active
  post-destruction access remain emulated. Semantic `03:60C3@all` handles
  all 2,646 fixed-Tower hit checks. Idle calls retain their two-boundary,
  zero-write, 27-cycle transcript; the routed hit clears its collision latch
  in a 63-cycle native handoff. Shared score/audio work remains emulated,
  then semantic `03:60E0@all` advances Tower progress and hands the
  interrupt-bearing shared dispatcher to assembly. Semantic `03:611C@all`
  composes the native coroutine scheduler for all four transition calls.
  The complete outer 538-write artifact and all original interrupt passages
  remain exact. Semantic `02:5A4B@all` handles all 2,144 Dragon's-Head
  collision checks through exact 27-cycle idle or 42-cycle collision-clear
  transcripts. Shared score/audio/bonus/appearance helpers remain emulated,
  then semantic `02:5A6E@all` performs both routed 27-cycle, one-write
  nonterminal counter decrements. The eighth-hit mouth-opening branch
  remains emulated, while the complete outer 866-write artifact stays
  exact. Semantic `03:78C7@all` handles all 3,468 fixed-Bumper checks:
  ordinary calls retain their two-boundary, zero-write, 27-cycle return,
  while all 11 routed hits clear the collision latch, advance the nine-step
  phase, and dispatch to the original handler through an exact 165-cycle,
  four-write native prefix. Shared score/audio/appearance/scheduler helpers
  remain emulated. Semantic `03:78F5@all` performs all nine routed primary
  progress increments in 36 cycles and one write, while `03:7912@all`
  performs both routed secondary damage updates in 114 cycles and three
  writes. The complete outer 3,127-write artifact remains exact; the
  unobserved ninth-hit destruction award and six-child release remain
  emulated. Semantic
  `02:5116@all` handles all 125 fixed Devil Shooter collision-state checks:
  124 idle calls return through the exact two-boundary, zero-write,
  27-cycle path, while the catch takes the exact 21-cycle branch into the
  original occupied-latch and shared `00:C087` helper. After that helper
  performs the assembly-owned ball positioning/audio work and its interrupt,
  semantic `02:52B8@all` executes the complete 228-cycle, nine-write
  catch-state/coroutine continuation natively and installs `02:52C3`.
  Semantic
  `02:54B9@all` handles every left Skeleton Insect trigger check from
  Front-Spider latch `$2D13`. Its two-boundary, zero-write, 21-cycle idle
  transcript covers 1,630 calls, while the 978-cycle routed trigger composes
  the native score accumulator and coroutine scheduler to award 4,000
  points and start the opening animation. All nine original interrupt-
  handler passages and the complete 1,631-call/150-write artifacts remain
  exact. Semantic `02:5551@all` handles every right Skeleton Insect trigger
  check from Front-Spider latch `$2D14`. Its two-boundary, zero-write,
  21-cycle idle transcript covers 335 calls, while the 978-cycle routed
  trigger composes the native score accumulator and coroutine scheduler to
  award 4,000 points and start the opening animation. All six original
  interrupt-handler passages and the complete 336-call/101-write artifacts
  remain exact. Semantic `02:55F9@all` handles all 2,403 center
  Skeleton-Insect trigger checks from Tower latch `$2CFC`: 2,402 clear
  calls return after the exact two-boundary, zero-write, 21-cycle path, and
  the nonzero call takes the exact 27-cycle branch into the original shared
  drawing helper. After that helper returns, semantic `02:5604@all`
  executes the complete 1,152-cycle, 30-write continuation natively,
  composing the score accumulator and both coroutine helpers for the
  10,000-point award, presentation-bit changes, delayed opening, and
  `02:561A` loop toward `02:5625`. All 23 interrupt-bearing idle calls and
  the trigger's assembly-owned interrupt remain exact. Semantic
  `03:63F1@all` handles the Devil's Coffin's ordinary return,
  Shooter-session type refresh, and both routed collision-entry paths. Its
  five/seven/21/23-boundary, zero/one/eight/nine-write,
  51/66/270/285-cycle transcripts replace all 643 current route calls,
  including all 13 original interrupt-handler passages. The collision
  entries compose the existing native coroutine scheduler to install
  `03:640A`.
  Semantic `03:640A@all` now advances that presentation boundary by
  handling the opening pass's timer ticks, appearance advances, and final
  coroutine handoff through the existing native scheduler. Its
  two/eleven/28-boundary, one/four/12-write, 33/132/366-cycle transcripts
  replace all 18 routed opening calls.
  Semantic `03:6445@all` applies the same bounded contract to the complete
  closing pass. Its terminal path clears the session-open bit, restores
  type `$82`, runs initializer `03:63E1`, and composes the native scheduler
  to reinstall steady dispatch `03:63F1`. Its two/nine/34-boundary,
  one/four/15-write, 33/111/429-cycle transcripts replace all 18 routed
  closing calls.
  Semantic `03:60A6@all` handles the adjacent Tower
  Guard's route-proven closed-selector update from nonzero gate counter
  `$2D4D`. Its four-boundary, one-write, 48-cycle transcript replaces all
  2,650 current route calls, including 58 original interrupt-handler
  passages; the zero-counter `$8E` open selector and BS-5 entrance remain
  emulated. Semantic `03:7CAF@all` handles the hidden fixed
  collision/scene-response controller's route-proven dormant return from
  nonnegative scene state `$2CC0` and zero collision scratch `$CC`. Its
  four-boundary, zero-write, 45-cycle transcript replaces all 2,650 current
  route calls, including 54 original interrupt-handler passages; negative
  scenes and the nonzero response state machine remain emulated. Semantic
  `03:7B27@all` handles all 2,650 adjacent hidden fixed-scheduler calls.
  The 332 timer expiries reload the eight-frame cadence and install the
  first `$02/$30/$08` banked-call submission natively; semantic
  `03:7B4E@all` and `03:7B5D@all` install the remaining `$03` and `$07`
  submissions after the original wrappers return. The wrapper and exported
  `08:4030` routine remain emulated, and the complete 46,618-write artifact
  stays exact; only the unobserved negative-scene path remains emulated.
  Semantic `03:7B7A@all` handles all 2,650 paired hidden fixed-marker
  calls. In addition to the exact 48/93-cycle suppressed and waiting paths,
  all 332 timer expiries reload the cadence, advance/wrap the eight-phase
  wave, select Y, and install the first `$1E:16` marker descriptor through
  an exact 231/240-cycle routed prefix. Semantic `03:7BBB@all` and
  `03:7BCD@all` install `$1E:18` and `$1E:1A` through 332 exact 36-cycle
  continuations apiece. Semantic `03:7BE0/7BE8/7BEC@all` replaces all 996
  marker-wrapper prologues, post-draw accumulator restores, and epilogues
  through exact 33/12/12-cycle stack-aware segments. Semantic
  `02:5955@all` now executes all 997 routed phase-stride additions, including
  the 996 marker calls, through a 15-checkpoint, eight-write, 198-cycle
  transcript; the remaining shared `01:C05D` drawing submission retains its
  interrupt-bearing assembly behavior. The complete 88,480-write marker
  artifact stays exact; only the unobserved negative-scene path remains
  emulated. Semantic `03:74D8@all` handles all 2,648 routed Giant-Face
  controller calls through the exact 27-cycle, three-boundary request-bit-
  clear return. All 23 original interrupt-handler passages and 289 handler-
  owned writes remain exact; wake and transformation requests stay in
  assembly. Semantic `03:767E@all` handles all 873 routed Giant-Face hidden
  animator calls through the exact 27-cycle, three-boundary idle return for
  animation modes other than one. All eight original interrupt-handler
  passages and 97 handler-owned writes remain exact; mode one's active
  animation stays in assembly. Semantic `03:76FA@all` handles all 867 routed
  Giant-Face crown collision checks through the exact 27-cycle, two-boundary
  no-hit return. All seven original interrupt-handler passages and 86
  handler-owned writes remain exact; the 300-point crown-hit path remains in
  assembly. Semantic `03:7726@all` handles all 1,214 routed Giant-Face mouth
  checks through the exact 48-cycle, four-boundary closed/no-hit return. All
  22 original interrupt-handler passages and 288 handler-owned writes remain
  exact; open-mouth capture and the 300-point hit path remain in assembly.
  Semantic `02:4C91@all` handles both collision-free Giant-Face helmet
  channels through exact 66/135/144-cycle timer-wait, appearance-advance, and
  appearance-reset transcripts. All 1,748 calls, including 57 original
  interrupt-handler passages and the complete 3,558-write helmet artifact,
  remain exact; collision activation remains in assembly.
  Semantic `02:4A93@all` now applies the same shared four-tick controller to
  all 1,008 collision-free Four-Star Wall-Pocket calls and the trace-backed
  144-cycle ordinary or 171-cycle matching-arrow collision/access decision through
  `$4ABB` or `$4ACF`, including the matching path's `$2CFA` latch clear and `$41`
  audio-command load. Its post-audio continuations `02:4AD2@all` and
  `02:4AE5@all` now compose the checkpointed 100-unit score, four-tick timer,
  appearance write, and scheduler handoff natively while the shared audio and
  appearance helpers remain emulated. The 66/135/144-cycle timer and appearance
  transcripts, including their original interrupt passages, remain exact;
  Big-Star score, color, and bonus-stage access paths remain assembly-backed.
  Semantic `02:5972@all` now initializes both mirrored upper Wall-Pocket
  hands natively. It composes the existing coroutine scheduler to preserve
  each seven-write, 192-cycle setup and installs live dispatch `02:5975`;
  semantic `02:5975@all` now composes the shared RNG recurrence and coroutine
  scheduler for the traced slot-`$40` live timer setup, preserving its
  1,323-cycle, 33-write parent transcript and original interrupt passage.
  semantic `02:5991@all` also executes all 299 traced delayed-timer
  decrements, including the high-byte borrow and original interrupt passages,
  plus the trace-backed slot-`$40` `$0001`-to-zero coroutine handoff. That
  20-write, 918-cycle passage composes `01:C5C9`, preserves its periodic IRQ,
  and installs `02:59A5`. Semantic `02:59A5@all` now handles the traced
  no-interrupt 16-write, 456-cycle opening transition, including its
  collision/state/type changes, `$59C4` coroutine submission, and three-tick
  scheduler result. Its `$2D33`-set branch also now replaces all 298 observed
  dormant returns in 27 cycles without game-owned writes, preserving the
  original interrupt-handler passages. Semantic `02:59C7@all` now executes
  the positive-state opening follow-up, writing type `$82`, a one-tick timer,
  and the `$59DF` scheduler continuation through an exact nine-write,
  261-cycle no-interrupt transcript. Its negative close loop also executes
  natively: it clears the state bit, resubmits the `$59BF` four-tick delay,
  and composes the original scheduler IRQ. Collision, animation, and response
  branches remain assembly-backed. Semantic `02:59E0@all` now covers the
  scheduled opening timer: four exact decrement/appearance passes and the
  terminal appearance-two RNG/delay/scheduler handoff complete in five calls,
  with all 33 writes and the 825-cycle terminal transcript preserved. The
  actual post-opening scheduler entry `02:5A08@all` now preserves its seven
  writes and exact 192-cycle `C018`/`C5C9` transfer, installing the `$5A0B`
  state dispatch. Semantic `02:5A0B@all` now initializes that fixed-slot
  state, marks its collision byte, and schedules `$5A1B` through an exact
  24-write, 1,170-cycle transcript including its timer IRQ. The later
  collision/response state paths remain assembly-backed. The
  scheduled `02:5A1B@all` countdown now covers all nine routed fixed-slot
  calls: six 33-cycle ticks, two 96-cycle collision-counter expiries, and an
  exact 81-cycle transfer into the existing interrupt-bearing `$5975` tail.
  Semantic
  `02:4DD9@all` handles the Giant Skeleton's Eyeball activation gate's
  dormant branch from the live Dragon's-Egg latch and session flags. Its
  six-boundary, zero-write, 57-cycle transcript replaces all 1,988 current
  route calls, including 37 original interrupt-handler passages, while the
  inactive opening and final-animation paths remain emulated. Semantic
  `02:4FC1@all` handles the three Mercy Stars' not-last-ball activation-gate
  return from live table-mode and remaining-ball state. Its five-boundary,
  zero-write, 54-cycle transcript replaces all 7,950 current route calls,
  including 163 original interrupt-handler passages, while non-normal-mode,
  last-ball score-limit, and activation paths remain emulated. Semantic
  `03:62B8@all` handles the Giant Door's nonnegative collision-state return
  from indexed `$3800,X`. Its two-boundary, zero-write, 27-cycle transcript
  replaces all 676 current route calls, including seven original
  interrupt-handler passages, while the score, hit-counter, and completion
  handshake remain emulated. Semantic
  `03:65C2@all` handles the Three-Blobs controller's first-pair and
  third-value mismatch returns from live color state `$2D39-$2D3B`. Its
  three/five-boundary, zero-write, 42/63-cycle transcripts replace all 2,650
  current route calls, including 50 original interrupt-handler passages,
  while equal-state score and randomized-reset outcomes remain emulated.
  Semantic
  `00:E837@all` negates the shared signed 16-bit scratch word used by flipper
  projection and rotation. Its 8/9-boundary transcripts replace all 894
  route calls, including 24 interrupted scopes, through exact
  three/four-write, 90/102-cycle transactions. Semantic `00:E624@all`
  executes the fixed eight-bit unsigned multiply round inside that pipeline.
  Its live-bit-generated 41-113-boundary transcripts replace all 5,371 calls,
  including 1,592 interrupted scopes, while preserving all 181,868 writes
  and `600 + 84 * admitted-bit-count` body timing. Semantic `00:E65C@all`
  composes one or two of those children with signed operand normalization,
  exact nested stack boundaries, fixed-point result selection, and sign
  restoration. All 3,700 wrapper calls are native, including 2,157 calls
  with original handler writes, while all 244,531 ordered writes remain
  exact. Semantic `01:D09E@all` composes four of those signed wrappers plus
  one `E837` negation into both indexed flipper projection outputs. All 540
  calls are native, including 448 calls with original handler writes, while
  all 168,532 ordered writes remain exact. The fixed replacement checkpoint
  envelope is now 2,048 for this bounded composition while the independent
  write bound remains 512. Semantic `01:D1A2@all` uses the same four
  `E65C` children and one `E837` negation to produce both indexed rotation
  outputs. All 354 current route calls are native, including 296 calls with
  original handler writes, while all 103,362 ordered writes remain exact.
  Semantic `01:D612@all` composes one `E65C` child to scale the live signed
  flipper radius by its current coefficient into `$C2:$C3`. All 62 calls
  are native, including 51 calls with original handler writes, while all
  6,271 ordered writes remain exact.
  Semantic `01:D62E@all` derives the directional flipper-angle delta,
  converts it to 8.8, composes one `E65C` child, and applies the live
  direction sign into `$31:$32`. All 62 calls are native, including 43
  calls with original handler writes, while all 5,062 ordered writes remain
  exact.
  Semantic `01:CFD8@all` applies the mode-selected signed collision-motion
  clamp or 3/16, 1/4, 1/2, and 1/16 scaling. All 354 calls are native,
  including 41 calls with original handler writes, while all 4,103 ordered
  writes remain exact.
  Semantic `00:E905@all` now implements the carry-sensitive signed divider
  through its fixed 16-round unsigned core. All 752 calls are native,
  including 583 calls with original handler writes, while all 52,804
  ordered writes remain exact.
  Semantic `01:DD09@all` composes that divider for either nonzero motion
  component, subtracts its signed quotient, and applies the second-axis
  acceleration. All 753 calls are native, including the 669 previously
  deferred interrupt-bearing calls, while all 61,291 ordered writes remain
  exact.
  Semantic `01:CC1A@all` now rejects the central collision scan's four
  state-derived vertical-separation shapes, then composes semantic `CCD5`
  through all post-geometry horizontal and vertical bound rejections. The
  composed paths stage the child's ROM-resolved writes and all of its
  interrupt checkpoints before reproducing the parent subtractions, limit
  comparisons, stack, and return. The final in-range path now continues
  through mask-coordinate preparation and composes semantic `E569` and
  `CC98`, including their cross-bank stack, writes, flags, and interrupt
  checkpoints. All 85,715 calls execute natively while the complete
  1,784,119-write artifact remains exact.
  Semantic `00:E569@all`, the only nested arithmetic leaf used by those
  remaining calls, now executes all 874 eight-round unsigned-byte products
  natively. Its live-bit-generated transcript preserves stack state,
  conditional high-byte carries, 495-543-cycle uninterrupted paths, and
  every original interrupt passage while retaining all 10,729 aggregate
  writes.
  Semantic `01:CC98@all` now completes the final collision-mask lookup for
  all 874 in-range paths. It resolves both live-MPR pointer tables, derives
  the eight-byte mask row, applies the ROM column bit, and returns through
  the exact carry-set contact or carry-clear no-contact branch. Its
  378/384-cycle, nine-write transcript preserves all 9,617 aggregate writes
  and every original interrupt passage.
  Semantic `01:CCD5@all` resolves the near-target geometry leaf through the
  live MPR map and verified ROM pointer directories. Its path-sensitive
  instruction transcript now covers all four 783/789/885/891-cycle,
  16/19-write shapes and replaces all 66,523 remaining calls while
  preserving the complete 1,414,954-write artifact and original interrupt
  handling.
  Semantic `09:72FD@all` now writes all 376 score-panel glyphs natively. Its
  56-boundary transcript resolves four tile bytes from the verified ROM,
  drives both rows through the real VDC ports, preserves dynamic VDC wait
  states, advances the right-to-left cursor, restores Y, and absorbs the
  return with exact state, call, work-RAM-write, and bus-write parity. The
  deployed registry now covers 695,369,472 master cycles (53.81% of the route,
  97.87% outside the frame-sync wait) across 434 admitted routines.
  Semantic `03:6490/651F@all` now composes the complete Front- and Side-Spider
  steady parents around the existing collision-table and appearance
  transcripts. All 2,645/2,796 natural entries preserve the two nested call
  frames and instruction interrupt boundaries; the 2,642 Front idle returns
  and all Side returns are absorbed, while three Front hits resume at the
  original transition code. The deployed registry now covers 696,006,024
  master cycles (53.85% of the route, 97.96% outside the frame-sync wait)
  across 436 admitted routines, and bank `03`'s emulated remainder falls from
  1,174,179 to 532,035.
  Semantic `02:525F@all` now replaces the complete observed Devil-Shooter
  post-launch idle controller. All 2,169 natural entries preserve the five
  instruction interrupt boundaries and absorb the return at `02:526A`; the
  collision-active and session launch-request branches remain assembly-backed.
  The deployed registry now covers 696,149,178 master cycles (53.87% of the
  route, 97.98% outside the frame-sync wait) across 437 admitted routines, and
  bank `02`'s emulated non-entry remainder falls to 860,013 cycles.
  Semantic `02:5EEC@all` now composes the complete Giant Skeleton controller
  around the native `01:DAF8` direction classifier. All 2,139 calls retain the
  exact direction-to-appearance clamp, appearance store, collision gate, and
  original interrupt boundaries. The 2,138 idle calls absorb `02:5F21`; the
  sole collision-active call resumes assembly at `02:5F06` for the original
  score/audio/bonus continuation. Release and Debug preserve byte-identical
  gameplay artifacts. The deployed registry now covers 696,416,985 master
  cycles (53.89%, or 98.02% outside the wait) across 438 admitted routines,
  and bank `02`'s non-entry remainder falls to 589,950 cycles.
  Semantic `02:5590@all` now replaces all 1,084 natural calls to the right
  Skeleton-Insect post-trigger idle controller. Its five instruction
  checkpoints preserve every timer-interrupt passage before absorbing the
  `RTS` at `02:55AF`; the inactive secondary-collision and Tower-active
  animation branches remain assembly-backed. The deployed registry now covers
  696,498,285 master cycles (53.89%, or 98.03% outside the wait) across 439
  admitted routines, and bank `02`'s non-entry remainder falls to 507,954
  cycles.
  Semantic `02:4387@all` now replaces all 1,154 natural calls to the dormant
  Dragon's-Egg reset gate installed after the route's single egg hit. Three
  exact instruction checkpoints preserve all nine timer-interrupt passages
  before the idle arm absorbs the `RTS` at `02:438E`; the unobserved
  group-reset/reinitialization arm remains assembly-backed. Release and Debug
  preserve exact call, write, replay, and canonical census artifacts. The
  deployed registry now covers 696,553,677 master cycles (53.90%, or 98.04%
  outside the wait) across 440 admitted routines, and bank `02`'s non-entry
  remainder falls to 452,178 cycles.
  Semantic `02:5397@all` now publishes all eight original instruction
  boundaries in the Devil-Shooter companion position leaf and absorbs its
  `RTS` at `02:53A7`. All 1,260 natural calls run natively, including all 47
  interrupt-bearing variants; the canonical census, complete family call
  trace, and ordered writes remain byte-identical. The deployed registry now
  covers 696,591,984 master cycles (53.90%, or 98.04% outside the wait) across
  the same 440 admitted routines, and bank `02`'s non-entry remainder falls to
  418,428 cycles.
  Semantic `02:5408@all` now publishes every original instruction boundary on
  the Devil-Shooter companion appearance leaf's cached, advance, and reset
  paths. All 1,288 natural calls run natively, including all 22
  interrupt-bearing variants, before `RTS` at `02:5420` is absorbed. Release
  and Debug preserve the byte-identical canonical census, complete companion
  call trace, and ordered writes. The deployed registry now covers 696,611,766
  master cycles (53.90%, or 98.05% outside the wait) across the same 440
  admitted routines, and bank `02`'s non-entry remainder falls to 402,093
  cycles.
  The larger `01:D525` controller remains a decomposition boundary: its 128
  census calls span 29-1,456 writes and 1,035-49,122 body master cycles, so
  the largest live paths exceed the bridge's independent 512-write limit.
  Unimplemented `03:6966` semantic paths remain emulated.**
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

### Native-port iteration cadence

Milestone 4 routine work follows [the native-port workflow](docs/NATIVE_PORT_WORKFLOW.md).
Each slice requires an incremental Release build, ROM-free core coverage in
both configurations, and one owning-family Release ROM regression. Related
sibling routines should share that family route and artifact rather than add
duplicate full-route tests. The full Release suite is the family, milestone,
and cross-family checkpoint; it runs in parallel in under three minutes, so it
is not narrowed.

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
