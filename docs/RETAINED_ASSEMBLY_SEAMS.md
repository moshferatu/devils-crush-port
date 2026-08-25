# Retained Assembly Seams

Some code is left emulated on purpose. These are not gaps in the port and not
work items — each one is a place where a native replacement would be *less*
faithful than the emulated instruction, or where the bridge contract cannot
express the transaction. They were repeatedly re-proposed and re-rejected while
they lived in the `Blockers` section of `PROGRESS.md`, so they are collected
here instead.

Before proposing a replacement that touches any address below, read the reason.
If the reason no longer holds, say why in the slice, and change the entry.

## Compound-sampled `BBRn` boundaries

`00:F423`, `00:FA67`, `01:C6E3`, and `01:C756` are HuC6280 `BBRn` instructions
whose zero-page read and relative branch are sampled together. Collapsing the
pair into one native checkpoint shifts VDC event handling.

The core therefore executes each of these instructions itself and re-enters
native code on both proven arms — for `00:FA67`, all 832 instances, resuming at
`FA6A` or `FA6E`. The outer `00:FAA5` branch is retained as the row-transaction
boundary for the same reason.

This is a deliberate hardware-timing seam, not missing semantic coverage.
Moving `01:C6E3` or `01:C756` would require a bridge checkpoint that preserves
the core's four-step event sampling; an ordinary two-boundary transcript is not
sufficient, and it is not needed to keep porting adjacent gameplay code.

## The boot work-RAM clear at `08:585A`

One `TII` emits 4,095 ordered writes against a 512-write transaction channel,
and offers no interior instruction boundary to split on. The code on both sides
of it is native.

Moving the transfer itself needs a compact block-transfer transaction primitive
or a wider bridge contract — not another instruction-checkpoint transcript.

## Timer-horizon fallbacks in audio-visible routines

Two routines fall back to assembly when a serviceable timer IRQ is close
enough that native execution would move a PSG write. Both were found by the
`twist` route's audio-only drift (see
[ROUTE_SCREENING.md](ROUTE_SCREENING.md)).

| Routine | Guard | Effect |
| --- | --- | --- |
| `02:570A` (Big-Star idle) | Serviceable timer-IRQ horizon within its 81-cycle body | 280 no-interrupt calls native, 10 interrupt-bearing calls to the core |
| `03:66A3` (Three-Blobs first-movement prefix) | Timer horizon of 300 master cycles or less | 998 of 1,107 calls native, 109 to the core |

Do not widen either admission into its timer-sensitive path without an exact
device-write transcript. With the guards in place the deployed 3,600-frame
recording is byte-identical to assembly for audio, work RAM, and video.

## Write-capacity boundaries

The bridge carries 512 ordered writes per transaction; the checkpoint capacity
is separate and is 2,048 for a bounded parent.

- `01:D466` has two whole-parent fallbacks. Their current-slot paths enter
  `D525` at frames 1,820 and 2,255 and emit 823 and 868 writes. These belong to
  the `D525` decomposition boundary, not to an unresolved `D466` branch. The
  bounded `D525-D594` prefix is already native; resolving the two parents means
  decomposing the remaining callee-owned segments below 512 ordered writes, not
  widening that prefix.

## Interrupt-bearing routines needing their own checkpoints

Faithful checkpointed mid-routine interrupt delivery exists and is proven by
all 540 `01:D081` calls, so interrupts are no longer a blanket blocker. Each
routine still needs an exact instruction-boundary transcript for the semantic
branch being implemented.

Still emulated for want of one:

- `03:6966@11` (enters via `03:69A2` / `00:FC26`) and `03:6966@13` (shifts
  interrupt-owned stores from about `03:69B7`).
- `03:6177`, the Undead-Head launch consumer. It is dynamically bounded through
  fixed slot `$4D`, but its 5,778-cycle combined-route scope contains a
  periodic interrupt: RNG-owned writes are deterministic while normalized
  replay still detects shifted interrupt-owned writes. Keep it out of
  replacement coverage until its native semantic path supplies checkpoints, or
  a separately proven no-interrupt invocation is reproduced.
- `00:E807`, `00:E813`, and `00:E825`, the helpers adjacent to the signed
  multiply wrapper. No deterministic route enters them; they remain emulated
  until a natural caller is reproduced. Their neighbour `00:E85D` was in the
  same position until a player-session capture reached it — see
  [ROUTE_SCREENING.md](ROUTE_SCREENING.md).

Already resolved, and recorded here because the pattern recurs: `01:DD09@52`
showed that interrupt-owned stack and controller writes can enter through
`00:E058/E169` even when a natural trace never visits the catalogued
`FCxx/FDxx` handler range. Semantic `00:E905@all` and its `01:DD09@all` parent
supply those checkpoints, so every observed damping call is native.
`01:D09E` (all 540 calls) and `00:E65C` (all 3,700 calls) are likewise fully
native, with their wide observed cycle ranges explained by original interrupt
handlers retained through composed checkpoints.

## Defensive fallbacks that are meant to fire

These are not faults. Routes reproduce them deterministically in both
configurations, and the resulting recording stays byte-identical to a fully
emulated run.

- `00:E241 -> 00:E250` and `00:E255 -> 00:E25F`: the banked-call epilogue
  abandons on an MPR mismatch and resumes in the core. Keep them covered.

## The screening horizon

The canonical 3,600-frame route has no further standalone table-rule entry and
return contract to admit. Its filtered residue — 3,228,057 unregistered
non-wait cycles in 238 clusters as of the 2026-08-22 re-rank — consists only of
the device, block-transfer, compound-branch, setup/scheduler, music, and
caller-owned continuation seams above. The remaining bank-`02`/`03` table rows
in the profiled tail are all already-classified actor-loader, scheduler,
collision, appearance, allocator, or caller-return interiors.

This is a prioritization horizon, not an external blocker, and manufacturing a
seam by splitting a known owner around its child or its return is the wrong
answer to it. Resolve it with new *evidence*: a player-session capture, or a
natural route whose profile leaves the discovery union. See
[ROUTE_SCREENING.md](ROUTE_SCREENING.md).

## Cabinet-side deferrals

- The DMD P1/P2 discriminator is deferred at the user's request. When it
  resumes it still needs a two-player route that distinguishes the actual
  player-count and current-player fields from `$2251`'s already-proven
  score-array selection role.
- `$F768` selectors 1-4 remain a future scene-mapping question. No unresolved
  sprite-pattern source blocks normal-table fixed-actor composition.
- The longer state route has a configuration-specific Beetle framebuffer
  variance from frame 2,147, and the maximum-score route has a second
  presentation-only variance at frames 1,555-1,680. In both cases work RAM,
  audio, ordered writes, and semantic traces are identical, so
  configuration-specific census hashes guard them. Each must be explained or
  normalized before that route can become a single cross-configuration
  framebuffer golden.
