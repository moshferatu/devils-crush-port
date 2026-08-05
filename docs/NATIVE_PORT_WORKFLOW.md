# Native Port Iteration Workflow

This workflow keeps routine replacement faithful while making the normal
iteration loop fast enough to port related code as a unit. It applies to
Milestone 4 native gameplay work; it does not weaken the release or milestone
acceptance criteria in [PLAN.md](../PLAN.md).

## Validation tiers

### Per slice

Run these after every bounded native change:

1. Incrementally build the affected Release targets.
2. Run the ROM-free core tests.
3. Run one Release ROM regression for the owning routine family. Extend that
   family regression with the new replacement count, normalized replay, and
   relevant artifact assertions instead of creating another full-route test.
4. Run `git diff --check` and inspect resulting artifact hashes before
   recording them.

Release is the normal behavioral gate because deterministic state-census
routes execute substantially faster there. The tested native adapter must
still preserve the original instruction checkpoints, registers, timing, RAM
writes, and interrupt hand-off.

### Family checkpoint

Run Debug build/core coverage and the owning Debug ROM regression after a
related batch, before declaring that family complete, and before a milestone
or hand-off checkpoint. A related batch normally contains two to five sibling
routines that share a route, actor family, or state machine. Run Debug sooner
when a change affects transactional invocation, interrupt delivery, shared
memory helpers, undefined-behavior-sensitive code, or a previously unseen
branch.

The Debug ROM route is an audit, not a second required execution of every
single narrow slice. Do not run an additional focused Debug route when the
family regression already proves the same replacement count and artifact.

### Broad acceptance

Run all relevant Debug and Release ROM-backed regressions at milestone,
release, CI, or deliberate cross-family integration checkpoints. Use the full
suite when changing shared runtime, trace, checkpoint, save-state, or ROM-load
behavior.

## Regression design

- Prefer one family CTest invocation that observes all sibling routines and
  enables their replacements together. It should assert every newly admitted
  replacement count and at least one normalized replay per newly implemented
  branch.
- Add a separate focused ROM regression only when it exercises a unique route,
  branch, write scope, or artifact that the family regression cannot express.
  Mark its reason in the test and in `PROGRESS.md`.
- Keep pure-C/game-logic tests exhaustive for classifier states, malformed
  work RAM, and invalid slots. They are cheap and run on every slice.
- Do not bless a changed hash by itself. Explain the changed behavior, inspect
  call/write output, and preserve an original-assembly comparison first.
- Use the same family trace to profile several candidate sibling routines
  before choosing the next batch. A natural route is stronger evidence than a
  synthetic state patch.

## Fallback hotspot profiling

Use `tools/Analyze-NativeFallbackHotspots.ps1` to rank a bounded observer set
from one or more routine-call traces. It measures calls, master cycles, and
ordered writes, then assigns a transparent heuristic weight of total master
cycles plus 12 per write. The native dispatch registry automatically marks
registered addresses; all other observed addresses are assembly candidates.

The runtime deliberately records routine-call scopes only for explicit
`--observe-hook` addresses. This is therefore a measured candidate/family
profile, not a misleading whole-ROM call graph. Before selecting a batch,
observe the current native parent plus its likely continuation or sibling
addresses on a natural route, then rank them:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\Analyze-NativeFallbackHotspots.ps1 `
  -CallPath out\build\windows-vs2022\candidate.calls.csv `
  -AssemblyOnly
```

Use `-AssemblyRoutine 02:4AEB` to force an address to be shown as an assembly
candidate when a deliberately broad experimental registration exists. Passing
multiple call traces combines their route weights; compare like-for-like
recording plans, not Debug and Release duplicates. Treat the result as a
prioritization aid: confirm each selected branch with its original transcript
before writing a replacement.

## Commands

Assuming the full host preset has been configured and `DC_ROM_PATH` is
available to CMake:

```powershell
cmake --build --preset windows-release --target dc_core_tests devils_crush --parallel 8
.\out\build\windows-vs2022\tests\Release\dc_core_tests.exe
ctest --test-dir out\build\windows-vs2022 -C Release --output-on-failure -R '^dc_rom_<family>$'
```

At a family checkpoint:

```powershell
cmake --build --preset windows-debug --target dc_core_tests devils_crush --parallel 8
.\out\build\windows-vs2022\tests\Debug\dc_core_tests.exe
ctest --test-dir out\build\windows-vs2022 -C Debug --output-on-failure -R '^dc_rom_<family>$'
```

Run Debug and Release builds sequentially: multi-config CMake generation and
build output are shared. Do not run their builds concurrently.

## Future acceleration work

The next infrastructure improvement is a persisted routine-replay corpus:
record normalized pre-call save states and expected artifacts from an
authoritative family route, then replay adapters directly without booting the
entire 3,600-frame scenario. Until that corpus exists, batching sibling work
against a single Release family route provides the best speed/fidelity trade.
