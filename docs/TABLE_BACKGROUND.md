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
input. It copies and decodes video state at frames 900, 1,200, 1,500, 1,800,
2,100, 2,400, 2,700, 3,000, 3,300, and 3,600.

Every checkpoint reports MWR `$10`, selecting a 64-by-32 BAT: a 512-by-256
pixel circular background window. BXR remains zero. BYR is respectively
`0, 0, 345, 290, 394, 0, 427, 0, 425, 330`.
Those BYR values are exactly the low nine bits of signed camera integer
`$0C:$0B`: `0, 0, -167, -222, -117, 0, -85, 0, -87, -182`.

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

## Interpretation and next step

The live BAT is a streamed circular window, not a complete table image.
Animation and actor-related tile/palette updates also make arbitrary
frame-wide VRAM hashes unsuitable as region identities. The three normal
table backgrounds must therefore be captured at the cartridge's stable
stream-commit boundaries and unwrapped using the corresponding world-camera
state.

The next bounded task is:

1. Decode the four `F400` caller paths into destination row and world-band
   direction.
2. Collect post-commit BAT rows, referenced patterns, and background palette.
3. Assign each band a signed world origin and remove circular BAT wrapping.
4. Stitch the three static bands.
5. Compare camera-sized native crops against sprite-masked emulator views.

Generated images and ROM-derived caches remain local ignored artifacts.
