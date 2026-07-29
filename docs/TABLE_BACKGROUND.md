# Table Background Map

Last updated: 2026-07-28

## Current boundary

Milestone 3 starts from a read-only frame-boundary copy of the base PC Engine
video state:

- 32,768 HuC6270 VRAM words (64 KiB)
- 512 raw 9-bit VCE palette entries
- VDC memory-width register (MWR)
- Background X and Y scroll registers (BXR/BYR)

`PceRuntime::export_video_memory()` returns owned vectors. No
emulator-managed pointer crosses the runtime boundary.

The downstream accessors added by
`0004-vdc-video-snapshot.patch` do not synchronize or mutate the core. Debug
and Release canonical framebuffer tests remain unchanged.

## Decoder contract

`render::decode_pce_background()` implements the base HuC6270 background
format:

- One 16-bit BAT entry selects a 12-bit 8-by-8 tile and a 4-bit subpalette.
- Each tile occupies sixteen VRAM words. Rows 0-7 contain bitplanes 0/1 and
  rows 8-15 contain bitplanes 2/3.
- The leftmost pixel consumes bit 7 of each plane.
- A nonzero pixel selects `subpalette * 16 + pixel`.
- Background pixel zero always selects VCE entry zero, regardless of the BAT
  subpalette.
- Raw VCE colors use three bits each in blue, red, green order and expand with
  the core's exact `component * 36` rule.
- Dormant BAT cells may reference a pattern beyond the configured 2,048-tile
  VRAM cache. Beetle leaves those cache entries zeroed; the decoder mirrors
  that blank-tile behavior.

The renderer also provides signed-world-coordinate composition and exact
bounded crops. Later regions own overlap pixels, making stream seams
deterministic.

## First live route

`dc_rom_video_snapshot` follows the established 3,600-frame state-census
input. It copies and decodes video state at frames 900, 1,200, 1,500, 1,600,
1,800, 2,100, 2,400, 2,700, 3,000, 3,300, and 3,600.

Every checkpoint reports MWR `$10`, selecting a 64-by-32 BAT: a 512-by-256
pixel circular background window. BXR remains zero. BYR is respectively
`0, 0, 345, 210, 290, 394, 0, 427, 0, 425, 330`.
Those BYR values are exactly the low nine bits of signed camera integer
`$0C:$0B`:
`0, 0, -167, -302, -222, -117, 0, -85, 0, -87, -182`.

Frame 900 is the first fixed live golden:

| Artifact | SHA-256 |
| --- | --- |
| VRAM words | `f452889188abdf446a4caf85f5aac51e3afa1cd8ccc1c86ce6044562a0a7d900` |
| VCE palette | `be9d93e01ba3e5f26cfaaa6df5f6107bd05f41a8ec8f29d6f8ff8b88e14f710f` |
| Decoded 512-by-256 BAT | `f812ec7fdba43453dbd1dc926fdb8adf2d4ef400d7631b8ca7ebc49f18446c17` |

## Streamed-row owner

CPU bus-write observation identifies `00:F400` as the 32-cell BAT-row
transfer. Each call:

1. Selects VDC MAWR and writes the destination word from zero-page
   `$2D:$2E`.
2. Selects VDC VWR.
3. Iterates Y from 0 through `$3F` in pairs.
4. Reads tile low bytes through pointer `$2F:$30`.
5. Reads packed high-bit/palette nibbles through pointer `$2B:$2C`.
6. Writes exactly 32 low/high BAT words through VDC ports 2/3.

The 3,600-frame route completes 539 calls from frame 1,201 through 3,600.
Every call returns at `00:F43C`; its caller paths are:

| Caller | Calls |
| --- | ---: |
| `00:F2FE` | 286 |
| `00:F32D` | 5 |
| `00:F3B2` | 232 |
| `00:F3E1` | 16 |

`dc_rom_video_row_writer` guards four VDC setup writes and 32 word pairs per
call: 539 writes each at `F400/F405/F40A/F411`, and 17,248 writes each at
`F417/F434`. It is Release-only because all-bus and bounded-call observation
is intentionally expensive in an unoptimized build. The separate
`dc_rom_video_snapshot` live decode/golden test runs in both configurations.

## World-row mapping

The four callers are two directional pairs:

