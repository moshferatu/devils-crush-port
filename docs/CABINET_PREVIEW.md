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

The palette-attribute plane was originally treated as region-owned. The game
writes all 1,536 packed bytes once at scene start, then rewrites tile rows as
play continues: across the 3,600-frame state-census route, 8,274 of its 8,669
in-play byte changes — 95.4% — land on rows the camera is not showing, and
`render::TablePaletteCache` refreshed a row only while the camera covered it
on the theory that those writes belonged to a region being streamed in.

That theory did not survive measurement, and holding rows back caused a worse
defect than the one it prevented. The plane is indexed by tile row — a write to
row N is about row N — and the game keeps animating rows it is not showing.
Freezing them left five consecutive rows at world y 440-479 displaying a
subpalette from a long-finished animation phase for 330 frames each: a
tile-aligned rectangular band across the middle of the table in the wrong
colours until the ball travelled far enough to refresh it. Nine displayed rows
were affected across 2,264 row-frames of the census route.

Every row now follows the live plane. `--cabinet-attribute-check` is the
measurement that settled it, scoring both policies at the one instant the
answer is not a matter of opinion — when a row arrives under the camera, where
the live plane is authoritative for it by definition:

| policy | agreed with the authoritative value |
| --- | --- |
| retained until the camera covers the row | 123 of 129 arrivals |
| **read the live plane** | **129 of 129** |

The oracle is unchanged at 2.41%, as expected: inside the camera window the two
policies agree by construction, so the whole difference is in what the cabinet
shows of the table the original never displayed.

The tile patterns themselves need no such treatment. The table's 3,072 cells
reference only 663 distinct tiles against a 2,048-tile VRAM capacity, so its
artwork stays resident; pattern words change on 55 of 3,599 frames and the
large bursts are scene loads. 23 of those 663 tiles carry pattern numbers past
the end of VRAM and decode as zeroed cells, which remains a known gap.

`--table-vram-census <path>` records both, per frame, split by whether each
change lands inside the camera window.
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

The copied SAT and the work-RAM camera are not in the same time base. The
SAT is latched at vblank alongside the VDC's background scroll register,
while work RAM is exported after the frame's logic has run. Translating SAT
screen coordinates through the post-frame camera therefore displaces every
live sprite relative to the retained actors and the fixed table by one
frame of camera motion. Across the video-snapshot route the residue is zero
on 2,279 of 2,727 frames -- every frame the camera is at rest -- and
reaches 13 pixels on the 448 frames it moves, which is why the defect is
invisible in `--cabinet-layers retained` or `sat` alone and only appears
while the ball travels. The host recovers the camera the SAT was built
against: the scroll register settles exactly 32 rows below the table-world
viewport, and work RAM supplies the absolute region that the register's
256-line wrap cannot. `dc_rom_video_snapshot` guards both halves of that
relation -- agreement at rest, and bounded disagreement while scrolling.

Actor flags bit 5 hides an actor, and the retained layer must honour it.
The bank-8 descriptor walk already omits hidden actors, so a hidden slot
produces no sprite metadata; the composition loop previously read that as
"nothing new to resolve" and kept drawing whatever artwork the mechanism
last had while live work RAM went on moving it. Across the 2,727 decoded
frames of the video-snapshot route that was 53,871 stale resolutions, an
average of 19.75 ghost actors per frame. Many mechanisms are hidden for an
entire session — slots `$16`, `$44`, `$45`, `$48`, and `$5C`-`$5E` are
hidden on all 520 sampled frames — so their artwork persisted permanently
once seeded. Hidden slots now clear their visual each frame while keeping
their learned phase variants, because hiding is a visibility state rather
than a family change. `dc_rom_video_snapshot` asserts that no off-camera
actor keeps stale artwork after its phase moves.

The hidden bit is not a camera cull. Actor phases keep advancing while the
camera is elsewhere: over the state-census route every one of the 36 slots
with more than 50 off-camera samples changed phase off-camera, and none
advanced only while observed. Slots `$17`-`$1E` never entered the camera at
all on that route and still changed phase 194-208 times each. Off-camera
actors therefore must be re-resolved, never frozen.

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

## One rendered layer, measured against the emulator

The cabinet used to compose two sprite layers: actors reconstructed from work
RAM and ROM descriptors, plus the copied hardware SAT for whatever the camera
was showing. The two sit in different time bases -- the SAT is the VDC's
internal sprite cache, DMA'd at vblank and placed through the scroll register,
while work RAM is exported after the frame's logic -- so they disagreed
wherever they met. Every symptom traced back to that seam rather than to any
one of them: actors drawn twice a few pixels apart, mechanisms that animated
only while the ball was near, and the whole live region displaced by a full
camera step whenever the table scrolled.

Ownership predicates cannot fix this. Several were tried -- anchor-in-window,
sprite-extent geometry, pattern presence in the SAT, and a full
pattern-plus-position correspondence -- and each traded double-drawn pixels for
erased ones, because the question "which layer owns this actor" has no stable
answer when the two sources disagree about when the frame was.

So there is one layer. Every occupied actor, the ball included, is drawn from
work RAM through its ROM descriptors, on camera or off. The camera no longer
places sprites at all; it only decides what is visible, which is why the
cabinet's whole-table view and the original 208-row view can share one path.
`observed` survives only to drive learning, because the moment an actor is
under the live camera is the only moment its artwork is in VRAM.

### Resident patterns and off-camera animation

