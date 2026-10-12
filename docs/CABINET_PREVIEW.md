# Cabinet Preview

Last updated: 2026-08-16

## Current vertical slice

Press R in the main window to rotate the playfield clockwise by 90 degrees;
Shift+R rotates counterclockwise. The entire presentation rotates, including
smoothed sprites, title screens, and bonus stages anchored at the table's foot.
The DMD and backglass keep their orientation. Cabinet sessions save this as
`playfield.artwork_rotation` in `devils-crush.ini` on normal exit;
older configurations default to zero. This is separate from the saved monitor
orientation. R also works for the base single-window view during the session.

Cabinet mode is the default when launching the game. Use `--no-cabinet`
for the base single-window view; `--cabinet` remains an explicit enable flag.
Music voice overlay also defaults to on during play; disable it with
`--no-music-voice-overlay`. Both defaults, and the ROM, the starting bonus
stage, native or emulated code, smoothing and the nudge multiplier, can be set
in `devils-crush.ini` instead (`game.*`, `audio.music_voice_overlay`,
`video.smoothing`, `input.nudge_multiplier`), which is what the launcher
edits; a flag beats the file for one run. F6 toggles `video.smoothing`, and a
cabinet session saves it on normal exit like the rotation. Cabinet mode creates
three independent SDL windows:

1. A resizable portrait playfield. This is the required host window and owns
   input, fullscreen toggling, and the fixed sharp-scaling pipeline.
2. A resizable, non-focusable DMD. It draws a large score above a smaller
   current-ball and bonus row, using the original game digits.
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

## Motion smoothing

On displays above 63 Hz, the playfield automatically draws intermediate
pictures at the display refresh rate. F6 toggles smoothing for the
current run. VSync must be available; 60 Hz displays keep the original pictures.
The refresh-rate check reads SDL's cached desktop mode, avoiding a fullscreen
mode enumeration that can delay the first playfield picture on Windows.
After five seconds, and after each F6 toggle, a short sample reports
completed presents per second, game frames per second, extra interpolated
presents between game frames, and the longest presentation gap. An "active"
configuration alone does not establish the delivered cadence.
The game's simulation, input sampling, audio, collision timing and capture
replays still run at the original 59.8261 Hz.

Like the Zombies Ate My Neighbors port's layered renderer, this uses separate
sprite artwork and actor positions. Pieces of an actor share one motion offset.
Previous and current artwork are aligned to that same interpolated position,
then the two complete compositions are blended. This smooths rotating artwork, giant-face transformations, flashes and palette effects
without giving moving actors a second trailing position. Rendering into a target at least four times
the original resolution preserves fractional-pixel motion. Sprite order,
transparency and the table's background-priority mask remain intact.

The position filter also follows the Zombies port's treatment of uneven pixel
steps: `shown(t) = (3*p(t) + 2*p(t-1) - p(t-2))/4`. Steady motion stays on its
original path; alternating two/one-pixel steps become steady 1.5-pixel steps.
Corrections are capped at half a source pixel for the ball and one pixel for
other actors. Direction reversals, sharp acceleration, missing history and
teleports bypass the correction. A small stop can have that bounded visual
overshoot for one tick; no game coordinates are changed.

Interpolation adds up to one game frame (about 17 ms) of visual delay. F6 is
available for comparing the feel, especially when timing flippers. Animation
artwork transitions use a short crossfade, not generated geometric rotation
frames. Fast pose changes can therefore look briefly softer. Teleports, newly
visible actors, scene changes, quick-state restores and DMD-owned playfield
holds clear or bypass motion history.

Flippers are the exception, on the table and in the bonus stages. The game
draws a flip as four poses, one per frame, so a crossfade only blurs each step.
Instead, between frames the current pose is drawn turned back toward the
previous pose's angle about the flipper's pivot, through
`degrees * (1 - alpha)`, so the flip is a continuous swing and every game frame
still shows the game's own pose exactly. `table_swing()` measures each step
from the pixels: the turn is the change in the long axis of the flipper's
drawn pixels, and the pivot is the one point that turn leaves in place. The
bottom flippers' poses measure about +24, +2, -21 and -33 degrees, and every
step puts the pivot within two pixels of the centre of the round end.

At 240 Hz that is four angles per game frame, each a sharp flipper about five
degrees from the last, so each present also draws a fading trail of the angles
swept since the previous present (`trail_from`). The trail reaches back into
the previous frame only when the flipper was already turning the same way, and
a draw at a frame's own alpha with no present before it, as the motion checks
make, has no trail and is pixel-exact. Collisions still use only the game's
poses.

A turn is accepted only from a shape that is long and thin, whose pivot lies on
its own pixels, and whose previous pose, turned, lands at least 85% on the
current one. Every flipper pose spreads at least 7.24 times further along its
axis than across; the bonus-room shapes that otherwise measure as a turn, the
first room's swinging dragon heads, reach 6.23 at most and fit as poorly as
74%, so they keep their own redrawn poses.

Bonus stages are drawn from separate pieces as well: the room's background,
read from the BAT at the scroll registers, and its 64 hardware sprites. The
picture a frame shows is built from the SAT copied at the end of the frame
before it, so each room frame is drawn from the previous frame's SAT, with
sprite halves past the sixteen a line can show cut out as hardware drops them.
The pieces are used only when, put back together, they reproduce the
emulator's picture pixel for pixel, in Beetle's RGB565 colors; a frame they
cannot rebuild blends complete pictures instead. Sprites have no actor
identity, so each links to the nearest previous sprite of the same size within
the table's 16-pixel teleport limit, and every sprite's step correction is
bounded to the ball's half pixel. A sprite that changes pattern while breaking
from its own last step by more than two pixels, as a flipper's next pose does
eight pixels along, is a cut: it is shown as drawn, not slid and blended. A
sprite that changed pattern within that distance is cut as well when its drawn
pixels touch a cut or new sprite, since a new pose redraws the whole thing;
otherwise one piece of the old pose lingers for a frame inside the new one, as
a dragon head's did in the first room when the heads drop at the end of the
fade-in. The cut sprites, with the new and vanished sprites whose drawn pixels touch them,
make up one flipper changing pose, and that group is measured for its turn;
two flippers whose sprite boxes sit edge to edge stay apart because their
pixels do not meet. On the scripted route every room frame after the first is
drawn from pieces, in all six stages.

Title/high-score screens and the base single-window emulated framebuffer still
blend consecutive complete pictures. These screens do not expose separate
actors, so fast movement can show a brief double edge. Screen-selector and
geometry changes discard old pictures, preserving scene cuts and the cabinet's
existing bonus-entry hold. DMD and backglass rendering are unchanged.

`--cabinet-motion-check <directory>` runs the scripted gameplay route through
the SDL software renderer. It checks exact 4x endpoint pictures against the
existing compositor with step correction disabled, verifies distinct fractional
pictures without modifying gameplay RAM, and saves comparison BMPs. Synthetic
readback checks cover palette midpoints, changing silhouettes, overlapping
pieces, background priority, appearance changes, teleports and scene cuts.
The same check also verifies exact endpoints and distinct blended pictures on
the title screen and in all six bonus stages, and that each room's pieces
reproduce the emulator's picture at the endpoint, move between frames, and
cover at least 95% of its frames. Both routes pulse the flippers once the ball
is in play, and fail unless the table's flippers and each room's swing.
`--cabinet-motion-gpu-check <directory>` runs the same readback checks using the
actual graphics renderer in a hidden window.

`--cabinet-presentation-check` uses the actual host, native registry, display
settings, DMD and backglass for 1,920 scripted game frames. It switches smoothing
on/off/on and reports the measured cadence of each segment. Display settings
are read but are not saved by this bounded diagnostic.

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
- session bit `$53.4` does not request a bonus stage; and
- the cartridge's screen selector at `$2054` reads 0, the table.

