# Routine Map

This map records bank-qualified HuC6280 instruction entries observed through
the read-only execution-hook registry. Addresses use the physical 8 KiB ROM
bank plus logical PC. Labels remain conservative: an address is not considered
replaceable gameplay logic until its inputs, writes, callers, return behavior,
and cycle role are understood.

## Confirmed boundary

| Routine | Observations | Status | Evidence |
| --- | ---: | --- | --- |
| `00:E195` | Canonical: 599, frames 2-600. Gameplay: 1,799, frames 2-1,800. | Controller poll | Reads the PCE controller I/O register, selects controller nibbles, and updates the controller state arrays in work RAM. Beetle and Mesen agree exactly in both recordings. |

## Canonical screen-flow candidates

| Routine | Canonical observations | Status | Evidence |
| --- | ---: | --- | --- |
| `08:5977` | 115, frames 157-600 | Setup-screen data helper | Active across the title, play-mode, speed, and data-load screens. The routine copies indexed setup-screen data and raises work-RAM update flags. Its higher-level scene role is not yet named. |
| `08:59B5` | 1, frame 300 | Transition candidate | Entered on the scripted Run press that advances from play-mode selection toward speed selection. |
| `08:59CE` | 1, frame 420 | Transition candidate | Entered on the scripted button-I press that advances from speed selection toward data load. |
| `08:59E7` | 1, frame 540 | Transition candidate | Entered on the final scripted left-plus-button-I input in the data-load screen. The resulting screen does not leave data load within the canonical window. |

The five-address canonical Beetle trace contains 717 globally ordered entries
and has SHA-256
`38ffd96714732c1f1f4073888c44f1b94b325dee8429e36d4712290bd1d5bddc`.
Its RAM/video/audio recording retains SHA-256
`67a55bed2d4f4e8dc5019c2b9b845df2f29514bbb564e2e44cde406d159575fe`.
All five entry counts and frame ranges agree with an independent Mesen
execution capture.

## Gameplay recording

The separate 1,800-frame script retains the canonical recording unchanged. It
selects New Game, confirms the selection, enters the table by frame 800,
holds button I to launch the ball, operates both flippers eight times, and
applies four button-II nudges. The Beetle gameplay CSV has SHA-256
`1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353`.

| Routine | Beetle observations | Status | Evidence |
| --- | ---: | --- | --- |
| `01:C3CB` | 860, frames 941-1,800 | Nudge/tilt session-input gate candidate | Called from `01:C166`. It gates on session flags at `$53` and `$2CC7`, decrements/reloads `$2CC6`, tests the active player's button-II state at `$2018,Y`, updates `$2CC2-$2CC5`, and can apply an offset to actor-position arrays `$3500,X/$3580,X`. Its outer RTS exits are `C3DC`, `C3FA`, `C43E`, `C456`, and `C49D`; `C484` returns only from the nested position helper. |
| `01:C49E` | 860, frames 941-1,800 | Pause/high-score session-input gate candidate | Called from `01:C169`, immediately after `C3CB`. It gates on `$53`, tests the active player's Run/Select edge bits at `$2018,Y`, calls the pause/session helpers at `ED37`, `E321`, and `ED42`, and otherwise initializes session fields beginning at `$2CBA`. The short input path returns at `C4C8`; the longer initialization path returns at `C52B`. |
| `03:6966` | 19,648, frames 873-1,800 | Actor collision-table interpreter | Nine call sites are active in this route. A selects a pointer from `$6A38`, X selects the actor, and `$3880,X` selects one or two passes. Helper `03:6995` advances table cursors `$3E00,X/$3E80,X`, records an event/type in `$3A80,X`, maintains the orientation bit in `$3D00,X`, and applies signed horizontal (`$3180,X/$3200,X`) and vertical (`$3300,X/$3380,X`) deltas. This route reaches outer RTS sites `6988` (8,512 calls), `69A1` (22), and `6A37` (11,114); `6994` is another static outer return. Carry set means the table reached its `$FF` terminator, while carry clear means no terminal record was reached. |

Together with the controller poll, these four hooks produce 23,167 globally
ordered Beetle entries and trace SHA-256
`d66b6fd13beba8a36370a7508bad968f72570bfad7d947b8442376279bf928b0`.
The hooked recording retains the gameplay CSV hash.

## Bounded call observations

The same route now brackets every selected entry through its matching outer
RTS. The host derives the caller and return PC from the HuC6280 stack and
captures input/output registers and MPRs, the return instruction, a
serialized absolute master clock, body cycles excluding RTS, and every
ordered work-RAM write made by the routine, nested callees, or an interrupt.

All 23,167 entries complete within the recording. The call summary has
SHA-256
`920fb4f8132b58ded27e65eb0a4b0c88def481fcb5f29286f8029f3594057b38`;
the 235,173-write companion has SHA-256
`84c398603f50b279ad670eed0d06d887006f085aa1d5aa7d4a8de4ad8178264c`.
Both are byte-identical in Debug and Release. Twenty calls cross Beetle's
frame-local timestamp reset; the absolute clock prevents their cycle budgets
from wrapping. Observed body budgets range from 69 to 2,280 master cycles.

The active session return paths are:

| Routine | Outer RTS | Calls |
| --- | --- | ---: |
| `00:E195` | `00:E1F7` | 1,799 |
| `01:C3CB` | `01:C3DC` | 859 |
| `01:C3CB` | `01:C3FA` | 1 |
| `01:C49E` | `01:C4C8` | 860 |
| `03:6966` | `03:6988` | 8,512 |
| `03:6966` | `03:69A1` | 22 |
| `03:6966` | `03:6A37` | 11,114 |

## Callers observed by Mesen

The Mesen oracle derives the JSR call site from the HuC6280 stack at each
entry. The deterministic route observes these callers:

| Callee | Caller | Mesen calls |
| --- | --- | ---: |
| `00:E195` | `00:E114` | 1,799 |
| `01:C3CB` | `01:C166` | 861 |
| `01:C49E` | `01:C169` | 861 |
| `03:6966` | `02:5890` | 1,800 |
| `03:6966` | `02:58DA` | 3,848 |
| `03:6966` | `02:5FFE` | 260 |
| `03:6966` | `03:6019` | 2,828 |
| `03:6966` | `03:6499` | 1,724 |
| `03:6966` | `03:6528` | 1,724 |
| `03:6966` | `03:7F1D` | 1,130 |
| `03:6966` | `03:7F5A` | 6,360 |

Static ROM inspection also finds calls to `03:6966` at `02:4421`,
`03:73AC`, and `03:7A74`; this route does not exercise them.

## Cross-core phase resolution

The earlier raw census appeared to show three cross-core deltas:

| Routine | Beetle | Mesen | Delta |
| --- | ---: | ---: | ---: |
| `00:E195` | 1,799, frames 2-1,800 | 1,799, frames 2-1,800 | 0 |
| `01:C3CB` | 860, frames 941-1,800 | 861, frames 940-1,800 | +1 Mesen |
| `01:C49E` | 860, frames 941-1,800 | 861, frames 940-1,800 | +1 Mesen |
| `03:6966` | 19,648, frames 873-1,800 | 19,674, frames 873-1,800 | +26 Mesen |

These are now explained completely by one frame of phase at table activation:

- The first four `03:6966` bootstrap calls occur on frame 873 in both cores.
- From table activation onward, every per-frame count agrees with
  `Beetle frame n == Mesen frame n-1`.
- Mesen's extra session entry is its frame 1,800 call beyond Beetle's aligned
  frame-1,799 endpoint.
- Mesen's apparent 26 extra collision entries are exactly its 26 frame-1,800
  calls beyond the same aligned endpoint.
- Across all aligned candidate calls, A, X, Y, SP, MPR0-MPR3, MPR6, and MPR7
  agree. Status differences are exclusively the transient HuC6280 T bit
  (`0x10`). MPR4/MPR5 are presentation-data windows and can differ while the
  aligned gameplay call arguments remain identical.

The checked-in `tools/mesen2/gameplay_routines.lua` reproduces the Mesen
entry and caller trace. Two independent captures are byte-identical
(SHA-256
`bdce4c3cecc6587e5319e56dfea2a47ed603ec9aa01c7dd9053627468db9889e`);
their frame-boundary work-RAM streams are also byte-identical (SHA-256
`868b08b94d0d725b0a56f7c1f88a1286a232d7860c8f310695a694624631e183`).
The bounded Mesen call trace is byte-identical across repeats (SHA-256
`587425ed4915c1e8786e3bb6b31fdde8692e1d8181817f6ab000b9e7f807e9fd`);
its ordered-write trace is also byte-identical (SHA-256
`859f2ba491ce08237870d1935746ee60d48e7b3928b710221fb9736a1e7d5aca`).
`tools/mesen2/Capture-GameplayRoutines.ps1` performs the isolated two-run
capture, and `tools/mesen2/Compare-GameplayRoutines.ps1` enforces the count,
phase, register, stable-MPR, T-bit, return-path, output, cycle, and ordered
write rules.

Across the 21,368 aligned session/collision calls, 18,075 call scopes have
exact cycle budgets and ordered work-RAM writes in both cores. In the
remaining 3,293, the cores place a periodic interrupt in neighboring call
scopes. The comparison separates the interrupt's three stack pushes and
bank-zero handler writes; the routine-owned ordered writes, callers, return
paths, and outputs still agree. This is a scheduling-boundary difference,
not a gameplay semantic difference, and the already verified frame-boundary
work RAM remains identical under the established phase alignment.

The count and observation blockers are closed. These addresses remain
non-replaceable until the native shadow-call contract defines interrupt
delivery and replacement-cycle accounting.

## State-census traversal and camera boundary

The 1,800-frame gameplay route remains the stable baseline for the initial
session, collision, and physics scopes, but its ball remains in the launch
lane. A separate 3,600-frame state-census route preserves those artifacts
while holding the launcher longer and alternating the left flipper with
button I. The resulting slot-0 integer position visits 208 distinct
horizontal values and vertical values from 241 through 684.

The active scroll accumulator is stored at `$0C:$0B:$0A`: `$0A` is the
fractional byte, `$0B` is the integer low byte, and `$0C` is the integer
high/sign byte. The VDC interrupt handler selects VDC register 8 and writes
`$0B/$0C` to its data ports, establishing the signed 16-bit integer scroll
consumed by the display. Across this route the integer scroll changes on 448
frames, assumes 245 distinct values, first changes on frame 1,201, and remains
active through frame 3,599.

Dynamic writers and static control flow bound the first camera scope:

| Routine | Calls | Caller | Outer RTS | Current role |
| --- | ---: | --- | --- | --- |
| `00:F43D` | 232, frames 1,201-3,600 | `01:C15A` | `00:F4FB` | Updates the camera motion fields `$A6-$AA`, integrates and constrains `$0A-$0C`, then sweeps indexed slots through the nested `F4FC` visibility/activation helper relative to the camera. |

Every call completes. Body budgets range from 34,395 to 88,980 master cycles,
with 653-1,475 scoped writes per call and 205,563 writes in total. Debug and
Release reproduce the same work-RAM stream and byte-identical scope artifacts:

- Work-RAM stream SHA-256:
  `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`
- Entry SHA-256:
  `4d40c0841a4783d612ed375ed0e12c74fd87014bd87f1d6c9b221323f4b07f74`
- Call SHA-256:
  `36e126530f88760882ae1cd2a6ccd8264985c28b620cb0d49d6d075b68fb3481`
- Write SHA-256:
  `6c4678d6c0810d0aae3533b2389396310bcb9f0aa5575c17868203fe88f2c2c8`