| Caller | Direction | World row |
| --- | --- | --- |
| `00:F2FE` | Downward, first row | `floor8(anchor_y) + 240` |
| `00:F32D` | Downward, second row | `floor8(anchor_y) + 248` |
| `00:F3B2` | Upward, first row | `floor8(anchor_y) - step`, where step is 8 or 16 |
| `00:F3E1` | Upward, second row | `floor8(anchor_y) - 8`; reached only for a 16-pixel step |

Every one of the 539 dynamic calls satisfies
`anchor_y - camera_y = 480`. A world row is written to:

```text
BAT word = (((world_y - 480) & 0xff) / 8) * 64
```

The route covers every eight-pixel world row from 168 through 720. It also
locks both source streams:

- Tile-map words use MPR4 physical bank `$14` and logical address
  `$800A + world_y * 8`.
- Packed palette nibbles use the work-RAM mapping in MPR1 `$F8` and logical
  address `$2259 + world_y * 2`.

Each tile row contains 32 little-endian tile words. The ROM supplies the low
12 tile-number bits. One work-RAM byte supplies two palette nibbles, high
nibble first. This establishes a finite 32-by-96-cell source envelope, but
does not by itself prove that all 96 rows are visible table world data or
that one VRAM/palette snapshot can render the entire envelope faithfully.

## Stable regions and accepted extent

`decode_packed_pce_background()` combines the ROM tile words, packed
work-RAM palette nibbles, copied VRAM patterns, and copied VCE palette. The
first full-source export used frame 900 and produced a reproducible
256-by-768 image with SHA-256
`f0df872362bcc231674151eecdd402752d1bc18dd9e333e5a95fb872dad54cee`.
Frame 900 is still in the table fade-in, so most VCE colors are black. This
hash now identifies a diagnostic fade-state artifact, not an accepted
playfield.

The frame-900 packed-source crop `(0,480,256,208)` matches the same decoder
applied to the live BAT and has SHA-256
`da841334896409730877e48a19d89cfa4bacfde153b3157e31c2b126c03bbb2d`.
That comparison proves decoder and source-map self-consistency only; it is
not independent evidence against Beetle's rendered background or final
framebuffer. The last 32 lines of that 240-line live-BAT crop are outside
the 208-line playfield source window and must not be appended to the table.

The accepted background world is 256 by 668 pixels at source coordinates
`(0,20)-(255,687)`. The settled frame-1,200 accepted crop has SHA-256
`3d5afa6f1f633e23b837c1f3ec3e0a19c8f00ca87abf2a7ff841f600dadc7e1e`.
This is now bounded at both camera clamps:

- The ordinary route reaches camera zero. Its 208-line playfield therefore
  displays world rows 480-687.
- A separate sequential runtime route holds only slot 0's ball Y at 64 every
  ten frames from frames 1,200-3,000, then lets the original camera logic
  converge. At frame 3,420 the original camera stops at -460, its paired
  stream anchor is 20, and the playfield displays world rows 20-227.
- The natural frame-1,600 camera position -302 independently covers rows
  178-385. Together the captures overlap across the upper, middle, and lower
  source regions.

Rows 0-19 are the decoded top guard/prefetch cap and cannot enter the
clamped viewport. Rows 688-720 are lower streamed prefetch: the natural
`00:F400` route writes them, but camera zero prevents them from becoming
visible. Rows 721-767 are outside both the observed streamer and camera
range and contain unrelated or invalid tail data. The fixed score/presentation
area is not part of this world image.

## Independent composite comparison

The final Beetle framebuffer is an independent rendering path: HuC6270
background fetch, scrolling, sprite priority, display timing, and RGB565
conversion occur inside the emulated core. The comparison quantizes the
native decoder's ARGB colors through the same RGB565 round trip, then aligns
the table crop with the 208-line playfield at framebuffer Y 30.

Exact post-fade evidence is:

