# Routine Map

This map records bank-qualified HuC6280 instruction entries observed through
the read-only execution-hook registry. Addresses use the physical 8 KiB ROM
bank plus logical PC. Labels remain conservative: an address is not considered
replaceable gameplay logic until its inputs, writes, callers, return behavior,
and cycle role are understood.

## Confirmed boundary

| Routine | Observations | Status | Evidence |
| --- | ---: | --- | --- |
| `00:E195` | Canonical: 599, frames 2-600. Gameplay: 1,799, frames 2-1,800. | Controller poll | Reads the PCE controller I/O register, selects controller nibbles, and updates the controller state arrays in work RAM. Beetle and Mesen agree exactly in both recordings. Native since the pad-reader slice. |

## Canonical screen-flow candidates

| Routine | Canonical observations | Status | Evidence |
| --- | ---: | --- | --- |
| `08:5977` | 115, frames 157-600 | Setup-screen palette stager | Active across the title, play-mode, speed, and data-load screens. It resolves a colour set through the asset resolver at `00:E1FD`, selects one of two phases from the cadence at `$3A80,X`, copies thirty-two bytes into block zero of the palette shadow buffer at `$27B9`, and raises the dirty flags at `$2CCC` and `$2CCB` — `00:EBA5`'s job for the block those screens flash. Native across `08:5977` and `08:5982`; see the palette section below. |
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

Semantic `00:F4FC@all` now derives all four paths from live slot and camera
state. The empty path supplies two instruction checkpoints and returns after
27 cycles without a write. The below-window path supplies 18 checkpoints,
writes the lower-bound scratch pair plus the cleared flag, and returns after
189 cycles. The inside/above paths supply 31 checkpoints, write both bound
pairs and the final flag, and return after 333/327 cycles. All 65,927 calls
are replaced, including 4,858 calls whose original scopes contain periodic
interrupt handlers; the hashes above remain exact in Debug and Release.

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

Semantic `01:C5AB@all` now owns that complete allocator. Its state-derived
instruction transcript covers every occupied-slot iteration, the `$80`
reservation, the alternate `$C0` reservation selected by `$2055`, the
carry-set exhausted return, and the live stack-derived `RTS`. All 44 natural
calls execute natively, including 72 original interrupt-handler passages;
the exact 44-call/1,077-write artifacts and canonical census remain unchanged.
Six persisted uninterrupted fixtures cover the natural scan lengths. Forced
fixtures additionally prove the 4,374-cycle `$C0` overwrite and the 5,406-cycle
carry-set exhausted return, while the existing patched Knight route verifies
all 64 of its calls.

The state census observes 132 `C574` calls: 88 fixed-layout resets, 35
session/actor releases from `01:C51D`, six paired-flipper setup resets, and
three setup-screen resets. Ninety-seven calls run uninterrupted at 3,627
body master cycles with 99 writes; 35 contain periodic interrupt handlers.
The complete call artifact has SHA-256
`836c596cd4891fed4c47214fa9cd777299662ce001b5f25fef07756db7f3ce0f`;
the 14,163-write artifact has SHA-256
`e3664e9ac79b6d6c90f08a4c246930385ba0c86d1c01fe843ac1eecbd108f2d0`.

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

## Timer sound driver

The `TIMER` vector lands at `00:FC00`, and the handler that follows is the
sound driver. It is flat — no `JSR` anywhere in it — and runs to `00:FE48`.
The prologue guards against reentry and opens the interrupt window:

```
FC26  PHA
FC27  STZ $1403     acknowledge the timer interrupt
FC2A  LDA $D9       already inside the handler?
FC2C  BNE $FC8C     yes: bail without touching a channel
FC2E  INC $D9
FC30  STZ $DA       clear the rerun request
FC32  CLI           the rest of the handler is interruptible
FC33  PHX / PHY / TMA #$40 / PHA
FC39  LDX #$04
```

The body then walks three channels at X = 4, 2, 0, two-byte-strided through
zero page: `$DD,X`/`$DE,X` is the sequence cursor, `$E3,X` a shift register of
sample bits, `$E4,X` the channel state, `$E9,X`/`$EA,X` a hold counter,
`$EF,X`/`$F0,X` a fractional rate accumulator, `$F5,X` the data bank with bit 7
meaning "not yet mapped", and `$F6,X` per-channel flags. Each iteration selects
the channel with `STX $0800`, maps the channel's bank in with `TAM #$40`, and
emits one five-bit DDA sample:

```
FC58  LDA ($DD,X)   sample byte: two four-bit samples
FC5A  AND #$F0
FC5C  LSR A x3      high nibble, scaled
FC5F  ASL $E3,X     carry out is the extra low bit
FC61  ADC #$00
FC63  STA $0806     PSG waveform port
```

`00:FD10` does the same for the low nibble with `AND #$0F` / `ROL A`. The
`$EF,X`/`$F0,X` pair resamples: adding advances the cursor slowly, and the
`SBC` paths at `00:FC97`, `00:FCB6`, `00:FD8B` and `00:FDC2` step it faster.
When a channel's hold counter runs out, `00:FDDB` either silences it — `STZ
$0805`, `STZ $F5,X`, and `STZ $0C01` to stop the timer if all three channels
are now idle — or falls into `00:FDF4`, which advances the track: it maps bank
`$48` (the table at `$FC06` is `48 49 4A 4B 4C 4D 4E 4F`), reads the next
eight-byte command through `$2E10,X`, and reloads the channel record.

The epilogue at `00:FC7C` and `00:FD70` unwinds, restores the caller's bank,
clears `$D9`, and then reads `$DA`: if another handler asked for a rerun while
this one held the interrupt window open, it chains through `JMP $FC03` to
`$E003` rather than returning. That read is why the native walk covers the
epilogue as two transcripts rather than one — it stops in front of each copy's
`SEI` and is re-entered at `00:FC82` and `00:FD76`, so the rerun test runs
against a snapshot taken after any preempting handler has written `$DA`. The
`SEI` itself, and the closing `RTI`, stay emulated.

## Music sequencer

Bank `45`, at `$4000`-`$4CB8`, is a separate component: the music sequencer,
which programmes the PSG registers `$0801`-`$0809` directly and decides what
the timer driver then streams. It is not part of the driver.

Its head is a seven-entry jump table:

```
4000  JMP $4B12     cold start: clear $D6, silence $0801/$0809, seed $2E3A-$2E3F
4003  JMP $4015     the per-frame tick
4006  JMP $4B7B     guarded call into the track loader at $4BCC
4009  JMP $4B33     stop: clear the voice table, the timer, and $F5/$F7/$F9
400C  JMP $4B8E
400F  JMP $4B93
4012  JMP $4B97     fade request: set $2E42/$2E44, mark all nine voices
```

The tick at `45:4015` walks nine or ten voices through `$2E34`, and each
iteration calls into a fixed set of subroutines. Voice state is a set of
parallel arrays in `$2E00`-`$2FFF` indexed by a voice cursor that steps by ten
(`4C90` is `ADC #$0A`), with the sixteen-bit quantities split across two
arrays a fixed distance apart -- `$2F22,X`/`$2F2C,X` is the sweep
accumulator, `$2EC8,X`/`$2ED2,X` the step counter and its limit.

The subroutines the tick calls, and their cost on the census route:

| Entry | Calls | Master cycles | What it does |
| --- | ---: | ---: | --- |
| `4883` | 22,664 | 3,958,542 | Pitch/period update; calls `48B4` |
| `48F6` | 22,664 | 4,438,275 | Sweep accumulator update; calls `48B4` |
| `48B4` | 25,968 | 1,771,122 | Sequence-data fetch, the shared command decoder |
| `49A2` | 22,664 | 1,239,765 | Vibrato accumulator update |
| `49F9` | 22,664 | 8,577,900 | Voice commit; drives `$0800`/`$0804` |
| `4142` | 3,599 | 7,404,498 | PSG register programmer, chunked at `4143` |
| `457A` | 2,299 | 1,690,857 | Command interpreter; calls `4522`/`4569`/`46D7` |
| `46D7` | 2,254 | 1,203,030 | Note to period, and the sample trigger at `4776` |
| `4228` | 1,142 | 1,326,999 | Portamento/sweep stepper; shares `46D7`'s tail |
| `49B6` | 3,604 | 607,533 | Voice-latch clear |
| `4211` | 7,198 | 541,458 | Two pending-event gates |
| `4936` | 548 | 93,495 | |
| `4569` | 867 | 106,761 | |
| `4522` | 287 | 30,984 | |
| `4527` | 120 | 23,082 | |
| `4BCC` | 66 | 195,549 | Track loader; calls `4527` |

`48B4` is the piece worth reading first, because it defines the data format
everything else consumes. A byte below `$80` is literal data; `$80`-`$84` are
commands that re-read the stream relative to the cursor -- `$80` steps back
one, `$81` subtracts the following byte from the cursor and re-reads there
(a loop), `$82` restarts at offset zero, and `$83`/`$84` branch on bit two of
`$2EB4,X` before doing one of the above.

`45:4142` is the other half worth reading. It walks the six PSG channels in
X, bracketing each in `PHP`/`SEI` ... `PLP`, and pushes the voice's latched
state out to the register file: `$0802`/`$0803` period, `$0804` amplitude,
`$0805` balance, `$0807` noise. A voice whose latched amplitude `$2E22,X` is
zero is silenced instead. When `$2E2E,X` names a waveform other than the one
already loaded (`$2E3A,X`) or `$FF`, it computes `$4E59 + index * 32` through
`$D2`/`$D3` and uploads 32 bytes through `$0806`. The tail programs the LFO
at `$0808`/`$0809`, writing `$0809` twice when `$2E41`'s high bit marks a
pending reset.

It also owns the handshake with the timer driver. `$F5`, `$F7` and `$F9` are
the driver's per-channel bank bytes; `4142` clears one at `4151` to release a
channel, and at `415A`-`4165` reads all three and stops the timer through
`$0C01` when every channel has fallen idle. That read-modify is why each
iteration runs under `SEI`.

The channel walk is a loop with `4143` at its head and `41F7` closing it with a
`JMP $4143`, which is where the port chunks it. `4143` is a registry entry in
its own right for that reason, and the reason it is a legal boundary is in the
bracket: the `PLP` at `41F1` restores the caller's mask before control reaches
`41F7`, so the top of an iteration is a point the hardware may take an
interrupt at anyway. Iterations are 390 master cycles plain and 2,190 for one
that uploads a waveform, against a 5,068-cycle timer period.

`45:457A` is the third piece worth reading: the command interpreter, and the
only part of the bank that reads the music data itself. It points `$D0`/`$D1`
at the voice's cursor and scans forward. A byte under `$80` is a note; `$C0`
-`$DD` selects an instrument, copying eight bytes of the record at `$5AA9` into
the voice's arrays; and `$80`-`$BF` is a command, dispatched through the table
at `$42BA` by the four-instruction trampoline at `45:4569`:

```
4569  ASL A         command to table index
456A  TAX
456B  LDA $42BB,X   push the handler's address, high byte first
456E  PHA
456F  LDA $42BA,X
4572  PHA
4573  LDX $2E34     restore the voice cursor
4576  INY
4577  LDA ($D0),Y   the command's operand, in A for the handler
4579  RTS           ... which returns *to* the handler
```

The table holds `address - 1`, so the `RTS` at `4579` jumps rather than
returns, and the handler runs with `4598`'s own return address still on the
stack: its `RTS` lands back at `459B` and the scan continues. Forty entries are
populated, at `$430E`-`$4522`, and most are three or four instructions --
set the level, adjust the volume with a clamp, arm the sweep, retarget the
cursor. Two are worth noting: `$82`/`$92` maps to `4356`, which pops four
bytes and jumps to `$40EE`, abandoning `457A`'s frame entirely; and `$9A`,
`$9B` and `$A3` all map to `451F`, which falls into the balance ramp at
`4522`.

Once the scan reaches a note it falls into `4622`, which stores the note and
its duration and calls `46D7` to turn it into a period. `46D7` handles the
melodic case with a table lookup at `$4CB7`, and for a percussion voice
(`$2E46,X` negative) looks the note up in an instrument range table first.
If that lookup lands in the sample table at `$5B41`, the tail at `4776`
hands the channel over to the timer driver: it remaps MPR6 to the sample's
bank, copies the eight-byte header into the driver's per-channel zero page
(`$DD`/`$DE`/`$E4`/`$E9`/`$EA`/`$EF`/`$F0`/`$F5`/`$F6`, all indexed by
channel), silences the PSG channel while it does, and starts the timer at
`$0C01` if no channel was streaming yet. That is the sequencer's only write
into driver state, and the mirror image of the handshake `4142` owns.

The sample/envelope group -- `4883`, `48F6`, `48B4`, `49A2`, `49B6`, `49F9`
and `4211` -- the register programmer `4142`, the portamento stepper `4228`
and the command interpreter `457A` now run natively; `4569`, `4522`, `46D7`
and the dispatch handlers are inlined into `457A`'s transcript rather than
hooked separately. The tick at `4015`, the track loader `4BCC` and `4936`
are still emulated, and the tick will stay that way: its voice loop reaches
`4148` (`LDA $F5,X`) three times per call, so a transcript for it reads the
timer driver's channel bank bytes, and its 12,667-cycle body cannot finish
inside the 5,068-cycle timer period the shared-state guard requires.

## Palette shadow buffer

Gameplay code never writes the VCE. It maintains a shadow palette at
`$27B9`-`$2BB8` — thirty-two blocks of sixteen colours, two bytes each — plus
a dirty flag per block at `$2CCC,X` and a "something is dirty" flag at
`$2CCB`, and one routine drains that once a frame.

A colour is nine bits packed low byte first: bits 0-2 are channel zero, bits
3-5 channel one, bits 6-8 channel two, with bit 8 in the high byte. Every
routine below unpacks and repacks it the same way, through the scratch bytes
`$39`-`$3E`.

| Entry | Calls | Master cycles | What it does |
| --- | ---: | ---: | --- |
| `EB30` | 3,599 | 8,636,040 | Drains the dirty blocks to the VCE |
| `E9B6` | 672 | 6,548,946 | One crossfade step over one block |
| `EBA5` | 1,121 | 2,629,866 | Stages a source palette into one block |
| `EAA7` | 8 | 1,640,100 | Steps every channel of every block toward black |
| `E93C` | 8,969 | 758,370 | Sixteen-bit compare, `$2F/$30` against `$2D/$2E` |
| `E94D` | 1 | 138,582 | Crossfade driver; twenty-one passes, waits on `E321` |
| `EA77` | 8 | 1,455 | Fade-to-black driver; same shape |

`EB30` is the only one that touches hardware. It tests `$2BB9` and `$2CCB`,
and takes one of two paths: a whole-palette `TIA $27B9, $0404, $0400` when
`$2BB9` is non-zero and positive, or a walk of the thirty-two dirty flags,
writing the VCE address to `$0402`/`$0403` and then sixteen colours through
`$0404`/`$0405`. The dirty walk keeps its shadow-buffer read pointer in
`$47`/`$48` and its colour-table address in `$49`/`$4A`, advancing them by
`$20` and `$10` per block whether the block was dirty or not. On this route it
runs the walk 616 times, the `TIA` 29 times, and returns early 2,954 times;
the largest walk finds seven dirty blocks.

`E9B6` is the interesting one. For each of the sixteen colours it sums the
target's three channels and compares the total against the step counter at
`$2CEC`; below the counter the colour is skipped entirely, which is what
staggers a fade across its twenty-one passes. Otherwise it unpacks the
shadow colour too, computes the three differences with `SBC` and no clamp,
and increments exactly one shadow channel — whichever difference is largest,
decided by `$40` against `$41` and then the winner against `$3F`. Because
the differences are unsigned, a channel the shadow has overshot wraps high
and is treated as furthest away, which drives it back.

`EBA5` takes a block index in A, multiplies it by thirty-two into `$2D/$2E`
against a `$27B9` base, sets `$2CCC,Y` and `$2CCB`, and copies thirty-two
bytes in from `$2F/$30`. The source is always ROM on this route: `$8000`-
`$BFFF` through MPR4/MPR5.

`E93C`, `E9B6`, `EBA5`, `EAA7` and `EB30` now run natively, through seven
registered addresses. `EAA7` is 205,012 cycles per call, past the checkpoint
cap, so it chunks at `EAB0`. `EB30`'s `TIA` arm splits rather than declining:
the walk stops in front of the transfer, whose 1,024 writes are twice what the
replacement channel carries and which has no interior boundary for a chunk, the
core executes it, and `EB4D` clears `$2BB9`/`$2CCB` on the other side. The two
fade drivers are left emulated — not because they call the frame-sync spin at
`00:E321`, which a walk can stop in front of like any other `JSR`, but because
the route enters `E94D` once and `EA77` eight times, which is too few calls to
demonstrate a transcript is state-generic.

Every walk here also ends *past* its own `RTS` rather than in front of one,
which is where that technique was established. The stack is ordinary work RAM
at `$2100`, so the return address is two bytes the entry already carries: low
at `$2100 + S + 1`, high at `$2100 + S + 2`, land on that word plus one, `S`
up by two. `RTS` writes nothing, so absorbing it moves no trace — it moves
14,369 returns and 301,749 master cycles from emulated to native, and takes
these seven return addresses from 302,628 emulated cycles to 63.

Every call to `EB30` arrives with `I` set — the upload has to land inside the
blanking interval — which is why an 18,585-cycle transcript here costs the
sound driver nothing, while `E9B6` at half the length costs it 1,181 entries.

The buffer has one more producer, and it is not in this bank. `08:5977` does
`EBA5`'s job for block zero, which is the block the title, play-mode, speed and
data-load screens flash: its caller runs a four-tick timer at `$3A00,X`, flips
bit 0 of the cadence at `$3A80,X` when it expires, and calls here. What differs
is the source. `EBA5` is handed a pointer; `08:5977` builds one — `$04`/`$1E`
into `$2F`/`$30`, then `JSR $E1FD`, which maps bank `$1E` at `$8000` and `$1F`
at `$A000` and replaces the pair with the pointer it reads out of `$8004`. Only
then is the phase shifted left five times into a sixteen-bit displacement and
added on, so the two phases are thirty-two bytes apart in the same table.

It is native across two registered addresses, 177 calls each and 397,719 master
cycles between them, and it is a crossing rather than a composition: the
resolver exists to switch banks and both of its halves have been native since
the call-wrapper slice, so `08:5977` pushes the return, charges the twenty-one
cycles and ends on `00:E1FD`. `08:5982` is where that call comes back, and it
runs the arithmetic, the copy and the two flags and ends past its own `RTS` —
which closes the call `08:5977` was entered on, so it is watched there.
`dc_rom_setup_screen_palette_stager_semantic` and
`dc_rom_absorbed_return_trace_resolved` are the two regressions.

## The VDC interrupt handler and the bank-switching call wrappers

`00:E058` is the VDC interrupt vector and it runs twice a frame. It pushes the
four registers, reads the VDC status register into `$01`, and branches on bit
5: clear is the raster interrupt and jumps to `E136`, set is VBlank and
dispatches on the mode byte at `$54` — 0 to `E0EB`, 3 to `E111`. Both arms
converge on the epilogue at `E12C`, which reloads the VDC control register
from `$06` and returns through `RTI`.

The VBlank arm is where the frame's video work is issued. It programmes the
VDC ports at `$0000`-`$0003`, calls the palette upload `00:EB30`, calls the
pad reader `00:E195`, bumps the frame counter at `$222A`/`$222B`, and calls
`00:ECC6` — the wrapper that maps banks `$45`-`$47` and enters the music
sequencer. On the standard route that is 3,599 VBlank entries and 3,599
raster entries.

`00:E058` was listed here as unportable because its first act is `LDA $0000`,
the VDC status register. That was true of the instruction and false of the
routine. The read cannot be transcribed — not because the value is unknowable
(the native VDC model answers a `peek` without disturbing anything) but because
the read *is* the interrupt acknowledge, and the replacement contract has only
a write channel. So the walk splits there. `00:E058` covers the four register
saves and stops in front of the read; the core executes it; `00:E05F` picks the
dispatch up from the accumulator and hands off to whichever arm `$54` selects.
Both are admitted on all 7,198 arrivals, 640,374 of the dispatcher's 769,938
cycles.

`00:E195` was listed here beside it for the same reason, and the reason did not
hold. It strobes the joypad at `$1000` and reads it back five times a frame,
once per multitap port, shifting the low nibble of each read into `$2013,Y` and
computing newly-pressed bits into `$2018,Y` — but what comes back is a function
of the frame's pad state and of the `SEL`/`CLR` lines it drives itself, both of
which a walk can know. The port returns `$B0` over the selected pad's four
lines, active low; the store at `E198` clears `CLR` whatever it held, so the
store at `E19D` always resets the multitap cursor, and the loop steps the
cursor once per pass, which makes it the loop counter. `E195` is native and
admitted on every call. The distinction that survives is between a read whose
answer is outside input and a read whose answer is device state.

The wrappers around them are ported. There are five, all the same idiom, and
`$1E`, `$1F`, `$20`, `$21` are the zero-page shadows of MPR2 through MPR5:

| Entry | Calls | Master cycles | What it does |
| --- | ---: | ---: | --- |
| `E241` | 11,599 | 1,333,638 | Save MPR2/MPR3, map `$30` into them, `JSR $E262` |
| `E255` | 11,599 | 1,054,086 | Store the result in `$2A`, restore MPR2/MPR3 |
| `E262` | 11,671 | 706,284 | Build `$40xx` from `$2F` in `$2CBE`/`$2CBF` and jump |
| `ECC6` | 3,599 | 583,038 | Save MPR2-MPR4, map banks `$45`-`$47`, `JSR $ECF2` |
| `ECE2` | 3,599 | 424,491 | Restore MPR2-MPR4 |
| `E219` | 3,523 | 426,936 | Map the mode's fixed bank pair, indexed by `$55` |
| `E20D` | 1,861 | 135,192 | Map `$30` into MPR4/MPR5 |
| `ECF2` | 3,599 | 129,564 | Write `$4003` into `$47`/`$48` and jump through it |
| `E200` | 1,861 | 123,666 | Read the `$8000` reference table through that slot |
| `E1FD` | 1,861 | 39,081 | One instruction: the `JSR $E20D` above |

Every callee is entered at `$4000 + mem[$2F]`, so each bank begins with a jump
table of up to 128 entries — which is why `02:4088`, `08:4030` and `45:4003`
all sit in the first page of their bank.

All ten run natively, admitted on every call. They are the first transcripts
in the port to change the bank mapping: `TAM` sets `registers.mpr[slot]` and
costs fifteen master cycles, and the core applies the slots at every
checkpoint. Every walk also makes its own call rather than stopping in front of
one — `E252`, `E26E`, `ECDF`, `ECFA` and `E1FD`, 32,329 of them at 21 master
cycles each. Two are indirect jumps, and each reads a vector the same walk
wrote two instructions earlier; the target is always `$40xx`, so the bank is
MPR2, which the prologue mapped.

`E1FD` is the last row of that table and the smallest entry in the port: one
instruction, whose whole body is the call. It is registered for that reason
and no other. Three walks end on it — `08:597F` in the setup-screen palette
stager and `00:F2A8`/`00:F35C` in the tilemap upload — and until it was an
entry all three were crossing onto an address the core had no hook for, which
made them cost twenty-one cycles for nothing. What an entry is worth is what
its callers stop interpreting, not how long its body is.

`00:ED5E`, the fifth wrapper, is deliberately absent: it is entered with two of
its four stack slots already pushed by its caller, so its entry contract is not
its own.

The handler's own body is ported too. Nothing below the `LDA $0000` needs the
device again, so the walks split at that instruction, at the dispatcher's jumps
and at the four calls it makes — not in front of those calls but past them, on
the first instruction of the callee:

| Entry | Calls | Ends on | What it does |
| --- | ---: | --- | --- |
| `E058` | 7,198 | `E05C` `LDA $0000` | Push `P`, `A`, `X` and `Y`, then hand the status read back to the core |
| `E05F` | 7,198 | `E136`, `E0EB` or `EB30` | Stash the status byte at `$01`, test the VBlank bit, pick the arm from `$54` |
| `E0EB` | 2,748 | `EB30` | Programme VDC `$08`/`$07`/`$05` from the shadows at `$02`, clear the raster-compare bit, cross the `JSR $EB30` |
| `E114` | 3,599 | `E195` | One instruction: cross the `JSR $E195` the palette upload returns onto |
| `E117` | 3,599 | `ECC6` or `E134` | Bump the frame counter at `$00`; on `$D9` set, write `$DA` and skip the music call, otherwise cross into it |
| `E123` | 3,212 | `E134` | Restore MPR6 from the stack and join the epilogue |
| `E136` | 3,599 | `E134` | Raster arm: on `$54` of 0 or 2 reprogramme the split from `$0B`/`$0C`, then tick `$222A`/`$222B` |
| `E179` | 387 | `ECC6` | The secondary handler's prologue, across the same call `E117` crosses |
| `E183` | 387 | `E193` | Its epilogue: restore MPR6, rewrite `$0000` twice, unwind |
| `E195` | 3,599 | `E1F7` `RTS` | Poll all five ports; publish held state at `$2013,Y`, last frame's at `$200E,Y`, edges at `$2018,Y` |

`E179` is reached only on the frames where `$D9` suppresses the VBlank arm's own
music call, which is why its 387 entries match `E128`'s exactly.

The four calls are `E111` into the palette upload, `E114` into the pad reader,
`E120` into the bank wrapper and `E180` into that same wrapper from the
secondary handler. Every one of them was a stop for as long as this family has
existed. Each is now crossed instead: the transcript pushes the two return bytes
itself, at the offset `JSR` writes them, charges the same twenty-one cycles and
ends on `EB30`, `E195` or `ECC6`. All three targets are registered entries of
their own, so the core enters each natively rather than interpreting a call on
the way in. `E114` is an entry only so that the second of the four can be crossed
at all: between the palette upload's return and `E117` the core would otherwise
find no hook, and the crossing above it would have bought nothing but its own
twenty-one cycles.

Each walk stops one instruction short of its `RTI`, in front of the `PLP` at
`E134` or `E193`. That is not the usual "let the core execute the return": the
core samples the IRQ at `LastCycle`, which for `PLP` runs before the pop, so a
transcript covering it would sample with the restored mask rather than the one
the instruction ran under and swallow an interrupt assembly services between the
`PLP` and the `RTI`.

The handler is entered masked and does not stay masked. Bank `45` ends three
critical sections with an unconditional `CLI` instead of restoring the caller's
mask — `45:4872`, `45:49F2` and `45:47EE` — so the music tick below `ECC6`
returns with interrupts enabled. Counting interrupt entries against the stack
page in an assembly trace puts 78 inside these six walks, all at or after `E123`.

## Camera-visibility sweep

Before the actor loop runs, `00:F4F2` walks all 127 slots and calls `00:F4FC`
on each to decide whether the slot is on camera this frame:

```
F4F2  CLX
F4F3  JSR $F4FC     per-slot visibility test
F4F6  INX
F4F7  CPX #$7F
F4F9  BNE $F4F3
F4FB  RTS
```

`00:F4FC` compares the slot's position at `$3300:$3380,X` against the camera
anchor `$A9:$AA` biased by `$F0`, and sets or clears bit 7 of `$3080,X`
accordingly. It has three returns: `00:F523` clears the bit for a slot that is
not presented at all, `00:F545` clears it for a slot above the camera, and
`00:F54E` returns after `00:F546` has set it for a slot below. That bit is what
`01:C510` dispatches on and what `08:407C` checks before emitting slot `$00`.

`00:F4FC` is also called from outside this loop; the sweep's replacement reads
the frame under the stack pointer to tell the two apart.

## Main actor loop

`01:C4DE` sets up the per-frame actor pass and `01:C510` is its head. The
preamble decides the frame's actor enable mask, resets the three cursors the
pass allocates its queue entries from, and clears the slot cursor:

```
C4DE  LDA $2D35     frozen: nothing is enabled
C4E1  BNE $C4FC
C4E3  LDX $2251     the current side
C4E6  LDY <$13,X    its mask
C4E8  LDA $2254     ... or the mask staged in <$B7 instead
C4EB  BEQ $C4F1
C4ED  LDY <$B7
C4EF  BRA $C4FD
C4F1  LDA $2CC7     ... and $2CC7 or bit 3 of <$53 clears it
C4F4  BNE $C4FC
C4F6  LDA <$53
C4F8  AND #$08
C4FA  BEQ $C4FD
C4FC  CLY
C4FD  STY $2CEE     the mask 01:D466 tests each slot against
C500  STZ <$56
C502  STZ $2CBA     $2BBA from the bottom
C505  STZ $2CBC     $2C3A from the bottom
C508  LDA #$7F
C50A  STA $2CBB     $2BBA from the top
C50D  CLX
C50E  STX <$4D      the slot cursor
```

`$2CEE` is a mask rather than an index: `01:D466` loads it and ANDs it against
each slot's `$3B80,X` attribute byte, so a zero there suppresses every slot for
the frame. `$2CBA` and `$2CBB` walk the single 128-entry queue at `$2BBA` from
its two ends, which is why one starts at zero and the other at `$7F`; `$2CBC`
indexes the separate queue at `$2C3A`. `01:C539` is a shorter variant of the
same preamble with only the `$2D35` gate.

The loop walks slots `$00-$7F` through the cursor at `$204D`:

```
C510  LDA $3000,X   slot state; positive means idle
C513  BPL $C523
C515  LDA $3080,X   attributes
C518  BMI $C51E     camera bit set: dispatch
C51A  AND #$02
C51C  BNE $C523     suppressed
C51E  BSR $C52C     dispatch: vector out of $3F00/$3F80, JMP ($202F)
C520  JSR $C606     file the slot into one of the SAT queues
C523  INC $4D       tail: advance the cursor
C525  LDX $4D
C527  CPX #$80
C529  BNE $C510
C52B  RTS
```

`01:C52C` copies the slot's handler vector out of the `$3F00`/`$3F80` jump
table into `$2F`/`$30` and jumps through it; the handler returns to `01:C520`.
`01:C606` is the queue filer described under the SAT presentation below, and
its `RTS` at `01:C654` returns to the tail at `01:C523`.

Every instruction listed here runs natively, the preamble included. Its walk
ends by falling through into `01:C510` rather than by calling it, so the machine
steps from one transcript into the next with nothing pushed; only `01:C4ED` and
`01:C4EF` are left to the core, and they have not executed in 3,600 frames. The
walk at `01:C510` absorbs the tail for each slot it skips, so `01:C523` exists
as a separate replacement only for the arrivals a dispatch hands back.

The collision sweep at `01:CB48` has the same shape: a walk that rejects most
slots inline, the vertical scan at `01:CC1A` for the rest, the `BCC` at
`01:CB61` that resolves the result, and the shared tail at `01:CB6B` that
advances the cursor at `$2064` and the countdown at `$2060`. A set carry means
a hit and sets bit 2 of the latch at `$2077` before the helper at `01:CBCF`.
The call out to the scan is composed into the walk rather than handed to the
core; see the end of the next section.

### The collision frame around the sweep

The code on either side of that sweep is native too, as of the collision-frame
slice. Five routines in physical bank `01`, none of which reads a device
register:

| Entry | What it is | Calls | Native cycles |
| --- | --- | ---: | ---: |
| `01:CD74` | Playfield-tile probe | 3,233 | 2,531,280 |
| `01:CB2A` | Sweep setup, falling into `01:CB48` | 3,233 | 455,853 |
| `01:CA84` | Ball/table gate head | 2,649 | 291,759 |
| `01:CAA1` | After the first `JSR $E93C` | 2,315 | 221,076 |
| `01:CB11` | After the sixth `JSR $E93C` | 2,121 | 132,687 |
| `01:CB8A` | The `BBR`'s clear arm | 2,934 | 105,624 |
| `01:CB04` | After the fifth `JSR $E93C` | 2,121 | 89,082 |
| `01:CBCF` | Confirmed-contact dispatcher | 270 | 48,054 |
| `01:CBB6` | After `JSR $E219` | 980 | 44,100 |
| `01:CB79` | After `JSR $CD74` | 3,233 | 36,888 |
| `01:CB74` | The `BBS`'s fall-through arm | 2,986 | 35,832 |
| `01:CBAB` | Single-slot probe head | 980 | 32,340 |
| `01:CAB6` | After the second `JSR $E93C` | 194 | 24,444 |
| `01:CBC2` | After `JSR $CC1A` | 980 | 17,526 |
| `01:CB81` | The `BBR`'s set arm | 299 | 10,764 |
| `01:CAD3` | After the third `JSR $E93C` | 194 | 9,012 |
| `01:CB76` | One instruction: the `JSR $CD74` below | 247 | 5,187 |
| `01:CAED` | After the fifth `JSR $E93C`, upper arm | 42 | 2,400 |
| `01:CAE0` | After the fourth `JSR $E93C` | 50 | 2,196 |
| `01:CBCA` | After `JSR $CBCF` | 19 | 456 |

`01:CD74` is the playfield-tile probe and the largest of them by far. It takes
the ball position in `$65`/`$66`/`$67`, splits the horizontal byte into a tile
column (`>> 3`) and a column within the tile (`& 7`), does the same to the
vertical position less `$20`, and forms `row * 32 + column`. That offset is
biased by the two table pointers at `$C8`/`$C9` and `$C6`/`$C7` into two
parallel maps: `($2D)` yields the tile id, `($2F)` its attribute. The attribute
times eight, biased by `$90`/`$91`, addresses an eight-byte solidity mask; row
`$5F` of that mask ANDed with the one-bit column mask at `$CF2B,Y` decides the
result. Solid publishes the tile id in `$78` and returns carry set at
`01:CDF7`; empty clears carry at `01:CDF1`. It makes no calls at all.

`01:CA84` is the ball/table gate: it returns immediately unless `$3C80` is zero,
then runs six 16-bit range tests through the compare helper at `00:E93C` and
writes `$3D00,X` with bit 7 set or clear. Its seven `JSR $E93C` return addresses
are why it needs eight entry points.

`01:CBAB` is a one-shot version of the sweep for a slot the caller already
knows, and `01:CBCF` is the confirmed-contact dispatcher described under the
normal-table collision closure route below. Its `PHA`/`PHA`/`RTS` tail jump at
`01:CBF1`–`01:CBF9` is ordinary for a transcript: the two pushes are transcribed
and the core executes the `RTS`, which jumps through the vector exactly as it
would have.

Two instructions in the region are deliberately left to the core. `01:CB7E` is
a `BBR` and `01:CB71` a `BBS`, and the tilemap slice established that neither
can be a checkpoint — they are `LD_ZP` wrapping `JR`, so the machine crosses
them in four clock steps where a checkpoint crosses in two, and the core tests
`next_event` on those steps. `01:CB74`, `01:CB76`, `01:CB81` and `01:CB8A` are
entry points precisely because the core resolves those branches itself —
`01:CB74` is the `BBS`'s fall-through arm and `01:CB76` the arm it takes.

All twelve calls the region makes are crossed rather than stopped in front of.
A transcript pushes the two return bytes, charges the twenty-one cycles the
`JSR` costs, and ends on the callee's first instruction, so the core arrives
there instead of interpreting its way in. That is 263,907 master cycles over
12,567 calls, and it is why the eleven return addresses above are still entry
points for the plain reason that a callee's `RTS` lands on them, rather than
because a walk stopped short. Seven of the twelve go to the same place, the
compare helper at `00:E93C`.

`01:CB7B`, the `JSR $CF33` on the contact path, is the one that corrected the
rule. `01:CF33` is not a registered routine and is still interpreted
instruction for instruction, and the workflow notes said for two slices that a
crossing onto such an address buys nothing. It is worth 6,678 over 318 calls,
21 apiece, which is what every crossing pays: the conversion is of the `JSR`
itself and happens whatever is on the far side. `01:CB76` is the other
correction, from the other direction. It is one instruction and was worth
registering because 247 of its 3,233 arrivals come through the `BBS` at
`01:CB71` rather than through `01:CB74` — the crossing was already installed
two bytes earlier and those arrivals went round it.

What was left emulated in the region was almost entirely the call boundary: the
`JSR $CC1A` at `01:CB5E` and its `RTS` at `01:CC40`, crossed 84,735 times for
3,592,728 master cycles even though both sides are native — 42 cycles per
crossing, because a transcript cannot contain a call.

The sweep now contains it. `01:CB48` transcribes the `JSR`, runs the scanner's
own replacement on a work RAM staged with its own writes, splices the result at
the cycle the call landed on, transcribes the `RTS`, and carries on to
`01:CB61`. The two instructions are 17,622 and 19,776 cycles now, and what is
left of them is the hit path: the scanner leaves from `01:CCD2` or `01:CCD4`
when a contact is confirmed, and a parent cannot transcribe a return that does
not happen, so those 1,810 calls still cross the boundary and `01:CC1A` stays
installed for them.

## Fixed-actor SAT presentation

Physical bank `$08` owns the normal-table actor-to-SAT presentation path.
`08:4036` selects VDC register 0, begins at the existing sprite count in
zero-page `$58` (copied to `$4E`), and points the VDC memory-write address at
`$7800 + ($4E << 2)` in the 256-word staging table at VRAM `$7800-$78FF`. It
then submits slots in four stages, each calling `08:40DF`:

| Stage | Source | Cursor | Call site |
| --- | --- | --- | --- |
| `08:4056` | `$2C3A,Y` | `$2CBC`, drained to zero | `08:4060` |
| `08:4068` | `$2BBA,Y` | `$2CBA`, drained to zero | `08:4072` |
| `08:407A` | slot `$00` | — | `08:4085` |
| `08:4088` | `$2BBA,Y` | `Y` from `$7F` down to `$2CBB` | `08:4094` |

Slot `$00` is emitted only when `$3080` has camera bit 7 set and hidden bit 5
clear. The third stage preserves `Y` across its call with `PHY`/`PLY`, since
`08:40DF` clobbers it. `08:409B` then re-points the write address past the
entries just emitted and falls into the tail clear at `08:40C3`, unless `$4E`
has reached `$40`, in which case `08:40A6` skips straight to the `RTS`.

Every instruction of this driver runs natively except the four `JSR $40DF`
call instructions and the closing `RTS`.

The queues it drains at `08:4056` are filled by `01:C606`, which the main
actor loop calls once per dispatched slot after the handler returns. It reads
the slot's attribute byte at `$3080` and files the slot index into one of
three queues, or drops it:

- bit 5 set: not emitted at all.
- bit 2 set: `$2C3A`, cursor `$2CBC`, filled upward.
- bit 3 set: `$2BBA`, cursor `$2CBA`, filled upward.
- neither: the same `$2BBA` queue, cursor `$2CBB`, filled *downward*.

The emitter drains all three downward — the first two from their own cursors,
the third from a fixed `$7F` — so the first two are emitted in reverse
registration order and the third, whose cursor counts down, in registration
order. Bit 0 diverts the slot to a one-shot clear of `$3800` bit 6 and the
helper at `01:F605` before any queue is chosen.

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

`00:E41F` is also native as a CPU transcript now, together with the two clears
that share its idiom, and the region `00:E3C0`-`E47C` is closed apart from a
one-call stub at `00:E3E7` that jumps into the frame-sync loop. It is the first
family in the port whose transcripts are **chunked**: one call is 167,000
master cycles and 3,400 port writes against a 512-write channel, so each
routine has a second entry point at the head of its loop and a transcript stops
in front of the backward branch that closes it.

| Entry | What it is | Calls | Master cycles |
| --- | --- | ---: | ---: |
| `00:E3C0` | Clear 512 VRAM words from `$7800`, through `$0002`/`$0003` | 2 | 20,736 |
| `00:E3D7` | that clear's loop head | 4 | 25,524 |
| `00:E3FC` | Clear the 1,024-entry VCE colour table through `$0404`/`$0405` | 4 | 37,704 |
| `00:E403` | that clear's loop head | 16 | 128,136 |
| `00:E41F` | The decoder's head: program MAWR from `$2D`/`$2E`, select VWR | 41 | 813,204 |
| `00:E439` | The stream-record loop head, where every continuation resumes | 326 | 5,883,540 |

The 41 calls decode 7,480 records and 57,648 words on the census route, in
literal records of at most 127 words and repeat records of at most 127 — the
`$80` command that would mean 256 never occurs. Sources resolve through MPR4
and MPR5 for the most part, with pointer high bytes spanning `$80`-`$BF`, and
the walk resolves each byte through the live `MPR` rather than assuming the
slot, because a stream that runs past `$BFFF` continues through MPR6.

`$2F`/`$30` is shared with the rest of the game — 340 instructions across eight
banks write it — so the transcripts depend on nothing writing it under them.
Measured on an assembly write trace, bracketing each call between its header's
`$06` store at `00:E422` and its last data-port write: all 23,300 writes to
`$2D`-`$30`, `$37` and `$38` inside those windows are the decoder's own.

### Background tilemap upload

Physical bank `$00`, logical `$F261`-`$F4F1`, is the other half of the graphics
driver's port work: where `$E41F` streams a prepared word list, this decides
which row of the scrolling tile table has just come into view and pushes it. It
is two routines.

`$F3E7` is the row writer. `$2D:$2E` is a VRAM word address, `$2F:$30` an
MPR-mapped tile-index source and `$2B:$2C` a work-RAM attribute array based at
`$2259`. It programs MAWR, selects VWR, and walks `Y` from `$00` to `$3E` in
twos writing 32 words: the low byte is a tile index straight from the source,
the high byte the next index with the tile's attribute nibble folded into its
top four bits. `Y` halved is the word index and halved again the attribute byte,
which packs two of them; `BBR0 $27` at `$F423` picks the nibble by parity. It
returns early when `$2F:$30` still equals the `$A4:$A5` of the previous upload,
which never happens on the census route.

`$F261` is the scroll driver. It subtracts the camera position `$A9:$AA` from
the scroll limits at `$3300`/`$3380`, and the difference decides whether a row
has come into view below the camera, above it, or neither. Either arm resolves
the tile source through `$E1FD` — which maps the reference's high byte into
MPR4/MPR5 exactly as the sprite-stream path above does — builds the three
addresses by shifting the tile row left three times through `$2E`, and calls the
writer once, or twice when the movement spanned two rows. The tails at `$F43D`
and `$F48D` fold the movement into `$0A:$0C` and `$A8:$AA`, and `$F4DF`
publishes its negation in `$35:$36` for the camera-visibility sweep, which
`$F261` then falls through into at `$F4F2`.

All ten entry points are native, admitted on every call:

| Entry | What it is | Calls | Master cycles |
| --- | --- | ---: | ---: |
| `00:F261` | Scroll driver head; ends on `$F28F` or, across `$F2A8` or `$F35C`, on `$E1FD` | 2,649 | 619,371 |
| `00:F2AB` | After `JSR $E1FD`, the downward arm | 286 | 137,280 |
| `00:F301` | After the first `JSR $F3E7` | 286 | 140,214 |
| `00:F330` | After the second `JSR $F3E7` | 5 | 2,250 |
| `00:F35F` | After `JSR $E1FD`, the upward arm | 232 | 114,144 |
| `00:F3B5` | After the third `JSR $F3E7` | 232 | 111,216 |
| `00:F3E4` | After the fourth `JSR $F3E7` | 16 | 7,488 |
| `00:F3E7` | Row writer head, to the first `BBR` | 539 | 176,253 |
| `00:F426` | `BBR`'s odd-nibble arm, and the rest of the iteration | 8,624 | 2,009,931 |
| `00:F42A` | `BBR`'s even-nibble arm, and the rest of the iteration | 8,624 | 1,862,784 |

Six of those are just the return addresses of the driver's calls, one per `JSR`.
None of those six calls is stopped in front of any more: each is crossed, four
onto `$F3E7` in the same table and two onto `$E1FD`, the asset resolver's
one-instruction head, which became an entry of its own in the same slice.
22,197 master cycles between them. `$F426` and `$F42A` are there for a different
reason: `BBR` is `LD_ZP` wrapping `JR` in the core, so it samples the interrupt
twice, and a checkpoint carries one sample and one clock jump. Handing `$F423`
back keeps the core's event phase exact and leaves 365,664 of the region's
476,079 still-emulated cycles.

`$2B`-`$30` and `$A2`-`$AA` are shared with the rest of the game. Measured
against 2,649 exact call windows from a `00:F261` observer, 92,978 writes inside
them are the routine's own; the rest are interrupt frames below the live stack
pointer, plus four stores to `$20` from the VDC handler's bank wrapper, which
this region writes only below the `$E1FD` it crosses into and never reads.

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
phase's SAT metadata. Decoded pixels retain VCE palette indices whose colors
are resolved at composition time. They participate in background priority;
the live SAT retains the hardware 16-half scanline rule, while retained
sprites from separate camera visits do not suppress one another. Type,
appearance, phase, or dispatch changes invalidate the seed.

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

The active record loop has two native transaction boundaries around the
per-record loader:

| Entry | Calls | Native body | Resume |
| --- | ---: | ---: | --- |
| `02:4090` | 89 | 88 x 33 cycles, one x 39 cycles | `02:4096` or terminal `02:40A7` |
| `02:4099` | 88 | 78 cycles | Registered head `02:4090` |

`02:4090` resolves `[$2F]` through the live MPR mapping, so the replacement
reads the same user-ROM byte as the cartridge before comparing `$FF`.
`02:4099` advances the little-endian pointer by twelve with the original
`CLC/LDA/ADC/STA/CLA/ADC/STA/BRA` instruction boundaries and both writes to
`$2F/$30`. The family regression preserves the exact artifacts:

- Record-loop entry SHA-256:
  `46865e9d5f8c4f978af84492f29c2e9700b9ca231425386f6e60a9d06815d47a`
- Overlapping call SHA-256:
  `4992a30f04799441c1fbd6ed9992864dc483d797f325a5b8dd6f5aa9f7b7bad3`
- Scoped-write SHA-256:
  `c6cf2e3a04d4ec5904acf7562a85c776e4226e354ab1fbfbe8cdfd1d2a5153ad`

The two entries own 9,807 exact native cycles.

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

The record loader is now native in two additional pieces that retain both
callee families as explicit composition boundaries:

| Entry | Calls | Native body | Resume |
| --- | ---: | ---: | --- |
| `02:40F6` | 88 | 33 cycles | Original `JSR $C027` slot reset |
| `02:40FE` | 88 | 426 cycles, nine writes | Record-selected initializer |

The suffix reads bytes 2-11 through the live MPR mapping, writes the two-byte
initializer pointer and seven indexed actor fields, and resolves the final
indirect jump from the newly written pointer. Its immutable input snapshot is
therefore never mistaken for the transcript's pending scratch writes. The
focused family preserves exact assembly/native artifacts:

- Record-loader entry SHA-256:
  `6bdca3670ba4385a2bd5c375a085a2d23af78ff9c8cc081ea36202e345a16a12`
- Overlapping call SHA-256:
  `fcd226a00fb51d7f211d83a1738fc23e39e5144b1b0e088a2481ebe4e5b37c3e`
- Scoped-write SHA-256:
  `fb50c6170d5e1ea7129538607eade76d1718eb2a22f27f638fd94639f0e8240c`

These entries own 40,392 exact native cycles. The seams around them are native
too:

| Entry | Calls | Native body | Resume |
| --- | ---: | ---: | --- |
| `02:4096` | 88 | 21-cycle `JSR $40F6`, two stack writes | `02:40F6` |
| `02:40FB` | 88 | 21-cycle `JSR $C027`, two stack writes | `01:C027` |
| `02:40A7` | 1 | 21-cycle terminal RTS | Banked-call caller |

The JSR replacement and assembly routes have identical canonical census, work
RAM, 176-entry trace, 176 complete call scopes, and 510,770 scoped writes:

- Entry SHA-256:
  `9e1e74965b2f98d5004ce1f13c4c52b1142b408800928fb81c34f0c4847b3997`
- Call SHA-256:
  `1b626258404f1807e3129fd5ff966c3223003f072fcb5b8f762ba8f154d1092c`
- Scoped-write SHA-256:
  `7c63133c260e7de316dc066b206e76b040cc681fdb18c2627bf5ff600d2d319e`

The terminal RTS preserves the exact assembly entry trace
`649c39b34b73221b20bc54a5c8a339feb30f78775bc0677b1cb47a25bda5c2df`
and declares its absorbed return at the original instruction boundary. These
three entries own the final 3,717 local cycles and complete the normal-table
scene-layout loader family.

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
| Fixed slot `$13` (Dragon's Head) | `5A31` (native setup) | (64, 510) | `5A4B` | `$2E` |
| Fixed slots `$14/$15` (Bumpers) | `789A/78A4` | (190/74, 604) | `78C7` | `$20` |
| Fixed slots `$26/$27` (Front Spiders) | `646B` | (64/216, 636) | `6490` | `$34` |
| Fixed slot `$2D` (Tower) | `60B3` | (144, 538) | `60C3` | `$33` |
| Fixed slot `$30` (Giant Skeleton) | `5EE4` (native setup) | (224, 518) | `5EEC` | `$2E` |
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
| Tower-insect source | `$2C` | `02:5F22` | (140, 526) | `$82` | `02:5F4A`, then `02:5F5F` |
| Tower | `$2D` | `03:60B3` | (144, 538) | `$83` | `03:60C3` |

The insect source clears the live-child count at `$2D2B`, initializes a
four-spawn countdown, and uses `BSR 02:5F78` to allocate children. Each child
copies the source position and collision fields. When `$2D2B` reaches zero,
the source declares a 200-unit (20,000 displayed point) group-clear award.
The child's two-hit behavior is bounded separately below.

Semantic `02:5F22@all` now covers the source's one fixed-slot setup call at
frame 872. It preserves the exact 192-cycle, seven-write original scheduler
transaction and final `01:C5E6` checkpoint; the countdown, allocation, and
group-clear states remain assembly-backed.

The Tower initializer clears ten-step progress `$2D3F`, initializes the
per-actor state and a five-step global gate, and falls into `03:60C3`. The
fixed setup `03:60B3@all` now executes natively through its exact 249-cycle,
ten-write transcript and final `01:C5E6` scheduler checkpoint. Its later
collision controller remains within the existing native-or-assembly split.
The
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
`$2CFC`; later qualifying passes decrement `$2D4D`. When that decrement
reaches zero, `03:60F9` branches directly to `03:612B` with X=`$2D`; the
Guard's `$8E` selector is not that call source. `612B` clears Tower collision
`$382D` and installs `03:6136`, which waits for the next Tower collision.
Semantic `03:60F9@all` covers only this armed final-gate branch through its
exact 54-cycle, four-checkpoint transcript. It keeps A at `$01` while
`DEC $2D4D` commits the zero gate at cycle 42, then resumes original `612B`.
Semantic `03:6136@all` also covers the scheduled no-hit return: the indexed
collision load and not-taken branch consume 21 cycles and resume original
`RTS` at `613B`. Its first checkpoint is after the load at cycle 15, which
preserves the observed frame-3413 timer IRQ return PC of `6139`. A real
geometry-delivered sign-bit collision takes the separate exact 75-cycle
prefix: it sets slot `$2D` stage flag `$3C80` to one and transfers at original
`01:C021`; the shared timer, audio, and scene-transition dispatcher remains
assembly-owned.
Only that later negative latch takes `6136`'s stage-selection prefix, which
invokes the table bonus helper with `$10`, selects bonus stage 5, sets the
session-transition mode, emits command `$41`, restores `$2D4D` to 5, and sets
`$2D2D` for the next Tower/insect cycle. A Guard mask hit instead returns to
the ordinary ball-response dispatcher.

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

Semantic `03:789A/78A4@all` now owns the two one-shot layout transactions:
the left `$14` setup is an exact 342-cycle, fourteen-write transcript and the
right `$15` setup is the mirrored 336-cycle, fourteen-write transcript. Both
preserve their original side/type, collision, phase, appearance, stack, and
`01:C5E6` scheduler writes before the existing `03:78C7` dispatcher begins.

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

Semantic `03:78C7@all` replaces all 3,468 calls. The 3,457 no-hit calls
retain the exact two-instruction path: `LDA $3800,X`, taken `BPL`, then the
RTS at `03:78E6`. Its two-checkpoint transcript preserves 27 body cycles,
zero direct writes, the loaded accumulator and N/Z flags, and every one of
the 38 original idle interrupt-handler passages. Each of the 11 collision
paths now clears `$3800,X`, increments `$3E80,X`, resolves the nine-entry
handler table, and hands assembly the selected `03:78E7` or `03:7901`
handler through a 13-boundary, 165-cycle, four-write transcript.

Shared score, audio, appearance, and scheduler helpers remain in assembly.
After those helpers, semantic `03:78F5@all` performs all nine routed primary
counter increments through exact two-boundary, 36-cycle, one-write
transcripts. Semantic `03:7912@all` performs both routed secondary
counter/appearance/collision updates through nine-boundary, 114-cycle,
three-write transcripts; the second call at frame 3,342 is the normalized
replay fixture. Counter two still falls back to the original ninth-hit
destruction award and six-child release.

The semantic Release and Debug routes preserve the outer call and write
hashes below; their census hashes are respectively
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
and
`a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`.
The focused 11-call/953-write continuation trace has call SHA-256
`1225992b04220cdea996a4a1a11e3f67be24ef5dd5a42e0ae1d6039ded66b5ce`
and write SHA-256
`99674df0a29df6a45f2eb4e32c8d3f8dd0fe423fb30b699c6828825af7cb7b29`.

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

### Devil-Shooter initializer extension: semantic `02:50EE@all`

The fixed slot `$0E` setup now executes natively through its exact 351-cycle,
15-write transcript. It installs the five-tick actor timer, live position
`(264,671)`, `(264,632)` reference position, appearance four, `$2D15=$27`,
and the `02:5116` idle script. The typed adapter accepts only the authentic
slot-zero-equivalent layout call (`A=$00`, `X=$0E`, `Y=$0B`, status `$02`) and
preserves the final `01:C5E6` scheduler boundary at cycle 351. The focused
Release regression replays frame 871 and retains the baseline census hash
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`, call
hash `dbe6eacd9f123091ef9e511c2b8e93feef1211b9c75f46aad65cfd95253db86e`,
and write hash `b6004eb4a6b2ebcc1f96a9cb41d7847ba9f9adc53239c3b2d1e3fd40b737e8b6`.

The deterministic route bounds the first service cycle across two dispatcher
states:

1. `02:5116` runs 125 times. Its first 124 ordinary calls are 27-cycle,
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

Semantic `02:5116@all` replaces all 125 checks through two exact,
two-instruction paths: `LDA $3800,X` plus taken `BPL` reaches the RTS at
`02:5123` after 27 body cycles, while the untaken catch branch reaches
`02:511B` after 21. Both paths have zero direct writes and preserve the
loaded accumulator and N/Z flags. The 124 idle paths now absorb the 21-cycle
RTS at `02:5123`; the frame-1,064 catch resumes assembly for
the occupied latch and shared `00:C087` ball-position/audio helper. After
that helper's original interrupt-bearing scope returns, semantic
`02:52B8@all` writes `$3180=$0E` and `$3200=$01`, reproduces both JSR stack
writes, and composes native `01:C5C9` to install `02:52C3`. Its one routed
call is exact at 228 cycles and nine ordered writes. The semantic Release
and Debug routes preserve the 125-call and 116-write outer hashes below;
their census hashes are respectively
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
and
`a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`.

Semantic `02:5159@all` covers 100 of 101 calls: the four released-input delay
returns at frames 1,181--1,184 and all 96 held-Button-I prefixes. The delay
path preserves its exact eight-checkpoint, 96-cycle body and absorbs the
21-cycle RTS at `02:5178`. The held path clears bit 7 at `$3800,X`, clears
collision scratch `$BD`, and reproduces both original call frames. The first
four zero-charge calls hand command `$3D` to original `01:C087` and resume at
semantic `02:518D`; every held call hands charge positioning to the separately
observed `02:52E3` child and resumes at semantic `02:5197`.

`02:518D` reproduces the 48-cycle load/compare/`JSR $52E3` continuation on
all four audio returns. `02:5197` covers all 96 post-charge calls through the
75-cycle ordinary tail (and the state-derived 105-cycle full-charge variant):
it clears absolute ball appearance `$3980`, preserves the indexed Shooter
appearance at `$3980,X`, and hands shared presentation back at `02:5475`.
Alternate table mode and the one launch call remain assembly-backed. The
owning Release and required Debug Devil-Shooter regressions preserve the
existing census, complete 101-call/8,883-write outer artifact, work RAM,
trace, and actor state. The three entries add exactly 26,292 native cycles.

Semantic `02:5475@all` covers all 107 shared appearance submissions from the
fixed Shooter. Its 66-cycle prefix pushes the live appearance byte, stores
asset reference `$12:$10` in `$2F/$30`, reproduces the exact `JSR $C042`
frame, and hands the call to the existing bank-01 jump-table and asset-resolver
transcripts. Semantic `02:5481@all` resumes after that child on all 107 calls,
pulls the appearance, multiplies it by 32 into `$2E:A`, offsets the resolved
pointer, and jumps through native `01:C05D` to native palette stager
`00:EBA5`. The pair owns exactly 31,458 native cycles while preserving the
existing family census and complete outer artifacts.

Semantic `02:52E3@all` covers all 252 charge-position calls from the fixed
Shooter's held-button and post-launch-animation callers. It carries the
three-byte coordinate through `$3280,X`, `$3300,X`, and `$3380,X`, derives
charge at `$2D15`, and selects the resulting appearance at `$3980,X`. Every
original instruction is checkpointed: the ordinary five-store paths remain
243, 261, 279, or 297 cycles, while the fully drawn shooter follows the
original 390-cycle/eight-write clamp through `$3E00,X/$3E80,X` and charge
`$27`. All 21 event/IRQ-bearing calls now resume at their authentic boundary.
The owning regression replays `02:52E3@1` at frame 1,085 and preserves the
complete call/write artifact and canonical census.

Semantic `02:533A@all` absorbs the charge-position controller's shared `RTS`
on 231 of 252 natural calls. Each accepted leaf costs the original 21 master
cycles and returns through the live stack frame. The other 21 calls have a
serviceable timer interrupt before the return and deliberately fall back to
assembly; servicing it after the `RTS` changes the observer's ordered-write
scope even though final RAM is unchanged. Enabling the leaf leaves the owning
family's existing census, RAM, call, write, trace, and actor-state hashes exact.

Semantic `02:5249@all` composes the held-charge controller's real
`JSR $52E3` frame and its checkpointed charge-position child on all 156
entries. It owns the 155 ordinary compare/return paths plus the one
`$2D15=$27` path that clears `$3800,X.7` and composes the shared scheduler to
install `02:525F`. Registered continuation `02:524C@all` remains available but
receives no independent route entry now that the parent crosses every child
horizon. Its proof therefore runs independently with the parent left in
assembly and now admits all 156 natural continuation entries, rather than only
the 70 formerly exposed by a partially admitted parent. Release and Debug
share census
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
while the parent-only regression pins 156 calls/1,302 writes and the
continuation-only regression pins 156 calls/48 writes. Their call hashes are
`2f25467a0ce012580d48b027e69ef1d661874b2ac47a588bc6921fbcb230255d`
and
`b74f8f871f0ccad157b962a0ef9fb5242a7764fc580f960e9ae7fa338cc3bcd1`.

Semantic `02:533B@all` covers all 32 charge-retraction calls from the catch
and launch-controller callers. It preserves every instruction boundary, the
three coordinate stores, remaining-charge store, and conditional appearance
store exactly. The transcript also validates and pops the live caller frame,
absorbing terminal `RTS` `02:5393` before resuming at catch continuation
`02:52C6` or launch continuation `02:520B`. Its 21-cycle checkpoint preserves
the return's original LastCycle interrupt sample, while absorbed-return
metadata keeps the call trace closed at `02:5393` exactly as assembly does.
The nonzero appearance paths take 273, 291, 309, or 327 cycles; the final
zero-charge path returns after four stores and 234 cycles. A coordinate that
crosses its configured base takes the 303-cycle/seven-write clamp path. All
five original interrupt-bearing scopes resume at their authentic LastCycle
boundary. The owning regression replays `02:533B@2` at frame 1,066 with exact
timing, writes, registers, and RAM parity, rejects any executed `02:5393`
instruction-profile row, and shares the full registry's exact stack-page
write trace with the emulated route.

Semantic `02:52C6@all` covers all 20 catch-retraction continuations after the
separately observed `02:533B` leaf. It carries the leaf's subtract borrow
through `LDA $3300,X / SBC #$09 / STA $3300` and
`LDA $3380,X / SBC #$00 / STA $3380`, intentionally preserving the ROM's
absolute slot-zero destinations. Every instruction is checkpointed. Nineteen
nonzero-charge calls reach the original `02:52E2` observer boundary after 105
body cycles, validate the outer `01:C51D` dispatcher frame, and absorb its
21-cycle `RTS`; the final call clears Shooter occupancy `$3C80` and resumes
assembly cleanup at `02:5124` after 126 cycles without popping that frame.

Release and Debug replacement routes retain the canonical state census. The
focused assembly/native comparison preserves all 20 calls and 119 scoped
writes with hashes
`cbfa183a7196791a216c7da5faaacfdda5a05e80a6933308f8f595a9de6e1aaa`
and
`959a19129762f206f6a8b31f7191ebb28776e09f04e5ff635e0ca7d2999c3e50`;
`02:52C6@1` replays frame 1,065 with 105 body cycles and two ordered writes.
The full Devil-Shooter regression keeps its existing `02:533B` observer and
all family artifacts exact while rejecting any executed `02:52E2` row.

Semantic `02:520B@all` covers all 12 launch continuations after the same
separately observed `02:533B` leaf. It checkpoints the `$2D15` charge load and
branch. Eleven nonzero-charge calls load the appearance from `$3980,X` and
jump to native `02:5475` after 48 cycles; the final zero-charge call resumes
original launch cleanup at `02:5216` after 27 cycles. No work RAM is written
inside the replacement itself.

The focused route records 12 complete calls and 794 nested writes with SHA-256
values
`48b37d777b5c4f1c76ba3bdd9f725a973727f677e93ab8dcf9f93453076d3c99`
and
`cbd4200c162c161e5232ef6676286fe2e59303a8d5a18047f3b543d20ee5efe9`.
The fifth call replays frame 1,190 with 2,913 body cycles and 61 ordered writes;
the canonical census and full Devil-Shooter family artifacts remain exact.

Semantic `02:5208@all` and `02:52C3@all` cover the single-instruction caller
seams into the shared retraction helper. Each exact 21-cycle `JSR $533B`
writes its last operand address (`$520A` or `$52C5`) high byte first through
the live stack page, decrements S twice, and checkpoints the original LastCycle
interrupt sample before resuming at the independently registered `02:533B`
entry. The child therefore retains its observer boundary, absorbed `02:5393`
return, and native `02:520B/52C6` continuations.

All 12 launch and 20 catch calls execute natively. The full Devil-Shooter
regression preserves its canonical census and exact RAM, trace, call/write,
replay, and actor-identity artifacts; its call write stream covers all 64 new
wrapper stack writes. The call sites add 672 deployed native cycles, taking
the registry to 696,916,347 cycles across 470 routines.

Semantic `02:5223@all` covers all 67 pre-charge controller calls from fixed
slot `$0E`. Each loads collision marker `$3800,X`; the observed route always
takes `BPL` into the `$53.3` session gate, so the unobserved collision-active
reset arm at `$5228` remains assembly-backed. Sixty-six calls see bit 3 set and
return through `02:5245`. Five exact checkpoints plus the absorbed `RTS`
retain their 72-cycle native shape and preserve timer interrupts inside calls
24 and 50.

The final call sees `$53.3` clear. It takes the 12-cycle branch to `02:5246`,
pushes return address `$5248`, crosses the bank-one thunk at `01:C018`, and
composes native coroutine scheduler `01:C5C9`. The scheduler consumes that
stack frame and writes `02:5249` into slot `$0E`'s dispatch pointer, producing
the original 249-cycle/seven-write transaction. Replacement and assembly runs
share census
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
67-call hash
`8511fa5d1750b88cd3a5004c42d93c2d56d709958796aeee5d2a77b49796b4fa`,
and 32-write hash
`28e4db1c09d3e4c1fceb0722c596a2532ee3a8ad8841c101fc282ac0c2eedf00`.
Regression: `dc_rom_devil_shooter_charge_entry_controller`.

Semantic `02:525F@all` covers the complete observed post-launch steady
controller. The earlier profile span `02:523F-526A` combines three coroutine
entries: `02:5223`, `02:5249`, and `02:525F`; `$523F` itself is an interior
session-bit gate. From frames 1,421 through 3,600, all 2,169 true `$525F`
entries see fixed slot `$0E`'s collision marker nonnegative and session bit
`$53.1` clear. The native adapter reproduces `LDA $3800,X`, `BMI`, `LDA $53`,
`AND #$02`, and `BNE` through five exact checkpoints, retains all 37 original
interrupt-bearing scopes, and absorbs the `RTS` at `$526A`. The unobserved
collision-active and launch-request continuations at `$5276/$526B` remain in
assembly.

Release assembly and replacement runs are byte-identical across the state
census, canonical work RAM, all 2,679 family calls, 11,229 scoped writes, and
the actor timeline. Debug preserves the same gameplay artifacts and selects
the repository's documented normalized framebuffer-placement census.

- Call SHA-256:
  `35c847d6ba93f7cc2fc2b1157e6d8c6f3652c61271320d9c321bc06eebee393b`
- Write SHA-256:
  `1def4a539ab329c73389dd0b7acbcd4ae76c8fd5616326da2c63ddedc75e7218`
- Continuation call SHA-256:
  `7d3a28d70531af55837c5cb70573e7172b13469ecae092ca1c109f0f164bf55f`
- Continuation write SHA-256:
  `795ccd98c887afec7fab1c92e9b7bebbe61681c7d06dc08167dc742397f1c069`

No score-accumulator store occurs anywhere in this boundary: the Devil
Shooter serves and launches the ball but awards no points. The global actor
identity trace retains the subsequent pointer progression through
`02:5223`, `02:5249`, and `02:525F`.

`tools/Analyze-DevilShooterSemantics.ps1` guards the fixed layout record,
initializer position overrides, Button-I test, catch/charge/release and
post-launch ROM signatures, call and return populations, ball placement,
charge progression, audio schedule, launch velocity, absence of score writes,
actor-pointer timeline, and normalized replay. Debug and Release reproduce:

- Trace SHA-256:
  `7951bfdc12478f2f7a385aed2494e15d109ad264001db5d4ae369628033875ef`
- Call SHA-256:
  `305eda8844916d6357bc9d4143587b115a6f51fedff07b6f6b124b6c25bf0b53`
- Write SHA-256:
  `53d844b9f824eb2959245997e7bd736effc13e40d8a8118eddce53703e5f8c12`
- Canonical work-RAM SHA-256:
  `07673ea2b1101aff61f6058780a0817ec47191ea3d2abf5559a994d14db84779`
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

### Devil's-Coffin initializer extension: semantic `03:63E1@all`

The fixed Coffin setup and both post-cycle reinitializations now execute
natively. Slot `$0F` receives the normalized `$40` collision marker and
cleared appearance before the exact 249-cycle, nine-write scheduler handoff
installs `03:63F1`. The adapter preserves both observed caller stack shapes
and the returned status bit six; collision, opening, closing, and session
behavior retain their existing bounded native-or-assembly paths.

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

Semantic `03:63F1@all` replaces all 643 steady checks. When indexed collision
marker `$3800,X` is nonnegative, the native path exactly preserves the
51-cycle clear-session return or the 66-cycle Shooter-session path that
writes `$83` to indexed actor type `$3000,X`. The two negative-marker
contacts at frames 1,244 and 1,297 continue natively by writing one to
indexed timer `$3A00,X`, reproducing the `JSR 01:C018` stack transaction,
and composing semantic `01:C5C9` to install `03:640A`. The no-refresh and
type-refresh collision paths use 21/23 checkpoints, eight/nine writes, and
exactly 270/285 master cycles. Together with the five/seven-checkpoint
ordinary returns, these transcripts preserve the accumulator, N/Z flags,
write ordering, and all 13 original interrupt-handler passages. Ordinal 18
independently shadows the 285-cycle, nine-write frame-1,244 contact. Debug
and Release preserve:

- Call SHA-256:
  `be298d2a16ba1d3437f0abe284e63153d27c5aaefbde1373acf285cb75581523`
- Write SHA-256:
  `df111ba927c865c9a94bc5fd8b47325103bfeb3374988d92beacfb006d824e1f`

Semantic `03:640A@all` extends native coverage into the Devil's-Coffin
opening presentation pass. The indexed `DEC $3A00,X` is classified from
live slot state. When its result is nonzero, the adapter emits the original
timer write at master-cycle offset 21, preserves the accumulator and exact
N/Z flags, takes `BNE`, and returns at `03:6426` after 33 master cycles.
Two instruction checkpoints preserve the original interrupt-sampling
boundaries. When the timer expires and incremented appearance remains below
three, the adapter also reproduces the timer reload, appearance increment,
collision-marker update, compare, and return through eleven checkpoints,
four ordered writes, and exactly 132 master cycles.

When appearance advances to three, the adapter clears the indexed collision
sign bit and reproduces `JSR 01:C018`, including both stack writes. `C018`
tail-jumps into the existing semantic `01:C5C9` scheduler transaction,
which consumes that return, writes scratch `$26:$27`, and installs
`03:6432` in indexed dispatch. The composed handoff uses 28 checkpoints,
12 ordered writes, and exactly 366 master cycles before returning through
`01:C5E9`.

The route contains 18 opening calls. Twelve decrement timer values
`4→3→2→1`, four advance appearance from zero to one or one to two, and two
perform the final scheduler handoff; all execute natively. The complete
18-call/52-write artifact remains exact. Ordinal 9 independently shadows
frame 1,253 with exact 366-cycle/12-write parity. ROM-free tests also cover
both expiry classifications, byte wraparound, invalid slots, and truncated
snapshots. Debug and Release preserve:

- Call SHA-256:
  `fc6e5cf0cb80a28e6eff545ff3ba7aaec694d8b2dbad71c283a2a40109143c4c`
- Write SHA-256:
  `654ee9d3753bb4500334b67b6fa4fd08ae931ccf3115baf36f51c4d1b6c956ca`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Semantic `03:6445@all` applies the same bounded native contract to the
Devil's-Coffin closing presentation pass. A nonzero result from indexed
`DEC $3A00,X` writes the timer at master-cycle offset 21, preserves the
accumulator and exact N/Z flags, takes `BNE`, and returns at `03:645C`
after 33 master cycles. Two instruction checkpoints preserve the original
interrupt-sampling boundaries.

On a timer expiry whose appearance remains nonzero, native execution reloads
the timer to four, ORs `$40` into indexed collision state, decrements indexed
appearance, and returns at `03:645C` after 111 master cycles. Four ordered
writes and nine instruction checkpoints preserve the original transaction.

An appearance decrement to zero continues natively through the final
closure. It clears session bit `$53.3`, restores actor type `$82`, jumps
through initializer `03:63E1` to normalize collision and appearance, then
reproduces `JSR 01:C018` and composes semantic `01:C5C9`. The scheduler
increments stacked return `$63F0` into steady dispatch `03:63F1` and returns
through `01:C5E9`. This path preserves 15 ordered writes, 34 instruction
checkpoints, and exactly 429 master cycles.

The route contains 18 closing calls: twelve non-expiring decrements, four
nonterminal timer expiries, and two final closures all execute natively. The
complete 18-call/58-write artifact remains exact. Ordinal 9 independently
shadows frame 1,263 with exact 429-cycle/15-write parity. ROM-free tests also
cover all three classifications, session-bit derivation, byte wraparound,
invalid slots, and truncated snapshots.
Debug and Release preserve:

- Call SHA-256:
  `5a88e7e39a4520287a584b68f8cb0ba789b190ad8f96cb047e642a96ae98cc92`
- Write SHA-256:
  `3f16cadfd078c9f0713396202dbf3c4fee4505876f7c18dfd571436963e1a0e8`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

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

Semantic `02:5A4B@all` now replaces all 2,144 Dragon's-Head collision
checks. Its idle transcript reproduces indexed `LDA $3800,X`, the taken
`BPL`, accumulator/status output, outer return at `02:5A73`, and the exact
27-cycle zero-write body, including all 17 interrupt-bearing idle scopes.
For the bit-7 hits at frames 2,076 and 2,080, the native path reproduces
`AND #$7F` and indexed `STA $3800,X`, then hands the shared score, audio,
bonus, and appearance helpers to assembly after exactly 42 cycles.

Post-helper semantic `02:5A6E@all` owns both route-proven nonterminal
countdowns. It decrements indexed counter `$3C00,X` from 8 to 7 and 7 to 6,
updates flags, and returns at `02:5A73` after 27 cycles and one write.
Counter one is rejected so the second 50-unit award and mouth-opening
transition at `02:5A74` remain original. Debug and Release preserve outer
call SHA-256
`81160e67b4b09b27825ea32260423151d866fdff6367da503b638c8456c25724`
and write SHA-256
`73f13fa9786e728cb2052de384052e5ee60e3fe496a6549a686f6cc3a3edcb94`.
The focused continuation artifact has call SHA-256
`564b3e42385479f383e4a93b46d568f58242867b4f7f9593810a37ece7fa0b3c`
and write SHA-256
`03b84e95c2335cb54cdfab0673164dc6a1816e7b16d12cf7509ba53e81dc2931`;
ordinal 1 shadows frame 2,076 with exact 27-cycle and one-write parity.

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

Semantic `03:664A@all` now replaces the three fixed setup calls through their
exact 396-cycle, 16-write transcript. The typed adapter accepts only slots
`$58-$5A` with their matching layout parameter `0/1/2`, installs the observed
`$8A/$82/$85` initial appearances and `03:6677` script, and preserves the
final `01:C5E6` scheduler boundary. The focused Release regression replays
frame 872 and guards all three writes/calls against the standard census.

Both movement phases converge on collision handler `03:67B3`. A collision
clears bit 7 of `$3800,X`, emits command `$44`, advances the actor-selected
global state modulo six, and refreshes `$3D00,X`; it does not submit a score
award. The route records 16 such producer attempts:

Semantic `03:66A3@all` initially replaced bounded first-movement prefixes for
the fixed Blob slots. Its 535 direct no-zero paths reproduce the three ordered
countdown stores and resume at `03:66DE`. Its 272 timer-expiry paths preserve
the decrement, reload-to-four, and phase-bit-toggle transcript, including the
observed timer interrupt boundaryâ€”then resume original mixed movement at
`03:66B5`. Forty-five y-expiry paths reproduce all three counter decrements
and resume at `03:66BF` before the original reset/coroutine path. Six exact
instruction checkpoints preserve timer-IRQ delivery for eleven formerly
horizon-adjacent y expiries; five user-event-horizon calls remain
assembly-backed. Direct motion at an event horizon and other zero movement
transitions continue through assembly.
All six x-expiry paths reproduce the timer/x decrement pair and branch
directly to the original reset/scheduler tail at `03:66E2`; four exact
instruction checkpoints preserve timer-IRQ delivery for the two formerly
near-horizon calls.

That partial coverage is now superseded: semantic `03:66A3@all` replaces all
1,107 observed first-movement calls. The direct no-zero transcript has six
exact instruction checkpoints: the timer branch is taken at 33 cycles, the X
condition falls through at 60 cycles, and the Y branch is taken at 93 cycles.
Those boundaries preserve the 249 formerly horizon-adjacent calls, leaving
the original mixed movement, reset, and coroutine tails as the only assembly
work after their respective prefixes. The complete Release state census,
call trace, and ordered-write artifact remain byte-identical to assembly.

Semantic `03:676B@all` composes the scene-clear shared-motion leaf used by
both movement phases. It accepts only clear group latch `$2CFD`, clear scene
latch `$2D3C`, and the existing collision-free appearance-gate state. The
exact 147-cycle path loads both latches, branches to `03:67B3`, stores the
derived appearance at `03:67BD`, and returns at `03:67E5`. Its exact latch
and appearance checkpoints admit 1,950 of 1,965 current calls; only suspended,
moving, and collision-active states retain the original assembly path. The
Release census, call, and ordered-write artifacts remain byte-identical, and
`03:676B@1` replays at frame 1,229 with one write.

Semantic `03:66DE@all` is the compact first-movement caller of that shared
leaf. It preserves the nested `JSR $676B` stack writes at entry, all child
instruction boundaries, and the child's return to outer `RTS` at `03:66E1`.
The exact 189-cycle, three-write composition admits 1,034 of 1,041 current
calls; non-idle child states retain the original assembly path. The focused
Release census, call, and ordered-write artifacts remain byte-identical, and
`03:66DE@1` replays at frame 1,229 with three writes.

Semantic `03:67C0@all` now replaces every direct final collision-latch gate
call for the fixed Three-Blobs slots `$58-$5A`. It reloads `$3800,X`, derives
the exact N/Z flags, and takes the 27-cycle no-hit branch to `03:67E5` through
two instruction checkpoints. Negative latches execute the exact 69-cycle
latch-clear, command-`$44`, and `JSR $C087` audio-trampoline prefix, including
the return stack frame; original audio, color-state advance, and
appearance-update behavior resumes in assembly after that boundary. The
focused Release census, call, and ordered-write SHA-256 values
are `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
`242d6988bbfb0713027cf6dde7fa31c52a509ad44aec9e47ab5d3b132cb54fc1`, and
`e414b0dfc26d2a56195e1e48f18cb400404839226f26aea4396a547efb061185`.

- Slot `$58` is not hit.
- Slot `$59` receives seven hits and ends at state 2.
- Slot `$5A` receives nine calls and ends at state 4. Two calls occur during
  frame 1,546 and both write state 4, so only eight effective advances occur.
- Those two same-frame attempts coalesce at the per-frame audio export,
  explaining why the earlier command census contains 15 accepted `$44`
  exports rather than 16 producer writes.

Fixed slot `$57`, immediately before the three visible records, is the family
controller. Its category-2 layout record uses initializer `03:65BC` (native setup), type
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

Semantic `03:6876@all` now replaces the Gray-Blob initializer's exact
372-cycle, 15-write setup transcript for all seven fixed layouts. Its final
`01:C5E6` checkpoint samples the interrupt boundary at cycle 369 before the
372-cycle return, so calls that formerly stopped at the conservative
user-event or timer-interrupt horizon retain the original scheduler timing.
The complete initialization artifact, including the shared `01:C018` script
setup, remains exact.

Semantic `03:68A1@all` now also replaces the seven natural first-live
prefixes, only while the fixed Gray-Blob slots `$50-$56` retain their initial
one-tick phase timer and standard transient/collision latches. It reproduces
the 462-cycle latch-clear, RNG, timer-store, and scheduler-call prefix, then
hands off at original `01:C018`; all seven complete 633-cycle/20-write
observer scopes remain byte-identical. Later movement timer, phase animation,
velocity, collision, hit, score, and impact paths remain assembly-owned.

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

### Dragon's-Egg controller initializer extension: semantic `02:42EB@all`

The hidden controller's one setup call now executes natively. Its typed
slot-`$1F` adapter clears group-reset byte `$2CF7` and preserves the exact
228-cycle, eight-write coroutine handoff to `02:42F6`, including the
caller-relative stack transcript. Live-count completion, collision, score,
hatchling, and reset behavior retain their existing native-or-assembly
boundaries.

### Dragon's-Egg initializer extension: semantic `02:433F@all`

All six fixed layouts now execute natively. The typed `$20-$25` adapter
derives the `$2CF4` live-count increment from the pre-entry RAM snapshot and
preserves each exact 213-cycle, eight-write shared coroutine-scheduler
handoff to `02:4345`, including the caller-relative stack stores. The group
controller, collision, score, hatchling, and reset paths retain their
existing native-or-assembly boundaries.

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

### Dragon's-Egg dormant reset-gate extension: semantic `02:4387@all`

The second scheduler update at `02:4384` installs `02:4387` as the destroyed
fixed egg's persistent dispatch. This is a routine boundary, not merely the
tail of the one `02:4345` hit call. Slot `$22` enters it 1,154 times from
frames 2,427-3,584. Each call loads group-reset byte `$2CF7`, masks bit 0,
finds it clear, and returns at `02:438E`. Of those calls, 1,145 are exact
27-cycle, zero-write bodies and nine contain only timer-interrupt writes.

The typed classifier accepts any fixed egg slot `$20-$25`, distinguishes the
dormant and reset-requested states, and leaves the unobserved requested arm in
assembly. That arm starts at `02:438F`, clears the actor's disabled and
collision bits, clears its activation byte, and branches to initializer
`02:433F`. The native dormant arm retains all three instruction checkpoints
and absorbs the return, including exact interrupt replay. Normalized replay
`02:4387@1` verifies frame 2,427, 27 body cycles, and zero ordered writes.
Release and Debug reproduce the canonical census and focused artifacts:

- Call SHA-256:
  `e390996962f207c344043d065ee978a662f58e18f9f7bc67ce9e4297213f8762`
- Write SHA-256:
  `b9aa1662e7152bd361af29a16f06a071abd3151ad5ad3babf121bcd40afe1f6c`

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

### Mercy-Star initializer extension: semantic `02:4F9F@all`

The three fixed Mercy-Star slots `$5F-$61` share a 318-cycle initializer that
sets their dormant collision value, appearance state, and script dispatch
before returning through the common coroutine helper. The typed native adapter
accepts only those three layout slots and their authentic frame-872 register
entry, preserving the exact eleven writes per call. Its final `01:C5E6`
checkpoint samples cycle 315 before returning at `01:C5E9`, retaining the
original scheduler/interrupt boundary for every setup call.

The owning Release semantic regression replaces all three initializers,
shadows `02:4F9F@1` with its 318-cycle, eleven-write transcript, and keeps
the complete census, call, and write artifacts byte-identical. The existing
`02:4FC1@all` dormant gate continues to cover all 7,950 normal-route steady
calls; Mercy-Star activation, scoring, table transport, and release remain
assembly-owned until naturally exercised.

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

Semantic `02:42A3@all` now owns the activation's seven exact `JSR $C087`
instructions. It preserves each live source-selected command, pushes return
address `$42A5` through the live stack pointer, and stops at the independently
registered `01:C087` bank-one trampoline after the exact 21-cycle LastCycle
checkpoint. The original audio handler and post-command activation body remain
assembly-owned. The canonical census is unchanged; the focused entry, call,
and ordered-write SHA-256 values are
`918a84f71509685531895b48256cd57dd8a5108483ad6ab850c12a37949a5bc8`,
`c6087846548fc872952ffa9f67da5c8b6521535a2d541875bcf1d07393b1f001`, and
`fa961f3ea6880a56d241ad7516537123bbe25021b47171069f835604e09de5b6`.

Semantic `02:427C@all` now owns the allocator's seven successful terminal
`RTS` instructions after `CLC` at `02:427B`. It reads each live return frame
from the stack page and resumes the two Small-Soldier sites, the Tower-insect
site, or the Dragon's-Egg sites without encoding a source-specific address.
The seventh call coincides with a timer horizon, so the transcript publishes
the return's original master-cycle-18 interrupt sample and master-cycle-21
completion instead of treating the instruction as atomic. All seven calls are
accepted and the canonical census is unchanged. The exact entry, direct-call,
and zero-write SHA-256 values are
`c2a29eddf96d026cde5cec90c7c0b99c9e6bd2e4261b12b91e217bfced48bc8b`,
`fae06be87bee960fb2faab61cbc7b21d375e6c74dc4b86dd349550a5ecdb39ef`, and
`8734f9ee3371dc8708ba0f671a838816a1aa8339582df709daef67d990377c1c`.

Semantic `02:420E@all` now owns the complete successful allocator tail on all
seven natural calls. It copies the `$42E1` controller pointer, derives both
coordinate pairs from the source slot and `$2B/$2C` offsets, initializes the
new slot's collision, appearance, timer, type, dispatch, and source-derived
attribute fields, clears carry, and returns through the live caller frame.
The successful path is exactly 522 master cycles and 16 writes before its RTS;
46 instruction checkpoints preserve the timer interrupt nested inside call
two. That interrupted direct scope remains 1,407 cycles and 30 writes in both
assembly and native runs. The exact entry, call, and ordered-write SHA-256
values are
`c5a73149b4b1c91d1b5c1e67b77c761a5c6b509efdd941c990add4ea98b258dc`,
`1b3333a23c5db108a6cefeb91e86fe201f26ea439a11813e97c569baf23a0ea1`, and
`0ac675f2f114fd14cc7cd7802982ddbd26804da70d44a611ae077399ca3c1f9b`.
The patched Knight-terminal route sharing `02:420E` retains its prior
instruction-sized proof rather than being folded into this natural contract.

Semantic `02:42B0@all` now covers all 252 steady-state lifetime calls. It
reproduces the negative actor gate, four-tick timer, phase increment, indirect
`$42E1,Y` animation-table read, appearance store, and `$FF` release at every
instruction boundary. Its seven terminal paths compose the registered
`01:C027` jump and `01:C574` actor reset; its wait and advance paths absorb
the return at `02:42E0`. Assembly and native runs retain the canonical census
and byte-identical direct artifacts:

- Lifetime call SHA-256:
  `02d0567d62b5130c74a6a310762badcb9b882bf03abe5d4983edd0415ee0f06d`
- Lifetime write SHA-256:
  `93774afdee74412026678a8e86f9cb969fc18ab8bf7924477ab25c3255d6dcae`

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
Semantic `03:646B@all` now covers both fixed layouts through their exact
342-cycle, fourteen-write side-parameter/collision/script setup transcript and
final `01:C5E6` scheduler checkpoint; the collision-owning steady controller
is now native as described below.

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

Semantic `03:6490@all` now composes the selector prefix and both native child
transcripts for all 2,645 calls. It absorbs all 2,642 collision-free returns;
the three hit calls retain the exact checkpoints through the children and hand
control back at `03:64A5`, before the original safety, score, audio, and
transition sequence. The complete call, ordered-write, and actor-identity
artifacts remain unchanged.

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

Native semantic `03:64FA@all` now covers both fixed layout setups through the
exact 342-cycle, fourteen-write transcript and final `01:C5E6` checkpoint.

Semantic `03:651F@all` now composes the side selector and both native child
transcripts for all 2,796 natural calls, preserving every interrupt boundary,
9-47 ordered-write shape, and the normalized 903-cycle first call. Every call
is collision-free on this route, so the parent absorbs `03:6533` on all of
them; the collision continuation remains available as an exact assembly
handoff for other states.

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
cycles and make no writes.

The post-trigger `02:5590` state is one complete routine behind the profiled
interior rows at `5592`, `5595`, `55AA`, and terminal `RTS` `55AF`. All 1,084
natural calls use fixed slot `$33`, set Y to zero, find both `$3200,Y` and
Tower state `$2CC7` clear, and return without a gameplay write. Seventeen calls
are lengthened only by original timer interrupts sampled at those five
instruction boundaries.

Semantic `02:54B9@all` replaces all 1,631 left-target trigger checks. The
clear-latch path remains the exact `LDA $2D13`, not-taken `BNE`, and RTS at
`02:54BE`: 1,630 calls take 21 body cycles, write nothing, and preserve the
loaded accumulator and N/Z flags. The frame-3,466 nonzero latch now follows
a 978-cycle, 23-write transcript. It writes award `$002F:$0030 = $0028`,
composes native `00:F55C` for the 4,000-point score, sets `$3A31 = $04`,
and composes native `01:C5C9` to install dispatch `02:54D0`. Focused replay
`02:54B9@1631` verifies the cross-bank stack and complete RAM result. All
nine original interrupt-handler passages remain exact, and Debug and
Release preserve the complete 1,631-call/150-write artifacts:

- Call SHA-256:
  `8e07185d94f3dcd3323afdbc62f1cb314605b18c5eb60ca8d54d68c1a0c9cf51`
- Write SHA-256:
  `925b610c5591457d8cec1d5b8795b5b5cd16e6a94b7af8433acdfd582c619296`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`

The active trigger installs `02:54D0`, a three-round four-tick startup
countdown for fixed slot `$31`. Semantic `02:54D0@all` ports its indexed
decrement and branch on all 12 calls. Nine nonzero calls absorb the plain
`RTS` at `02:54EC`; three zero calls hand the original appearance transition
back to `02:54D5`,
so the timer interrupt immediately following the final round stays on the
emulated CPU path. The exact focused artifacts contain 12 calls and 44 writes,
and normalized `02:54D0@4` replay covers the first 132-cycle, four-write zero
transition. The countdown and its wait returns own 567 native cycles without
changing the family artifacts.

Semantic `02:5500@all` replaces all 80 routed calls after the left target's
opening countdown. Every call uses slot `$31`, loads a nonnegative `$3800,X`,
finds session bits `$53.1/$53.3` and Tower state `$2CC7` clear, and reaches the
`RTS` at `02:551F` without a gameplay write. The exact nine-checkpoint
transcript preserves timer interrupts after `02:5518` and `02:551D`, then
absorbs the return. The inactive sign-bit collision/award arm and active-gate
branches remain assembly-backed. Normalized replay verifies frame 3,479's
84-cycle, zero-write body. Assembly and native runs share exact entry, call,
and write SHA-256 values `4c0d5ded1079ecab6327af70db54176bd01732cc6b002fbc6096777c5f25b0ae`,
`2cb007495b265059d30ecccf79f8b920306455656d0c4e12841ffd9225fb841f`,
and `607743197bb2007cd46edbd8cbdcb429ebbf8a8ea09b8bf47ce16bf357ce496d`.

Semantic `02:5551@all` replaces all 336 right-target trigger checks. The
clear-latch path remains the exact `LDA $2D14`, not-taken `BNE`, and RTS at
`02:5556`: 335 calls take 21 body cycles before the native transaction absorbs
the 21-cycle return, write nothing, and preserve the loaded accumulator and
N/Z flags. The frame-2,182 nonzero latch still follows
a 978-cycle, 23-write transcript. It writes award
`$002F:$0030 = $0028`, composes native `00:F55C` for the 4,000-point score,
sets `$3A33 = $04`, and composes native `01:C5C9` to install dispatch
`02:5568`. Focused replay `02:5551@336` verifies the cross-bank stack and
complete RAM result. All six original interrupt-handler passages remain
exact, and Debug and Release preserve the complete 336-call/101-write
artifacts:

- Call SHA-256:
  `5d492bfc62f7e50bf19b536e3c47a9e50ac45c55a400e399b7ac80909aa2023c`
- Write SHA-256:
  `865a439dcddc733ff94cbf9ec50cd0185dd5d5f630e62458a91538b129584efe`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`

The routed trigger installs `02:5568`, a two-round four-tick startup countdown
for fixed slot `$33`. Semantic `02:5568@all` replaces its indexed decrement
and branch on all eight calls. Six nonzero waits absorb the plain `RTS` at
`02:5584`; the two zero calls hand the appearance transition back to original
code at `02:556D`,
including the final `01:C018` scheduler handoff. The focused artifacts contain
eight calls and 22 writes, with call SHA-256
`b958c25a1d1950616223a0c52d9310faa23ed6fbf2e774fe2086a4e933b0d2c0` and
write SHA-256
`5b8c6dd68f3a53567abfd0999f9ff6b936de507854be7cd5682b4714d68e306b`.
Normalized `02:5568@4` replay covers frame 2,186's first zero transition at
132 body cycles and four ordered writes. The countdown and its wait returns
own 378 native cycles without changing the complete Skeleton-Insect family
artifacts. The two return-absorption follow-ons add 315 native cycles together.

Semantic `02:5590@all` replaces all 1,084 right-target post-trigger idle calls.
Its exact five-checkpoint transcript reproduces `LDY #0`, both shared-gate
loads and branches, and absorbs the `RTS` at `02:55AF`. The unobserved
secondary-collision and Tower-active animation paths remain assembly-backed.
Normalized replay `02:5590@1` verifies frame 2,191, 54 body cycles before the
return, and zero ordered writes. Release and Debug preserve the complete family
census, call, write, trace, and selected actor-state artifacts.

Semantic `02:55F9@all` replaces all 2,403 center-target trigger checks. The
2,402 clear-latch calls execute `LDA $2CFC`, the not-taken `BNE`, and the
RTS at `02:55FE` through an exact two-checkpoint, zero-write, 21-cycle
transcript. The frame-3,353 nonzero latch takes the exact 27-cycle branch
and resumes assembly at `02:55FF`, where the shared `01:D8D2` drawing
helper remains emulated.

When that helper returns, semantic `02:5604@all` owns the complete remainder
of the trigger. Its 1,152-cycle, 30-write transcript writes
`$002F:$0030 = $0064`, composes native `00:F55C` for the 10,000-point
award, clears center presentation bit `$3080,X.6`, reproduces immediate
delay installer `01:C5EA`, toggles `$3080,X.5`, and composes native
`01:C5C9` to decrement the new `$10` delay and yield through `01:C5E9`
while retaining the `02:561A` loop that eventually installs `02:5625`.
Focused replay `02:5604@1` verifies exact register, cycle, stack,
write-order, and complete-RAM parity.

Semantic `02:561A@all` owns every subsequent center-target presentation
toggle. It flips `$3080,X.5`, reproduces the `JSR $C018` return frame, and
composes native scheduler `01:C5C9`. Fifteen calls take the 171-cycle
countdown path; the final 249-cycle call installs dispatch `02:5625` in
`$3F00:$3F80,X`. The ninth direct call's timer interrupt is delivered between
the same original instructions. Focused replay `02:561A@1` verifies its six
ordered writes and the family regression preserves all 16 direct calls.

Semantic `02:5625@all` owns the one-time post-delay handoff. It loads the
center target's presentation state, clears bit 5, writes the exact `JSR $C018`
stack frame, and composes native scheduler `01:C5C9` to install `02:5630`.
Focused replay `02:5625@1` verifies frame 3,371's 228 body cycles and eight
ordered writes.

Semantic `02:5630@all` owns the subsequent idle opening gate. All 229 observed
calls find session bits `$53.1/$53.3` and global gate `$2CC7` clear, traverse
eight exact instruction boundaries, and return through absorbed `02:5641`.
Six timer-interrupt passages remain at their original boundaries. Focused
replay `02:5630@1` verifies frame 3,372's 69-cycle, zero-write body. The
unobserved opening branch at `02:5642` remains assembly-backed.

All 23 idle interrupt-handler passages and the one interrupt inside the
assembly-owned drawing helper remain exact. Debug and Release preserve the
complete outer 2,403-call/391-write artifacts:

- Call SHA-256:
  `efd9519c4adab99a53ad1c2cb2c1d3085ce44460eefe83c0908f6c7d299aca06`
- Write SHA-256:
  `d46e26ec07b399d23e0d3a64c62fda8af19658d75b0c3375c8761e9d5de603ad`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`

The native continuation independently preserves:

- Call SHA-256:
  `a66d5c7ed84a10d335b7e50d03102a870bb64882002922a76dbb674dcafc9482`
- Write SHA-256:
  `449dd7d55c5e020681b24ced4c38dbca69d72361083eaee29f25ada2dd7c83ce`

`tools/Analyze-SkeletonInsectSemantics.ps1`
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

Semantic `02:57F8@all` now replaces the preceding eight staging calls. Its
fixed slot/route-index adapter preserves the exact 249-cycle, ten-write
route, collision, and `01:C5E6` scheduler handoff to `02:5808`.

Semantic `02:5808@all` now replaces those eight fixed deployment setups. It
preserves the exact 642-cycle, 22-write transcript for slots `$17-$1E`,
including route-indexed staggered delay, live-count increment, and the
original `01:C5E6` scheduler handoff to `02:586F`. The later deployment,
arrival, orbit, collision, and regeneration states retain their existing
native-or-assembly boundaries.

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

The three states themselves are native as of the Sorcerer-tick slice.
`02:586F` (deployment delay), `02:5886` (arrival) and `02:58C2` (orbit) are
instruction-boundary transcripts, together with `02:5893` and `02:58DD`, the
two addresses the core arrives at after this family's calls to the
collision-table interpreter at `03:6966`. Five entry points, admitted on every
call, 3,156,294 master cycles gross. The reset block at `02:5896` is not an
entry point: the walk from `02:5893` takes its own branch into it. What is left
emulated in `02:586F`-`02:5903` is 910,530 cycles, about 87% of it the two
interpreter calls, the two calls to the bank-01 wrapper at `$C018`, and the
three `RTS` instructions the walks stop in front of.

All four of those calls are handled now. The two to the interpreter are
composed where the interpreter accepts the entry and crossed where it declines;
the two to `$C018` are crossed outright, which is worth 336 cycles over their
sixteen calls. The interesting number is the one that did not move. `02:58DA`
profiles 598 interpreted executions worth 12,558, and crossing it recovered
none of them: those executions never reach the stop. They are residue — the
core running on through the routine after an interrupt abandoned a transcript
upstream — and the whole stretch `02:58C7`-`02:58E3` carries the same kind of
count inside a routine whose entry installs 18,152 times. Recovering residue
means a new entry point, not a crossing.

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

## Sorcerer controller color gate native replacement

The controller's active-live-count branch reaches direct target `02:57E9`
with hidden controller slot `$16`. It loads shared color `$2D40`, compares it
to cached `$3980,X`, and returns at `02:57F7` when equal. A mismatch stores
the new color and calls `02:593F`; that update path remains assembly-backed.

Semantic `02:57E9@all` now replaces all 2,648 observed direct entries with
the exact 42-cycle load/compare/branch return transcript. The natural route
has 2,610 zero-write calls; the remaining calls retain original interrupt
passages through instruction checkpoints. Release native census, call, and
ordered-write SHA-256 values are respectively:

- `e4826889df5db0c7585e889ab2df1db3e8401be39db0b909ebea6be0950278ce`
- `902b5143e8de9aabf2accc6954f6e870bbee820e5ca0fd0edf933fd651962a64`
- `6ca2df71d6b3cd024e9dbf997f7e590083aeb18772abc79961b36a15aa753ca4`

The focused normalized replay `02:57E9@1` returns at frame 942 after 42 body
cycles with no ordered writes. Debug replay is intentionally deferred to a
Sorcerer sibling or family checkpoint under the native-port cadence.

Semantic `02:57AD@all` now also replaces the live-controller branch before
that color gate. All 2,648 standard-route calls load nonzero shared live count
`$2D0F` and take the exact 27-cycle `LDA`/taken-branch handoff to `02:57E9`.
The route's complete census, call, and ordered-write artifacts remain exact:

- `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- `9e592bdfbb2141f7efb64277a7e1d8c8410b0c258325856e1957a5923229059b`
- `d432cc5f932791ec2ef75f62950174dcbf024452f7b93d7d1fc9483f99f114e0`

The zero-live delay, RNG selector, and deployment-reset state machine remains
assembly-backed at `02:57B2`.

## Tower-insect startup-controller native replacement

Spawner `02:5F78` initializes each of the four dynamic Tower-Insect slots
`$64/$67/$6A/$6D` with type `$82`, dispatch `02:5FDB`, and coroutine countdown
`$3D80,X=$10`. The startup dispatch toggles `$3080,X.5` and calls the shared
`01:C018/C5C9` coroutine scheduler. Each slot takes fifteen 171-cycle
decrement/yield calls and one 249-cycle expiry that installs `02:5FE6`.

The one-time `02:5FE6` continuation clears direction bits 5/6, writes
`$3880,X=$80` and `$3A00,X=4`, and composes the scheduler's immediate-install
path to set dispatch `02:5FFB`. All four calls take 270 cycles. Semantic
`02:5FDB@all` and `02:5FE6@all` accept only the observed slots, type, live
dispatch, and countdown states and publish every original instruction
boundary. The focused Release regression covers all 68 calls and 481 ordered
writes; normalized replays are frame 941 (171 cycles, six writes) and frame
957 (270 cycles, ten writes). Assembly and native artifacts are:

- Census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Call SHA-256:
  `e22cb671f237cffc58374b00759f2a5440ad882b788f21ae200071e379c0cb5f`
- Write SHA-256:
  `6b924a26c4eada468c6e23229e54e13fad955380a1eb396dc4edee6e68512b89`

The subsequent `02:5FFB` collision crossing and steady bank-`03` controller
remain at their separately proven native boundaries below. Post-collision
continuation `03:6001` remains assembly-backed because its cross-bank interior
entry is not a stable standalone routine-call observer scope.

## Tower-insect animation native replacement

Dynamic Tower-insect controller `03:6016` branches directly to `03:6054`
after its collision-table call. This leaf loads cadence `$3A80,X`, clears its
low bit and doubles it, combines the result with low two phase bits from
`$3C80,X`, and selects one of 32 frame values at `03:6079`. It stores that
frame in `$3980,X`, decrements `$3A00,X`, and on expiry reloads six and
increments the raw phase.

Semantic `03:6054@all` accepts only the four observed dynamic Tower-insect
slots `$64/$67/$6A/$6D`; out-of-table cadence states conservatively remain
assembly fallback. It replaces all 6,750 natural calls: 5,338 uninterrupted
147-cycle frame/timer waits and 1,056 uninterrupted 183-cycle reload/phase
advances, with all original interrupt passages retained at instruction
checkpoints. The focused `03:6054@1` replay returns at frame 958 with 147
body cycles and three ordered writes. Release native census, call, and write
SHA-256 values are respectively:

- `7c99d041007e2a735b2bf8e6c7c870615c15bb0e79dee9330f1414da2187853c`
- `9d3beee15ee6ada8ea7b15b414745437e71876b1ef5be14e5f184c188b7ddf76`
- `a0d6ad02b991511ecfde741d752b4c11b5da61afe9a27cdd02d90b397a4c2f2e`

The broader `03:6016` controller is now three transcripts of its own, split
where the assembly leaves the routine: `03:6016` reads the collision selector
and makes the `JSR $6966`, `03:601C` makes the `BSR $6054`, and `03:601E`
reads the hit flag, takes the no-hit branch and ends past the `RTS` at
`$603E`. Neither call is composed — both callees are entered at the same
address either way — so the three walks are 36, 24 and 48 master cycles and
add 701,016 across 6,494 dispatches. Score, audio, the two-hit counter and
child lifetime stay with the assembly: they are behind the branch, and the
seven hits on the route decline the third walk. `dc_rom_tower_insect_controller_semantic`
compares the whole stack page, the hit flags, both counters and the census
against a run with all four routines emulated, and
`dc_rom_absorbed_return_trace_dispatched` compares the call trace. The paired
source-gate slice completes the Tower-insect Debug family checkpoint below.

The fixed source has a matching collision crossing at `02:5FFB`. It loads
selector `$3700,X`, pushes the exact return frame for `JSR $6966`, and lands on
the same collision-table interpreter through a 36-cycle native transcript.
All 260 calls are admitted. Direct assembly/native traces preserve the complete
260-call/4,016-write artifact and normalized replay `02:5FFB@1` at frame 958:

- Call SHA-256:
  `1e3173f878f158bd61715e3d1bff4736309a3f577a2fe77106dafedba38e7629`
- Write SHA-256:
  `cfabe3531b0ad7a5f9cf78b668c3daef05c0bb34a9143abba093d828b9cb115d`
- Census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

## Tower-insect source gate native replacement

The fixed Tower-insect source runs from physical bank `02`. After the four
child spawns, direct entry `02:5F5F` loads live count `$2D2B`; nonzero takes
the short return at `02:5F64`, while zero enters the 200-unit group-clear
award path. Semantic `02:5F5F@all` accepts only source slot `$2C` and covers
the waiting branch, preserving the original load/branch/return status and
instruction checkpoints. The zero-count award, audio, and release sequence
remain assembly fallback.

All 2,504 natural calls on the standard route now execute natively, including
the original interrupt passages. Focused replay `02:5F5F@1` returns at frame
1,086 after 21 body cycles with no ordered writes. Release native census,
call, and write SHA-256 values are respectively:

- `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- `d0caafb2f34cd597e0fa24e2f1a179bf44897dc5fd1aa3fc63ffbcdc186f3c05`
- `338ff7769d0f05f93aab446eaac4c27066232aa9daf86cac5cebf033622da6f9`

Together with `03:6054`, this completes the Tower-insect family checkpoint.
The combined Debug regression preserves census/call/write SHA-256 values
`f70fbc86ce325a15ddaecfd1da91436ec948071b5ae7d7b1914dd99d9092d5ab`,
`948b20cb40d3329be3218769db1511ddf37b86d1af0d0cb022751aa90f3b2b7f`, and
`f19b75496f5fb83d86f42ab500a24f51c511f01c429d04fa516912187da824b5`.

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

Semantic `03:7D4D/7DE9@all` now owns the two fixed setup leaves. The left
source is an exact 327-cycle, fourteen-write transcript and the right is its
333-cycle, fourteen-write side-specific counterpart; both retain their
original collision IDs, group-count clear, source state, subtype, child
budget, stack, and `01:C5E6` scheduler writes. Delayed spawning, allocation,
and source waiting remain in their existing assembly/native-gate split.

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
appearance resets. No helmet collision occurs. Semantic `02:4C91@all` now
replaces all of these steady calls, preserving the three observed 66/135/144
master-cycle paths and leaving collision activation in assembly.

Semantic `02:4C78@all` also ports both fixed channel setups through their
exact 240-cycle, eight-write script/timer/appearance transcript. Collision-
marked entries remain assembly fallback, and the native boundary returns at
the original `02:4CF1` RTS instruction.

Semantic `02:4A5C/4A64@all` now also ports the two direct outer wrappers.
Each writes its fixed `$13/$1D` selector to `$3C80,X`, then composes the
shared setup as one exact 273-cycle, nine-write transcript, returning at the
same `02:4CF1` RTS boundary. The collision-marked path remains an all-or-
nothing assembly fallback, so it still performs the original selector store
before entering active behavior.

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

The original mapping capture contains 22 shared RNG calls. Twenty complete in
282 master cycles with eight writes. Calls at frames 1,285 and 1,297 contain
periodic interrupts, taking 939/1,029 cycles and 20/22 scoped writes while
preserving the same eight RNG-owned writes. The current native parent
regression asserts the embedded eight-write recurrence directly while avoiding
duplicate nested observer records. `tools/Analyze-UpperWallPocketHandRngSemantics.ps1`
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

### Fiftieth native replacement: semantic `02:5972@all`

The two fixed upper-hand initializers now execute natively. Each composes the
existing `01:C5C9` coroutine-scheduler replacement after the `01:C018`
transfer: the native transaction preserves the two `02:5972` stack writes,
the scheduler's five writes, the `02:5975` dispatch pointer, and the exact
192-cycle return. The deterministic route has exactly two calls, one for
each of slots `$40/$41`; the delayed RNG, collision, animation, and response
continuations remain assembly fallback. Release preserves the existing
hand-family artifacts:

- Entry SHA-256:
  `defcf166fa1b1bf77b4e415f57d082f06f60d73d8f73afbbad036b46713062d9`
- Call SHA-256:
  `98595fe928d45a59437f77f804b662a67f1860c2358c649c109b48d2d420ed92`
- Write SHA-256:
  `1f7ce0a73fc4e1c5c9dcc3953563ea48def6dfa37b482d9e827a57cbe23f3490`
- Selected-state SHA-256:
  `e4521a0c6afa7d98deba96c4f21032e2ff7e702629371a2ac07b9c097bd3dc0b`

### Fifty-first native replacement: semantic `02:5991@all`

The delayed upper-hand timer now executes natively for the fixed hand records.
The traced slot-`$40` route admits all 299 non-expiry calls: 292 ordinary
75-cycle low-byte decrements, one 90-cycle high-byte borrow, and six calls
that pause at the original instruction checkpoints for interrupt handlers.
The independently timed close-state expiry paths remain assembly-backed. The
trace-backed initial `$0001`-to-zero handoff is recorded below; the current
combined parent/countdown artifact hashes are recorded with the subsequent
live-setup replacement.

### Fifty-second native replacement: semantic `02:5975@all`

The traced fixed slot-`$40` live timer setup now executes natively. It hides
the hand collision surface, composes the existing eight-write `00:E8B3` RNG
recurrence, masks result `$73` to `$33`, stores timer `$0133`, and composes the
existing coroutine scheduler to install `02:5991`. Its complete parent scope
preserves 33 ordered writes and 1,323 master cycles, including the original
parent-level IRQ that returns through `02:5983`. The focused regression records
the parent plus countdown scopes without redundant nested observer records:

- Trace SHA-256:
  `917e2973508c58a15a7c573149a808ecf723e69e05aa75010dc3ea40817eb36e`
- Call SHA-256:
  `cf195dceb46c10ceceb195f202c4ed3871c127ee8d28fbad4058819fca4ca395`
- Write SHA-256:
  `ac951f417d75a00928fb1dd255030856345542357f0315795f74af753ebbe92d`
- Selected-state SHA-256:
  `664cdeb5e46aa4e1f8e2986f9cd915d9dca00456a8c268546e897abe52ebcba5`

### Fifty-third native replacement: semantic `02:5991@all` zero-timer handoff

A controlled route changes only the first active fixed slot-`$40` countdown
from `$0001` to `$0000`. The native replacement follows the original
`02:5991 -> 01:C5C9` transfer, preserves the scheduler's periodic interrupt,
and installs continuation `02:59A5`. The first parent call remains exactly
918 master cycles and 20 ordered writes. The focused 174-call/235-write replay
has configuration-specific census hashes
`9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed`
(Release) and
`6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`
(Debug), with shared trace/call/write/state hashes
`0e5676d007ede57f34e878ce69b920fb8d3e112b070cd025b0f12ad38785235d`,
`32cdc094ab4f750ed568fe93598a73b401ef0688e087d8ab66ee453835227744`,
`0ff5459dd6105bc7a05a7313cc2de3f59c7e7db765de6ca60c59351485adf41e`, and
`638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab`.
Later independently timed close-state expiry, collision, animation, and
response paths remain assembly-backed.

### Fifty-fourth native replacement: semantic `02:59A5@all` opening transition

The handoff-installed slot-`$40` continuation now executes natively when its
scene latch is clear. It masks collision flags with `$BF`, clears the actor's
high state bit, assigns type `$83`, writes the `$59C4` coroutine continuation,
and submits a four-tick delay to `01:C5C9`, which leaves three ticks. The
complete 16-write, 456-master-cycle transcript contains no interrupt and
matches assembly exactly. `dc_rom_upper_wall_pocket_hand_opening` composes the
existing `$0001`-to-zero parent with this continuation; its Release census,
entry, call, write, and selected-state SHA-256 values are
`9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed`,
`9df5f42ab0789c93b6127e0d40dd029b59e6b892a5b38a5fa69c61389cb0d24d`,
`6796cbd0ac56579f11033fa2a96aca50d6a437f1eaf82d8486af89263fe5b1af`,
`959c00ca63264aeb7e49b95a29d5f46c62b93c112b9a3cf607996c97735e597a`, and
`638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab`.
The `$2D33`-set branch is now natively covered below; later
close-state/collision/animation responses remain assembly-backed.

### Fifty-fifth native replacement: semantic `02:59C7@all` opening follow-up

The hand's observed positive-state follow-up now executes natively for fixed
slot `$40`. It writes type `$82` and the one-tick `$3A40` timer, then composes
`01:C5C9` to install continuation `$59DF`. The complete no-interrupt scope is
261 master cycles with nine ordered writes and matches assembly exactly.
`dc_rom_upper_wall_pocket_hand_opening_followup` composes the existing
`02:5991 -> 02:59A5 -> 02:59C7` route. Its Release/Debug census hashes are
`9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed` and
`6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`; shared
entry/call/write/state hashes are
`b7d9ad263c05ebcbe441dd829947f36a30571ad693a4dc5e73798b62fc27ca05`,
`8096c9e92792beabce21640fce8a0eeb516577c91ce1b185e515723cbb08ad26`,
`f9b54e63be8bda6487d9499589586156d0c7bc5570dbdb3cb204f2ee2bd0c1f4`, and
`638d7d5d24d36dba5188eaefa8ae48af1b3af8275fb8b93abe1f63792f1fbbab`.
The negative-state close loop and later close-state/collision/response paths
remain assembly-backed.

### Fifty-sixth native replacement: semantic `02:59E0@all` scheduled opening expiry

The `$59DF` stack return installed by the prior follow-up is dispatched at
`$59E0`. Fixed slot `$40` now executes all five observed entries natively:
the first one-tick expiry resets `$3A40`, advances `$39C0` to appearance one,
three four-to-one timer decrements return directly, and the next zero expiry
advances appearance two before composing `00:E8B3`, `01:C021`, and `01:C5C9`.
The terminal path retains its randomized `$3DC0` delay and `$5A07` continuation.
The complete artifact has 33 ordered writes; its terminal call is exactly 825
master cycles.

`dc_rom_upper_wall_pocket_hand_opening_expiry` composes
`02:5991 -> 02:59A5 -> 02:59C7 -> 02:59E0 -> 02:5A08 -> 02:5A0B -> 02:5A1B` in both
configurations.
Its Release/Debug census hashes are
`9679914eeb1d06e2ada37a32cbb9d64fa7cb82c7a7adce7745cf74984b2068ed`,
and `6b598922dcb8e8e668c537f7461504108cf5840221cc751743021d96175f008b`;
the shared entry, call, and write hashes are
`0c42eb69a906861a5f2e6335aa08c89a72b0a5a61bd411b5f849faf04fd4a9cb`,
`40ead7d76cf9de8f1621ed2e31f1b4363f153f5e31e8ccb8f11a1dfac5f2ce89`, and
`fa415070ae64c33cf45ecb5dd700a2f3e2ae873658c1f2569eb34ef7518b3232`.

### Fifty-seventh native replacement: semantic `02:5A08@all` post-opening scheduler

`$5A07` is the JSR return address prepared by the opening-expiry path; the
shared coroutine scheduler invokes the following instruction at `$5A08`.
The fixed-slot-`$40` wrapper records the JSR's `$5A0A` return on the emulated
stack, then composes `01:C018` and native `01:C5C9`. The one observed call is
an exact 192-master-cycle, seven-write transfer that installs `$5A0B` as the
next actor dispatch. The `$2D33` guard is covered by the opening semantic's
dormant branch.

### Fifty-eighth native replacement: semantic `02:5A0B@all` post-opening state

The installed `$5A0B` dispatch now executes natively for fixed slot `$40`.
It writes the one-tick `$3A40` timer, preserves the `$3080` collision mark,
records the `$5A1A` JSR stack return, and composes `01:C018` with the native
coroutine scheduler to install `$5A1B`. The one observed call has 24 ordered
writes and a 1,170-master-cycle transcript; its 921-cycle timer IRQ occurs
inside the scheduler and is preserved by the composed instruction checkpoints.
The prior six-entry family regression has exact Release/Debug censuses and
shared entry/call/write hashes recorded above. Later collision/response paths
remain assembly-backed.

### Fifty-ninth native replacement: semantic `02:5A1B@all` post-opening countdown

The fixed slot-`$40` countdown now replaces all nine observed dispatches. Six
single-byte timer decrements return through `$5A30` in 33 cycles. Each of the
three expiries reloads four ticks at `$5A22`; the first two consume the live
collision counter and return in 96 cycles. The final zero counter resumes the
existing `$5975` live/RNG setup at its exact 81-cycle boundary, keeping that
interrupt-bearing tail in assembly. The expanded family regression observes
16 entries and has 112 ordered writes, with its established census and shared
trace/call/write hashes recorded above. Later collision/response paths remain
assembly-backed.

### Sixtieth native replacement: semantic `02:59A5@all` `$2D33` dormant guard

The same opening semantic now covers the shared-latch branch independently of
the opening transition. A controlled route asserts `$2D33` at frame 1,222 and
observes all 298 fixed-slot-`$40` calls from frames 1,223 through 1,520. Each
performs only `LDA $2D33` and a taken branch to `$59F4`: 27 master cycles, no
game-owned writes, and exact handler-owned writes when an interrupt falls in
the routine scope. `dc_rom_upper_wall_pocket_hand_guard` preserves Release /
Debug census SHA-256 values
`c12f290cd61c254e573b362ab572cb1afb19a5ddaabc27ca77bcdd19e51c71d8` and
`5ffb267191ede4e70121638ae0198463a25e95b71582d82bb6e84457bb80248e`; shared
trace/call/write/state hashes are
`3e2722d448c4e22c57bb4fb6846f4e5331ff062b55a3344c2afc95b312d57b9d`,
`d114365a0d52b054fa80394cd83a2a9d86041847c069f709d567dff02f7039d2`,
`edb384280b11fa4cab1e6527ac1ee928d13c9dd7b2753af938d76715eef38a83`, and
`eccef7f1968b9576f34ba291712d1e67d52cbf428c1c8c74cc12855c6d5c1df0`.

### Sixty-first native replacement: semantic `02:59C7@all` negative-state close loop

The shared opening-followup semantic now also owns the close-state branch.
The controlled route points fixed slot `$40` at `$59C7` and asserts its high
state bit at frame 1,221. The first call clears `$3840`, branches through
`$59BF`, writes the four-tick delay, and composes `01:C5C9` to schedule the
same continuation again. Its 1,089-master-cycle, 27-write transcript includes
the original timer IRQ through the scheduler's instruction checkpoints. The
two following positive-state calls retain their existing 261-cycle,
nine-write transcripts.

`dc_rom_upper_wall_pocket_hand_close` pins all three calls. Its Release/Debug
census hashes are
`7a3466734aa3856e9158828057ce31f7a9854d95a9ce8e9b6768b372b3bcb9df` and
`45a7a4ef3c92924574d97666aa03cfae4f36d837e2a74efc3eec71c6ecd4fba9`; the
shared trace/call/write/state hashes are
`2fbfe2597e236fc6f6249f9388f61999514985c88b339cfd9c9a4a4232542096`,
`7b7516225b1a6fe7c1612c42c386816c69cef5ca04c294b76e720e623796fb4e`,
`4514ec8143334011d1856f0163c460ce1d454601e2763307e2a1fd08cb8d352b`, and
`d9608965ca1ec3f589fe9e048c9598e8a77a9a18a09d1fdf7a1ea5bd973595f2`.
Collision/access and response branches remain assembly-backed.

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

### Big-Star initializer extension: semantic `02:565C@all`

The fixed Big-Star setup now executes natively. Its slot-constrained adapter
writes the pending feature bit, stopped arrow selector, zero angular velocity,
and speed-state timer through the exact 285-cycle, twelve-write transcript.
The final `01:C5E6` checkpoint returns at the original `01:C5E9` scheduler
boundary; subsequent Big-Star direction, rotation, collision, and scoring
paths retain their existing native gates or assembly behavior.

### Undead-Head initializer extension: semantic `03:61A0@all`

The fixed Undead Head startup now executes natively for slot `$4D`. Its
192-cycle, seven-write transcript is the shared coroutine scheduler handoff:
it preserves the caller-return stack pair, installs `03:61A3`, and returns at
`01:C5E9` with the final `01:C5E6` checkpoint. The direct stopped-selector
gate at `03:61AB` is separately native; activation, collision, RNG, scoring,
and bonus behavior remain assembly-owned.

### Undead-Head selector gate native replacement

The fixed Undead Head's direct `03:61AB` entry loads stopped-selector `$2D18`,
compares it to zero, and returns at `03:61B2` when the selector is nonzero.
Selector zero continues into the original activation path. The controlled
visible-window route sets slot `$4D`'s camera position at frame 1,200 and
observes 770 direct calls: 762 uninterrupted 27-cycle, zero-write returns and
eight interrupt-bearing passages.

Semantic `03:61AB@all` admits the fixed slot only when the selector is
nonzero. It reproduces the `LDA`/`CMP`/not-taken-branch transcript through
three instruction checkpoints and returns to the original RTS. The selector-
zero activation path remains assembly-backed. The focused Release artifact
SHA-256 values for census, entry trace, call trace, and ordered writes are:

- `cb6445df6361443a43fde2ca163b30248adb250d53b7ca4b547fad9ef900e788`
- `79d7aacec10f992d4816e1e39413216e976e5fc103f18eb7f03ac8bbbc42fef0`
- `1a633161d9aca5c50e8ff7631b3973257da06855300e918901d105bb2b7cdf9c`
- `5b3522ae147fd75be3018cd1d16afb3adcc52a09fa9a5432cae1c5e9187d8869`

### Side-Worm initializer extension: semantic `02:5C4B/5C7C@all`

The fixed left/right Side-Worm layouts now execute their mirrored setup
leaves natively. Slot `$4E` receives its state-zero, selector-six, collision,
appearance, and `02:5CC5` dispatch fields in the exact 204-cycle, ten-write
`02:5C4B` transcript; slot `$4F` receives the corresponding state-one,
selector-two fields through the 210-cycle, ten-write `02:5C7C` transcript.
Each final direct-return checkpoint preserves the original RTS boundary.
The stopped-selector gate's nonmatching return is also native; matching
activation, collision, motion, scoring, and feature outcomes remain
assembly-owned.

### Side-Worm selector gate: semantic `02:5CCD@all`

The shared left/right stopped-selector gate now executes its compact waiting
leaf natively for fixed slots `$4E/$4F`. It loads global selector `$2D18`,
compares it to the per-record `$3780,X` selector, and returns through the
original `02:5CD5` RTS only when they differ. The native transaction preserves
the exact 36-cycle, zero-write `LDA`/indexed-`CMP`/branch transcript and all
four instruction boundaries, including the intermediate LDA N/Z flags needed
by original interrupt passages. A selector match falls through to the original
active Side-Worm motion/collision path.

The controlled visible-window fixture forces both dispatch records at frame
1,200 and explicitly preserves their fixed selectors six/two. It records
1,760 calls to both `02:5CC5` and `02:5CCD`: 1,739 clean returns plus 21
interrupt-extended calls. Native census, entry, call, and ordered-write
artifacts are byte-identical:

- `297d2de96400e7e6e660a05fdb86a5dacf18a60d99a2c69b8b9423f3b777339d`
- `ae8d7416dc638972e0d830b15143111330d5d89b46879888e35f3042cdae34e8`
- `2441effa6e1eda9ffb66e611e6a966cc9a14d4c4a19a007d72317aa6b5ce320b`
- `f177c053f5332f48c3858fab3af1ceec65296d094863fbaed0c8cb838df0d1ce`

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

### Little-Skull initializer extension: semantic `02:44E4@all`

The four fixed Little-Skull layouts now execute their shared initializer
natively. The typed adapter admits only slots `$46/$47` with pair parameter
zero and `$49/$4A` with pair parameter one, preserves the pair-count byte's
eight-bit increment, and writes the exact collision, state, appearance, and
`02:4515` scheduler fields. Each no-interrupt setup remains an exact
390-cycle, fourteen-write transcript with the final `01:C5E6` checkpoint and
the original `01:C5E9` return boundary. The collision, regeneration,
gray-release, scoring, and Big-Star behavior remain assembly-owned except for
their separately mapped gates.

### Little-Skull pair initializer extension: semantic `02:4485@all`

The two fixed pair controllers now execute their shared initializer natively.
The adapter admits only slot `$45`/pair zero and slot `$48`/pair one, clears
the corresponding live-count and regeneration bytes, and schedules
`02:449A` through the exact 285-cycle, ten-write transcript. Its final
`01:C5E6` checkpoint preserves the original `01:C5E9` return boundary;
collision, regeneration, and all event-owned controller behavior remain in
assembly.

### Little-Skull hidden initializer extension: semantic `02:4455@all`

The fixed hidden transition controller now executes its one setup call
natively. The adapter accepts only slot `$44`, clears feature latch `$2CFA`,
and schedules `02:445B` through the exact 207-cycle, eight-write coroutine
transcript. The final `01:C5E6` checkpoint preserves the existing native
idle-gate boundary and all pending-feature behavior continues through its
already mapped or assembly-owned paths.

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

### Gray Little-Skull initializer extension: semantic `02:458A@all`

The targeted gray-release route enters this setup once, for left-pair slot
`$46` at frame 1,221. Semantic `02:458A@all` now reproduces its strict
291-cycle, eleven-write transaction: gray selector `$0B`, flag `$C2`, layout
Y restore, and the `01:C018` scheduler installation of existing continuation
`02:45A4`. The initializer's normalized replay and the downstream RNG
continuation retain the original ordered writes and full route artifacts.

### Gray Little-Skull release extension: semantic `02:45A4@all`

The subsequent gray-release continuation now runs natively for the observed
slot-`$46` state at frame 1,223. Its exact 768-cycle, 23-write transaction
clears gray/collision state, restores selector/timer fields, composes the
native RNG recurrence, takes the observed odd-result pending-value-two branch,
and composes the original coroutine scheduler to install `02:4515`. The
other RNG parity, collision, regeneration, and release states remain assembly
fallback.

### Little-Skull ordinary continuation: semantic `02:4515@all`

The scheduled ordinary continuation now runs natively for all live slots
`$46`, `$47`, `$49`, and `$4A` on the same natural route. It preserves the
three-write, 105-cycle position/appearance/countdown wait and composes the
ten-write, 303-cycle expiry handoff through `01:C018` and the coroutine
scheduler. All 22 direct entries execute natively: the instruction-boundary
transcript preserves slot `$49`'s periodic timer IRQ between the position and
appearance stores. The focused Release semantic regression replays the expiry
call at frame 1,227 and retains the complete census, call, and ordered-write
artifacts.

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

### Fixed scheduler initializer extension: semantic `03:7B1F@all`

The hidden slot `$5C` setup now executes natively. The typed adapter restores
its one-tick `$3A00,X` timer and preserves the exact 213-cycle, eight-write
shared coroutine-scheduler handoff to `03:7B27`, including caller-relative
stack stores. The steady selector scheduler retains its existing bounded
native-or-assembly behavior.

### Fixed marker initializer extension: semantic `03:7B6D@all`

The adjacent hidden slot `$5D` setup now also executes natively. Its typed
adapter restores marker mode `$04`, the one-tick scheduler timer, and the
exact 234-cycle, nine-write coroutine handoff to `03:7B7A`, including the
caller-relative stack transcript. The established marker animator retains
its existing bounded native-or-assembly behavior.

### Fixed response initializer extension: semantic `03:7CA7@all`

The final hidden slot `$5E` setup now executes natively. Its typed adapter
clears response state `$2D38` and collision scratch `$CD`, then preserves the
exact 219-cycle, nine-write coroutine handoff to `03:7CAF`, including the
caller-relative stack transcript. The conservative collision/scene-response
controller retains its existing bounded native-or-assembly behavior.

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
`(64,254)`, with layout initializer `03:6294` (native setup), type `$82`, initial state
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

Native semantic `03:734E/7353/735A/7361@all` now covers all four setup calls
through their exact typed queue-index and shared scheduler transcripts.

Semantic `03:7378@all` owns both the inactive wait and released activation
branches. A set bit zero clears the indexed request, disables the collision
surface, initializes motion counters, and installs `03:73A6` through the
common scheduler in an exact 363-cycle, 12-write transaction. The later
movement path now has three carry-clear fixtures, `03:73A6@2`, `03:73A6@4`,
and `03:73A6@16`: each composes the exact 165-cycle collision-free table pass
and native no-hit child. They cover the 360-cycle, nine-write timer decrement;
the 429-cycle, 11-write timer reload and phase advance; and the 438-cycle,
12-write phase wrap, respectively. Checkpointed `03:73A6@58` additionally
preserves a 1,053-cycle, 22-write timer-interrupt passage, including the
handler's `73D8` stacked return boundary. Checkpointed `03:73A6@60`
additionally composes collision child `03:6988`'s RTS into the parent and
preserves its 1,233-cycle, 27-write interrupt passage with `73AF` stacked.
Collision, multi-pass, remaining interrupt-bearing movement records, and the hit path remain
assembly-owned. The waiting branch's two JSR stack writes occur at cycle 42,
matching the original transcript.

Semantic `03:73A6@all` now runs 128 of the controlled route's 129 calls
natively, including the checkpointed `73D8`, `73AF` and `73FD` timer-IRQ
boundaries. The multi-pass restriction above is gone: the parent no longer
pins its collision child to the single 165-cycle path, because the interpreter
checkpoints every instruction it walks and the parent takes the child's own
return instruction — `$6988`, `$6994`, `$69A1` or `$6A37` — rather than
assuming `$6988`. The `73FD` boundary was added at the same time; without it
an interrupt landing on the hit gate's return stacked `73CC` instead of
`73CA`. The one remaining decline is the call whose interpreter returns carry
set and branches to `73CD`, which is untranscribed.

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
sets the wall-active bit, and installs steady dispatch `03:6339`. Semantic
`03:6319@all` now ports that sole fixed-slot entry through its exact 321-cycle,
twelve-write initialization transcript and final `01:C5E6` scheduler
checkpoint.

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
access surfaces. Semantic `02:4A44@all` now ports the sole fixed-slot setup
through its exact 273-cycle, nine-write transcript and final `02:4CE4`
checkpoint. A qualifying collision:

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
four ticks per appearance and cycles through four appearance values. Semantic
`02:4A54/4A4C@all` now ports both fixed setup calls through their exact
273-cycle, nine-write transcripts and final `02:4CE4` checkpoints.

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

`dc_accumulate_score_ordinary` and `00:F55C@all` now implement the ordinary
unsaturated path when the new score remains below the current high score.
The adapter derives the selected three-byte score, status flags, three
direct score writes, both nested JSR stack pairs, the `$2CC0` dirty write,
and the fixed 558-master-cycle budget from state. A 44-boundary transcript
spans the outer path, both nested helpers, their stack writes, and their RTS
returns. It admits all 40 census calls, including ten original
interrupt-bearing scopes, while Beetle executes each original handler at its
sampled boundary and resumes the native transcript afterward. The upgraded
`dc_rom_score` regression preserves its configuration-specific census hash
and exact 40-call/446-write hashes in Debug and Release while retaining
ordinal 1 only as a restored-assembly reference coordinate.

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

### Native ordinary drain threshold

Semantic `01:DD6E@all` now owns the ordinary ball-Y comparison at the entry
to the drain route. With session byte `$55` clear, it subtracts `$02D0` from
the live indexed `$3380:$3300` coordinate and returns locally with carry
clear below the threshold or carry set at/after it. The 3,600-frame injected
route executes all 1,744 calls natively, including the 1,680 ordinary
84-cycle, no-write returns and the frame-3,000 78-cycle, no-write drain
return. Its instruction checkpoints retain all original interrupt passages;
the census, call, and ordered-write SHA-256 values remain byte-identical to
assembly:

- Census: `9e928058264849ba617b7c36697cb24779b28a10072c6c92c6b1786d72f284a2`
- Calls: `0416efdc96c110a6a029e34d9e6c00a6f11dc338534d793cb57ed476a4ec3664`
- Writes: `d96cbef47b9f30cb83512a12eb135951ac432f6f1572308556dc2b238ccc2341`

### Native ordinary remaining-ball presentation

The selected ordinary continuation `01:C80A@all` is a branch-free coroutine
handoff. It loads the fixed 16-tick delay, uses `01:C5EA`'s return-address
rewrite to install `01:C80F` as the slot dispatch, then uses `01:C5C9` to
consume the first tick and return to the scheduler at `01:C5E9`. The native
transaction reproduces both stack pairs, `$2F:$30` scratch values, the slot's
timer/dispatch writes, and the first decrement to `$0F`: thirteen writes and
318 master cycles for the frame-3,271 call. With `01:C80A@all` enabled, the
existing session/scoring census, entry, call, and scoped-write SHA-256
artifacts remain byte-identical to assembly.

### Native ordinary transition-latch gate

Semantic `01:C7EA@all` reads the shared transition latch at `$2D32`. When it
is zero, it retains the original 15-cycle load and 12-cycle taken branch,
then composes the native `01:C80A` transaction. The resulting parent scope is
the exact 345-cycle, thirteen-write ordinary continuation. A nonzero latch
composes its 15-cycle load and 6-cycle fall-through with native `01:C7EF`'s
228-cycle latch-clear/scheduler setup. The resulting exact 249-cycle,
nine-write transaction resumes at `01:C5E9`; the later alternate presentation
body remains in assembly. The controlled alternate-latch route preserves
Release census, call, and ordered-write hashes
`987c6f07d131bae52958c71ef8345304ca7d737216687bcadd77ec03b5a742c1`,
`fde029cff362c212d8d2a000ce683e4545bab79c922d8b373ac53bebeee3149a`, and
`775e0419916ea3fe37b35e8aacf00b26b5c2c6d3d7119cc1d81f57d8f4a80e87`.
The composed Debug replay preserves census SHA-256
`78576500728927e8bd6fd5595743b04ea54f1662e092040191a4efcd9e0e36af`.
The direct `C7EF` scheduler regression records native call and ordered-write
SHA-256 values
`0da1d358166e9079b5f156af8797caa34743778f470855502183d9d8802c6e42` and
`cca4b5ac2294bbfdcdb7e0b57c6265a7be22609273d05c75fcd8886b11c61b1a` in
both configurations.

### Native ordinary remaining-ball decision

Semantic `01:C7E5@all` reads the live ball count at `$2D52`. A nonzero count
retains the 15-cycle load and 6-cycle fall-through, then composes the native
`01:C7EA` gate: its zero latch takes the exact 366-cycle, thirteen-write
ordinary route, while its nonzero latch takes the exact 270-cycle, nine-write
alternate route through `C7EF`. The zero-count branch likewise composes its
27-cycle load/taken-branch with native `01:C7F4@all` into an exact 345-cycle,
thirteen-write outer transaction. The scheduler setup loads `$FF`, installs
the `01:C7F9` dispatch through `C5EA`, consumes the first tick through
`C5C9`, and resumes at `01:C5E9`. The following game-over body remains
assembly-owned. The controlled zero-ball route preserves Release census,
call, and ordered-write hashes `669f74d514ce6af6fd70c307a74cf085d31e878506191171a134ee94bc339e09`,
`2e7b3eb8be3d9812ed1215818abc5ef91de81c20da30a48bb6cce7556a1b1109`, and
`4ee5e8268dd8511e9cdbfba1e6ee6cfb8702bc1fa7142d32843dbc531679b40a`.
The delayed `01:C7F9@all` callback now additionally composes its 21-cycle
JSR frame with the common 102-cycle non-expiring `C5C9` decrement. It covers
254 of 255 controlled callbacks through exact 123-cycle, five-write scopes;
the one countdown-expiry transition remains assembly-owned. Its Release
census, call, and ordered-write SHA-256 values are
`669f74d514ce6af6fd70c307a74cf085d31e878506191171a134ee94bc339e09`,
`ec3b772ba19dafa3da83c178113f9e62b3bd0ac553c188636468caf06717fc25`, and
`325ba76f5ed1485b32aad7ab22fa88ef68ae6d711e5e80b459f852f759b9f067`.
The resulting `01:C7FC@all` expiry callback shares the alternate scheduler's
common tail and natively owns its exact 201-cycle, eight-write setup before
resuming at `C5E9`. Its Release call and ordered-write hashes are
`19e0a4a5b8f530b3b5b9eaf0ab7659841b1c734a7eece9cdae002e8b560337fe` and
`d67e1246845ada7d4d062b27cbded1e1368b4cfbd62cbafb4e6d654de77f9ce3`.
With `$2D53` clear, `01:C804@all` now composes its 27-cycle latch test and
taken branch with `C80A`'s presentation transaction, matching the exact
345-cycle, thirteen-write continuation. The controlled earlier-drain route
has Release census, call, and ordered-write SHA-256 values
`4f08a0d6040481044c9896ab9fd3b7123cf80b1f858b2311bc435ef06ea86416`,
`ca1e7bd4b5ee0a0bae38692dca4eb996e4490adb6839857a6f58f00493f2adfd`, and
`0d7584c7b60f3373d8e2bf04445af858cf8f896dff00781492724a4416eb525a`.
The nonzero-latch return remains assembly-owned.
The following zero-count `01:C812@all` callback now owns its exact 237-cycle,
eight-write `$53.5` state-flag and `C841` scheduler setup, while its nonzero
reload path remains assembly-owned. The same controlled route records call
and ordered-write SHA-256 values
`9ef9f47d2561659ef7fb75ea72bbee48716f15c6e0f7c018489601f589699334` and
`73f499d862d9c089d4865ab1d3ec24c8719d15cec2d566fed33fda46d6c9843b`.
The paired Debug audit observes one `C804` and one `C812` entry and matches
the original assembly census, call, and ordered-write artifacts exactly:
`baf530269ea4effe1c42aaead83c7f1f18b953e992d890c0dc08a4906a3a851c`,
`fc7e46a8258045229faea9a87070da05f358a0197b07701c3981f7feec95ad52`, and
`d4bf75fdea28df80ea85151ef0d4c53e10dc8767ed0dd6009e1b3fe405727de6`.
The controlled nonzero-ball alternate outer regression preserves the same
Release census and records native call and ordered-write SHA-256 values
`c7922d14f6ac0cf273211f8f457c4ac2252027b06752824d060eeead5e4c9522` and
`a0dfe62a7f2fddef735dad6fc31e9d2bf009d67e9da2011d49dd900d96650a7e`.
The direct `C7F4` replacement regression preserves that census and records
native call and ordered-write SHA-256 values
`80feb43474cb780365d2d36701379863e05de79a35c193aa6a61120d2ed16771` and
`773ab75800ad5b961c131313e5499bf04735c62e0471890cd63f1ad80aada1bf`.
Its paired Debug replay has census SHA-256
`000f85c840f1ae52475746d8e8af723c9509a24b37a28baa4e9c4b895c814b7d`
and the same native call/write hashes.

With this outer decision enabled, the session/scoring observer intentionally
records `C7E5` as the composed scope rather than redundant nested `C7EA` and
`C80A` calls. The gameplay census and work-RAM artifacts remain unchanged; the
bounded observer artifact contains 400 entries/calls and 27,743 scoped writes:

- Entry SHA-256:
  `708911509d4c40cbf8cedb221d68ee4b1ccdfa2338354b0419fc3e57a3e942ea`
- Call SHA-256:
  `4e47497731f54305d39017e573c2f686145afe34b7a678878a726de289162d92`
- Scoped-write SHA-256:
  `c3555460f6a4c7146fb4ea90b8faa82de50c7427769c20bb93ce2e1b86ccc735`

The nonzero-`$55` alternate threshold, the preceding session and ball-count
decisions, and later table-reload and panel paths remain assembly-backed.

### Preceding transition/ball-count boundary screen

An assembly-only replay after the `C7E5` composition retained the nested
ordinary-continuation observers and measured 402 calls/27,769 scoped writes.
It confirms that the immediately preceding `01:C786`, `01:C7B0`, and
`01:C7BF` entries are not independent prefix candidates on this route.
`C786` writes session bit `$53.1` but, with `$55` clear, enters shared
`00:ECFD` before its `C5EA/C5C9` coroutine handoff. The one-player `C7B0`
wrapper reaches `C7BF`; that routine decrements `$2D52` and then follows the
same `00:ECFD`/scheduler path. Their measured 2,877/4,806/4,752-cycle scopes
contain 83/114/114 ordered writes and the original interrupt passage. They
remain assembly-backed until the shared presentation helper can be composed
from its own transcript or another natural route reaches a local return
branch.

The repeated score-panel leaf `09:72FD` was originally screened out because
its dominant 255-call, 699-cycle/9-work-RAM-write form directly emits VDC-port
commands through `$0000/$0002/$0003`. The bounded hardware-port channel now
supports it: all 376 natural calls execute through an exact 56-boundary native
transcript. Its `09:7266` parent remains in assembly.

`tools/Analyze-SessionScoringRoute.ps1` and `dc_rom_session_scoring` guard the
ROM gates, complete work-RAM timeline, 400 globally ordered entries and
bounded calls, 27,743 scoped writes, score reconstruction, panel dirty-bit
handoff, exact digit streams, and continuation:

- Work-RAM SHA-256:
  `229ffbff969db6067e2adb10b7f05523f048c7be1539fc2188499ca62f18ab5b`
- Entry SHA-256:
  `708911509d4c40cbf8cedb221d68ee4b1ccdfa2338354b0419fc3e57a3e942ea`
- Call SHA-256:
  `4e47497731f54305d39017e573c2f686145afe34b7a678878a726de289162d92`
- Scoped-write SHA-256:
  `c3555460f6a4c7146fb4ea90b8faa82de50c7427769c20bb93ce2e1b86ccc735`

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

`dc_advance_rng` and semantic `00:E8B3@all` implement that recurrence for
arbitrary live seed/frame state. The adapter emits the exact six scratch
writes and two seed writes, returns the new high byte with carry,
overflow, negative, and zero flags derived from the final instructions, and
uses the fixed 282-master-cycle budget. Its 23 original instructions are
represented by 23 interrupt checkpoints, so all 78 route calls now enter the
replacement rather than the earlier 38 (48.7%). The upgraded `dc_rom_rng`
regression retains ordinal 1 only as a restored-assembly coordinate and
preserves the configuration-specific census plus exact entry, call, write,
and selected seed-write hashes in Debug and Release.

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

The exact `D1A2@1` write transcript includes three timer-interrupt handler
passages, identifiable through their bank-0 `FCxx`/`FDxx` instruction PCs
and stack/scratch writes. It is a valid deterministic replay sample, but not
a valid work-RAM-only replacement target: copying its writes without
delivering and acknowledging those interrupts leaves the timer IRQ pending
at the outer RTS. It therefore remained emulated until the twentieth
replacement composed instruction-checkpointed semantic children.

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

### Second native replacement: `00:F1B4@1`, `00:F1B4@10`, and `00:F1B4@17`

Milestone 4 begins with the frame-1,064 position integration immediately
before the existing native rollback. `dc_integrate_position` in
`src/game_logic/physics.c` caches both original signed 16.8 positions,
sign-extends and adds the two signed 8.8 increments, and applies the
fractional-only movement marker at `$3C80,X`.

Independently replayed ordinal 17 has a negative second increment, a
nonnegative first increment, no integer-position change, 714 body master
cycles, and 17 ordered writes. The transaction
preserves the observed `A=$02`, `X=$00`, `Y=$06`, `P=$01`, `SP=$F9`, all MPRs,
and outer RTS at `00:F240`.

Independently replayed ordinal 1 admits the nonnegative counterpart at frame
1,049. Second position `$028F00` receives increment `$0010`; first position
`$010E00` receives zero. Neither integer portion changes, so the final motion
flag is again 2. Both sign branches are nonnegative, omitting ordinal 17's
`F1E0` `$2B=$FF` scratch store and shifting the remaining stores 12 cycles
earlier. The exact path performs 16 writes and resumes at `00:F240` after 702
body master cycles with `A=$02`, `X=$00`, `Y=$00`, `P=$01`, and `SP=$F9`.
Every MPR and complete post-call RAM match restored assembly.

Ordinal 10 at frame 1,058 admits the first integer-position-change branch.
Its nonnegative `$00A2` second increment advances `$0291D1` to `$029273`.
Because the integer low byte changes from `$91` to `$92`, the ROM comparison
exits before the final `$3C80` fractional-only marker store. The transaction
has 15 writes, returns `A=$91`, `P=$80`, and reaches outer RTS `00:F240`
after 639 body master cycles. Complete post-call RAM and all MPRs match
restored assembly.

The focused ordinals remain restored-assembly fixtures, but admission is no
longer ordinal-gated. Semantic `00:F1B4@all` derives both sign-extension
scratch stores, all six position-store offsets, the first changed integer
comparison, final flags, and one of the observed 639/651/702/714-cycle
budgets from the live slot-zero state. Its path-sensitive 50-57-boundary
transcripts span both sign branches, all six stores, and the first changed
integer comparison. The 1,800-frame aggregate executes all 1,156 calls
natively, including 250 scopes containing original interrupt-handler
execution, while preserving the established gameplay, complete-call, and
21,738-write SHA-256 values.

The adjacent `00:F241` rollback was already a state-generic six-store
transaction. Its six load/store pairs now supply 12 instruction-boundary
checkpoints, so semantic `00:F241@all` executes all 404 calls rather than the
earlier 242 (59.9%), preserving byte-identical gameplay plus the complete
404-call/2,634-write artifacts.

`dc_rom_native_replacement` now executes native `F1B4@17` followed by native
`F241@1`, then restores both pre-call savestates and runs the original
assembly for shadow comparison. The 23 writes and 876 combined body master
cycles match exactly, complete post-call work RAM is identical, and the
established gameplay, entry, call, and write SHA-256 values above remain
unchanged.

The focused `dc_rom_position_integration_positive` regression replaces
ordinal 1 and restores its exact pre-call state for assembly replay. Debug
and Release preserve the gameplay hash and complete
1,156-call/21,738-write F1B4 trace hashes.

The companion `dc_rom_position_integration_integer_step` regression replaces
ordinal 10 and restores its frame-1,058 state. Debug and Release preserve
the same gameplay, complete call-trace, and ordered-write hashes while
covering the distinct early-return register contract.

### Initial third replacement fixtures: `01:DD09@1`, `@2`, and `@181`

The next bounded slice is the short frame-1,048 damping/acceleration call.
`dc_damp_and_accelerate_motion` in `src/game_logic/physics.c` implements the
complete pure calculation: each nonzero signed 8.8 component subtracts its
signed quotient truncated toward zero using `$8C` or `$8D`, then the unsigned
low-byte term `$8B` is added to the second component.

The first independently replayed call is ordinal 1. Both components are zero
on this call, so the branches through signed divider `00:E905` are skipped.
With `$8B=$10`, the exact HuC6280-visible contract is:

- `01:DD63` writes `$10` to `$3500,X` at body cycle 132.
- `01:DD6A` writes `$00` to `$3580,X` at body cycle 168.
- Input `A=$00`, `X=$00`, `Y=$00`, `P=$43`, and `SP=$F9` becomes
  `A=$00`, `X=$00`, `Y=$00`, `P=$02`, and `SP=$F9`; all MPRs are preserved.
- Assembly resumes at outer RTS `01:DD6D` after 168 body master cycles.

Independently replayed ordinal 2 admits the first nonzero divider path. At
frame 1,049, axis one is `$0000`, axis two is `$0010`, both divisors are
`$80` (-128 signed), and acceleration is `$10`. Axis one skips division.
Axis two runs the complete `00:E905` signed-divider sequence; truncating
`16 / -128` toward zero yields zero, so damping leaves `$0010` before
acceleration produces `$0020`.

The exact path contains 68 ordered writes over 2,409 body master cycles,
including the divider's zero-page and stack scratch transcript but no
`FCxx`/`FDxx` interrupt-owned writes. Input `A=$01`, `X=$00`, `Y=$00`,
`P=$03`, and `SP=$F9` becomes `A=$00`, `P=$02`; X, Y, SP, every MPR, outer
RTS `01:DD6D`, and complete post-call RAM match restored assembly.

Independently replayed ordinal 181 admits the negative signed-divider branch
at frame 1,228. Axis one remains zero. Axis two begins at `$FF0D` (-243);
division by signed `$80` (-128) produces truncating quotient 1, damping
restores the negative result `$FF0C`, and `$10` acceleration yields `$FF1C`.
The exact path contains 69 ordered writes over 2,427 body master cycles,
including the divider's sign-restoration store at `00:E8FA`. Input
`A=$01`, `X=$00`, `Y=$06`, `P=$03`, and `SP=$F9` becomes `A=$FF`, `P=$80`;
X, Y, SP, every MPR, outer RTS `01:DD6D`, and complete RAM match.

Ordinal 52 was initially classified as clean because its natural trace did
not visit the known `FCxx/FDxx` handler range. Normalized replay revealed a
different interrupt path: interrupt-owned stack and controller writes enter
through `00:E058/E169`, shifting the signed-divider scratch transcript even
though final registers, cycle count, and write count agree. This originally
kept the call emulated; the later semantic `E905`/`DD09` composition below
now supplies its exact instruction checkpoints.

`dc_rom_native_replacement` now runs native `DD09@1`, `F1B4@17`, and
`F241@1`, then restores all three checkpoints for assembly replay. All 25
ordered writes and 1,044 combined body master cycles match, complete
post-call work RAM is identical for every checkpoint, and the established
gameplay, entry, call, and write SHA-256 values above remain unchanged.

The focused `dc_rom_motion_damping_nonzero` regression replaces ordinal 2,
restores the same pre-call state, and executes original assembly for shadow
comparison. Debug and Release preserve the established gameplay hash and
the complete 753-call/61,291-write DD09 trace hashes.

The companion `dc_rom_motion_damping_negative` regression replaces ordinal
181 and restores its frame-1,228 state. Debug and Release preserve the same
gameplay, complete call-trace, and ordered-write hashes while covering the
negative-result register and scratch-write contract.

### Fourth native replacement: `01:D1A2@23`

`dc_rotate_motion` completes the pure C reconstruction of `01:D1A2`'s
signed 8.8 rotation. The ROM multiplier converts each operand to magnitude,
multiplies, truncates the fractional product, reapplies the sign, and the
caller combines those products as `u*c-v*s` and `u*s+v*c`.

The first replacement-safe call is independently replayed ordinal 23 at
frame 1,099. It has identity coefficients `$0100/$0000`, maps
`$0000/$FFFF` to the same two outputs, executes in 5,076 body master cycles,
and performs 190 ordered work-RAM writes. Its transcript contains no
`FCxx`/`FDxx` interrupt-handler passage. The adapter preserves input/output
`A=$FF`, `X=$00`, `Y=$02`, `P=$80`, `SP=$F5`, every MPR, and resumes at
outer RTS `01:D227`.

The downstream core patch also avoids calling the hardware synchronizer for
a zero-cycle delta when writes share a timestamp or the final write lands
exactly on the body boundary. That prevents an already-pending event from
being sampled at an artificial boundary. The patch was reapplied from a
clean archive of the pinned upstream revision and matches the generated
source exactly.

`dc_rom_native_replacement` now runs four native calls and restores all four
pre-call checkpoints for assembly replay. Their 215 ordered writes and 6,120
combined body master cycles match exactly; complete post-call work RAM is
identical, and the established gameplay, entry, call, and write SHA-256
values above remain unchanged.

### Fifth native replacement: semantic `03:7FB1@all`

The first native collision slice is the Small-Soldier response-state helper.
`dc_advance_collision_response` implements the complete leaf calculation:
it clears event bit 0 and doubles the result, adds the low two actor-phase
bits, writes the response, decrements the actor countdown, and reloads the
countdown to 4 while advancing the phase when it reaches zero.

The first independently replayed call is ordinal 1. At frame 957, slot `$62`
has event `$0F`, phase 0, and countdown 4. The exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:7FB7` | `$2026` | `$1C` response base |
| 93 | `03:7FC1` | `$39E2` | `$1C` actor response |
| 114 | `03:7FC4` | `$3A62` | `$03` countdown |

Input `A=$00`, `X=$62`, `Y=$01`, `P=$10`, and `SP=$F9` becomes
`A=$1C` with the other registers, status, and all MPRs preserved. Beetle
resumes at outer RTS `03:7FD1` after 126 body master cycles.

Ordinal 2 at the same frame was not safe under the atomic bridge. Its 819-cycle
transcript includes 13 interrupt-owned stack, timer, and controller writes
before the same three leaf stores. Normalized replay detects that shifted
interrupt passage. The checkpointed aggregate replacement now admits this
coordinate while preserving those original-handler writes. Direct `@2`
remains gated because restored replay rebases the frame clock and relocates
the periodic interrupt; `@all` exercises it at its natural clock position.

Independently replayed ordinal 3 admits the next interrupt-free non-reload
call. At frame 958, slot `$62` again has event `$0F` and phase 0, while its
countdown is now 3. Input `A=$80`, `X=$62`, `Y=$81`, `P=$12`, and `SP=$F9`
produces the same response `$1C`, decrements the countdown to 2, and exits
with `P=$10`. Its three stores remain at body cycles 39/93/114 and it
resumes at `03:7FD1` after 126 cycles.

Independently replayed ordinal 7 admits the other bounded leaf branch. At
frame 960 the same slot and event enter with phase 0 and countdown 1:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:7FB7` | `$2026` | `$1C` response base |
| 93 | `03:7FC1` | `$39E2` | `$1C` actor response |
| 114 | `03:7FC4` | `$3A62` | `$00` decremented countdown |
| 141 | `03:7FCB` | `$3A62` | `$04` reloaded countdown |
| 162 | `03:7FCE` | `$3CE2` | `$01` advanced phase |

Input `A=$80`, `X=$62`, `Y=$81`, `P=$12`, and `SP=$F9` becomes `A=$04`,
`P=$10`; X, Y, SP, and every MPR are preserved. Beetle resumes at the same
outer RTS after 162 body master cycles. The initial shadow run caught a
branch-specific adapter error that returned response `$1C` rather than the
reload value `$04`; the corrected result now matches restored assembly.

The 3,600-frame `dc_rom_collision_boundary` regression now executes this
native transaction before restoring the same pre-call state for assembly
replay. The complete census, work-RAM timeline, 97,379-entry/call trace,
1,163,259 scoped writes, and selected collision-state artifacts retain their
established SHA-256 values in Release. A focused Debug route independently
passes the same frame-957 replacement and restored-assembly comparison
across all 21,987 observed `7FB1` calls.

The separate `dc_rom_collision_response_reload` regression replaces ordinal
7 and restores the same pre-call state for assembly replay. Debug and Release
retain their established configuration-specific state-census hashes and the
complete 21,987-call/91,295-write `7FB1` trace hashes.

The companion `dc_rom_collision_response_followup` regression applies the
same proof to ordinal 3. Release completes the route with the same census,
call, and write hashes; Debug retains its expected configuration-specific
census hash while matching the complete semantic traces.

The focused ordinals now serve as branch fixtures rather than admission
rules. `03:7FB1@all` accepts any slot and derives the response, countdown,
optional reload and phase increment, output accumulator/status, ordered
writes, and 126/162-cycle budget from the copied work-RAM state. The ordinary
path declares 11 instruction checkpoints and the reload path 14, including
the original IRQ-sampling offsets and complete CPU/MPR state. Across the
3,600-frame census this admits all 21,987 calls. Of those, 1,042 retain
longer original interrupt-handler passages.
`dc_rom_collision_response_semantic` preserves the configuration-specific
census hash and the exact 21,987-call/91,295-write hashes in Debug and
Release.

### Sixth native replacement: semantic `03:6966@all`

The next native collision slice enters the shared actor collision-table
interpreter itself. `dc_select_collision_table_pass` implements `03:6966`'s
negative-state pass selection: it preserves the high active bit, alternates
the low seven-bit pass index, and reports the early return when that index
wraps to zero. The project-owned adapter supplies the selector's ROM table
pointer, keeping ROM metadata out of the pure work-RAM function.

`dc_advance_collision_table_record` extends that boundary through one
nonterminal record. It applies the record's event/orientation update, signed
horizontal and vertical unit-motion flags with carry or borrow, and the two
record-cursor increments while still leaving the caller to commit writes.

The original ordinal gates below are retained as exact restored-assembly
fixtures, not as the broad scheduler. Semantic `03:6966@all` now reads the
selector pointer from the supported user ROM and accepts both the `$81->$80`
alternate-pass early return and ordinary `$80->$81` headerless record steps
for every observed selector and actor slot. The latter resolve the
collision-list cursor, table entry, motion pointer, and motion cursor from
ROM, then emit either or both signed axes with exact high-byte carry/borrow
timing. The alternate-pass path supplies all 17 original instruction
checkpoints, including interrupt-sampling offsets and complete CPU/MPR state.
Beetle can therefore run original interrupt handlers inside that
transaction. Other paths use conservative master-cycle distances to
Beetle's next user/video event and next serviceable timer interrupt; if the
native body reaches either horizon, the callback returns no transaction and
assembly executes normally. Harmless timer-divider ticks can be crossed.
This avoids both a per-occurrence allowlist and a hand-maintained interrupt
ordinal blacklist.

The bounded walker also follows `$C0` motion terminators, applies `$FE`
collision-list wraps, recognizes `$FF` table termination, and resumes into
headered records with exact accumulated cursor-write timing. The former
compiled selector/slot/ordinal proof matrix is retired; its ordinal cases
remain only as restored-assembly regression coordinates.

The generic forward/reverse one-axis headerless family additionally supplies
all 50 executed instruction boundaries, including its direction-sensitive
INC/DEC path. Ordinary two-axis records without a high-byte carry or borrow
reuse the same state-based model and extend the transcript to 55 boundaries
by executing both direction branches in their original order. Across the
1,800-frame route, semantic scheduling executes 18,806 of 19,648 calls
natively (95.7%). The aggregate Debug and Release regression preserves
the established gameplay SHA-256, the complete 19,648-call trace SHA-256,
and all 180,389 ordered writes. Interrupt-bearing ordinals 11 and 13 retain
their original transcripts through automatic fallback.

The independently replayed first call occurs at frame 873. Selector `$2A`
and table entry `$03` resolve `$6C2A->$6D56`; X selects fixed Front-Spider
slot `$26`. Its collision state is `$80`, event orientation is `$0C`, record
cursor is zero, and horizontal low motion is `$40`. The exact full-record
transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F5` | `$10` saved status |
| 135 | `03:697F` | `$38A6` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$56` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 420 | `03:69D9` | `$3AA6` | `$02` event |
| 480 | `03:69EE` | `$3D26` | `$8C` orientation |
| 501 | `03:69F1` | `$3EA6` | `$01` cursor after header |
| 573 | `03:69FF` | `$31A6` | `$41` horizontal low |
| 657 | `03:6A33` | `$3EA6` | `$02` cursor after delta |

Input `A=$2A`, `X=$26`, `Y=$00`, `P=$02`, and `SP=$F5` becomes
`A=$00`, `X=$26`, `Y=$01`, `P=$10`, and `SP=$F5`; every MPR is preserved.
Beetle resumes at `03:6A37` after 663 body master cycles. Neither the natural
scope nor restored replay includes an interrupt-owned write.

Ordinal 12 is the second headerless continuation. Selector `$2C` and table
entry `$05` resolve `$6C30->$6D7A`; slot `$28` enters with cursor 2 and
consumes the current `$08` stream byte:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$30` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$38A8` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$7A` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 489 | `03:6A1E` | `$3328` | `$84` vertical low |
| 534 | `03:6A33` | `$3EA8` | `$03` cursor |

Input `A=$2C`, `X=$28`, `Y=$00`, `P=$02`, and `SP=$F9` becomes
`A=$08`, `X=$28`, `Y=$02`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Beetle resumes at `03:6A37` after 540 body master cycles. Neither the natural
scope nor restored replay includes an interrupt-owned write.

Ordinal 14 is the alternate-selector early return at frame 943. Caller
`03:5890` supplies `A=$06`, which resolves to table pointer `$6B0A`; X
selects slot `$17`, whose collision pass state is `$81`. Its exact
transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$0A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6B` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$3897` | `$80` alternate-pass state |

Input `A=$06`, `X=$17`, `Y=$75`, `P=$00`, and `SP=$F9` becomes
`A=$80`, `X=$17`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index returns at `03:6988` after 165 body master
cycles. No interrupt-owned write appears in either the natural scope or its
restored replay.

Ordinal 15 returns to the fixed-selector family later in frame 943. Input
`A=$2A` resolves to table pointer `$6C2A`; X selects Front-Spider slot
`$26`, whose collision pass state is again `$81`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A6` | `$80` alternate-pass state |

Input `A=$2A`, `X=$26`, `Y=$00`, `P=$02`, and `SP=$F9` becomes
`A=$80`, `X=$26`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index returns at `03:6988` after 165 body master
cycles. No interrupt-owned write appears in either the natural scope or its
restored replay.

Ordinal 16 is the next repeated fixed-selector return in frame 943. Input
`A=$2B` resolves to table pointer `$6C2D`; X selects Front-Spider slot
`$27`, whose collision pass state is `$81`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2D` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A7` | `$80` alternate-pass state |

Input `A=$2B`, `X=$27`, `Y=$01`, `P=$10`, and `SP=$F9` becomes
`A=$80`, `X=$27`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index returns at `03:6988` after 165 body master
cycles. No interrupt-owned write appears in either the natural scope or its
restored replay.

Ordinal 17 is the third repeated fixed-selector return in frame 943. Input
`A=$2C` resolves to table pointer `$6C30`; X selects fixed slot `$28`, whose
collision pass state is `$81`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$30` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A8` | `$80` alternate-pass state |

Input `A=$2C`, `X=$28`, `Y=$00`, `P=$12`, and `SP=$F9` becomes
`A=$80`, `X=$28`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The earlier matching selector return enters with `P=$02`, so the native
guard explicitly admits both independently replayed status variants. The
zero alternate-pass index returns at `03:6988` after 165 body master cycles.
No interrupt-owned write appears in either the natural scope or its restored
replay.

Ordinal 18 is the fourth repeated fixed-selector return in frame 943. Input
`A=$2D` resolves to table pointer `$6C35`; X selects fixed slot `$29`, whose
collision pass state is `$81`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$35` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A9` | `$80` alternate-pass state |

Input `A=$2D`, `X=$29`, `Y=$01`, `P=$00`, and `SP=$F9` becomes
`A=$80`, `X=$29`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The earlier matching selector return enters with `P=$10`, so the native
guard explicitly admits both independently replayed status variants. The
zero alternate-pass index returns at `03:6988` after 165 body master cycles.
No interrupt-owned write appears in either the natural scope or its restored
replay.

Ordinal 19 is the alternate-selector reverse-vertical continuation at frame
944. Selector `$06` and table entry `$0B` resolve `$6B0A->$6DE6`; slot
`$17` enters with cursor 2 and consumes headerless motion byte `$10`. Its
exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$0A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6B` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$3897` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$E6` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 522 | `03:6A30` | `$3317` | `$C3` vertical low |
| 543 | `03:6A33` | `$3E97` | `$03` cursor |

Input `A=$06`, `X=$17`, `Y=$75`, `P=$00`, and `SP=$F9` becomes
`A=$C4`, `X=$17`, `Y=$02`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Event and orientation remain unchanged, vertical low changes `$C4->$C3`,
and the cursor advances once from 2 to 3. Beetle resumes at `03:6A37` after
549 body master cycles. Neither the natural scope nor restored replay
includes an interrupt-owned write.

Ordinal 20 is the headerless horizontal continuation later in frame 944.
Selector `$2A` and table entry `$03` resolve `$6C2A->$6D56`; Front-Spider
slot `$26` enters with cursor 3 and consumes motion byte `$20`. Its exact
transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$38A6` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$56` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 450 | `03:69FF` | `$31A6` | `$42` horizontal low |
| 534 | `03:6A33` | `$3EA6` | `$04` cursor |

Input `A=$2A`, `X=$26`, `Y=$00`, `P=$02`, and `SP=$F9` becomes
`A=$00`, `X=$26`, `Y=$03`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Event and orientation remain unchanged, horizontal low changes `$41->$42`,
and the cursor advances once from 3 to 4. Beetle resumes at `03:6A37` after
540 body master cycles. Neither the natural scope nor restored replay
includes an interrupt-owned write.

Ordinal 21 is the reverse-horizontal continuation later in frame 944.
Selector `$2B` and table entry `$07` resolve `$6C2D->$6D9E`; Front-Spider
slot `$27` enters with cursor 3 and consumes motion byte `$40`. Its exact
transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2D` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$38A7` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$9E` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 483 | `03:6A11` | `$31A7` | `$D6` horizontal low |
| 543 | `03:6A33` | `$3EA7` | `$04` cursor |

Input `A=$2B`, `X=$27`, `Y=$01`, `P=$00`, and `SP=$F9` becomes
`A=$00`, `X=$27`, `Y=$03`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Event and orientation remain unchanged, horizontal low changes `$D7->$D6`,
and the cursor advances once from 3 to 4. Beetle resumes at `03:6A37` after
549 body master cycles. Neither the natural scope nor restored replay
includes an interrupt-owned write.

Ordinal 22 is the later vertical continuation in frame 944. Selector `$2C`
and table entry `$05` resolve `$6C30->$6D7A`; fixed slot `$28` enters with
cursor 3 and consumes motion byte `$08`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$30` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$38A8` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$7A` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 489 | `03:6A1E` | `$3328` | `$85` vertical low |
| 534 | `03:6A33` | `$3EA8` | `$04` cursor |

Input `A=$2C`, `X=$28`, `Y=$00`, `P=$12`, and `SP=$F9` becomes
`A=$08`, `X=$28`, `Y=$03`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Event and orientation remain unchanged, vertical low changes `$84->$85`,
and the cursor advances once from 3 to 4. Beetle resumes at `03:6A37` after
540 body master cycles. Neither the natural scope nor restored replay
includes an interrupt-owned write.

Ordinal 23 is the matching second later vertical continuation at the end of
frame 944. Selector `$2D` and table entry `$05` resolve `$6C35->$6D7A`;
fixed slot `$29` enters with cursor 3 and consumes motion byte `$08`. Its
exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$35` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$38A9` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$7A` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 489 | `03:6A1E` | `$3329` | `$85` vertical low |
| 534 | `03:6A33` | `$3EA9` | `$04` cursor |

Input `A=$2D`, `X=$29`, `Y=$01`, `P=$00`, and `SP=$F9` becomes
`A=$08`, `X=$29`, `Y=$03`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Event and orientation remain unchanged, vertical low changes `$84->$85`,
and the cursor advances once from 3 to 4. Beetle resumes at `03:6A37` after
540 body master cycles. Neither the natural scope nor restored replay
includes an interrupt-owned write.

The adjacent independently replayed second call shares frame 873. Selector
`$2B` and table entry `$07` resolve `$6C2D->$6D9E`; X selects the other
fixed Front-Spider slot `$27`. Its collision state is `$80`, event
orientation is `$0C`, record cursor is zero, and horizontal low motion is
`$D8`. Its reverse-motion transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2D` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F5` | `$10` saved status |
| 135 | `03:697F` | `$38A7` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$9E` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 420 | `03:69D9` | `$3AA7` | `$06` event |
| 480 | `03:69EE` | `$3D27` | `$8C` orientation |
| 501 | `03:69F1` | `$3EA7` | `$01` cursor after header |
| 606 | `03:6A11` | `$31A7` | `$D7` horizontal low |
| 666 | `03:6A33` | `$3EA7` | `$02` cursor after delta |

Input `A=$2B`, `X=$27`, `Y=$01`, `P=$10`, and `SP=$F5` becomes
`A=$00`, `X=$27`, `Y=$01`, `P=$10`, and `SP=$F5`; every MPR is preserved.
Beetle resumes at `03:6A37` after 672 body master cycles. The reverse
horizontal branch accounts for the nine-cycle difference from ordinal 1,
and neither the natural scope nor restored replay includes an
interrupt-owned write.

The third independently replayed call is the next frame-873 transaction.
Selector `$2C` and table entry `$05` resolve `$6C30->$6D7A`; X selects fixed
slot `$28`. Its collision state is `$80`, event orientation is `$0C`,
record cursor is zero, and vertical low motion is `$82`. Its transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$30` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F5` | `$10` saved status |
| 135 | `03:697F` | `$38A8` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$7A` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 420 | `03:69D9` | `$3AA8` | `$04` event |
| 480 | `03:69EE` | `$3D28` | `$8C` orientation |
| 501 | `03:69F1` | `$3EA8` | `$01` cursor after header |
| 612 | `03:6A1E` | `$3328` | `$83` vertical low |
| 657 | `03:6A33` | `$3EA8` | `$02` cursor after delta |

Input `A=$2C`, `X=$28`, `Y=$00`, `P=$12`, and `SP=$F5` becomes
`A=$08`, `X=$28`, `Y=$01`, `P=$10`, and `SP=$F5`; every MPR is preserved.
Beetle resumes at `03:6A37` after 663 body master cycles. Neither the natural
scope nor restored replay includes an interrupt-owned write.

The fourth independently replayed call completes the adjacent frame-873
quartet. Selector `$2D` and table entry `$05` resolve `$6C35->$6D7A`; X
selects fixed slot `$29`. Its collision state is `$80`, event orientation is
`$0C`, record cursor is zero, and vertical low motion is `$82`. Its
transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$35` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F5` | `$10` saved status |
| 135 | `03:697F` | `$38A9` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$7A` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 420 | `03:69D9` | `$3AA9` | `$04` event |
| 480 | `03:69EE` | `$3D29` | `$8C` orientation |
| 501 | `03:69F1` | `$3EA9` | `$01` cursor after header |
| 612 | `03:6A1E` | `$3329` | `$83` vertical low |
| 657 | `03:6A33` | `$3EA9` | `$02` cursor after delta |

Input `A=$2D`, `X=$29`, `Y=$01`, `P=$10`, and `SP=$F5` becomes
`A=$08`, `X=$29`, `Y=$01`, `P=$10`, and `SP=$F5`; every MPR is preserved.
Beetle resumes at `03:6A37` after 663 body master cycles. Neither the natural
scope nor restored replay includes an interrupt-owned write.

The independently replayed fifth call occurs at frame 941. Input `A=$2A`
selects table pointer `$6C2A`, X selects fixed Front-Spider slot `$26`, and
the slot's collision pass state is `$81`. The exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A6` | `$80` alternate-pass state |

Input `A=$2A`, `X=$26`, `Y=$00`, `P=$02`, and `SP=$F9` becomes
`A=$80`, `X=$26`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index takes the early return at `03:6988` after 165
body master cycles. No interrupt-owned write appears in either the natural
scope or its restored replay.

The independently replayed sixth call is the adjacent frame-941 return.
Input `A=$2B` selects table pointer `$6C2D`, X selects fixed Front-Spider
slot `$27`, and the slot's collision pass state is `$81`. Its exact
transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2D` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A7` | `$80` alternate-pass state |

Input `A=$2B`, `X=$27`, `Y=$01`, `P=$10`, and `SP=$F9` becomes
`A=$80`, `X=$27`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index returns at `03:6988` after 165 body master
cycles. No interrupt-owned write appears in either the natural scope or its
restored replay.

Ordinal 9 leaves the fixed-slot selector family at frame 942. Caller
`03:5890` supplies `A=$06`, which resolves table entry `$0B` at `$6B0A` to
motion record `$6DE6`; X selects slot `$17`. Its collision state is `$80`,
event orientation is `$0D`, record cursor is zero, and horizontal low motion
is `$F6`. Its reverse full-record transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$0A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6B` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$3897` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$E6` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 420 | `03:69D9` | `$3A97` | `$0A` event |
| 480 | `03:69EE` | `$3D17` | `$8D` orientation |
| 501 | `03:69F1` | `$3E97` | `$01` cursor after header |
| 606 | `03:6A11` | `$3197` | `$F5` horizontal low |
| 666 | `03:6A33` | `$3E97` | `$02` cursor after delta |

Input `A=$06`, `X=$17`, `Y=$75`, `P=$00`, and `SP=$F9` becomes
`A=$00`, `X=$17`, `Y=$01`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Beetle resumes at `03:6A37` after 672 body master cycles. Neither the natural
scope nor restored replay includes an interrupt-owned write. This path also
proves that the saved-status write is stack-relative (`$2100+SP`) rather
than fixed at `$21F5`.

Ordinal 10 is the first headerless continuation. Selector `$2A` and table
entry `$03` still resolve `$6C2A->$6D56`, but slot `$26` enters with record
cursor 2. The current stream byte is `$08`, so no event/orientation header
is applied and the cursor advances only once:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$2A` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$10` saved status |
| 135 | `03:697F` | `$38A6` | `$81` alternate-pass state |
| 177 | `03:6995` | `$2035` | `$00` orientation scratch |
| 306 | `03:69BB` | `$202D` | `$56` motion-pointer low |
| 333 | `03:69C0` | `$202E` | `$6D` motion-pointer high |
| 489 | `03:6A1E` | `$3326` | `$7D` vertical low |
| 534 | `03:6A33` | `$3EA6` | `$03` cursor |

Input `A=$2A`, `X=$26`, `Y=$00`, `P=$02`, and `SP=$F9` becomes
`A=$08`, `X=$26`, `Y=$02`, `P=$10`, and `SP=$F9`; every MPR is preserved.
Beetle resumes at `03:6A37` after 540 body master cycles. Neither the natural
scope nor restored replay includes an interrupt-owned write.

The independently replayed eighth call completes the frame-941 quartet.
Input `A=$2D` selects table pointer `$6C35`, X selects fixed slot `$29`, and
the slot's collision pass state is `$81`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$35` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A9` | `$80` alternate-pass state |

Input `A=$2D`, `X=$29`, `Y=$01`, `P=$10`, and `SP=$F9` becomes
`A=$80`, `X=$29`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index returns at `03:6988` after 165 body master
cycles. No interrupt-owned write appears in either the natural scope or its
restored replay.

The independently replayed seventh call is the next frame-941 return. Input
`A=$2C` selects table pointer `$6C30`, X selects fixed slot `$28`, and the
slot's collision pass state is `$81`. Its exact transaction is:

| Body cycle | Store PC | Logical address | Value |
| ---: | --- | --- | --- |
| 39 | `03:696B` | `$202F` | `$30` table-pointer low |
| 66 | `03:6970` | `$2030` | `$6C` table-pointer high |
| 114 | `03:697C` | `$21F9` | `$12` saved status |
| 135 | `03:697F` | `$38A8` | `$80` alternate-pass state |

Input `A=$2C`, `X=$28`, `Y=$00`, `P=$02`, and `SP=$F9` becomes
`A=$80`, `X=$28`, `Y=$81`, `P=$12`, and `SP=$F9`; every MPR is preserved.
The zero alternate-pass index returns at `03:6988` after 165 body master
cycles. No interrupt-owned write appears in either the natural scope or its
restored replay.

`dc_rom_collision_table_record`,
`dc_rom_collision_table_record_reverse`, and
the two vertical-record regressions cover ordinals 1 through 4, while
the four adjacent collision-table-pass regressions cover ordinals 5 through
8, and the alternate-reverse and continuation regressions cover ordinals 9
and 10. The second continuation regression covers ordinal 12, and the
alternate-selector and repeated-pass regressions cover ordinals 14 and 15.
The second through fourth repeated-pass regressions cover ordinals 16
through 18, and the alternate continuation regression covers ordinal 19.
The horizontal-continuation regression covers ordinal 20. All restore the
same pre-call states and execute original assembly for each shadow
comparison. The reverse-horizontal continuation regression covers ordinal
21, and the two later vertical-continuation regressions cover ordinals 22
and 23 with the same complete artifact hashes.
Debug and Release
preserve the established gameplay SHA-256
`1f6d46ac99798bbac1ecab97702d9e10076b9766796cc64d22d9662efce0b353`,
the complete 19,648-call trace SHA-256
`52662c6a1367b40d062cef954c52aec7e01a92ecd9fbcf4709e8dc1d2cf28a11`,
and its 180,389-write SHA-256
`e1314a534e10b0df2e6e3c101ce420b4bcd62e6cc8522c4e5c311556f7aa9a1c`.
The remaining `6966` branches stay in assembly. The next interpreter slice
models positive-state repeated passes and the reverse-oriented `SMB0 $35`
record class, validated through the aggregate route rather than new ordinal
promotion.
The independently replayed frame-945
call is already covered by the state-based analysis: selector `$06` resolves
to `$6B0A`, slot `$17` changes pass state `$81->$80`, and the routine
returns at `03:6988` after 165 cycles and four writes. It is not a new semantic
branch. Ordinal 11 remains
assembly because its natural scope enters a mid-routine interrupt at
`03:69A2` / `00:FC26`, producing 1,233 cycles and 22 writes. Ordinal 13 is
also
interrupt-bearing: its 1,299-cycle/24-write replay shifts interrupt-owned
stores beginning near `03:69B7`.

### Seventh native replacement: semantic `03:7F57@all`

The Small-Soldier collision consumer is a caller-level composition rather
than another leaf routine. Its common no-hit branch calls `03:6966`, calls
`03:7FB1`, loads `$3800,X`, and returns when the collision high bit is clear.
The native adapter therefore reuses the two existing semantic replacements.
It accepts any transaction admitted by `03:6966`, stages its ordered writes
into a private work-RAM snapshot, and derives either `03:7FB1` response
branch from that resulting state before deriving the final accumulator and
N/Z flags from the collision byte.

The composed body emits the outer JSR stack pair at cycle 15 and begins the
collision-table child at cycle 36. The second JSR, response-child offset,
write count, and total body cycles are derived from the selected child
transactions. The original alternate-pass/non-reload shape remains the
11-write, 417-cycle case. Its 17 child checkpoints are composed with the
response child's 11/14 checkpoints plus both parent JSR/RTS boundaries and
the final collision test, yielding 35/38-boundary parent transcripts. The
50-checkpoint one-axis child yields 68/71-boundary parent transcripts, and
the 55-checkpoint bounded two-axis child yields 73/76. This construction
keeps the inner routines as the single source of their state, flag, write,
and timing semantics; there is no per-ordinal or per-actor allowlist.

Across the 3,600-frame collision-boundary census, the semantic composition
admits 20,113 of 20,867 calls (96.4%). The Debug and Release
`dc_rom_collision_consumer_semantic` regression preserves the
configuration-specific state-census hash and the complete routine artifacts:

- Call SHA-256:
  `3ab64128aab89d056216f79d0d3f041e186eb109048aefe82785e2875f41ec4f`
- Write SHA-256:
  `3b2eb1b5dbd54c1b08cf6a509820e47c2320faf544b4d15824f5426f414c6544`

### Eighth native replacement: semantic `01:D466@all`

The per-piece flipper input helper begins with one state-defined gate:
`LDA $2CEE; AND $3B80,X`. A zero intersection returns immediately at
`01:D46E`; a nonzero result marks the piece active and enters the longer
response path. `dc_evaluate_flipper_input_gate` models that indexed
intersection independently of actor slot or dynamic call number.

The zero-result transaction returns `A=$00`, derives N/Z while preserving
the other status bits, performs no writes, and resumes at `01:D46E` after
36 master cycles.

`dc_prepare_flipper_activation` also models the common nonzero-input shape
when X is not the current `$BD` slot and `$3D80,X` is zero. It writes active
state 1 to `$3E00,X/$3A00,X`, reproduces the `D48B` nested-JSR stack pair,
uses `C5C9`'s popped return bytes in `$26/$27`, increments `$26`, and
installs coroutine dispatch `01:D48E` in `$3F00,X/$3F80,X`. The complete
nine-write transaction returns `A=$D4` at `01:C5E9` after 282 cycles with
carry derived from `CPX $BD`. Current-slot and later held/release variants
remain in assembly.

The zero-result path supplies three instruction-boundary checkpoints. The
nested activation/scheduler path supplies 21, including `D48B`'s JSR stack
transition and every instruction through the `C5C9` dispatch installation.
The 3,600-frame controls/physics route contains 7,470 calls; 7,468 (99.97%)
now enter the replacement. The remaining two are current-slot calls that
enter `D525` at frames 1,820 and 2,255 and emit 823/868 ordered writes,
beyond the bridge's independent 512-write limit.
`dc_rom_flipper_input_gate_semantic` preserves the configuration-specific
census and the complete routine artifacts in Debug and Release:

- Call SHA-256:
  `5230b181b1cb23394d0a540346418e98244f25e0ea2a50fce136b0b007fe2947`
- Write SHA-256:
  `b5cec75fa9f5d055fd5ed869df3ef6dabe578d9b7cd9b8481b6d701a81a209b8`

### Ninth native replacement: semantic `01:C3CB@all`

The session/nudge gate first tests session flags `$53`, tilt lock `$2CC7`,
and the `$28` session-state mask. Once ordinary table input is eligible, it
decrements timer `$2CC6`, checks the nudge phase at `$2CC4`, resolves the
active player through `$2251`, and tests Button II at `$2018,Y`.
`dc_evaluate_nudge_input_gate` classifies the five bounded paths
from those live fields.

The session-state shape returns `A=$08` at `01:C3DC` after 69 cycles and no
writes. The ordinary idle shape writes the decremented timer at instruction
`01:C3DD`, cycle 96, then returns `A=$00` at `01:C3FA` after 177 cycles.
The timer-reload shape decrements `$2CC6` to zero at cycle 96, restores it to
`$78` at cycle 123, clears `$2CC5` at cycle 138, and returns after 207
cycles. All three derive their final N/Z flags while preserving unrelated
status bits.

The session return supplies eight instruction-boundary checkpoints, the idle
return supplies 17, and timer reload supplies 20. The three active Button-II
starts supply 66 checkpoints and finish after 762 cycles; their three
phase-1 continuations supply 44 checkpoints and finish after 522 cycles.
Those paths initialize or consume `$2CC2/$2CC3`, apply the halved signed
vector to `$3500/$3580`, and update `$2CC4`. Their writes to logical
`$0000/$0002/$0003` are constrained to physical bank `$FF`, sent through
Beetle's normal base-VDC bus path, and charged the original VDC wait cycles.
All 2,649 calls on the 3,600-frame controls/physics route now enter the
replacement, including every original interrupt-bearing scope.
`dc_rom_nudge_input_gate_semantic` preserves the configuration-specific
census and the complete routine artifacts in Debug and Release:

- Call SHA-256:
  `eb17f2e9bc2e4fac07823be555379dfb75c21e5c0f4ef4161084ed7588198905`
- Write SHA-256:
  `9e7bf6564ef1c61afac2ad63cdcbf2f8742318711b051744ffab24f552c6c40d`

### Tenth native replacement: semantic `01:C49E@all`

The pause/high-score session gate follows `C3CB` in the main session
controller. Its ordinary path first requires `($53 & $02) == 0`, loads the
active-player index from `$2251`, then tests the Run edge with
`$2018,Y & $08`. A zero result returns immediately at `01:C4C8`.
`dc_evaluate_session_input_gate` derives that decision from live state.

The native adapter admits only this ordinary no-Run-input result. It returns
`A=$00`, preserves X and SP, returns the selected player in Y, derives N/Z
while preserving unrelated status bits, performs no writes, and completes
after 72 master cycles. Run-input pause/high-score work, the alternate
session return, and the longer initialization path remain in assembly.

The seven original instructions are represented by seven interrupt
checkpoints. The 3,600-frame route contains 2,649 calls, all of which now
enter the replacement rather than the earlier 1,716 (64.8%).
`dc_rom_session_input_gate_semantic` preserves the configuration-specific
census and the complete routine artifacts in Debug and Release:

- Call SHA-256:
  `1e5052965bd3252e116ee6cd4b72ea5213cab371e9e0c6faad083af9915e381e`
- Write SHA-256:
  `09e04a8271fd92ad23977712ed8511a70fccac3457b3101cbda6671d5a0ec402`

### Eleventh native replacement: semantic `01:D500@all`

The shared flipper phase helper decrements `$3A00,X`. When that countdown
expires, it reloads from `$3E00,X` and uses orientation bit 7 at `$3C80,X`
to move phase `$3980,X` toward the active or resting endpoint.
`dc_advance_flipper_phase` derives this transition for any indexed slot.

The supported forward branch writes countdown zero at cycle 21, reloads it
at cycle 57, increments phase at `01:D517` on cycle 126, and returns at
`01:D524` after 138 cycles. The reverse branch shares the first two writes,
decrements phase at `01:D521` on cycle 126, and returns on that same cycle.
Both preserve the loaded old phase in A and derive the exact final N/Z and
direction-specific carry state. Non-expiring countdowns and phase endpoints
remain in assembly.

The reverse path supplies nine instruction-boundary checkpoints and the
forward path eleven. The 3,600-frame route contains 980 calls, all of which
now enter the replacement rather than the earlier 600 (61.2%). Endpoint and
non-expiring states remain assembly-owned outside this route.
`dc_rom_flipper_phase_step_semantic` preserves the configuration-specific
census and the complete routine artifacts in Debug and Release:

- Call SHA-256:
  `8d3708e4fe36ddd9745d77af68c3a6d575acef3ce29e23c70e72dc30eca9056f`
- Write SHA-256:
  `29274f5c8a62a9fc9d41f7796f343ac55167fcafdf71448dc6e9f76bcfd31965`

### Twelfth native replacement: semantic `01:C5C9@all`

The shared actor-coroutine scheduler removes its own JSR return from the
stack and stores that return in scratch `$26/$27`. If indexed delay
`$3D80,X` is zero, or becomes zero after one decrement, it increments the
stacked return into the following instruction and installs that address in
`$3F00,X/$3F80,X`. Otherwise it yields with the decremented delay. This one
state rule is shared by flippers, collision consumers, and many table-actor
controllers; it is not specialized by caller or call ordinal.

`dc_schedule_coroutine` derives all three branches plus low-byte wrap into
the dispatch high byte. The native adapter advances SP by two for the two
`PLA`s and resumes at the original `01:C5E9` RTS so the outer return remains
emulated. The decrement-and-yield branch takes 102 master cycles and three
writes. Immediate installation takes 159 or 171 cycles, while
decrement-to-install takes 180 or 192 cycles depending on return-page carry.
All scratch, delay, dispatch, accumulator, N/Z/T, and write-timing effects
are reproduced exactly.

The delayed-yield path supplies eight instruction-boundary checkpoints.
Immediate and decrement-to-install paths supply 12-15 depending on countdown
and return-page carry. That lets Beetle execute original interrupt handlers
at the same instruction boundaries and then resume the pending native
transcript.

The 3,600-frame route contains 2,462 calls, all now replaced rather than the
earlier 1,537 (62.4%). `dc_rom_coroutine_scheduler_semantic` preserves the
configuration-specific census and the complete 2,462-call/863,105-write
artifacts in Debug and Release:

- Call SHA-256:
  `924221bbff7b244e91850fa206ad8ce70c2e86d3cf894824dd5e6b7b09da3bd9`
- Write SHA-256:
  `1ef7a9e9aa5007f01a0138660f948ab77d5bc67292d9e5a076a8284d1eebc1aa`

### Thirteenth native replacement: semantic `01:D081@all`

The flipper/ball interaction calls `D081` to turn live state `$78` into two
16-bit geometry vectors. The routine masks to five phase bits, doubles that
index, and reads low/high pairs from user-ROM tables `01:D2AE` and
`01:D2EE`. It writes the first pair to `$7F/$80`, the second to `$7D/$7E`,
and preserves the caller's Y register through `PHY`/`PLY`.
`dc_lookup_flipper_geometry` receives the 128-byte table explicitly from
the verified ROM boundary, so no ROM-derived table data is compiled into
the native module.

The native transaction reproduces the Y stack write at cycle 27 and the
four vector writes at cycles 66, 93, 120, and 147. It returns at `01:D09D`
after 159 master cycles with A equal to the second vector's high byte,
carry cleared by the doubled bounded index, and N/Z derived from restored Y.

The replacement declares 14 instruction-boundary checkpoints. Each carries
the original last-cycle interrupt-sampling offset, completion offset,
instruction identity, resume PC, registers, and MPRs. Beetle advances only
to the next checkpoint, samples IRQs where the original instruction did,
and runs an asserted interrupt through the original ROM handler. After a
state-matching `RTI`, the pending native transcript resumes. Pending writes
and checkpoints are included in savestates; a handler that does not restore
the expected registers or MPRs causes safe continuation in assembly at the
current original instruction.

The 3,600-frame route contains 540 calls, of which 38 include periodic
interrupt-handler passages and therefore retain longer body/write
transcripts. All 540 calls now enter the semantic replacement; the prior
conservative horizon admitted 305. `dc_rom_flipper_geometry_semantic`
preserves the configuration-specific census and complete
540-call/3,214-write artifacts in Debug and Release:

- Call SHA-256:
  `32d52a16cff5b2f20d1b3e8174e75a4ca097648253ce0726970fb92707a96d08`
- Write SHA-256:
  `b13407d580d6428cf3f7fc0460040953d4cb7136faa9305c2339ed99a8336934`

### Fourteenth native replacement: semantic `00:F4FC@all`

The shared actor camera-window helper is a state-generic leaf rather than a
slot-specific mechanism handler. `dc_classify_actor_camera_activation`
reads occupancy `$3000,X`, signed integer Y `$3380:$3300,X`, camera anchor
`$AA:$A9`, and flags `$3080,X`. Empty slots return unchanged; occupied slots
set bit 7 exactly for the half-open interval
`[$AA:$A9 - 48, $AA:$A9 + 240)`.

The adapter reproduces the original 16-bit `SBC`/`ADC` flag behavior, lower
and upper scratch writes to `$35/$36`, and the final indexed flag store.
Path-sensitive 2/18/31-checkpoint transcripts allow Beetle to execute and
return from original interrupt handlers between the same instructions.
Across the state census all 65,927 calls are native, including 4,858
interrupt-bearing scopes, while the complete 283,956-write artifact remains
unchanged in Debug and Release.

### Fifteenth native replacement: semantic `01:C574@all`

The generic slot reset begins by saving Y, swaps X's actor slot into the
indirect index, and initializes `$2D:$2E` to `$3000`. Its fixed 32-iteration
loop stores zero through `($2D),Y`, advances the pointer by `$80`, and finally
restores X and Y. `dc_prepare_actor_slot_reset` exposes the 32 legal
work-RAM destinations without embedding any actor-specific behavior.

The host generates the loop's 361 instruction checkpoints and all 99 ordered
writes from that one state model. The fixed checkpoint capacity is bounded
at 2,048 so the complete loop remains savestate-serializable. All 132 route
calls are native, including 35 interrupt-bearing scopes, while the state
census, call hash, and 14,163-write hash remain exact in Debug and Release.

### Sixteenth native replacement: semantic `00:E837@all`

The shared math leaf at `00:E837` performs a 16-bit two's-complement
negation of scratch word `$2F:$30`. It XORs both bytes with `$FF`, increments
the low byte, and increments the high byte only when the low result wraps to
zero. Callers `01:D0F2` and `01:D1D2` connect it to the flipper projection
and signed rotation paths.

`dc_negate_signed_word` derives the result and high-byte carry from the live
scratch word without mutating the copied RAM view. The ordinary path writes
complemented `$2F`, complemented `$30`, and incremented `$2F` at body cycles
30, 60, and 78, then returns after 90 master cycles. The wrap path adds the
high-byte write at cycle 102. Both preserve the complemented high byte in A
and reproduce the final N/Z/T state from the last increment.

The ordinary and wrap paths supply eight or nine exact instruction
checkpoints. Across the 3,600-frame route, all 894 calls execute natively;
24 retain original interrupt-handler passages. Restored ordinal 1 at frame
1,064 independently shadows the 102-cycle, four-write wrap path against
original assembly before the aggregate replacement result is accepted. The
complete artifacts are identical in Debug and Release:

- Call SHA-256:
  `5871cb89939cddaaa4d7b3f9ed74dcb78b04cf80ccc9264c672d750c412ac849`
- Write SHA-256:
  `2904482e2d30a8800ad5a13e26e11b3b0f12adc0d9ac6bca0ecc729b6252d6bd`

The adjacent `01:D09E` vector projection was initially deferred because its
540 calls range from 5,328 to 33,471 master cycles and contain 168,532
scoped writes across the variable multiply pipeline and periodic handlers.
It is now the composed nineteenth replacement below.

### Seventeenth native replacement: semantic `00:E624@all`

`E624` is the fixed eight-bit unsigned round used by the shared signed
fixed-point multiply pipeline. Four outer iterations each perform two
rounds: shift the 24-bit `$42:$43/$30` state, test the outgoing multiplier
bit, conditionally add multiplicand `$2D:$2E`, and propagate addition carry
by incrementing `$30`.

`dc_multiply_unsigned_round` exposes all eight post-shift and post-add
states. The native adapter uses those states to generate the exact original
instruction stream. A zero outgoing bit contributes the three shift writes;
a one bit adds the two accumulator stores and optional multiplier carry
write. Every admitted body therefore takes exactly
`600 + 84 * admitted-bit-count` master cycles and emits 24-48 writes.

Generated transcripts contain 41-113 instruction checkpoints. Across the
3,600-frame route, caller `00:E61B` contributes 3,700 calls and `00:E614`
contributes 1,671. All 5,371 execute natively, including 1,592 calls with
original interrupt-handler passages. Restored ordinal 1 independently
shadows its 1,689-cycle, 44-write interrupted path; ordinal 2 shadows the
adjacent 684-cycle, 26-write interrupt-free path. Debug and Release preserve
the complete artifacts:

- Call SHA-256:
  `d704b3698ce86f82d504ad4b7693b40f425a035d056483fd473bebba672b022c`
- Write SHA-256:
  `a073ff2ee296a0bd34a76bee12fcf8bcd15f5c4b2407c7698feabd16e08653d4`

### Eighteenth native replacement: semantic `00:E65C@all`

The enclosing signed 8.8 wrapper is state-bounded despite its observed
921-27,306-cycle scopes. The long tail comes from original periodic handlers,
not an unbounded software loop. `dc_multiply_signed_fixed` normalizes both
signed operands, derives the combined sign, runs the `E60B` high-byte round
only when the normalized multiplier requires it, always runs the low-byte
round, and restores the final sign.

The host composes each admitted wrapper from exact sign-normalization
instructions, nested JSR/RTS and PHX/PLX stack transactions, one or two
semantic `E624` children over a private staged RAM snapshot, and the final
sign restoration. The resulting path-sensitive transcript remains below the
2,048-checkpoint savestate bound and lets Beetle deliver the original
interrupts at their instruction boundaries.

All 3,700 route calls execute natively, including 2,157 calls with original
handler writes. Interrupt-free ordinal 3 independently shadows its
921-cycle, 35-write zero-high-round path against restored assembly. Debug
and Release preserve the complete artifacts:

- Call SHA-256:
  `bd816aa5d64221cdb21835e6d27fe81b640f45c32e549490bedf5535a9adc695`
- Write SHA-256:
  `c21901978bb16c29d33af18899cdb2494aee21b76da7560a43f6405841ad0ce4`

### Nineteenth native replacement: semantic `01:D09E@all`

The downstream flipper projection is a bounded composition once `E65C` is
native. It loads the indexed motion pair from `$3400:$3480,X` and
`$3500:$3580,X`, combines it with coefficients `$7D:$7E` and `$7F:$80`,
and computes:

- `$C0:$C1 = first*c + second*s`
- `$BE:$BF = -first*s + second*c`

`dc_project_flipper_vectors` supplies the pure signed 8.8 result model. The
host recreates the exact setup, sum, and scratch stores around four semantic
`E65C` children and one semantic `E837` child over a staged RAM snapshot.
The replacement checkpoint bound increases from 512 to 2,048 while the
independent write bound remains 512; this keeps the composed transcript
fixed, savestate-serializable, and large enough for every live path.

All 540 route calls execute natively, including 448 calls with original
handler writes. Interrupt-free ordinal 16 independently shadows its
5,328-cycle, 194-write path against restored assembly. Debug and Release
preserve the complete artifacts:

- Call SHA-256:
  `b2f33cee152dc71d55b6da09a446510faeecdca93010f4284ed24ab46a0e1cae`
- Write SHA-256:
  `51fc462de6afc7355c005bbde18879a4fa0fa6390554ddf097ff3f06c9b44826`

### Twentieth native replacement: semantic `01:D1A2@all`

The signed two-dimensional rotation is likewise a bounded composition after
`E65C` becomes native. With `u=$C1:$C0`, `v=$BF:$BE`,
`c=$7E:$7D`, and `s=$80:$7F`, it computes:

- `$3480:$3400,X = u*c - v*s`
- `$3580:$3500,X = u*s + v*c`

`dc_rotate_motion` supplies the pure signed 8.8 result model. The host
recreates all parent operand setup, scratch and indexed output stores, and
16-bit additions around four semantic `E65C` children and one semantic
`E837` negation over a staged RAM snapshot. Each nested JSR/RTS boundary is
checkpointed, so original timer handlers remain Beetle-owned.

All 354 current route calls execute natively across observed
5,412-15,270-cycle and 198-441-write paths. Of those, 296 contain original
handler writes. Interrupt-free ordinal 23 independently shadows its
frame-1,241, 7,584-cycle, 273-write path against restored assembly. Debug
and Release preserve the complete 354-call/103,362-write artifacts:

- Call SHA-256:
  `dac3fcbd9d547af534494dbb2d0e85508209afce444689831d0fd9525d88578c`
- Write SHA-256:
  `e0465a8fee8556dc82827845a87a1dc575a7d70f9d9aa81e9c9d75dca6cdf523`

### Twenty-first native replacement: semantic `01:D612@all`

This flipper-controller leaf scales the signed 8.8 radius at `$31:$32` by
the signed 8.8 coefficient at `$7D:$7E`, storing the projected component in
`$C2:$C3`. `dc_project_flipper_radius` supplies the pure result model.

The host recreates the eight parent operand loads/stores, the exact JSR/RTS
stack transaction around one semantic `E65C` child, and the four final
result instructions. The complete composition stays within the fixed
512-write and 2,048-checkpoint bounds.

All 62 route calls execute natively across observed 2,079-25,755-cycle and
77-487-write paths. Of those, 51 contain original handler writes.
Interrupt-free ordinal 36 independently shadows its frame-2,498,
2,079-cycle, 77-write path against restored assembly. Debug and Release
preserve the complete artifacts:

- Call SHA-256:
  `20c409aa3cda48b3dbc0bf8c5e1e0a82319f7aa8efdeb2c70374ae884811bb52`
- Write SHA-256:
  `73c5f1848187c576bbb48595f7d3fe95b03c70cee7c67cd18994b71c9038308f`

### Twenty-second native replacement: semantic `01:D62E@all`

This paired flipper-controller leaf chooses one of two directional angle
deltas from `$3180,X/Y` according to `$3C80,Y & $01`. A negative directional
delta takes the three-clear zero exit. Otherwise, five `LSR A` / `ROR $2F`
pairs convert the byte delta to 8.8, `$9E:$9F` scales it through `E65C`, and
the sign of `$3C80,Y` chooses direct storage or two's-complement negation
into `$31:$32`.

`dc_project_flipper_angle` supplies the pure branch/result model. The host
recreates both delta branches, every shift/rotate scratch write, one complete
semantic `E65C` child, and both output-sign paths with exact checkpoints.

All 62 route calls execute natively across observed 1,407-4,191-cycle and
51-114-write paths. Of those, 43 contain original handler writes.
Interrupt-free ordinal 1 independently shadows its frame-1,268,
1,407-cycle, 51-write path against restored assembly. Debug and Release
preserve the complete artifacts:

- Call SHA-256:
  `fd133773c9c76ba17d3143052aaf88efc3a4cbbfe887b72dfd076f5f74e9eafb`
- Write SHA-256:
  `23a2558bb00f772c4509ec470d8f8b5b642d95b2ac4c343c7f64ae1c870735eb`

### Twenty-third native replacement: semantic `01:CFD8@all`

This `D525` helper adjusts signed motion `$BF:$BE` according to `$78 & $60`.
Mode zero applies the original positive or negative clamp, with `$2CC7`
redirecting into scaled behavior. Mode `$20` selects signed 3/16 scaling or
the `$222C` quarter-scale variant; modes `$40` and `$60` select one-half and
one-sixteenth scaling.

`dc_adjust_collision_motion` supplies the complete result model, including
the two branches not exercised by the route. The host recreates every
compare, clamp, arithmetic rotate, scratch store, and addition checkpoint.
All 354 route calls execute natively, including 41 with original handler
writes. Interrupt-free ordinal 29 shadows its frame-1,297, 102-cycle,
two-write half-scale path. Debug and Release preserve:

- Call SHA-256:
  `a3ec557a3eddcd91aa5b58918fb52ab8d038e36a7e2ea13bda6d602149bb842f`
- Write SHA-256:
  `0f9ec94cd10cc59a8f0cbc9d70bfdd06c8ff2a3bea9623f76c6e662dc00b30ec`

### Shared unsigned 24-bit divider: semantic `00:E7D7@all`

This arithmetic helper reads the unsigned 24-bit dividend at `$2E:$2F:$30`
and the byte divisor at `$2D`. It saves X, clears remainder `$42`, rolls the
dividend once through the inherited carry, and then executes exactly 24
restoring division rounds. Each round shifts the remainder, conditionally
subtracts the divisor, and rolls the resulting quotient bit through the three
dividend bytes. The quotient replaces `$2E:$30`; A and `$42` hold the remainder.

`dc_divide_unsigned_24` exposes every round's shifted remainder, subtraction
decision, post-subtraction remainder, and rolled quotient. The host recreates
the PHX/PLX stack traffic, all 101-125 routine-owned writes, binary compare and
subtract flags, branch timing, and every interrupt checkpoint before absorbing
the live stack-derived `RTS`. Division by zero intentionally retains the ROM's
24 subtract/store decisions and all-one quotient instead of being rejected.

All 105 route calls execute natively. Twenty-three are uninterrupted and 82
contain original handler passages; the complete scopes span 3,123-48,450
master cycles and preserve 13,975 aggregate writes. Interrupt-free ordinal 1
directly replays its frame-874, 3,123-cycle, 101-write call. Release preserves:

- Census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Call SHA-256:
  `9163eaedd115dda2148b6c2c3e6d045dcfaaa1ff674629946d38fbba3d2207f0`
- Write SHA-256:
  `cb32774ea1c72400d67db014ae3337605d7a63b4cbc44fb532021a5a9b4d57d0`

### Shared unsigned byte divider: semantic `00:E6DC@all`

The byte-width sibling reads dividend `$30` and divisor `$2F`, saves X, clears
remainder `$42`, and rolls the dividend once through the inherited carry. Its
eight restoring rounds shift `$42`, conditionally subtract `$2F`, and roll the
quotient bit into `$30`. The quotient replaces `$30`; A and `$2F` receive the
remainder before X and the live stack-derived return are restored.

`dc_divide_unsigned_byte` exposes every shifted remainder, subtraction
decision, post-subtraction remainder, and rolled quotient. The checkpointed
adapter reproduces all zero-page and stack writes, compare/subtract flags,
branch timing, PHX/PLX traffic, and division-by-zero ROM behavior.

All 46 route calls execute natively. Thirty-three are uninterrupted and 13
contain original handler passages; complete scopes span 795-1,899 master
cycles and preserve 1,108 aggregate writes. Ordinal 3 directly replays the
last frame-874 call at 795 cycles and 20 writes. Release preserves:

- Census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Call SHA-256:
  `d0b4d0891016f40c8d1768b703294374b9436e7413734c3a05170d51b848d31e`
- Write SHA-256:
  `94a4d92c6600323147e3c2e2b65760fa83d2ff891fcaa167384eae3f424b1ced`

### Initial Small-Soldier collision tail: semantic `03:7F1A@all`

The Small-Soldier's initial controller resumes at `03:7F1A` after its
scheduler step. It loads collision selector `$3700,X`, calls the shared
`03:6966` collision-table interpreter, rejects the transition branch at
`03:7F20`, calls response helper `03:7FB1`, and returns at `03:7F45` while
collision byte `$3800,X` remains nonnegative. The ten transition calls branch
into actor-state setup and remain assembly-owned.

The native path parameterizes the established steady-state `03:7F57`
composition rather than duplicating its child semantics. It stages both JSR
frames, composes the collision interpreter and response helper against their
ordered work-RAM effects, adds the initial state's six-cycle carry branch, and
absorbs the distinct live-stack return. Every child and parent instruction
checkpoint remains available for original timer interrupts.

All 1,120 no-hit calls execute natively out of 1,130 entries. Their scopes span
423-19,131 master cycles, preserve all 21,255 writes in the aggregate trace,
and produce zero horizon declines or abandonments. Interrupt-free ordinal 4
directly replays its frame-958, 423-cycle, 11-write call. Release preserves:

- Census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- Call SHA-256:
  `dd9c01d2a5145c96bebdf856212c6acdcd11d1af64c65c0d7fd5e6c802bfb693`
- Write SHA-256:
  `5111f1e67085f9bb4b2d4b39efd5a2bd2fde51c23892b751cd797609beb5f05a`

### Twenty-fourth native replacement: semantic `00:E905@all`

This shared signed divider reads the 16-bit dividend at `$2F:$30` and the
signed byte divisor at `$2D`. It preserves the carry inherited by the
initial dividend rotate, normalizes both operands, executes the fixed
16-round `E8E7` unsigned remainder/quotient core, and restores the quotient
sign through the original two-byte subtraction.

`dc_divide_signed_word` exposes the carry-sensitive pure result and every
round's remainder, subtraction decision, and rolled quotient. The host
recreates all wrapper and child instructions, JSR/RTS and PHX/PLX stack
traffic, scratch writes, flags, branches, and interrupt checkpoints.

All 752 gameplay calls execute natively, including 583 calls with original
handler writes. Interrupt-free ordinal 516 shadows its frame-1,564,
2,037-cycle, 59-write path. Debug and Release preserve:

- Call SHA-256:
  `20d7b35e6e17c63980db6b22e4ed425000d5a4576f1bfa5bd9b36fda86bf335d`
- Write SHA-256:
  `59822abdd58d01fb652cd197fa0555fa40c4f21b57dbe51591adb5e0360fb81d`

### Twenty-fifth native replacement: semantic `01:DD09@all`

This motion helper conditionally divides each nonzero indexed signed 8.8
component by `$8C` or `$8D`, subtracts the signed quotient, and then adds the
unsigned low-byte acceleration `$8B` to the second component.

The state-derived host transcript composes zero, one, or two complete
semantic `E905` children with exact operand setup, nested stack boundaries,
indexed stores, subtraction, acceleration, and interrupt checkpoints. The
old ordinal-specific packed write tables are no longer used.

All 753 gameplay calls execute natively, including the 669 calls previously
deferred for interrupt delivery. Focused ordinal 181 retains its
frame-1,228, 2,427-cycle, 69-write negative sign-restoration replay. Debug
and Release preserve:

- Call SHA-256:
  `e285eb631a38539fcca0bed8c7e7d84a6f7dbbaa0613ee8009467b50c0f03327`
- Write SHA-256:
  `ef4285d7a05ee530edb4563e88b4af8e0fb23deaee2188bc40828ff48b8571d8`

### Twenty-sixth native replacement: semantic `01:CC1A@all`

The central collision scanner begins by calling `01:CCD5` to compare the
indexed actor and ball vertical coordinates. Four outcomes reject the
candidate before pointer resolution, collision-mask testing, or contact
response:

- a positive high-byte separation rejects after 150 base master cycles;
- a same-page low-byte separation of at least `$80` rejects after 180;
- a negative high-byte separation rejects after 246;
- a negative low-byte magnitude of at least `$80` rejects after 282.

`dc_classify_collision_scan_vertical_reject` derives these shapes from the
live work-RAM snapshot. The host recreates the internal JSR stack writes,
the exact `$2D/$2E/$2F/$30` scratch writes, binary `SBC`/`CMP` flags, and
the instruction checkpoints through the shared `CLC`/`RTS` at `01:CC3F`.
The positive paths emit three ordered writes and the negative paths emit
six.

The parent now also composes semantic `CCD5` for candidates that fail any
post-geometry bound at `CC26-CC3D`. The composition stages the child's
live-MPR ROM lookups and writes, carries all child instruction checkpoints
through the parent's JSR/RTS boundary, then reproduces `CC1D-CC3F` with
exact horizontal/vertical subtraction, comparisons, `$2D/$2F` writes,
flags, and timing. Focused route ordinal 5 independently replays the
resulting 1,005-cycle, 22-write horizontal path against restored assembly.

The final in-range continuation now derives its optional side flag, sub-cell
row/column, rounded horizontal span, and cell coordinates, then composes the
semantic `E569` multiply and `CC98` mask lookup through the parent. The
cross-bank `E57F` return retains its physical-bank identity for interrupt
sampling and write attribution. Across the 3,600-frame held-flipper route,
all 85,715 `CC1A` calls execute natively. Debug and Release preserve the
complete 85,715-call/1,784,119-write artifacts:

- Call SHA-256:
  `c01321d866af5636ce12db1174a9a3428d08ce273cf62820fcbf446dcab1a313`
- Write SHA-256:
  `a0366616f0bbd85b22d5a4a72e8d03e09a8866525c4c90d50c51e2ea7d035881`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

`dc_rom_collision_scan_vertical_semantic` guards all four early vertical
shapes, the composed post-geometry bound shapes, full aggregate admission,
in-range ordinal 19,176 replay at 2,277 cycles/52 writes, and exact
state/call/write artifacts. ROM-free core tests cover every bounds
classification, mask-coordinate preparation, and truncated snapshots.

### Twenty-seventh native replacement: semantic `01:CCD5@all`

The near-target continuation of `CCD5` is a bounded geometry leaf. It swaps
the target slot into X, selects four directory bytes through zero-page
pointers `$96-$9D`, resolves two phase-indexed geometry pointers, and reads
the first four-byte descriptor. The host follows the live MPR map, including
the supported 384-KiB HuCard mirror, while
`dc_prepare_collision_geometry` supplies the pure arithmetic model.

Positive and negative initial vertical differences combine with positive
and negative descriptor offsets into four uninterrupted bodies:

| Initial difference | Descriptor offset | Cycles | Writes |
| --- | --- | ---: | ---: |
| Positive | Negative | 783 | 16 |
| Positive | Positive | 789 | 16 |
| Negative | Negative | 885 | 19 |
| Negative | Positive | 891 | 19 |

The path-sensitive instruction transcript covers both initial-difference
prefixes and both descriptor-offset branches. All 66,523 near-target calls
now execute natively, including the 60,704 previously deferred
interrupt-bearing scopes, while Beetle delivers and returns from original
handlers at the declared instruction boundaries. The standalone
`dc_rom_collision_geometry_semantic` regression observes all 85,715 leaf
entries and admits the 66,523 near-target calls, preserving these complete
artifacts in Debug and Release:

- Call SHA-256:
  `394b345b5ba30aadd0d4fdfd27e88a4fe812d749b5560b4f0ccea10a88c2f578`
- Write SHA-256:
  `ba33300ad75be145eff8a7d6de4f98356fc4ca338aa111cfe27cb77057d019dc`

ROM-free tests cover both signed descriptor paths, the late position-page
rejection model, and truncated snapshots.

### Twenty-eighth native replacement: semantic `00:E569@all`

Every one of the 874 in-range `CC1A` mask paths calls `00:E569` once. The
leaf treats `$2F` as an unsigned multiplicand and `$30` as an unsigned
multiplier, clears A, and performs eight left-shift/add rounds. The low
product returns in A and `$2F`; `$30` holds the high product byte. X is
preserved through the original stack transaction.

`dc_multiply_unsigned_bytes` records each admitted multiplier bit and
high-byte carry. The host turns that state into a 45-69-boundary transcript
with exact `471 + 24 * admitted-bit-count + 12 * carry-count` base timing.
All 874 route calls execute natively, including calls containing original
interrupt-handler passages, while the complete artifacts remain exact:

- Call SHA-256:
  `8f1438081244282123e463ae80ed2570e3d6573830efdea00d0ac0eecadedfd4`
- Write SHA-256:
  `300a548b00ea468d70108085d950ca867282a3903075b7fb789d08c6da50156e`

Ordinal 2 independently shadows its uninterrupted frame-1,072 path against
restored assembly with exact 519-cycle and 10-write parity. The collision
parent composes this leaf before its ROM-pointer and final mask test.

### Twenty-ninth native replacement: semantic `01:CC98@all`

The final collision-mask tail adds scratch bytes `$2D+$2F`, uses the wrapped
sum to read the first pointer table at `$74`, and uses the sum plus four to
read the second table at `$72`. The second byte is multiplied by eight and
added to mask base `$8E:$8F`. Row `$5F` selects the mask byte; column `$5E`
selects the ROM bit table at `01:CF2B`. A nonzero intersection returns at
`CCD2` with carry set, while a zero intersection returns at `CCD4` with carry
clear.

`dc_resolve_collision_mask_lookup` exposes the pure index, pointer, mask, and
contact state. The host resolves all pointer-derived bytes through the live
MPR map and turns that state into an exact instruction transcript. Across the
3,600-frame route, all 874 calls execute natively: 270 take the contact return
and 604 take the no-contact return. Their uninterrupted bodies are 378 and
384 master cycles respectively, with nine direct writes on either path.
Original handlers can still preempt at every declared instruction boundary.

The registered Debug/Release regression preserves:

- Call SHA-256:
  `0e5caea1ddc513ddb39089b2653be432029ee03589822467a56ec63d011f46e0`
- Write SHA-256:
  `e40d0c2787fc68ae883799dee3d688ae9e60f2521693e6bb105edb988e1a3fc6`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 2 independently shadows its uninterrupted contact return at frame
1,072 with exact 378-cycle and nine-write parity. ROM-free tests cover index
wrap, pointer derivation, contact/no-contact masking, and truncated snapshots.
The enclosing 874 `CC1A` calls now compose this tail and the native `E569`
child through the parent transaction; focused parent ordinal 19,176 matches
restored assembly at 2,277 cycles and 52 ordered writes.

### Thirtieth native replacement: semantic `02:4DD9@all`

The Giant Skeleton's Eyeball steady routine is a compact normal-table rule
gate. It loads Dragon's-Egg completion latch `$2D20`, masks bit 0, and enters
the opening state when set. Otherwise it loads session flags `$53`, masks bit
1, jumps to the final animation when set, or returns dormant at `02:4DE9`.

`dc_classify_giant_skeleton_eyeball_gate` exposes all three state-derived
outcomes. The native adapter admits the current dormant path and reproduces
both loads, masks, branches, accumulator, flags, and interrupt sampling
through a six-checkpoint, 57-master-cycle, zero-direct-write transcript.
Across the 3,600-frame route, all 1,988 steady calls execute natively,
including 37 calls containing original interrupt handlers. Those handlers
account for all 493 aggregate writes.

The registered Debug/Release regression preserves:

- Call SHA-256:
  `3d4b6d3bb0feca214446a05837ca88e0eaf2751eb49e2e2bfd5828b4a45f4dff`
- Write SHA-256:
  `88c98631164d2f0c64fd2a1a255601c2806cb2fa050bafb4ed28b211f14be4ef`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 dormant path against restored
assembly with exact 57-cycle and zero-write parity. ROM-free tests cover
the dormant, activation, and final-animation classifications plus a
truncated work-RAM snapshot. The inactive opening and final-animation paths
remain emulated until a natural route exercises them.

### Eyeball initializer extension: semantic `02:4DC8@all`

The fixed slot `$2F` initializer clears its transient flag bit, clears the
Dragon's-Egg completion latch and local state, then uses the shared coroutine
script helper to install steady dispatch `02:4DD9`. Its sole natural call at
frame 872 has an exact 258-cycle, ten-write transcript. The typed adapter is
restricted to that slot and register state, preserves every direct and
helper-owned write, and supplies the final `01:C5E6` interrupt-sampling
boundary before returning at `01:C5E9`.

The expanded owning Release regression replaces all 1,989 observed Eyeball
calls: one initializer plus 1,988 checkpointed dormant-gate calls. It also
shadows `02:4DC8@1` at frame 872 with its 258-cycle, ten-write artifact and
retains the existing `02:4DD9@1` dormant replay. The activation and
final-animation state machine remain assembly fallback, except for the
separately proven opening-timer wait below.

### Eyeball opening-timer wait: semantic `02:4DF7@all`

After the Dragon's-Egg completion latch starts the fixed slot `$2F` opening
state, the coroutine schedules direct entry `02:4DF7`. It decrements indexed
timer `$3A2F`; when the result remains nonzero, it returns locally at
`02:4E13`. The controlled completion-latch route has seven exact 33-cycle,
one-write waits. `dc_classify_giant_skeleton_eyeball_opening_wait` admits
only that slot, opening state, and timer-greater-than-one condition. Both
timer-expiry paths retain the original animation, audio, and scheduler work.

The focused Release route observes nine `02:4DF7` calls: seven native waits,
one 132-cycle early expiry, and one 2,487-cycle completion scheduler path.
Its census, entry, call, and ordered-write SHA-256 values are respectively:

- `41c10591867cf5bce6440f563e014cd40a0291757230eb77045f7fcd4d51a8bf`
- `ecc0807204d69acd7bb85e21331bc4126a72b309046902207808f2217f35703c`
- `11c72476f845698d709e2ebd11709d7b5d5ba4913a8871bd3035524cacae08ce`
- `4dbf1b5f665ba86a1b1d22094a532a4ba9df63c731e315d2d30eb9aa650b7e85`

Normalized replay `02:4DF7@2` returns at frame 1,202 after 33 body cycles
and one ordered write. The expanded Debug family checkpoint preserves the
same entry/call/write artifacts; its configuration-specific census SHA-256 is
`eb5b0e905cf6a88aa9317329d6167b95fe19d683f2d598373a3312837c43f89c`.

### Eyeball steady idle: semantic `02:4E24@all`

The expiry continuation schedules direct entry `02:4E24`, which first reads
the fixed slot `$2F` collision byte at `$382F`. When it is nonsigned and
session flag `$53` has bit one clear, the routine reaches its local RTS at
`02:4E32` after an exact 51-cycle, zero-write transcript.
`dc_classify_giant_skeleton_eyeball_steady_idle` admits only that fixed slot
and idle state. Calls at a conservative user/video or timer-IRQ horizon, and
all signed-collision or final-animation continuations, remain in assembly.

The focused route observes 1,377 `02:4E24` calls: 1,355 are local idle
returns, while 22 interrupt-bearing/collision continuations range from 375
to 1,008 body cycles and retain their 276 ordered writes. The native semantic
replaces 1,066 horizon-safe idle calls. Its census, entry, call, and
ordered-write SHA-256 values are respectively:

- `41c10591867cf5bce6440f563e014cd40a0291757230eb77045f7fcd4d51a8bf`
- `2b22ba345af39e3ba613b9c58a25a5505a457c68ff12a847faf049f389800076`
- `03d9a5225fd43469656d5c65b2fe1effc7803b762db7353d59c58f4aaf42c77a`
- `134f7f71e5d9ebc331e27982b86ff961267ae4e7f267d4f89fc23c3ae48c667f`

Normalized replay `02:4E24@2` returns at frame 1,211 after 51 body cycles
and no ordered writes. The Debug family checkpoint preserves the entry/call/
write artifacts and has census SHA-256
`eb5b0e905cf6a88aa9317329d6167b95fe19d683f2d598373a3312837c43f89c`.

### Eyeball final-animation wait: semantic `02:4F3D@all`

The final-animation scheduler invokes `02:4F3D` for the fixed slot `$2F`.
When indexed main timer `$3A2F` and phase `$3B2F` both exceed one, the routine
decrements them and returns locally at `02:4F75`. The typed classifier retains
the indexed secondary counter `$3BAF` to reproduce the final `ORA` result and
flags. It declines both counter expiry/transition paths and all user/video or
timer-IRQ horizon calls.

The focused route sees 224 direct entries. Semantic `02:4F3D@all` replaces
122 safe 102-cycle, two-write waits; the remaining 102 calls retain their
animation transition, scheduler, and interrupt-bearing assembly paths. Its
census, entry, call, and ordered-write SHA-256 values are respectively:

- `41c10591867cf5bce6440f563e014cd40a0291757230eb77045f7fcd4d51a8bf`
- `e2956a3ae275dd26c5e1c2db49b33b8ed759446b0f0d4b98737df9ff6d2ca660`
- `b8a6cdea23d885c4f0ea096a5a49e9b75e2209141cd91b58ea6783468f7b3ebd`
- `bb359392ced34c7ffce95238b069b30d480dee19594b4ca39e395649081c4264`

Normalized replay `02:4F3D@2` returns at frame 3,257 after 102 body cycles
and two ordered writes. The Debug family checkpoint preserves entry/call/write
artifacts and has census SHA-256
`eb5b0e905cf6a88aa9317329d6167b95fe19d683f2d598373a3312837c43f89c`.

The enclosing `01:D525` controller is not admissible as one bounded
transaction under the current bridge. Its 128 census calls cover
1,035-49,122 body master cycles and 29-1,456 ordered writes, and tail into
several external controller exits. Its remaining work must therefore be
split at smaller state-derived boundaries; the existing 512-write limit is
not raised for this parent. The two remaining `01:D466` calls are concrete
examples: their current-slot paths enter `D525` at frames 1,820 and 2,255
and emit 823 and 868 ordered writes.

### Thirty-first native replacement: semantic `02:4FC1@all`

The three Mercy Stars share a compact normal-table activation gate. It first
returns when mode `$2254` is not normal play, then decrements remaining-ball
state `$2D52` and returns unless the original value was exactly 1. Only that
last-ball state selects the active player through `$2251`, subtracts stored
threshold `$001388` from the three score bytes at `$2205/$220C/$2213,Y`,
and activates when the score is below 500,000 displayed points.

`dc_classify_mercy_star_gate` exposes all four state-derived outcomes. The
native adapter admits the current not-last-ball path and reproduces both
loads, both branches, the decrement, accumulator, flags, and interrupt
sampling through a five-checkpoint, 54-master-cycle, zero-direct-write
transcript. Across the 3,600-frame route, all 7,950 steady calls execute
natively, including 163 calls containing original interrupt handlers. Those
handlers account for all 2,173 aggregate writes.

The registered Debug/Release regression preserves:

- Call SHA-256:
  `18c767a16b3d205f3698a4a6961f43a3c8d4bd1b72e602631c1031dbb57eba33`
- Write SHA-256:
  `ff4cb68930bc10a4e0f59b05a1e8e733ed6554aaa5b6bb874d77744410a3f91a`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 return against restored
assembly with exact 54-cycle and zero-write parity. ROM-free tests cover
non-normal mode, not-last-ball, exact score limit, activation, selected-player
score lookup, and a truncated work-RAM snapshot. The unexercised gate paths
remain emulated until natural routes exercise them.

### Thirty-second native replacement: semantic `03:65C2@all`

The fixed Three-Blobs controller compares the three global color states at
`$2D39-$2D3B`. A mismatch between the first two values returns immediately
at `03:65E9`. When the first pair matches, it compares the third value and
returns at the same instruction when that value differs. Equal states
continue into the existing score and randomized consecutive-state reset
dispatcher.

`dc_classify_three_blobs_controller` exposes the first-pair mismatch,
third-value mismatch, and equal-state outcomes. The native adapter admits
the two mismatch paths and reproduces the load, one or two comparisons,
accumulator, compare flags, and interrupt sampling through three/five
checkpoints. The uninterrupted bodies consume exactly 42 or 63 master
cycles and perform no direct writes.

Across the 3,600-frame route, all 2,650 steady calls execute natively,
including 50 calls containing original interrupt handlers. Those handlers
account for all 665 aggregate writes. The registered Debug/Release
regression preserves:

- Call SHA-256:
  `ce6ebb6fe1f195baca404dcdadab54c645c35fa0bc2167a46f3b0aee1260024f`
- Write SHA-256:
  `089a9dbb38d1e2044b005c9d070262b1aa0cd5150979a11a0207ad2fcbcb6f09`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 first-pair mismatch against
restored assembly with exact 42-cycle and zero-write parity. ROM-free tests
cover both mismatch paths, equal-state fallback classification, and a
truncated work-RAM snapshot. Equal-state outcomes remain emulated until a
bounded natural route is selected for their nested scoring and RNG work.

### Thirty-third native replacement: semantic `02:42F6@all`

The Dragon's-Egg group controller loads shared live count `$2CF4`. A nonzero
count returns immediately at `02:42FB`; zero continues into the existing
completion sequence that increments `$2CFB`, sets `$2D20` bit 0, awards 100
stored score units, and begins the egg reset.

`dc_classify_dragon_egg_controller` exposes the active and complete outcomes.
The native adapter admits the active path and reproduces the absolute load,
not-taken zero branch, accumulator, flags, and interrupt sampling through two
checkpoints. The uninterrupted body consumes exactly 21 master cycles and
performs no direct writes.

Across the 3,600-frame route, all 1,790 controller calls execute natively,
including 17 calls containing original interrupt handlers. Those handlers
account for all 219 aggregate writes. The registered Debug/Release regression
preserves:

- Call SHA-256:
  `2adbbdfefa20a785460ddb24bf99f78e1f9d9b74210c05674667f0b91cb75e04`
- Write SHA-256:
  `4a7b82f79cc0b85b1eaf1686eda6d17925898bb7685bc4fa3bf6bdcb8b15652e`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 active return against restored
assembly with exact 21-cycle and zero-write parity. ROM-free tests cover
nonzero and zero classification plus a truncated work-RAM snapshot. The
count-zero completion path remains emulated until its nested score and reset
work is selected as a bounded replacement.

### Thirty-fourth native replacement: semantic `02:4345@all`

Each fixed Dragon's Egg loads its indexed collision state from `$3800,X`.
A nonnegative byte returns immediately at `02:434A`; bit 7 continues into
the existing path that awards 10 stored score units, selects impact command
`$39`, disables the egg, and allocates its hatchling.

`dc_classify_dragon_egg_hit_gate` exposes the idle and triggered outcomes for
the live actor slot. The native adapter retains the two-checkpoint,
21-cycle, zero-write idle return. On the routed hit, it takes the negative
branch and writes score argument `$002F:$0030 = $000A` before handing the
original score/audio helpers entry `02:4351` control after 57 cycles.

Semantic continuation `02:4357@all` resumes after those helpers. It sets bit
6 in indexed actor state `$3080,X`, installs impact command `$39`, and writes
`$F8` to allocator parameters `$2B/$2C` through a 93-cycle, four-write
transcript. Original helper `02:41FF` and allocator `02:43A4` retain burst
and hatchling creation. Semantic `02:4371@all` then writes `$01` to
`$3980,X` in 21 cycles before returning to the original coroutine scheduler
at `02:4376`.

Across the 3,600-frame route, all 9,857 gate calls plus the one entry at each
continuation execute natively. The registered Debug/Release regression
preserves all 1,682 aggregate outer writes and:

- Call SHA-256:
  `33b991bd2fa83f5e8aa0a3cefa5001b299c95e34cc8fdf230f0ed96fdc83c6b4`
- Write SHA-256:
  `3fb20b0f8a9359c995f83c83f1bb0e96e1aa2c1c88158dbc162017d063a3ffd0`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 idle return against restored
assembly with exact 21-cycle and zero-write parity. ROM-free tests cover
idle and triggered classification, a truncated snapshot, and an invalid
actor slot. The stable frame-2,425 `02:4371@1` replay preserves the complete
213-cycle, eight-write outer scope around its 21-cycle native prefix. The
focused continuation call/write SHA-256 values are
`291c4e486e4d173fd34b60f2b20b1b04a8a16e9a0f71edd500190b2adf2aec9c`
and
`2fc625cf5a5fc5d0f700bb7b8fb805a419a31733bc22baf5848aa5f7ad951088`.

### Thirty-fifth native replacement: semantic `03:62B8@all`

The upper-middle Giant Door loads its indexed collision state from
`$3800,X`. A nonnegative byte takes `BPL` and returns immediately at
`03:62D3`; bit 7 continues into the existing path that awards 50 stored
score units, decrements the three-hit counter, and starts the four-channel
completion handshake after the final hit.

`dc_classify_giant_door_hit_gate` exposes the idle and triggered outcomes for
the live actor slot. The native adapter admits the idle path and reproduces
the absolute-indexed load, taken positive branch, accumulator, flags, and
interrupt sampling through two checkpoints. The uninterrupted body consumes
exactly 27 master cycles and performs no direct writes. A geometry-delivered
collision route places ball slot zero at `(64,256)` in frame 1,228; collision
staging naturally delivers `$90` to `$384C` at frames 1,316 and 1,338. Both
sign-bit calls execute the exact 48-cycle load/not-taken-branch/mask/clear/
score-load prefix natively, then resume original `03:62C4` for the aliased
score operands and shared helper. Its Release owning regression preserves the
complete census, call, ordered-write, and selected-state artifacts.

The companion semantic `03:62CB@all` admits only fixed slot `$4C` with a live
counter of one through three. It reproduces `DEC $3B00,X`, including the
counter write at cycle 21, then takes the first two hits through the 27-master-
cycle nonzero branch to original `03:62D0`. A second geometry placement at
frame 1,340 yields the terminal contact at frame 1,372; its taken zero branch
uses 33 master cycles and enters semantic `03:62D4@all`. That exact 57-cycle,
four-write prefix loads the 50,000-point `$01F4` score argument, preserves the
`62DE` JSR return frame, and hands original `01:C02A` score processing back to
assembly. On that helper's return, semantic `03:62E2@all` performs the exact
243-cycle, nine-write completion-state transaction: it ORs `$60` into
`$3080,X`, clears `$3B00,X`, and composes the existing `01:C018/C5C9`
coroutine scheduler to install the subsequent `03:62F0` dispatch. That
`03:62F0@all` successor sets bit zero in the live indexed `$2D06`
handshake channel and composes the same scheduler to install `03:62FE` in an
exact 243-cycle, eight-write transcript. That `03:62FE@all` successor owns
the clear-bit polling branch: its indexed channel load, bit-one mask, taken
wait return, and four interrupt checkpoints reproduce all 378 calls (including
five timer-interrupt passages) in an exact 48-cycle zero-write body. The
set-bit `03:6308@all` successor now owns all four channel advances: it
increments `$3B4C`, takes the nonterminal comparison branch and composes the
coroutine scheduler in an exact 252-cycle, eight-write transcript for channels
one through three, or takes the equality branch in an exact 246-cycle,
eight-write transcript to schedule `03:6318` for terminal channel four. The
callback is a one-byte coroutine `RTS`; the released Knight's `03:7378`
activation prefix is native, while its later movement and hit body remains
assembly-owned. The
completion-route regression preserves exact census, call, ordered-write, and
selected-state artifacts in both configurations. Its Debug terminal route
hashes are census
`eabc62d7004bed2b1512c9a1b9aa2f4dcb55d98bc3ae6318a64a5922cc1ef193`, call
`91becf34f9fd330df75c7c8df138738eecebc671403685da96fad37c5a301687`, and
write `5880d6d56f946747ff34bf97df773c6358d9a8073faf9bbb9a17a5830472993a`.
The dedicated post-helper scheduler regression additionally preserves Debug
census `eabc62d7004bed2b1512c9a1b9aa2f4dcb55d98bc3ae6318a64a5922cc1ef193`,
call `d85ac681c4370ab16fd0297c7fff7e324f054447b8aee467a3c4daa4ef1a70b8`, and
write `93ddf2f9365c168ccdbc951d9448c609b02bd120ebd72d286664675208c4b266`.
The request-prefix regression preserves Release census
`5e39c2543cf8a17bc5e74e7c55693c0d6abf7429ea82822df8b641eb7860f28b`, call
`97f8c3fc24bd37e80517ad61b130ad41488be03ae060d09f73dc25ba3a644892`, and
write `6f9f18ad307c99e7743143cdf6f6edb62ba14749a0eadf38e8bc32b765e99c37`.
The completion-wait regression preserves Release census
`5e39c2543cf8a17bc5e74e7c55693c0d6abf7429ea82822df8b641eb7860f28b`, call
`045b058fa941239e7640f4c4831f545b317a3a7c86a780d8400f0e678fadea18`, and
write `420145b28f2a4c8d4f64517acfbeeb43b1902e35f51f002e3c1e3457a72249e9`.
Debug preserves census
`eabc62d7004bed2b1512c9a1b9aa2f4dcb55d98bc3ae6318a64a5922cc1ef193` with
the same call and ordered-write artifacts.
The terminal-channel regression preserves Release census
`4d53b12cc98765dbdca75627bca14c602dda8d02d5c13478e43a03d1bd404317`, call
`09f4fb600c3881ec19b9ad0bb6ce3f13659f45c0698ec60f1ba860053b861d7c`, and
write `29c33530271acfcff8d45b17302419b94136f5e272a5d1856adf89d3bceb0ffb`.
Debug preserves census
`3d2f588027fd6ed84764b16dd8c61dee0fdf22ca47f40803a75338f3c816d5b9` with
the same call and ordered-write artifacts.
The Knight-release transition regression preserves Release census
`9b1bbcadc419ba8ed03e9864734ed667dffcb6c72638d1be87e3e403a0d90021`, call
`0c3e450c58a81c995f5e84cdb8b83c722ff1f2584c20198d3d2d8c3bcd0b3270`, and
write `f1ed812127e7310cea8891cc76f271b3dcf1f916ed7662b097a4565a2e9a1fa6`.
Debug preserves census
`d0f0f3d7902c77e9937727cd8001446f5657d11d1ff278d69d8108304d89b4d3` with
the same call and ordered-write artifacts.

Across the 3,600-frame route, all 676 calls execute natively, including
seven calls containing original interrupt handlers. Those handlers account
for all 88 aggregate writes. The registered Debug/Release regression
preserves:

- Call SHA-256:
  `ce3630ab97227c504bdc79202e6ad794d558431a2af769b5c1f471fd452753cc`
- Write SHA-256:
  `7e187ab17d7db0fc9411d3ec524023cf270c2d9ec6ce16f9b414a39541063ebb`
- Debug state-census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release state-census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-1,228 idle return against restored
assembly with exact 27-cycle and zero-write parity. ROM-free tests cover
idle and triggered classification, a truncated snapshot, and an invalid
actor slot. The triggered path remains emulated so its nested scoring,
hit-counter, impact, and completion behavior retains the established
assembly implementation.

### Thirty-sixth native replacement: semantic `02:445B@all`

The hidden Big-Star feature controller loads the shared Little-Skull result
latch from `$2CFA`. A zero value falls through `BNE` and returns immediately
at `02:4460`; pending bits continue into the existing direction, actor,
score, and audio update.

`dc_classify_big_star_feature_gate` exposes the idle and pending outcomes.
The native adapter admits the idle path and reproduces the absolute load,
not-taken nonzero branch, accumulator, flags, and interrupt sampling through
two checkpoints. The uninterrupted body consumes exactly 21 master cycles
and performs no writes.

The state-census route contains 2,650 calls: 2,622 uninterrupted leaves and
28 calls containing original interrupt-handler passages. Semantic
replacement admits all 2,650 calls while preserving the complete 364-write
aggregate. Debug and Release preserve the assembly-only artifacts exactly:

- Call SHA-256:
  `3afba05be63a290e4e1e453212f354495c082e90ab116fe3716e104ade182943`
- Write SHA-256:
  `c0789fb65ce97f845a147d6c000ec55b3724ee3146499e4cb7c1328c186d3b29`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 idle return against restored
assembly with exact 21-cycle and zero-write parity. ROM-free tests cover
idle and pending classification plus a truncated work-RAM snapshot. The
pending-feature path remains emulated until a natural route exercises its
direction, actor, score, and audio behavior.

### Thirty-seventh native replacement: semantic `03:6339@all`

The Walled Star controller first loads indexed collision state `$3800,X`.
Bit 7 enters the existing wall-damage path. A nonnegative value branches to
the shared post-destruction access latch at `$2DBC`; zero returns immediately
at `03:6375`, while a nonzero latch continues into the existing access path.

`dc_classify_walled_star_hit_gate` exposes collision, idle, and shared-active
outcomes. The native adapter admits the idle path and reproduces both loads,
the taken nonnegative and zero branches, accumulator, flags, and interrupt
sampling through four checkpoints. The uninterrupted body consumes exactly
54 master cycles and performs no writes.

The state-census route contains 2,650 calls: 2,601 uninterrupted leaves and
49 calls containing original interrupt-handler passages. Semantic
replacement admits all 2,650 calls while preserving the complete 659-write
aggregate. Debug and Release preserve the assembly-only artifacts exactly:

- Call SHA-256:
  `7789b6b0ef005e1d302342c9c03e569d2c8aad3c43bea200aa05920f7182f46e`
- Write SHA-256:
  `f847b9fa97dd98c6afa4011eb6b134f9f1551d2ed25ab5a59f2f219b7471d2dc`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 idle return against restored
assembly with exact 54-cycle and zero-write parity. ROM-free tests cover
idle, collision, shared-active, and truncated-snapshot classification.
Collision damage and active post-destruction access remain emulated.

### Walled Star post-destruction idle prefix: semantic `03:638F@all`

After the terminal wall-damage state, fixed slot `$5B` dispatches through
`03:638F`. The controller loads primary access latch `$2D53`, then fallback
latch `$2D32`; only when both are zero does it return at `03:6399`.

Semantic `03:638F@all` admits that clear-latch leaf. It preserves the two
loads, two not-taken branches, accumulator/flags, and four interrupt
checkpoints, consuming 42 master cycles with no direct writes before the
original RTS. A controlled Release route contains 2,390 entries: 2,354
uninterrupted leaves and 36 original timer-interrupt passages. Its census,
call, and ordered-write SHA-256 artifacts are respectively
`432af3cde4152def8c9afc6845b9575c8812859e76330e8de1f3dd61c9a24ee6`,
`6bdbc72c548309243846bb601536361693c162bd6689dbec1532f6a034f78cf6`, and
`1df166a843eb05bb8f258d942b6c0999b14dee6c5c723276dab8702b55b96b10`.
The Debug census also matches its assembly baseline at
`7de49230f4654595da7eb268945929c9cbceabcadb03de21cddd376dc285d1ac`.
Any asserted latch still falls back to assembly for acknowledgment, animation,
and bonus-stage routing.

### Thirty-eighth native replacement: semantic `03:60C3@all`

The fixed Tower controller first loads indexed collision state `$3800,X`.
Bit 7 enters its existing hit path; a nonnegative value branches directly
to the return at `03:612A`.

`dc_classify_tower_hit_gate` exposes idle and triggered outcomes. The native
adapter reproduces the indexed load, branch, accumulator, flags, and
interrupt sampling. Idle calls consume exactly 27 master cycles and perform
no writes. The routed trigger clears the indexed collision latch and reaches
the shared score helper after 63 native cycles.

The state-census route contains 2,646 calls: 2,615 uninterrupted idle
leaves, 30 idle calls containing original interrupt-handler passages, and
one real hit at frame 3,353. Semantic replacement admits all 2,646 calls.
The score and audio helpers remain emulated. Post-helper semantic
`03:60E0@all` performs the routed progress increment and transition flag in
156 native cycles, then hands the original interrupt-bearing shared
dispatcher to assembly. Post-dispatch semantic `03:611C@all` composes the
native coroutine scheduler for all four transition calls: three 135-cycle,
five-write countdown paths and one 213-cycle, eight-write dispatch install.
The complete outer 538-write aggregate remains exact:

- Call SHA-256:
  `1b6440a384b815ba8ee97473dc389a56f9455ec0a82ee307c5baa499bbf3712f`
- Write SHA-256:
  `26696c169ae2296a752ea3916c9151134febbb89ef1a9eea54826645c64815c3`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 idle return against restored
assembly with exact 27-cycle and zero-write parity. `03:611C@1` independently
replays the first frame-3,353 transition with exact 135-cycle and five-write
parity. The five-call continuation artifact has call SHA-256
`7e282adac47c3b913dc254b24cb364e41a12ac7c216f5d9944bac4a1e38306b8`
and write SHA-256
`eb7ffe66312e2c613cb842a6e755f8facba6574218e069a93be809ec2d27ba04`.
ROM-free tests cover idle, triggered, and truncated-snapshot classification.
The shared score/audio and dispatcher helpers remain emulated; Tower-owned
progress and Tower-Guard/BS-5 entrance-gate transition state are native on
the routed path.

### Thirty-ninth native replacement: semantic `03:60A6@all`

The Tower Guard update loads closed selector `$84`, reads Tower-owned
five-step counter `$2D4D`, and branches past the open-selector load while
that counter is nonzero. It then writes the selector to indexed completion
field `$3D00,X` and reaches the return at `03:60B2`.

`dc_classify_tower_guard_selector` exposes the nonzero closed outcome and
zero-counter open outcome. The native adapter admits only the route-proven
closed path and reproduces the accumulator, Y register, flags, indexed
write, and interrupt sampling through four checkpoints. The uninterrupted
body consumes exactly 48 master cycles and performs one write.

All 2,650 state-census calls retain counter `$05` and selector `$84`: 2,592
are uninterrupted leaves and 58 contain original interrupt-handler
passages. Semantic replacement admits every call and preserves the complete
3,403-write aggregate. Debug and Release preserve the assembly-only
artifacts exactly:

- Call SHA-256:
  `942df45f1604783b877753a92fedf7ef806cf7b56cbd7456f0fc1b53cc4c26bd`
- Write SHA-256:
  `e2a76edd3d070915c33cc37891072178bbd8427c816d88262fe8ac1cc0ce5528`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 closed update against restored
assembly with exact 48-cycle, one-write parity. ROM-free tests cover closed,
open, and truncated-snapshot classification. Counter zero, selector `$8E`,
and the open BS-5 entrance remain in assembly until dynamically exercised.

### Fortieth native replacement: semantic `03:7CAF@all`

The hidden fixed collision/scene-response controller first loads scene state
`$2CC0`; a negative value jumps to the shared inactive actor path. A
nonnegative value loads zero-page collision scratch `$CC`. Zero returns at
`03:7CBB`, while nonzero scratch enters the existing scene query, audio,
ball-response, and countdown state machine.

`dc_classify_fixed_response_gate` exposes inactive, dormant, and active
outcomes. The native adapter admits only the route-proven dormant path and
reproduces both loads, both branches, accumulator, flags, and interrupt
sampling through four checkpoints. The uninterrupted body consumes exactly
45 master cycles and performs no direct writes.

All 2,650 state-census calls take the dormant path: 2,596 are uninterrupted
leaves and 54 contain original interrupt-handler passages. Semantic
replacement admits every call and preserves the complete 721-write
aggregate. Debug and Release preserve the assembly-only artifacts exactly:

- Call SHA-256:
  `64a5571154cc9053e92176ea6b7100432f73540d464e3f8b48c6ef80a8b322fa`
- Write SHA-256:
  `e585851a0949429e428026066f706bb4c40a5b0cd9ef59dbef9fd553df2d5a47`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 dormant return against
restored assembly with exact 45-cycle, zero-write parity. ROM-free tests
cover inactive, dormant, active, and truncated-snapshot classification.
Negative-scene handling and the nonzero response state machine remain in
assembly until dynamically exercised.

### Forty-first native replacement: semantic `03:7B27@all`

The hidden slot-`$5C` fixed scheduler first loads scene state `$2CC0`; a
negative value jumps to the shared inactive actor path. A nonnegative scene
loads presentation gate `$2BB9`; nonzero returns immediately at `03:7B34`.
When presentation is enabled, indexed timer `$3A00,X` decrements. A nonzero
result returns at `03:7B6C`, while zero reloads eight and submits selectors
`$02`, `$03`, and `$07` through the original banked-call wrapper.

`dc_classify_fixed_scheduler_gate` exposes inactive, suppressed, waiting,
and active outcomes from live work RAM. The native adapter admits every
route-proven nonnegative-scene path. It reproduces the loads, branches,
indexed decrement, accumulator, flags, ordered writes, and interrupt
sampling. Suppressed/waiting bodies consume 48/87 master cycles and perform
zero/one direct writes. An active tick uses a 156-cycle prefix to reload
eight and install the first `$02/$30/$08` submission before handing the
original wrapper control.

Of 2,650 state-census calls, one is presentation-suppressed, 2,317 decrement
without timer expiry, and 332 enter the active triple-submission tick.
Semantic `03:7B27@all` therefore admits all 2,650. Once the first original
wrapper returns, semantic `03:7B4E@all` installs the `$03` submission and
semantic `03:7B5D@all` installs `$07`; each continuation executes 332 times
through an exact 54-cycle, three-write transcript. All three wrappers and
the exported `08:4030` routine stay in assembly. The complete 46,618-write
aggregate remains exact, and Debug and Release preserve the baseline
artifacts:

- Call SHA-256:
  `f8696a6a4fe1eceef25d312e722a5ac2279b1886f018eb5f6cf145984acb8645`
- Write SHA-256:
  `257383a1a8128e9d5b3159ceaeb9766c54257bc2cc6b0b53b3fe641ee625c521`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 presentation-suppressed
return against restored assembly with exact 48-cycle, zero-write parity.
ROM-free tests cover inactive, suppressed, waiting, active, and
truncated-snapshot classification. The stable ordinal-1 suppressed call
shadows exactly; full-route hashes guard every active tick because a
normalized isolated replay of the long downstream helper can shift its
interrupt instruction. Only negative-scene handling remains in assembly.

### Forty-second native replacement: semantic `03:7B7A@all`

The hidden slot-`$5D` three-marker animator shares the scheduler's scene and
presentation gates. A negative scene jumps to the shared inactive actor
path, while a nonzero `$2BB9` returns at `03:7B87`. When presentation is
enabled, indexed timer `$3A00,X` decrements. A nonzero result jumps to the
return at `03:7BDF`; zero reloads eight and advances the three marker
descriptors through phase table `0,1,2,3,4,3,2,1`.

The shared `dc_classify_fixed_scheduler_gate` exposes inactive, suppressed,
waiting, and active outcomes from live work RAM. The `03:7B7A` adapter
reproduces the exact 48/93-cycle suppressed and waiting paths. On every
route-proven active tick it reloads eight, increments `$3A80,X`, wraps at
eight, loads the resulting phase into Y, preserves the compare carry, and
installs the first `$1E:16` descriptor before returning at the original
`PHY`/drawing-helper boundary. Nineteen instruction checkpoints describe
the 231-cycle non-wrap prefix; a twentieth checkpoint and second ordered
phase write describe each 240-cycle wrap prefix.

Of 2,650 state-census calls, one is presentation-suppressed, 2,317 decrement
without timer expiry, and 332 enter the active marker-update tick. All 2,650
now enter native code. The active set contains 291 non-wrap prefixes and 41
wrap prefixes. After the first original drawing and marker-helper calls,
semantic `03:7BBB@all` installs descriptor `$1E:18`; after the second pair,
semantic `03:7BCD@all` installs `$1E:1A`. Each continuation executes 332
times through four instruction checkpoints, two direct writes, and 36
master cycles. Semantic `03:7BE0@all` replaces every marker wrapper's
`PHY/PHA`, phase-table load, and handoff to the first shared helper through
three checkpoints, two stack writes, and 33 master cycles. Semantic
`03:7BE8@all` restores the saved accumulator in 12 cycles after that helper,
and semantic `03:7BEC@all` restores Y in 12 cycles after the second helper.
Each segment executes 996 times and reads or writes the live emulated stack.
The shared phase-stride helper is now native as described below; only the
`01:C05D` drawing submission remains an assembly boundary. The complete
88,480-write aggregate remains exact. Debug and Release preserve:

- Call SHA-256:
  `5ed50a6da96f19502e06fd4622c3986ca9fab5aadd28d26603075e6968cc52ff`
- Write SHA-256:
  `2b78c118caa3cdb9f37a19d40dc9f967eb0eb232b56c27d3bde64a6d350301f9`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

Ordinal 1 independently shadows its frame-873 presentation-suppressed
return against restored assembly with exact 48-cycle, zero-write parity.
ROM-free tests cover slot-`$5D` timer classification and truncated snapshots.
Negative-scene handling remains unobserved and in assembly.

### Forty-third native replacement: semantic `02:5955@all`

This shared phase-stride helper shifts the input accumulator left five
places and adds the resulting unsigned word to scratch pointer `$2F:$30`.
Each marker wrapper calls it after selecting phase
`0,1,2,3,4,3,2,1`, advancing the descriptor source by 32 bytes per phase
before `01:C05D` performs the drawing submission. One additional routed
caller at `03:7664` uses the same arithmetic.

`dc_add_scaled_byte_to_word` exposes all five shifted low/high pairs and the
final little-endian sum without mutating the copied RAM view. The host
adapter recreates `STZ`, five `ASL A`/`ROL $2E` pairs, `CLC`, and the two
byte additions through 15 exact instruction checkpoints. The uninterrupted
body consumes 198 master cycles and writes `$2E` six times followed by
`$2F` and `$30`.

The state-census route contains 997 calls: 996 from the marker wrapper and
one from `03:7664`. All execute natively, including 75 calls containing
original interrupt-handler passages; all 8,978 aggregate writes remain
exact. Ordinal 1 independently shadows its frame-940 call against restored
assembly with exact 198-cycle and eight-write parity. ROM-free tests cover
ordinary addition, both shift and addition carries, and truncated RAM.
Debug and Release preserve:

- Call SHA-256:
  `1b13d35fb24027b1cce65657d384c44df7c12a1c1cad8b2c5bfb0e77ed8cd7f8`
- Write SHA-256:
  `4f850f8293f347dd5798764fd07f1fc83e01fd41a81c666aac1cbcf3c907795c`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

### Forty-fourth native replacement: semantic `03:767E@all`

The Giant Face's hidden-form animator loads shared animation mode `$2D0E`
and compares it with one. Mode one enters the timer, phase, and drawing
state machine beginning at `03:7687`; every other value returns at
`03:7685`. The state-census route visits the dispatch 873 times and always
takes that idle return.

`dc_classify_giant_face_animator_gate` exposes the live mode and separates
the mode-one active path from the routed idle path. The host adapter
recreates the absolute load, immediate comparison, and untaken `BEQ` through
three exact instruction checkpoints. Each uninterrupted call consumes 27
master cycles and performs no direct writes.

All 873 calls execute natively. Eight contain original interrupt-handler
passages, whose 97 writes remain exact. Ordinal 1 independently shadows its
frame-1,222 call with exact 27-cycle, zero-write parity. The existing
5,608-call/812-write Giant Face family regression remains byte-identical in
Debug and Release with the replacement enabled. ROM-free tests cover mode
zero, active mode one, a non-one mode, and truncated RAM. Active mode one
continues through assembly until a natural route exercises it.

- Call SHA-256:
  `b960fb4cabe1f6009e49c62ad9a0e7036aa4d2407c60ad8119f9a23b64b100d6`
- Write SHA-256:
  `874224be422684644a59f3bad052e8e6cf73098cdeb01caeba034b585d89e248`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

### Forty-fifth native replacement: semantic `03:74D8@all`

The Giant Face controller loads shared request byte `$2D0C` and masks wake
bit zero. A set bit enters the wake sequence at `03:74E0`; a clear bit
returns at `03:74DF`. The state-census route visits the dispatch 2,648 times
and always takes that idle return.

`dc_classify_giant_face_request_gate` exposes the live request byte and
separates the wake path from the routed idle path. The host adapter recreates
the absolute load, immediate mask, and untaken `BNE` through three exact
instruction checkpoints. Each uninterrupted call consumes 27 master cycles
and performs no direct writes.

All 2,648 calls execute natively. Twenty-three contain original interrupt-
handler passages, whose 289 writes remain exact. Ordinal 1 independently
shadows its frame-942 call with exact 27-cycle, zero-write parity. The
existing 5,608-call/812-write Giant Face family regression remains byte-
identical in Debug and Release with both native Giant Face gates enabled.
ROM-free tests cover request zero, unrelated bit one, wake bit zero, and
truncated RAM. Wake and transformation handling continues through assembly.

- Call SHA-256:
  `d110a06449ef76dc8af186ca017263fa0302174c2173ec7b0b3d52b4d418e289`
- Write SHA-256:
  `023a7f3933bdb84c56c5b68016fb622e6b9b21c08894a5abf85a0c65b6c978b9`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

### Forty-sixth native replacement: semantic `03:76FA@all`

The Giant Face crown loads indexed collision byte `$3800,X`. Bit 7 enters
the 300-point score, impact, and command-`$20` audio path beginning at
`03:76FF`; a clear bit takes `BPL` and returns at `03:7717`. The deterministic
state-census route invokes this collision check 867 times and always takes
the no-hit return.

`dc_classify_giant_face_crown_gate` exposes the live collision byte and
separates the deferred hit path from the native no-hit path. The host adapter
recreates the indexed load and taken branch through two exact instruction
checkpoints. Each uninterrupted call consumes 27 master cycles and has no
direct writes.

All 867 calls execute natively. Seven contain original interrupt-handler
passages, whose 86 writes remain exact. Ordinal 1 independently shadows the
frame-1,223 call with exact 27-cycle, zero-write parity. The existing
5,608-call/812-write Giant Face family regression remains byte-identical in
Debug and Release with the controller, animator, and crown gates enabled.
ROM-free tests
cover nonnegative/no-hit, bit-seven/hit, truncated RAM, and invalid slots.
The active hit path remains assembly until a natural route exercises it.

- Call SHA-256:
  `2685dfcf37be1b335eebfa5eb143cc81c72193ab199594626fa5909de28b5582`
- Write SHA-256:
  `f5a56fa651a27a000897546ee595dbe7cb003de7b30272c08c3f243911bce494`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

### Forty-seventh native replacement: semantic `03:7726@all`

The Giant Face mouth first loads `$2D21`. A nonzero state enters the original
open-mouth capture path at `03:7749`; while closed, it loads indexed collision
byte `$3800,X`. Bit 7 enters the shared 300-point score, impact, and
command-`$20` audio path at `03:7730`; a clear bit takes `BPL` and returns at
`03:7748`. The deterministic state-census route invokes this closed/no-hit
check 1,214 times and always takes that return.

`dc_classify_giant_face_mouth_gate` exposes mouth state and indexed collision
state, distinguishing deferred open-mouth and hit paths from the native
closed/no-hit path. The host adapter recreates the absolute mouth-state load,
not-taken branch, indexed collision load, and taken branch through four exact
instruction checkpoints. Each uninterrupted call consumes 48 master cycles
and has no direct writes.

All 1,214 calls execute natively. Twenty-two contain original interrupt-handler
passages, whose 288 writes remain exact. Ordinal 1 independently shadows the
frame-1,213 call with exact 48-cycle, zero-write parity. The existing
5,608-call/812-write Giant Face family regression remains byte-identical in
Debug and Release with all four Giant Face gates enabled. ROM-free tests cover
closed/no-hit, open-mouth fallback, bit-seven/hit fallback, truncated RAM, and
invalid slots. The active open-mouth and hit paths remain assembly until a
natural route exercises them.

- Call SHA-256:
  `531e2094a7c17f742b1474a3c9511fa4918349e86dc415d7ed8ce5a28dc5dc95`
- Write SHA-256:
  `48a862dec687758d8ecf7fcd480285dbc08e2fe1b9ee50a790a726ea0afc9ae5`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

### Forty-eighth native replacement: semantic `02:4C91@all`

Both mirrored Giant-Face helmet openings first load indexed collision state
`$3800,X`. Bit 7 remains on the original activation path; the collision-free
route branches to `02:4CDA`, decrements `$3A00,X`, and returns at `02:4CF1`
while the timer remains nonzero. On expiry it reloads four, increments
`$3980,X`, and either returns below appearance three or clears the appearance
at three.

`dc_classify_giant_face_helmet_controller` exposes collision, timer, and
appearance state for fixed `$2A/$2B` channels. The adapter admits all
collision-free paths through instruction checkpoints for the collision read,
branches, decrement, reload, increment, compare, and optional reset. It
preserves the three exact non-interrupt body shapes: 66 cycles/one write,
135 cycles/three writes, and 144 cycles/four writes.

All 1,748 steady calls execute natively. Fifty-seven calls retain original
interrupt-handler passages; the complete 1,752-call/3,558-write helmet
artifact and selected-state trace remain byte-identical in Release and at the
Debug family checkpoint. ROM-free coverage exercises waiting, advance, reset,
collision fallback, truncated RAM, and invalid-slot classification. The
score, audio, request, open-state, and ball-routing activation remains
assembly-backed until a natural collision route is traced.

- Call SHA-256:
  `48a1a89f272e80679e859d6ecdc34d4102d864985fba5ba64bdde12d0642e64b`
- Write SHA-256:
  `1d70d3313ad65639066692b2721f1df8d5ffc4d041e5625a3f84fceaf3dcd4fa`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

### Forty-ninth native replacement: semantic `02:4A93@all`

The four Wall-Pocket records first load indexed collision state at
`02:4A93`. The controlled lower-left collision now executes the exact
`LDA $383E` / taken `BMI $4A9B` plus selector and zero-rotation decision
prefix natively: the non-matching arrow reaches original `$4ABB` in 144
cycles, while the matching arrow clears `$2CFA`, loads audio command `$41`, and reaches original `$4ACF` in 171 cycles. Its shared audio helper remains assembly-backed,
but post-audio `02:4AD2@all` composes the fixed 100-unit score and four-tick
timer (including the score's timer IRQ); after the original `$406F` appearance
helper, `02:4AE5@all` writes phase four and composes the scheduler handoff to
`$4AEB`. The matching route preserves its initial timer interrupt at the
shared 27-cycle checkpoint. The
1,008 collision-free calls branch to the ROM-shared
`02:4CDA` tail, decrement `$3A00,X`, reload the four-tick timer on expiry,
and advance or reset `$3980,X` exactly as the Giant-Face helmet controller.

`dc_classify_four_star_wall_pocket_controller` makes that shared
collision/timer/appearance state explicit. The adapter supplies the distinct
`4A93/4A96/4A98` entry checkpoints before composing the common tail's exact
66-cycle/one-write, 135-cycle/three-write, and 144-cycle/four-write paths.
All no-entry calls execute natively; original interrupt handlers are retained
through the instruction checkpoints. The focused collision route has 1,168
entries, including the 4,482-cycle access scope, and its original/native
artifacts match exactly in both configurations. Debug and Release preserve
the complete 1,016-call/2,036-write steady-family artifact and selected-state
trace.

- Call SHA-256:
  `2f86b84a3997f8a84239a554ecb34523fc94c564f337ff446b8df61f91b1d64b`
- Write SHA-256:
  `d4fe55cf0b4b975eecd160accace5b3f0e753bf05b99b2e6a5c7b7d375f256b8`
- Debug census SHA-256:
  `a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2`
- Release census SHA-256:
  `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`

`dc_rom_four_star_wall_pocket_collision` fixes the unique collision route's
configuration-specific census SHA-256 values to
`d7a897a7c019c8b618176c9b92371d08f82f602b3ba17cd5fede2e8dcf0c000c`
(Debug) and
`53432539e70810d69ef8b3cce45cd81300749ea021f27f4fd71e8587ccfbe28e`
(Release). Its shared trace/call/write/state SHA-256 values are
`414300f98d17d9dfcbb29723f818de3114848e7b44c7ca87d359153de24c3526`,
`a4e5c8e36845ab70dd128a3b2e814dfdff5cc2ae406d00336c1494b0ee13f808`,
`f822e87f6a423a9de0d00f10ae6bea73bde54c314c734d13c740f86886ccb9fc`, and
`a019a78ef19fc7d72e11b329aaf90bb390e50cf6728a56aa897c1a974664069a`.

### Fiftieth native replacement: semantic `02:4AEB@all`

The lower-left Four-Star Wall-Pocket's scheduled animation continuation now
executes natively after the matching-arrow handoff. Appearance values below
six decrement `$3A00,X`; a nonzero timer returns through the exact 60-cycle,
one-write path, while expiry reloads four, increments the shared `$3980`
appearance, and returns through the exact 96-cycle, three-write path. The
adapter separates write visibility from its HuC6280 instruction-boundary IRQ
sample, preserving all naturally observed interrupt-handler passages and
ordered writes.

All 506 calls on the deterministic matching route execute natively. A
controlled appearance-six probe reaches the `$4B00` scene-transition body;
its 942-cycle, 22-write original-helper and scheduler transaction remains
assembly fallback. The expanded matching regression retains
its original Release census and selected-state artifacts; its new trace,
call, and write SHA-256 values are
`6105daa60a740fc1d9f9a1b9e421a1dc9c87fe980e8aec4b804214a12087dea2`,
`9abc6c7b5a8d2518f5ecc5481bc693917b056659e9d9f0eec95e155e4ee30e24`, and
`4e983a7998075dfa71ddd478cdcab4ed8995ce7c7960ced84ccbc0bd15ff59a3`.
Release and the required Debug family checkpoint both pass.

### Fifty-first native replacement: semantic `02:4B3F@all`

The Walled-Star access pocket at fixed slot `$11` shares the common
four-tick collision/timer/appearance tail starting at `02:4CDA`. Its
collision-free path loads `$3811`, takes the original `JMP $4CDA` boundary,
then executes the existing native 66-cycle timer wait, 135-cycle appearance
advance, or 144-cycle appearance-reset transcript. The `02:4B3F` entry
adapter retains its three exact prefix checkpoints before composing that
tail, including naturally occurring timer IRQ passages.

All 905 naturally observed steady calls now execute natively. Bit-7 collision
entry stays assembly-owned. A controlled `$3811=$80` probe reaches it at
frame 1,220 as a 1,182-cycle, 31-write transaction across original
allocation, score, opening animation, and coroutine scheduling, so it is not
an independently composable transition. The owning Walled-Star access
regression preserves its
existing Release trace/call/write/state SHA-256 artifacts:
`bef6b62849397616eabdbc4f63247da80689c1472925c28e4bbc438ba82f830b`,
`1ff4a18a3f7caa572e46000abcd46598b96f3239fae0e8c75cec3a7a98aad3b8`,
`26c396dc657674d9d4e93c3ea07cd76390ef2ae9bb12bb7878997ede8bc4a8a5`,
and `dcaf467b0f5e7bc35b3051723ddebb6da2ef5f4266c8573d15f48f3b34d6c876`.

### Fifty-second native replacement: semantic `02:4BD5/4C0A@all`

The lower-left and upper-right Screen-2 Side-Star access surfaces share the
same `LDA $3800,X` / collision branch / `JMP $4CDA` prefix and common
four-tick tail as the Walled-Star access pocket. Semantic `02:4BD5@all` and
`02:4C0A@all` therefore compose that already-proven tail with their distinct
three-prefix-checkpoint entries, retaining exact 66-cycle timer waits,
135-cycle appearance advances, 144-cycle resets, and natural timer IRQ
passages.

All 1,277 lower-left and 905 upper-right collision-free calls now execute
natively. Bit-7 collision entry remains assembly-owned. A controlled
lower-left `$3810=$80` probe reaches it at frame 1,210 as a 1,902-cycle,
44-write score/effect, Giant-Face request, animation, and scheduler
transaction. The upper-right entry additionally has its form-zero six-step
bonus-stage-6 entrance. A controlled upper-right `$3812=$80` probe reaches
that persisted-form-zero sequence at frame 1,220 as a 1,188-cycle, 30-write
score/animation/scheduler transaction that schedules the entrance's next
step. The scheduled `02:4C33@all` continuation now natively covers its eight
nonterminal 60-cycle timer decrements and 96-cycle reload/appearance advances;
the ninth stage-selection transition remains assembly-backed. Neither
collision sequence is an independently composable leaf. The owning Side-Star regression
preserves its existing Release trace/call/write/state SHA-256 artifacts:
`7091984892271017079a1d98450f5e72f1242017eed26bb66c1209d917113339`,
`4cd9ebfab43aa644483536b36f95d49dc47c02743f5d3f51d5fe2b824ddcd956`,
`e4a2b7c0be94f44bd7593d8b0b4930a7f95aca598fd8acf90e357ddaab5279c2`,
and `a8932210562ebfb1b4195fbc1b27e9094a604fa10a9255c8364726d6bf01650e`.
The required Debug access-controller family checkpoint also passes alongside
the Release regression.

### Fifty-third native replacement: semantic `03:73D8@all`

The queued-Knight collision helper reads indexed `$3800,X` and returns through
`03:73FD` when bit seven is clear. For nonterminal set-bit hits, semantic
`03:73D8@all` clears the indexed latch, decrements the four-hit counter, and
supplies the exact 96-cycle `JSR $7BF6` frame. A terminal fourth hit now also
executes the bounded 75-cycle latch-clear/decrement-to-zero/`SEC` prefix and
returns at `03:73E8` with carry set. The movement parent rejects that
carry-set child so its downstream destruction state machine remains isolated.
Its terminal successor `03:73FE@all` now supplies the exact 51-cycle
100-unit score-argument/stack handoff to the existing banked `01:C02A`
score helper. After the assembly-owned shared release helper returns,
`03:740A@all` supplies the exact 51-cycle queue-channel acknowledgement
prefix and resumes at original `03:7415` for the common helper.
That helper's terminal `02:41A0@all` entry now supplies only its exact
48-cycle four-store scratch-clear prefix, resuming at original `02:41A8` for
the next command setup. Its `02:41A8@all` successor supplies the exact
21-cycle `LDA #$2D` / `STA $2D6E` prefix. The new `02:41AD@all` successor
then preserves the exact 123-cycle `CLY`/`PHY` saved-index and cleared-scratch
table-coordinate setup, resuming at original `02:41C2` for the shared `41F5`
call. Its `02:41C2@all` successor supplies the exact 21-cycle `JSR $41F5`
stack frame and resumes at the shared child's original entry. Its terminal-only
`02:41F5@all` successor supplies the exact 48-cycle `$E1:$42` to `$2D:$2E`
pointer-copy prefix and resumes at original `02:420B`. The shared call, table,
actor, and coroutine body remain assembly-backed. Its terminal-only
`02:420B@all` successor supplies the exact 21-cycle `JSR $C024` stack frame;
its terminal-only `01:C024@all` child successor supplies the exact 12-cycle
jump to `$C5AB`. Its terminal-only `01:C5AB@all` successor supplies the
exact 6-cycle scan initializer. Its terminal-only `01:C5AD@all` successor
supplies the exact 15-cycle slot-four occupancy read and resumes at the
original branch. Its terminal-only `01:C5B0@all` successor supplies the exact
6-cycle nonzero not-taken branch and resumes at the original scan increment.
Its terminal-only `01:C5B2@all` successor supplies the exact 6-cycle increment
and N/Z update and resumes at the original range comparison. Its terminal-only
`01:C5B3@all` successor supplies the exact 6-cycle limit comparison and
resumes at the original loop branch. Its terminal-only `01:C5B5@all` successor
supplies the exact 12-cycle taken loop branch and resumes at the shared scan
read. Its terminal-only `01:C5B9@all` successor then supplies the exact
6-cycle `$80` reservation-byte load after the shared scan finds its first
free slot, resuming at the original indexed store. Its terminal-only
`01:C5BB@all` successor supplies the exact 15-cycle indexed reservation store
and resumes at the original actor setup. The later actor setup and shared
setup's terminal-only `01:C5BE@all` successor supplies the exact 12-cycle
cleared `$55` mode-byte load and resumes at the original branch. The branch
path's terminal-only `01:C5C0@all` successor supplies the exact 12-cycle
taken cleared-mode branch and resumes at the original carry-clear return. That
return's terminal-only `01:C5C7@all` successor supplies the exact 6-cycle
carry clear and resumes at the shared return. The shared worker's later
actor/coroutine body remains assembly-backed. After the terminal return,
`02:420E@all` supplies the exact 6-cycle nonnegative `BMI` fall-through and
resumes at the original actor setup. Its terminal-only `02:4210@all` successor
supplies the exact 12-cycle `$2D` pointer-low-byte load and resumes at the
original indexed store. Its terminal-only `02:4212@all` successor supplies the
exact 15-cycle indexed pointer-low-byte store and resumes at the original actor
setup. Its terminal-only `02:4215@all` successor supplies the exact 12-cycle
`$2E` pointer-high-byte load and resumes at the original indexed store.
Its terminal-only `02:4217@all` successor supplies the exact 15-cycle indexed
pointer-high-byte store and resumes at the original actor setup.
Its terminal-only `02:421A@all` successor supplies the exact 12-cycle `$26`
scratch clear and resumes at the original position-field setup.
Its terminal-only `02:421C@all` successor supplies the exact 12-cycle `$2B`
mode load and resumes at the original signed branch.
Its terminal-only `02:421E@all` successor supplies the terminal nonnegative
12-cycle signed branch and resumes at the original position load.
Its terminal-only `02:4222@all` successor supplies the exact 6-cycle carry
clear before the terminal position arithmetic.
Its terminal-only `02:4223@all` successor supplies the exact 15-cycle indexed
base-position load before the original mode-byte add.
Its terminal-only `02:4226@all` successor supplies the exact 12-cycle
mode-byte add before the original actor-field store.
Its terminal-only `02:4228@all` successor supplies the exact 15-cycle indexed
position store before the remaining actor setup.
Its terminal-only `02:422B@all` successor supplies the exact 15-cycle indexed
high-position load before the original scratch-byte add.
Its terminal-only `02:422E@all` successor supplies the exact terminal
12-cycle cleared-scratch high-byte add before the original indexed store.
Its terminal-only `02:4230@all` successor supplies that exact 15-cycle
indexed high-position store. Its terminal-only `02:4233@all` successor then
supplies the exact 12-cycle high-position scratch clear. Its terminal-only
`02:4235@all` successor then supplies the exact 12-cycle high-byte mode load.
Its terminal-only `02:4237@all` successor then supplies the exact 12-cycle
nonnegative signed branch. Its terminal-only `02:423B@all` successor then
supplies the exact 6-cycle carry clear. Its terminal-only `02:423C@all`
successor then supplies the exact 15-cycle indexed high-position base load.
Its terminal-only `02:423F@all` successor then supplies the exact 12-cycle
high-position scratch add. Its terminal-only `02:4241@all` successor then
supplies the exact 15-cycle indexed high-position actor-field store. Its
terminal-only `02:4244@all` successor then supplies the exact 15-cycle indexed
companion actor-field load. Its terminal-only `02:4247@all` successor then
supplies the exact 12-cycle companion-field scratch add. Its terminal-only
`02:4249@all` successor then supplies the exact 15-cycle indexed companion
actor-field store. Its terminal-only `02:424C@all` successor then supplies the
exact 6-cycle companion actor-field accumulator clear before the remaining
setup. Its terminal-only `02:424D@all` successor then supplies the exact
15-cycle indexed companion actor-field zero store. Its terminal-only
`02:4250@all` successor then supplies the exact 15-cycle indexed second
companion actor-field zero store. Its terminal-only `02:4253@all` successor
then supplies the exact 15-cycle indexed actor-attribute load before the
remaining mask and setup. Its terminal-only `02:4256@all` successor then
supplies the exact 6-cycle actor-attribute high-nibble mask. Its terminal-only
`02:4258@all` successor then supplies the exact 6-cycle actor-attribute
mode-bit merge before the remaining setup. Its terminal-only `02:425A@all`
successor then supplies the exact 15-cycle indexed actor-attribute store. Its
terminal-only `02:425D@all` successor then supplies the exact 6-cycle
actor-field constant load before the remaining setup. Its terminal-only
`02:425F@all` successor then supplies the exact 15-cycle indexed actor-field
constant store. Its terminal-only `02:4262@all` successor then supplies the
exact 6-cycle second actor-field constant load before the remaining setup.
`03:7418@all` then supplies the exact 36-cycle surface-release prefix and
hands off to `03:7420@all`; its instruction checkpoint retains the timer IRQ
at that handoff. The terminal scheduler composes the exact 33-cycle
`JSR $C018` banked wrapper with the existing 159-cycle `01:C5C9` scheduler,
then returns at original `01:C5E9`. Score accumulation, the common helper,
post-clear helper body, post-dispatch continuation, and the existing `7BF6` collision-response,
audio, and interrupt-bearing body remain in assembly. The native classifiers
admit only the fixed Knight slots `$3A-$3D`.

All 10,600 natural inactive calls execute natively through the exact
33-cycle `LDA $3800,X` / taken `BPL` / `RTS` transcript. The three instruction
checkpoints preserve 117 timer-interrupt passages and all 1,539 aggregate
handler-owned writes. Focused Release artifacts are census
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`, call
`6d8c143b2d3519b08ab769316c6754d14d7b8217e4c1d31609c76b085ecfb3c1`, and
write `9d7282f06841f7e4dcbd40d6ccfeb507647c85afe351251be26d2c04832f31da`.
The forced frame-1,402 `$383A=$80` route proves the nonterminal prefix with
byte-identical census/call/write artifacts
`116338aeb088606d937f58c7aa04d853c2284e2137c60d323fe0e1125e416890`,
`6bb57b4cbce1bd9dac52677950a2b430513a873df2942774eb43550ea44f8ced`, and
`62b0d5380d9719cd86cc9f6b98b710a64d15bea4fdb88a23b77a6e46ab0f2acd`.
The parent `03:7378` now also executes natively for its bounded waiting
state: its indexed Giant-Door channel must have bit zero clear and the child
collision byte must be nonnegative. The composed 123-cycle transcript retains
the two `JSR $73D8` stack writes, nested return, and all observed timer
interrupt passages. Door release, movement, and collision progression states
remain assembly-owned.

### Composed queued-Knight waiting parent: semantic `03:7378@all`

The new parent adapter is deliberately conservative: it admits only fixed
slots `$3A-$3D` with their valid queue index, a clear `$2D06-$2D09` request
bit, and an idle `$3800,X` collision byte. It therefore composes the prior
idle child without expanding into the Giant-Door release or scoring paths.
All 10,600 natural parent calls execute natively through their exact
123-cycle, two-stack-write transcript. Focused Release census, call, and
write SHA-256 values are respectively
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
`fcc85209b476ba21441502ef04ae63c9511045399c9720cd188dc5de49b68f6c`, and
`a43918c5cb8f86a9a2c458e2dc9ec4687f0eb33b3550be3a1f5592258854c793`;
the required Knight family Debug checkpoint preserves census
`a95d0c343108b107d7ec1aa3c20915540d91bb08817704776531e5696ea9b5e2` and
the same call/write artifacts.

### Fifty-fourth native replacement: semantic `03:65A4@all`

The shared Front- and Side-Spider appearance helper decrements indexed
`$3A00,X`, returns while nonzero, and on expiry reloads eight then increments
`$3980,X`. Appearance zero through two return after the increment; appearance
three resets to zero. The native classifier admits only the fixed Spider slots
`$26-$29` and identifies the wait, advance, and reset paths from copied work
RAM.

All 5,441 natural entries execute natively through exact instruction
checkpoints: 4,689 33-cycle timer waits, 499 102-cycle phase advances, and
163 111-cycle resets. The checkpoint transcript retains all timer-interrupt
passages and the complete 8,167-write artifact. Focused Release census, call,
and write SHA-256 values are
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
`7e987ec3f4faa07710a6d528d2e108ca4b5157cf8dd0f9183c701a1eb3583356`, and
`d7c55c4dde46c9c844669f67e5a3e932d411fa8aac206d6e1d9bc284fccac210`.
The collision-owning `03:6490` and `03:651F` parents are now native on all
natural entries through the composed transcripts documented in their family
sections above.

### Fifty-fifth native replacement: semantic `03:7D85/7E23@all`

The lower-left and upper-right Small-Soldier source controllers each reach a
compact steady gate after their five-child spawn countdown. `03:7D85` loads
left-group count `$2D16`; `03:7E23` loads right-group count `$2D17`. A
nonzero value returns immediately, while zero falls through into the existing
assembly-owned group-completion and regeneration sequences.

The native classifier admits only fixed source slots `$34/$35` at their
matching routine address. All 2,457 left and 2,457 right natural waits execute
through exact 21-cycle absolute-load/clear-branch transcripts, including the
original timer-interrupt passages. The focused Release census, call, and
write SHA-256 values are
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
`15b059014d358901b21d75b349df894df0fc9e981f1f038ded740e622ce779db`, and
`e9918a499097dea8cc28d1315f6e25d6adf6012ece1914b236d26ff33451993f`.
The zero-count group-completion path, child deployment `03:7EFA`, and
collision-owning child dispatcher `03:7F57` remain assembly-backed.

### Fifty-sixth native replacement: semantic `03:67B3@all`

The shared Three-Blobs appearance/collision gate serves fixed slots
`$58-$5A`. It loads the actor-selected global-state index from `$3A80,X`,
loads color state `$2D39,Y`, maps that value through the original six-entry
appearance table at `03:67E6`, stores the resulting appearance at `$3D00,X`,
then tests collision state `$3800,X`. A negative collision state continues
into the existing command, global-state advance, and response path; a
nonnegative state branches directly to `03:67E5`.

The native classifier accepts only the three fixed Blob slots, phase indexes
zero through two, and the table's six valid state values. Semantic
`03:67B3@all` now begins natively for all 1,965 calls. Its 1,950
collision-free calls execute through six exact instruction checkpoints and
one 93-cycle appearance write, including 63 interrupt-bearing calls whose
original handlers remain in the transaction. The 15 collision-active calls
execute the exact 66-cycle color/appearance prefix, then resume original
collision handling at `03:67C0`.
Focused Release census, call, and write SHA-256 values are
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
`60ff27df73bf6021e3375b709c63484962d563d109109229eb89145ad5685ce2`, and
`56203a9aa4d02e29f8755fc9c6eb629c7a234dbe78f812f08e7d0acf19910162`.
`dc_rom_three_blobs_appearance_semantic` also verifies normalized replay
`03:67B3@1` at frame 1,229 with 93 body cycles and one ordered write.

### Fifty-seventh native replacement: semantic `02:570A@all`

The Big-Star active controller reaches `02:570A` after its camera gate. It
loads `$2D04`, services bit zero through `02:5722` and bit one through
`02:5738` when present, then clears the pending byte. The no-pending leaf
therefore consists of two load/mask/taken-branch pairs followed by the byte
clear; unrelated high bits are intentionally ignored by those masks.

The native classifier accepts only the clear-low-bit path. Its one observed
pending-left child `02:5722@all` is now a separately guarded native helper for
fixed slot `$4B` phases three through six: it preserves the exact 240-cycle,
six-write phase-four/`$F0:$FF`/cadence-eight, phase-five/`$E0:$FF`/
cadence-six, and phase-six/`$C0:$FF`/cadence-three selections through the
original `02:576B` RTS. The phase-six saturation repeats that vector through
its separate 252-cycle compare/decrement/branch prefix.
Phases zero through two reset to phase three, select the zero vector/cadence,
and take the shared 252-cycle latch tail. The controlled phase-zero Release
census, entry, call, and ordered-write SHA-256 values are
`3b38afb94a4ed5d13dfcdd850b20e4410a6b744dbd2e5aec398844249d8b6f7a`,
`a5a2099bbf5e8ef56bf4b8cf1525b9f1883d3dace477e85d0c41ea192eba9179`,
`36960817c0b3eaa2c20f080e55a52b2b6b30b1f5c8464eb5d224f6f6e8a90d7c`, and
`d6c35b6cc9f716ecb7503c8048592fcde0e124ab0446e90d72a870d98ee06ed3`.
Its Debug census SHA-256 is
`84638ea265c2f353cf112b9927bc95b5ece558c3e411336a57dcd7b84b839bf7`;
the entry, call, and ordered-write artifacts match Release and assembly.
The separately controlled `$2D18 == 4` latch-clear tail has Release census,
entry, call, and ordered-write SHA-256 values
`58c458bc540bcbf55ecf8f2ffdd970c2910ee536488e20af1cfda78ec4990de5`,
`8390a5dbe39cab0f25302a527cfbef18a0d9548afe414281032a7b881b484717`,
`a4bf428ce345b18a3524a20495bfea9a86e4bfb63de82e496b2bdca14e013b95`, and
`441a6881c91157718df266d7b14ed2dab78d2803645399b1631578ace9680392`.
The independently controlled phase-four replay reaches the helper once at
frame 1,221; its Release census, entry, call, and ordered-write SHA-256 values
are `8c11b31e774141e07f62c6f902161f61b34fc41267d86de44e9827ea0a1ce3ea`,
`76c71dbb478e7e7123d6e49a27e8bd755e1781b1c85899621d4cd09b2d41720b`,
`986de52412dacc8f72247e5bc1fc6f62e1ccb149c03c44bf78de414fb6e02a8b`, and
`b27ddcc4ac9074584102f35abb5e2011f31709169f3a7a98bb6fd0488650f76b`.
Its Debug census SHA-256 is
`700587830ac9cc82e1f19ac5ee54506410241cc87ef1c42c4e92ea629807b5a8`;
the remaining artifacts match Release and assembly.
The independently controlled phase-five and saturating phase-six replays each
reach the helper once at frame 1,221. Their respective Release census, entry,
call, and ordered-write SHA-256 values are
`381bef7a42632deba91fedafaa3ece05bf8a9c440b80a03e3494de4aff26826d`,
`48cec3dee721f7cebda0974c0c1bea959bed537a340bc243a42cd6695d783a1e`,
`f3cc388593aa972f80a3ebb5e0f321676abebde3134ca88eccd7f4aa803c3444`, and
`c6f5398623d579384c7102fed0266fa1b5d7b6b6643823259aea244fc3b42390`;
then `5923ee03453e728c82d7d9289f2e975ea900848142b80bfb6d4ffda6559826ad`,
`d7a704832484e8fc34fe6e1fb1fb191897dc1c2bbcfacabcb149f77a38339ea5`,
`be856f725156f6c1fc09ae5ef84145552a9a5a706ca9790f94e0ccbf6314e2af`, and
`cdaedba22eb308e66248e41aa3e38070543510663b5ea4d48c3dfe38d2fa037f`.
The phase-six Debug census SHA-256 is
`a31bfb402382bea7a77e13c7bac7c66164162d2887589ede746b217a5ecda6e7`;
its entry, call, and ordered-write artifacts match Release and assembly.
Its mirrored pending-right child `02:5738@all` separately preserves fixed
slot `$4B` phases zero through four: phase zero holds with
`$40:$00`/cadence three, phases one through three decrement through their
six-write 228-cycle velocity selections, and phase four resets to three then
takes its distinct 252-cycle latch tail. Other helper phases remain
assembly-owned. Together the gate covers 489 of the 490 controlled active-window calls
through seven instruction checkpoints and the exact 81-cycle, one-write
return. The focused Release census and call artifacts are byte-identical to
assembly with SHA-256
`6110f853aa00423d97598e10ec1a64abe507bb6b8629e6f3768a3f40f0c3c78d` and
`16d8830c2f52b6db5e2902a82b91db22a7fce8662e2729ed6e8075515b23e315`.
`dc_rom_big_star_pending_gate_semantic` shadows ordinal two at frame 1,222
with 81 body cycles and the ordered `$2D04` clear, plus `02:5722@1` at frame
1,221 with 240 body cycles and six ordered writes.

The independently controlled pending-right route injects bit one after the
Big Star enters the active camera window. It reaches `02:5738@1` at frame
1,221 with 228 body cycles and six ordered writes; its Release census, entry,
call, and ordered-write SHA-256 values are respectively
`44dd43af3a76c12707bb308896041f9e851ce696bd7abb9e9961912e2c8d19fe`,
`ce381a9c7dfcf48c76af4a055a91fdf084b3e98477962e450fc8d1a0de5a9b0a`,
`b94e174dd020ebb55aed72ed820f042a07f75874a157caefa00e7afc7173cbb2`,
and `2bf7d841881f963d809fb98cd10bf20431af108f70764fb5c656082f714543b3`.
The required Debug checkpoint has census SHA-256
`72aaeac436120338640996362d268477fac2a2b8fcef5c2a0d629cc516d9b740`;
the other three artifacts match the Release and assembly transcripts.

### Fifty-eighth native replacement: semantic `02:53DD/5456@all`

The two fixed Devil-Shooter side companions return through `02:53DD` (slot
`$0A`) and `02:5456` (slot `$0D`) after their session animation completes.
Each loads session flags `$53`, masks bit three, loops back to its active
initializer only when that bit is set, and otherwise reaches a 24-cycle,
zero-write return.

The native classifier admits only slots `$0A/$0D` with session bit three
clear and a non-transfer CPU state. The adapter publishes exact checkpoints
after the 12-cycle `LDA`, 18-cycle `AND`, and 24-cycle not-taken branch, then
absorbs each leaf's own `RTS`. It therefore replaces all 1,055 left calls and
all 1,085 right calls, including 20 interrupt-bearing variants; active-loop
paths remain assembly-backed.
`dc_rom_devil_shooter_companion_dormant_semantic` preserves the full focused
Release census, call, and ordered-write artifacts with SHA-256
`9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
`ac711b3b5d4781da6841ce2dc4b85583605a83e64e5f682ffece4b2f2183b6e0`, and
`93ae519370adfbceb08bc4b84812d1f7d17ed5e2601f612ade3c21238f1768e6`.
It also shadows ordinal ten at frames 2,145 and 2,141 respectively.
The deployed pair owns 96,300 native master cycles.

### Fifty-ninth native replacement: semantic `02:5397@all`

The direct charge-position leaf for fixed Devil-Shooter companion slot `$0B`
loads `$2D15`, shifts the charge right once, adds `$00A8`, and stores the
resulting low byte to `$3300,X`. It then clears the accumulator, adds the
first addition's carry to `$02`, and stores the high byte at `$3380,X` before
returning through `02:53A7`. The native classifier admits only slot `$0B` and
derives the same low byte, carry, and high byte from the typed work-RAM view.

The adapter retains the original 75-cycle body, stores at instruction
boundaries `02:539E` and `02:53A4` at offsets 48 and 75, and reproduces every
intermediate and final flag state. All eight original instruction boundaries
are checkpoints; the transcript then absorbs `RTS` at `02:53A7`. It declines
only transfer-mode calls and therefore replaces all 1,260 focused Release
calls, including all 47 original interrupt-bearing variants. The extended
`dc_rom_devil_shooter_companion_dormant_semantic` family regression preserves
the same census, complete 5,406-call trace, and all 6,353 ordered writes, and
shadows ordinal ten at frame 949 with 75 body cycles and two ordered writes.
The deployed routine owns 120,960 native master cycles.

### Sixtieth native replacement: semantic `02:5408@all`

The fixed Devil-Shooter charge-appearance companion at slot `$0C` loads the
shared charge counter `$2D15`, compares it with cache `$3E0C`, and returns
unchanged when they match. On a change it writes the new cache, increments
appearance `$398C`, and clears that appearance after its third frame. The
native classifier admits only slot `$0C` and only its naturally bounded
zero-to-two source appearance state.

The adapter preserves the 42-cycle cached return, 105-cycle cache/advance,
and 114-cycle cache/advance/reset transcripts. Its writes remain anchored at
`02:5410`, `02:5413`, and (on reset) `02:541D`, with offsets 51, 72, and 114.
It publishes every retired-instruction boundary on all three paths, including
the indexed increment's temporary N/Z result, and absorbs `RTS` at `02:5420`.
Only transfer mode remains outside the contract. All 1,288 focused Release
calls now execute natively, including all 22 interrupt-bearing variants. The
extended companion regression preserves the same byte-identical artifacts and
shadows ordinal ten at frame 949 with the 42-cycle, zero-write cached path.

### Sixty-first native replacement: semantic `02:545D@all`

The fixed right Devil-Shooter companion at slot `$0D` decrements timer
`$3B0D` and returns while it remains nonzero. On expiry it reloads eight
ticks, increments appearance `$398D`, and clears that appearance when it
reaches five. The native classifier covers the full byte-valued timer and
appearance arithmetic of that direct leaf.

The adapter preserves the 33-cycle decrement/wait, 102-cycle
reload/advance, and 111-cycle reload/advance/reset transcripts. Its writes
remain anchored at `02:545D`, `02:5464`, `02:5467`, and (on reset) `02:5471`,
with offsets 21, 48, 69, and 111. It preserves DEC or final CMP flags and
declines only transfer-mode or wrong-slot calls. Every instruction boundary is
published, so all 267 fixed-slot calls in an isolated assembly/native sweep
execute natively with an identical 267-call/410-write artifact. In the owning
combined regression its native parent leaves one direct call visible, which is
replaced, absorbs `RTS` at `02:5474`, and is shadowed at frame 2,131 with the
33-cycle, one-write timer body. The absorbed return raises deployed ownership
for that call to 54 cycles; composition under `02:5441` deliberately keeps the
return separate because the parent stages and consumes the child JSR frame.

### Left Devil-Shooter companion active timer

The shared left active-timer entry `02:53E4@all` now has a deliberately
context-constrained contract. It accepts only fixed slot `$0A`, decrements
`$3B0A`, reloads it to eight on expiry, advances `$398A`, and clears that
appearance at frame four. Its equivalent 33/102/111-cycle paths return at
`02:53FB`; writes are anchored at `02:53E4`, `02:53EB`, `02:53EE`, and (on
reset) `02:53F8` at offsets 21, 48, 69, and 111.

`53E4` is also a shared code entry for other caller/slot combinations, so the
adapter still declines non-`$0A` and transfer-mode invocations. It no longer
declines at an event or interrupt horizon: every original instruction boundary
is checkpointed. An isolated assembly/native sweep admits 269 of 272 calls,
leaving only three transfer-mode entries, and preserves the complete combined
539-call/820-write timer artifact with right leaf `02:545D`. The owning family
regression pins replay `02:53E4@3` at frame 941 (33 cycles, one ordered write).
Four directly admitted calls absorb `RTS` at `02:53FB`, raising their deployed
ownership by 84 cycles. Three transfer-mode/wrong-context calls still execute
that return in assembly. Composition under `02:53C8` retains the explicit
child-return seam for the same stack-transcript reason as the right parent.

### Left Devil-Shooter active controller

The normal active-controller entry `02:53C8@all` calls the left timer leaf,
tests `$53 & $09`, and returns through `02:53D1` while that session flag stays
set. The native parent composes the child’s 33/102/111-cycle timer bodies with
the authentic `JSR $53E4`, child `RTS`, and controller tail, producing exact
99/168/177-cycle wait/advance/reset transcripts. It emits the JSR stack stores
at `$01FB/$01FA` with values `$53/$CA`, shifts child writes by 21 master
cycles, restores the caller stack, and preserves the child carry/overflow
state while updating the final AND-derived N/Z flags.

The adapter requires fixed slot `$0A`, non-transfer execution, and a nonzero
`$53 & $09` result. It publishes every child and parent boundary and absorbs
the final `RTS` at `02:53D1`, so no complete-body horizon guard remains. It
replaces 265 of 266 controller entries; the sole fallback is the genuine
zero-flag alternate path. Replay `02:53C8@3` pins the frame-945 ordinary wait
at 99 observed body cycles and three ordered writes.

### Right Devil-Shooter active controller

Mirrored entry `02:5441@all` uses the same composition around `02:545D`, but
keeps its right slot `$0D`, five-frame terminal comparison, return instruction
`02:544A`, and JSR stack bytes `$54/$43` at `$01FB/$01FA`. Its exact normal
wait/advance/reset paths are likewise 99/168/177 cycles, preserving the
child’s carry/overflow state and final `$53 & $09` N/Z flags. It admits 174
of 267 controller entries in the old atomic form. With every child and parent
boundary published and the final `RTS` at `02:544A` absorbed, it now admits 266
of 267; the sole fallback is the genuine zero-flag alternate path. Replay
`02:5441@3` fixes the frame-945 ordinary wait at 99 observed body cycles and
three ordered writes.

An independent assembly/native sweep over both parents preserves all 533 calls
and 2,113 writes exactly. Its call/write SHA-256 values are
`6b6d832cc1c55a7044429e9d1f3e03123023fb72931db7fab9ef22ab77628ead` and
`5e087253cfd8a2b57507d316827a40c4d8af45935f2bf39cb61fc38fdb0bc159`.
The combined family regression has 5,229 calls and 5,972 writes with SHA-256
values `ac711b3b5d4781da6841ce2dc4b85583605a83e64e5f682ffece4b2f2183b6e0`
and `93ae519370adfbceb08bc4b84812d1f7d17ed5e2601f612ade3c21238f1768e6`.

### Sixty-second native replacement: semantic `02:5674@all`

The active Big Star at fixed slot `$4B` first invokes the pending-direction
gate `02:570A`. When no direction is pending, its signed angular velocity
`$2D00:$2D01` is nonzero, and decrementing timer `$3A4B` remains nonzero, the
controller takes a compact rotation wait: it clears `$2D04`, decrements the
timer, and clears session bit one in `$20CB`.

The adapter preserves the composed 222-cycle, five-write transcript,
including the nested JSR stack stores, the `571E` latch clear at offset 102,
the `567F` timer store at 180, and the `56A9` session store at 222. It retains
the final AND-derived A/N/Z result and declines transfer-mode, event/IRQ
horizon, stopped, pending-direction, and timer-expiry calls. The Release
active-window regression replaces 199 of 490 controller calls and shadows
ordinal four at frame 1,224; the other 291 controller calls retain assembly,
while their directly observed no-pending child gate replaces 290 of 291 calls.

### Sixty-third native replacement: semantic `03:7428/742B/742E@all`

The Giant Face's fixed form-controller slot `$36` enters two compact scheduler
stages at `03:7428` and `03:742B`, then resolves persisted form at `03:742E`.
The routed game has form zero, so the final entry clears `$2258`, shared
request/display/animation bytes `$2D0C-$2D0E`, and local form state `$3E36`
before scheduling existing request gate `03:74D8`.

`dc_classify_giant_face_controller_initializer` accepts only slot `$36`; its
final stage additionally requires persisted form zero. The adapter preserves
the 192/192/306-cycle, 7/7/12-write stack and scheduler transactions exactly.
Persisted forms one through six retain assembly ownership, including their
state reconstruction and visual-effect helpers. The existing Giant-Face
family regression now replaces and replays all three stages while retaining
the request, animator, crown, and mouth boundaries.

### Sixty-fourth native replacement: semantic `03:7D6F/7E0D@all`

After each Small-Soldier source setup, the left and right delayed states
decrement source timers `$3A34/$3A35`. A nonzero result branches immediately
to `03:7D81/7E1F`; expiry enters the original timer reload and shared `7E87`
child-allocation path.

`dc_classify_small_soldier_delayed_spawn_gate` exposes the two fixed source
slots and their pre/post-decrement timer state. The native adapter preserves
the exact 33-cycle, one-write DEC/BNE transaction and its resulting N/Z flags
for 155 left and 161 right horizon-safe route calls. Expiry, allocation, and
all user or timer interrupt horizons remain assembly-owned. The owning source
regression replays the established child completion before the two new wait
samples.

### Sixty-fifth native replacement: semantic `03:766C@all`

The hidden Giant-Face animator at fixed slot `$37` installs its form state,
appearance, and one-tick timer before scheduling the existing `03:767E`
animator gate. `dc_classify_giant_face_animator_initializer` admits only that
fixed slot. The adapter preserves the exact 255-cycle, ten-write scheduler
transaction; active animation remains assembly-owned.

### Sixty-sixth native replacement: semantic `03:76F7@all`

The fixed Giant-Face crown surface at slot `$39` directly installs the
existing `03:76FA` collision gate. `dc_classify_giant_face_crown_initializer`
admits only that slot, and the adapter preserves the exact 192-cycle,
seven-write stack and scheduler transaction. The crown hit path remains
assembly-owned.

### Sixty-seventh native replacement: semantic `03:7718@all`

The fixed Giant-Face mouth surface at slot `$38` clears its indexed collision
and appearance state, then schedules the existing `03:7726` mouth gate.
`dc_classify_giant_face_mouth_initializer` admits only that slot. The adapter
preserves the exact 243-cycle, nine-write transaction; open-mouth capture and
collision handling remain assembly-owned.

### Sixty-eighth native replacement: semantic `03:61A3@all`

The stopped Undead-Head controller parent at fixed slot `$4D` first confirms
that both session-transition bytes are clear, then reaches the existing
`03:61AB` selector return. `dc_classify_undead_head_waiting_parent` admits
only the stopped state. The adapter composes the exact 63-cycle, zero-write
path with six instruction checkpoints, retaining original interrupt passages.
Session transition and activation behavior remain assembly-owned.

### Sixty-ninth native replacement: semantic `02:5CC5@all`

The paired Side-Worm stopped parents at fixed slots `$4E/$4F` first confirm
that both session-transition bytes are clear, then reach the existing
`02:5CCD` selector return. `dc_classify_side_worm_waiting_parent` admits only
that stopped state. The adapter composes the exact 72-cycle, zero-write path
with six instruction checkpoints, retaining the original interrupt passages.
Session transition and activation behavior remain assembly-owned.

### Seventieth native replacement: semantic `02:4DF7@all`

The Giant-Skeleton Eyeball's fixed slot `$2F` reaches this scheduled opening
timer state only after its completion latch is consumed. The typed classifier
admits timer values above one in opening state two. The adapter reproduces the
exact 33-cycle decrement, one RAM write, and branch-to-RTS path through two
instruction checkpoints. Its expiry branches remain assembly-owned.

### Seventy-first native replacement: semantic `02:4E24@all`

The same fixed Eyeball slot schedules this local steady-state return after
its opening expiry. The classifier requires an unsigned collision byte and a
clear final-animation flag, then the adapter preserves the exact 51-cycle
load/branch/RTS prefix. It deliberately declines the conservative event-
horizon calls and every collision/animation continuation.

### Seventy-second native replacement: semantic `02:4F3D@all`

The final Eyeball animation's ordinary tick decrements its main timer and
nonzero phase, preserving the original `4F75` RTS boundary after 102 cycles
and two writes. The adapter retains assembly for both transition paths and
for every conservative event-horizon call.

### Seventy-third native replacement: semantic `03:76C5@all`

When hidden-form animation mode is active, fixed Giant-Face animator slot
`$37` reaches direct helper `03:76C5`. The helper decrements `$3A37` and,
while it remains nonzero, returns through `03:76CA`. Timer expiry reloads the
timer, advances its phase, and enters original drawing/scheduler work.

`dc_classify_giant_face_animator_active_wait` admits only the fixed slot with
a timer above one. The adapter preserves the exact 27-cycle, one-write
decrement/untaken-branch path: the `$3A37` store occurs at cycle 21 and both
the decrement and branch have instruction checkpoints. It declines every
expiry and both user/video and timer-IRQ horizons at or inside that body.

The controlled active-animation route sees 872 helper entries. Semantic
`03:76C5@all` replaces 639 safe waits; the remaining 233 calls retain the
original phase, drawing, scheduler, or horizon-bound behavior. Release
census, entry, call, and ordered-write SHA-256 values are respectively:

- `a99ac333c3f0dbbde8e2df778e5921482c34b33eb4d790e206fa8d72702ac5c3`
- `061c37ce4746470289e9555f2aa541a1817522efab90389f749565c201897e8d`
- `66f1ff0e6889137519b2989c42497295f5db57d5ca88b8936bec7b6c4b55b9fd`
- `156a20e944e971c7d58d1f2e4e85286a6e39c98a277121a7edbf70728cc62aa3`

Normalized replay `03:76C5@2` returns at frame 1,224 after 27 body cycles
and one ordered write. The required Debug Giant-Face family checkpoint also
passes with census SHA-256
`b56d786a79b31e51e5ffef5ebc40be80f46be13a1d930b11c7372be76a4eca4e`;
its entry, call, and ordered-write artifacts match Release and assembly.

### Seventy-fourth native replacement: semantic `02:5738@all`

Big Star's pending-right helper is a fixed-slot transaction for verified
phases zero through four. Semantic `02:5738@all` retains `$3B4B` at zero
with `$40:$00` velocity and cadence three, advances from three to two with
`$10:$00` and cadence eight, from two to one with `$20:$00` and cadence six,
or from one to zero with `$40:$00` and cadence three, then clears the shared
pending latch. Phase four instead resets `$3B4B`
to three, selects zero velocity/cadence, and follows the `$2D18 > 4` tail to
set the pending latch to one. It preserves the original six ordered writes and
instruction checkpoints: 228 cycles for phases zero through three and 252 for
the verified reset. The `$2D18` reset tail sets the pending latch and returns
through `$5778` unless it equals four; equality clears the latch and returns
through `$576B`. Dedicated controlled Release replays preserve both tails;
the equality tail's census, entry, call, and ordered-write SHA-256 values are
`94233f9cbfa19062effe9c1173b77696ee1a1ccb44108cf8c6646a38bc942526`,
`c18f28b197b65c5571cf102f6cf9874abf30ba20bdd1568587548ec289a267c2`,
`1fd09f08c5444b19030c54a2ebe0c437914b3bcbfbb62dfbfe620795343c0c2d`, and
`a4cba2cef1f238eb05387eacb456f7ae30ff27d7717bf96850c819627dddf712`.

The controlled visible-window route reaches the helper once at frame 1,221.
Release census, entry, call, and ordered-write SHA-256 values are respectively:

- `44dd43af3a76c12707bb308896041f9e851ce696bd7abb9e9961912e2c8d19fe`
- `ce381a9c7dfcf48c76af4a055a91fdf084b3e98477962e450fc8d1a0de5a9b0a`
- `b94e174dd020ebb55aed72ed820f042a07f75874a157caefa00e7afc7173cbb2`
- `2bf7d841881f963d809fb98cd10bf20431af108f70764fb5c656082f714543b3`

The phase-three normalized replay `02:5738@1` returns after 228 body cycles
and six ordered writes. The independently controlled phase-two replay has the
same frame and body shape; its Release census, entry, call, and ordered-write
SHA-256 values are respectively
`8efb90dc878fc3a7c933fd02555cf9f55ee8709f7f7debf06ac55e40376b5c5b`,
`a77687c23267303a947f9128a4b2e1a795910551cab3840682d369a3df940b85`,
`f27a7630fef22eef980fd79ae83f588d4a2c4ba9421656b14da5f048271a249f`, and
`00c32f864c71e2327b706d883716ff88d09f3fb4c16734b02cc90e0233cda58d`.
Its required Debug checkpoint has census SHA-256
`c5c5cec4625fd77625ac37a4f18e336340c5a33b32fb8e1fb0473b185da0b806`;
the entry, call, and ordered-write artifacts match Release and assembly.

The independently controlled phase-one replay has the same 228-cycle,
six-write shape. Its Release census, entry, call, and ordered-write SHA-256
values are respectively
`21cf3e22b95eb07f7f10602f29aa879af209405a0582b6ced272dc068b3424cb`,
`dbd5f344015d8ac237c6b1bc541c0e7d2c26e81d65ef9ad0ad995c9f85a02df8`,
`4f40b108fdcdb3485a7fd40d4c14264be9c77c184c110f2396180ae4a8f75b53`, and
`04a64f0bc710c4fe8ad9ea25bd5c123a498a67b3619a9ab97b4b663a968dc5e5`.
Its Debug census SHA-256 is
`ed64e39a4c427c98052151f3c4f7e0410945cbf0ca809ee2be06d14179633e65`;
the other artifacts match Release and assembly.

The independently controlled phase-zero hold has the same 228-cycle,
six-write shape. Its Release census SHA-256 is
`db9511ef77e930d97f6aebacf0f3b7f4bda68073c232406d7857d6b9f8a0ba8b`;
its entry, call, and ordered-write artifacts are the same as phase one and
match assembly. Its Debug census SHA-256 is
`493a1ee77abf53ab229a95d5ef50ba4067617b72556845324ffd9dfac4c362af`;
the remaining artifacts match Release and assembly.

The independently controlled phase-four reset replay reaches the separate
252-cycle, six-write tail at the same frame. Its Release census, entry, call,
and ordered-write SHA-256 values are respectively
`90676de185f227f6d091522fde78440ffc814499ec140cf46eacf25085fbe857`,
`4cc1c1833829633bb4399cbac23061f20f84c4f50ad1a86b67554526699e8f85`,
`2fee3c80e2d30dc6fd42b93dc22afbe2117ca27362e76acdc3b60ccf4dca5a1d`, and
`15915d885872dc0a9a37b60e8b739e4180b6c89107f92b0966e11f6b122b1061`.
Its required Debug checkpoint has census SHA-256
`3684061b116d8626b85b21bcdc3b550cf61e52fd506bed136cbfcc6c91f5c325`;
the entry, call, and ordered-write artifacts match Release and assembly.

### Seventy-fifth native replacement: semantic `02:5F4A@all`

The Tower-insect source controller begins with a direct countdown leaf.
Semantic `02:5F4A@all` admits fixed slot `$2C` only while `$3A2C` exceeds
one, decrements it at cycle 21, branches at cycle 33, and now absorbs the
`02:5F5B` RTS for a 54-cycle native transcript. Expiry continues into the
original spawn/reload path.

Of 145 natural source entries, the replacement covers 141 non-expiry waits,
including two calls with original interrupt passages reproduced at its two
instruction checkpoints. Three of the four expiry paths return through
`5F5B`; semantic `02:5F5B@all` handles those with a direct stack-driven RTS
transcript and publishes the cycle-21 boundary needed by the event-coincident
third call. Release and Debug census, trace, call, and ordered-write SHA-256
values are respectively:

- `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`
- `926e0960db22ad0d9155e2763441554e5698f847003c58ebee1cb88838b203e4`
- `2f7675e5cbfb81c42af154daf980793e568c3bc6b47d3c7395a3da9646339c26`
- `272538f08055c250b8d15e359440a20d9691b3ed27232b814e1e288604c0b809`

Normalized replay `02:5F4A@2` records the original call boundary at frame 942
after 33 body cycles and one ordered write; the parent then owns the 21-cycle
return. Replay `02:5F5B@1` records the absorbed boundary at frame 941 with zero
body cycles and writes. The complete 144-return slice adds 3,024 native cycles.

### Seventy-sixth native replacement: semantic `02:4C33@all`

The upper-right Side-Star's persisted-form-zero collision schedules this
six-step entrance animation. While appearance `$3980` remains below six, the
fixed slot `$12` decrements `$3A12` and returns after 60 cycles, or reloads it
to four, advances appearance, and returns after 96 cycles. Appearance six
enters the original stage-selection and coroutine path.

Semantic `02:4C33@all` admits the eight nonterminal controlled calls through
four/five exact instruction checkpoints, preserving all one-write decrement
and three-write reload/advance transcripts. The ninth, 309-cycle, ten-write
appearance-six transition falls back to assembly. The focused Release replay
at frame 1,221 has exact 60-cycle, one-write parity, and the complete
controlled census, entry, call, and ordered-write SHA-256 values are
`39fa9e48a8e65cbefb3dfd9fe31e0cff8d483edc536f606400317f9ea5308060`,
`396d1f3a44994bb45d3c0927de09bb7b462fe2d30e40282db7da8da6a272368a`,
`4ea7f8734c42177a479b639a4a53831045561265243aded17040b9118496727d`, and
`fb22429cbfaaf8affe041f21e71b69d40f4e6ca267832dabc10e33e406ea08cc`.

### Seventy-seventh native replacement: semantic `02:5D4A@all`

The controlled left Side-Worm activation reaches this direct coroutine setup
once, before the pointer-driven movement/collision helper begins. Fixed slot
`$4E` saves the `$5D4C` JSR return, transfers through `01:C018` to the shared
coroutine scheduler, and installs continuation `$5D4D` in 192 cycles with
seven ordered writes.

Semantic `02:5D4A@all` composes that exact stack frame with the existing
scheduler replacement. The normalized frame-1,997 replay has exact 192-cycle,
seven-write parity; its complete controlled artifacts are census
`cc2e26bb3c446937072eb8d963dd702c803c908fe17e9be493c737cc4a375020`, entry
`5f6cefded416003d5fc9da1549745780dd1b923405ee04c74378dff506ba9ac1`, call
`aae7744f8d8726aea57f42548a72123d8bb5cb527083d71a319d2393f0a74d05`, and
ordered write `f64e6c64b41606dc771a2a0f15cda3b92d398501b5979c9f78267bd8f4750e1e`.
The scheduled `02:5D4D` continuation has no isolated replacement boundary:
its apparent 69-cycle timer turns still invoke shared `01:C07E`, while its
other turns enter `02:5DFC`. The later motion/collision helper and this
continuation remain assembly-owned.

The ninth, appearance-six Side-Star entrance call at `02:4C33` likewise
remains assembly-owned. Its superficially compact `01:C018` handoff reaches
`01:C5C9` only after 150 cycles with changed input registers, then crosses a
1,926-cycle mixed controller/scheduler scope; it is not the direct coroutine
scheduler composition used by the ordinary entrance ticks.

### Giant Door collision injection boundary

The Giant Door's initialized `$384C=$10` field is consumed by the collision
pipeline, not retained as a software hit request. Release probes that wrote
`$80` and selector-preserving `$90` around the frame-1,229 `03:62B8` call
both reached the gate with `$10` restored and took its 27-cycle idle return.
The sign-bit branch must be recorded from a real collision delivery (or a
later collision-pipeline injection point) before a native score/counter
prefix can be considered. The existing native idle gate is unchanged.

### Seventy-eighth native replacement: semantic `02:4CFB@all`

Either Giant-Face helmet's controlled collision path at `02:4C91` performs the
score/audio and wake-request work before jumping to `02:4CFB`. That post-score
continuation reloads active helmet slot `$2A` or `$2B`'s timer, calls the
original ball-restore helper to write slot-0 position `$0086,$0124` or
`$009E,$0124`, sets appearance four, and schedules `$4D0B` through
`01:C018/C5C9`.

Semantic `02:4CFB@all` admits the captured left- and right-channel post-score
states and composes the two original JSR stack frames with the native coroutine
scheduler. It preserves all 15 ordered writes and the 396-cycle body before
returning at `01:C5E9`. The controlled parent remains assembly-owned: its
preceding 1,000-point score, audio, and Giant-Face wake request take 3,171
cycles and 76 writes. Focused Release replays `02:4CFB@1` for both channels
return at frame 1,222 with exact 396-cycle, 15-write parity; the existing normal
helmet family regression continues to pass.

### Seventy-ninth native replacement: semantic `02:4D0B@all`

The installed `02:4D0B` follow-up is separately native for slots `$2A/$2B`
while global appearance is below six. Its exact 60-cycle form decrements the
helmet timer at cycle 48; its 96-cycle expiry reloads it to four at cycle 75
and advances appearance at cycle 96. The left collision route admits 92 of 93
calls, retaining the one appearance-six animation transition in assembly; the
right route admits all 353 observed calls. Both forms preserve their original
instruction-boundary interrupt checkpoints and return through `02:4D1F`.

The retained left phase-six fallback transfers at `02:4D20`, not to another
timer leaf. Its frame-1,614 scope takes 18,474 cycles and 296 writes through
original drawing, scheduler, actor, and animation paths (18,507 cycles for
the enclosing `02:4D0B` entry), so it remains assembly-backed.

The documented Mercy-Star activation route was separately re-screened from
last-ball state. All three `02:4FC1` gates immediately enter 471/1,128/471-
cycle mixed actor/presentation/scheduler transactions, while the later
`02:50C1` continuation is a 5,625-cycle, 133-write state-animation scope.
The dormant `02:4FC1` return remains the bounded native contract; activation,
award, transport, and safety behavior continue through the original ROM.

The Giant-Face mouth's independent open-state screen sets only `$2D21=1` at
frame 1,200. At frame 1,213, `03:7726` runs a 27-cycle prefix then transfers
to `03:7749`; the resulting 1,122-cycle, 40-write capture/score/actor-script
transaction schedules its continuation through `01:C5CA/C5CD/C5D9/C5E1/C5E6`.
Its `01:C5C9` descendant begins 990 cycles after the gate and lasts 1,395
cycles with 27 writes. The state alone does not reach `03:7821/7834`, so the
later RNG-release path is not conflated with capture. Open-mouth states remain
assembly-owned; semantic `03:7726@all` continues to replace only its bounded
closed/no-hit form.

The complementary closed-mouth collision screen sets only slot `$38`'s
collision byte `$3838=$80`. The frame-1,213 gate reaches `03:7730` after its
42-cycle prefix; its hit continuation takes 15,414 cycles and writes 236
bytes across the shared 300-point score/impact/audio path and actor/display
work (15,456 cycles for enclosing `03:7726`). A later `01:C5C9` entry begins
64,023 cycles after the gate and adds a 1,896-cycle, 36-write scheduler scope,
with repeated subsequent passes continuing animation. The hit state remains
assembly-owned and is intentionally separate from semantic `03:7726`'s
48-cycle closed/no-hit replacement.

The Giant-Face controller's complementary wake-state screen sets only
`$2D0C.0` at frame 1,200. Fixed slot `$36` enters `03:74D8/74E0` for a
261-cycle, nine-write prefix that clears `$1E36`, sets `$1A36` to mode one,
and installs `03:74EA` through the scheduler. The first active `03:767E`
call arrives at frame 1,222 (282 cycles, ten writes), followed by a
3,675-cycle, 77-write active-animation/drawing call at `03:76C5`. A guarded
exact-write trial of the apparent initializer diverges on the request frame,
so its scheduler transfer is not a safe isolated replacement. Wake and later
transformation states remain assembly-owned; semantic `03:74D8@all` keeps
only its request-free 27-cycle native form.

The natural Dragon-Egg hatchling in dynamic slot `$64` provides a separate
five-call screen. Its four ordinary `02:441E` updates take 1,164--1,986 cycles
and 22--38 writes through the shared physical-bank collision/drawing helper;
the fifth takes 19,965 cycles with 384 writes across collision, the
20,000-point score, impact allocation, and final release through `01:C594`.
No state-derived timer leaf survives outside those mixed paths, so the
hatchling controller remains assembly-owned after the existing Dragon-Egg hit
and post-allocation prefixes.

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

## Giant Skeleton steady controller

`02:5EEC-5F21` is one fixed-slot `$30` controller. It calls the direction
classifier through `01:C06F -> 01:DAF8`, clamps the returned direction to the
range used by five appearance values, halves it, and stores the result at
`$39B0`. It then tests collision state `$3830`. A nonnegative value returns at
`02:5F21`; a negative value clears the latch and enters the repeatable 40-unit
score, command `$2E`, bonus, and presentation helpers at `02:5F06`.

The 3,600-frame route enters 2,139 times: 2,138 idle returns and one hit at
frame 2,642. Semantic `02:5EEC@all` composes the already-native direction
helper with its real stack frame, absorbs each idle return, and hands the hit
back at `02:5F06`. The complete 4,283-call/50,711-write Dragon/Giant-Skeleton
artifact, canonical work RAM, entry trace, and actor identity remain
byte-identical in Release and Debug. Regression:
`dc_rom_dragon_giant_skeleton`.

## Bank-01 arithmetic leaves

Three routines in physical bank 01 that share no caller, no subsystem and no
data, and are grouped here because they are the bank's pure-arithmetic leaves:
each is straight-line integer work over work RAM and zero page, ends in an
`RTS`, and makes no call, touches no device register and changes no bank. All
three are instruction-boundary transcripts, admitted on every call.

| Address | Calls | Native cycles | Caller | What it computes |
| --- | ---: | ---: | --- | --- |
| `01:C8F3` | 2,340 | 1,507,932 | `01:C765` | Path-length accumulator |
| `01:DAF8` | 2,139 | 1,450,425 | `02:5EEC` via `$C06F` | Direction octant |
| `01:C969` | 2,993 | 107,748 | `01:C75E` | Range-squared guard |

`01:C8F3` reads the actor's two signed sixteen-bit per-frame velocity
components — `$3400,X`/`$3480,X` horizontal and `$3500,X`/`$3580,X` vertical —
negates each in place if its high byte is negative, adds the two magnitudes,
and shifts the sum left four times into a twenty-four-bit intermediate across
`$2F`, `$30` and `$2D`. That is added to the running total held little-endian
in `$3E00,X`, `$3A00,X` and `$3980,X`, whose top byte saturates: at 4 or more,
`01:C965` zeroes it. X is slot zero on every call this route makes.

`01:DAF8` is the Giant Skeleton's direction lookup, reached from `02:5EEC`
through the fixed-bank trampoline at logical `$C06F`. It subtracts actor X's
position from actor Y's on both axes — `$3180`/`$3200` horizontal, `$3300`/
`$3380` vertical — saturating each signed difference to a byte in `$43` and
`$42` and recording the two signs in bits 1 and 0 of `$45`. It orders the pair
so `$43` holds the larger, noting the swap as bit 2, then splits the octant
three ways: 3x the smaller against 2x the larger sets bit 4, and 12x the
smaller against the same sets bit 3. The five bits index a thirty-two-entry
table at `$DB9E` in the bank's own ROM page. Two exits: 1,459 calls return
through `01:DB9D` and 680 through `01:DB81`.

`01:C969` guards the range-squared helper on the sixteen-bit counter at
`$2D00`/`$2D01`. That counter is zero on all 2,993 entries this route makes, so
the body from `01:C976` — which calls `00:E5EC` twice, once per axis, and sums
the results — never runs. The transcript covers the three-instruction guard and stops in
front of the `RTS` at `01:C971`; the taken arm at `01:C972` is handed back to
the core rather than transcribed from a path the route has never taken.

Shared state was measured on exact call windows from observers on all three
heads. Of the 358 instructions that write any address these routines read,
exactly four ever write inside a window, and all four are `01:C8F3`'s own
stores at `01:C94E`, `01:C956`, `01:C95E` and `01:C965`. There is no foreign
writer and no callee. Regression: `dc_rom_actor_geometry_semantic`.

What remains emulated in the three is 158,955 master cycles, of which 158,472
is the four `RTS` instructions the transcripts stop in front of.

## Collision-table interpreter, instruction by instruction

`03:6966` is documented above by its call sites and its returns. This section
records what transcribing it established about the routine itself, which the
earlier result-computing replacement did not have to know.

**The selector and the phase.** `A` is doubled and indexes the sixteen-bit
pointer array at `$6A38`; the pointer lands in `$2F`/`$30` and names the
actor's table. `$3880,X` is then loaded into Y and its sign decides the shape
of the call. With bit 7 set — which is every call but 27 on the census route —
the low seven bits are a phase counter. `03:697A` toggles bit 0 of it, and the
`PHP`/`PLP` pair around the store at `03:697F` is there so the toggle's zero
flag survives the store: a phase that came back to zero returns immediately
through `03:6988` with carry clear, and a phase that did not falls through to
the step. So a path advances on one call in two. With bit 7 clear, Y is a
repeat count and `03:6989` runs the step that many times through `JSR $6995`,
stopping early if a step reports contact. That loop executes ten times on the
whole route.

**The two cursors.** `$3E00,X` indexes the table and `$3E80,X` indexes the
motion record the current table entry names. A table byte of `$FF` is the
terminator: the routine sets carry and returns through `03:69A1`, which is what
the caller reads as "this path is finished". `$FE` is a rewind — both cursors
are zeroed and the first entry is re-read, which is how a looping path repeats.
Bit 7 of a table entry marks the record as reversed and is stashed in bit 0 of
`$35`, the only use of zero page `$35` in the routine.

**The record.** The low seven bits of the table entry index a second
sixteen-bit array at `$6C3A`, giving the record pointer in `$2D`/`$2E`. A
record byte of `$C0` is that record's terminator: `03:69CB` advances the table
cursor, zeroes the record cursor and branches back to `03:6995` to start over
on the next entry, which is the routine's only loop outside the repeat count.
A byte with bit 7 set carries a sprite frame in its low seven bits — stored to
`$3A80,X`, with `$3D00,X`'s bit 7 set or cleared from the reverse flag in `$35`
— and the step itself is the byte after it. Bits 6 and 5 of the step byte are
the horizontal component and bits 4 and 3 the vertical, each pair meaning "one
unit positive", "one unit negative" or "none", applied to the sixteen-bit pairs
`$3180,X`/`$3200,X` and `$3300,X`/`$3380,X` with an explicit borrow.

Every read is work RAM, zero page or this bank's own ROM through `$2F`/`$30`
and `$2D`/`$2E`. There is no device register, no call out of the bank and no
bank change, which is what makes the whole routine transcribable. Verification
is a two-run write diff over both position pairs, both cursors, the frame and
mirror bytes, the two zero-page pointers and the stack page: 6,563,673 writes,
byte-identical hooked and unhooked. Regression:
`dc_rom_collision_table_semantic`, and `dc_rom_collision_consumer_semantic` for
the composition at `03:7F57` that depends on it.

## Exported bank-7 mode gate: semantic `07:4003@all`

Physical bank 7 exports a per-frame gate at `07:4003`, a `JMP $4634`. The
body compares mode `$2D51` with 9, accepts range byte `$2D4F` only from `$3C`
through `$45`, requires persisted form `$2258=6`, and tests latch `$CB.0`.
The active clear-latch case sets counter `$CA` to 100, raises session flag
`$53.2`, sets `$CB.0`, and returns. An already-set latch returns unchanged;
all mode/range/form rejects clear `$CB.0` before returning.

The canonical route enters the export 2,649 times at frames 941-3,600. Every
call takes the 75-cycle inactive-mode path; periodic interrupts expand 57 call
scopes to 720-1,086 cycles while leaving the routine's own instruction timing
unchanged. The complete trace contains 3,602 ordered writes. Controlled
frame-1,200 fixtures cover the two range rejects (102/114 cycles), form reject
(141), already-armed return (135), and clean one-shot arm (219 cycles, three
writes). All six fixtures replay natively from authoritative assembly corpus
states.

The native adapter retains the export jump, every instruction sampling
boundary, exact N/Z/C results, all zero-page writes, and the live-stack return
at `07:466B`. Release assembly/native artifacts reproduce canonical census
SHA-256 `9036a9de0c3c9917f47bff8c04c136a4824ec12601fb457d260aa2dfe4443490`,
call SHA-256 `799723c75cca1ad3860a1f82a28fbf73781691f3796857ff74145cc3916a9dee`,
and ordered-write SHA-256
`c29f5be6dc7c7196fb3e855c26ff85fe21c7ccc24cfccfb42eaa8547fa0e2b8d`.