The complete frame CSV has SHA-256
`a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
in Debug and
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
in Release. Their work-RAM and audio columns agree on every frame; 908
framebuffer hashes differ beginning at frame 2,147. The ROM-backed regression
keeps exact configuration-specific frame goldens and common semantic
scope/write goldens so this Beetle rendering-build variance remains visible
without weakening the camera boundary.

This scope establishes camera integration and reaches the camera-relative
activation sweep. The nested slot helper is bounded separately below.

## Actor-slot camera-window activation

`00:F4DF` is the shared sweep entry reached after either directional camera
update. It clears X and calls `00:F4FC` once for every slot from `$00` through
`$7E`. The state-census route reaches 519 complete sweeps: one table-setup
call from `00:F929` on frame 872 and 518 gameplay calls from `01:C15A`
through frame 3,600. This explains why the 232 calls at the earlier `F43D`
boundary represent only one camera-update entry rather than every sweep.

`F4FC` uses parallel actor arrays:

| Field | Role |
| --- | --- |
| `$3000,X` | Occupancy/type byte; zero returns without changing the slot |
| `$3380:$3300,X` | Signed 16-bit integer vertical world coordinate |
| `$3080,X` bit 7 | Camera-window membership flag |
| `$AA:$A9` | Signed 16-bit camera/world anchor used by this sweep |

For an occupied slot, the helper sets bit 7 exactly when its vertical
coordinate is in the half-open interval
`[$AA:$A9 - 48, $AA:$A9 + 240)`. Equivalently, the inclusive integer bounds
are anchor minus 48 through anchor plus 239. Slots below or at/above those
bounds preserve the low seven flag bits and clear bit 7. Empty slots preserve
the entire flag byte.

The route records 65,927 bounded calls:

| Path | Calls | Outer RTS | Direct effect |
| --- | ---: | --- | --- |
| Empty slot | 10,873 | `F54E` | No write |
| Below window | 28,823 | `F523` | Clear `$3080,X` bit 7 |
| Inside window | 17,706 | `F54E` | Set `$3080,X` bit 7 |
| Above window | 8,525 | `F545` | Clear `$3080,X` bit 7 |

The complete sweeps account for 65,913 calls. Fourteen additional calls
refresh newly initialized slots `$62-$6F` immediately through fixed-bank
trampoline `01:C06C`: four originate at `02:5FD3` and ten at `03:7EEB`.
The route establishes this allocation shape:

- Slot `$00` is occupied and inside the window on all 519 sweeps.
- Slots `$04-$61` remain occupied throughout the observed route.
- Slots `$62-$71` each exhibit both empty and occupied lifecycle states.
- Slots `$01-$03` and `$72-$7E` remain empty.
- In total, 111 of the 127 slots become occupied.

Uninterrupted paths take 27 cycles for an empty slot, 189 for a below-window
clear, 327 for an above-window clear, and 333 for an in-window set. Periodic
interrupts raise observed body budgets as high as 39,024 cycles and account
for additional scoped writes; each occupied call still has exactly one
slot-addressed result matching its returned A value. The first in-window call
has a normalized replay at frame 872 with 333 body cycles and five writes.

Debug and Release reproduce 283,956 scoped writes and byte-identical
artifacts:

- Entry SHA-256:
  `8cd60958fe27377c1eec09c3c665f27f3642c581ac10da84be319d83c7dbae74`
- Call SHA-256:
  `6147c3b0a582a7810f7cbc97f3b9345b6138db4acf966a2d97873685f7042f91`
- Write SHA-256:
  `9d263045a2fbd253c3660ac1bbea41807bae3615627514a373421243a9f6ba7c`

`tools/Analyze-ActorActivationSemantics.ps1` guards the ROM instruction
signatures, sweep order, direct-refresh callers, all four paths, slot
lifecycles, flag writes, and normalized sample. The array contract and route
allocation shape are resolved; assigning each persistent slot to a named
table mechanism remains future census work.

## Actor layout and dispatch identity

The occupancy byte is only one layer of actor identity. Types `$80-$84` are
shared across unrelated mechanisms. The initial behavior entry and the live
dispatch pointer distinguish them:

| Slot range | Ownership on the state-census route |
| --- | --- |
| `$00` | Ball, allocated by `01:C65D` |
| `$01-$03` | Reserved/empty |
| `$04/$06/$08` | Left-side flipper/control pieces initialized at `01:D335` |
| `$05/$07/$09` | Right-side flipper/control pieces initialized at `01:D376` |
| `$0A-$61` | 88 fixed records from the ROM table beginning at `02:45DB` |
| `$62-$71` | Dynamically allocated pool used by this route |
| `$72-$7E` | Unused by this route |

`01:C574` is the generic slot reset. It clears 32 parallel arrays at
`$3000 + slot + n*$80`. `01:C5AB` scans slots `$04-$7B` and reserves the
first empty slot by writing a provisional occupied value; although that
allocator has a wider legal range, this route's 44 allocations and 35 frees
remain within `$62-$71`.

The fixed layout record is 12 bytes:

| Byte | Meaning |
| ---: | --- |
| 0 | Category selector used by the three layout entry modes |
| 1 | Slot |
| 2-3 | Initializer entry address |
| 4-5 | Initial signed integer X |
| 6-7 | Initial signed integer Y |
| 8 | Initial `$3900,X` state |
| 9 | Initial `$3080,X` flags |
| 10 | `$3000,X` type |
| 11 | Accumulator parameter passed to the initializer |

The records cover every slot from `$0A` through `$61` in order and reference
59 distinct initializer entries. Their category distribution is 64 category
0, 7 category 1, and 17 category 2. Their type distribution is:

| Type | Fixed records |
| --- | ---: |
| `$80` | 12 |
| `$81` | 1 |
| `$82` | 44 |
| `$83` | 19 |
| `$84` | 12 |

The live state-machine entry is the little-endian pointer at
`$3F80:$3F00,X`. The route records 3,296 writes to its two halves, balanced
at 1,648 each. Common helpers `C5E1/C5E6` perform 1,107 paired pointer
transitions. Seven additional dynamic writer families are active:

| Pointer/state installed | Writes | Direct writers |
| --- | ---: | --- |
| `$7EFA` | 10 | `03:7EE1/7EE6` |
| `$5FDB` | 4 | `02:5FC9/5FCE` |
| `$4290`, then `$42B0` | 7 each | `02:425F/4264`, `02:42A8/42AD` |
| `$441E` | 1 | `02:43AB/43B0` |
| `$7C34` | 19 | `03:7C2B/7C30` |
| `$7C6F` | 3 | `03:7C57/7C5C` |
| `$7F81` | 8 | `03:7F8C/7F91` |

The combined identity census contains 576 type/occupancy writes and 3,296
dispatch writes. Debug and Release reproduce write SHA-256
`b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`.
`tools/Analyze-ActorLayoutSemantics.ps1` verifies the generic clear and
allocator, static record schema and distributions, exact layout writes,
ball/flipper ownership, dynamic allocation range, dispatch balance, and
active dynamic families. Initializer addresses now provide stable technical
identities for all fixed actors. Subsequent rule mapping has assigned all 59
initializer identities a bounded gameplay-facing or conservative
compatibility role.

## Fixed-actor SAT presentation

Physical bank `$08` owns the normal-table actor-to-SAT presentation path.
`08:4036` selects VDC register 0, begins at the existing sprite count in
zero-page `$58`, and points the VDC memory-write address into the 256-word
staging table at VRAM `$7800-$78FF`. It submits queued slots around one
ascending sweep of all 127 actor records. The main sweep calls `08:40DF`
only when `$3080,X` has camera bit 7 set and hidden bit 5 clear, then clears
the unused SAT tail.

`08:40DF` is the descriptor renderer:

- `$3900,X` selects one of 69 parallel low/high pointer entries at
  `$41AC/$41F1`.
- `$3980,X * 2` selects a descriptor pointer from that appearance table.
- The descriptor begins with a sprite count, followed by six bytes per
  sprite: signed Y offset, signed X offset, pattern low/high, and attribute
  low/high.
- Signed offsets are added to `$3300:$3380,X` and
  `$3180:$3200,X`. Y is translated through camera anchor `$A9:$AA` plus the
  HuC6270 `$40` bias; X descriptors already include the `$20` hardware bias.
- Pattern words add the global `$52:$51` base with carry.
- `$3800,X` bits 4/5 optionally merge `$3D00,X` and `$3E00,X` into the low
  and high attribute bytes.

The copied post-DMA SAT is an exact copy of VRAM `$7800-$78FF` at stable
frame 1,200. All 256 high-byte staging writes occur at these sites:

| Site | Writes |
| --- | ---: |
| `08:40C6/40CC/40D2/40D8` | 12 each |
| `08:4141` | 52 |
| `08:4154` | 43 |
| `08:4165` | 9 |
| `08:4176/419E` | 52 each |

The bounded native metadata decoder resolves 100 sprites across occupied,
non-hidden fixed slots `$0A-$61` at frame 1,200. Every one of the 23
camera-eligible fixed actor groups appears as an exact contiguous word
sequence in the copied SAT. A ROM-free fixture independently guards signed
offsets, pattern-base addition, attribute merging, and invalid appearance
rejection.

Descriptor metadata is not a complete unseen-actor asset source. Excluding
the SAT table, VRAM words `$4000-$77FF` change from SHA-256
`f84447542add674e39a660d57bde50277ef414c6ee860ab0e8c5d707e889edd3`
at lower frame 1,200 to
`c5f344d3130e20d40a37e5d055a557022290be8c87d97df397068850fbf7155d`
at upper-middle frame 1,600. The cabinet cache therefore stores resolved
pixels for observed mechanisms. Pre-seeding unvisited mechanisms requires
mapping the camera-region/state stream selectors rather than reusing whatever
patterns currently occupy those VRAM indices.

### Sprite-pattern VRAM streams

Physical bank `$00`, logical `$E41F`, is the generic VRAM word-stream
decoder. Its first instruction is `PHY`; `$E420` is therefore an interior
instruction, not a valid call-observation boundary. Zero-page `$2D:$2E`
supplies the destination word and `$2F:$30`
supplies an MPR-mapped source pointer. Its command format is:

- `$00`: terminate.
- `$01-$7F`: copy that many literal little-endian words.
- `$81-$FF`: repeat the following word `(command & $7F)` times.
- `$80`: repeat the following word 256 times.

Literal high bytes are written at `00:E449`; repeated high bytes are written
at `00:E467`. Across frames 1,201-1,600 these are the only writers into
sprite-pattern VRAM `$4000-$77FF`, contributing 1,036 literal and 2,548
repeated words. Fourteen calls select physical-bank `$12` sources:

| Frame(s) | Destination | Source(s) | Actor selector |
| --- | --- | --- | --- |
| 1,220 | `$7000` | `12:8030` | slot `$5B`, appearance `$03`, phase 0, descriptor `$F6FA` |
| 1,227 | `$5400` | `12:8AFD` | slot `$0F`, appearance `$11`, phase 0, descriptor `$F73B` |
| 1,245-1,263 | `$5400` | `12:8B80`, `12:8C5C`, `12:8D52`, `12:8C5C`, `12:8B80`, `12:8AFD` | slot `$0F`, phases 1/2/3/2/1/0 |
| 1,298-1,316 | `$5400` | `12:8B80`, `12:8C5C`, `12:8D52`, `12:8C5C`, `12:8B80`, `12:8AFD` | slot `$0F`, phases 1/2/3/2/1/0 |

All 14 calls enter from `00:F661`. The containing selector at `00:F606`
indexes 42 parallel descriptor-pointer bytes at `$F6A6/$F6D0` with actor
appearance `$3900,X`. It combines descriptor destination offsets with
pattern base `$4F:$50`; non-mode-2 descriptors select an asset reference
with phase `$3980,X`. Resolver `$E1FD` maps the asset reference's high byte
into MPR4/MPR5, then uses its low byte as an index into the two-byte directory
at logical `$8000`. This turns IDs `$1200/$1212/$1214/$1216/$1218` into the
observed physical-bank `$12` stream addresses.

The bounded native decoder replays every stream over the frame-1,200 VRAM
snapshot. The resulting `$4000-$77FF` words equal the frame-1,600 snapshot
exactly and reproduce SHA-256
`c5f344d3130e20d40a37e5d055a557022290be8c87d97df397068850fbf7155d`.
A ROM-free fixture independently guards literal, repeated, and bounded-output
behavior.

The complete `$F6A6/$F6D0` selector family is now decoded. Its 42 appearance
entries collapse to these nine unique pointers:

| Pointer | Appearances | Destination with base `$3800` | Phase assets |
| --- | --- | --- | --- |
| `$F6FA` | `$00-$03` | `$7000` | `$1200/$1202/$1204/$1206/$1208` |
| `$F709` | `$04-$07` | `$5500` | `$121A/$121C/$121E/$1220` |
| `$F716` | `$08` | `$4F00` | `$1012/$1014/$1016` |
| `$F721` | `$09-$0E` | `$7200` | `$1028/$102A/$102C/$102E` |
| `$F72E` | `$0F` | `$7380` | `$1028/$102A/$102C/$102E` |
| `$F73B` | `$10-$11` | `$5400` | `$1212/$1214/$1216/$1218` |
| `$F748` | `$12-$19` | `$6D00` | `$120A/$120C/$120E` |
| `$F753` | `$1A-$23` | `$7500` | `$1018/$101A/$101C/$101E`, repeated |
| `$F768` | `$24-$29` | no-per-phase sentinel; scene loader writes `$7C00` | no per-phase descriptor |

The first eight pointers provide 35 compressed phase uploads containing
12,544 decoded words. `$F768` is executable special-loader code, not
descriptor data, so the atlas records those six selectors as one explicit
no-per-phase sentinel. Separately, the atlas decodes all five assets selected
by `$F768`:

| Selector | Asset | Physical source | Destination | Words |
| --- | --- | --- | --- | --- |
| 0 | `$1222` | `12:91EB` | `$7C00` | 1,008 |
| 1 | `$1224` | `12:9919` | `$7C00` | 1,008 |
| 2 | `$1226` | `12:A04F` | `$7C00` | 1,008 |
| 3 | `$1228` | `12:A79D` | `$7C00` | 1,008 |
| 4 | `$122A` | `12:AECC` | `$7C00` | 1,008 |

Selector 0 equals the live `$7C00-$7FEF` words after normal-table
initialization. The native descriptor decoder also implements
`00:F666-00:F6A5`'s mode-zero path through raw uploader `00:E365`: it resolves
the base asset, applies the phase-relative source offset, copies the declared
word count, and enforces the same output bound. None of the 42 static
selectors uses mode zero, so that branch is guarded with a ROM-free synthetic
descriptor rather than falsely classifying `$F768` as raw data.

The stream-backed entries are now applied downstream of selection. The cache
uses `$4F:$50` only for loader destinations and `$51:$52` only for bank-8 SAT
pattern fields, selects the actor's live `$3980,X` phase, and accepts a seed
only when the upload owns every 64-word pattern block referenced by that
phase's SAT metadata. Resolved pixels retain the live sprite palette and
participate in the existing background-priority and 16-half scanline rules.
Type, appearance, phase, or dispatch changes invalidate the seed.

At settled lower frame 1,200, 16 of 17 camera-active stream-backed actor
groups have their complete selected upload byte-for-byte in live VRAM, while
all 23 eligible bank-8 metadata groups still match the copied SAT. The 24
fixed groups using resident or out-of-selector appearances
`$24/$25/$29/$2A/$2B/$2C/$37/$41/$43` use no `$7C00` pattern blocks.
Every referenced 64-word block is disjoint from the exact half-open ranges
of all 35 `$F606` uploads and all five `$F768` uploads, so these groups can
use static live-VRAM pixels without a camera visit. Generalizing the same
proof across all appearances classifies all 61 non-hidden frame-1,200 fixed
groups: eight are owned by their selected stream, 53 are upload-disjoint,
and zero are unresolved. The 23 live-camera groups plus 38 pre-seeded groups
cover the complete fixed population, write 10,980 surviving pixels, and
produce accepted-table SHA-256
`63d4e780207d47cd24cbde762073b3bec42627b8a1d945ec7446a1269cfed752`.
The corresponding upper-middle and middle persistent goldens are
`6658664596ec120762f233416a244902cfdee4315c6fe05b4eba9f8f31d6593b`
and
`d229dd586d9b465260b9fcbbff3aa614b2418116d45b3d51107098f8033059eb`.

The same bank-8 descriptor path also closes ball/flipper presentation.
Slot `$00` plus slots `$04-$09` decode 13 primitives: one ball sprite and
two sprites per flipper. Their actor coordinates are absolute table-world
coordinates, not viewport-relative values. At frame 1,200 all five active
groups match contiguous copied-SAT words, and all seven actor groups use
pattern blocks disjoint from every decoded upload. The ball remains
live-SAT-only; flipper slots join the phase-aware persistent cache. This
replaces the three fixed-only persistent goldens above with
`0b52e88e49d740a264395d4a73c545635f799e841cc5eedb8b5c0a0acaecfc2b`,
`22e28684b9f0ec5ef2e453b1fec33bac5408b9c5edb24b0696d4706007861e7c`,
and
`c103b179edbd5c851cff2137b37ee776e8165829867329c34c9e5a7abbcb588b`
for lower, upper-middle, and middle checkpoints respectively.

Dynamic slots `$62-$7E` also use absolute table-world coordinates through
this descriptor path. At settled frame 1,200 the decoder resolves 14
one-sprite persistent enemies: ten Small Soldiers with type `$82`,
appearance `$1A`, and four Tower insects with type `$82`, appearance `$39`.
All four camera-active groups match contiguous copied-SAT words. Every one of
the 14 pattern blocks is disjoint from all decoded uploads, so unseen phases
can use the proven static VRAM source. The cache retains only those two
families and invalidates immediately on occupancy, type, appearance, phase,
or dispatch changes; reused type-`$8F` impact slots therefore cannot trail.
The dynamic-enemy-inclusive lower, upper-middle, and middle goldens are
`eb2e1147b9b65304deed11e8df26e681cfa1162c8b89ac60363ebc910e55dcfd`,
`2aeb44b0911f1a75aed9ef239c3ffbe3b460c39ed6a6b933256c287693470835`,
and
`bbba7ee929c4029843611daa08943a79a89ff6a98ae676b457c83d8e98f8db34`.

Frame 874 enters `$F768` with `A=0`, resolving asset `$1222` through the
bank-12 directory and loading it at `$7C00`. `X=0` and the slot-0
appearance/phase are both zero, confirming scene-level initialization rather
than an actor-local `00:F606` call. Each of the six deterministic bonus-stage
routes observes only that earlier selector-0 call, so selectors 1-4 are not
bonus-stage packs. Their scene association remains unmapped, but the exact
pattern-range classification closes the normal-table fixed-actor
first-visit gap independently.

## Normal-table scene layout

The three exported entries at the beginning of physical bank `$02` select
different loading modes over the same actor-layout table:

| Entry | Static behavior | State-census coverage |
| --- | --- | --- |
| `02:4088` | Load all 88 records | One call, frame 871 |
| `02:40A8` | Load only records whose category byte is 1 | Inactive |
| `02:40CF` | Load only records whose category byte is 2 | Inactive |

The active full-layout entry is reached through the common banked-call wrapper
at `00:E241/E262`. The wrapper maps physical banks `$02/$03` into the
`$4000-$7FFF` CPU window, tail-dispatches to `$4088`, and restores the prior
mapping after the call. The bounded path is:

| Routine | Caller / resume | Outer RTS | Body cycles | Scoped writes |
| --- | --- | --- | ---: | ---: |
| `02:4088` | `00:E252` / `00:E255` | `02:40A7` | 421,956 | 11,331 |

The call starts in frame 871 and completes in frame 872. It therefore cannot
complete inside the replay system's restored single-frame window; the natural
cross-frame call is retained as the deterministic oracle instead of weakening
replay semantics.

For each record, `40F6` invokes the generic 32-array clear at `01:C574`, then
writes initializer scratch, integer X/Y, state, flags, and type before jumping
to the record's initializer. Across the call this produces:

- 2,816 exact zero writes from 88 records times 32 parallel arrays.
- 616 direct field writes from seven record-backed actor fields.
- 176 initializer-pointer scratch writes.
- Nested initializer and interrupt activity accounting for the remaining
  scoped writes.

`tools/Analyze-SceneLayoutSemantics.ps1` verifies all 88 record values against
their destination slots, every parallel-array clear, the bank mapping,
caller/return path, category populations, and the selective-mode signatures.
Debug and Release reproduce:

- Entry SHA-256:
  `8fdc6c0c9807c5bbb22a560bd6e05d9288fe7d315db8056ee24527362995b847`
- Call SHA-256:
  `bd601b053e6328985f279f4de1354b5617ecd3ca2ebb4feac9facbd114f63ab9`
- Write SHA-256:
  `dc65b4240bd68dd32cbf35f27da707d655ce0487551fbe94afac3aa2f895ac58`

This establishes the full normal-table scene-layout boundary.

Static caller mapping now identifies every normal-table request:

| Export / loader | Banked-call site(s) | Record population |
| --- | --- | --- |
| `02:4000` / `02:4088` | `00:F935` | All 88 records |
| `02:4003` / `02:40A8` | `00:F829`, `01:C826` | Seven category-1 records |
| `02:4006` / `02:40CF` | `00:F052` | Seventeen category-2 records |

Category 1 is the persistence-oriented set: slots `$2F`, `$36`, `$3A-$3D`,
and `$4C` cover Tower Guard, Giant Face state, four Knights, and Giant Door.
Category 2 is the respawn-oriented set at slots `$14/$15`, `$2C`, `$32`,
`$34/$35`, and `$50-$5A`, including the Tower-insect source, Side Stars,
Gray Blobs, and adjacent upper-table actors. These populations and caller
locations are exact ROM facts. The dynamic routes below establish their
transition roles.

The standard 3,600-frame route, a continuation to 12,000 frames with stopped
and directed controls, and a 60,000-frame deterministic irregular-control
extension entered neither selective export. The modes therefore require
specific rule/state prerequisites rather than elapsed time or a passive ball
trajectory.

To make those prerequisites reproducible, the runtime now has an exact-size
work-RAM import complement to its export operation. Recording option
`--work-ram-patch FRAME:ADDRESS:VALUE` imports a copied 8 KiB image
immediately before the selected frame, records every mutation in artifact
metadata, and rejects invalid or duplicate locations. It does not masquerade
as a CPU write. The first regression changes only logical `$3FFF` before
frame 3,600 and produces the same final work-RAM SHA-256
`c858cafb51a462658046c9529e5d4c795d1b63bfd09db61198c9c149a77adc96`
in Debug and Release. Full artifact SHA-256 values are:

- Debug:
  `1e8540272bb49e7b35a1cba902feed6723c3752ed6409ec2c276f77136d55c79`
- Release:
  `f9ad5753938bbab4d87a9b160b57c9cbcc1fe17610b3bfef892b27f082421b39`

Initial seeds of guessed ball position/velocity fields, occupancy, and an
isolated `$CF` request byte did not satisfy either complete transition state
machine. Disassembly of the active `01:C765` ball handler resolves the exact
category-1 prerequisite: `01:DD6E` compares ball slot `$00`'s signed 16-bit Y
coordinate at `$3380:$3300` with `$02D0`. Seeding Y=`$0300` at frame 3,000
enters the original drain/remaining-ball path and reaches `01:C826` and
export `02:4003` at frame 3,287.

That bounded 98,931-master-cycle call has 2,095 scoped writes and reloads
exactly category-1 slots `$2F`, `$36`, `$3A-$3D`, and `$4C`: 49 direct record
fields plus 224 parallel-array clears. `tools/Analyze-SelectiveSceneSemantics.ps1`
and `dc_rom_selective_scene` guard the drain threshold, remaining-ball call
site, all seven record values, and cross-configuration artifacts:

- Entry SHA-256:
  `3d3014febaf1421cf4b5bc0bd91f97891a9d2ac6983af80b2cc0fed164245a2f`
- Call SHA-256:
  `5b8c39149b64176c9eac0569886dab76123f0a04da420f2b30116eb5a0ec9e0e`
- Write SHA-256:
  `59751f692de3fbd36a5666fd14492c982e6fb40c46baf5f5129186bb6f299a6c`

Category 2 belongs to normal-table restoration after a bonus transition.
Seeding bonus-stage ID 1 in `$55` and short-transition request `$53.7` at
frame 1,300 bypasses the stage-layout loader, returns through `00:EF8B` at
frame 1,815, and invokes export `02:4006` at frame 1,851. Static reinspection
corrects its banked-call site to `00:F052`; the following `00:F05E` invokes
the distinct export `08:4000`.

The category-2 call takes 133,077 master cycles, contains 3,117 scoped
writes, and reloads exactly slots `$14/$15`, `$2C`, `$32`, `$34/$35`, and
`$50-$5A`: 119 direct record fields plus 544 parallel-array clears.
`tools/Analyze-BonusReturnSemantics.ps1` and `dc_rom_bonus_return` guard the
short transition, corrected call site, all 17 record values, and
byte-identical Debug/Release artifacts:

- Census SHA-256:
  `c88040c5de53e4d55e218c259a1a920d8e3ee05804832a3c5263ae52aba9b9ce`
- Entry SHA-256:
  `fe018ea73e784fe401ce98af51eb5df090891a75f7571a8b3173055819faa98c`
- Call SHA-256:
  `2a76f0ad803d2304d44a2e5516ff5ed4a8f4607e123a7be62386d94d6079a1c4`
- Write SHA-256:
  `026a4bccd25048321aed98599b76b6a0abb7936fbbfed58b4bdad9d245de14c8`

Both dormant selective normal-table modes and the short category-2 return
transition are therefore dynamically covered.

The complete bonus-stage prerequisite is distinct: request bit `$53.4`
selects `01:C1BA`, which maps the bonus banks and calls the full initializer
at `00:ED86`. That path preserves normal-table state, invokes bank-4 export
`04:4000`, and selects one of six 12-byte record tables using stage ID `$55`.
A frame-1,300 `$53=$10` seed reaches the real layout loader for every stage:

| Stage | Loader frame | Records | Body cycles | Scoped writes |
| --- | ---: | ---: | ---: | ---: |
| BS-1 | 1,341 | 16 | 76,704 | 590 |
| BS-2 | 1,342 | 8 | 33,240 | 305 |
| BS-3 | 1,342 | 12 | 52,353 | 422 |
| BS-4 | 1,342 | 20 | 91,932 | 600 |
| BS-5 | 1,342 | 24 | 114,144 | 716 |
| BS-6 | 1,342 | 7 | 28,734 | 278 |

`tools/Analyze-BonusLayoutSemantics.ps1` verifies all six table pointers and
87 records, legal dynamic allocations, initializer pointers, seven direct
actor fields, call boundaries, cycles, and writes. `dc_rom_bonus_layout`
uses BS-1 as a byte-identical Debug/Release dynamic golden and runs all six
dynamic routes in Release. The true routes also activate two formerly static
RNG consumers: BS-1 calls `04:47C7` five times and BS-6 calls `05:7217`
twice. Targeted BS-1 event coverage now also calls `04:4805` once, and the
targeted BS-4 route below activates `05:6B88/6CB1`. BS-6's natural hazard
allocator also activates `05:732E/733B`. The maximum-score route below
activates the final bank-7 pair, so all nine bonus/ending-bank consumers now
have deterministic dynamic coverage.

This establishes deterministic entry and layout initialization for all six
bonus scenes. BS-1's first rule and no-progress result path are bounded below;
the remaining stage outcomes and event-specific families remain to be mapped.

## Bonus stage 1 target family and no-progress result

The BS-1 layout's five records at positions `(100,54)`, `(120,68)`,
`(152,64)`, `(184,68)`, and `(204,54)` form one objective row. They use
initializer `04:47B5`, occupy dynamic slots `$34/$35/$50-$52`, and share
type `$C2`, collision selector `$10`, completion selector `$81`, and six-hit
counter `$3C00,X`. Controller `04:44C6` occupies slot `$2C` and initializes
the shared remaining-target counter `$2D2E` to five.

The target state machine has these exact scoring and lifecycle rules:

- `04:47C7` invokes the shared RNG once per target and initializes
  `$3B00,X` to `$80 + (RNG & $2F)`, producing `$80-$AF`.
- Steady state `04:47D5` clears a qualifying collision and decrements the
  six-hit counter.
- Each of the first five hits submits 50 stored score units (5,000 displayed
  points). The sixth submits 200 stored units (20,000 displayed points),
  decrements `$2D2E`, and releases the actor.
- An expired movement timer invokes RNG site `04:4805` and refreshes the
  timer to `$C0-$EF`; the targeted event route below exercises this branch.

Five adjacent records at `(48/240,80)` and `(104/144/184,120)` use
initializer `04:49D4`, occupy slots `$53-$57`, and form a support row. Their
contact path submits 10 stored units (1,000 displayed points). They reset for
another contact while `$2D2E` is nonzero and release once the objective is
clear. When the controller observes zero remaining targets, its presentation
state ultimately writes completion latch `$2DBA=1`, sets `$53.1`, and
releases itself.

The deterministic forced-entry route provides a dynamic no-progress result
oracle:

| Routine | Calls / frames | Routed behavior |
| --- | --- | --- |
| `04:44C6` | 1 at 1,341 | Initializes `$2D2E=5` |
| `04:44CE` | 2 at 1,342 / 1,403 | Both see a nonzero objective and return |
| `04:47B5` / `47C7` | 5 / 5 | Initializes the five targets and their RNG timers |
| `04:47D5` | 5 at 1,403 | Collision-free updates; all return at `4813` |
| `04:49D4` / `49DF` | 5 / 10 | Initializes and checks the five support actors without contact |
| `04:456C` | 1 at 1,403 | Enters the cross-frame result controller |
| `04:4DC4` | 240, frames 1,529-1,768 | Four-tick result-glyph scheduler with 60 advances |

The objective remains five, and `$2DB7-$2DB9` plus completion latch `$2DBA`
remain zero after their frame-1,341 resets. Result export `04:456C` therefore
takes its zero-accumulator branch and selects the three ROM strings
`SORRY`, `YOU LOST YOUR`, and `BONUS POINTS`. Its nonzero branch statically
selects `YOU GET` / `BONUS POINTS`; a later nonzero `$2DBA` check additionally
selects `PERFECT` / `SPECIAL POINTS`. This separates the result selector from
the target-family completion latch without claiming an unobserved successful
playthrough.

`tools/Analyze-BonusStage1Semantics.ps1` and `dc_rom_bonus_stage1` guard the
layout identities, target/support code, score constants, RNG transforms,
completion and result selectors, failure text, exact dynamic call
populations, ordered writes, and `$2Dxx` timeline. Debug and Release reproduce:

- Census SHA-256:
  `e660ac5620fefbc65147496ec757893e9e599b60ad7635dcb0ee69b1f476dec4`
- Entry SHA-256:
  `4be82e8645661eaa9fdb132f932181b7cc3356e94cc03a7011a2aafcfeed2139`
- Call SHA-256:
  `fa8e1cfeb714e0519f5b142c8fa2f4578780ca827e7877f47da0ccebd3a7cbad`
- Scoped-write SHA-256:
  `f20e5d5b610a6be765bd3225be2c73ce8517313bf5f7ee4126428646421070bc`
- Selected-state SHA-256:
  `bb5d4c02783e742240c88615b971669f0f64e87715734d358596a45f1f2df315`

The two nonzero result selectors are now dynamically separated with targeted
frame-1,459 seeds:

| Route | Additional seed | Required entries | Selected-state behavior |
| --- | --- | --- | --- |
| Scored progress | `$2DB7=1` | `04:45F0`, then ordinary cleanup `04:46A4` | Handshake `$2D66` advances to 1; 181 glyph ticks and 46 advances |
| Perfect | `$2DB7=1`, `$2DBA=1` | `04:45F0`, then perfect branch `04:464F` | Handshake advances to 1 and 2; 874 glyph ticks and 219 advances |

The scored route clears `$2DB7-$2DB9` at frame 1,649 and bypasses the perfect
award builder. The perfect route instead calls `04:4A5B`: with routed
`$2D38=0` and zero-page `$CA=0`, its nested decrement loops perform 256
additions of 1,000 stored score units and produce the exact accumulator
`$03E800` (256,000 stored units, or 25,600,000 displayed points).
`04:46B8` then transfers the award to the main score through
`01:C0C9 -> 00:F55C` in 512 frame-paced chunks of `$01F4` (500 stored units).
The accumulator reaches zero at frame 2,341 and the terminal remainder path
clears it and advances `$2D66` to 2 at frame 2,342.

`tools/Analyze-BonusStage1ResultSemantics.ps1` and
`dc_rom_bonus_stage1_results` guard the result selectors, perfect-award
construction, score trampoline, transfer loop, handshakes, and presentation
cadence. The scored route is byte-identical in Debug and Release; Release
also runs the longer perfect route:

| Route | Census SHA-256 | Selected-state SHA-256 |
| --- | --- | --- |
| Scored | `f9d1d29b90774a62b495bc57c00fbe6df18c676351441f92893ceced8771979b` | `6f06098ff55ee92e2dac4d4e5b8ebdea34c9fd4cb49f4a0cb8c08fa5629a746c` |
| Perfect | `3d0c8e2de33d3fe6c48ea085205fb7e0d362a2d27c8b96ba0f02fabb4f2aaefb` | `f2e47ee8eea9e533d72855e097ae2744b76df3d89f2288f22ad3b59fc21c465c` |

## Bonus stage 1 target contact and timer expiry

A combined frame-1,403 route exercises two independent target events in the
same five-call `04:47D5` sweep:

| Slot | Seed | Dynamic result |
| --- | --- | --- |
| `$34` | Collision byte `$3834=$90` | Clears bit 7 to `$10`, decrements `$3C34` from 6 to 5, adds `$32` (50 stored units) to `$2DB7-$2DB9`, emits command `$36`, and decrements timer `$AD->$AC` |
| `$35` | Timer `$3B35=1` | Decrements to zero, calls RNG consumer `04:4805`, refreshes to `$EB`, invokes `04:4836`, and emits command `$46` |

The contact's existing impact helper allocates dynamic slot `$62`. The expiry
helper therefore receives slot `$63`, writes dispatch type `$C3`, copies the
second target's horizontal position `$78`, adds `$18` to its vertical position
to produce `$5C`, installs live dispatch `04:488D`, collision selector `$03`,
completion selector `$06`, and one-tick initialization state. The expiry
target does not execute any contact-branch write.

The five bounded calls all occur at frame 1,403 and return through `04:4813`.
Together they contain 564 ordered writes. `04:4805` is now the third
dynamically covered bonus-bank RNG consumer, leaving six event-dependent
bonus consumers under static guards only.

`tools/Analyze-BonusStage1EventSemantics.ps1` and
`dc_rom_bonus_stage1_events` guard the target branches, expiry allocator,
score/audio coupling, call contracts, and ordered writes. Debug and Release
are byte-identical:

- Census SHA-256:
  `3b819a0a88a7169e2c3d7118f607ad0e380670a3b3acb921702c2b3b4afcd82c`
- Entry SHA-256:
  `31fb5bca73a6f8a25942ce56674592f1ccb1a7fb86810269a030dbdf2a0c6225`
- Call SHA-256:
  `7a01d9bd957c3b1ac7f9f358b4e6e965c96d8206eebaf72d7f486bea8c2dcf2e`
- Scoped-write SHA-256:
  `8632ddac95070c188ea1e16626c34c1a0d8fb23f1cc9f7862e3c50fbd6018232`

## Bonus stage 2 emitter and target family

BS-2's eight-record layout begins with a transient setup actor and then
reuses slot `$2C` for controller `04:4E8F`. The controller initializes shared
objective counter `$2D2E` to `$1C` (28) and waits for zero before entering the
common completion presentation. Layout records at slots `$34/$35` provide the
paired type-`$C2` target emitters.

The first emitter's batch setup clears lane counter `$2D26`, initializes a
one-tick cadence and five-emission batch, then advances into state `04:4EFB`.
When its cadence reaches zero, that state reloads it to `$30` (48 ticks),
calls allocator `04:4FBA`, and decrements the batch count. The allocator:

- Obtains a dynamic slot and installs dispatch type `$C2`.
- Copies source position, animation, collision shape, and subtype.
- Sets collision selector `$10`, completion selector `$81`, type `$84`, an
  entrance delay of `$10`, and one hit.
- Installs live dispatch `04:5051`.
- Increments subtype-indexed live counter `$2D26/Y`.

The targeted frame-1,404 route leaves seeded slot `$53` occupied, so the
authentic emitter allocates slot `$54`. Its exact routed state is position
`(248,48)`, animation `$1D`, shape `$11`, subtype 0, and live counter
`$2D26=1`. Slot `$53` independently enters the genuine post-delay target
state `04:5079` with collision bit 7 set. The hit clears that bit, submits 50
stored score units (5,000 displayed points), writes response selector `$35`,
decrements `$2D26` back to zero, and releases the actor. This hit does not
decrement the controller's global `$2D2E` objective because its seeded
position lies outside the 8-by-5 board domain. The next section bounds the
valid movement/resolution path.

`tools/Analyze-BonusStage2TargetSemantics.ps1` and
`dc_rom_bonus_stage2_targets` guard the eight-record layout, controller,
emitter batch, allocator fields, target contact, score/response coupling,
lane-count balance, and generic release. Debug and Release reproduce:

- Census SHA-256:
  `0df2459325e5aaac657e588346b2ca5125d59f56e95f49a881e0eebf88e2996e`
- Entry SHA-256:
  `8a941d2af5bebdbf096c775b4dae77329d7cbb9ddcbff5cbb38b22e66a108420`
- Call SHA-256:
  `f4b01ddc19c7289b390d1f12850eef4b620a5cb308fe1f2ae1d3d329ddba2913`
- Scoped-write SHA-256:
  `7a1300ffb2144e1ddd92322ffed3ad1faaafcafb8c050ac69f950b95e41a337e`
- Selected-state SHA-256:
  `138e1f0d90ba8107e655740306deac70f02f69b8bb114d2c9dbf16a9fb8bb882`

## Bonus stage 2 board resolution and results

The paired emitter is now dynamically complete. Slot `$34` uses subtype 0,
position `(248,48)`, live counter `$2D26`, and steady batch state `04:4EFB`.
Slot `$35` uses subtype 1, position `(60,48)`, live counter `$2D27`, and
steady state `04:4F83`. A targeted frame-1,404 route advances both one-tick
cadences:

- Each reloads its cadence to `$30` (48 ticks), calls shared allocator
  `04:4FBA`, and decrements its five-target batch to four.
- With seeded slot `$53` occupied, subtype 0 obtains slot `$54` and subtype 1
  obtains slot `$55`.
- Both new actors install type `$C2`, live dispatch `04:5051`, one hit,
  entrance delay `$10`, and their source-specific position/subtype.
- Their independent live counters both advance from zero to one.

The contact-to-board resolver at `04:5260` converts a target's position into
an 8-by-5 cell index. Its ROM table has 28 valid cells: all eight cells in
the top, middle, and bottom rows, plus only the two endpoints in each
intervening row. This is the controller's exact `$1C` objective domain.

The event route places seeded target slot `$53` at `(96,60)` with collision
bit 7 set. Collision helper `04:5A12` moves the target to `(208,60)` before
mapping, so this seed resolves logical board cell `(7,0)`. Its authentic
`04:5079` contact:

1. Submits the ordinary 50-unit target award.
2. Calls `04:5260`, which resolves four tile addresses through four bounded
   calls to `04:52ED`.
3. Each resolved tile submits 100 stored units, producing a 450-unit total
   for the contact.
4. Decrements `$2D2E` exactly once at `04:52E9`, from `$1C` to `$1B`.
5. Balances subtype-0 live counter `$2D26` and releases the actor.

The remaining stage-specific layout records are also bounded. Slot `$50`
initializer `04:511D` places the ball at `(63,205)`, selects presentation
state 6, and installs continuation `04:5166`. Slot `$51` initializer
`04:5169` installs repeatable capture dispatch `04:5192`; its ROM-bounded
contact path submits 10 stored units before entering the shared capture
presentation.

Two result routes close the first BS-2 outcome boundary:

- The unmodified forced-entry route times out with zero accumulated bonus.
  Result export `04:456C` selects failure presentation `04:4DC4`, which runs
  240 ticks and 60 glyph advances without entering the special-award builder
  or transfer.
- A targeted completion-state route sets the objective to zero and supplies
  the accumulated-score/completion latches at the result boundary. Controller
  `04:4E97` takes its real zero-objective branch, loads `$CA=$32` (50), and
  enters common completion presentation `04:44DB`. The shared special branch
  adds 1,000 stored units 50 times, yielding `$00C350` (50,000), then
  transfers the award to the main score in 100 chunks of `$01F4` (500).

`tools/Analyze-BonusStage2ResolutionSemantics.ps1` and
`dc_rom_bonus_stage2_resolution` guard the paired emitters, remaining actor
initializers, 28-cell lookup, four tile calls, award sequence, objective
decrement, timeout cadence, and completion transfer. The resolution route is
byte-identical in Debug and Release:

- Census SHA-256:
  `e4104154585c0062c9f26cb31551d5e0f2015e240bea74868e61947c3d8707f5`
- Entry SHA-256:
  `70e149e081ad494ca9c61de285e14df6a7b8df2a1876ad5e19e3bb984162a1de`
- Call SHA-256:
  `11797b82527289b89446716b765ef611e5932ccef55e25ee767718c279fa3843`
- Scoped-write SHA-256:
  `d4b7325d26780c5918d0f5698fafbbb96c57cf614886056774724f25255f3701`
- Selected-state SHA-256:
  `52c30a3f2635e9e5e590c03e28fa3289fdb2b5789952e5594f7d5e90725370f6`

Release additionally guards the longer result routes:

- Timeout census/state SHA-256:
  `c0aa885ddd4df8e8993ccf532f567123dea9963150650c0d77e8d6184c8058ad` /
  `189f9ba5c9dcb5b5a1ea46308e39ccbba5e97a8b3aa0b9ea29758c025766d1f7`
- Completion census/state SHA-256:
  `eac2656f6d53a22600db65c2dd38279a7b19d7057a482021b407d46501249beb` /
  `152d7d5d51820be1c7e81daeeca689319898fe3d2d05667c1b17031fc42899dd`

### Full-contact perfect route

The earlier perfect checkpoint entered post-collision continuation
`04:5099` directly for each board cell. The full-contact route now begins at
live target state `04:5079` and retains its call to shared collision geometry
`04:5A12` before reaching that continuation.

On this collision branch, helper `04:5A12` performs 144 decrement-X writes at
`04:5ABE`, moving each seed backward by `$90` before the mapper reads it.
The route therefore seeds X as `desired-$70` modulo 256: `$F0/$00/.../$60`
resolve to board columns `$60/$70/.../$D0`. It also performs 56 increment-Y
writes at `04:5ACB` followed by 56 decrement-Y writes at `04:5ADD`, returning
each target to its seeded row before `04:5260`.

All 28 full envelopes run in consecutive frames 1,304-1,331. They enter
`04:5099` 28 times, map all 28 valid perimeter cells, invoke `04:52ED` 112
times, award 12,600 stored units, and reduce `$2D2E` from 28 to zero.
Controller `04:4E97`, the 87-step completion animation, gameplay-owned
`$2DBA` latch, earned-score transfer, 50,000-unit special award, and both
handshakes remain identical to the post-collision checkpoint.

Release-only `dc_rom_bonus_stage2_full_contact_perfect` and the
`-FullEnvelope` mode of
`tools/Analyze-BonusStage2ContactPerfectSemantics.ps1` guard the 317-entry
ordering, 10,705 selected writes, collision-coordinate scans, board
resolution, and result:

| Artifact | SHA-256 |
| --- | --- |
| Census | `533a642e33244ce95f9ab9ea9b42f9635600c463346b0dc52928c07326222a5d` |
| Entries | `9c45ebf3663a8b85dd4446c3c2026664f86c4813fcdeb3fcd69b093604a30c2e` |
| Selected state | `3469ef08e3600de588b45d613dd0363f6dc27d01fa6763296594c416df73d34b` |

## Cross-stage ordinary scored results

BS-1's targeted result oracle already separated zero-progress failure,
ordinary scored progress, and perfect completion. The same ordinary
scored-but-incomplete branch is now dynamically bounded for BS-2 through
BS-6. Each route enters its selected stage normally and preserves the
authentic nonzero objective initialization:

| Stage | Guarded objective initialization |
| --- | --- |
| BS-2 | `$2D2E=$1C` at `04:4E91` |
| BS-3 | `$2D2F-$2D31=1` at `04:53AE/53B1/53B4` |
| BS-4 | `$2D2F-$2D31=1` at `05:6A12/6A15/6A18` |
| BS-5 | `$2D2E=$0F` at `05:6E10` |
| BS-6 | `$2D2E=2` at `05:71E5` |

A one-unit `$2DB7` seed at frame 1,459 separates accumulated score from
objective completion. All five routes enter result branch `04:45F0` exactly
once, leave completion latch `$2DBA` clear, and take ordinary cleanup
`04:46A4` instead of perfect branch `04:464F`. Cleanup advances handshake
`$2D66` to one and clears `$2DB7-$2DB9`. The shared presentation performs
181 `04:4DC4` ticks and 46 glyph advances. Stage loading shifts its exact
frame placement by at most one frame: BS-2/3 end at frame 1,709, BS-4 at
1,708, and BS-5/6 at 1,710. No route enters perfect-award builder `04:4A5B`
or transfer `04:46B8`.

`tools/Analyze-BonusScoredResultSemantics.ps1` and the Release-only
`dc_rom_bonus_scored_results` regression guard the common selectors,
stage-specific objective initialization, ordinary handshakes and clears,
presentation cadence, perfect-path absence, and complete artifacts:

| Stage | Census SHA-256 | Selected-state SHA-256 |
| --- | --- | --- |
| BS-2 | `03e029d1729a05fe3ba465363b52d53b30a7a0a6c7294e966cb0fb23e939f1c1` | `37279a0833e597e336febde56a5f3829578c764769a5d34996f11d08874600f7` |
| BS-3 | `60345b0d06c0309c5e2c1da742806cfdf6e03293640f78a6b08abd7ad7504e21` | `a2464941052ef38669ec9c1b941dddb2868c2ddffd200a8b306644d5b146572b` |
| BS-4 | `3d4e67c9db612d7a2b8f510dc68b3048f5cad1e7e315a94746778c6cc045f279` | `dc7e34a53de43c324e1ad0c914821e66ada62e82aaaf2e6dc6ff926b520f4940` |
| BS-5 | `db1223ee8f639eab4666cf50594eef70dd00e410b13d2a3089f04128090cc5ff` | `81a0510b254e6f52e0c04bd2e0036f27798020cdbe1282019be25c95248dfbf1` |
| BS-6 | `086e35d380bd59d9f06f2d5d407bfd8eacafc5e2d033f93afb7c54792ac0c2c1` | `01669cf3cb8271d0793eb6677b8d1ddd92e8b4326ce6c6640fb9adf0bc426611` |

## Bonus stage 1 natural perfect outcome

The earlier BS-1 selection route lets its authentic initializer run at frame
1,241. Controller `04:44C6` initializes `$2D2E` to five and target
initializer `04:47B5` owns slots `$34/$35/$50/$51/$52`.

At frame 1,303, only those five actors' collision bits and remaining hit
counts are prepared. Their original `04:47D5` terminal-contact paths each
submit 200 stored units and enter the shared destruction response. The five
delayed `04:4828` continuations run at frame 1,309, decrementing `$2D2E`
through `4/3/2/1/0` without patching the objective count or accumulated
score.

Controller `04:44CE` observes zero at frame 1,310 and enters shared
completion `04:44DB` with multiplier `$0A`. Its 87 animation returns end
with the gameplay-owned `$2DBA=1` write at `04:4560` on frame 1,462. Result
export `04:456C` first transfers the 1,000 earned units in two chunks, then
perfect branch `04:464F` constructs the 10,000-unit special award and
transfers it in 20 chunks. The handshakes occur at frames 1,710 and 1,911;
the complete presentation performs 384 glyph ticks and 96 advances.

`tools/Analyze-BonusStage1NaturalPerfectSemantics.ps1` and the Release-only
`dc_rom_bonus_stage1_natural_perfect` regression guard the five target
identities, objective countdown, controller-owned latch, earned and special
score transfers, handshakes, presentation cadence, and complete artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Census | `a64ab169a786eb3df1aa6ea4622a92e9af1fd6ccc9bfbb9e81d3177e3bfe0a3c` |
| Entries | `a66366020d8a9ffce74f9ccd608d6f173aad85adda82ee6f8d9b242ece57f016` |
| Selected state | `41901f52d3e610a775263296193c9819f4b753bc02b0e18a07ff200ed01eccad` |

## Bonus stage 5 natural perfect outcome

BS-5 uses the same earlier stage-selection boundary described below for
BS-6. Its authentic initializer creates the five-by-three objective field in
slots `$51-$5A/$62-$66` and initializes `$2D2E` to 15 at frame 1,242.

At frame 1,304, all 15 original `05:7112` terminal-contact paths run in slot
order. Each adds 100 plus 400 stored units and decrements the shared count,
producing the exact `$0F`-through-zero countdown and 7,500 earned units.
Controller `05:6E16` observes zero on frame 1,305 and enters `04:44DB` with
multiplier `$1E` (30). The same 87-step animation writes `$2DBA=1` at
`04:4560` on frame 1,457 without a result-latch patch.

The result transfers the 7,500 earned units in 15 chunks, advances handshake
one at frame 1,718, builds the 30,000-unit special award, and transfers it in
60 chunks across frames 1,899-1,958. Handshake two follows at frame 1,959.
The complete presentation performs 437 ticks and 110 glyph advances.

`tools/Analyze-BonusStage5NaturalPerfectSemantics.ps1` and
`dc_rom_bonus_stage5_natural_perfect` guard the ROM contracts, target-slot
order, all 15 decrements, controller-owned latch, score transfers,
handshakes, presentation cadence, and complete Release artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Census | `5232409f1434cd2f8223eae6ea7303b02325e54205e6f895f3ab1f21894e8033` |
| Entries | `2ca1575931a1fa892e9755b1cf09d685c3195fced3b82f88780a21f3373e267b` |
| Selected state | `9ffa6901309a6fed1deac690038f1cef1eda42d2cd4906bf9b60b863f3d7d2aa` |

## Bonus stage 6 natural perfect outcome

The earlier BS-6 selection route establishes the stage request at frame
1,200, before the authentic initializer runs at frame 1,242. This avoids the
short frame-1,300 forced-entry window that reaches the result coroutine before
a newly selected controller can finish its completion animation.

At frame 1,303, only the two objective actors' collision bits and remaining
hit counters are prepared. Their original `05:72A8` terminal-contact paths
run at frame 1,304, award 50 plus 2,000 stored units apiece, and decrement
`$2D2E` from 2 to 0. Controller `05:71EB` observes the zero count on the next
frame and enters shared completion `04:44DB` with multiplier `$0F`.

The original animation remains scheduled for 87 returns through `04:4555`
across frames 1,338-1,424. Its terminal continuation writes the gameplay
perfect latch `$2DBA=1` at `04:4560` on frame 1,457 and immediately enters
result export `04:456C`. No objective-count, accumulated-score, or result-latch
patch is used.

The result first transfers the 4,100 units earned by the two terminal
contacts, advancing handshake `$2D66` to 1 at frame 1,711. Perfect branch
`04:464F` then builds the multiplier-15 special award as 15 additions of
1,000 stored units. Its 15,000 units transfer in 30 chunks of 500 across
frames 1,892-1,921, followed by handshake 2 at frame 1,922. The complete
presentation performs 400 `04:4DC4` ticks and 100 glyph advances.

`tools/Analyze-BonusStage6NaturalPerfectSemantics.ps1` and the Release-only
`dc_rom_bonus_stage6_natural_perfect` regression guard the ROM contracts,
98-entry ordering, controller-owned latch, earned and special transfers,
handshakes, presentation cadence, and complete artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Census | `fef4dc91446e524de66892be7692b88d7782cb0113cde447c569a3c8a7b1c08a` |
| Entries | `7e05d90d709b89945587846e1b4b1080dc5ab9d9212186c2af8dc56910b0f2e1` |
| Selected state | `d1dd5a31f20306e8aa776b599707d3c9bcbf65137730ffe2270a35c925a4ad4b` |

## Bonus stage 4 linked-target RNG paths

BS-4's paired source/linked-target family contains two periodic RNG
consumers. Both decrement `$3B00,X`, reload it to `$20` (32 ticks) on
expiry, mask the shared RNG result with `$1F`, pass the result to common
motion helper `01:C075`, and then integrate the actor through `01:C012`:

| Consumer | Target state | Calls | Routed actors |
| --- | --- | ---: | --- |
| `05:6B88` | Initial linked target `05:6B7E` | 18 | Slot `$66` before its continuation, then authentic later allocations `$68/$6D` |
| `05:6CB1` | Post-allocation state `05:6CA0` | 22 | Seeded slot `$67`, plus slot `$66` after its authentic continuation |

The combined route retains the existing real source continuation at
`05:6ACC`, which allocates slot `$66`. A separate targeted event-state seed
places slot `$67` at the already bounded post-allocation dispatcher with a
one-tick timer. The authentic slot `$66` transitions from `6B7E` to `6CA0`
during the route, while later source activity allocates slots `$68` and
`$6D`. This proves both consumers in their shared lifecycle rather than only
executing disconnected call sites.

All 40 calls enter `00:E8B3`, complete in 282 master cycles with the same
eight ordered writes, and reproduce the exact seed recurrence. The two call
sites contribute 18 and 22 calls respectively, and every expiry reloads the
indexed timer to `$20`.

`tools/Analyze-BonusStage4RngSemantics.ps1` and
`dc_rom_bonus_stage4_rng` guard both ROM call-site signatures, exact
consumer/actor/frame ordering, RNG outputs, the complete eight-write
recurrence, and timer reloads. Debug and Release are byte-identical:

- Census SHA-256:
  `1c1ac47ddc107600e254ea3693ebe7852ec9a9addb52fa64374ba3192eee2817`
- Entry SHA-256:
  `078475a2616e02c1190ed6ca84459985c5f004f709a53b1afd8c837fdd71880c`
- Call SHA-256:
  `4937301556a172c2d2f9694167cd0c7c48596c280c27dbe3c1a9c8b23298f29a`
- Scoped-write SHA-256:
  `e93d97f153f5afac834d06766943f0f94de3d22e53aa5a3d6563bed8432932f0`
- Selected-state SHA-256:
  `92f94c30b19ff03a3645a7df63eb6e3ce6e6544383fa555130cc6999726cdb86`

## Bonus stage 6 hazard RNG paths

BS-6 objective animation state 5 naturally calls hazard allocator `05:72E0`.
The deterministic route's terminal contact frees slot `$34`; at frame 1,510,
objective slot `$35` reaches that animation state and allocates a type-`$C2`
hazard back into slot `$34`.

The allocator contains the paired RNG consumers:

| Consumer | Routed result | Transform | Dynamic result |
| --- | ---: | --- | --- |
| `05:732E` | `$A0` | `$18 + (A & $1F)` | Stores minimum timer `$18` at `$3B34` |
| `05:733B` | `$C5` | Table `05:73B1[A & $0F]` | Index 5 selects value 5 for motion helper `01:C072` |

The second transform installs signed motion bytes
`$34B4:$3434=$FFE8` and `$35B4:$3534=$003B`. The allocator also copies the
source-relative position, installs live dispatcher `05:734E`, collision
selector `$13`, completion selector `$81`, and command `$32`. Its complete
scope returns in 12,924 master cycles with 281 ordered writes.

The route contains five shared `00:E8B3` calls: the two existing objective
initializers, the new `732E/733B` pair, and one later objective initializer.
All five complete in 282 cycles with eight ordered writes and reproduce the
exact seed recurrence.

`tools/Analyze-BonusStage6RngSemantics.ps1` and
`dc_rom_bonus_stage6_rng` guard the allocator, consumer signatures,
16-entry selector table, exact call order, RNG recurrence, complete allocated
actor fields, selected velocity vector, and audio command. Debug and Release
are byte-identical:

- Census SHA-256:
  `5e48d45f879ccddd0231a4642a7542fb117abbd578c7d81666d7780e282c9238`
- Entry SHA-256:
  `7260f94a57e30deaab5f972ab1f9e7b0786c14f47ee6ed7e6e9ab1ac1fdc3201`
- Call SHA-256:
  `9dd2612a519f54ced33a70b9d871929a08c229d84a9c2ab249e353285603488d`
- Scoped-write SHA-256:
  `347f443af749d6da0d934fdad52f245647659bfbf954e801b2c7487c9949cbda`

## Maximum-score ending RNG paths

Score saturation helper `00:F5A9` clamps the stored score to `$98967F`
(9,999,999 stored units) and sets ending-request bit `$2CC0.7`. The main
session dispatcher observes the negative byte at `01:C190` and takes the
real transition through `01:C239`. A targeted frame-1,300 `$2CC0=$80` seed
therefore enters the original bank-7 maximum-score ending without patching
code, mapper state, or the ending controller.

The static cartridge census originally named the final two physical-bank-7
consumers `07:68C7/68CF`. Runtime mapping proves that the ending controller
is dispatched through MPR2 and the live bank-qualified logical keys are
`07:48C7/48CF`. This alias correction is material because execution hooks
are keyed by physical bank and logical PC.

Fixed ending-controller slot `$05` uses entry `07:4899`, steady state
`07:48B2`, and an eight-frame cadence. Its 23 routed events have two
independent RNG decisions:

| Consumer | Transform | Dynamic coverage |
| --- | --- | --- |
| `07:48C7` | Low nibble passed to geometry helper `07:48DF` | 23 calls covering 15 of 16 indices |
| `07:48CF` | Bit 0 selects command `$2C/$2D` | 23 calls: `$2C` seven times, `$2D` sixteen times |

Geometry helper `07:48DF` selects one of sixteen base coordinate pairs, adds
six fixed signed offsets, and calls allocator `07:4980` for each. Every event
creates six type-`$8F` actors with live dispatch `07:49DA`, lifetime 4, and
shared presentation field `$10`: 138 actors over the routed interval.

The 1,800-frame route contains 62 calls to shared RNG `00:E8B3`: 16
normal-table calls before the transition and 46 ending calls. All complete
in 282 master cycles with the exact eight ordered writes. The ending
controller contributes 184 bounded calls: 161 idle 27-cycle calls and 23
event calls with 272 writes apiece.

`tools/Analyze-MaximumScoreRngSemantics.ps1` and
`dc_rom_maximum_score_rng` guard the saturation request, session branch,
live logical aliases, controller cadence, both output transforms, geometry
tables, all six allocations per event, commands, and complete RNG
recurrence. Entry, call, and scoped-write artifacts are byte-identical in
Debug and Release:

- Entry SHA-256:
  `698b5ebf3b98bc33f35019a2f95041c473989c706dafc31eacd75c2683aaecbc`
- Call SHA-256:
  `49cd13f4257c7f596428f2ac120bc353ad260c5f5a2d5b962bdd2206de93edbf`
- Scoped-write SHA-256:
  `2beaddce2b938f3d9a3c6a8ce1afd565719b7d234ab5e6f49743823c27b4187e`

The ending presentation has a configuration-specific framebuffer interval
from frames 1,555 through 1,680, after which the framebuffers converge.
Work RAM, audio, and all semantic traces remain identical. Census SHA-256 is
`ac639564102a7549bc6aaa58aabb32e3231afafc5a3e3defb8d2a383a07bbdfb`
in Debug and
`97f5089683bb67c53a3755a2d320ce1bcf5ea5e347d65200373a3f22b69cbc4b`
in Release.

## Game-to-audio command dispatch

The state-census route reaches the accepted game-side audio command path at
`00:ED18` 66 times. This is a synchronous dispatcher boundary, not a deferred
queue:

1. Mapped gameplay code normally calls JMP-table entry `01:C087`, which
   tail-jumps to `00:ECFD`.
2. `ECFD` stores the command from A in zero-page `$2A`, rejects `$FF`, and
   conditionally filters commands `$31/$27/$47` while `$2CC7` is nonzero.
3. Accepted commands fall through to `ED18`, which selects export 6 in raw
   mapper bank `$45`.
4. `00:ED58` saves MPR2-MPR4, maps raw banks `$45-$47`, and invokes the common
   `00:E262` indirect trampoline. Raw bank `$45` mirrors backing ROM bank
   `$25` in this cartridge image.
5. Export `45:4006` jumps to the interrupt-safe wrapper at `45:4B7B`.
   `45:4BCC` latches the command at work-RAM offset `$00D4`, doubles it, and
   loads a little-endian descriptor pointer from logical table `$5D81`.
6. Every observed descriptor has a nonzero first byte and follows the data
   parser that populates driver channel state in logical `$2E00-$2FFF`.

The outer `ED18` scope is the useful replacement-facing boundary because it
includes bank mapping, the original audio driver, and restoration of the
caller's X, Y, SP, and all eight MPRs. Its 66 calls span frames 7-3,466, take
2,400-10,791 master cycles, and contain 66-271 ordered writes apiece. The
scopes contain 7,261 writes total: 5,628 executed by raw bank `$45` and 1,633
by bank zero. All 2,737 writes to physical work-RAM offsets `$0E00-$0FFF`
(logical audio state `$2E00-$2FFF`) belong to the driver.

| Command | Calls | Descriptor | Tag | Producer caller/resume paths |
| --- | ---: | --- | ---: | --- |
| `$01` | 1 | `$5E11` | `$06` | `C13E/C141` (1) |
| `$0C` | 2 | `$89C0` | `$06` | `5633/5636` (1), `7091/7094` (1) |
| `$20` | 11 | `$9103` | `$02` | `78F2/78F5` (9), `790F/7912` (2) |
| `$21` | 5 | `$9128` | `$02` | `7F75/7F78` (5) |
| `$25` | 1 | `$9183` | `$01` | `52B3/52B6` (1) |
| `$29` | 7 | `$8C07` | `$02` | `6033/6036` (7) |
| `$2B` | 1 | `$8C70` | `$03` | `5202/5205` (1) |
| `$2D` | 5 | `$8CC2` | `$02` | `42A3/42A6` (5) |
| `$2E` | 6 | `$8CF6` | `$02` | `570F/5712` (1), `573F/5742` (1), `5777/577A` (1), `5A60/5A63` (2), `5F1B/5F1E` (1) |
| `$33` | 1 | `$8DCC` | `$01` | `60DD/60E0` (1) |
| `$34` | 4 | `$8DE7` | `$01` | `42A3/42A6` (1), `64B4/64B7` (3) |
| `$39` | 1 | `$8EC5` | `$02` | `42A3/42A6` (1) |
| `$3D` | 4 | `$8F4C` | `$02` | `518A/518D` (4) |
| `$3E` | 1 | `$8F99` | `$01` | `55FC/55FF` (1) |
| `$42` | 1 | `$9056` | `$01` | `5638/563B` (1) |
| `$44` | 15 | `$908A` | `$01` | `67CC/67CF` (15) |

Most producer sites load an immediate command before calling `C087` or
`ECFD`. The `02:42A3` producer instead loads `$2D6E`, accounting for its
observed `$2D`, `$34`, and `$39` commands. Gameplay-facing names for the
command IDs remain deliberately unset until their audible effects and
triggering mechanisms are correlated.

Aligning each call's absolute cycle with the actor identity trace recovers the
occupancy/type byte and live `$3F80:$3F00,X` dispatch pointer immediately
before the command. Calls before the normal-table layout are retained as
session/scene traffic; the gameplay-era ownership is:

| Owner | ROM initializer | Position | Live dispatch | Commands |
| --- | --- | --- | --- | --- |
| Ball slot `$00` | Dedicated ball allocation | Dynamic | `C696` | `$01` |
| Fixed slot `$0E` (Devil Shooter) | `50EE` | (264, 630; live Y 671) | `5116`, `5159` | `$25`, `$2B`, `$3D` |
| Fixed slot `$13` (Dragon's Head) | `5A31` | (64, 510) | `5A4B` | `$2E` |
| Fixed slots `$14/$15` (Bumpers) | `789A/78A4` | (190/74, 604) | `78C7` | `$20` |
| Fixed slots `$26/$27` (Front Spiders) | `646B` | (64/216, 636) | `6490` | `$34` |
| Fixed slot `$2D` (Tower) | `60B3` | (144, 538) | `60C3` | `$33` |
| Fixed slot `$30` (Giant Skeleton) | `5EE4` | (224, 518) | `5EEC` | `$2E` |
| Fixed slots `$58/$59/$5A` (Three Blobs) | `664A` | (128/160/192, 254) | `66A3`, `66D3` | `$44` (hits on `$59/$5A`) |
| Small Soldiers, dynamic type `$82`, slots `$62/$63/$69` | Runtime allocation | Dynamic | `7F57` | `$21` |
| Tower insects, dynamic type `$82`, slots `$64/$67/$6A/$6D` | Runtime allocation | Dynamic | `6016` | `$29` |
| Shared impact-burst effects, dynamic type `$8F`, slots `$62/$69/$70/$71` | Runtime allocation from four source families | Source position | `02:4290` | `$2D`, `$34`, `$39` |

This provides stable mechanism identities without guessing visible or audible
names. In particular, command `$2E` is shared by pre-table scene code and two
unrelated fixed actors, while `$34` is shared by a fixed pair and the dynamic
`4290` family. A native event layer therefore cannot treat command ID alone as
a gameplay mechanism identity.

`tools/Analyze-AudioCommandSemantics.ps1` verifies the wrapper and driver ROM
signatures, bounded path, complete producer/command histogram, descriptor
pointers/tags, mapping restoration, channel-state ownership, all 10 relevant
layout records, and the complete slot/type/dispatch context histogram. The
first call has a normalized replay at frame 7 for command `$3E`, with 2,400
body cycles and 66 writes. Debug and Release reproduce:

- Entry SHA-256:
  `7f47c3163e51226ff5b85b5cd13dc354cdcdde832f24ca3c6b63f2e8eb2ef3b3`
- Call SHA-256:
  `6f12c1827eee9f7b924c42664bc2720f78a285ff2409bd53d4816558eb758a84`
- Write SHA-256:
  `ffa478f6c45d353f37beda37bd15126f9e4bd6d8447ab058a2ebfca434b71c80`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

The per-frame audio update export at `45:4003` remains emulated and outside
this command regression. It is active 3,599 times on the route; the lower
timer/PSG writer at `00:FC43` is much finer grained, with 286,408 entries.

## Lower-table Tower family

The supported game's
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names the adjacent lower-table mechanisms the **Tower** and the **insects
living in Tower**. The fixed layout, world positions, score values, and
parent/child counters make those names stable code identities:

| Mechanism | Fixed slot | Layout initializer | Position | Type | Steady dispatch |
| --- | --- | --- | --- | --- | --- |
| Tower-insect source | `$2C` | `03:5F22` | (140, 526) | `$82` | `03:5F4A`, then `03:5F5F` |
| Tower | `$2D` | `03:60B3` | (144, 538) | `$83` | `03:60C3` |

The insect source clears the live-child count at `$2D2B`, initializes a
four-spawn countdown, and uses `BSR 03:5F78` to allocate children. Each child
copies the source position and collision fields. When `$2D2B` reaches zero,
the source declares a 200-unit (20,000 displayed point) group-clear award.
The child's two-hit behavior is bounded separately below.

The Tower initializer clears ten-step progress `$2D3F`, initializes the
per-actor state and a five-step global gate, and falls into `03:60C3`. The
state-census route contains 2,646 steady calls from frames 873-3,600:

- 2,615 calls take the 27-cycle, zero-write no-hit path.
- 30 no-hit calls contain only periodic interrupt activity.
- One hit at frame 3,353 clears the collision result, awards 50 stored units
  (5,000 displayed points), and emits audio command `$33`.
- That hit advances `$2D3F` to 1, sets `$2CFC`, transitions slot `$2D` through
  `03:611C` and `03:611F`, then returns to `03:60C3`.

`tools/Analyze-TowerFamilySemantics.ps1` guards both fixed layout records,
the insect emitter/spawner bridge, Tower ROM control flow, all bounded calls
and writes, the four child allocations, Tower dispatch transitions, and the
first normalized no-hit replay. Debug and Release reproduce:

- Entry SHA-256:
  `9771eca6e3a0c8f297eb9311ebc18af9b498362770a3b157e91553af6f4f0c79`
- Call SHA-256:
  `1b6440a384b815ba8ee97473dc389a56f9455ec0a82ee307c5baa499bbf3712f`
- Write SHA-256:
  `26696c169ae2296a752ea3916c9151134febbb89ef1a9eea54826645c64815c3`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Tower Guard and bonus-stage-5 entrance

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
lists a **Guard** beside the Tower and describes five passes before access to
BS-5. Fixed slot `$2E` is directly above-left of the Tower/insect records and
implements that exact five-step entrance contract:

| Mechanism | Slot | Position | Initializer | Steady dispatch |
| --- | --- | --- | --- | --- |
| Tower Guard / entrance surface | `$2E` | (132,510) | `03:6099` | `03:60A6` |

The record has type `$82`, state `$37`, and flags `$84`. Initialization sets
collision selector `$10`, closed completion selector `$84`, and installs
`03:60A6`. Every live update reads the Tower-owned gate counter `$2D4D`.
Nonzero selects `$84`; zero selects open completion ID `$8E`.

Tower slot `$2D` initializes `$2D4D` to 5. Its first qualifying hit arms
`$2CFC`; later qualifying passes decrement `$2D4D`, with zero opening the
Guard surface. The open-entry routine at `03:612B` clears the collision
latch, invokes the table bonus helper with `$10`, selects bonus stage 5,
sets the session-transition mode, emits command `$41`, restores `$2D4D` to
5, and sets `$2D2D` for the next Tower/insect cycle.

The canonical route reaches only one Tower hit. At frame 3,353 it changes
`$2CFC` from 0 to 1 but does not decrement `$2D4D`, so all 2,650 Guard
updates write closed selector `$84`. Of these calls, 2,592 are exact
48-cycle, one-write leaves and 58 include periodic interrupts. Normalized
replay `03:60A6@1` occurs at frame 873 with 48 cycles and one write. Selector
`$8E` and the BS-5 entrance remain ROM-bounded and are not claimed as
dynamically exercised.

`tools/Analyze-TowerGuardSemantics.ps1` guards the layout identity,
initializer, closed/open selector logic, Tower counter initialization and
progression, BS-5 entrance signature, complete selector population, shared
gate timeline, timing classes, initial actor state, and normalized replay.
Debug and Release reproduce:

- Entry SHA-256:
  `102589405057af635281317de488ab08131f2a9254b67b52b98d560994e271e6`
- Call SHA-256:
  `141a3045e96ddfb1f0c736850ddcbebfa6ebc18231d66f115addb311e704258f`
- Write SHA-256:
  `c37bed44613fad1cb3f8a8776d182be59e5cd6cd025faa4890832ba9c52bd3e6`
- Selected-state SHA-256:
  `d66b67b3a2cac7a9f8ec532124a2404a548c7a184a6d2f58030b36e891552f6c`

## Lower-table bumpers

The manual's **Bumpers** are the mirrored category-2 fixed records in slots
`$14/$15`, at world positions `(190,604)` and `(74,604)`. Initializers
`03:789A/78A4` assign side indices 0/1 and converge on steady dispatcher
`03:78C7`. Both actors use type `$82`; the layout initializer and position,
not that shared type, provide their stable gameplay identity.

On collision, `03:78C7` clears bit 7 of `$3800,X`, increments phase
`$3E80,X`, and selects one of nine handlers:

- Phases 1-6 award 10 stored units (1,000 displayed points), emit command
  `$20`, increment `$3B00,X`, and refresh the bumper appearance.
- Phases 7-8 apply the same score/audio rule and advance the secondary damage
  counter at `$3B80,X`.
- Phase 9 adds the 200-unit (20,000-point) destruction award, applies the
  completion transition, and calls `03:7998`.

The terminal spawner makes six allocation attempts, initializes each child
as type `$83` at `03:7A71`, copies the bumper position, and increments the
side-selected live count at `$2D11,Y`. This is the manual's six-creature
release. Neither bumper reaches phase 9 on the current route, so the
destruction and child paths are guarded by exact ROM signatures rather than
claimed as dynamically exercised.

The bounded route contains 3,468 calls—1,734 per bumper—from frames
872-3,562. It records 11 hits: slot `$15` reaches phase 8 and slot `$14`
reaches phase 3. All 11 award 10 units and emit `$20`, contributing 110
stored score units. Of the remaining calls, 3,419 are 27-cycle zero-write
no-hit paths and 38 include periodic interrupt activity. The first call is
the normalized replay sample at frame 872.

`tools/Analyze-BumperFamilySemantics.ps1` guards both layout records,
initializers, the nine-entry phase table, every routed hit and counter value,
score/audio coupling, the terminal award and six-child initializer, fixed
dispatch identity, inactive terminal path, and normalized replay. Debug and
Release reproduce:

- Entry SHA-256:
  `5fb7885f28519eec2758e06dac02327d2a9a86f3c0a416124fa0c7ce4865f3db`
- Call SHA-256:
  `0495b9de1e3dcfb1781e0056411705464a2e52d8ddcde371d2efc4d0124b882a`
- Write SHA-256:
  `754938e2e0b04dad1c51ee416f6d22ea3865055cfe4390a1af9d1610072f57da`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Devil Shooter ball service

The supported game's
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names the far-right launch mechanism the **Devil Shooter** and says that it
serves the ball under Button I control. Fixed layout slot `$0E` supplies that
stable identity: its record starts at `(264,630)`, uses type `$82`, and calls
initializer `02:50EE`. The initializer moves the live actor to `(264,671)`,
sets a `(264,632)` reference position, and installs idle dispatch `02:5116`.

The deterministic route bounds the first service cycle across two dispatcher
states:

1. `02:5116` runs 125 times. Its first 123 ordinary calls are 27-cycle,
   zero-write idle samples. At frame 1,064 the collision result enters
   `02:52B1`, sets the shooter-occupied latch `$3C80`, emits command `$25`,
   places the ball at X=`$010E`, and advances the live pointer.
2. The actor reaches Button-I dispatcher `02:5159` at frame 1,084. The route's
   scripted Button-I hold yields 96 consecutive charge calls from frames
   1,085-1,180. These update the ball through `02:52E3`; charge counter
   `$2D15` progresses from `$00` to `$18`. The first four calls emit command
   `$3D`.
3. Four released-input calls at frames 1,181-1,184 take the 96-cycle,
   zero-write delay path. Frame 1,185 computes `$F8` launch velocity at
   `$3580`, clears animation state, emits command `$2B`, and advances to
   `02:5208`.

No score-accumulator store occurs anywhere in this boundary: the Devil
Shooter serves and launches the ball but awards no points. The global actor
identity trace retains the subsequent pointer progression through
`02:5223`, `02:5249`, and `02:525F`.

`tools/Analyze-DevilShooterSemantics.ps1` guards the fixed layout record,
initializer position overrides, Button-I test, catch/charge/release ROM
signatures, call and return populations, ball placement, charge progression,
audio schedule, launch velocity, absence of score writes, actor-pointer
timeline, and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `c5b2500b726c9e161a138880e5f35b1dd306cd9c41e06bfb2994c8716367ec1d`
- Call SHA-256:
  `0ee9384584f2d5e177ce8580a6dc535c0c8a30c94b3d7dbaf953c97ad96bfdaf`
- Write SHA-256:
  `90557ce04ba3e79132967255e9cdf6747cb4c5628f0f99a15d6f5c402ade0139`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Devil-Shooter companion actors

Four contiguous type-`$80` records surround Devil-Shooter slot `$0E` in the
lower-right table region:

| Slot | Position | Initializer | Routed roles |
| --- | ---: | --- | --- |
| `$0A` | (244,676) | `02:53A8` | Four-frame session-gated animation |
| `$0B` | (256,646) | `02:5394` | Charge-to-position projection |
| `$0C` | (264,686) | `02:53FC` | Three-frame charge appearance |
| `$0D` | (248,666) | `02:5421` | Five-frame session-gated animation |

Their placement and exclusive dependency on Devil-Shooter charge counter
`$2D15` identify them as presentation/control companions rather than
independent scoring targets.

Slot `$0B` runs `02:5397` and writes its integer vertical coordinate as
`$02A8 + floor($2D15 / 2)`. All 1,260 routed calls reproduce that equation
exactly. Slot `$0C` caches `$2D15` at `$3E0C`; each change increments
appearance `$398C` modulo three. The route records 58 cache changes and 19
wraps while the Shooter charges and resets.

Slots `$0A/$0D` clear visibility bit 5, initialize eight-tick timers, and
wait in `02:53BB/5434` for session bit `$53.3`. At frame 942 they transition
to active states `02:53C8/5441`, using helpers `02:53E4/545D` to cycle
four/five appearances. When session bits `$53.0/.3` clear, the right actor
enters zero-write dormant state `02:5456` at frame 2,131 and the left enters
`02:53DD` at frame 2,135.

The combined boundary contains 5,231 calls and 6,032 scoped writes. It
includes 1,044/1,076 uninterrupted 24-cycle dormant calls, 1,213
uninterrupted 75-cycle charge-position calls, and exact timer/advance/wrap
populations for both active animations. Periodic interrupts account for the
remaining timing variants. Eight normalized replays cover every routed
non-initializer state.

`tools/Analyze-DevilShooterCompanionSemantics.ps1` guards all four layout
records, exact ROM state machines, charge-position equation, charge-change
cache, animation moduli, dispatch timeline, timing populations, absence of
score/audio ownership, and normalized replays. Debug and Release reproduce:

- Entry SHA-256:
  `8b23e18603b196bf1adb7c67527f111769099e01682d0b47625a0a40a918c9b0`
- Call SHA-256:
  `31713928b949d25e0a2345febd119371e14e2d4723574d425ffc02d8adb565d2`
- Write SHA-256:
  `ecfc80c0488c5fe7e98f1ffe67db35d108c4746f222345f846f6af443ad8acc3`
- Selected-state SHA-256:
  `13e3373182b0982eb40b61df93d0002d6ba9004c6b2d099a0129380ec6e21211`

## Devil's Coffin and return-gate cycle

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names **Devil's Coffin** and assigns it the function “opens return gate back
to Shooter.” Fixed slot `$0F` is the matching right-middle-table target:

| Slot | Position | Initializer | Initial type/state | Steady dispatch |
| --- | ---: | --- | --- | --- |
| `$0F` | (221,257) | `03:63E1` | `$83/$11` | `03:63F1` |

Its state contract links it directly to the Shooter session. While `$53.3`
is set, `03:63F1` refreshes type `$83`. A collision installs `03:640A`,
which advances appearance `$398F` from 0 through 3 at four-tick intervals.
`03:6432` hands off to `03:6445`, which reverses the appearance back to 0,
clears `$53.3`, changes the type to `$82`, and returns through initializer
`03:63E1` to steady state.

The routed ball contacts the Coffin twice, at frames 1,244 and 1,297. Each
contact produces the complete `0→1→2→3→2→1→0` cycle over 20 calls:
nine opening calls, one handoff, nine closing calls, and the same-frame
reinitializer. The first completion consumes the active `$53.3` state; the
second repeats the visual cycle while that bit is already clear. The family
submits no score award and no audio command, so its bounded ownership is the
return/session state and presentation cycle rather than a scoring rule.

The full boundary contains 684 calls and 355 scoped writes: three
initializers, 643 steady checks, 18 opening calls, two handoffs, and 18
closing calls. Normalized samples cover `03:63F1`, `03:640A`, `03:6432`,
and `03:6445`.

`tools/Analyze-DevilsCoffinSemantics.ps1` guards the fixed record, complete
ROM state machine, both collision inputs, appearance and dispatch timelines,
type/session-state transition, score/audio absence, timing populations, and
all four replays. Debug and Release reproduce:

- Entry SHA-256:
  `d98bad357038cec227531a34c354103eec68890c1f0a73795e17af7bcaf6d4fa`
- Call SHA-256:
  `5ca7ca788988c56bb24209faa87a4f08ece30a9be5148a34cf7f5c0ab05a6888`
- Write SHA-256:
  `9ae1c018ec68f77c49a2188b6e0950527ba70f6c13cd5ea2260f79873284068c`
- Selected-state SHA-256:
  `1c3fd4bac31662487cb5c48b2389a4584526bd567a42fdcc587bf5d0cbfe3169`

## Dragon's Head and Giant Skeleton

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names the lower-left major target **Dragon's Head** and the lower-right target
**Giant Skeleton**. The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
describes the same actors as the Dragon and Big Skull and confirms their
left/right placement and 5,000/4,000-point hit values. Those positions and
values resolve the two previously unnamed command-`$2E` owners:

| Mechanism | Slot | Layout initializer | Position | Type | Steady dispatch |
| --- | --- | --- | --- | --- | --- |
| Dragon's Head | `$13` | `02:5A31` | (64, 510) | `$82` | `02:5A4B` |
| Giant Skeleton | `$30` | `02:5EE4` | (224, 518) | `$83` | `02:5EEC` |

The Dragon initializer sets its hit counter at `$3C00,X` to 8. Its steady
dispatcher normally takes a 27-cycle, zero-write no-hit path. A hit clears
bit 7 of `$3800,X`, submits 50 stored score units (5,000 displayed points),
emits command `$2E`, runs the common bonus/appearance helpers, and decrements
the counter. The route hits it at frames 2,076 and 2,080, advancing the
counter 8 → 7 → 6 and contributing 100 stored units. The counter-zero path
is not active on this route; its exact ROM signature adds a second 50-unit
award on the eighth hit and begins the multi-stage mouth-opening transition.

The Giant Skeleton dispatcher invokes the shared RNG every active update and
maps its result into five appearance values `$02-$06`. The deterministic
route contains 2,139 calls and reproduces the complete value histogram. Its
frame-2,642 hit clears bit 7 of `$3800,X`, submits 40 stored units (4,000
displayed points), emits command `$2E`, and returns with the same live
dispatcher. Unlike the Dragon, this observed hit has no countdown or
completion transition.

The combined boundary contains 4,283 calls from frames 872-3,600 and 50,711
ordered writes. The three hits contribute 140 stored score units and three
command-`$2E` writes. The Dragon accounts for 2,144 calls, including 2,125
27-cycle zero-write paths, 17 interrupt-bearing no-hit paths, and two hits.
The normalized first Dragon call is a 27-cycle zero-write sample.

`tools/Analyze-DragonGiantSkeletonSemantics.ps1` guards both layout records,
initializers and hit rules, the inactive Dragon terminal path, fixed
identities, call populations, RNG-derived Giant-Skeleton appearance
histogram, every score/audio write, counter progression, and normalized
replay. Debug and Release reproduce:

- Entry SHA-256:
  `ca858c9f1fcea148c369d95bd13b026f5b45b1f35d5ff0cefbd8ac825d129cf5`
- Call SHA-256:
  `aeb9de1db6f9dfae7e59a45c37bc82f2cb2e1360892c76dfcafe1131d3332819`
- Write SHA-256:
  `698de7535fbd7958698b1e7ee897b5082ac5a18ddd4734fe263feabc45c95372`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Three Blobs and the color cycle

The
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
describes **Three Blobs** hovering above the central face, following a
four-color visual cycle, awarding no points on ordinary contact, and
producing a special result when all three are red. The fixed layout and ROM
state contract match that description:

| Slot | Position | Record parameter | Global state |
| --- | --- | ---: | --- |
| `$58` | (128, 254) | 0 | `$2D39` |
| `$59` | (160, 254) | 1 | `$2D3A` |
| `$5A` | (192, 254) | 2 | `$2D3B` |

All three category-2 records use initializer `03:664A`, type `$82`, and
initial states 0/1/2. The six internal states select the appearance sequence
`$8A/$82/$85/$8A/$8E/$85`, so repeated internal states collapse to four
visual IDs. RNG calls at `03:6678/6697` seed the motion timers before the
actors enter alternating dispatches `03:66A3/66D3`.

Both movement phases converge on collision handler `03:67B3`. A collision
clears bit 7 of `$3800,X`, emits command `$44`, advances the actor-selected
global state modulo six, and refreshes `$3D00,X`; it does not submit a score
award. The route records 16 such producer attempts:

- Slot `$58` is not hit.
- Slot `$59` receives seven hits and ends at state 2.
- Slot `$5A` receives nine calls and ends at state 4. Two calls occur during
  frame 1,546 and both write state 4, so only eight effective advances occur.
- Those two same-frame attempts coalesce at the per-frame audio export,
  explaining why the earlier command census contains 15 accepted `$44`
  exports rather than 16 producer writes.

Fixed slot `$57`, immediately before the three visible records, is the family
controller. Its category-2 layout record uses initializer `03:65BC`, type
`$82`, parameter 2, and installs steady dispatch `03:65C2`. The steady
handler compares `$2D39-$2D3B`. Its all-state-4 branch is consistent with the
guide's all-red result: it submits 100 stored score units and sets bit 7 of
`$53`. The post-outcome path randomizes one state modulo four and assigns the
next two consecutive values.

The controller trace contains one initializer and 2,650 steady calls. Of the
steady calls, 2,565 return after the first comparison, 35 reach the third
value before returning, and 50 include periodic interrupts. Every steady
return is at `03:65E9`; the routed states never align, so no outcome-side
state, score, or bonus write occurs. The first normalized sample is
`03:65C2@1` at frame 873 with 42 cycles and zero writes. The result contract
remains exact-ROM-bounded without being presented as dynamically exercised.

A targeted equality route now preserves fixed controller slot `$57` and
changes only `$2D3A/$2D3B` at frame 1,200 to match existing state zero in
`$2D39`. The authentic `03:65C2` comparisons reach state-zero outcome
`03:65DE`, which submits 10 stored score units and enters shared reset
continuation `03:661B`.

Reset consumer `03:6620` receives shared RNG result `$5D`. Its low two bits
select state 1; the caller increments and masks twice to install consecutive
states `1/2/3` in `$2D39-$2D3B`. It raises reset latch `$2D3C`, starts the
shared eight-tick continuation, and clears the latch at frame 1,208.
`03:661B@1` and `03:6620@1` reproduce as 774-cycle/27-write and
753-cycle/26-write normalized scopes.

`tools/Analyze-ThreeBlobsResetRngSemantics.ps1` and
`dc_rom_three_blobs_reset_rng` guard the equality and reset signatures,
state-zero award, all 23 shared RNG recurrences on the route, exact
consecutive-state writes, latch lifetime, and both normalized replays. Debug
and Release have distinct framebuffer census hashes but byte-identical
semantic artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `527e8b3f876703cc5444a3432e8dae8ada1e8a848a93e5e847193675c262c4b3` |
| Release census | `8885978460f9f0c234ad13a27b113818e32de5c12cfbb84a31c56b27867d5b61` |
| Entry | `288c2cf4baad7a15eb8f7923ada61dbe526606bf8272b86f46d1ef83cee98533` |
| Call | `20006bec6775a00d04288c669fe695346ab0b40999c1571a20d700f9bcaf4c27` |
| Scoped write | `25c575c6ccfe1939c9886452abc7bbb2212a810ffeb4e09391454c298d232ebb` |
| Selected state | `96b9bd72a8a5343841098aa86ae1ed498ede65f5d6a9e38b291f65a079d74aa6` |

The bounded trace contains 1,827 calls and 33,397 ordered writes. Each slot
contributes 369 calls to `03:66A3` and 240 to `03:66D3`. The first normalized
sample is `03:66A3@1` at frame 1,229 with 282 cycles and six writes.

`tools/Analyze-ThreeBlobsSemantics.ps1` guards all three layout records,
parameter/state ownership, initializer and movement signatures, the complete
collision chronology and state sequence, score absence, the command-`$44`
producer schedule, four-ID appearance mapping, inactive all-state-4 rule,
fixed dispatch identity, and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `7f53b1f567fb59dc13c2967e8a083cb2d24b9fe0c0f45dd13e51fe962db00d5e`
- Call SHA-256:
  `b2a96ba58d4ecc6063e1ea812b1e8b721e3a265f6c6333010e9efc94eb949f43`
- Write SHA-256:
  `e72fbf7baa742a8f23bace6ed57adda2e407e906953450306507fb6b476a6b2c`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

`tools/Analyze-ThreeBlobsControllerSemantics.ps1` independently guards slot
`$57`, the equality/outcome/reset ROM contract, both routed mismatch paths,
interrupt-bearing timing bounds, selected-state write absence, and normalized
controller replay. Debug and Release reproduce:

- Entry SHA-256:
  `900701d0d15596ed2c2fc7a27dc5b130758243460d32ea48a617f41baf690927`
- Call SHA-256:
  `866089232b31b10cde14def18e2833b78f21ad47e7ef02c852cdbe7263787ce2`
- Write SHA-256:
  `c834a7894530277d04cde21a1d3d3d17caf2ee44180f181f8906db8e12874dff`
- Selected-state SHA-256:
  `2079deeea652e94c820715c3f9f4ad805c49a47da281991642c422b3d4d69f8e`

## Upper-chute Gray Blobs

The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
describes seven **Gray Blobs** in the chute at the very top of the table,
awarding 100,000 points each and linking their destruction to the
Three-Blobs safety behavior. The fixed layout contains exactly one matching
seven-record upper-table row:

| Slot | Position | Parameter | Initializer | Live dispatch |
| --- | --- | ---: | --- | --- |
| `$50` | (40, 78) | 0 | `03:6876` | `03:68A1` |
| `$51` | (46, 66) | 1 | `03:6876` | `03:68A1` |
| `$52` | (54, 50) | 2 | `03:6876` | `03:68A1` |
| `$53` | (66, 34) | 2 | `03:6876` | `03:68A1` |
| `$54` | (90, 30) | 2 | `03:6876` | `03:68A1` |
| `$55` | (114, 30) | 2 | `03:6876` | `03:68A1` |
| `$56` | (138, 30) | 2 | `03:6876` | `03:68A1` |

All seven records have type `$83`, state `$41`, and flags `$02`.
Initialization selects appearance 5 and collision selector `$13`, preserves
the original vertical coordinate, installs the movement fields, and enters
`03:68A1`. The live state clears transient actor flags, derives a 64-95-tick
movement interval from RNG output masked with `$1F`, alternates appearance
every four ticks, applies its two-component velocity, and runs the common
collision helper.

The exact hit path at `03:6906` raises the one-frame family signal `$2CFD`,
submits `$03E8` stored score units (100,000 displayed points), creates the
already mapped default command-`$2D` impact burst through `02:41EC`, disables
the source actor, and passes `$FF` to the common collision response. This
signal is a direct cross-family link: the Three-Blobs movement code at
`03:676B` branches to alternate placement `03:67EC` when `$2CFD` is set, and
the following `03:681D` state gates on the same byte. The static link is exact;
the route does not exercise a Gray-Blob hit, so it does not establish the
complete cumulative safety rule dynamically.

The standard census initializes all seven records at frame 872. Every
initializer takes 372 body cycles and 15 scoped writes, installs `03:68A1`,
and preserves its slot and MPR state. The upper chute never becomes
camera-active on this route, so no live dispatch is entered and every one of
the 2,019 selected `$2CFD` writes remains zero. The first initializer has a
normalized `03:6876@1` replay.

`tools/Analyze-GrayBlobSemantics.ps1` guards the complete layout row,
initializer, live movement/RNG/collision loop, hit score and impact result,
the `$2CFD` Three-Blobs coupling, routed initializer population, inactive
signal timeline, and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `75aa6fc29520a22fe188cb81242b65e9a504d36b81756b83f9865e55cf89d8b0`
- Call SHA-256:
  `d8d4381418800d61f2e06b4d458b1f3a9b37d8dc74081e1d6866703c700420e8`
- Write SHA-256:
  `9b5c4d13a3bd3fcaa229ef2b23fa0094c7fa57af9ec5eb898ace6e19ded38193`
- Selected-state SHA-256:
  `7b5c9ebeb29c3784227aa6ccf6c367bb9c5f57a27126777022c1142dfae0ca6a`

The maximum-score gameplay route naturally reaches the upper chute before
entering the bank-7 ending. Visibility activates slots `$50-$56` in order,
and their first live `03:68A1` calls occur across frames 1,541-1,547. Each
call clears the actor's transient flags, enters RNG consumer `03:68B1`, and
stores a movement timer:

| Slot | Frame | RNG result | Stored timer |
| --- | ---: | ---: | ---: |
| `$50` | 1,541 | `$46` | `$46` |
| `$51` | 1,542 | `$10` | `$50` |
| `$52` | 1,544 | `$47` | `$47` |
| `$53` | 1,546 | `$30` | `$50` |
| `$54` | 1,547 | `$F6` | `$56` |
| `$55` | 1,547 | `$52` | `$52` |
| `$56` | 1,547 | `$70` | `$50` |

This dynamically proves `$40 + (A & $1F)` for all seven fixed actors. Each
first live call is a no-interrupt 633-master-cycle scope with 20 ordered
writes. `03:68A1@1` also reproduces through normalized replay.

`tools/Analyze-GrayBlobRngSemantics.ps1` and `dc_rom_gray_blob_rng` guard the
fixed row, live-loop signature, natural activation order, all seven consumer
results and timer writes, all 62 shared RNG recurrences on the route, and
the normalized replay. Debug and Release have distinct framebuffer census
hashes but byte-identical semantic artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `ac639564102a7549bc6aaa58aabb32e3231afafc5a3e3defb8d2a383a07bbdfb` |
| Release census | `97f5089683bb67c53a3755a2d320ce1bcf5ea5e347d65200373a3f22b69cbc4b` |
| Entry | `29002d80e6b56756239b168d2d1786d90a2d5cdf7d7acdb6e0685acc226d767e` |
| Call | `2666ad7f448191cdd63a0df8fccde70c3a9b3609d32557b460a085b8b888ae29` |
| Scoped write | `6903bb8c1f2ceffad57d213e461a3cd29e0282a74800e3a4d9ffd5e0929c99bb` |
| Selected state | `1481a117b3597d134c449033f56012c89f24ab87edc12081e7c3ee74d847e81f` |

## Dragon's Eggs and hatchling

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names **Dragon's Egg** as mechanism 6. A
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/13526)
places the eggs beneath Dragon's Head and describes breaking them to release
bird-like creatures. The ROM layout matches that family exactly: fixed slots
`$20-$25` form a six-piece cluster below Dragon's Head slot `$13`.

| Slot | Position | Initializer | Steady dispatch |
| --- | --- | --- | --- |
| `$20` | (48, 566) | `02:433F` | `02:4345` |
| `$21` | (58, 561) | `02:433F` | `02:4345` |
| `$22` | (68, 556) | `02:433F` | `02:4345` |
| `$23` | (40, 558) | `02:433F` | `02:4345` |
| `$24` | (50, 553) | `02:433F` | `02:4345` |
| `$25` | (60, 548) | `02:433F` | `02:4345` |

All six records have type `$82`, state `$13`, and flags `$02`. Their
initializer increments the shared live-egg counter at `$2CF4`, producing the
sequence 1-6. Invisible fixed slot `$1F`, initialized at `02:42EB` and
updated at `02:42F6`, is the group controller.

The route records one egg hit, on slot `$22` at frame 2,425. Dispatch
`02:4345` submits 10 stored score units (1,000 displayed points), selects
command `$39` for the shared impact-burst family, disables the fixed egg, and
allocates a dynamic type-`$82` hatchling in slot `$64`. The allocator copies
the egg's exact `(68,556)` position and installs dispatch `02:441E`. On the
next frame, transition `02:4379` decrements `$2CF4` from 6 to 5 and leaves
the destroyed egg inactive.

The hatchling is updated five times during frames 2,425-2,429. Its animation
timer runs `$03/$02/$01/$00/$FF`. The frame-2,429 collision submits 200
stored units (20,000 displayed points), selects command `$34` for a second
impact burst, and releases slot `$64` through the generic clear at
`01:C581`.

The route does not destroy the other five eggs, so the controller always
takes its nonzero-count return. Its exact ROM-bounded count-zero branch
increments `$2CFB`, sets bit 0 of `$2D20`, submits 100 stored score units
(10,000 displayed points), and enters a timed reset sequence. These
completion effects are not claimed as dynamically exercised.

The combined bounded trace contains 11,660 calls and 2,481 ordered writes:
one controller initializer, 1,790 controller updates, six egg initializers,
9,857 egg updates, one next-frame egg transition, and five hatchling
updates. Normalized idle replays `02:42F6@1` and `02:4345@1` both occur at
frame 873 with 21 cycles and no writes.

`tools/Analyze-DragonEggSemantics.ps1` guards all seven layout records,
controller/count behavior, fixed-slot populations, the routed hit, score and
command selection, hatchling allocation/position/state, next-frame
transition, animation chronology, child release, inactive group-completion
branch, and both normalized replays. Debug and Release reproduce:

- Entry SHA-256:
  `f2fa919a64354707db835f25299715964c07887380e6057ff05f4f743a49717b`
- Call SHA-256:
  `a1e20d918dd1098feb4d81e301ce5e0f6b4f63d5d087d042cb6330c15a4fc1d5`
- Write SHA-256:
  `a486c285201f6b932f5ad29948475b1e8d2ced44df28b513ee8258fb8958e071`
- Selected-state SHA-256:
  `596c47f8705d721bd0c75b3c41691e8d75c92f466be98b291384624a3e69c3a8`

## Giant Skeleton's Eyeball and egg-completion signal

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
separately labels mechanism 8a **Giant Skeleton** and mechanism 8b **Giant
Skeleton's Eyeball**. The layout makes that relationship exact: category-1
fixed slot `$2F` is at `(225,535)`, one pixel right and 17 pixels below the
Giant Skeleton slot `$30` origin. It has type `$82`, initial state `$08`,
flags `$8B`, initializer `02:4DC8`, and steady dispatch `02:4DD9`.

The initializer clears flag bit 6, `$2D20`, and actor state `$3E2F`. The
steady routine then tests `$2D20` bit 0. With that signal clear it takes a
57-cycle, zero-write return. The signal owner is the Dragon's-Egg controller:
when shared live count `$2CF4` reaches zero, `02:42F6` increments `$2CFB`,
sets `$2D20` bit 0, awards 100 stored score units, and begins the egg reset
sequence. The Eyeball consumes that signal to enter the Giant Skeleton's
timed mouth-opening animation and eventually loops back to its initializer.

The canonical route destroys only one egg, so the completion signal remains
clear. The trace contains one initializer at frame 872 and 1,988 steady
calls through frame 3,600. Of the steady calls, 1,951 are exact 57-cycle
zero-write leaves and 37 include periodic interrupts. Normalized replay
`02:4DD9@1` occurs at frame 873 with the same 57 cycles and no ordered
writes. The activation and reset path is ROM-bounded and is not claimed as
dynamically exercised.

`tools/Analyze-GiantSkeletonEyeballSemantics.ps1` guards the layout identity,
initializer, shared Dragon's-Egg signal producer, full opening/reset
signatures, dormant call population, initial actor writes, inactive latch,
and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `ead3b1b7ce2290a86dac6bf535f9ac70de7534724636e0b4e03c04cdfa985d5c`
- Call SHA-256:
  `5a5b2e1fc0edc68a5930c51a0f18714e6ce1f27541b0357718d0ae64c15b8d6f`
- Write SHA-256:
  `dc8a4ef320c2acaa1ddf0601f0bbaaff69c83898e48da804b72e1243d9ddea4f`
- Selected-state SHA-256:
  `7d5d7067ee92eeb9c25882867ffcb3b6b91824d2dea673444fe32a2517112091`

## Lower-table 3 Mercy Stars

The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
names the **3 Mercy Stars** and places them in front of the lower-table
center hole. It also records their last-ball, under-500,000-point appearance,
1,000-point use award, upper-table transport, and automatic Blob Safety.
Three adjacent layout records match that description:

| Slot | Position | Initializer | Steady dispatch |
| --- | --- | --- | --- |
| `$5F` | (132,660) | `02:4F9F` | `02:4FC1` |
| `$60` | (148,660) | `02:4F9F` | `02:4FC1` |
| `$61` | (164,660) | `02:4F9F` | `02:4FC1` |

All three have type `$84`, state `$43`, and flags `$80`. Their common
initializer checks session eligibility at `$222D`, configures appearance
state, marks the actors dormant, and installs `02:4FC1`. The steady routine
first rejects a non-normal mode through `$2254`, then requires `$2D52` to
equal 1. It selects the active player's score with `$2251` and performs a
three-byte subtraction against stored `$001388`, exactly 5,000 stored units
or 500,000 displayed points. A score at or above the threshold returns;
only a lower score activates the stars.

The collision path submits 10 stored score units (1,000 displayed points),
runs a six-step presentation, and transfers ball control into the table
transition. The final state sets `$2CFD` to 1; the Three-Blobs controller at
`03:65BC` owns the corresponding clear, matching the guide's automatic Blob
Safety description. The triggering star is then released through the generic
actor clear.

The canonical route initializes `$2D52` to 3 at frame 873, so all three stars
remain dormant. Each slot enters `02:4FC1` 2,650 times, for 7,950 steady
calls total. Of those, 7,787 are 54-cycle zero-write leaves and 163 include
periodic interrupts. Normalized replay `02:4FC1@1` occurs at frame 873 with
54 cycles and no ordered writes. Activation and use remain ROM-bounded and
are not claimed as dynamically exercised.

`tools/Analyze-MercyStarSemantics.ps1` guards all three layout records,
initializer and live identities, eligibility/final-ball/score gates,
activation, scoring, transition, safety and release signatures, equal slot
populations, selected initial state, inactive route, and normalized replay.
Debug and Release reproduce:

- Entry SHA-256:
  `1843ecb62f1d3c7575aea3d44e19aadb145a4a7bfe24dfba3b1f1c1b6fd9c291`
- Call SHA-256:
  `18c7eaacc8eb696d68fe37519de54ab04823e3c0edc1854a74644de1cb7528c1`
- Write SHA-256:
  `444d3b49380e4ac58eafc96fac130f6e8a757165fa304347087e988cda444eab`
- Selected-state SHA-256:
  `62528a08094cea8cea98955e821398c75e0477d6e04aa5d2460ccdfccbd17fc6`

## Shared impact-burst effect

Dynamic type `$8F` is not an independent table creature. It is a shared
completion/impact effect allocated by several source actors. This internal
name describes the ROM contract without assigning an unverified visible or
audible name.

The default entry at `02:41EC` clears the position offsets, selects command
`$2D`, and joins the common allocator at `02:420B`. The alternate entry at
`02:41FF` also clears the offsets but preserves the command selected by its
caller. The allocator copies the source actor's position into a free dynamic
slot, installs type `$8F` and dispatch `02:4290`, and initializes the effect
state without awarding score.

The route creates seven instances:

| Source | Spawn entry | Effect slot | Command |
| --- | --- | --- | --- |
| Small Soldier slot `$62` (`03:7F57`) | `02:41EC` | `$70` | `$2D` |
| Small Soldier slot `$69` (`03:7F81`) | `02:41EC` | `$62` | `$2D` |
| Tower insect slot `$64` (`03:6016`) | `02:41EC` | `$71` | `$2D` |
| Dragon's Egg slot `$22` (`02:4345`) | `02:41FF` | `$62` | `$39` |
| Dragon's-Egg hatchling slot `$64` (`02:441E`) | `02:41FF` | `$69` | `$34` |
| Tower insect slot `$6A` (`03:6016`) | `02:41EC` | `$62` | `$2D` |
| Tower insect slot `$6D` (`03:6016`) | `02:41EC` | `$69` | `$2D` |

Each new instance enters `02:4290` exactly once. That activation clears its
pending flag, emits the command held in `$2D6E`, and changes the dispatch to
`02:42B0`. The steady state advances through animation values
`$00-$08/$FF`, then releases the slot through the generic clear at
`01:C581`. The seven observed lifetimes are 35 or 36 frames. The bounded
effect scopes contain no score-accumulator writes; scoring remains owned by
the source mechanism.

The combined trace contains 14 calls and 1,231 ordered writes: five default
spawns, two alternate spawns, and seven activations. The first normalized
sample is `02:4290@1` at frame 1,852 with 4,362 cycles and 114 writes.

The cabinet presentation route now closes the same effect at the copied SAT
boundary. At frame 2,100, the Tower-insect result in dynamic slot `$71` is
type `$8F`, appearance `$10`, phase 5, camera-active, and dispatched through
`02:42B0`. Bank `$08` metadata decodes two contiguous sprite primitives that
match the copied SAT exactly. Their pattern blocks are disjoint from every
known upload, so resolved live VRAM is a stable source. A cloned-cache probe
moves the viewport away without advancing emulation and reproduces the
effect in table-world space (`2355ce88...0dce14`); frames 2,400 and 2,700
contain no retained instance after the original slot clear. The renderer
treats `$4290->$42B0` as one visual identity and keeps all other transient
signatures live-camera-only.

`tools/Analyze-ImpactBurstSemantics.ps1` guards the allocator, activation,
animation/free path, known source signatures, all source/destination pairs,
source dispatch identities, command histogram, score absence, lifetime, and
normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `c5101a7c2ab1ed2a6642bcba29d603f5781d7120e8b41ff0a867699ae547e769`
- Call SHA-256:
  `afe846b641d472fa0de55aa6868f0c877dfb408f4f8e195e6ced7064632b300d`
- Write SHA-256:
  `0ddbb0da9a4af51466a80a7258dcf88500a966d6b2c92404fe7ddbff06241911`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Front Spiders and side safeties

The two fixed actors immediately below the Bumpers are the **Front Spiders**
described by player documentation (the USA manual groups this artwork under
its Skeleton Insect terminology). Layout slots `$26/$27` use initializer
`03:646B`, type `$83`, positions `(64,636)/(216,636)`, and record parameters
0/1 for left/right ownership. Both converge on steady dispatch `03:6490`.

Every update selects collision ID `$2A` or `$2B` from that side parameter and
passes through `03:6966/65A4`. A hit:

1. Clears bit 7 of `$3800,X`.
2. Writes 1 to side-selected `$2D13,Y`: `$2D13` for the left trap-door
   safety or `$2D14` for the right launcher safety.
3. Emits audio command `$34`.
4. Awards 40 stored units (4,000 displayed points).
5. Transitions through `03:64D2/64D5/64DD/64E0/64ED/64F0` before returning
   to `03:6490`.

The route contains 2,645 calls from frames 873-3,551: 1,357 for slot `$26`
and 1,288 for slot `$27`. Three calls hit—right-side frames 2,182 and 2,564,
then left-side frame 3,466—contributing 120 stored score units. The other
2,642 return through `03:64A4` after collision processing. The normalized
first call at frame 873 takes 903 cycles and contains 19 writes.

`tools/Analyze-FrontSpiderSemantics.ps1` guards the two fixed layout records,
side ownership, collision selection, every safety/score/audio write, the
appearance-transition timeline, fixed dispatch identity, and normalized
replay. Debug and Release reproduce:

- Entry SHA-256:
  `e7c5827a2cec93436b944294dfe9b212e383ef03a95a5a3294f0916a2b7a9e47`
- Call SHA-256:
  `74f2ea92d7d9b105b0a2a416d7de871ef8723ed25beca28e7107b0f7f8045a4c`
- Write SHA-256:
  `743457198ef6ddaae820652b016fe0f6ade99e4937d09225f24dee13f2414659`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Side Spiders and side-shoot exits

The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
distinguishes two **Side Spiders** at the outer lower-table side shoots from
the Front Spiders beneath the Bumpers. The adjacent layout identities match
that distinction:

| Slot | Position | Side parameter | Initializer | Steady dispatch |
| --- | --- | ---: | --- | --- |
| `$28` | (40,642) | 0 | `03:64FA` | `03:651F` |
| `$29` | (240,642) | 1 | `03:64FA` | `03:651F` |

Both records have type `$83`, state `$29`, and flags `$82`. Their initializer
preserves the side parameter at `$3C80,X`, installs collision selector `$10`,
completion selector `$8C`, secondary collision state `$80`, and a one-tick
timer, then installs `03:651F`.

The steady routine converts side parameter 0/1 into collision ID `$2C/$2D`
and passes through the shared `03:6966/65A4` collision and appearance helpers.
A collision clears the actor hit latch, writes 1 to shared transition latch
`$2D32`, emits command `$34`, and submits 100 stored score units (10,000
displayed points). It then runs the four-step disappearance/transition
sequence used by this spider family. The award matches the guide's
10,000-point one-hit Side Spider rule.

The canonical route does not collide with either Side Spider. It contains two
initializers at frame 872 and 2,796 steady calls through frame 3,548, split
equally at 1,398 calls per slot. All steady calls return through the no-hit
leaf at `03:6533`; body costs span 336-2,370 cycles and 9-47 ordered writes
as shared collision work and periodic interrupts vary. Normalized replay
`03:651F@1` occurs at frame 873 with 903 cycles and 19 writes. The hit and
transition sequence is ROM-bounded and not claimed as dynamically exercised.

`tools/Analyze-SideSpiderSemantics.ps1` guards both layout records,
initialization and side ownership, collision selectors, hit/score/audio and
transition signatures, shared appearance helper, equal call populations,
initial actor writes, inactive latch/hit path, timing bounds, and normalized
replay. Debug and Release reproduce:

- Entry SHA-256:
  `1b3f785f9b8f7d71822f545bc9a90f2e54a9992fe68aa52d26c09b488779939d`
- Call SHA-256:
  `409c5913a3ad8105a4c16d4c1b29197e35b3a3aee8fada9ceb2d0e8c44e1eab9`
- Write SHA-256:
  `f7b8545de33f6b1fd6c6a597ca2377523a413b7f70502d3460b2d2c792719ea5`
- Selected-state SHA-256:
  `4a72e3b731fe3913181c59c195431fba9e9f4a95aa2499c1b2497afedcbd3cb9`

## Skeleton Insects

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
lists three **Skeleton Insects**: two worth 4,000 points and one worth
10,000 points. Their fixed positions and the route's only three previously
unexplained score awards identify these records:

| Mechanism | Slot | Layout record | Position | Type | Initial / steady dispatch | Source latch | Award |
| --- | --- | --- | --- | --- | --- | --- | ---: |
| Left Skeleton Insect | `$31` | `00 31 A3 54 20 00 66 02 07 C3 82 00` | (32, 614) | `$82` | `02:54A3` / `02:54B9` | Front Spider `$2D13` | 40 |
| Center Skeleton Insect | `$32` | `02 32 D3 55 84 00 B6 02 38 E0 81 00` | (132, 694) | `$81` | `02:55D3` / `02:55F9` | Tower `$2CFC` | 100 |
| Right Skeleton Insect | `$33` | `00 33 43 55 F0 00 81 02 19 8B 82 00` | (240, 641) | `$82` | `02:5543` / `02:5551` | Front Spider `$2D14` | 40 |

Stored score units display at 100 points each. The initializers clear their
source latches: slot `$31` clears `$2D13` and slot `$33` clears `$2D14` at
frame 872; the center setup clears `$2CFC` at frame 941.

The right Front Spider writes 1 to `$2D14` at frame 2,182. The right target's
same-frame `02:5551` call submits 40 units, then advances through `02:5568`
for frames 2,183-2,190 and enters `02:5590`. The Spider writes the latch
again at frame 2,564, but the post-trigger state does not re-award. The Tower
writes 1 to `$2CFC` at frame 3,353; center dispatch `02:55F9` submits 100
units in the same frame, advances through 16 calls to `02:561A`, then
`02:5625`, and reaches `02:5630`. Finally, the left Front Spider writes 1 to
`$2D13` at frame 3,466; `02:54B9` submits 40 units, advances through 12 calls
to `02:54D0`, and reaches `02:5500`.

All three awards use the shared score boundary, accounting for exactly three
writes apiece at `00:F587/F58F/F597/F5A5`. None of the Skeleton-Insect
scopes calls the audio producer at `00:ECFD`: the triggering Front Spider or
Tower owns the same-frame `$34` or `$33` feedback. Exact ROM signatures also
retain inactive secondary collision/award branches in `02:5500` and
`02:5590`; the route reaches those states but does not take either branch.

The capture contains 5,805 calls and 1,316 scoped writes. Normalized first
steady-state replays for `02:54B9`, `02:5551`, and `02:55F9` each take 21
cycles and make no writes. `tools/Analyze-SkeletonInsectSemantics.ps1`
guards all three layout records, initializer clears, latch-to-award coupling,
dispatch timelines, the non-rewarding second right latch, score/audio
ownership, inactive branches, and normalized samples. Debug and Release
reproduce:

- Entry SHA-256:
  `c0fc725d3e4a44094041fe74b900f27f1e896c881d310ea69836cf91d760a031`
- Call SHA-256:
  `d8ef9a904913b93b0edbd8d6b014ffb90a4c5fb70bfa59d27d4aace0a41db8bb`
- Write SHA-256:
  `024c0aebd843222cfea420e4144b2134004d64755927cb21ceddf27dbbd78f86`
- Actor-state SHA-256:
  `c61ab3a4f0c35687c64ff7991f77e12ecf2864d8bb06ec8318a901db3c252d0a`

## Upper-table Sorcerers

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names the upper-table actors **Sorcerers**. The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
calls them Warlocks and identifies the defining behavior: eight emerge from
the lower-right Star, circle the Big Star, take two 1,000-point hits, add
5,000 points on destruction, cycle colors, and regenerate as a group. Those
properties identify fixed slots `$17-$1E` and hidden controller `$16`:

| Ownership | Slots | Initializer | Initial position | Type | Parameter |
| --- | --- | --- | --- | --- | --- |
| Group controller | `$16` | `02:578E` | (0, 158) | `$83` | 0 |
| Eight Sorcerers | `$17-$1E` | `02:57F8` | Off-table staging records | `$82` | 0-7 |

The controller initializes collision selector `$2D0B` to `$28`, live count
`$2D0F` to 0, deployment count `$2D10` to 8, and color state `$2D40` to 0.
All eight children enter `02:5808` at frame 873. That state replaces their
layout coordinates with signed position (-10, 196), retains route indices
0-7, assigns movement vectors from the eight-entry table at `02:5934`, and
increments `$2D0F` from 0 to 8. The controller clears the staging selector at
frame 941 and remains at `02:57AD` through frame 3,600.

Each Sorcerer then follows the same staggered deployment:

| Slot | `02:586F` calls | Arrival-state frames (`02:5886`) | Orbit begins |
| --- | ---: | --- | ---: |
| `$17` | 1 | 942-1,166 | 1,167 |
| `$18` | 45 | 986-1,210 | 1,211 |
| `$19` | 89 | 1,030-1,254 | 1,255 |
| `$1A` | 133 | 1,074-1,298 | 1,299 |
| `$1B` | 177 | 1,118-1,342 | 1,343 |
| `$1C` | 221 | 1,162-1,386 | 1,387 |
| `$1D` | 265 | 1,206-1,430 | 1,431 |
| `$1E` | 309 | 1,250-1,474 | 1,475 |

Every arrival state lasts 225 calls. Its final call installs orbit dispatch
`02:58C2` and decrements `$2D10`; the eight transitions reduce that count
from 8 to 0 at 44-frame intervals. The route then records 18,152 orbit calls
through frame 3,600, split 2,423/2,379/2,335/2,291/2,247/2,203/2,159/2,115
across slots `$17-$1E`.

No Sorcerer is hit on this route: every orbit call returns through `02:5903`,
the live count remains 8, and no Sorcerer score or audio producer executes.
The exact ROM branch remains replacement-relevant. A hit clears the collision
flag, awards 10 stored units (1,000 displayed points), emits command `$29`,
and decrements the two-hit counter at `$3C00,X`. Counter 1 applies the shared
first-hit response. Counter 0 decrements `$2D0F`, adds 50 stored units (5,000
displayed points), invokes group/color and impact helpers, then either
re-enters deployment at `02:5808` or leaves the actor in its completed state.
The controller's zero-live-count branch selects the next group mode and color
cycle before redeploying all eight.

The bounded capture contains 23,859 calls and 346,654 scoped writes.
Normalized first samples cover controller idle `02:57AD` (69 cycles, zero
writes), arrival `02:5886` (762 cycles, 15 writes), and orbit `02:58C2`
(306 cycles, seven writes). `tools/Analyze-SorcererSemantics.ps1` guards the
nine layout records, complete controller/deployment/orbit ROM, route-index
ordering, live/deployment counters, all dispatch timelines, inactive
hit/completion branches, and normalized samples. Debug and Release
reproduce:

- Entry SHA-256:
  `62d81fe4d2f68a22e56661826709bc30ab0c95b63666f16c30bed044d9734261`
- Call SHA-256:
  `1e747ab17db2e5fa1521d1e755672a9da6a44063dccd959f7b11816ba863ad12`
- Write SHA-256:
  `8ef36479191f192a24b5fc3eb79fde344469c02ab99d0ccd49d535d68e09ac99`
- Actor-state SHA-256:
  `5060cead3f36f5e807e1804fcba4b2675455a9d259a8ac455e45f8e814972407`

## Sorcerer zero-live-count RNG path

A targeted route now dynamically closes the Sorcerer controller's RNG
consumer while retaining fixed hidden controller slot `$16` and its authentic
`02:57AD` dispatch. At frame 1,200 it clears only existing live count
`$2D0F`. The controller observes zero, passes delay `$80` to the common timer
helper, and enters the resulting cross-frame continuation.

The first delay expires at frame 1,327 and dispatches `02:57BA/57C2` at frame
1,328. Because the Big Star's signed velocity `$2D00:$2D01` is zero, the
controller calls shared RNG directly. Its low bit selects collision selector
`$28/$29` at `$2D0B`, and the controller reloads deployment count `$2D10=8`.
Two continuation frames clear `$2D0B` and return to `02:57AD`; because the
seeded live count remains zero, the controller begins another delay. This
produces 18 authentic selector events through frame 3,556:

| Result parity | Selector | Routed results |
| --- | --- | --- |
| Even | `$28` | `$EA/$3C/$BC/$E4/$42` |
| Odd | `$29` | `$15/$35/$6B/$B7/$DD/$15/$05/$9F/$B1/$EF/$FF/$11/$A5` |

The first event follows the 128-tick delay; subsequent events repeat on the
controller's 131-frame state cycle. Thirteen consumer scopes contain no
interrupt and take 588 or 600 master cycles according to the selected branch,
with 19 ordered writes. Five retain the same gameplay writes around periodic
interrupts. Normalized replays cover the first zero-live `02:57AD` call and
the first `02:57BA/57C2` selector event.

`tools/Analyze-SorcererRngSemantics.ps1` and `dc_rom_sorcerer_rng` guard the
controller and RNG ROM signatures, complete shared recurrence write order,
18-event cadence, output-to-selector mapping, deployment reloads, selector
clears, pointer cycle, call contracts, and normalized replays. Debug and
Release have distinct framebuffer census hashes but byte-identical semantic
artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `289db077eed3dcf07579096d7a3f0bfefce5b58ea4cc75d78620a15d22780a90` |
| Release census | `fe88d7df827906e0a9f90ec5babd0c53d7d395089f8ab5f40ed4634ca1f98543` |
| Entry | `06e81bc2a61561c00035ba8a053144598139e6995eb7daa13c67088302d3af12` |
| Call | `2cce93e60f805f67d8c0ae93057415eac77d1699bbf28a5e653f79d610a02a87` |
| Scoped write | `745c72bace638162f17788180faaee01938b88297e3181cc2e1200cca2b04166` |
| Selected state | `11b203ce3459e654d7b1c2b2d2c853455adc9fa9093786b80867f6698edfcd59` |

## First dynamic table rule: Tower insects

The audio ownership map identifies dynamic type-`$82` dispatch `03:6016` as
the producer of command `$29`. Its fixed source is slot `$2C` at `(140,526)`,
immediately in front of the Tower; its exact four-child spawn count,
live-instance counter, score schedule, and 20,000-point group-clear award
identify these actors as the manual's insects living in Tower.

The spawner beginning at `02:5F78` uses the generic allocator, assigns type
`$82`, copies position and collision fields from its source actor, initializes
`$3C00,Y` to 2, installs initial dispatch `02:5FDB`, refreshes activation, and
increments the live-instance counter at `$2D2B`. The route creates:

| Slot | Allocation frame | Steady `03:6016` transition | Hits | Result |
| --- | ---: | ---: | --- | --- |
| `$64` | 941 | 1,022 | 2 at frames 2,074/2,078 | Completed and freed |
| `$67` | 989 | 1,070 | 1 at frame 2,931 | Remains with counter 1 |
| `$6A` | 1,037 | 1,118 | 2 at frames 1,875/2,595 | Completed and freed |
| `$6D` | 1,085 | 1,166 | 2 at frames 1,409/2,629 | Completed and freed |

`03:6016` passes `$3780,X` to the collision-table interpreter at `03:6966`.
When the interpreter leaves bit 7 of `$3800,X` clear, the routine returns
without applying the rule. On a hit it:

1. Clears bit 7 of `$3800,X`.
2. Submits a 10-unit award through `01:C02A -> 00:F54F/F55C`.
3. Emits audio command `$29` through `01:C087`.
4. Decrements the two-hit counter at `$3C00,X`.
5. On counter 1, invokes `03:7BF6` and returns with the actor alive.
6. On counter 0, submits an additional 50-unit completion award, invokes the
   completion helpers, decrements `$2D2B`, and tail-calls the generic
   32-array slot clear at `01:C574`.

The seven hits therefore contribute 220 stored score units: seven 10-unit hit
awards plus three 50-unit completion awards. Each hit has exactly one `$29`
command. The three terminal calls clear slots `$64/$6A/$6D` across all 32
parallel actor arrays and leave through `01:C594`; the other 6,491 calls
return through `03:603E`.

The bounded trace contains 6,494 calls across frames 1,023-3,600 and 114,100
scoped writes. Calls take 441-27,474 master cycles and contain 11-545 writes.
The natural first call includes an interrupt whose placement changes under
the restored frame-clock origin, so the regression uses the first short
no-interrupt sample, ordinal 5 at frame 1,027: 441 cycles and 11 writes.

`tools/Analyze-TwoHitTargetSemantics.ps1` guards the spawner and update ROM
signatures, allocation/dispatch timeline, slot population, hit chronology,
score/audio coupling, countdown results, terminal clears, live-instance
counter, and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `7e3e07db7f985011ff1d79972077c1524f7d14324e0b963d36cf99510cedf661`
- Call SHA-256:
  `cb5d44ef5d6aad1cf78af90d34f382ef288dcdedfb31ca186fbe644f86f12059`
- Write SHA-256:
  `7d2174b63f3d9180d124b262345ce1ae5cf7a4072d8520a436aeeefde8dd41ab`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Small-Soldier Side-Star sources

The fixed source controllers for the Small Soldiers are now bounded with
their children. The [USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names the actors Small Soldiers; the
[detailed player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
calls them Skeletal Soldiers and identifies two groups of five generated by
the lower-left and upper-right Side Stars. The fixed layout matches that
description:

| Side-Star source | Position | Initializer | Collision IDs | Group count | Child slots |
| --- | ---: | --- | --- | --- | --- |
| Lower-left slot `$34` | (32, 350) | `03:7D4D` | `$0A/$0B` | `$2D16` | `$62/$65/$68/$6B/$6E` |
| Upper-right slot `$35` | (232, 286) | `03:7DE9` | `$0C/$0D` | `$2D17` | `$63/$66/$69/$6C/$6F` |

Each initializer clears its live counter, sets a five-child pending count,
selects side index 0 or 1, and installs subtype `$88` or `$89`. Delayed states
`03:7D6F/7E0D` call the shared `03:7E87` spawner once per side at frames
873, 988, 1,036, 1,084, and 1,132. The spawner allocates type `$82`, copies
source position/collision/subtype/side identity, sets the two-hit counter to
2, installs initial dispatch `03:7EFA`, refreshes the child, and increments
the side-selected count. Both counts consequently rise from 0 to 5. The
sources then enter steady wait states `03:7D85/7E23`.

Both child completion forms converge on `03:7F81`. Slot `$62` takes its
non-`$0F` exit immediately at frame 1,852. Slot `$69` enters with subtype
`$0F`, rewrites the holding dispatch on eight calls across frames
1,959-1,969, and takes the exit on a second frame-1,969 entry after its
animation state changes. Each exit adds 50 stored units, creates the shared
impact effect, decrements the appropriate group counter, and clears all 32
parallel arrays for the child. The selected-state trace records `$2D16=4`
at frame 1,852 and `$2D17=4` at frame 1,970.

Neither group reaches zero on this route. Exact source ROM signatures retain
the inactive all-five rule: award 200 stored units (20,000 displayed points),
advance the Side-Star appearance modulo four, apply its helper/animation
state, and regenerate the group.

The bounded capture contains 5,322 calls, 3,324 scoped writes, and 3,888
selected-state writes. It normalizes steady samples `03:7D85@1` and
`03:7E23@1`, the first spawn `03:7E87@1`, and the immediate completion
`03:7F81@1`. `tools/Analyze-SmallSoldierSourceSemantics.ps1` guards both
layout records, complete source/spawner/completion ROM blocks, initialization,
allocation pairing, group counts, dispatch transitions, subtype holding,
awards, decrements, exact frees, and the inactive all-five path. Debug and
Release reproduce:

- Entry SHA-256:
  `c186bd9b395371e22a41e66f832561bd21c0632f349cdf44b00cf7a9b7bcd707`
- Call SHA-256:
  `575c226bcdfbbae6ea4f889e9b4cbac75f25e7d508e210776bb92d550e5b61f4`
- Write SHA-256:
  `336a25613adff3f64c608761a9cf6c1cea74053704cf6625067d9d1bb41ee774`
- Selected-state SHA-256:
  `ab06bfa930972c7aca88244ce26bdf22b73eb0cf915cb6b5f9e2a085d1ee8af8`

## Branching two-hit table rule: Small Soldiers

The second audio-owned dynamic family uses type `$82`, initial dispatch
`03:7EFA`, steady dispatch `03:7F57`, and audio command `$21`. Its spawner at
`03:7E87` initializes `$3C00,Y` to 2, copies a subtype through `$3D00,Y`,
and increments a group counter selected by `$3480,X` at `$2D16,Y`.
The manual calls this ten-actor, two-hit family the **Small Soldiers**;
player documentation also calls them Skeletal Soldiers. The observed
3,000-point hits and 5,000-point standard completion match that identity.

Ten actors are created in five pairs:

| Allocation frame | Slots | Steady-state frames |
| ---: | --- | --- |
| 873 | `$62/$63` | 1,053/1,085 |
| 988 | `$65/$66` | 1,101/1,133 |
| 1,036 | `$68/$69` | 1,149/1,181 |
| 1,084 | `$6B/$6C` | 1,197/1,229 |
| 1,132 | `$6E/$6F` | 1,245/1,277 |

`03:7F57` performs collision processing through `03:6966` and `03:7FB1`.
Its no-hit path returns immediately. Every hit clears bit 7 of `$3800,X`,
submits a 30-unit score award, emits command `$21`, and decrements the
two-hit counter. The observed hits are:

| Slot | Hit frames | Counter results | Second-hit result |
| --- | --- | --- | --- |
| `$62` | 1,479/1,852 | 1/0 | Standard completion: add 50, decrement group count, clear all 32 arrays, and free |
| `$63` | 1,832 | 1 | Remains active |
| `$69` | 1,955/1,959 | 1/0 | Subtype `$0F`: hold at `03:7F81`, then add 50 and free after the animation-state change |

The subtype branch is selected by `($3D00,X & $0F) == $0F`. Slot `$69`
rewrites its holding pointer at frames 1,959-1,965 and 1,969. A second
frame-1,969 call sees the changed animation state, adds the deferred 50-unit
completion award, decrements `$2D17` to 4, and clears slot `$69`. The standard
branch clears slot `$62` through `01:C574` and reduces `$2D16` to 4. Across
both captures, the family contributes 250 stored score units: five 30-unit
hit awards plus two 50-unit completion awards.

The bounded trace contains 20,867 calls from frames 1,054-3,600 and 362,060
scoped writes. Of these, 20,862 take the no-hit path. Calls range from
417-38,517 master cycles and 11-523 writes. The first call, slot `$62` at
frame 1,054, is a normalized replay sample with 417 cycles and 11 writes.

`tools/Analyze-BranchingTwoHitSemantics.ps1` verifies the spawner/update ROM
signatures, all ten allocation and dispatch identities, slot populations,
five hit sequences, six nested score awards, command coupling, standard free,
subtype holding transition, and normalized replay. Debug and Release
reproduce:

- Entry SHA-256:
  `c4d55adb49df61c3ab64250c1429e3bb07c0eef92d8b2c3290e1f9e5947d8f4f`
- Call SHA-256:
  `3ab64128aab89d056216f79d0d3f041e186eb109048aefe82785e2875f41ec4f`
- Write SHA-256:
  `3b2eb1b5dbd54c1b08cf6a509820e47c2320faf544b4d15824f5426f414c6544`
- Actor identity SHA-256:
  `b311b7704f05b9bca07da08e71026f9415d4752d044361d026e816c9cafdc338`

## Giant Face

The [USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names the central mechanism the **Giant Face**, states that seven level
changes open its mouth, and identifies the crown as a 300-point surface. The
[detailed player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
describes the same center-screen Dragon/Woman Head progressing from an
inactive shadow through Woman/Dragon forms before its open mouth enters bonus
stage 4. Four adjacent fixed records implement that family:

| Role | Slot | Position | Initializer | Type | Steady dispatch |
| --- | --- | ---: | --- | --- | --- |
| Persisted form/request controller | `$36` | (128, 292) | `03:7428` | `$83` | `03:74D8` |
| Hidden form animator | `$37` | (0, 292) | `03:766C` | `$83` | `03:767E` |
| Mouth and bonus entrance | `$38` | (144, 350) | `03:7718` | `$82` | `03:7726` |
| Crown/helmet collision surface | `$39` | (120, 286) | `03:76F7` | `$82` | `03:76FA` |

Controller slot `$36` advances through `03:7428`, `742B`, and `742E`, then
restores persisted form `$2258` and installs `03:74D8`. An out-of-range or
zero form clears `$2258`, request flags `$2D0C`, display form `$2D0D`,
animation mode `$2D0E`, and the slot-local form field. The routed game starts
in form 0: the selected-state trace records those clears at frame 941, and
all 2,648 `74D8` calls through frame 3,600 take the request-free return.

The ROM-bounded active state machine assigns `$2D0C` bit 0 to the wake
request. Its four-step animation stores persisted form 1, selects animator
mode 1, and clears that request. Bit 1 dispatches six later transformations
through vector entries at `03:760C` and persisted-form table values
`2,3,4,5,5,7` at `03:7618`. One late handler explicitly stores form 6 and
sets mouth-open latch `$2D21=1`; the final handler clears animation mode and
resets the controller. Hidden slot `$37` animates bounded form ranges for
`$2D0E` modes 1 and 2.

Slots `$38/$39` receive no routed collision. Exact ROM signatures retain the
inactive rules:

- A crown/helmet or closed-mouth hit clears bit 7 of `$3800,X`, creates the
  shared Giant-Face visual effect, emits command `$20`, and awards 3 stored
  units (300 displayed points).
- When `$2D21` marks the mouth open, slot `$38` becomes type `$84`. Capturing
  the ball sets bit 7 of `$2D21`, awards 100 units (10,000 displayed points),
  positions the ball at `(144,344)`, emits command `$41`, and selects bonus
  stage 4.

The bounded trace contains 5,608 calls and 812 scoped writes. The only ten
Giant-Face-owned writes are initializer/restoration writes; all later writes
inside these no-op scopes belong to interrupts. It normalizes controller wait
`03:74D8@1`, animator wait `03:767E@1`, crown no-hit `03:76FA@1`, and mouth
no-hit `03:7726@1`.

`tools/Analyze-GiantFaceSemantics.ps1` guards all four layout records, the
persisted-form reconstruction, request dispatcher, six transformation
vectors/forms, visual helper, animator, both 300-point collision rules, the
open-mouth award/ball placement/bonus-stage selection, routed call
populations, inactive state, and normalized samples. Debug and Release
reproduce:

- Entry SHA-256:
  `e00d2caf31a341328834cebb8deadaf3fa456f06184e415d45806acaf37d64dd`
- Call SHA-256:
  `357c7cc9550d7dd5c4804cd1cb76ff62fec6f4db8621b8b3e36c383c369ba3d7`
- Write SHA-256:
  `fc422de12e8b181f7456cb3d3d584a321be62eefeb73398bc2555ad3632f7c48`
- Selected-state SHA-256:
  `804e0ddee017e941be7cb222c24f4922fb1064b255d1d03d5fdd22c515925946`

### Giant-Face helmet activation channels

The detailed guide identifies two openings at the top of the Giant Face
helmet and states that the ball is launched back out through the side it
entered. Fixed slots `$2A/$2B` match those mirrored positions and close the
activation side of the family:

| Channel | Slot | Position | Initializer | Selector | Type | Steady dispatch |
| --- | --- | ---: | --- | --- | --- | --- |
| Left helmet opening | `$2A` | (134, 292) | `02:4A5C` | `$13` | `$84` | `02:4C91` |
| Right helmet opening | `$2B` | (158, 292) | `02:4A64` | `$1D` | `$84` | `02:4C91` |

Both initializers jump into shared setup `02:4C78`, install original-state
marker `4C78`, and start with timer 1. Their no-hit state decrements a
four-tick timer and cycles appearance `$3980,X` through 0-3. The route
contains 1,748 steady calls, split evenly between the two slots. It records
1,748 timer decrements, 438 timer reloads and appearance increments, and 146
appearance resets. No helmet collision occurs.

The exact inactive collision path first evaluates current score and the
`$2D16/$2D17/$2D0D` special-condition state, then clears transformation
request bit 1 and sets wake bit 0 in `$2D0C`. The ensuing channel animation
opens the actor, emits command `$20`, creates the shared visual response, and
awards 3 stored units (300 displayed points). It then emits command `$25`,
passes the channel selector through the ball-routing helper, and restores
the original steady dispatch. The request is consumed by Giant-Face
controller `03:74D8`, bounded above.

The capture contains 1,752 calls, 3,558 scoped writes, and 3,934 selected
state writes. `tools/Analyze-GiantFaceHelmetSemantics.ps1` guards both layout
records, selectors, shared setup, request/special-condition logic, idle
timer/appearance populations, activation award/audio/ball-return sequence,
and normalized `02:4C91@1`. Debug and Release reproduce:

- Entry SHA-256:
  `fb473f9da4c43e0eea2abcfed06c5510b3d2c6a457bcbd6e791c5b91be6cb057`
- Call SHA-256:
  `48a1a89f272e80679e859d6ecdc34d4102d864985fba5ba64bdde12d0642e64b`
- Write SHA-256:
  `1d70d3313ad65639066692b2721f1df8d5ffc4d041e5625a3f84fceaf3dcd4fa`
- Selected-state SHA-256:
  `6692ff8cb8ddb9927b57e2844dea70ea8c11afc48a3ac18707350690b83c5d26`

## Upper-table Four-Star Wall Pockets

The
[USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf)
names four upper-table bonus entrances **Wall Pockets** and assigns them
BS-1, BS-3, BS-5, and BS-6. The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
calls the same mechanisms the **4 Stars**: two sit outside the upper-table
flippers and two sit behind opening hands at the top. Their four fixed
records, stage fields, and zero-based Roman selectors agree exactly:

| Pocket | Slot | Position | Initializer | Ball route | BS ID | Selector |
| --- | --- | ---: | --- | --- | --- | --- |
| Lower left | `$3E` | (80, 214) | `02:49FC` | `$1C` | 1 | `$05` (VI) |
| Lower right | `$3F` | (254, 214) | `02:4A0E` | `$14` | 5 | `$03` (IV) |
| Upper left | `$42` | (86, 76) | `02:4A20` | `$03` | 3 | `$07` (VIII) |
| Upper right | `$43` | (246, 76) | `02:4A32` | `$0A` | 6 | `$01` (II) |

The initializers store the ball-route value at `$3C80,X`, bonus-stage ID at
`$3700,X`, and Roman selector at `$3780,X`, then enter common setup
`02:4A6C`. Setup installs timer 1, original-state marker `4A6C`, and steady
dispatch `02:4A93`. The no-entry path uses the same four-tick,
four-appearance idle cycle as the Giant-Face helmet channels.

The neighboring Big-Star controller owns signed rotation word
`$2D00:$2D01` and stopped-arrow selection `$2D18`. A pocket entry at
`02:4A93` has two replacement-relevant outcomes:

- While the Big Star is rotating or its stopped arrow does not match the
  pocket selector, the normal path advances Sorcerer color `$2D40` modulo
  eight, awards 10 stored units (1,000 displayed points), runs the common
  opening animation, emits command `$25`, and returns the ball through the
  pocket's route value.
- When rotation is zero and the stopped arrow matches, the special path
  clears transition latch `$2CFA`, emits command `$41`, awards 100 stored
  units (10,000 displayed points), passes the pocket's BS ID through `$55`
  to the shared scene-transition helper, then converges on the opening and
  ball-routing sequence.

The standard census enters no pocket. All four records initialize at frame
872. The lower pair later becomes camera-active and contributes 503
`02:4A93` calls per slot; the upper pair contributes only its initializer
call. Across 1,008 steady calls, the trace records 1,008 timer decrements,
254 timer reloads/appearance advances, and 84 appearance resets. It records
neither the normal `$2D40` write nor the selected `$2CFA` write.

The bounded capture contains 1,016 calls, 2,036 scoped writes, and 3,951
selected-state writes. `tools/Analyze-FourStarWallPocketSemantics.ps1`
guards all four records, stage/selector/route assignments, Big-Star
selection state, both award branches, Sorcerer-color coupling, idle
populations, inactive access, and normalized `02:4A93@1`. Debug and Release
reproduce:

- Entry SHA-256:
  `eafb2abd72e818a258eee861904f0abb097cb1703c885a66316b08a318fad3d9`
- Call SHA-256:
  `2f86b84a3997f8a84239a554ecb34523fc94c564f337ff446b8df61f91b1d64b`
- Write SHA-256:
  `d4fe55cf0b4b975eecd160accace5b3f0e753bf05b99b2e6a5c7b7d375f256b8`
- Selected-state SHA-256:
  `6bb4604bcffd88960c62c76ae98bc5a11984a4711bef456da014b3d31cf1de4e`

## Upper Wall-Pocket hands

The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
says the upper two of the four Stars sit behind hands that open and close to
admit or block the ball. Fixed slots `$40/$41` are the only mirrored records
directly above upper Wall-Pocket slots `$42/$43`:

| Hand | Slot | Position | Initial appearance | Initializer | Live dispatch |
| --- | --- | --- | ---: | --- | --- |
| Upper left | `$40` | (72, 55) | `$14` | `02:5972` | `02:5975` |
| Upper right | `$41` | (232, 55) | `$15` | `02:5972` | `02:5975` |

Both records have type `$82` and flags `$C2`. The distinct appearance values
preserve left/right handedness. Initialization installs `02:5975`; the live
scheduler hides the collision surface, masks RNG output with `$3F`, adds
shared byte `$0080`, and waits on the resulting 16-bit timer.

After the timer expires, shared region byte `$2D33` decides whether the hand
may continue. The upper Wall-Pocket initializers use selectors VII/II, and
their common setup at `02:4A6C` clears `$2D33`. With that upper-pair state
selected, a hand:

1. Clears its hidden flag and collision marker.
2. Changes type `$82→$83`.
3. Passes response selector 4 to the common ball helper.
4. Waits while the collision marker remains set.
5. Restores type `$82` and runs a two-step appearance pass.
6. Passes `$B0 + (RNG & $3F)` to the common ball-response helper.
7. Runs the reverse pass and returns to `02:5975`.

The state machine contains no score-entry or audio-command call. It is a
physical access gate for the neighboring pocket rather than an independently
scoring target.

The standard census initializes both hands at frame 872. Each initializer
takes 192 body cycles, performs seven scoped writes, and installs `02:5975`.
The route never camera-dispatches that live state, so the collision and
opening sequence remain exact-ROM-bounded rather than dynamically exercised.
The selected-state trace records only zero values for `$2D33`: its startup
clear and the two upper-pocket setup clears. The first initializer has a
normalized `02:5972@1` replay.

`tools/Analyze-UpperWallPocketHandSemantics.ps1` guards both hand and pocket
records, left/right appearance identity, the delayed live scheduler,
`$2D33` gate, temporary collision surface, two animation passes, random
ball-response range, score/audio absence, routed initialization, inactive
live dispatch, and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `defcf166fa1b1bf77b4e415f57d082f06f60d73d8f73afbbad036b46713062d9`
- Call SHA-256:
  `98595fe928d45a59437f77f804b662a67f1860c2358c649c109b48d2d420ed92`
- Write SHA-256:
  `1f7ce0a73fc4e1c5c9dcc3953563ea48def6dfa37b482d9e827a57cbe23f3490`
- Selected-state SHA-256:
  `e4521a0c6afa7d98deba96c4f21032e2ff7e702629371a2ac07b9c097bd3dc0b`

A targeted activation route now moves only slot `$40`'s Y coordinate from
55 to 300 immediately before the existing camera sweep. The actor retains
its fixed identity, live pointer, appearance, collision fields, and all
other state. Visibility helper `00:F54B` marks it camera-active at frame
1,220, and the original dispatcher enters `02:5975` at frame 1,221.

That first live call dynamically closes RNG consumer `02:597D`. Shared
`00:E8B3` returns `$73`; the consumer masks it to `$33`, adds the timer base,
stores `$3B40:$3BC0=$0133`, and installs countdown continuation `02:5991`.
The countdown starts at `$0132` on frame 1,222, borrows the high byte at
frame 1,273, and reaches `$0008` when the actor leaves the active window at
frame 1,520. The route does not force a collision or opening transition.

The capture contains 22 shared RNG calls. Twenty complete in 282 master
cycles with eight writes. Calls at frames 1,285 and 1,297 contain periodic
interrupts, taking 939/1,029 cycles and 20/22 scoped writes while preserving
the same eight RNG-owned writes. `tools/Analyze-UpperWallPocketHandRngSemantics.ps1`
and `dc_rom_upper_wall_pocket_hand_rng` guard activation, live/consumer call
contracts, all RNG recurrences, timer construction, continuation, and
countdown. Debug and Release have distinct framebuffer census hashes but
byte-identical semantic artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `fc0c5028d79ae69d7b231f9ccbdd4fbf10caa95a626f7ce541f3c5c585b93145` |
| Release census | `652969878cbd668f6eeaf29bcea3d1fb0aed793583b325f1261f0dc25ab81546` |
| Entry | `4ed2526ec7db17332f1719d8ab6d22451c49dbb15cb6492e42a049a685a60112` |
| Call | `c747286dc2d5800e454ad9bd54087700c8ee4d16c661bc686bf9acec957a0432` |
| Scoped write | `932562de125daad1e0d97ad4dab85e8b07a259ceec228174b34f7453e2ff48ff` |
| Selected state | `664cdeb5e46aa4e1f8e2986f9cd915d9dca00456a8c268546e897abe52ebcba5` |

A second targeted route keeps the same fixed slot `$40` and camera
activation but replaces its installed dispatch pointer with the exact
post-close continuation `02:59F5`. The original dispatcher enters that
state at frame 1,221. Shared RNG result `$73` is masked to `$33`, offset by
`$B0`, and passed to the common response helper as `$E3`.

Because upper-pair selector `$2D33` is zero, the ROM takes both continuation
advances and installs `02:5A00`. Its shared delay begins at `$E3`, counts
through zero at frame 1,447, completes the reverse animation, and naturally
returns to the ordinary `02:597D` delayed path at frame 1,450. That later
call receives result `$72` and installs timer `$0132`. The route therefore
dynamically closes the hand family's second RNG consumer while retaining its
full return cycle.

`tools/Analyze-UpperWallPocketHandResponseRngSemantics.ps1` and
`dc_rom_upper_wall_pocket_hand_response_rng` guard the fixed actor identity,
post-close signature, activation, complete response call, all 23 shared RNG
recurrences, response transform, continuation pointer, delay countdown, and
return to the ordinary timer path. Debug and Release have distinct
framebuffer census hashes but byte-identical semantic artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `f05eece598fa412b212a9bfb85b380c3239bb0e87b0906b7970b6ea0591fbb02` |
| Release census | `911b75168bfe006c674b96dfccfef3d181f85f4d969cd310567639cc37f2cb26` |
| Entry | `520517756add22e0fedb75396a643e5ac8719ef15f49df979e4e061cbacfd6ce` |
| Call | `7c6e516e4b053b490d59b4c16c1b84f33acd539e7f748e0294cdac9676f8a5e1` |
| Scoped write | `3be62639e568238b02878718fd0c5396a21c04b0fadd40327414bee0a8ad7932` |
| Selected state | `678b920696aeb42bab4cfd463a371fe1d212e54f1e9845606ba2f1769572e5a8` |

## Little Skulls and the Big-Star feature network

The detailed
[player guide](https://gamefaqs.gamespot.com/tg16/588942-devils-crush/faqs/51483)
places two pairs of **Little Skulls** at the top of screen 3, says each
one-hit target awards 2,000 points, and links their destruction to the speed,
direction, and stopped Roman-numeral arrow of the central **Big Star**. The
fixed layout matches that composite mechanism:

| Role | Slot(s) | Position(s) | Initializer | Parameter |
| --- | --- | --- | --- | ---: |
| Hidden transition controller | `$44` | (0, -2) | `02:4455` | 0 |
| Left-pair controller | `$45` | (0, 62) | `02:4485` | 0 |
| Left Little Skulls | `$46/$47` | (108/124, 62) | `02:44E4` | 0 |
| Right-pair controller | `$48` | (0, 62) | `02:4485` | 1 |
| Right Little Skulls | `$49/$4A` | (194/210, 62) | `02:44E4` | 1 |
| Big Star | `$4B` | (144, 126) | `02:565C` | 0 |

Each pair controller owns its live count in `$2CF5,Y` and regeneration flag
in `$2CF8,Y`. The four Little-Skull initializers build counts 0→1→2 for
each pair. A collision submits `$14` stored score units (2,000 displayed
points), creates the default command-`$2D` impact burst, disables the source
actor, and decrements its pair count. A zero count causes the controller to
raise the corresponding bit in `$2D04`, run its reset timer, and mark the
pair for reactivation. While shared latch `$2CFA` is set, the skulls use
their alternate gray/inactive state; when it clears, they reactivate and a
binary RNG result selects one of the two Big-Star direction bits.

Big-Star slot `$4B` initializes pending bit `$2D04.0`, unselected arrow value
`$80` in `$2D18`, stopped angular velocity `$2D00:$2D01=0`, and speed-state
index 3. Consumer `02:570A` applies the pending Little-Skull bits to the
seven-state signed velocity table:

| Speed state | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Angular velocity | +64 | +32 | +16 | 0 | -16 | -32 | -64 |
| Reload duration | 3 | 6 | 8 | 0 | 8 | 6 | 3 |

The live rotation state advances the arrow modulo eight and writes its
zero-based direction to `$2D18`. Stopping leaves that selector stable.
Together with the guide's Roman-numeral key, exact consumer comparisons now
close all eight directions:

| Selector | Roman | Consumer | Fixed slot / result |
| ---: | --- | --- | --- |
| 0 | I | Undead Head | `$4D`, `(152,65)`, `03:61A0` |
| 1 | II | Upper-right Wall Pocket | `$43`, BS-6 |
| 2 | III | Right Side Worm | `$4F`, `(256,94)`, `$3700=3` |
| 3 | IV | Lower-right Wall Pocket | `$3F`, BS-5 |
| 4 | V | Big-Star special condition | Warlock/session-state branch |
| 5 | VI | Lower-left Wall Pocket | `$3E`, BS-1 |
| 6 | VII | Left Side Worm | `$4E`, `(32,94)`, `$3700=2` |
| 7 | VIII | Upper-left Wall Pocket | `$42`, BS-3 |

The Wall Pockets compare their per-record selector at `02:4AB3`; both Side
Worms install selectors 6/2 and share the stopped-selector comparison at
`02:5CCD`; the Undead Head compares selector 0 at `03:61AB`; and the Big
Star's stopped-V branch compares 4 at `02:56BA`. This is a complete static
selector registry. It does not claim the guide's stage assignments beyond
the exact per-record fields and already mapped Wall-Pocket results.

The standard census initializes the hidden controller, both pair
controllers, four Little Skulls, Big Star, Undead Head, and both Side Worms
at frame 872. The feature actors never become camera-active. Hidden slot
`$44` contributes 2,650 `02:445B` calls: 2,622 are 21-cycle zero-write leaves
with `$2CFA=0`, and 28 contain periodic interrupts. No bounded call exercises
a Little-Skull hit, impact allocation, selector activation, or score award.
The normalized `02:445B@1` sample occurs at frame 873.

`tools/Analyze-BigStarFeatureNetworkSemantics.ps1` guards all eleven fixed
initializers, pair counts and regeneration, Little-Skull scoring/impact
result, alternate gray state, Big-Star direction inputs, signed speed and
duration tables, arrow selection, all eight feature consumers, routed idle
population, inactive outcomes, and normalized replay. Debug and Release
reproduce:

- Entry SHA-256:
  `09eab95b760dce5e1113ff412a35f4e554d1f8c83008dd2b8688f5634af6d3e6`
- Call SHA-256:
  `7677de9c364d382d657efbea5d2272322f50ba42c8253948efe6bb5b3acd6f36`
- Write SHA-256:
  `4be8e9c06b82726a381b316d6eee5f7f5a6b725e75b38c7770e0c17f4c432ce0`
- Selected-state SHA-256:
  `99f8a2f2ba146a08abc02fb2ebcc52f1f5fe0504636628cca249c95616776627`

## Little-Skull gray-release RNG path

A targeted route now dynamically closes the Little-Skull family's binary RNG
consumer while retaining fixed left-pair actor slot `$46`. At frame 1,200 it
moves only that actor's Y coordinate into the active camera window and selects
the exact gray-state initializer `02:458A`. The original camera sweep activates
the actor at frame 1,220 and its dispatcher enters `02:458A` at frame 1,221.

The gray initializer restores the layout Y coordinate `$0038`, so the next
visibility sweep removes the actor from the camera window. Reapplying only the
targeted Y coordinate at frame 1,222 lets the original sweep reactivate it and
dispatch the initializer-installed `02:45A4` continuation at frame 1,223.
That continuation:

1. Clears the gray/collision flags and restores live selector `$03`.
2. Loads the four-tick live timer.
3. Raises the left-pair pending bit at `$2D04`.
4. Enters RNG consumer `02:45C9`.
5. Uses result `$75` bit 0 to select pending value 2.
6. Returns through ordinary Little-Skull continuation `02:4515`.

The complete `02:45A4` scope takes 768 master cycles with 23 ordered writes;
the nested consumer scope takes 606 cycles with 18 writes. Both contain no
interrupt and reproduce through normalized replays. All 128 shared RNG calls
on the longer route preserve the eight direct recurrence writes; 114 are
282-cycle leaves and the other 14 retain the same direct writes around
periodic interrupts.

`tools/Analyze-LittleSkullRngSemantics.ps1` and
`dc_rom_little_skull_rng` guard the fixed actor identity, gray-release ROM
signature, activation/reintroduction ordering, target RNG result, pending-bit
selection, full RNG write order, call contracts, and normalized replays.
Debug and Release have distinct framebuffer census hashes but byte-identical
semantic artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `cf4c90d01c8bddc8109f0d48503f0a0713e3f7ea71f19c286f8091d0e1ff709a` |
| Release census | `7af61a496eb4584381b91dd56a611732361bb115e3c1cbc6110cbdcc5664e0a8` |
| Entry | `5d82dc538f803bcbca0d57f15dc8a4987ee77a9fb88236bc28b08e85a9d34a6d` |
| Call | `943a683be718416d5e6198497304494c3bb226f9a47d43b43faf814fbf1f4660` |
| Scoped write | `3c130ad07bd06f3e7bdef0f4847101963fd0493955acaae71316c076145e30bf` |
| Selected state | `9de9b281b871dfc270635d6c3aa9b017bbd1f97b146a0f7c6c00d7d335216290` |

## Undead-Head post-trigger selector RNG path

The feature-network map identifies fixed slot `$4D` at `(152,65)` as the
Undead Head and Roman-numeral selector I. Its ROM state immediately before
`03:6248` marks the ball, sets session-transition bit `$53.4`, selects bonus
stage 2 in `$55`, emits command `$32`, and installs `03:6248`. A targeted
route retains that fixed actor, moves only its Y coordinate into the active
camera window, and starts at the exact post-trigger continuation.

The original camera sweep activates slot `$4D` at frame 1,220 and its
dispatcher enters `03:6248` at frame 1,221. That state:

1. Sets shared actor marker `$3C80.0`.
2. Sets ball-slot flag `$3080.5`.
3. Calls shared RNG at consumer `03:625D`.
4. Maps result `$73` bit 0 to response value `$09` at `$3CCD`.
5. Stores continuation `$6279` in `$3E4D:$3ECD`.
6. Enters the shared response state at `$4D2D`.

The complete continuation takes 903 master cycles and 29 ordered writes; the
consumer scope takes 810 cycles and 26 writes. Neither contains an interrupt,
and both reproduce through normalized replay. All 150 shared RNG calls on
the longer route retain the exact eight direct recurrence writes; 140 are
282-cycle leaves and ten retain those writes around periodic interrupts.

The sibling launch-vector consumer `03:6177` is dynamically bounded through
fixed slot `$4D` by the combined final-consumer route. Result `$70` selects
vector `$17` and signed ball velocity `(-12,-62)`. Its 5,778-cycle outer
scope and 975-cycle RNG scope contain a periodic interrupt; the RNG-owned
writes remain exact, but normalized replay detects shifted interrupt-owned
writes. The consumer therefore remains outside no-interrupt replacement
coverage.

`tools/Analyze-UndeadHeadSelectorRngSemantics.ps1` and
`dc_rom_undead_head_selector_rng` guard the fixed identity, stage-2
predecessor, post-trigger ROM, activation, RNG output transform, response
fields, continuation handoff, complete recurrence write order, call
contracts, and normalized replays. Debug and Release have distinct
framebuffer census hashes but byte-identical semantic artifacts:

| Artifact | SHA-256 |
| --- | --- |
| Debug census | `02311387bcb1475c3b21aa53e68d961a2f7240de5d662abf687150b6250c479d` |
| Release census | `6bbb0d9dff3adc62427eba41e04f42ff7d7cd3442974c4a43f006def7abefaac` |
| Entry | `3ec5b243994df4fa327d815f3ef9674c87e4d96c9d6a809a99dd6de3c682cf14` |
| Call | `4678472d2eaea7be13f447d7510feddb745f0464667e5d5807fabf7f2a9e0152` |
| Scoped write | `7b6ce462d254f75bb02160c228926ebeca1a9cd8e29b4a7ce0e86a6d8c296cb8` |
| Selected state | `562c11ecc766cf832f6a00cf9aff1e384963addcad8234098c086f5ccd3a7d3d` |

## Fixed system and presentation controllers

The final three hidden normal-table records are system glue rather than
independent, gameplay-facing table mechanisms:

| Slot | Layout initializer | Steady dispatch | Bounded role |
| --- | --- | --- | --- |
| `$5C` | `03:7B1F` | `03:7B27` | Triple banked-call scheduler |
| `$5D` | `03:7B6D` | `03:7B7A` | Three-marker presentation animator |
| `$5E` | `03:7CA7` | `03:7CAF` | Collision/scene-response controller |

All three are hidden type-`$80` records at layout position `(0,-2)`.
Initializers run consecutively in frame 872 and install their steady
dispatches through the common transition helper.

Slots `$5C` and `$5D` share the same presentation gates: a negative scene
state `$2CC0` takes the generic inactive path, and nonzero `$2BB9` suppresses
updates. Once enabled, each controller advances on an eight-frame timer.
Slot `$5C` submits selectors `$02`, `$03`, and `$07` to exported entry
`08:4030` through the common `00:E241` banked-call wrapper. `C060` is the
wrapper thunk and is not the accepted audio-command boundary.

Slot `$5D` advances its phase modulo eight and renders three descriptors at
`$1E16`, `$1E18`, and `$1E1A`, with object selectors `$04`, `$09`, and
`$0A`. Helper `03:7BE0` maps the phase through the symmetric presentation
table:

| Phase | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Marker value | 0 | 1 | 2 | 3 | 4 | 3 | 2 | 1 |

Slot `$5E` clears shared response byte `$2D38` and zero-page collision
scratch `$CD` during initialization. Its steady state returns while scratch
`$CC` is zero. The statically bounded nonzero branch queries the scene,
submits command `$08` through the separate accepted-audio thunk `C087`,
updates `$2D38` from collision state, applies ball-response selector `$78`,
and runs a 16:14 countdown before resetting. The standard census never
enters that branch: `$2D38` has only its startup and initializer zero writes.
The controller is therefore retained under a conservative compatibility
role rather than assigned an unsupported visible-mechanism name.

The route records one call to each initializer and 2,650 calls to each
steady state. There are 332 active scheduler ticks and 332 marker updates;
the first normalized samples at frame 873 are 48, 48, and 45 body cycles
with zero writes. `tools/Analyze-FixedSystemControllerSemantics.ps1` guards
the three fixed records, exact code paths, wrapper identities, dispatch
installation, cadence, selectors, descriptor addresses, phase wave, dormant
collision outcome, and normalized replays. Debug and Release reproduce:

- Entry SHA-256:
  `fade5f879a62c2e9aadcac7bed74374c2ede950de526632a72e0c611db619d7b`
- Call SHA-256:
  `da3d925ec68f9295224ccec32fe500708b17f09404e4f8f99b2527a8cd736261`
- Write SHA-256:
  `66e67562ba3f7702c485ab0ff10d0a3d0b84a24b1116c1aae2834c99e1e96588`
- Selected-state SHA-256:
  `22e191e0f6dfb327096c30089d63f26c08a1e677cef0ffe0a41a376a7058f450`

Together with the previously mapped gameplay mechanisms, these compatibility
roles complete the initializer reconciliation for all 88 fixed normal-table
records and all 59 distinct initializer identities.

## Upper-middle Giant Door / Wooden Gate

The USA manual places the **Giant Door** at the upper-left of the middle
table region; the detailed player guide calls it the **Wooden Gate** and
documents the same three 5,000-point hits and 50,000-point destruction
award. Category-1 fixed slot `$4C` matches that identity at world position
`(64,254)`, with layout initializer `03:6294`, type `$82`, initial state
`$1E`, and flags `$02`.

Initialization clears the four handshake bytes `$2D06-$2D09` and controller
byte `$2D0A`, installs counter 3 at `$3B4C`, collision selector `$10` at
`$384C`, and completion selector `$81` at `$3D4C`. The `C018` transition
installs steady dispatch `03:62B8`.

The steady state has two replacement-relevant outcomes:

- With no pending collision, it returns immediately at `62D3`.
- A collision clears the sign bit in `$384C`, submits 50 stored units
  (5,000 displayed points), and decrements the three-hit counter. The third
  hit submits a further 500 units (50,000 displayed points), invokes the
  common impact helper, disables the original collision surface, and enters
  the `$2D06-$2D09` four-channel completion handshake.

The standard route activates the slot but does not strike it. It records one
initializer and 676 steady calls spanning frames 1,228-3,600. Of those
steady calls, 669 are 27-cycle, zero-write leaf paths; seven contain periodic
interrupts and take 684-774 cycles with 12-14 scoped writes. Normalized
`03:62B8@1` reproduces frame 1,228 with 27 body cycles and no ordered writes.

`tools/Analyze-GiantDoorSemantics.ps1` guards the fixed record, setup and hit
signatures, initial state, camera-window population, all steady calls, the
inactive hit/completion path, and normalized replay. Debug and Release
reproduce:

- Entry SHA-256:
  `a58c119ca3ff2027db4f1ff36b9489a40ad477e5d8a00a08511cbba8bbd5c45f`
- Call SHA-256:
  `44d2f8c8b9c2fd5ec20a27aaec5b0bbe379e75bb163cde3b296402c30e9cccff`
- Write SHA-256:
  `d4fb4aabcf69de8c9b6dd89349ffe4afbc61dc55af4197aedb48fe69ba7ed05e`
- Selected-state SHA-256:
  `64e95fd177928acbb6416a3048b3ceb6c12babde77d3279c14773697e5774cdc`

## Giant-Door Knight queue

The detailed player guide places four **Knights** in a vertical queue above
the Wooden Gate. They descend one at a time after the gate is destroyed and
each takes four hits. Category-1 fixed slots `$3A-$3D` match that structure:

| Slot | Position | Initializer | Queue/channel index |
| --- | ---: | --- | ---: |
| `$3A` | (32,198) | `03:734E` | 0 / `$2D06` |
| `$3B` | (32,182) | `03:7353` | 1 / `$2D07` |
| `$3C` | (32,166) | `03:735A` | 2 / `$2D08` |
| `$3D` | (32,150) | `03:7361` | 3 / `$2D09` |

The four initializers assign their queue indices at `$3B00,X`, then share
completion selector `$8B`, collision selector `$10`, hit counter 4, and
steady dispatch `03:7378`. The steady state waits for bit 0 in its indexed
Giant-Door channel. Once released it clears that request, enters the common
movement state, and retains the queue index for completion.

The collision helper at `03:73D8` decrements the four-hit counter.
Nonterminal hits emit command `$26` and submit 50 stored units (5,000
displayed points). The fourth hit takes the terminal carry path and submits
100 stored units, representing the normal 5,000-point hit plus the
5,000-point destruction bonus. It then sets bit 1 in the matching
`$2D06-$2D09` byte, runs the common release helper, and disables the actor
surface. This closes both directions of the Giant-Door/Knights handshake:
the gate requests a Knight with bit 0, and that Knight acknowledges
completion with bit 1.

The standard route never destroys the Giant Door, so all four channels remain
zero. Each slot contributes 2,650 calls to `03:7378`, for 10,600 total across
frames 873-3,600. Of those, 10,129 are 123-cycle wait calls whose only two
writes are the nested JSR stack pushes; 471 contain periodic interrupts and
take 645-1,950 cycles with 12-34 writes. Normalized `03:7378@1` reproduces
frame 873 with 123 body cycles and two ordered stack writes.

`tools/Analyze-KnightSemantics.ps1` guards the four layout records,
initializer/index assignments, wait/movement/hit signatures, slot and camera
populations, inactive handshake, interrupt-bearing calls, and normalized
replay. Debug and Release reproduce:

- Entry SHA-256:
  `920aa86f9080f3cdeb54bdbc7ed0928c3106b4429c1c12e4f2e8fbff6d772db5`
- Call SHA-256:
  `d0fd76fa97368baf2f2de9620ccf1d1fa478ced140c45b9e2d099b30b3e12dd0`
- Write SHA-256:
  `cfd3d8b0b096fba9f6188d4c855597d69ada545d65238fdae07cf1e7c4cdc3fb`
- Selected-state SHA-256:
  `97c15d5f95355bf7a7a70a876309688a1c982f165f7c213bedcf43bc8ebc3f18`

## Middle-table Walled Star

The detailed player guide places a **Walled Star** behind a brick wall at the
upper-left of the middle table region. Fixed slot `$5B` matches that position
at `(32,302)`, with initializer `03:6319`, type `$82`, initial state `$03`,
and flags `$89`.

Initialization clears the wall-damage appearance at `$39DB`, installs
collision selector `$10` at `$385B`, completion selector `$84` at `$3D5B`,
sets the wall-active bit, and installs steady dispatch `03:6339`.

The steady collision path has five stages:

- The first four collisions clear the pending-hit bit, submit 50 stored units
  (5,000 displayed points), emit command `$2E`, advance `$39DB`, re-arm the
  collision surface, and run the common impact helper.
- When `$39DB` is already 4, the fifth collision disables the wall, submits
  200 stored units (20,000 displayed points), invokes the shared bonus-update
  helper, and advances into the post-destruction access states.

The ROM therefore awards 40,000 total displayed points across one complete
wall lifecycle: four 5,000-point nonterminal hits followed by the
20,000-point terminal collision. The later states wait on `$2D53/$2D32`,
clear shared latch `$2DBC`, run the ball-routing and opening animation, and
eventually restore initializer `03:6319`.

The standard route does not hit the wall. It records one initializer and
2,650 steady calls spanning frames 873-3,600. Of those steady calls, 2,601
are 54-cycle zero-write leaves; 49 contain periodic interrupts and take
576-1,233 cycles with 10-22 scoped writes. Normalized `03:6339@1`
reproduces frame 873 with 54 body cycles and no ordered writes.

`tools/Analyze-WalledStarSemantics.ps1` guards the fixed record,
initialization, complete damage/access ROM signatures, actor state,
camera-window population, inactive collision path, interrupt-bearing calls,
and normalized replay. Debug and Release reproduce:

- Entry SHA-256:
  `768c7d7ec36841fc76780553fce953197a3fbb62228f45a9ab17f6732d5613af`
- Call SHA-256:
  `e31eab9f63b6c4b81bcd81eba32f3acba9c0feaccb8e482146a228003de9afc7`
- Write SHA-256:
  `7811e173ba484531f7545ca7dcf77d268216d0073aca628b5f689caa8baecc04`
- Selected-state SHA-256:
  `1e222fd4e49400bd45aaff1a23fb113b73fdb9240e60ba424c3ed4d12b03f14c`

## Walled-Star access pocket

Fixed slot `$11` is at `(38,308)`, immediately inside the Walled Star at
`(32,302)`. Its type `$84`, initializer `02:4A44`, steady dispatch
`02:4B3F`, and direct state handshake identify it as the collision surface
exposed after the wall is destroyed.

Initialization selects appearance `$04`, installs `02:4B3F`, and enters the
same four-tick/four-appearance idle loop used by the neighboring Screen-2
access surfaces. A qualifying collision:

1. Sets shared latch `$2DBC`.
2. Awards `$64` binary score units: 100 stored units, or 10,000 displayed
   points.
3. Runs a six-step opening animation.
4. Sets bit 0 of `$2D53` and waits for acknowledgment.

The Walled Star's post-destruction state at `03:638F` waits on `$2D53` (or
fallback field `$2D32`), advances through the common transition, then clears
`$2DBC`. This is the direct request/acknowledgment link between the outer wall
and inner access pocket.

After acknowledgment, `02:4B93` passes bound 6 to the RNG wrapper at
`00:E8A4`. Its `00:E569` range scaler returns
`floor(random_byte * 6 / 256)`, producing `0..5`; the caller adds one and
stores bonus-stage ID `1..6` in `$55`, then sets the session-transition bit
in `$53`. The pocket therefore selects one of all six bonus stages at random.

The canonical route does not destroy the outer wall or hit this pocket. It
records one initializer and 905 steady calls spanning frames 871-3,600:
661 are 66-cycle one-write timer calls, 144 are 135-cycle three-write
appearance advances, 72 are 144-cycle four-write wraps, and 28 contain
periodic interrupts. The interrupt-bearing calls take 588-1,023 cycles with
11-17 writes. Normalized `02:4B3F@1` reproduces frame 871 with 135 body
cycles and three ordered writes.

`tools/Analyze-WalledStarAccessSemantics.ps1` guards the fixed record,
initializer and live dispatch, idle population, 10,000-point hit, direct
`$2DBC/$2D53` handshake, bounded RNG implementation, random stage range,
both actor identities, untouched routed handshake, and normalized replay.
Debug and Release reproduce:

- Entry SHA-256:
  `bef6b62849397616eabdbc4f63247da80689c1472925c28e4bbc438ba82f830b`
- Call SHA-256:
  `1ff4a18a3f7caa572e46000abcd46598b96f3239fae0e8c75cec3a7a98aad3b8`
- Write SHA-256:
  `26c396dc657674d9d4e93c3ea07cd76390ef2ae9bb12bb7878997ede8bc4a8a5`
- Selected-state SHA-256:
  `dcaf467b0f5e7bc35b3051723ddebb6da2ef5f4266c8573d15f48f3b34d6c876`

## Screen-2 Side-Star access surfaces

The detailed player guide places two **Side Stars** at the lower-left and
upper-right of screen 2. The fixed layout contains matching type-`$84`
collision surfaces immediately below the already mapped Small-Soldier source
controllers:

| Access surface | Slot | Position | Initializer | Steady dispatch |
| --- | --- | --- | --- | --- |
| Lower-left Side Star | `$10` | (38,372) | `02:4A54` | `02:4BD5` |
| Upper-right Side Star | `$12` | (238,308) | `02:4A4C` | `02:4C0A` |

The initializers select appearance records `$1C/$0A`, install mirrored
dispatch pointers, and enter one shared idle animation. That animation counts
four ticks per appearance and cycles through four appearance values.

Both collision branches submit `$50` binary score units, or 80 stored units
(8,000 displayed points). The lower-left branch sets bit 1 in Giant-Face
request byte `$2D0C` and enters the shared access animation. The upper-right
branch checks persisted Giant-Face form `$2258`: a nonzero form requests the
same transformation, while form zero runs a six-step entrance animation,
selects bonus stage 6 in `$55`, and sets the session-transition bit in `$53`.
These hit outcomes are exact ROM contracts; the canonical route does not
exercise either collision.

The route records one initializer and 1,277 steady calls for the lower-left
surface, plus one initializer and 905 steady calls for the upper-right. The
uninterrupted idle populations are:

| Surface | 66-cycle one-write | 135-cycle three-write | 144-cycle four-write | Interrupt-bearing |
| --- | ---: | ---: | ---: | ---: |
| Lower-left | 934 | 202 | 103 | 38 |
| Upper-right | 660 | 148 | 70 | 27 |

The interrupt-bearing lower calls take 588-951 cycles with 11-17 writes;
upper calls take 588-1,245 cycles with 11-23 writes. Normalized
`02:4BD5@1` and `02:4C0A@1` both reproduce frame 871 with 135 body cycles and
three ordered writes.

`tools/Analyze-SideStarAccessSemantics.ps1` guards both layout records,
initializer and dispatch setup, shared idle animation, 8,000-point hit
awards, Giant-Face request branches, inactive BS-6 entrance, complete routed
call populations, global state timeline, and both normalized replays. Debug
and Release reproduce:

- Entry SHA-256:
  `7091984892271017079a1d98450f5e72f1242017eed26bb66c1209d917113339`
- Call SHA-256:
  `4cd9ebfab43aa644483536b36f95d49dc47c02743f5d3f51d5fe2b824ddcd956`
- Write SHA-256:
  `e4a2b7c0be94f44bd7593d8b0b4930a7f95aca598fd8acf90e357ddaab5279c2`
- Selected-state SHA-256:
  `a8932210562ebfb1b4195fbc1b27e9094a604fa10a9255c8364726d6bf01650e`

## Score accumulator

The broader route also reaches the first definitive scoring boundary.
`00:F55C` is called through the wrapper at `00:F54D`; the observed JSR is at
`F554`, it resumes at `F557`, and all calls leave through `F5A8`.

| Routine | Calls | Caller | Outer RTS | Current role |
| --- | ---: | --- | --- | --- |
| `00:F55C` | 40, frames 1,409-3,466 | `00:F554` | `00:F5A8` | Selects the score slot through `$2251`, adds the unsigned 16-bit award `$30:$2F` to the little-endian 24-bit score arrays `$2205,Y/$220C,Y/$2213,Y`, applies saturation and score/high-score status updates, and sets bit 0 of `$2CC0` for presentation. |

The route selects slot 0 on all 40 calls. Each scope writes the low, middle,
and high score bytes in that order and then writes `$2CC0`. Reconstructing the
three direct score writes produces a strictly increasing stored value from 0
to 1,280. Every increment equals entry A under the observed `F54D` caller
convention; the award set is 10, 30, 40, 50, 100, and 200.

The nested `F5A9` helper compares the new score against little-endian
`$98:$96:$7F`, or 9,999,999 stored units, and saturates all three bytes at
that value. `F5D6` then compares the selected score with the three-byte value
at `$2200/$2207/$220E` and updates score-status fields. The session-and-scoring
closure route below establishes that panel routine `09:7266` emits two fixed
zero glyphs before converting this binary value, so one stored unit is 100
visible points and saturation displays as 999,999,900.

All 40 calls complete with body budgets of 558-1,305 master cycles and 8-22
scoped writes. The first call has a normalized replay at frame 1,409 with 558
body cycles and eight ordered writes. Debug and Release reproduce:

- Entry SHA-256:
  `d4cbf19841208ff7045f87bf76677fb3ed7e9a69db2d14458d390f9e6ce43fa7`
- Call SHA-256:
  `d0caf39e0f764c653d3289cdc1b105627744aa094995506ecb05e7a68c2e289e`
- Write SHA-256:
  `241c03d3f1fc8c2de7fb08cd25d43ad18f4d3e48220172476548fb98fbc678c4`

`tools/Analyze-ScoreSemantics.ps1` guards the ROM instruction signatures,
bounded path, direct write order, observed award set, reconstructed final
value, and saturation constant.

## Session-and-scoring closure route

The Milestone 2 closure route reuses the 3,600-frame state-census input and
changes only slot 0's signed Y coordinate before frame 3,000. The existing
`$3380:$3300=$0300` seed crosses `01:DD6E`'s `$02D0` drain threshold and
connects the already bounded score accumulator to one complete ordinary
remaining-ball continuation:

| Frame | Ordered entry | State transition |
| ---: | --- | --- |
| 3,000 | `01:C786` | Sets session-transition bit `$53.1` after the live-ball drain gate |
| 3,016 | `01:C7B0`, `01:C7BF` | Decrements live-ball count `$2D52` from 3 to 2 |
| 3,271 | `01:C7E5`, `01:C7EA`, `01:C80A` | Finds a remaining ball, observes zero shared transition latch `$2D32`, and takes the ordinary presentation |
| 3,287 | `01:C812`, nested `02:4003` | Clears transition bit 1, reloads the seven category-1 table records, reinitializes the ball, and marks the panel dirty |
| 3,287 | `09:7266`, `09:72FD` | Consumes the dirty bit and redraws the score and remaining-ball digits |

The route contains 33 `00:F55C` calls before the drain. Their ordered direct
writes reconstruct stored score 1,020 from awards 10, 30, 40, 50, and 200.
The score stays unchanged from frame 2,962 through frame 3,600. The panel's
right-to-left glyph stream for the nine-digit score changes neither across
the drain nor during the continuation:

- Stored 1,020 emits `00,00,00,02,00,01,00,00,00`, which displays 102,000.
- The separately converted `$2D52 - 1` ball digits change from `02,00` to
  `01,00`, displaying two remaining balls before the drain and one after it.
- Static and dynamic evidence agree that the two `09:72FD` calls before
  binary-to-decimal conversion are fixed zero digits. Saturated stored score
  9,999,999 therefore displays as 999,999,900.

This ordinary route does not enter a multiplier, extra-ball, or alternate
transition branch: `$2D32` remains zero throughout the drain, `$2D52`
decrements exactly once, and no score mutation occurs after the last routed
award. Exhaustive extra-ball, last-chance, two-player, and game-over variants
remain release-acceptance work rather than Milestone 2 closure prerequisites.

`tools/Analyze-SessionScoringRoute.ps1` and `dc_rom_session_scoring` guard the
ROM gates, complete work-RAM timeline, 402 globally ordered entries and
bounded calls, 27,769 scoped writes, score reconstruction, panel dirty-bit
handoff, exact digit streams, and continuation:

- Work-RAM SHA-256:
  `229ffbff969db6067e2adb10b7f05523f048c7be1539fc2188499ca62f18ab5b`
- Entry SHA-256:
  `bd1bf1250380a6a7c7d87c92d5884e0190584619f1fed96ebd412413c96d8d47`
- Call SHA-256:
  `31f9af5449deeef5395aec78f916dbe54b42d72436c98fd36413c8550173e25a`
- Scoped-write SHA-256:
  `19a322d5efee8660e00e93f759c3b5ff308de8f2727e85becc13ed7b915bf567`

## RNG seed and update

The original RNG uses a free-running 16-bit seed at `$222B:$222A`. The frame
interrupt increments the low byte at `00:E16E` once in every frame after the
first and increments the high byte at `00:E173` on carry. The 3,600-frame
route records 3,599 low-byte ticks and 14 carry ticks.

Game code reaches the common update routine through the fixed JMP-table entry
`01:C081`:

| Routine | Calls | Active callers | Outer RTS | Current role |
| --- | ---: | --- | --- | --- |
| `00:E8B3` | 78, frames 1,228-3,594 | `03:6678` (9), `03:6697` (69) | `00:E8E6` | Mixes the current seed, constant `$503F`, and frame counter `$00`; stores the new 16-bit seed and returns its high byte in A. |

For old seed bytes `(H,L)` and frame counter `f`, the routine forms
`t = ((H:L + $503F) << 1) mod 65536`, then computes:

```text
L' = (L + f) mod 256
H' = (L + H + high(t) + carry(L + f)) mod 256
```

It stores `(H',L')` at `$222B:$222A` and returns `H'`. Each call has eight
direct writes: six intermediate writes through `$42/$43`, followed by the new
high and low seed bytes. `tools/Analyze-RngSemantics.ps1` reconstructs the old
seed from those intermediates and verifies every step of all 78 updates.

