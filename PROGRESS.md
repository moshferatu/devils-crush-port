# Project Progress

Last updated: 2026-08-25

This file is the short answer to "where is the port right now". It is kept
small on purpose. Detail lives in `docs/`:

| Question | Document |
| --- | --- |
| Architecture and delivery plan | [PLAN.md](PLAN.md) |
| What the port has bounded, replaced, and proven | [docs/PORT_HISTORY.md](docs/PORT_HISTORY.md) |
| Which routines are mapped, and how | [docs/ROUTINE_MAP.md](docs/ROUTINE_MAP.md) |
| Which routines are admitted natively | [docs/NATIVE_REGISTRY.md](docs/NATIVE_REGISTRY.md) |
| Which natural routes are already screened | [docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md) |
| Which seams stay emulated, and why | [docs/RETAINED_ASSEMBLY_SEAMS.md](docs/RETAINED_ASSEMBLY_SEAMS.md) |
| How to validate a slice | [docs/NATIVE_PORT_WORKFLOW.md](docs/NATIVE_PORT_WORKFLOW.md) |
| Where to profile the next candidate | [docs/NATIVE_FALLBACK_HOTSPOTS.md](docs/NATIVE_FALLBACK_HOTSPOTS.md) |
| What removing Beetle actually requires | [docs/EMULATOR_ELIMINATION.md](docs/EMULATOR_ELIMINATION.md) |
| The full historical log through 2026-08-23 | [docs/PROGRESS_ARCHIVE.md](docs/PROGRESS_ARCHIVE.md) |

## Milestones

| Milestone | Status | Completion gate |
| --- | --- | --- |
| 1. Emulated Windows baseline | Complete | Verified ROM runs through the pinned Beetle PCE fork with SDL3 video, audio, input, saves, and sharp monitor-resolution scaling |
| 2. Instrumentation and hook bridge | Complete | Required game state is mapped and original routines can be observed, shadow-tested, and replaced |
| 3. Cabinet preview | In progress | Three-window cabinet mode shows the stitched playfield, two-tier status DMD, native animated-logo backglass, assigned displays, and playfield-hosted bonus stages |
| 4. Native gameplay port | In progress | Physics, collisions, RNG, scoring, table rules, bonus stages, and session transitions execute natively |
| 5. Final cabinet world model | Not started | Desktop and cabinet profiles share the same native gameplay state without divergence |
| 6. Release hardening | Not started | Tested, documented, ROM-free Windows x64 package is ready for release |

Status values are `Not started`, `In progress`, `Blocked`, and `Complete`.

## Current status

**Milestone 4, native gameplay port.**

The last generated registry measurement (2026-08-22, 593 admissions) runs
**699,919,959 of 1,292,371,731 master cycles natively on the canonical
3,600-frame route — 54.16% of the route, 98.51% of everything outside the
frame-sync wait** — with 475 routines entered, zero interrupt-horizon declines
and zero abandoned transcripts, against a state census byte-identical to the
assembly-only baseline. The `00:E85D` square root, `05:6D27` live orbit arm,
`00:E5BC` multiply partial, and `02:4FC4` side-gate deadline check were
admitted after that measurement, so the current registry holds 597 entries;
regenerate [docs/NATIVE_REGISTRY.md](docs/NATIVE_REGISTRY.md)
with the next canonical census.

Milestone 3 cabinet composition has no external blocker. The accepted
background is rows 20-687 of the packed 256-by-768 envelope; camera-visited
fixed mechanisms retain decoded pattern pixels independently of later regional
VRAM changes. Ball, flipper, and dynamic-enemy presentation are closed. What
remains is display assignment polish and the deferred DMD discriminator.

### The current constraint on Milestone 4

Scripted routes are exhausted. The canonical route plus 30 input-only cadences
have no further standalone table-rule entry-and-return contract to admit, and
the residue is documented device, block-transfer, compound-branch,
setup/scheduler, music, and caller-owned continuation seams. Manufacturing a
seam by splitting a known owner around its child or return is not an acceptable
answer — see [docs/RETAINED_ASSEMBLY_SEAMS.md](docs/RETAINED_ASSEMBLY_SEAMS.md).

The way through is **player-session capture**. `--capture-session` /
`--replay-session` record a real session with per-frame work-RAM fingerprints
and replay it deterministically. `player-session-4` reached `00:E85D-E8B2`,
which no scripted route had ever entered, and produced the most recent
admission. Prefer a fresh capture with the current executable over another
scripted cadence. See [docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md).

## Active work

**Owner:** Codex.
**Slice:** screen the replay-verified `player-session-6` residue, preserving
the dynamically dispatched actor-loop and queue boundaries.

