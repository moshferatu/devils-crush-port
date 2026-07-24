# Third-Party Dependencies

Third-party source is fetched into the build tree or added from an explicitly
pinned fork. It must not be copied into this directory without also recording
its origin, immutable revision, license, and modification status.

The machine-readable source of truth is
[`dependencies.lock.json`](dependencies.lock.json). Human-readable license
and attribution information is maintained in
[`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

## Current dependencies

- SDL 3.4.12 is fetched from its official release archive. CMake verifies the
  archive SHA-256 before extraction.
- Beetle PC Engine 1.29.0 is fetched from the exact commit recorded in the
  lock file and built through the project-owned CMake wrapper. Upstream source
  is currently unmodified; move this pin into the project fork before adding
  HuC6280 execution hooks.

## Updating a dependency

1. Select a tagged release and resolve it to an immutable commit.
2. Prefer an upstream release archive and independently verify its SHA-256.
3. Update `dependencies.lock.json` and the corresponding CMake dependency
   declaration in the same change.
4. Update `THIRD_PARTY_NOTICES.md`, preserving the dependency's exact license
   and copyright notices.
   For Beetle, keep `bundledComponents` synchronized with the sources compiled
   by `cmake/BeetlePce.cmake`, including transitive code bundled in its tree.
5. Document local patches and plainly mark modified upstream files.
6. Build and run all tests from a clean build directory.
7. Confirm that source and binary packages contain every required license and
   the complete corresponding source required by GPL-3.0-or-later.

Never place ROMs, extracted assets, captures, traces, generated disassembly,
save data, or other proprietary game content in this directory.