| Capture | World rows | Native source SHA-256 | Exact background pixels | Overlay pixels |
| --- | --- | --- | ---: | ---: |
| Natural frame 1,200 | 480-687 | full source `e11ec46a715e311d584c19c4ddb78750b9e18e6a607f22db7d70de8dddf74a6f` | 44,901 | 8,347 |
| Natural frame 1,600 | 178-385 | full source `750ba5022e7604636f97107a374df21262ede4a5aa2ac1e7cc64cb9b9decf25d` | 47,768 | 5,480 |
| Natural frame 1,800 | 258-465 | full source `910a3372d4c516f6076bff49556dfd289915014c1cf652a8c8df921fa538bb44` | 49,432 | 3,816 |
| Upper-route frame 3,420 | 20-227 | crop `f2f8fda9090a1f98f1cf07ece0e775122c43257d38caef7f33f2d63c3ff81aec` | 44,247 | 9,001 |

Each row totals 53,248 compared pixels. Exact matches are background pixels
rendered by two different implementations. Mismatches retain the original
score, sprite, priority, animation, and transient-overlay output; they are
not treated as background-decoder failures. Both Debug and Release reproduce
the same counts and hashes.

## Sprite and actor overlay boundary

The cabinet background cache needs only the accepted 256-by-668 crop plus
the stable pattern and palette snapshot used to build it. A faithful
emulator-composite oracle additionally needs a copied active HuC6270 sprite
attribute table:

- 256 internal SAT words, preserving the post-DMA table actually consumed by
  the renderer rather than only the DVSSR source address in VRAM.
- Sprite patterns from the already copied 32,768 VRAM words.
- Sprite palettes from VCE entries 256-511, already present in the copied
  512-entry palette.
- SAT Y/X, pattern, palette, priority, flip, width, and height fields plus
  the camera/world transform.

The copied frame boundary now includes those 256 post-DMA SAT words. The
native decoder implements pattern addressing, 16/32-pixel widths,
16/32/64-pixel heights, both flips, transparent color zero, sprite palettes,
SAT-order sprite priority, the 16-sprite-half scanline limit, and the
foreground/background priority bit. The packed background decoder retains one
raw opacity byte per pixel for that last decision. The compositor translates
the current 208-line playfield viewport through `$200B:$200C` into
table-world coordinates and clips the score/status band.

At the settled frame-1,200 lower-table checkpoint, adding the SAT layer raises
exact agreement with Beetle's 53,248-pixel playfield composite from 44,901 to
51,778 pixels. The copied SAT SHA-256 is
`26ad08bcac982e81b34e02f899e16d790f148562daaa17e5651b92e5f6248276`;
46 visible sprites produce 9,175 opaque writes after priority rejection. The
remaining 1,470 pixels cover other transient presentation outside the packed
table plus live SAT boundary.

The cabinet renderer does not advance a second actor simulation. It now
decodes all 127 mapped work-RAM actor slots and resolves exact bank-8 ROM
descriptor groups for fixed slots `$0A-$61`, flippers `$04-$09`, and admitted
dynamic actors. It no longer proximity-associates nearby selected SAT
entries. Descriptor primitives remain tied to the actor's
type/appearance/phase/dispatch signature, while their signed work-RAM world
position remains live after the camera moves away. The cache stores resolved
pattern pixels and VCE palette indices, not references to mutable regional
VRAM. Colors resolve from the current VCE palette so initial fade-in seeds do
not remain dark. Actor presentation Y includes the score/status band and is
normalized by 30 lines before entering stitched-table space. Empty or
unseen-changed actors suppress stale art.

The retained layer is composed first. Camera-observed actors are omitted from
that layer and the copied live SAT is composed last, so it remains
authoritative without suppressing unrelated inactive mechanisms merely
because their pixels overlap the 208-line camera band. This removes the
camera-dependent flipper gaps and prevents clustered Sorcerers and
star/pentagram parts from acquiring a neighbor's geometry.

An off-camera phase change does not make the current region's VRAM a valid
pixel source. Animated actors keep a bounded set of exact phase visuals and
must resolve the ROM-owned current phase or restore the learned visual for
that same phase. This prevents moving enemies from sliding a frozen image.
Only static mechanisms may retain a prior aligned visual when a phase has no
independent artwork.