The two active consumers use consecutive results while initializing actor
arrays. `03:6678` masks the result with `$3F`, adds `$80`, and writes
`$3B00,X`; `03:6697` masks with `$1F`, adds one, and writes `$3B80,X`.

An exact cartridge-wide census finds 21 `JSR C081` sites. Twelve are in the
normal-table banks and nine are in bonus-stage banks:

| Consumer | Immediate result transform | Routed |
| --- | --- | --- |
| `02:45C9` | Bit 0 selects state `$2D04` value 1 or 2 | Yes, 1 targeted Little-Skull release |
| `02:5148` | `$40 + (A & $3F)` passed to `C021` | Yes, 1 targeted Devil-Shooter automatic reset |
| `02:57C2` | Bit 0 contributes selector `$28/$29` at `$2D0B` | Yes, 18 targeted Sorcerer resets |
| `02:597D` | `(A & $3F) + $0080` stored at `$3B00,X` | Yes, 1 targeted upper-hand call |
| `02:59F5` | `$B0 + (A & $3F)` passed to `C021` | Yes, 1 targeted upper-hand response |
| `03:6177` | Bit 0 selects `$17/$19`, passed to `C075` | Yes, 1 targeted Undead-Head launch |
| `03:625D` | Bit 0 selects `$07/$09` at `$3C80,X` | Yes, 1 targeted Undead-Head response |
| `03:6620` | Low two bits seed three consecutive modulo-4 Blob states | Yes, 1 targeted equal-state reset |
| `03:6678` | `$80 + (A & $3F)` at `$3B00,X` | Yes, 9 calls |
| `03:6697` | `1 + (A & $1F)` at `$3B80,X` | Yes, 69 calls |
| `03:68B1` | `$40 + (A & $1F)` at `$3B80,X` | Yes, 7 natural Gray-Blob calls |
| `03:7834` | Bit 0 selects `$07/$09`, passed to `C075` | Yes, 1 targeted Giant-Face mouth release |
| `04:47C7` | `$80 + (A & $2F)` at `$3B00,X` | Yes, 5 BS-1 calls |
| `04:4805` | `$C0 + (A & $2F)` at `$3B00,X` | Yes, 1 targeted BS-1 call |
| `05:6B88` | `A & $1F` passed to `C075` | Yes, 18 targeted BS-4 calls |
| `05:6CB1` | `A & $1F` passed to `C075` | Yes, 22 targeted BS-4 calls |
| `05:7217` | `$40 + (A & $3F)` at `$3B00,X` | Yes, 2 BS-6 calls |
| `05:732E` | `$18 + (A & $1F)` at `$3B00,Y` | Yes, 1 natural BS-6 call |
| `05:733B` | Low-nibble index into table `05:73B1`, then `C072` | Yes, 1 natural BS-6 call |
| `07:48C7` | Low nibble passed to `07:48DF` | Yes, 23 maximum-score-ending calls |
| `07:48CF` | Bit 0 selects command `$2C/$2D` through `C087` | Yes, 23 maximum-score-ending calls |

