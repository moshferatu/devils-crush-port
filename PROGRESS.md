# Project Progress

Last updated: 2026-07-24

## Current Status

**Phase:** Milestone 1 implementation and hardening

**Implementation progress:** 18% overall; approximately 92% of Milestone 1

**Current milestone:** 1. Emulated Windows baseline

The first native Windows host is implemented. It validates the exact USA ROM,
boots it through the pinned Beetle PCE core, produces video and audio, accepts
keyboard/gamepad input, persists backup RAM, and presents the framebuffer
through the fixed sharp monitor-resolution scaler.

## Milestones

| Milestone | Status | Completion gate |
| --- | --- | --- |
| 1. Emulated Windows baseline | In progress | Verified ROM runs through the pinned Beetle PCE fork with SDL3 video, audio, input, saves, and sharp monitor-resolution scaling |
| 2. Instrumentation and hook bridge | Not started | Required game state is mapped and original routines can be observed, shadow-tested, and replaced |
| 3. Cabinet preview | Not started | Three-window cabinet mode shows the stitched playfield, score DMD, user backglass, and playfield-hosted bonus stages |
| 4. Native gameplay port | Not started | Physics, collisions, RNG, scoring, table rules, bonus stages, and session transitions execute natively |
| 5. Final cabinet world model | Not started | Desktop and cabinet profiles share the same native gameplay state without divergence |
| 6. Release hardening | Not started | Tested, documented, ROM-free Windows x64 package is ready for release |

Status values used in this file are `Not started`, `In progress`, `Blocked`, and `Complete`.

## Completed

- [x] Inspected and identified the repository contents.
- [x] Confirmed the ROM as the supported USA image:
  - Size: 393,216 bytes
  - MD5: `92fbe89d4c87bf283dbfbeac69a64e6e`
  - SHA-256: `9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9`
- [x] Selected C++20, CMake, and SDL3 for the Windows host.
- [x] Selected a pinned Beetle PCE fork as the permanent emulated hardware layer.
- [x] Selected Mesen2 as the independent debugger and behavioral oracle.
- [x] Defined the progressive pass-through/observe/replace strategy for native gameplay routines.
- [x] Defined desktop and three-monitor cabinet presentation behavior.
- [x] Fixed sharp pixel scaling as the only v1 scaling method.
- [x] Approved the implementation and acceptance plan.
- [x] Added ROM/build/generated-content exclusions and a CI ROM-leakage guard.
- [x] Added the C++20/CMake/Visual Studio project and dependency-free test preset.
- [x] Licensed the combined Windows application as GPL-3.0-or-later and added dependency locks/notices.
- [x] Pinned SDL 3.4.12 by release-archive SHA-256.
- [x] Pinned and statically built Beetle PCE 1.29.0 at commit `ae99235c2139c176c1a8d0fde2957bf701d3cab0`.
- [x] Implemented exact ROM size, MD5, and SHA-256 validation.
- [x] Implemented the single-instance `PceRuntime` libretro adapter.
- [x] Verified ten emulated frames from the repository ROM:
  - 256x240 compact ARGB framebuffer.
  - 44,100 Hz interleaved stereo audio.
  - 2 KiB backup RAM with exact-size import/export.
- [x] Implemented SDL keyboard and first-gamepad input.
- [x] Implemented fixed-cadence emulation independent of monitor refresh.
- [x] Implemented high-DPI SDL presentation with the fixed two-pass sharp scaler.
- [x] Implemented atomic backup-RAM persistence in `portable-data/devils-crush.sav`.
- [x] Added ROM-free unit/CLI tests and an opt-in `rom_required` smoke test.
- [x] Verified Debug and Release builds, all four public CTests, the dependency-free stub test, and the local ROM-required smoke test.
- [x] Added raw Beetle savestate capture/restore with the host frame counter.
- [x] Added a checksummed version-1 quick-state container bound to the exact
  ROM SHA-256 and pinned Beetle revision.
- [x] Added atomic F5 quick-save and validated F9 recovery, including queued
  audio reset and rejection of corrupt or incompatible files.
- [x] Verified a real-core savestate round trip with exact restored framebuffer
  output and matching audio sample rate/batch timing.
- [x] Added first-gamepad hot-plug, disconnect failover, and live SDL mapping
  reload from `portable-data/gamecontrollerdb.txt`.
- [x] Bounded queued audio latency to 150 ms and added non-fatal playback
  stream recovery after queue errors, device loss, or format changes.
- [x] Exposed a copy of the non-SuperGrafx 8 KiB PCE work-RAM region for
  deterministic recording and future typed game-state views.
- [x] Added a fixed 600-frame canonical boot/input recorder with per-frame
  work-RAM, framebuffer, audio payload, geometry, and audio-timing hashes.
- [x] Verified repeated Debug recordings and an optimized Release recording
  were byte-identical (SHA-256
  `67a55bed2d4f4e8dc5019c2b9b845df2f29514bbb564e2e44cde406d159575fe`).
- [x] Added Debug and Release opt-in ROM tests that fail if the complete
  canonical recording differs from that golden SHA-256.

## Next Actions

- [x] Add repository hygiene for ROMs, generated assets, traces, build products, and local configuration.
- [x] Add the project license and third-party attribution structure.
- [x] Establish the C++20/CMake project and Windows CI build.
- [x] Pin and integrate the Beetle PCE core baseline.
- [x] Implement exact ROM hash validation.
- [x] Create the SDL3 application shell and emulator-facing `PceRuntime` boundary.
- [x] Display the emulated framebuffer in a resizable Windows window using the fixed sharp scaler.
- [x] Connect SDL keyboard/controller input and emulated audio.
- [x] Add versioned savestate serialization and recovery.
- [x] Add controller hot-plug/remapping and robust audio latency handling.
- [x] Add a canonical boot/input recording with RAM, framebuffer, and audio timing hashes.
- [ ] Compare the canonical recording against Mesen2 and document any differences.
- [ ] Exercise and document full backup-RAM reload behavior through the visible host.

## Active Work

Milestone 1 hardening: Mesen2 comparison, visible backup-RAM reload validation,
and release-oriented packaging checks.

## Blockers

No external blocker. The independent Mesen2 parity recording and remaining
host resilience work are the primary Milestone 1 engineering gaps.

## Decisions Locked for V1

- Windows 10/11 x64.
- Public GPL-3.0-or-later source with a user-supplied ROM; no ROM or extracted assets in releases.
- Exact USA ROM only.
- Beetle PCE remains as the hardware compatibility layer.
- Original gameplay behavior is the faithful default.
- Gameplay logic is ported incrementally to native C++.
- Sharp pixel scaling is always enabled; no alternative filters.
- Desktop mode retains original scrolling.
- Cabinet mode uses:
  - Full stitched table on the playfield monitor.
  - Bonus stages on the playfield monitor in place of the table.
  - Score on the DMD monitor.
  - User-selected static image on the backglass monitor.
- Initial cabinet input supports keyboard/controllers and semantic feedback hooks; physical DMD, DOF, and SSF support are deferred.

## Progress Update Template

When work advances, update:

1. The current phase, implementation percentage, and milestone status.
2. The `Completed` and `Next Actions` checklists.
3. `Active Work` with the current task and owner.
4. `Blockers` with a concrete resolution condition.
5. The date at the top of this document.