The source presentation path is now mapped at `08:4036/40DF`: its 69
appearance pointer tables and phase-selected descriptors reproduce all 23
eligible fixed-actor SAT groups at frame 1,200. The SAT itself is DMA-copied
from VRAM `$7800-$78FF`. However, VRAM `$4000-$77FF` changes when the camera
moves from the lower to upper-middle band, so descriptor IDs alone cannot
provide correct unseen pattern pixels. Generic decoder `00:E41F` and its 14
routed physical-bank `$12` streams now reproduce that entire transition
exactly. Selector `00:F606` now has a complete local atlas: all 42 appearance
entries map to eight upload descriptors with 35 decoded phase assets or the
six-entry `$F768` resident-pattern sentinel. Its separate mode-zero raw-word
path is also bounded and tested. The copied SAT remains the exact
live-camera visual oracle and covers transient effects that do not warrant
native gameplay objects. Stream-backed fixed mechanisms now select their
appearance/phase upload, require complete ownership of every referenced
pattern block, combine it with bank-8 geometry, and store decoded pixels plus
palette indices before their first camera visit. The cache keeps
loader destination base `$4F:$50` distinct from SAT pattern base `$51:$52`
and invalidates on phase changes. Accepted lower, upper-middle, and middle
persistent composites are golden-guarded. All five `$F768` scene packs are
decoded as 1,008-word uploads at `$7C00`, and selector 0
`$1222->$7C00` equals live normal-table VRAM. An independent frame-1,200
classification shows that `$F768` owns zero of the 24 fixed groups using
resident or out-of-selector appearances; every one of their pattern blocks
is also disjoint from the exact ranges of all known uploads. Those static
blocks first established the safe live-VRAM fallback. The same exact
disjoint-range proof now applies to all appearance families: eight of the 61
non-hidden frame-1,200 fixed groups are selected-stream-owned and 53 are
upload-disjoint, leaving zero unresolved. The lower view therefore uses 23
live-camera groups plus 38 pre-seeded groups and writes 10,980 retained
pixels. Selectors 1-4 still need scene association, but no longer block
fixed-actor composition. The score/status band remains a separate DMD input,
not a table overlay.

Slots `$00/$04-$09` use the same bank-8 presentation tables and absolute
table-world coordinates. Their 13 decoded primitives match all five active
frame-1,200 SAT groups exactly, and all seven actors use upload-disjoint
pattern blocks. The ball remains live-SAT-only so motion cannot leave a
trail. Slots `$04-$09` participate in phase-aware persistence, keeping all
three flipper pairs visible across the stitched table. The resulting
dynamic-enemy-inclusive lower persistent golden is
`45901880df0caad856496f1a4d1a0063132f144a424587cb2fc3936f17501785`;
upper-middle and middle are
`de214ff8a7195a1e1d8a099a00889dcef3a2ed5df4d06885fb3da0d2e68447e8`
and
`b8680ea899a0ea273118d9349d9d2b51006d497f47f5e1fb630b04159a4fe890`.
All six flippers are cache-resolved at every post-fade checkpoint, and the
eight Sorcerer positions differ between frames 1,600 and 1,800 after their
region leaves the live camera.

Dynamic slots `$62-$7E` use the same bank-8 descriptor path. At frame 1,200,
14 decoded groups comprise ten type-`$82`/appearance-`$1A` Small Soldiers
and four type-`$82`/appearance-`$39` Tower insects. All four camera-active
groups match the copied SAT exactly, and all 14 groups use pattern blocks
disjoint from every known upload. The persistent cache admits these two
enemy families plus one separately bounded transient family. Occupancy loss
or any unclassified type, appearance, phase, or dispatch reuse clears the
slot immediately.

The admitted transient is the shared type-`$8F`, appearance-`$10` impact
burst. Its activation dispatch `$4290` and animation dispatch `$42B0` are one
presentation identity; original work RAM remains authoritative for its
phase, position, and 35-36-frame lifetime. At frame 2,100, slot `$71` is in
phase 5 with two exact bank-8 primitives. The group matches the live SAT,
uses upload-disjoint pattern blocks, and renders from a cloned cache when the
camera boundary is moved away. The accepted probe SHA-256 is
`f075afa29ad327d70535fcbc53123e7fe7aae1b6be0322cda0ccf536d78c7c60`.
Frames 2,400 and 2,700 contain no cached or rendered impact, proving that the
original slot clear ends presentation without a trail. Every other dynamic
effect remains live-SAT-only until it receives equivalent coordinate,
lifetime, and pattern-source evidence.

Generated images and ROM-derived caches remain local ignored artifacts.