The `$2F` masks at `04:47C7/4805` are non-contiguous: their stored results
are `$80-$8F` or `$A0-$AF`, and `$C0-$CF` or `$E0-$EF`, respectively. The
standard state-census route enters `03:6678/6697`. Targeted normal-table
routes cover the other ten sites; the final combined route enters
`02:5148`, `03:6177`, and `03:7834` exactly once each. Result `$E7` becomes
Devil-Shooter selector `$67`, even result `$70` selects Undead-Head vector
`$17` / velocity `(-12,-62)`, and even result `$58` selects Giant-Face
vector `$07` / velocity `(+12,+62)`. All 12 normal-table consumers now have
deterministic dynamic coverage. Seeded bonus routes enter
`04:47C7`, `04:4805`, `05:6B88`, `05:6CB1`, `05:7217`, `05:732E`,
`05:733B`, `07:48C7`, and `07:48CF`. All nine bonus/ending-bank consumers
are dynamically covered as well.

`tools/Analyze-RemainingNormalRngSemantics.ps1` and
`dc_rom_remaining_normal_rng` guard the three final consumer signatures,
their output transforms and motion vectors, every RNG-owned recurrence
write, the interrupt-bearing call contracts, and normalized no-interrupt
samples `03:7821@1` and `00:E8B3@1`. Debug and Release reproduce census,
entry, call, scoped-write, and selected-state SHA-256 values
`17b63292db225e089e87b0c8216ef49414a300dcecb71ea73db51fd25b4c7790`,
`3352122bad7e6cebb1040d5a5299787a8cd8df98fc4808ef920c5b2ea5ad73fa`,
`c2924c92ee7b63443c6be2a13ed11cbc800fd4f761dc779a411bc4dd13c39d00`,
`9f0eb002eb61581bef1c9465c4f12782c3ce0716d60c7aadad330cc613cdc3f9`,
and
`88efd2fa575124ba3fd9167266d1fd9026547583c9487645869cfb96c45fa55d`.