Menus and bonus stages use the emulated framebuffer. Award and tally screens
are presented on the DMD while the playfield retains its preceding image. The
stitched table remains visible after the final ball reaches zero, throughout
the game-over presentation, and gives way only when the screen selector moves
back to a menu. Bonus stages preserve their corrected 4:3 aspect, match the
stitched table's fitted width, and align with the bottom of the playfield
window. On bonus entry, the last stitched image stays visible through the
outgoing flash/fade and loading interval. The viewport changes only when
`$2054` selects the bonus screen, preserving its normal fade-in without
briefly showing the departing table at bonus size. This also works without
a DMD window. The layout is recalculated when the window is resized. The last of those three conditions is
the one that took a defect to find; see
[Screens raised over a live session](#screens-raised-over-a-live-session).

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

Cabinet mode lifts the original camera pause for the upper Big Star (`$4B`)
and the eight orbiting Sorcerers with their controller (`$16-$1E`), plus the
seven gray orbs in the upper-left chute (`$50-$56`). Their normal rotation,
staggered deployment, orbit, and gray-orb movement logic runs from the start
of the session, even with the ball still in the shooter. The middle-table
Three Blobs (`$58-$5A`) retain their first-camera-visit activation requirement.

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

## Controls

The cartridge decides what each pad input does, and it does not do the same
thing in a menu as it does in play. During play `01:D466` gates each flipper
piece on `$3B80,X`: the left-side pieces `$04/$06/$08` carry mask `$F0`, so
*any* direction works the left flipper, and the right-side pieces
`$05/$07/$09` carry mask `$01`, so button I works the right flipper and also
launches the ball. Button II is nudge.

Left as shipped, that makes the flipper buttons unusable on a cabinet. Every
direction works the left flipper, so the right flipper button works the left
one too; and a right flipper button bound to button I accepts a letter during
high-score entry instead of scrolling one.

### Rewriting the flipper masks

The host therefore rewrites `$3B80,X` for the six flipper pieces each frame
from `input.left_flipper` and `input.right_flipper`. This is the surgical
place to do it: filtering the input instead would have to go through `$2CEE`,
which the scheduler shares with every other actor that reads held input, and
would take the directions away from the menus too.

| | cartridge | as shipped |
| --- | --- | --- |
| left pieces `$04/$06/$08` | `$F0` — any direction | `$80` — d-pad left |
| right pieces `$05/$07/$09` | `$01` — button I | `$30` — d-pad right or up |

#### Only while a flipper is in the slot

`$3B80,X` is a general-purpose per-actor byte, not a dedicated flipper field.
`03:6697` stores `1 + (A & $1F)` there and `03:68B1` stores `$40 + (A & $1F)`,
so slots `$04-$09` hold an input mask only while the pieces `01:D335`/`01:D376`
created still occupy them.

Rewriting the byte on every screen therefore corrupts whatever else owns the
slot. It did: the title screen keeps a counter in slot `$05`, and overwriting
it silenced the title music from about five seconds in. Measured at the title
screen, `$3B84..$3B89` reads `00 0A 00 00 00 00`; in play it reads
`F0 01 F0 01 F0 01`.

Two conditions now gate the write, and both are needed:

- a table session is in progress (`balls_in_session` is non-zero — it is `0`
  across the whole title screen)
- the byte still reads as the initializer left it, `$F0` or `$01`

The value test alone is not enough, because `$01` falls inside `03:6697`'s
`$01-$20` counter range. The session test alone would rely on slots `$04-$09`
never being lent to anything else mid-session. Together they also make the
rewrite idempotent, and it still reapplies if a piece is re-initialized
mid-session — the cartridge writes `$F0`/`$01` again and the next frame
catches it.

Occupancy is no help here: `$3000,X` reads `$80` for slots `$04`/`$05` at the
title screen exactly as it does in play.

### Both flippers at once

A pad cannot be pushed left and right together, and the cartridge rejects the
impossible rather than interpreting it: the *whole* held-input byte arrives at
`$2CEE` as zero, not merely the two bits. Measured across a sweep:

```text
pressed -> $2CEE:  90->90  C0->C0  81->81  84->84  A0->00  50->00
```

`$A0` is left with right and `$50` is up with down. Both are discarded
entirely; every other combination passes through intact. Two flipper buttons
on opposing directions therefore silence each other, and neither flipper
moves — which is exactly what happens if the two flippers are simply given
`$80` and `$20` and left there.

The right flipper's mask is `$30` for that reason: d-pad right *or* up.
`cabinet::resolve_opposing_directions()` swaps the flipper onto the second
input for the frames both buttons are held, and leaves the byte alone
otherwise. Each button keeps its natural direction when pressed alone, which
is all the menus and high-score entry ever see; up is never sent by a flipper
key on its own.

With the substitution in place the trace shows `A0` never reaching the game —
it arrives as `90` — and gate-open equal on all six pieces, meaning both
flippers open together during the both-held block:

| slot | mask | gate-open |
| --- | --- | ---: |
| `$04/$06/$08` | `80` | 150 |
| `$05/$07/$09` | `30` | 150 |

A configuration whose flipper list offers no alternative is left unchanged
rather than being given an input its flipper does not accept.

That decouples *which key works a flipper* from *which pad input the key
sends*, which is what makes the rest fit together:

| Key | Pad input | In the shooter | In play | Elsewhere |
| --- | --- | --- | --- | --- |
| Left Shift, Left | d-pad left | — | Left flipper | A direction — scrolls |
| Right, Right Shift | d-pad right | — | Right flipper | A direction — scrolls |
| Enter | button I + run | **Launch** | Pause | Start, pause, confirm |
| Space | button II | — | Nudge | — |
| Up, Down | d-pad up/down | — | — | Menu navigation |

No key that works a flipper sends button I, so no flipper button can accept a
high-score letter; all four arrive as directions and scroll. Confirming is
Enter's alone. Up and Down no longer flip anything.

Button I is dropped from the right flipper's mask for the same reason in
reverse: with nothing that flips sending it, leaving it there would only let
the confirm key flip on every press.

### Launch keys

Launching is button I — and so is accepting a high-score letter, and the
Devil Shooter reads it at `02:5159` as `LDA $2013,Y : AND #$01`, an immediate
operand with no per-actor mask to rewrite the way the flippers have. A key
therefore cannot launch and also avoid accepting unless the host looks at
where the ball is.

`input.launch` names keys that send button I, and only button I, while `$53`
bit 3 marks the Devil Shooter session and $2254 is zero (live gameplay).
The attract demo sets $2254 to one and also runs the shooter, so its launch
flag must not suppress Return: Run dismisses the demo through 01:C2AF-C2B5.
That bit marks the shooter session and
nothing else, which `--cabinet-nudge-probe` established while tracing the
nudge gate. Suppressing the key's ordinary pad input is the point rather than
a side effect: a launch button that also sent Run would pause the game at the
moment it was pressed.

```ini
input.launch=Return
```

Return launches from the shooter and still starts, pauses and confirms
everywhere else. The flipper buttons launch nothing. An empty list disables
the override entirely.

`--cabinet-launch-check` holds one key through a launch and reports what the
ball did:

```text
holding Return from frame 1000:
  frames the key sent button I: 180
  frames it sent its ordinary input: 0
  ball left the shooter at frame 1278
  $53 timeline: 1=00 942=08 1278=00 1567=02
```

One caveat the check turned up: the shooter fires on *release*, and holding
the key continuously from before the ball is seated did not launch even with
the charge counter well past `$18`. Pressing once the ball is in the shooter
does. That is cartridge behaviour — `$2018` is an edge latch — and not
something the override introduces.

An empty list leaves the cartridge's own mask untouched:

```ini
input.left_flipper=
input.right_flipper=
```

`--cabinet-flipper-trace` measures all of this. It reports what the scheduler
copied to `$2CEE`, which is the byte `01:D466` masks against:

```text
pressed -> $2CEE:  01->01  10->10  20->20  40->40  80->80
```

Every bit is copied verbatim, so d-pad right arrives as `$20`. Over a
600-frame sweep holding each input in turn for 120 frames — 60 of them held —
the gate-open counts confirm the rewrite:

| slot | cartridge mask | gate-open | rewritten mask | gate-open |
| --- | --- | ---: | --- | ---: |
| `$04/$06/$08` | `F0` | 241 | `80` | 60 |
| `$05/$07/$09` | `01` | 60 | `30` | 60 |

241 is all four direction blocks; 60 is one. All six pieces still reach four
dispatch phases and still move, so nothing was disabled — the left flipper was
narrowed and the right one moved from button I to a direction.

### PinOne accelerometer nudging

Cabinet mode automatically opens a joystick whose name contains `PinOne`
(case-insensitive) and turns X/Y accelerometer motion into the original button-II
nudge. This uses the same settings and debounce as Alien Crush:

```ini
input.nudge.enabled=1
input.nudge.device=PinOne
input.nudge.axis_x=0
input.nudge.axis_y=1
input.nudge.threshold=20
input.nudge.release=8
input.nudge.cooldown_ms=250
```

These settings are saved in `devils-crush.ini`. Threshold and release
are percentages of signed joystick full scale; lower `threshold` for a lighter
bump. Either axis, in either direction, produces a 50 ms button pulse. Before
another pulse, both axes must settle below `release` for 100 ms and the cooldown
must expire. A held tilt cannot continuously retrigger. Z remains the plunger.

Nudging requires a focused cabinet window and live table or bonus play; demos,
menus, results and drained balls ignore physical motion. The original game handles
button II during pause as usual. Focus loss, display assignment, reconnect and
quick-state restore require settling again. Captures record the resulting button
input, so replay does not need the device. Desktop mode does not open it.

### The plunger

A cabinet has a real plunger on it, and a launch key is not one. What a
plunger knows is a distance, and the interesting question was whether the
cartridge has anywhere to put one.

It does, and it is the shooter itself. `02:52E3` charges the Devil Shooter by
moving its own piece down the table — it adds `$0040` to the piece's
twenty-four-bit vertical position on every tick button I is held — and then
derives the charge from where the piece ended up:

```
02:52FB  SEC
02:52FC  LDA $3300,X      the piece's whole-pixel Y
02:52FF  SBC $3E00,X      the rest position 02:50EE gave it
02:5302  STA $2D15        the charge
```

So `$2D15` is not a timer. It is the distance the shooter has been pulled
down from its rest position, in pixels, running from `$00` to `$27`; the
initializer parks the live piece at Y=671 against a reference of 632, which
is that same `$27` apart. `02:51DA` turns it into the ball's launch velocity
as `-(($2D15 >> 2) + 2)`. **The shooter piece is a plunger, and its draw is
the only thing the shot is computed from.**

That is what makes an analog plunger possible rather than approximate. A held
button charges a quarter of a pixel per tick, so a full `$27` takes 156
frames — two and a half seconds — while a spring-loaded rod is pulled and let
go inside sixty. Mapped to the button, the pull distance would reach the game
as nothing whatever and every shot would come out near the bottom of the
range. Mapped to the piece's position, the pull *is* the charge, and
`02:51DA` computes the shot from it exactly as it always did. The host
chooses neither the velocity nor the moment of firing.

Three bytes are written, and they are the three `02:52E3` writes itself: the
fraction at `$3280,X` and the two halves of the coordinate at `$3300,X` and
`$3380,X`. `$2D15` is written with them because on the frame the rod is let go
button I is already up, so `02:52E3` does not run to recompute it and
`02:51DA` reads it that frame. The appearance at `$3980,X` is left alone —
`02:52E3` sets it from `$2D15` on every frame the rod is still back, which is
every frame but the last.

**The ball is carried with the shooter, which the cartridge does not have
to do and this does.** Nothing in the charge path places the ball: while the
shooter descends the ball rests on it and falls after it, and that works
because the cartridge descends a quarter of a pixel per tick. Measured on the
census route, the ball tracks the shooter exactly nine pixels above through the
whole 96-frame charge, 623 to 647 as the shooter goes 632 to 656. A rod moves
ten pixels in a frame; gravity cannot keep up, and the shooter slides out from
under a ball that stays where it was. Before this was fixed the ball was 14, 24,
34 and 44 pixels adrift at the four measured draws — growing with the draw,
exactly as a race between a rod and gravity would.

So the ball is placed with the shooter, and the rule is the cartridge's own
rather than an invention. `02:52C7`-`02:52D4` is where the Devil Shooter puts
the ball while it retracts:

```
02:52C7  LDA $3300,X      the shooter's whole-pixel Y
02:52CA  SBC #$09
02:52CC  STA $3300        the ball's
```

That is the same relationship, applied in the phase where the cartridge leaves
gravity to maintain it. It is only taken over while the ball is *already*
holding it, measured against where the shooter is rather than where it is
going — asking about the destination instead would refuse exactly the case that
needs carrying, a rod drawn far in a single frame.

The slot is found rather than assumed. `02:5156 JSR $C018` installs `$5159`
into `$3F00,X`/`$3F80,X`, so the charging shooter is the slot dispatching to
`02:5159`, and the write is refused unless exactly one slot is doing that, a
shooter session is running at `$53` bit 3, the table is in normal mode at
`$2254`, and the piece's current draw is already inside `$00`-`$27`.

That last gate turned out to matter for a reason worth recording. `$53` bit 3
goes up the moment a ball is served — frame 942 on the census route — and the
shooter does not catch the ball until 1,064 or install its charge script until
1,084. Two seconds separate "a ball is in play" from "there is something for
the rod to move", so whether the shooter took the write is the only sound
gate, and the host uses it for the launch button as well as for the position:
until the write lands, a drawn rod sends nothing.

#### Reading the rod

The plunger is read as a raw SDL joystick rather than as a gamepad. SDL's
gamepad layer only answers for a controller its database has a mapping for and
no plunger board is in it: what a cabinet has is a potentiometer on an
Arduino, presenting one axis of an anonymous HID device. The joystick layer
answers for anything with an axis, which is the whole set.

Four numbers describe one, and none of them can be guessed. Which axis the rod
is on is the board's choice; the two endpoints are a potentiometer's, so they
differ between two copies of the same board.

```ini
input.plunger.device=
input.plunger.axis=-1
input.plunger.rest=0
input.plunger.drawn=32767
input.plunger.deadzone=8
input.plunger.release=25
```

`axis=-1` is no plunger, and it is the default. Off rather than on, and that
is a measurement rather than caution: a plunger and a thumbstick are
indistinguishable from here. The PinOne's rests at the centre of its axis and
reads negative when drawn, which is exactly what a stick pushed one way looks
like, and axis 2 on an ordinary gamepad *is* the right stick — so a plunger
that defaulted to on would let a thumbstick charge the shooter on any desktop
with a pad plugged in. Somebody has to say which axis it is.

The endpoints do default to a Cleveland Software Design PinOne's, which
reports its plunger on an eight-bit Z axis resting at the centre and reading
positive as the rod is drawn; SDL widens that to the range above, and Windows
reports Z as the third axis of anything that has one. So a PinOne needs only
`input.plunger.axis=2` — but confirm it with the probe rather than assuming,
because a board's firmware settings decide which end of its own travel is
which. `device` is a case-insensitive substring of the
controller's name, and empty matches the first controller that has the axis.

Two endpoints rather than a range and an inversion flag: a plunger whose axis
runs backwards is one whose `drawn` is the smaller number, and needs no
setting of its own to say so.

Getting them the wrong way round is not subtle, and is worth naming because it
looks like a different fault than it is. The on-screen shooter answers the rod
being *pushed in* rather than pulled out; only the inch or two of push travel is
reachable, so the charge never gets far, and every shot dribbles back down the
lane instead of launching. That is one setting, not three symptoms. Run the
probe.

`--cabinet-plunger-probe` supplies all four. It watches every connected
controller, takes two seconds of stillness as the rest position, then watches
somebody pull the rod and prints the axis that moved furthest as a block ready
to paste into `devils-crush.ini`.

#### Letting go is a speed

A spring-loaded rod can return between samples. Release detection considers
one or two consecutive descending samples: a total drop of `release` percent
fires using the position at the start of that short descent. For example,
60% -> 45% -> 20% now launches from 60%, rather than the already-returning 45%.
The history is bounded to two samples; slow adjustments do not accumulate into
a release or retain a maximum pull from earlier in the stroke.

Calibration preserves 1/256-percent travel. The host rounds that pull to the
nearest quarter pixel, matching the positions reached by holding Enter, and
places the shooter and seated ball at that position. The 39-pixel travel has
156 quarter-pixel steps; the minimum launchable charge remains one pixel.
Whole-pixel charge and the original launch-velocity calculation remain the
ROM's rules. Analog input no longer selects positions between keyboard steps.

Reaching rest fires too, and has to. A rod eased all the way home by hand
never falls fast enough to trip the first test, and without the second the
shooter would sit at no charge with the button still held and the ball would
never leave.

A rod found already drawn is not somebody pulling it. It has to be seen at
rest once before it counts, which is what stops a controller that is not the
plunger — one whose axis happens to sit at full scale, or one that answers
nothing — from launching the ball on its own.

#### Four draws, four shots

`--cabinet-plunger-check` drives the same model the host drives from a
scripted rod at four strengths, and reads back what the cartridge computed:

```text
scripted plunger, one pass per draw:
  draw   $2D15   $3580   fired at   escaped   ride
   25%       9   $FC         1100        no      9
   50%      19   $FA         1101       yes      9
   75%      29   $F7         1098       yes      9
  100%      39   $F5         1098       yes      9
  4 distinct launch velocities, and the ball rode the rod in every one.
```

Both computed columns are the ROM's own arithmetic and neither is worked out by
the host. The charges are `draw * $27 / 100`, and the velocities are
`-(($2D15 >> 2) + 2)` of those: `-4`, `-6`, `-9`, `-11`. Four draws producing
one velocity would be exactly the failure a button-mapped plunger has, and
`ride` is the ball's distance above the shooter at the shot, which `02:52CA`
says is nine.

The launch frame is found by the charge starting to come back rather than by
watching the ball, because `02:51DA` does both in one dispatch — it stores the
velocity at `02:51FA` and then `02:5208 JSR $533B` begins the retraction. An
earlier version watched the ball instead, taking the most negative `$3580` of
the following thirty frames, and read a step too fast at the two largest draws:
a ball launched hard picks up more speed later in the lane than it left with.
The extreme of the flight is not the launch.

The quarter draw fires and does not escape, and that is the cartridge rather
than a shortfall: four pixels a frame will not climb the shooter lane, so the
ball rolls back and the shooter resets to charge again — which is what a short
press of button I does on the original. `dc_rom_plunger_check` pins all eight
numbers, including that one.

The check also compares a direct 60% release with returns sampled through
55%, 45%, and 40%. All must produce the same charge and launch velocity,
and the shooter and ball must receive matching fractional positions on the
release frame. Captures include `plunger_draw_units` alongside the rounded
percentage for diagnosing physical plunger sessions.

### Nudge

Use `--nudge-multiplier 2.5` to multiply the original nudge force by 2.5.
The default is `1` (unchanged cartridge behavior); values from `0` to `16`
are accepted, including fractions. `0` removes the impulse. This affects
keyboard, gamepad and PinOne nudges in live play, in both desktop and cabinet
mode. It changes force, not the accelerometer trigger threshold, pulse timing,
or the game's shooter/tilt restrictions. Amplified motion saturates at the
signed 16-bit limit instead of reversing through overflow. It adds no screen shake.

For example: `devils-crush --cabinet --nudge-multiplier 3`.
The factor lasts for that run and is recorded in captures so replay restores it
automatically. Older captures retain their original 1x force.

Nudging works and is faithful to the cartridge; it is simply very small.
`--cabinet-nudge-probe` measures it by running the state-census route twice
from a cold boot, identically except for the Button-II taps, and diffing the
ball's own coordinates.

`01:C3CB` is gated on `$53 & $2A`. Two of those bits are reached in ordinary
play: `$08` while the ball is in the Devil Shooter, and `$02` during the
post-drain session transition. Nudging is refused in both, which is correct —
a ball in the launcher cannot be nudged. A saved state taken with the ball in
the shooter therefore has `$53=08` and refuses every press until the ball is
launched.

Past the gate the routine tests Button II at the per-player *edge* latch
`$2018,Y`, not the held-input copy `$2CEE` the flippers read, so a held button
nudges once rather than continuously. `01:C457` then halves the signed vector
at `$2CC2/$2CC3` and adds it to slot-0 motion at `$3500/$3580`.

Halving is why it is hard to see. Measured on the route, one nudge moves the
ball by one pixel horizontally and two to four vertically over the following
frames:

```text
frame 1312  dx -1  dy  0
frame 1315  dx -1  dy -2
frame 1317  dx -1  dy -3
frame 1320  dx -1  dy -4
```

The cumulative effect is not small at all — over the same route the nudged
ball never drained, where the un-nudged one drained at frame 1,931, and the
two tracks ended 318 units apart. There is simply no per-press feedback: the
table does not shake and nothing sounds.

The tilt lock `$2CC7` was not reached even by deliberate spam. At 510 presses
and 194 accepted nudges the accumulator `$2CC5` peaked at 15 and never tripped
the lock, because `$2CC5` is cleared every time the `$2CC6` counter wraps at
120 frames.

All of this is identical with `--emulate-only`: the same `$53` timeline, the
same 487 gate passes, the same phase transitions. No native replacement is
involved in the behaviour.

### Rebinding

Bindings live in `devils-crush.ini` under `input.`, one line per pad
input, holding a comma-separated list of SDL key names:

```ini
input.one=Return
input.two=Space
input.select=Backspace
input.run=Return
input.up=Up
input.right=Right,Right Shift
input.down=Down
input.left=Left Shift,Left
input.left_flipper=left
input.right_flipper=right,up
input.launch=Return
input.plunger.device=
input.plunger.axis=-1
input.plunger.rest=0
input.plunger.drawn=32767
input.plunger.deadzone=8
input.plunger.release=25
```

The two `_flipper` lines name pad inputs rather than keys, from the same set
as the `input.` suffixes above. The `plunger.` lines are a controller rather
than a keyboard and are described under [The plunger](#the-plunger).

Names are SDL scancode names, matched case-insensitively, so the host can
resolve them with `SDL_GetScancodeFromName` and there is no bespoke key table
to keep in step with SDL. Any number of keys may share an input. An input
that is named but left empty is unbound; an input whose line is absent takes
its default, so a configuration written before bindings existed still loads
and keeps its display assignment. A name SDL does not recognise is reported
on stderr and skipped without disturbing the rest of the binding.

The gamepad keeps a fixed layout — south and east for buttons I and II, the
hat for directions — since only the keyboard has to match whatever a
cabinet's encoder is wired to send.

`devils-crush.ini` is rewritten in full on exit and after display assignment, so
the bindings are a stored field rather than a passthrough: an existing file
without them gains the section the first time the host exits normally.

The single-window view reads the same bindings, so a rebinding made for the
cabinet, or in the launcher, holds on a desktop too. It never writes the file,
and it ignores the file's artwork rotation, which belongs to the cabinet's
playfield monitor.

### An existing file pins its bindings

Because the file is rewritten in full, the first normal exit writes every
binding explicitly, and a stored field always wins over the default. Changing
a default in code therefore does not reach a configuration that already
exists.

This is worth knowing before chasing a phantom bug. It kept
`input.one=Right Shift,Right,Z` alive after the defaults had stopped saying
so, which left the right flipper button launching the ball — button I charges
the Devil Shooter — well after the code had removed it from that role. The
binding was correct; the file was old.

To pick up changed defaults, delete the `input.` lines from `devils-crush.ini`.
They are rewritten from the defaults on the next exit, and the window
placements above them are untouched.

### The mouse cursor

The host hides it at startup. Nothing here reads the mouse — not play, not
the F12 display assignment, which is entirely keyboard-driven — so a pointer
left on the playfield is only something to look at, and a cabinet has no way
to move it off. `SDL_HideCursor` is application-wide, so one call covers the
DMD and backglass windows too, and `SDL_Quit` restores it on exit.

The one exception is the ball-dragging cheat (`--drag-ball`, or
`game.drag_ball=1`), which leaves the pointer showing and reads the left
button over the playfield.

### Dragging the ball

A cheat for testing the table: while the left button is held on the
playfield, the ball is carried under the pointer
(`include/devils_crush/input/ball_drag.hpp`).

The pointer's pixel is turned back into the ball's actor position through
the picture `present_frame` last drew (its rectangle and rotation), and then
through the sprite's own geometry. Bank `$08`'s descriptor puts the ball's
sixteen-pixel sprite at its actor position less `(38, 6)`, the 38 being six
pixels and the `$20` the hardware takes off a sprite's X. On the cartridge's
own screen `08:40DF` draws it at that Y less the camera anchor `$A9:$AA`; the
stitched table draws it at that Y less 50, which is `kTableWorldTop` and the
thirty lines of score band above a screen's playfield.

A held ball is taken out of play rather than fought over. The actor scheduler
at `01:C510` runs each slot through the routine in `$3F00/$3F80`, and the
ball's in play is `01:C721`: motion, collisions and gravity. While the button
is down that pointer is `$C52B`, the scheduler's own closing `RTS`, so the
ball is still filed to be drawn and does nothing else, and its position is
whatever the host last wrote. Letting go writes `$C721` back. An earlier
version left the ball in play and gave it the velocity to reach the pointer;
the table knocked it about under the hand, and it could not be put inside
anything.

`01:C721` moves the ball by steps `01:DC95` worked out at the end of the
frame before (`$B9-$BC`), so a ball let go would spend its first frame on the
steps of whatever it was doing when it was picked up. They are cleared while
it is held: it stays where it was put for one frame and leaves with the
pointer's velocity in the next, held to eight pixels a frame, the Devil
Shooter's hardest shot.

The camera is the constraint. `00:F261` moves it only by the ball's own
vertical velocity, and only while the ball is in the top 64 or bottom 48 of
its 240 rows and heading out, so it never catches up with a ball that has
simply appeared elsewhere; and `00:F4FC` takes what is outside the camera's
rows off camera, where it is neither drawn nor, for most of the table, run.
So a held ball is kept sixteen rows inside the camera, and while the pointer
is at that edge or past it the ball is given the velocity that scrolls the
camera toward it, up to eight rows a frame, and rides the edge there. A bonus
stage is one screen and has no camera to bring.

A ball waiting in the Devil Shooter or climbing its lane is carried too. It
runs `01:C6C2` there, the same motion without the drain, which installs
`01:C721` itself once the ball is left of X `$F0`; a ball let go in the lane
goes back to `01:C6C2`. In play the launch ends when the ball touches the gate
at the top of the lane, fixed slot `$0F`: the gate opens and closes behind it,
and `03:645D` then clears the shooter's bit (`$53` bit 3) and makes the gate's
type `$82`, shut, from the `$83` a ball passes through. A carried ball touches
nothing, so the host does those two things itself as soon as the ball is
carried left of X `$F0`, and the game has a launched ball: Enter pauses again
and the ball cannot fall back into the lane. A gate caught mid-swing is left
to finish, which does both for itself.

The shooter holds the ball behind the table's background (`$3D00` bit 7
clear), and the ball's own routine sets that bit again as it moves. A held
ball has no routine, so the host sets it: the ball in the hand is drawn over
the table.

It is refused in the demo, after the last ball, for a drained or hidden
ball, and for a ball in any other routine, such as one still being served.
F5 lets go before it
saves, and a ball found held with the button up is let go, so a quick state
cannot keep one. `--cabinet-ball-drag-check` runs it on the census route's
first ball and in bonus stage 1 with no mouse, and `dc_rom_ball_drag_check`
runs that with the native registry and without.

### Taking the keyboard at start-up

The playfield window claims the foreground itself. Showing a window is not
enough on Windows: the foreground belongs to whoever the system thinks the
player is using, and a cabinet front end that puts its own window back up a
moment after launching the game keeps the keyboard with it. The table is then
on screen and the flippers do nothing, which is exactly the failure a player
sees.

So the host raises the playfield with `SDL_HINT_FORCE_RAISEWINDOW` set, which
is SDL's thread-input sequence for getting past that rule, and re-makes the
claim four times a second for the first five seconds the window is up -- but
only while something else holds the focus. That window is wall clock rather
than a frame count, because the host loop runs several times faster than the
display while the emulator warms up: 180 iterations of it measured under
eight tenths of a second.
After five seconds nothing here touches the foreground again, so alt-tabbing
away later behaves normally. Frame-limited runs never claim at all, so a test
suite cannot pull the foreground off the machine.

Measured against a stand-in front end -- a second process raising its own
window about a second after the playfield appears. Before the change the
front end held the foreground at the end of all three runs; after it, the
playfield had it back 0.8 seconds after the front end launched, and still at
the end, three runs out of three.
The DMD and backglass are left alone; they are created
`SDL_WINDOW_NOT_FOCUSABLE`.

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

The host atomically stores `devils-crush.ini` after assignment and
on normal exit. Each role records the SDL display name, a fallback window
rectangle, the display's current orientation, and fullscreen state. Startup
matches the exact display name first. If that display is absent, it chooses
the display with the greatest overlap against the saved rectangle, then the
primary display. Restored rectangles are clamped to the selected display.
This makes monitor reorder and monitor loss deterministic while preserving
playfield-only degradation when an auxiliary cannot be created.

The bounded `--cabinet-smoke-test` uses safe defaults and deliberately does
not read or write this user configuration.

## Backglass animation

The backglass shows the native title-logo animation, and nothing else. Black
is the fallback when it cannot be decoded or rendered; closing or losing that
window still does not affect emulation.

A user-supplied backglass image was supported for a while — a `backglass.path`
field in `devils-crush.ini`, a `--backglass <image>` switch, and a file picker on
the F12 screen — and has been removed along with the SDL_image dependency that
decoded it. A configuration file still naming an image loads normally: an
unknown field is ignored, and the line disappears the next time the file is
written.

The logo is inset by
`kBackglassMarginFraction`, currently 6%, so the artwork sits inside its
window the way a real backglass sits inside its frame rather than running to
the edge of the glass. The margin is taken from the shorter axis so the
reserved border is the same number of pixels on every side: on the default
1108-by-739 backglass window it is 44 pixels. `present_frame()` insets the box
it hands to `calculate_scale_layout()` and then offsets the returned
destination, so containment and the sharp-scaling policy are unchanged — the
artwork is simply contained in a smaller box. The playfield passes no margin;
it is the one surface that should fill its display.

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

The DMD follows the cartridge's presentation state, using the original 16-by-16
score digits for scores, awards, ball numbers, and high scores. Digit tile
selections come from `09:7390`, their graphics from asset `0C:8006` (uploaded
at VRAM `$2800`), and their shaded red and green palettes from `11:A1D9`.
The dim segments are part of the original artwork. Labels and initials use
the resident 8-by-8 cartridge font decoded from `11:A9C0`.

`cabinet::DmdPanel` composes a fixed 320-by-112 image. The existing sharp
scaler fits it inside the DMD window without stretching its pixels. Fixed digit
sizes remain stable as values grow, including the maximum 999,999,900-point
score. Numbers use the original digit artwork without comma separators.
The live page shows SCORE above a large value, with smaller BONUS and BALL
fields beneath it.

At the title, during demos, and on the high-score screen, the DMD turns a
carousel: the head from the game's ending, and then the five persisted high
scores for three seconds each. Pages show Grand Champion or High Score #1
through #4, initials, and the new digits. A turn begins when the carousel
comes up, at startup and when the game returns to those screens after a
session and any name entry, so the head always comes before the scores.
Attract presentation persists through the cartridge's six-frame RAM clear
between Best Five and the title. Only a non-demo table or bonus session with
balls remaining switches the DMD to live play. This prevents a false GAME OVER
flash during the attract loop while preserving real game-over presentation
after a live session.

### The ending's head

The head is the opening of the maximum-score ending. Score saturation raises
`$2CC0` bit 7 (`00:F5A9`); the session loop takes `01:C195`, the table blows
up, and screen `$2054=5` comes up black. Over the next 190 frames the Giant
Face fades in alone, four balls close on the jewel of its helmet and six
bursts go off over it, and then the screen is cleared for the scene that
follows. `render::EndingHeadAnimation` draws those 190 frames from the ROM
with no emulator behind it, and the DMD shows them at the game's own size
with the face in the middle of the panel and nothing else on it. The game
cuts from the last of them straight to black, which on the panel would be a
cut to the scores, so the panel adds a second of its own: the face goes out
by the same fade played forwards, a step every other frame, and the panel
stays dark for the third of a second left. The panel is
wider than the game's screen, whose edges are not applied: the ball from the
left is on the panel from its first frame, as the one from the right is on
the game's screen.

The ending loads no patterns. The face is the table's own, resident pack 0
(asset `$1222`, at VRAM `$7C00`): seven tiles by nine at screen (88, 64), tile
`$7C0 + 7 * row + column`, less its corners. Everything else on the table's
map is still there and is black because only background palette 1 is lit.
The balls and bursts are in the table's sprite set, the stream at `0D:862D`
(VRAM `$3800`). The three palettes are together at `07:4B49`: the face's, and
sprite palettes 0 and 14.

The face fades in over 21 steps, at frames 2 and 4 and every third frame to
61. A step short of whole takes one level off one channel of each colour, in
the order blue, green, red, passing over a channel with none left; the fade
plays that backwards. The balls are actors `$04-$07` under `07:4261` and are
on screen from frame 79: 63 frames of flight, straight along one axis at 2
or 1.5 pixels a frame and bowed along the other, where they start at two
pixels a frame and lose a sixteenth each frame. They turn through patterns
`$1E0-$1E6` four frames apiece, meet at the jewel, and shrink there through
`$228`, `$22A` and `$232` (`07:428E`). From frame 151 six type-`$8F` bursts
(`07:49F4`) run nine patterns each, the first two a frame ahead of the other
four, and the last is gone at frame 187. A sprite written in one frame is
shown in the next, which is why these are one later than the frame work RAM
has them.

None of it is random or reads the session. `dc_rom_dmd_panel` drives the
cartridge into its ending and compares all 190 frames with the emulator's
framebuffer, pixel for pixel at the VCE's three bits a channel.

When a game over's score ranks, the DMD shows the name entry instead: the
rank, the three letters with the one under the cursor blinking and
underlined, the new score, and ENTER YOUR INITIALS. The letters cannot come
from the table. The entry loop keeps them at `$2D66-$2D68`, with the cursor
at `$2D65` and the rank at `$2D61`, and copies them into the name rows only
when it ends -- at the third letter (`09:6791`) or on its timeout
(`09:6683`) -- so until then the table still holds the rank's old name, `AAA`
after a fresh boot. Observe hooks at `09:6629`, where the entry is set up,
and `09:6698`, where it has ended, bracket it; the page stays up with the
finished name until the High Scores screen gives way. The screen itself is
drawn three frames before `09:6629`, while `09:6605` is still sorting, so the
DMD keeps the game-over page for up to ten frames when High Scores replaces
a live session, rather than flashing the carousel first. The letters scroll
from `A` past `Z` to `$5B`, drawn as a full stop, and `$5C`, drawn blank; the
carousel shows them the same way.

The presentation reads scores from `$2205/$220C/$2213`, selected by `$2251`,
and multiplies stored units by 100. Base bonus `$2D50` is also in hundreds.
The standard three-ball countdown at `$2D52` becomes BALL 1, 2, then 3.

Award scenes replace the regular DMD page for the original sequence's duration:

- SPECIAL POINTS uses `$2053.2` and the million-point multiplier at `$20CA`.
  The skill-shot capture selects this page on frames 524–818, exactly matching
  the original award interval. EXTRA BALL follows `$2053.7`.
- Screen selector `$2054=2` identifies the end-of-ball tally or match screen.
  `$2D71` distinguishes match; otherwise the DMD shows the remaining bonus
  `$2D69–$2D6B` and the base-bonus, target-count, and multiplier calculation.
- Bonus-stage results begin at `04:456C`, observed through a read-only entry
  hook. The session's bit 1 is also present during stage entry and is not a
  safe result gate. The DMD shows YOU GET / BONUS POINTS, PERFECT / SPECIAL
  POINTS, or SORRY / YOU LOST YOUR BONUS POINTS. It follows the remaining
  award at `$2DB7–$2DB9` and keeps a successful result successful when the
  counter reaches zero.

During tallies, match, and bonus-stage results, the playfield retains its last
complete table or bonus-room image instead of switching to the original
result viewport. Normal rendering resumes after the sequence. If the DMD is
closed or unavailable, the original result viewport remains the fallback so
awards are still visible. The observer changes no game state or award timing.

`dc_rom_dmd_panel` checks all ten digits, distinct score/award palettes,
maximum-value margins, scene transitions, and successful/failed bonus payouts.
Its real-ROM fixtures cover scored, perfect, and lost results, and compare
all 3,600 RAM snapshots with and without the result observer. Optional review
artifacts can be generated with `dc_dmd_panel_tests <rom> <output-directory>`.

## Music overlay and bonus transitions

The experimental music overlay observes executed sequencer stores, including
stores inside native transcripts. It records each voice's envelope and fade
volume before the shared `$2E37` scratch byte is reused. Channel ownership
comes from the actual `$2E22-$2E27` latch writes: a silent effect can leave its
channel available to music, so treating it as occupied would double that
music in the overlay. Only slots visited during the current sequencer pass
can supply overlay voices. This prevents exclusive announcements from
resurrecting stale notes left in lower-priority slot records. A pass is
committed at the first PSG write after the sequencer's JSR into the `45:4142`
upload walk, not at a channel-zero select: while a timer-driven voice holds
channel zero, as the end-of-game and game-over tunes do for seconds at a time,
the walk skips that channel, and waiting for its select left the previous
pass's reconstructed voices sounding.

Perfect bonus results also expose an original double restart. Table return
resumes music through `01:C201`, then the star controller calls `03:7CF6`
120 frames later to resume it again. The cabinet observes completed table
return at `01:C204` and suppresses that second audio call only when table
return already resumed music for the pending star. Ordinary star resumes
and returns without a perfect result retain their normal behavior.

`dc_rom_music_overlay` compares original PSG writes and every RAM snapshot
with the overlay enabled and disabled, exercises the bonus-6 and game-over
music commands, and checks that the perfect-result transition starts table
music once while preserving score, ball position, and gameplay flags.

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
flipper pieces at `$04-$09`, the Front and Side Spiders at `$26-$29`, and
the selected upper-table families below.

The upper-table Wall Pockets (the four rotating portals at `$3E/$3F/$42/$43`)
and Little Skulls (`$44-$4A`, including their hidden and pair controllers)
run before the first camera visit. This lets the portals rotate and the skull
targets spawn while the ball is still on the lower table. Their original
animation, collision, regeneration, and Big-Star selection logic remains in
control. The Big Star (`$4B`), Sorcerers (`$16-$1E`), and upper-left Gray
Blobs (`$50-$56`) also run from the start.

The middle-table Side Stars and Walled-Star access portal (`$10-$12`),
plus the two Giant-Face helmet openings (`$2A/$2B`), likewise keep their
four-tick idle rotation running before and after a camera visit. The wall
and Giant-Face controllers retain their existing behavior; portal contacts
still drive the original entry, transformation, scoring, and ball-return logic.

In the stitched cabinet view, skeletal Small Soldiers draw in front of the
middle portals as they spawn, with the ball and flippers still above them.
This presentation order applies with smoothing on or off; it does not change
actor state, collisions, or the base emulated viewport.
The middle-table Three Blobs (`$58-$5A`) draw above the flippers, with the
ball above them, using the same ordering in both presentation modes.

The cabinet freeze census reports first off-camera artwork and first camera
visit for the pockets and skull targets, including static targets omitted
from its animation ranking (artwork frame 0 means none observed). On the
2,100-frame census route, both uppermost pockets and all four skulls render
at frame 873 without a camera visit; the uppermost pockets continue animating
with no missed artwork changes.

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
The three color-changing Blobs at slots `$58-$5A` use a camera-latched policy:
they retain the original cull until bit 7 is set and their Y lies within the
original camera interval (`camera - 48 <= Y < camera + 240`). Initialization
can set bit 7 for an off-camera orb, so the flag alone is insufficient. Once
both checks confirm activation, cabinet mode clears bit 1 so their floating animation keeps
running after the ball leaves. This avoids the incorrect start-of-session
state caused by unculling them before their first activation.

The `out/skill-shot` capture exposed an initialization race in the old
flag-only check. Frame 93 ended with `$30D8=$86`, orb Y=254, and camera
Y=480. The host cleared the cull on frame 94 even though the orb was outside
the camera, letting it move while the ball was still in the shooter. With
the position check, the orbs retain their original positions until genuine
camera activation at frame 465; the host latches that activation on frame
466. All three orb positions and the ball position match a registry-disabled,
original-instruction-boundary replay throughout the corrected 1,202-frame
capture. Their animation still advances after the camera leaves.

The same capture's special-points interruption is original game behavior:
`$2053.2` stays set during frames 524–818 (295 frames, about 4.9 seconds),
with or without the native registry or cabinet camera policy. `01:C1B0`
branches through `01:C223` into `01:DDDD`, which runs the award presentation
and counting sequence with explicit frame waits instead of the normal actor
loop. This verifies the emulated pause; the input capture contains no host
frame-duration measurements and cannot rule out additional rendering stalls.

`--cabinet-seed-check` measures the blast radius: the named-slot policy leaves
each latched family under the original cull until its first camera activation,
where the wholesale lift moved several hundred pixels before the game had put
those mechanisms into their live state.

`--cabinet-flipper-trace` measures this directly, booting the census route to
frame 1,300 and then running 600 frames while holding each pad input the
flippers respond to in turn (Button I carries `$3B80` mask `$01` for the
right-hand pieces, any direction carries `$F0` for the left):

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

### Freeing the flippers during the launch

Lifting the cull did not make the flippers answer while the ball is still in
the Devil Shooter, and the reason is a separate gate. The actor scheduler
publishes the held pad byte to `$2CEE` once a frame and blanks it outright
whenever the shooter owns the ball:

```
01:C4F1  LDA $2CC7        tilt lock
01:C4F4  BNE $C4FC
01:C4F6  LDA <$53
01:C4F8  AND #$08         the Devil Shooter session
01:C4FA  BEQ $C4FD
01:C4FC  CLY
01:C4FD  STY $2CEE
```

So from the moment a ball is served until the shooter session ends -- 98
frames after the launch button is let go, on the census route's first ball --
every control piece reads zero input no matter what is held. The camera cull
is real and does bite partway through that window, but it is not what is
holding the flippers: with the cull lifted and this gate left alone the
flippers still do not move.

`$2CEE` is read at exactly four addresses in all 384 KB -- `01:D466`,
`01:D49B`, `01:D4AA` and `01:D4D4` -- and every one of them masks it with
`$3B80,X` on the next instruction. It is the control pieces' input byte and
nothing else's, so putting bits back into it can reach nothing but them.

The host therefore hooks `01:C500`, the instruction after the store, and ORs
back the bits the two configured flipper masks select. Everything else the
blanking covers stays blanked, so the shooter still charges on a held button
and no other actor sees an input it did not see before. The masks are the
host's rather than the cartridge's because they are the ones that describe
keys; on a configuration that leaves `$3B80,X` alone the right-hand pieces
carry `$01`, which is Button I, which is also the launch button, and there the
right flipper rises while the shooter charges. The default bindings send
neither flipper a Button I -- so that a flipper key can never confirm anything
-- and do not have that overlap.

`--cabinet-launch-flipper-probe` measures it, launching from the census route
and then pulsing a flipper input ten frames on and ten off:

| camera cull | shooter input gate | first flipper movement | phase changes |
| --- | --- | ---: | ---: |
| original | original | +102 frames | 69 |
| lifted | original | +102 frames | 66 |
| original | lifted | +2 frames | 100 |
| lifted | lifted | +2 frames | 96 |

The gate is the whole of it, and lifting it is worth about 1.7 seconds of
flipper control per ball. This is host code and no recording route installs
it, so the state census still hashes to
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`.

#### The port swallowed this hook once

Porting `01:C4DE`-`01:C50E`, the actor scan's preamble, broke this gate
silently and completely. `01:C500` is an interior instruction of that range,
and a routine that runs as a native transcript never presents its interior to
the interpreter -- the walk leaves `01:C4DE` and hands back a program counter
of `01:C510`. So the hook fired on every frame before the slice and on none
after it, and the flippers went dead during the launch again.

Nothing failed. `install_execution_hook` still succeeded, because `01:C500` is
not a registry entry and so collided with nothing; the collision check only
covers entries, and this address was in the middle of a body. **An address is
hookable only if the interpreter still arrives at it, which for a ported range
means its entry and whatever it hands back to, and nothing in between.**

Neither of those was usable here. `01:C4DE` is before the store, and every
address between the store and the four reads -- `01:C510`, `01:C52C`,
`01:D466` -- is a registry entry itself, and a key carries one hook. So the
host claims the preamble back through `install_native_registry`'s
`claimed_routines` and runs it interpreted. That costs 775,224 master cycles of
a 1.29-billion-cycle route, 0.06% of a session, and the host says so on the way
up rather than quietly deploying one routine fewer:

```text
Native registry: 398 of 399 admitted routines deployed (0 withheld as
state-diverging, 1 claimed by the host).
Registry claim: 01:C4DE claimed; the shooter's input gate hooks 01:C500 inside
it, and a native transcript never presents its interior to the interpreter.
```

`dc_rom_host_registry` checks that arithmetic -- deployed equals admitted minus
withheld minus claimed, and every claim is named -- because a routine that
quietly stopped deploying would otherwise be indistinguishable from one
deployed and never entered.

The probe below missed all of this for as long as it ran without the registry,
and now deploys it. **A probe that measures a host behaviour has to deploy the
registry, or it measures a machine nobody plays.**

`02:C020` is a second copy of the same scheduler with the same gate. It is
deliberately not hooked: the 3,600-frame census route never executes it, and
an unmeasured second site would be a claim rather than a change.

### Screens raised over a live session

Opening the High Scores screen used to corrupt the cabinet playfield: the
table's shape stayed on screen with most of its artwork replaced by the
letters of the words "Number 1". The same picture appeared at the end of the
attract demo, which is the other place the cartridge raises that screen.

Neither was a rendering fault. Cabinet mode substitutes a reconstructed table
for the emulator's picture whenever `decode_cabinet_state` says the playfield
is the table background, and the rule it used -- table attributes initialised,
ball count non-zero, no bonus stage -- is a test for *a session existing*, not
for *a table being on screen*. The High Scores screen leaves the ball count
and the attribute plane exactly as the session left them, so the rule stayed
true. Meanwhile the screen rewrote pattern VRAM with its own font. The
reconstruction composes the table from the ROM tilemap and resolves each
tile's pixels from live VRAM, so every table tile drew whatever glyph now sat
in its VRAM slot -- which is why the corruption reads as text.

`--cabinet-view-probe <directory>` measures it without needing to know which
screen is at fault. It runs three routes, and for every frame the rule draws
as the table it crops the reconstruction to the camera window the way
`--cabinet-oracle` does and counts the pixels that disagree with hardware.
Ordinary sprite error is a few per cent; most of the frame disagreeing means
the cabinet is not showing the game. It writes both pictures for the frames
that fail, so the two can be put side by side.

`$2054` is the cartridge's own screen selector -- 0 the table, 3 the title and
attract screens, 4 the High Scores screen -- and adding it to the rule
separates the two exactly:

| route | table frames | disagreeing by 25%+ | `$2054` seen |
| --- | ---: | ---: | --- |
| attract, before | 6,104 | 363, frames 5,212-5,574 | `$00` 5,741, `$04` 363 |
| attract, after | 5,741 | 0 | `$00` only |
| census, either way | 2,728 | 3 isolated frames | `$00` only |

The 363 disagreeing frames and the 363 frames at `$04` are the same frames,
and the two high-score renderers at `09:69B2` and `09:69FA` run inside that
span. Six seconds of wrong picture per attract loop, and for as long as a
player left the screen open.

The census route is the one every reconstruction measurement in this document
is made against, and it keeps all 2,728 of its table frames: the new condition
costs nothing where the reconstruction was already right. Its three isolated
disagreeing frames -- 1,451, 2,251 and 3,051 -- are a separate, pre-existing
artefact of the camera crop on scroll-wrap frames, not a wrong screen; the
picture dumped for each is an ordinary table.

### Keeping the high-score table

The cartridge keeps its high-score table in work RAM at `$2200`, as six
parallel rows of seven bytes:

| Row | Holds | Row | Holds |
| --- | --- | --- | --- |
| `$2200` | score, low byte | `$2215` | name, first character |
| `$2207` | score, middle byte | `$221C` | name, second character |
| `$220E` | score, high byte | `$2223` | name, third character |

Columns 0-4 of a row are the five ranked entries; columns 5 and 6 are the two
players' live values. The selection sort at `09:6B6F` runs X over 0-5 and Y
over X+1-6 and swaps all six rows together, which is the entire ranking
mechanism: a finished player's score walks into the ranked columns and the
loser falls out the far end. `01:C107` lays the ROM's defaults over all 42
bytes at boot — five entries of ten thousand stored units, which the panel
shows as 1,000,000, all named `AAA`.

Nothing else writes the ranked columns. A work-RAM write census over the
3,600-frame route with `--work-ram-write-range 2200-2229` records 210 writes
into the block: 42 from the work-RAM clear at `01:C100`, 42 from `01:C107`,
and 126 that all land on columns 5 and 6 as the two players' scores
accumulate.

What the cartridge does *not* do is keep the table. Its only persistence is
the save screen the player reaches by pausing, opening High Scores with
Select, and pressing Button I on a numbered slot. That path packs the whole
42-byte block into a backup-RAM record — `09:649B` is `TII $2200,$2D78,$002A`,
and the Data Load path at `08:5C4E` is the same transfer backwards — and the
records live in two ordinary backup-RAM files, `DCRASH1` and `DCRASH2`, of
five 48-byte records each. Spending one of five slots, by hand, to keep a
score table was a reasonable trade against a HuCard's two kilobytes of
battery-backed RAM. It is not a trade a PC has to make.

So the host keeps the table itself. `devils-crush.hiscore` holds
the thirty ranked bytes behind a magic, a format byte and a rank count; it is
written atomically whenever the table changes and read back at boot. The
restore hangs on `01:C10E`, the instruction after the boot-time transfer,
which is the one moment when overwriting the whole table is safe — the
defaults are in place and nothing has read them yet. Only the ranked columns
are carried. Columns 5 and 6 belong to a session rather than to the machine,
and restoring a stale live score into them would be visible.

The save side samples twice a second and waits for two quiet samples before
writing, so scrolling through the letters of a name costs one file write
rather than one per letter, and a table that changed inside the last second of
a session is still flushed on the way out. The five original save slots are
left entirely alone: this is a separate host file, so a player who does use
the cartridge's own save screen still has all five.

`--high-score-check` measures both halves against the ROM without needing a
game over:

```
cartridge defaults after a boot:
  1     1000000  AAA
  ...
stored table after a boot:
  1   119304600  ABC
  ...
  boot hook entered:        yes
  stored table installed:   yes
  live columns unchanged:   yes
  restore left nothing due: yes
  changed table written at: frame 90 (settled)
  written table matches:    yes
```

Like the flipper gate, this is host code that no recording route installs, so
the state census still hashes to
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`.

One interaction is worth knowing rather than hiding: the cartridge's own Data
Load puts back all 42 bytes of the block, ranked columns included. Loading an
old save therefore replaces the current table with whichever one that slot was
written with, and the host then saves what the game now holds. That is the
cartridge's behaviour, not something the host adds, and it only happens when a
player deliberately loads a save.

## Deploying to a cabinet

The cabinet is a separate machine from the workstation the port is developed
on, and the workspace is far too large to sync. `tools/Publish-Cabinet.ps1`
builds the `windows-cabinet` preset and stages a folder that stands alone:

```powershell
.\tools\Publish-Cabinet.ps1
```

It stages to `..\devils-crush-cabinet` by default, refusing any destination
inside the workspace, and accepts `-Destination` and `-RomPath`. `-SkipBuild`
restages an executable that is already built.

The staged folder holds six files and about 9 MiB:

| File | Why it is there |
| --- | --- |
| `devils-crush.exe` | The whole game |
| `Devil's Crush.pce` | Proprietary, so never part of the CMake install set |
| `LICENSE`, `THIRD_PARTY_NOTICES.md`, `README.md` | GPL-3.0 distribution set |
| `.stignore` | Keeps runtime state out of the sync |

The `windows-cabinet` preset differs from `windows-vs2022` in three ways:
Release only, tests off, and `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`. SDL3
and Beetle PCE were already linked statically; folding in the Microsoft C
runtime removes the last file the cabinet would otherwise have had to obtain
separately. `dumpbin /dependents` on the staged executable lists
only DLLs that ship with Windows — no `MSVCP140.dll`, no `VCRUNTIME140*.dll`,
no `api-ms-win-crt-*`. Nothing needs installing on the cabinet.

Because it uses its own build directory, the first publish rebuilds every
dependency and takes as long as a clean configure. The workstation's
`windows-vs2022` tree, and therefore the oracle and the test suite, is left
untouched.

### Runtime state is per-machine

Every path the host reads or writes at run time is relative to the working
directory: `devils-crush.ini`, `devils-crush.sav`, `devils-crush.hiscore`,
`devils-crush.state`, `gamecontrollerdb.txt`. Run the executable from inside
the staged folder and they are written there, beside it. Earlier builds kept
them in a `portable-data` subfolder, with the settings as `cabinet.ini`; the
game and the launcher move them out on first start
(`core::migrate_legacy_data_folder`), and remove the folder if that empties
it.

Syncing it back would be actively wrong. Display assignment records SDL display
names and window rectangles for the machine that wrote it, so a workstation
push would overwrite whatever the cabinet was configured with, and the
cabinet's assignment would replace the workstation's on the next pull. The
generated `.stignore` excludes those files (with the `.tmp` each is written
through), `portable-data`, `logs`, and `captures`, so each machine keeps its
own display layout, high scores, and quick states. Syncthing does not sync
`.stignore` itself: the cabinet's copy needs the same patterns.

That also means display assignment is a one-time step on the cabinet itself:
launch with `--cabinet`, press F12, and assign the playfield, DMD, and
backglass to the cabinet's own displays as described above.

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
