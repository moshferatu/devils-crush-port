# Cabinet Preview

Last updated: 2026-07-30

## Current vertical slice

Launch the preview with `--cabinet`. It creates three independent SDL
windows:

1. A resizable portrait playfield. This is the required host window and owns
   input, fullscreen toggling, and the fixed sharp-scaling pipeline.
2. A resizable, non-focusable DMD. It draws the stored score as
   monitor-resolution seven-segment digits in frontend and gameplay states.
3. A resizable, non-focusable backglass. It displays the native animated
   Devil's Crush logo with aspect-preserving sharp containment. A persisted
   PNG, JPEG, or WebP remains available only as a nonfatal fallback.

Failure or closure of the DMD or backglass does not stop emulation. The
playfield remains the lifecycle owner, so auxiliary windows cannot steal
keyboard focus or terminate play.

`--cabinet-smoke-test` creates and renders all three windows for ten emulated
frames, then exits. The ROM-enabled CTest runs this through SDL's dummy video
and audio drivers so window ownership and fallback rendering remain covered
without an interactive desktop.

## Playfield switching

Frontend screens use the emulated 256-by-240 framebuffer at corrected 4:3
aspect. Normal play switches to the accepted 256-by-668 table crop documented
in `TABLE_BACKGROUND.md`. Its pixel-aspect-correct presentation ratio is
80:167:

```text
(256 * 5 / 4) : 668 = 80 : 167
```

The playfield's unused surround is cleared explicitly to RGB 0,0,0 before
each presentation so letterboxing remains true black on emissive displays.

The host decodes the packed ROM tile map and work-RAM palette attributes
against the current copied VRAM/VCE state, then crops source rows 20-687.
The table is decoded only after:

- at least one packed table-attribute byte is initialized;
- the live-ball count at `$2D52` is nonzero; and
- session bit `$53.4` does not request a bonus stage.

Menus, session-end presentation, and bonus stages therefore fall back to the
emulated framebuffer. This reuses original presentation and gives bonus
stages the required full playfield window without a second simulation.

The playfield now copies Beetle's internal post-DMA HuC6270 SAT and decodes
the live camera viewport's sprite patterns, palettes, dimensions, flips, and
sprite-to-sprite priority over the accepted background. SAT screen
coordinates are translated through the mapped camera value at `$200B:$200C`
into the same table-world coordinate system, and score-panel sprites are
clipped out because that presentation belongs on the DMD. The packed
background decoder retains a raw nonzero-pixel plane, so the HuC6270
foreground bit determines whether each sprite pixel crosses the background.
The hardware limit of 16 sprite halves per scanline is also enforced.

Typed views decode all 127 mapped work-RAM actor slots. Fixed mechanisms in
slots `$0A-$61`, flippers in `$04-$09`, and persistent enemies in the reused
dynamic pool `$62-$7E` resolve their exact bank-8 ROM descriptor groups with
signed position, type, appearance, phase, and live dispatch. Production
composition does not proximity-associate nearby SAT entries. When the camera
moves away, work-RAM coordinates continue moving their decoded sprite
patterns on the full table without depending on mutable regional VRAM.
Their VCE palette indices remain live, allowing colors captured during
fade-in to reach the settled palette. Actor Y is converted from the VDC's
240-line presentation space into table space by removing the 30-line
score/status band. Empty slots clear immediately. Dynamic retention is restricted
to the ten type-`$82`/appearance-`$1A` Small Soldiers and four
type-`$82`/appearance-`$39` Tower insects; any slot reuse by an impact or
other effect family invalidates the old visual. This remains
observation-only; no second actor simulation is advanced.

The retained layer is drawn before the copied live SAT. Actors whose anchors
are observed in the current camera are omitted from the retained draw, making
the SAT authoritative without punching a 208-line hole through other
inactive mechanisms. This keeps all three flipper pairs visible and prevents
clustered Sorcerers or star/pentagram parts from inheriting one another's
geometry.

Live VRAM is region-owned. Animated actors therefore keep bounded exact
phase variants. An off-camera enemy resolves its current phase from a
ROM-owned upload or restores the learned visual for that same phase; it
never slides a frozen fallback image while its work-RAM position changes.
Only static mechanisms may retain a prior aligned visual when a phase has no
independent artwork. Unrelated regional VRAM is never allowed to erase a
flipper or replace pentagram/Sorcerer pixels.

The original presentation metadata is now independently mapped. Bank
`$08:4036` schedules actor records into the 256-word SAT staging table at
VRAM `$7800`; `$08:40DF` resolves 69 appearance tables, phase-selected sprite
descriptors, signed offsets, pattern IDs, and attribute modifiers. The native
decoder produces 100 fixed-table sprite descriptors at frame 1,200 and all 23
eligible live groups match the copied SAT word-for-word.