Seventy-two calls complete in 282 master cycles with eight writes. Six calls
contain a periodic interrupt and complete in 939-1,239 cycles with 20-24
scoped writes; their eight RNG-owned writes and recurrence remain unchanged.
The first call has a normalized replay at frame 1,228 with 282 body cycles and
eight ordered writes. Debug and Release reproduce:

- Entry SHA-256:
  `3202fb333bba3b82e7126a08bad244f534237403983ccb65206342b4849a77c5`
- Call SHA-256:
  `cca8a9e2c1dba861e4eb550e781eb969aac3625f76764a66569f66e450bd029b`
- Scoped-write SHA-256:
  `06389c11544b16f5a98b6b509b124533cb9e0b57858c274471faf6eee506eb7e`
- Seed-write SHA-256:
  `61bf739045460c23feb39498cc48c09d000cccb888bd0fe0110b897b50f66952`

This establishes the deterministic seed, call-order boundary, and immediate
output transform required by a native replacement. The detailed gameplay
meaning and dynamic call ordering of the three inactive normal-table consumers
still require routes that enter their event branches.

## First physics census

A selective gameplay write census over logical work-RAM ranges
`$0000-$007F`, `$2000-$20FF`, `$2C00-$2FFF`, and `$3000-$3FFF` records
2,820,652 writes without changing the deterministic gameplay artifact. The
census preserves both the CPU-visible logical address and the physical
8 KiB work-RAM offset, which is important because gameplay arrays visible at
`$3000-$3FFF` occupy offsets `$1000-$1FFF`.

