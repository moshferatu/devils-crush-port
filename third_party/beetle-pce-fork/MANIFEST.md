# Beetle PC Engine Downstream Fork Manifest

This directory is the project-owned downstream fork layer for Beetle PC
Engine. CMake fetches the exact upstream base revision and applies the ordered
patch series below before compiling the core. Keeping the small delta here
makes the fork reproducible without copying unrelated upstream sources into
this repository.

## Base

- Repository: <https://github.com/libretro/beetle-pce-libretro>
- Version reported by the core: 1.29.0
- Revision: `ae99235c2139c176c1a8d0fde2957bf701d3cab0`

## Local sources

- `include/dc_beetle_execution_hook.h`: narrow C boundary shared by the
  patched HuC6280 core and the host adapter.
- `src/dc_beetle_execution_hook.cpp`: single-instance callback registration
  owned by the downstream fork.

## Patch series

1. `patches/0001-huc6280-execution-hook.patch`
   - Adds a pre-instruction callback at the HuC6280 instruction boundary.
   - Reports the logical PC, its mapped physical 8 KiB bank, registers, MPRs,
     and the current master-cycle timestamp.
   - Does not modify registers, memory, timing, or opcode dispatch.

All local files and patches are GPL-3.0-or-later. Upstream per-file notices
and licenses remain authoritative for upstream sources.