The local ROM-derived pattern atlas now seeds every non-hidden fixed
mechanism, all flippers, and both persistent dynamic-enemy families before a
camera visit when necessary. The shared type-`$8F`, appearance-`$10` impact
burst is the only admitted transient family: its original `$4290->$42B0`
dispatch transition, phase, table coordinates, and slot lifetime drive two
exact bank-8 primitives. Unclassified effects remain live-camera-only, and
slot reuse clears retained pixels immediately.

## Display assignment and persistence

Press F12 in cabinet mode to pause emulation and enter display assignment.
The playfield, DMD, and backglass become blue, orange, and purple
identification screens. A yellow inset marks the selected role. The window
titles carry the controls:

- 1, 2, or 3 selects the playfield, DMD, or backglass.
- Left and Right cycle the selected role across currently connected displays.
- F toggles borderless fullscreen for that role.
- B opens the native PNG/JPEG/WebP backglass picker.
- Delete clears the selected static backglass fallback.
- Enter, Escape, or F12 saves the assignments and resumes emulation.

The host atomically stores `portable-data/cabinet.ini` after assignment and
on normal exit. Each role records the SDL display name, a fallback window
rectangle, the display's current orientation, and fullscreen state. Startup
matches the exact display name first. If that display is absent, it chooses
the display with the greatest overlap against the saved rectangle, then the
primary display. Restored rectangles are clamped to the selected display.
This makes monitor reorder and monitor loss deterministic while preserving
playfield-only degradation when an auxiliary cannot be created.

The bounded `--cabinet-smoke-test` uses safe defaults and deliberately does
not read or write this user configuration.

## Backglass animation and image fallback

`--backglass <image>` supplies a scripted selection; the F12 screen's B key
provides the interactive equivalent. Successful selections are stored as
UTF-8 in `portable-data/cabinet.ini` and retained as the fallback used only
when the native animation cannot be decoded or rendered.

The loader recognizes PNG, JPEG, and WebP from file content rather than the
extension. It rejects missing or empty files, files over 64 MiB, unsupported
content, dimensions above 16384 pixels on either side, and decoded images
above 64 megapixels. Failures are nonfatal and leave black as the final
fallback.

Valid images are contained at their source aspect ratio against black. They
use the same integer-nearest staging and controlled final linear resolve as
the playfield, so pixel-art backglasses retain the fixed sharp policy. A
backglass renderer reset reloads the persisted source; closing or losing that
window still does not affect emulation.

## Native backglass logo

The backglass uses a native, ROM-derived title-logo renderer. It
decodes the five original compressed graphic streams once, resolves the two
title palettes and eye palette directly from the cartridge, and reproduces
the mapped 64-frame palette and four-sprite eye cycle. The renderer does
not crop the emulated framebuffer or read live VRAM/SAT state. Its curated
216-by-152 logo crop preserves the complete lettering while omitting the
trademark glyphs, and is presented against black through the fixed sharp
scaler with the PCE's pixel aspect preserved.

## DMD state

`cabinet::decode_cabinet_state()` selects the score slot through `$2251` and
reads the three-byte little-endian score arrays at `$2205`, `$220C`, and
`$2213`. Stored units are multiplied by 100, preserving the verified original
display rule and the maximum visible score of 999,999,900.

The current DMD omits the P1/P2 discriminator until the two-player session
field is dynamically mapped. The score remains live during emulated
framebuffer and bonus-stage presentation because it is read independently
from work RAM.

## Remaining cabinet work

- Fixed-mechanism pre-seeding is complete. The eight stream-backed
  `00:F606` descriptors and every upload-disjoint pattern family are applied
  to bank-8 actor metadata before first camera visits. At the lower
  checkpoint, 23 live-camera groups plus 38 pre-seeded groups cover all 61
  non-hidden fixed actors. Lower, upper-middle, and middle persistent
  composites guard their changing appearance/phase frames. All five
  `$F768` `$7C00` scene packs are decoded; selector 0 matches live
  normal-table VRAM, while selectors 1-4 remain a non-blocking scene-mapping
  question.
- Ball/flipper presentation is complete. Slot `$00` remains live-SAT-only
  and trail-free; all six flippers use absolute table-world coordinates and
  phase-aware persistence, so the three pairs remain visible together.
- Dynamic-enemy presentation is complete for the ten Small Soldiers and four
  Tower insects. Their live SAT groups, pattern sources, animation metadata,
  and reused-slot invalidation are regression guarded.
- Transient-effect presentation is complete for the bounded impact-burst
  family. Frame 2,100 guards its exact live SAT group and off-camera
  persistence; frames 2,400/2,700 guard release without trails. Other
  effects remain live-camera-only.
- Camera-transition alignment is complete. Every post-fade route checkpoint
  resolves all six flippers, and the Sorcerer positions continue advancing
  between the observed frame-1,600 region and retained frame-1,800 region.
  The same route proves a zero-pixel background vertical offset through
  launch, and retained palette indices prevent pre-fade flippers from
  remaining as dark silhouettes.
- Two-player/current-player mapping and the P1/P2 DMD discriminator are
  deferred again at the user's request. When resumed, do not infer player
  identity from `$2251`; it is currently proven only as the score-array
  selector.