Static JSR-target inspection and the dynamic writer PCs identified these
first physics boundaries:

| Routine | Calls | Caller | Outer RTS | Current role |
| --- | ---: | --- | --- | --- |
| `00:F1B4` | 1,156, frames 1,049-1,800 | `01:C6D6` | `00:F240` | Signed 16.8 position integrator. Adds two signed 8.8 increments to the parallel slot arrays and marks subpixel-only movement in `$3C80`. |
| `00:F241` | 404, frames 1,064-1,800 | `01:C6E6` | `00:F25F` | Collision rollback helper. Restores the six pre-integration position bytes cached by `F1B4`. |
| `01:D1A2` | 404, frames 1,064-1,800 | `01:CF89` | `01:D227` | Signed 8.8 two-dimensional rotation. Produces the components at `$3400/$3480` and `$3500/$3580`. |
| `01:DD09` | 753, frames 1,048-1,800 | `01:C6EF` | `01:DD6D` | Component damping and acceleration. Subtracts a signed quotient from each nonzero component and adds `$8B/256` to the second component. |

All 2,717 entries complete. Observed body-cycle ranges are 639-2,154,
162-855, 5,076-8,736, and 168-5,232 master cycles in the table order above.
Their entry, bounded-call, and 171,993-write traces are byte-identical in
Debug and Release:

