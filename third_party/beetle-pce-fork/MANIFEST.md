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
2. `patches/0002-huc6280-call-observation.patch`
   - Adds the current opcode to instruction-entry state.
   - Adds a post-write callback reporting logical/physical addresses, value,
     originating instruction, frame-local master cycle, and the core's
     serialized absolute master-cycle timestamp.
   - Enables bounded read-only routine-call observation without changing
     registers, memory values, timing, or opcode dispatch.
3. `patches/0003-huc6280-native-replacement.patch`
   - Adds an optional transactional routine-replacement callback after
     instruction observation and before opcode dispatch.
   - Validates ordered entry-relative write times, commits writes through the
     HuC6280 memory path, advances hardware master time, applies final
     registers/MPRs, and resumes at the declared outer RTS.
   - Leaves normal opcode execution unchanged when no replacement is
     returned. Transactions without the checkpoint extension remain limited
     to mapped no-interrupt scopes.
   - Exposes conservative master-cycle distances to the next user/video event
     and next serviceable timer interrupt. Broad semantic replacements can
     cross harmless timer-divider ticks while still falling back before an
     interrupt could be asserted inside a native body.
4. `patches/0004-vdc-video-snapshot.patch`
   - Adds read-only VDC VRAM, VCE palette, and background-register accessors.
   - The project-owned C boundary copies their values between frames and never
     exposes emulator-owned pointers.
5. `patches/0005-vdc-sprite-snapshot.patch`
   - Adds read-only access to the base VDC's 256-word internal post-DMA sprite
     attribute table.
   - Extends the same frame-boundary copy so the native renderer observes the
     table actually consumed by Beetle rather than only its VRAM DMA source.
6. `patches/0006-huc6280-interruptible-replacement.patch`
   - Adds optional instruction-boundary checkpoints to a native replacement
     transaction, including the original interrupt-sampling time and complete
     CPU/MPR state at each boundary.
   - Advances one checkpoint at a time so Beetle can run an asserted interrupt
     through the original ROM handler and then resume the remaining native
     transcript after `RTI`.
   - Defers a same-offset write attributed to the following instruction until
     that instruction resumes, so an interrupt at the preceding boundary
     cannot prematurely commit nested-JSR stack writes.
   - Advances to the pre-write boundary for a replacement write to a device
     that arbitrates the bus -- the base VDC's four logical ports and the VCE
     register file at `$0400`-`$0405`, the two cases where `IOWrite` calls
     `StealCycle` -- and lets the normal HuC6280 bus path charge that stolen
     cycle, so it lands inside the offset the transcript declared. Applies
     only when MPR0 maps the hardware page to physical bank `$FF`.
   - Preserves the pending transcript in savestates and safely abandons it at
     the current original instruction if an interrupt handler does not restore
     the checkpoint register/MPR state.
   - Keeps replacement storage fixed and bounded; the checkpoint capacity is
     2,048 so composed parents such as the four-multiply flipper projection
     can retain every original interrupt-sampling boundary. The independent
     write capacity remains 512.

All local files and patches are GPL-3.0-or-later. Upstream per-file notices
and licenses remain authoritative for upstream sources.