One layer is necessary but not sufficient: an actor also needs artwork for the
phase it is currently in. Sprite patterns normally arrive through per-phase
VRAM uploads that the decoded ROM atlas can reproduce, so an off-camera actor
resolves its own artwork. When the atlas upload for an actor's appearance does
not cover its descriptor patterns, there is no ROM-side source, and the cache
used to hold the last learned artwork rather than sample live VRAM -- which
belongs to whichever region the camera is on and would be the wrong data.

That guard was too broad. Patterns no decoded loader ever writes are
*resident*: they occupy fixed VRAM addresses regardless of the camera, so live
VRAM is authoritative for them everywhere. The proof already existed but ran
after the guard, so resident actors were held at stale artwork before it was
ever consulted. Flippers and the lower-table enemies are exactly this case, and
they froze the moment the ball left them -- the last visible symptom of the old
architecture, surviving the layer merge.

Hoisting the proof above the guard resolves them. Over the census route,
`missing_phase_variants` -- an off-camera actor whose phase moved with no
artwork to match -- falls from 2,194 to zero, with every other freeze path
already at zero. Oracle disagreement improves from 2.398% to 2.394%, so nothing
on camera regresses, and compose cost rises from 1.16 to 1.28 ms per frame
against a 16.7 ms budget: the cost of actors that actually animate.

A streamed phase never observed under the camera still has no honest source and
still holds its learned artwork. That case is real but no longer fires here.

### The oracle

`--cabinet-oracle` is what makes this checkable without looking at a screen. It
replays the census route and, per frame, crops the composition to the camera
window and diffs it against Beetle's own framebuffer, quantized back to the
VCE's three bits per channel because Beetle reaches ARGB through RGB565 while
the decoder scales those bits directly.

| composition | disagreement with hardware |
| --- | --- |
| background + retained + live SAT (previous) | 3.33% |
| background + retained, live SAT withheld | 9.28% |
| **single layer, every actor from work RAM** | **2.40%** |

The middle row is why the seam was invisible to earlier measurements: with the
SAT withheld but the handover still in place, the composition had a hole
exactly where the camera was. Removing the handover closed it, and the single
layer is now measurably closer to hardware than the two-layer composite it
replaces -- the copied SAT was making the result worse. Compose cost fell from
1.33 to 1.17 ms per frame against a 16.7 ms budget.

The oracle also found that cropping with the work-RAM camera disagreed on
12.1% of pixels, with scrolling frames misaligned by a full frame of camera
motion. Work RAM is exported after the frame's logic, so its camera belongs to
the *next* frame; the VDC's scroll register says which rows hardware actually
drew.

The remaining 2.40% is genuine reconstruction error and is the number to drive
down. Route frame 1,451 is the worst single frame at 34,100 pixels, where a
row-offset sweep collapses it to 619 -- the camera lands between values there.

### Lifting the camera cull

Once the layers collapsed, what was left was not a rendering fault at all. Of
the three flipper pairs only the one under the camera answered the buttons, and
the Front Spiders at `$26/$27` stood still. The renderer was following the ROM
exactly -- the ROM had stopped simulating the rest of the table.

`00:F4FC` sweeps every slot each frame and sets `$3080,X` bit 7 only inside
`[camera - 48, camera + 240)`. Two per-frame sweeps then read that flag:

```
01:C515   LDA $3080,X : BMI dispatch : AND #$02 : BNE skip    actor scheduler
02:404A   LDA $3080,X : BMI dispatch : AND #$02 : BNE skip    collision sweep
08:407C   LDA $3080,X : BPL skip     : AND #$20 : BNE skip    SAT presentation
```

A slot is skipped only when bit 7 is clear *and* bit 1 is set, so bit 1 is the
opt-in to the cull. Those two sites are the only reads of bit 1 anywhere in the
cartridge; every other `$3080` access in all 384 KB touches bits 5, 6, or 7.

The cabinet therefore clears bit 1 before each frame -- but only for the
flipper pieces at `$04-$09` and the Front and Side Spiders at `$26-$29`.

Bit 1 rather than bit 7 because bit 7 is also the gate on `08:4036`, the
original SAT presentation path: leaving it alone keeps the emulator queueing
exactly the actors the hardware drew, so its 64-entry sprite table cannot
overflow and the framebuffer the oracle measures against stays the same
picture.

Named slots rather than all of `$04-$61` because the cull is not merely an
optimization the ROM could have done without. Actor state machines are written
on the assumption that they stop while off camera, and running all 88 fixed
mechanisms everywhere advances timers and appearances that were never meant to
advance unseen: lifting it wholesale left the middle-table circles and the
giant face reading as lit from the start of a game until the camera reached
them and the on-camera logic put them right. The flippers are safe because
`01:D466` is driven entirely by the button and consults nothing else, and the
spiders were the other family the whole-table view exposed as frozen.
`--cabinet-seed-check` measures the blast radius: under the named-slot lift its
opening sweep is pixel-identical to the original cull, where the wholesale lift
moved several hundred pixels a frame.

`--cabinet-flipper-trace` measures this directly, replaying the quick state for
600 frames while working both flipper buttons (Button I carries `$3B80` mask
`$01` for the right-hand pieces, any direction carries `$F0` for the left):

| slot | phase changes on camera | off camera | with the cull lifted |
| --- | ---: | ---: | ---: |
| `$04`/`$05` | 24 | 0 | 84 |
| `$06`/`$07` | 72 | 0 | 84 |
| `$08`/`$09` | 17 | 1 | 84 |

Every change all six pieces made happened while their bit 7 was set, and they
made one between them while it was clear. With the cull lifted all three pairs
respond wherever they stand, and `--cabinet-freeze-census` shows the spiders at
`$26-$29` changing 12.5 times per 100 off-camera frames against 9.7 on camera,
where they previously missed most of what the ROM asked of them.

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