The last slice closed the unsigned square root at `00:E85D-E8A4`: a call-free,
eight-pass restoring leaf isolated from 665 natural calls in `player-session-4`,
all with contract `01:C9D3 -> 00:E85D -> 00:E8A3`. Across every call its
reconstructed `$30:$2F` operands (2 through 11,629) produce exactly
`floor(sqrt(operand))`. It is registry-admitted, driven by a portable CMake
fixture across all 65,536 unsigned operands, and the default registry preserves
all 6,605 per-frame work-RAM hashes with the calls native.

`player-session-5` was a useful initial screen but no longer replays through
the current host. The fresh cabinet-mode v3 `player-session-6` restores the
certification boundary: all 31,971 work-RAM frame hashes and its complete
8,799-address profile replay through the default registry. Its 1,744 calls to
the short actor scan at `01:C539` total 117,359,643 cycles and 2,379,778
writes. They retain the same dynamic-handler and queue-filing interior, so the
whole scope remains assembly-backed; the current trace is recorded in
[docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md).

The next distinct residue, `01:C969`, also failed admission. Its 33,498
session-6 calls take three exits and, on the active path, compose five arithmetic
helpers. A minimal prefix transcript matched focused original calls but changed
the deployed replay at frame 4,888, so the expansion was rejected and the
source was restored before continuing.

`05:6D27` is now admitted from the same capture: the bonus-stage-4 orbit child
has one caller (`05:C565`) and normal return (`05:C568`), with 7,719 live arms
that each take 450 master cycles and stage ten ordered writes. Nine calls join
the outer controller at `05:6D7F`, so they fall back before staging work. The
full default registry, including the new arm, preserves all 31,971 session-6
work-RAM hashes; the owning Release BS-4 semantic regression also passes.

`00:E5BC` is also admitted: it is the eight-bit partial loop inside the unsigned
word multiply, entered 2,423 times only from `00:E5AF` and returning at
`00:E5EB`. Its 600 + 72 x set-bit master-cycle model stages all `$42`, `$43`,
and `$30` shifts plus each ordered add; the production fixture covers every
byte multiplier pattern over a 16-bit operand grid and rejects decimal/T mode.
The deployed session-6 registry again preserves all 31,971 hashes and adds
2,000,280 native master cycles.

Next: screen the next independently bounded child from the remaining session-6
residue, without splitting the composite actor owner around its dynamic
dispatch, queue, or arithmetic-call seams.

## Next actions

- [ ] Screen the replay-verified `player-session-6` residue for an unobserved
      table-rule transaction with an independent caller-and-return contract.
      Keep every known scheduler, collision, fixed-Bumper, actor-loader,
      appearance, allocator, and caller-return continuation assembly-backed.
- [ ] Continue screening the remaining unregistered residue below the
      documented hardware, capacity, compound-branch, scheduler, and
      music-resumption seams.
- [ ] Finish display assignment and playfield composition. Everything except
      the last item is done; see the archive for the closed checklist.
  - [ ] **Deferred at user request:** map the two-player/current-player RAM
        contract and add the P1/P2 discriminator to the DMD.
- [ ] Continue porting gameplay routines behind the transactional replacement
      boundary, one owning family at a time.
- [ ] **Milestone 4 deferred:** reproduce additional normal-table and
      bonus-stage rule-family variants as their routines are ported.

## Blockers

None external. Every constraint that used to be listed here is a deliberate,
documented seam rather than an open problem; they are collected in
[docs/RETAINED_ASSEMBLY_SEAMS.md](docs/RETAINED_ASSEMBLY_SEAMS.md).

## Decisions locked for V1

- Windows 10/11 x64.
- Public GPL-3.0-or-later source with a user-supplied ROM; no ROM or extracted
  assets in releases.
- Exact USA ROM only.
- Beetle PCE remains as the hardware compatibility layer.
- Original gameplay behavior is the faithful default.
- Gameplay logic is ported incrementally behind a C ABI, using C for the first
  gameplay module and C++ for the host/transaction adapter.
- Sharp pixel scaling is always enabled; no alternative filters.
- Desktop mode retains original scrolling.
- Cabinet mode uses the full stitched table on the playfield monitor, bonus
  stages on the playfield monitor in place of the table, score on the DMD
  monitor, and a native animated Devil's Crush logo on the backglass, with a
  user-selected static image as the failure fallback.
- Initial cabinet input supports keyboard/controllers and semantic feedback
  hooks; physical DMD, DOF, and SSF support are deferred.

## Updating this file

Keep it short. When a slice lands:

1. Update the milestone status and the current-status numbers.
2. Replace `Active work` with the new slice — do not append to it.
3. Add or tick a `Next actions` item; delete items that are done.
4. Put the slice's evidence, hashes, and reasoning in the commit message and in
   the owning `docs/` file, not here.
5. Update the date at the top.

A new deliberate seam goes in
[docs/RETAINED_ASSEMBLY_SEAMS.md](docs/RETAINED_ASSEMBLY_SEAMS.md); a screened
route goes in [docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md).
