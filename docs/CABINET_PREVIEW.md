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
- the live-ball count at `$2D52` is nonzero;
- session bit `$53.4` does not request a bonus stage; and
- the cartridge's screen selector at `$2054` reads 0, the table.

Menus, session-end presentation, and bonus stages therefore fall back to the
emulated framebuffer. This reuses original presentation and gives bonus
stages the required full playfield window without a second simulation. The
last of those four conditions is the one that took a defect to find; see
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
| X | button II | — | Nudge | — |
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
bit 3 marks the Devil Shooter session. That bit marks the shooter session and
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

### Nudge

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

Bindings live in `portable-data/cabinet.ini` under `input.`, one line per pad
input, holding a comma-separated list of SDL key names:

```ini
input.one=Return
input.two=X
input.select=Backspace
input.run=Return
input.up=Up
input.right=Right,Right Shift
input.down=Down
input.left=Left Shift,Left
input.left_flipper=left
input.right_flipper=right,up
input.launch=Return
```

The two `_flipper` lines name pad inputs rather than keys, from the same set
as the `input.` suffixes above.

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

`cabinet.ini` is rewritten in full on exit and after display assignment, so
the bindings are a stored field rather than a passthrough: an existing file
without them gains the section the first time the host exits normally.

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

To pick up changed defaults, delete the `input.` lines from `cabinet.ini`.
They are rewritten from the defaults on the next exit, and the window
placements above them are untouched.

### The mouse cursor

The host hides it at startup. Nothing here reads the mouse — not play, not
the F12 display assignment, which is entirely keyboard-driven — so a pointer
left on the playfield is only something to look at, and a cabinet has no way
to move it off. `SDL_HideCursor` is application-wide, so one call covers the
DMD and backglass windows too, and `SDL_Quit` restores it on exit.

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

## Backglass animation

The backglass shows the native title-logo animation, and nothing else. Black
is the fallback when it cannot be decoded or rendered; closing or losing that
window still does not affect emulation.

A user-supplied backglass image was supported for a while — a `backglass.path`
field in `cabinet.ini`, a `--backglass <image>` switch, and a file picker on
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

`cabinet::decode_cabinet_state()` selects the score slot through `$2251` and
reads the three-byte little-endian score arrays at `$2205`, `$220C`, and
`$2213`. Stored units are multiplied by 100, preserving the verified original
display rule and the maximum visible score of 999,999,900.

The current DMD omits the P1/P2 discriminator until the two-player session
field is dynamically mapped. The score remains live during emulated
framebuffer and bonus-stage presentation because it is read independently
from work RAM.

### Digit layout

`cabinet::calculate_dmd_digit_layout()` places the seven-segment digits. A
digit is three units wide by five tall and neighbours are separated by a fifth
of a digit width, so a row of `n` digits spans `0.72n - 0.12` digit heights.
The field may use 90% of the window's width and 75% of its height.

Both bounds have to be taken. Taking only the height bound is what made a
fullscreen DMD illegible: at 0.75 of a 1080-pixel panel a digit is 810 pixels
tall, and nine digits at that size span 5,152 pixels across a 1,920-pixel
panel, so most of the score fell off both edges. The width bound was present
but computed as `available_width / (0.72n - 0.12) * 5 / 3`. The `0.72n - 0.12`
coefficients are already expressed in digit heights, so the trailing `5 / 3`
applied the digit aspect a second time and inflated the width-limited height
by 67%, leaving the field 1.5 times wider than the window whenever that bound
did bind.

Digits are sized against `kDmdScoreFieldDigits` rather than the count actually
being drawn, so the score does not rescale itself each time it gains a digit.
A short score is centred within the window, not left-aligned in the field. If
a score longer than the field is ever decoded the field widens to hold it.
`test_dmd_digit_layout` asserts that both bounds hold for one to twelve digits
across five output shapes: the default 960-by-240 window, a 1920-by-360 DMD
strip, fullscreen 16:9 and 16:10 panels, and a portrait-rotated panel.

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

So the host keeps the table itself. `portable-data/devils-crush.hiscore` holds
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
directory: `portable-data/cabinet.ini`, `portable-data/devils-crush.sav`,
`portable-data/devils-crush.hiscore`, `portable-data/devils-crush.state`. Run
the executable from inside the staged folder and that directory is created
there on first use.

Syncing it back would be actively wrong. Display assignment records SDL display
names and window rectangles for the machine that wrote it, so a workstation
push would overwrite whatever the cabinet was configured with, and the
cabinet's assignment would replace the workstation's on the next pull. The
generated `.stignore` excludes `portable-data`, `logs`, and `captures`, so each
machine keeps its own display layout, high scores, and quick states.

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