- Entry SHA-256:
  `9442322858ad18bfdf67ccb9a05aaf2312c512e168e2c7dbcd3ab858fed3cf79`
- Call SHA-256:
  `92f638ae77cad9e2964cb2bafdca6b21a2aa50187abded37480ebd55146170a4`
- Write SHA-256:
  `6f272290712ed6432205e7858d6fa72102623b08dece9beeb5c4cee2d3ffbbce`

### Resolved indexed-slot and fixed-point semantics

`tools/Analyze-PhysicsSemantics.ps1` checks the defining instruction
signatures in the supported ROM and the complete bounded call/write traces.
It establishes:

- `X`, not `Y`, indexes all parallel physics arrays in these four routines.
  Every one of the 2,717 recorded calls enters with `X=$00`, so the current
  route proves only indexed physics slot 0. `Y` is caller context retained
  across the calls and is not an operand in these calculations.
- The first signed 16.8 position is stored little-endian across
  `$3100,X` (fraction), `$3180,X` (integer low), and `$3200,X` (integer
  high/sign). The second uses `$3280,X`, `$3300,X`, and `$3380,X`.
- `F1B4` adds signed 8.8 increments `$BA:$B9` and `$BC:$BB`, sign-extended
  to 24 bits, to those positions. It caches the original six bytes in
  `$85-$8A`. If neither 16-bit integer portion changes, it sets bit 1 at
  `$3C80,X`; a fractional-only update therefore does not trigger an integer
  position step.
- `F241` writes `$85-$8A` back to the six position arrays in order. All 404
  calls have exactly those six routine-owned writes. The enclosing
  `01:C6D6` loop invokes it on the collision-response path, making it a
  position rollback rather than a velocity adjustment.
- `D1A2` uses the signed 8.8 multiplier at `00:E65C` and negate helper at
  `00:E837`. With `u=$C1:$C0`, `v=$BF:$BE`, `c=$7E:$7D`, and
  `s=$80:$7F`, it computes `u*c - v*s` into `$3480:$3400,X` and
  `u*s + v*c` into `$3580:$3500,X`.
- `DD09` uses the signed divider at `00:E905`. Each nonzero component becomes
  `value - trunc(value/divisor)`, using `$8C` and `$8D` as the respective
  divisors. It then adds the unsigned low-byte term `$8B` to the second
  signed 8.8 component, equivalent to an acceleration of `$8B/256`.

The route does not yet establish the meaning or valid range of nonzero `X`
slots, the gameplay names of the transform inputs and coefficients, or the
interrupt placement required by replacement execution.

### Independent physics oracle

The Mesen capture now supports a `physics` profile using the same deterministic
input route and bounded-call format. Two isolated captures reproduce:

- Entry SHA-256:
  `d1f4091585a65e6f5d62eb0d1a738944fca5b2d708fbcac749bdae47bbcec5b2`
- Frame-boundary work-RAM SHA-256:
  `868b08b94d0d725b0a56f7c1f88a1286a232d7860c8f310695a694624631e183`
- Call SHA-256:
  `53ea96f7a690da43f08a2d5282ebd71224057897ce172890c408b376c8adfc7f`
- Write SHA-256:
  `0b414a8f27f33ca76573048af1f79eb8358d2211df32ff60ded3c695f09a6178`

Mesen observes 1,157 `00:F1B4`, 404 `00:F241`, 404 `01:D1A2`, and
754 `01:DD09` entries. The one extra `F1B4` and `DD09` entry belongs to
caller-context `Y=$00`; the `Y=$06` context streams have the same 404 and 579
calls in both cores. Callers and outer return sites agree for every completed
scope.

The comparison deliberately separates entry equivalence from pre-call state
equivalence:

- Caller-context register and stable-MPR input streams align.
- `F241` and `D1A2` provide 808 aligned leaf calls. Of these, 549 have
  matching outputs and routine-owned ordered work-RAM writes.
- The other 259 leaf calls begin with different work-RAM state even though
  their register arguments agree. They occur after the two cores place the
  slot-0 `Y=$06`-context motion calls in different output frames.
- `F1B4` and `DD09` therefore have verified counts, callers, returns, and
  caller-context input streams, but their wrapper outputs/writes are not
  asserted equivalent without restoring an identical pre-call state.

This is not classified as a routine semantic mismatch. It is a limitation of
comparing naturally scheduled calls from two cores. For the upcoming
assembly/native shadow comparison, the host now provides the normalized
checkpoint/replay boundary below so both branches can start from the same
Beetle state independently of frontend frame placement. Naturally scheduled
cross-core traces remain supporting context rather than interchangeable
savestates.

### Savestate-normalized pre-call replay

The host now accepts repeatable `--routine-replay BANK:PC@ORDINAL` requests
alongside an observed bounded-call recording. At the selected one-based entry
ordinal, the pre-instruction hook captures both Beetle's opaque savestate and an
independent 8 KiB work-RAM copy. After the natural recording and its existing
artifacts are committed, the host:

1. Restores the checkpoint and verifies the complete pre-call work-RAM image.
2. Replays the restored frame with the original deterministic input.
3. Compares input/output registers and MPRs, caller and return path, body-cycle
   budget, ordered writes and their entry-relative timing, and complete
   post-call work RAM.

The ROM-backed physics regression covers `00:F1B4@17`, `00:F241@1`,
`01:D1A2@1`, and `01:DD09@1`. The first three are the frame-1064 actor-motion
group; the fourth is `DD09`'s short frame-1048 path. Their bounded results are:

| Replay | Frame | Body cycles | Ordered writes |
| --- | ---: | ---: | ---: |
| `00:F1B4@17` | 1,064 | 714 | 17 |
| `00:F241@1` | 1,064 | 162 | 6 |
| `01:D1A2@1` | 1,064 | 8,736 | 259 |
| `01:DD09@1` | 1,048 | 168 | 2 |

Beetle does not resume a mid-frame savestate with the original frame-local
timestamp value. The replay therefore has a new absolute clock origin, while
the serialized CPU/RAM/device state, body-cycle budget, and entry-relative
write timing remain reproducible. The comparator normalizes only that origin.

Longer actor-6 `DD09` samples can contain a periodic interrupt. Their final
registers, work RAM, call-cycle budget, and write count reproduce, but the
rebased frontend clock can move the interrupt within the scope and therefore
relocate its writes. Those samples remain assigned to the explicit
interrupt-delivery contract rather than the deterministic no-interrupt replay
gate.

### First native replacement: `00:F241@1`

The first gameplay code has now been ported to C in
`src/game_logic/physics.c`. `dc_restore_cached_position` reads cache offsets
`$0085-$008A` from an isolated 8 KiB work-RAM snapshot and returns the six
indexed destination offsets and values. The host adapter adds the
HuC6280-visible contract:

- Writes occur at logical `$3100/$3180/$3200/$3280/$3300/$3380 + X`.
- Originating store PCs remain `F243/F248/F24D/F252/F257/F25C`.
- Relative write times are 27, 54, 81, 108, 135, and 162 master cycles.
- `A` and N/Z reflect the last cached byte; X, Y, SP, other status bits, and
  all MPRs are preserved.
- Beetle resumes at outer RTS `00:F25F` after 162 body master cycles, leaving
  RTS stack and caller-return behavior emulated.

`--replace-hook 00:F241@1` replaces only the mapped first call. The
instruction observer captures its pre-call savestate before the transaction.
After the 1,800-frame natural run, `--routine-replay 00:F241@1` restores that
state; the ordinal gate declines the second encounter, so the original
assembly executes. The two branches match input/output registers and MPRs,
caller/return path, body cycles, six ordered writes and relative timings, and
complete post-call work RAM.

The replacement run also preserves the established physics gameplay, entry,
call, and ordered-write hashes:

- Gameplay SHA-256:
  `1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353`
- Entry SHA-256:
  `9442322858ad18bfdf67ccb9a05aaf2312c512e168e2c7dbcd3ab858fed3cf79`
- Call SHA-256:
  `92f638ae77cad9e2964cb2bafdca6b21a2aa50187abded37480ebd55146170a4`
- Write SHA-256:
  `6f272290712ed6432205e7858d6fa72102623b08dece9beeb5c4cee2d3ffbbce`

An exploratory all-404-call replacement found periodic interrupts in the
longer `F241` scopes: deferring a newly asserted interrupt to the replacement
boundary moved interrupt-owned writes and later framebuffer state. This is
now an enforced design boundary, not an unexplained mismatch. Replacement is
limited to separately mapped no-interrupt ordinals until the core bridge can
deliver interrupts at their original internal cycle boundary.

## Controls-to-physics closure route

The 3,600-frame state-census input is now also the deterministic Milestone 2
controls-and-physics closure route. One globally ordered trace combines the
Devil Shooter's Button-I state, the six flipper/control pieces, the session
nudge gate, and the mapped slot-0 integration/damping scopes:

| Routine | Calls | Routed role |
| --- | ---: | --- |
| `02:5159` | 101 | Devil-Shooter Button-I charge and release |
| `01:D466` | 7,470 | Per-piece flipper input-mask response |
| `01:C3CB` | 2,649 | Session, tilt, and Button-II nudge gate |
| `00:F1B4` | 3,568 | Slot-0 signed 16.8 position integration |
| `01:DD09` | 2,540 | Slot-0 signed 8.8 damping and acceleration |

The route connects each control family to live emulated state:

- Button I remains held through frame 1,180. Devil-Shooter charge counter
  `$2D15` reaches `$18`; four released-input delay calls follow, and
  `02:51FA` writes signed vertical motion `$3580=$F8` on frame 1,185.
  `00:F1B4` next integrates slot 0 on frame 1,186.
- The central actor scheduler copies held input to `$2CEE`. Button I value
  `$01` drives right control slot `$09` into `01:D48E` on frame 1,265.
  Left input `$80` drives slot `$08` on frame 1,280. Combined left+nudge
  input `$82` drives left slots `$04/$06` on frame 3,050.
- Button II enters `01:C3CB` on frames 1,450, 2,250, and 3,050. Each event
  and its following-frame continuation write slot-0 motion
  `$3500/$3580`; the next mapped `F1B4` call consumes that state.
- The route never enters tilt lock: `$2CC7` remains zero in all 3,600
  frame-boundary work-RAM images. The ROM signature retains the explicit
  `$2CC7 != 0` early return, so tilt gating is classified without forcing an
  additional tilt outcome into this closure route.

`tools/Analyze-ControlsPhysicsRoute.ps1` and
`dc_rom_controls_physics` guard the scripted inputs, ROM gates, complete
work-RAM timeline, control-event ordering, selected writes, and the five
combined routine populations. Debug and Release have their established
configuration-specific framebuffer census hashes, while the semantic
artifacts are identical:

- Work-RAM SHA-256:
  `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`
- Entry SHA-256:
  `1a7a40ad5aeb86b7612a17db10e789b0d169ce020670f23165f45de46f316077`
- Call SHA-256:
  `48c4bf3c99233301cd8b3408116436f2b7960a7e0fd4a441ca3162c692f295ca`
- Scoped-write SHA-256:
  `7bca93abffaef5b85e855434c802fd03c1b75f6eca6eac9d0d9410eaf782846a`

## Normal-table collision closure route

The same unpatched 3,600-frame state-census route now closes the Milestone 2
normal-table collision boundary. Small-Soldier slot `$62` receives two
natural ball contacts, at frames 1,479 and 1,852. One globally ordered trace
connects the central confirmed-contact dispatcher to the actor collision
state, stored-score accumulator, accepted audio-command path, and group/event
state:

| Routine | Calls | Routed role |
| --- | ---: | --- |
| `01:CBCF` | 270 | Confirmed ball/actor contact and type-specific ball response |
| `03:7F57` | 20,867 | Small-Soldier collision consumer and two-hit rule |
| `03:6966` | 54,149 | Shared actor collision-table interpreter |
| `03:7FB1` | 21,987 | Small-Soldier response-state update |
| `00:F55C` | 40 | Stored-score accumulator |
| `00:ED18` | 66 | Accepted game-to-audio command path |

`01:CBCF` is entered after the central overlap test succeeds. It loads the
target slot from `$64`, sets bit 7 in `$3800,Y`, stores the target in `$BD`,
masks the target type, and tail-dispatches through the vectors at
`01:CBFA/CBFF`. Small-Soldier type `$82` selects vector 2 and response
`01:CF33`. That response derives reflected ball motion, calls shared motion
updater `01:D1A2`, marks the contact handled, and returns through the bounded
`01:DC95` continuation. The two slot-`$62` contact calls write:

| Frame | Contact flag | Final scoped slot-0 motion | Contact return |
| ---: | --- | --- | --- |
| 1,479 | `$3862=$90` | `$3500=$06`, `$3580=$02` | `01:DCD7` |
| 1,852 | `$3862=$90` | `$3500=$09`, `$3580=$07` | `01:DD08` |

Both contact calls finish before slot `$62` enters `03:7F57` later in the
same frame. Each actor call then enters `03:6966` from `7F5A`, enters
`03:7FB1` from `7F5D`, consumes bit 7 at `03:7F67`, calls the score path with
`$001E` (30 stored units / 3,000 displayed points), and passes command `$21`
through `00:ED18` to the driver latch at `45:4BCC/$00D4`.

The first hit changes `$3C62` from 2 to 1 and stored score from `$00000A` to
`$000028`, leaving actor type `$82` active. The second hit changes the
counter from 1 to 0 and adds the same 30-unit award, then takes the ordinary
completion branch: a further 50-unit award produces stored score `$000096`,
`$2D16` changes from 5 to 4, and `01:C574` clears all 32 slot-`$62` arrays.
The frame-boundary image therefore records zero type, collision, counter,
and dispatch fields for the completed actor.

`tools/Analyze-CollisionBoundaryRoute.ps1` and
`dc_rom_collision_boundary` guard the ROM contact/response vectors, complete
work-RAM timeline, 97,379 ordered calls, 1,163,259 scoped writes, 8,893
selected state writes, contact-before-consumer timing, nested collision and
response calls, score/audio coupling, group count, and complete actor clear.
Debug and Release retain their established configuration-specific
framebuffer census hashes while reproducing identical semantic artifacts:

- Work-RAM SHA-256:
  `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`
- Entry SHA-256:
  `c151ef79d4052f94b0740eb6232398d19eee5b0e8b1086f71990134f8b113d7b`
- Call SHA-256:
  `239b72a47ddb1965af5c206136062a4e735dbcb475e798b1f9da29cc6752eeca`
- Scoped-write SHA-256:
  `9c0fda84b35afa075465975fc8efb0320c9fc441bedd42d93b067e016b2dd426`
- Selected-state SHA-256:
  `99cf6ab9fed4cfa976510fda30f77c20671e8f2134fc3b2842e3d90fd81d32d8`
