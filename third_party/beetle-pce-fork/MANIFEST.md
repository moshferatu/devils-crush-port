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
- `include/dc_beetle_reference_psg.h` and `src/dc_beetle_reference_psg.cpp`:
  the emulator's own `PCE_PSG` as a bare reference device, for the tests only.
  `PCE_PSG` needs nothing from the rest of the machine, so this is a lifetime
  and naming shim rather than a hook — nothing in the core calls it, and
  installing a native model does not affect a device created through it. It
  exists because the game never asks the chip for the LFO, for noise on
  channel 4, or for more than seven emissions in its accumulating mode, so
  those paths of the native model have no oracle in any recording of the game.
  Driving both devices from the same synthetic register stream gives them one.
  It is deleted with the emulator.
- `include/dc_beetle_reference_vdc.h` and `src/dc_beetle_reference_vdc.cpp`:
  the emulator's own `VDC` as a bare reference device, on the same terms and
  for the same reason. The VDC was expected to resist this, because the game
  reads it back and a reference device was assumed to need the machine around
  it; it does not. `vdc.cpp` touches no emulator global, the device owns its
  VRAM, sprite attribute table, registers and raster phase, and its reads are
  methods on it. Two differences from the PSG shim are recorded in the header.
  Its outward edges are two function-pointer hooks with no user data, so only
  one reference device may exist at a time. And the wait-state hook's refusal
  is load-bearing rather than advisory — the device stalls by looping until
  the hook says no — so this reproduces `VCE`'s per-frame budget of 455 * 64
  stolen cycles, without which the device hangs the first time a port access
  lands during a block transfer. It is deleted with the emulator.
- `include/dc_beetle_reference_vce.h` and `src/dc_beetle_reference_vce.cpp`:
  the emulator's own `VCE`, for the tests only, and the one of the three that
  is not a lifetime shim. The encoder does need the machine around it: it
  steals processor cycles for a stalled port access, drives an interrupt
  line, asks the processor to stop early at two scanlines, and takes its own
  timestamps from it. So this supplies the smallest machine it will run in --
  the emulator's own processor object, initialised and never executed, moved
  forward by stolen master cycles instead of by instructions. That is enough
  because everything `VCE` asks the processor for is a counter it advances or
  a flag it sets. It claims the emulator's `vce` global, which the
  controller's two hooks find their encoder through, so only one may exist at
  a time and none may exist while a real machine is running. It is deleted
  with the emulator.
- `include/dc_beetle_reference_irq.h` and `src/dc_beetle_reference_irq.cpp`:
  the emulator's own timer and interrupt controller, for the tests only, and
  a step further than the encoder's shim. This device is not an object at
  all -- it is a dozen fields inside `HuC6280` and four methods the machine's
  I/O page handler calls from outside -- so there is nothing to lift out.
  This supplies the processor those fields live in, initialised and never
  executed, and drives it by stealing master cycles. That substitution is
  exact rather than convenient: the timer's whole relationship with time is
  one phase against the processor's clock, and that clock advances whether
  or not instructions are running. Stealing cycles also runs `HappySync`
  whenever the divider comes due, so the emulator catches its timer up at
  the moments a real machine would, which is what makes comparing it against
  a single catch-up worth doing. It claims the emulator's own processor, so
  only one may exist at a time, none may exist while a real machine is
  running, and it must not share a process with a live reference encoder.
  It is deleted with the emulator.
- `include/dc_beetle_reference_scheduler.h` and
  `src/dc_beetle_reference_scheduler.cpp`: the emulator's own clock and event
  scheduler, for the tests only. The timer's shim drove the processor's clock
  in order to watch a device hanging off it; this one watches the clock, and
  the substitution it relies on is the same and slightly stronger -- the
  scheduler does not know what an instruction is, so a stolen cycle and an
  executed one move it identically, and everything it does happens inside
  `StealCycles`. Two of its fields have no public entry point at all, `speed`
  being written by CSL and CSH and the loop flag by `Run`, and both are
  reached by writing the register block `0015` makes public -- which is how a
  native machine reaches them too. Unlike the other three shims this
  one installs the *user event handler* as well, because when the handler is
  called and with what timestamp is half of what is being tested: the
  installed handler returns a configurable interval and folds every call into
  an order-sensitive digest. It claims the emulator's own processor, so only
  one may exist at a time, none may exist while a real machine is running,
  and it must not share a process with a live reference encoder or a live
  reference timer. It is deleted with the emulator.
- `dc_beetle_irq_state`, `dc_beetle_irq_level` and `dc_beetle_irq_model` in
  `include/dc_beetle_execution_hook.h`: the fourth device-model boundary,
  installed by `0013`. `dc_beetle_irq_level` is the one piece of it that has
  no counterpart in the other three -- the three values the processor mirrors
  from the model, read back after every dispatch, because they are on the
  emulator's hottest path and cannot be fetched through a call.
- `dc_beetle_scheduler_registers` and `dc_beetle_scheduler_model` in
  `include/dc_beetle_execution_hook.h`, with
  `dc_beetle_active_scheduler_registers` beside them: the fifth device-model
  boundary, installed by `0015`. The registers block is what makes this one
  unlike the four before it -- it is the device's whole state, owned by the
  model and pointed at by the processor, so that the clock, the near horizon
  and the loop flag can be read from inside the instruction loop without a
  dispatch. The pointer is never null; with no model installed it addresses
  this boundary's own storage.
- `dc_beetle_scheduler_sync_timer`, `dc_beetle_scheduler_timer_horizon`,
  `dc_beetle_scheduler_rebase_timer` and `dc_beetle_scheduler_user_event` in
  `src/dc_beetle_execution_hook.cpp`: the four machine edges an installed
  scheduler stops at. Three are the timer and reach whichever controller the
  machine has; the fourth is the user event handler, called at the moment its
  horizon came due rather than the moment the scheduler noticed.

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
7. `patches/0007-huc6280-nested-replacement.patch`
   - Lets a replacement be installed while another is suspended at a
     checkpoint. A suspended transcript is saved to a fixed frame stack so the
     interrupt handler that preempted it can carry a transcript of its own,
     and is made current again when the inner one completes or is abandoned.
   - Consults the replacement callback on every instruction reached, rather
     than only when no transcript is active. A transcript's own resume points
     restart the instruction loop before the callback, so the instructions
     newly exposed to it are exactly the interrupt handlers'.
   - Declines a replacement instead of exceeding the fixed depth, and counts
     nested installs, refusals, and the depth high-water mark through the
     project-owned C boundary. The refusal count is what lets a host that
     credits a native call at offer time detect an overstated native share.
   - Serializes the frame stack and validates every saved frame's counts
     against their capacities on load.

8. `patches/0008-psg-device-model.patch`
   - Lets a project-owned sound generator replace the emulator's. Every one
     of the five entry points the PSG has -- the register write at `$0800`,
     the frame-boundary update, the timestamp rebase, and power -- consults
     the registered model first, and the emulator's own PSG receives nothing
     while one is installed.
   - Reports the device the emulator resolved from its settings: the two
     output buffers it built and which of the two chip revisions it chose.
     The revision scales every accumulated sample by two, so a model left to
     infer it from the same settings a second time is a different device.
   - Adds a plain field export and import to the emulator's PSG so its
     savestate format still reads and writes the model's values. The
     serialized set is unchanged, so a state written by either arm loads in
     the other.
   - Changes nothing when no model is registered.

9. `patches/0009-vdc-device-model.patch`
   - Lets a project-owned video display controller replace the emulator's
     first one. Every entry point `VCE` has into `vdc[0]` consults the
     registered model first: reset, the three clocking calls, both port
     directions, the interrupt poll, the register read the encoder does for
     its own scanline bookkeeping, the two debugging controls, and the two
     read-only peeks the project's frame snapshot uses.
   - Routes reads as well as writes, which the sound patch did not have to.
     The game opens by reading the status register, so a write-only route
     would answer from a device that had never been written to.
   - Leaves the schedule in the emulator. The dot-clock divider, the two
     blanking counters, the palette, the framebuffer and the SuperGrafx
     compositor are untouched; only the chip that answers changes. The
     second chip of a SuperGrafx is never routed.
   - Exports the two edges the device reaches back through, rather than
     letting a model invent them. `dc_beetle_vdc_irq_change` re-derives the
     CPU's interrupt level from every chip's latched status the way
     `VCE::IRQChangeCheck` does, and `dc_beetle_vdc_wait_state` charges the
     CPU and refuses when the per-frame budget of `455 * 64` stolen cycles
     is spent. The refusal is load-bearing: the device stalls by asking
     until it is told no, so a hook that always agreed would hang the
     machine rather than fail it.
   - Adds a plain field export and import to the emulator's VDC so its
     savestate format still reads and writes the model's values, on the same
     terms as the sound patch. The state is 128 KiB, so it is heap-allocated
     for the shuttle rather than left on a stack. The character cache is not
     carried in either direction: it is derived from video memory, and both
     the emulator's loader and the model rebuild it.
   - Changes nothing when no model is registered.

10. `patches/0010-vce-reference-observation.patch`
    - Opens the colour encoder to a test that has to drive it without a
      processor. Unlike the two device patches, it installs nothing and
      routes nothing: every addition is a new method, and no existing line
      changes.
    - `DcObserve` reads the encoder's schedule -- the clock divider, both
      controller horizons, the compact disc event, the scanline, the pixel
      cursor, both blanking counters and their phases, the stall budget, the
      palette address, the control register, the pending field restart, and
      whether the frame is done. All of it is private, and a differential
      test that could only compare pixels would report "the picture moved"
      for any of a dozen causes.
    - `DcSurfClut` and `DcColorTableCache` read the two expansions of a
      nine-bit palette entry and the merge of them with palette memory.
      What an entry looks like is the display's decision rather than the
      chip's, so a native model is handed these rather than deriving them:
      the comparison is then about the encoder and not about colour
      arithmetic.
    - `DcSyncReal` is an out-of-line body for `SyncReal`, which is declared
      `INLINE` and inlined into the single caller `vce.cpp` has, so no
      caller outside that file can link against it.
    - `DcCalcNextEvent` and `DcResetWaitStates` reproduce what the register
      file's callers and `VCE::RunPartial` do around the device, for a
      caller that has no processor to do them for it.
    - Changes nothing at runtime. Nothing in the emulator calls any of it.

11. `patches/0011-vce-device-model.patch`
    - Routes every entry point the machine has into the colour encoder to a
      registered `dc_beetle_vce_model`: power-on, the `$0400`-`$0405`
      register file, the three controller-facing entry points, the timestamp
      rebase, the layer and sprite-limit controls, the horizontal overscan,
      the palette expansion, `RunPartial`, `StartFrame`, `EndFrame`'s last
      scanline report, the savestate, and `Sync` -- which is the processor's
      event handler and so the one place the whole schedule is entered from.
    - `DcOwned()` carries the `!sgfx` test, so a SuperGrafx keeps its
      encoder entire. Its second controller and the priority unit that
      composites the two are not modelled.
    - The framebuffer crosses the boundary. `StartFrame` hands over
      `MDFN_Surface::pixels`, the pitch and the line-width array, under a
      `static_assert` that `bpp_t` is still sixteen bits wide -- which it is
      only because this build selects RGB565.
    - `write_scanline_info` is split. Its body moved to
      `DcWriteScanlineInfo`, which takes the six values it used to read,
      because with a model installed every one of them belongs to the model.
      The frontend keeps the resolution bookkeeping those values feed, the
      multi-resolution rescale in `EndFrame`, and the display rectangle. The
      original returns early when a model is installed: the emulator's own
      loader calls `SetVCECR` on the dormant object, and a report from there
      would be a second and wrong entry beside the model's.
    - One repair that is not routing. `scanline_info` holds 16 entries behind
      a counter upstream never bounded, safe only while a field restart
      arrives before a seventeenth report; a host that loads ten savestates
      without a full field between them overruns it. That reproduces with
      every device model handed back, so the path is the emulator's -- this
      patch only moved the statics the overrun was landing in, which is what
      turned it from silent into fatal. The write is now bounded and the
      resolution summary still updated, so a dropped entry costs the rescale
      one boundary rather than the frontend its geometry.
    - `dc_beetle_vce_vdc_*` export the `DcVDC_*` dispatch `0009` introduced,
      so an installed encoder finds the native controller when one is
      registered and the emulator's chip when none is. One decision point
      serves both encoders, and the two models may be installed in either
      order or one without the other.
    - `VCE::IRQChangeCheck` and `VCE::WS_Hook` route to the model as well.
      Those are reached from the emulator's *own* controller, so this is
      what makes the arm with a native encoder and an emulated controller
      work rather than a defensive check.
    - `dc_beetle_vce_irq`, `dc_beetle_vce_steal_cycles`,
      `dc_beetle_vce_exit` and `dc_beetle_vce_cd_event` are the four machine
      edges an installed model reaches back through. Each is one line of
      what the emulator's own encoder does in the same place.
    - `DcExportVceState` and `DcImportVceState` are the savestate shuttle:
      plain field moves, read back after the emulator's own load-time
      repairs have run over them. Roughly a kilobyte, so it travels on a
      stack rather than the heap the controller's 128 KiB needed.
    - Changes nothing when no model is registered.
12. `patches/0012-huc6280-irq-reference-observation.patch`
    - Observation only, for the timer and interrupt controller. Both register
      files are already public, because the machine's I/O page handler
      reaches them from outside; everything behind them is private, and the
      one field a differential test most needs is the divider's phase, which
      no register exposes.
    - `DcIrqObservation` and `DcObserveIrq` read out the timer's five fields,
      the request register, the mask, the processor's latched sample of the
      masked pending set, and the next-event horizon.
    - `DcTimerInterruptHorizon` exposes
      `GetMasterCyclesUntilTimerInterrupt`, which `0003` added and which
      every native transcript declares itself against. It is part of this
      device's surface even though nothing outside the processor calls it.
    - `DcSetIFlag` is the one entry point here that writes processor state.
      That horizon is gated on the I flag and no public entry point reaches
      the status register, so a test that wants the gate closed has no other
      way to close it -- and closing it is half of what the horizon means.
    - `DcAcknowledgeReset` is what `RunSub` does to the controller when it
      takes the reset vector: the mask opens to all three lines and the
      request is released. Nothing else ever opens the mask, so a device
      driven without an executing processor stays behind a closed one
      forever without this.
    - Adds no data members, so the processor object's layout and the
      `MDFN_ALIGN(128)` assertion in `Init` are untouched. Changes nothing
      for any caller that does not use these.
13. `patches/0013-huc6280-irq-device-model.patch`
    - Routes the HuC6280's timer and interrupt controller to a registered
      `dc_beetle_irq_model`. The fourth device seam and the only one inside
      the processor rather than beside it: there is no device object to make
      dormant, only a dozen private fields.
    - `Init`, `Reset`, `Power`, `TimerSync`, `SyncAndResetTimestamp`, both
      register files, `IRQBegin`/`IRQEnd`, the reset-vector acknowledge in
      `RunSub`, `GetMasterCyclesUntilTimerInterrupt` and the savestate all
      dispatch when a model is installed.
    - `timer_div`, `IRQlow` and `IRQMask` become a read-only mirror of the
      model, refreshed by `DcIrqPublish` after every dispatch that can move
      them. That is the one design decision in this patch that is not
      mechanical routing. `CalcNextEvent` is reached from `ADDCYC`, and the
      interrupt test at the top of `RunSub` runs once per instruction; both
      read those three values, and an indirect call at either point would
      cost more than the whole device does. Every *write* still goes through
      the model, so nothing reads a value the model does not hold, and
      `CalcNextEvent` itself is left untouched.
    - The sampling boundary stays where it was. `IRQSample`, `IFlagSample`,
      `LastCycle` and the vector-selection rule in `RunSub` -- with its
      asymmetric `(IRQlow & IQTIMER & IRQMask) && !IFlagSample` -- are the
      processor's, because they are decided at instruction boundaries and
      this device has no notion of an instruction. The model publishes a
      level; the processor decides what to do about it. That split is what
      lets the controller be native while the processor is still Beetle's.
    - `Reset` routes but keeps `IRQSample = IQRESET` here, since that is the
      processor's copy. `Power`'s `IRQlow = 0` routes to `end_request` of
      every line even though the `Reset()` below assigns the register
      outright and makes it invisible -- a mirror written by anything other
      than a dispatch is a mirror a reader has to reason about.
    - The reset-vector acknowledge opens the mask and releases the line
      together, where the emulator releases the line seven cycles later after
      the vector fetch. The reset line is not one of the three the status
      register reports and nothing in between can observe it.
    - The savestate shuttle is inside `HuC6280::StateAction` rather than in a
      dormant object, because the fields are the processor's own. Seven
      values out before serializing, seven back in after the emulator's own
      load-time repairs have run over them.
    - Changes nothing when no model is registered.
14. `patches/0014-huc6280-scheduler-reference-observation.patch`
    - Observation only, for the processor's clock and event scheduler.
      Installs nothing and routes nothing: every addition is a new method
      and no existing line changes.
    - `DcSchedulerObservation` and `DcObserveScheduler` read out the
      timestamp, the overclock remainder and rate, both event horizons, the
      loop flag, the clock rate and the shift that shadows it. All of it is
      private. The shift is included even though it is derived, because it
      is the value the conversion from cycles to master clocks actually
      uses, and a model that held the rate right and shadowed it wrong would
      otherwise be invisible.
    - `DcSetSpeed` and `DcSetRunFlag` are the two entry points here that
      write processor state, and they exist because neither field is
      reachable without executing an instruction: `speed` is written by the
      CSL and CSH opcodes, and the loop flag by `Run`, which runs. A
      scheduler driven by stolen cycles has no other way to reach them.
      `DcSetSpeed` refreshes `speed_shift_cache` for the same reason
      `DcSetIFlag` refreshes `PIMaskCache`.
    - Everything else the scheduler does is already public, which is worth
      recording because it is why this patch is so small: `StealCycles`,
      `StealMasterCycles`, `SetEvent`, `SetEventHandler`, `TimerSync`,
      `SyncAndResetTimestamp`, `SetOverclock`, `Timestamp` and `Exit` are
      all reachable from outside. `CalcNextEvent` is not, and needs no entry
      point either: `SetEvent` handed the horizon it already holds is the
      same two statements with the first one a no-op.
    - Adds no data members, so the processor object's layout and the
      `MDFN_ALIGN(128)` assertion in `Init` are untouched. Changes nothing
      for any caller that does not use these.
15. `patches/0015-huc6280-scheduler-device-model.patch`
    - Routes the HuC6280's clock and event scheduler to a registered
      `dc_beetle_scheduler_model`. The fifth device seam, and the first whose
      shape is a block of memory rather than a call table.
    - The clock, its remainder, the overclock rate, both event horizons, the
      loop flag, the clock rate and the shift that shadows it stop being
      members of `HuC6280` and become the eight fields of a
      `dc_beetle_scheduler_registers` block that `DcSched()` points at. With
      no model registered that block is the boundary's own storage, so the
      uninstalled machine is the emulator's own scheduler and nothing else.
    - That is the one design decision here that is not mechanical routing,
      and it is the opposite of `0013`'s. The controller could be mirrored
      because the processor only *reads* the three values on its hot path;
      this device is *written* on its hot path, so a mirror would have to be
      refreshed from inside `ADDCYC`. Worse, the handler an advance stops for
      reads the clock back and re-arms its own horizon while that advance is
      still on the stack -- a mirror would be stale exactly there. Sharing
      the storage removes the question: reads are loads, writes are calls.
    - `Init`, `ADDCYC` and `ADDCYC_MASTER`, `CalcNextEvent`, `SetEvent`,
      `SyncAndResetTimestamp`, `SetOverclock`, `Run`, `Exit` and the CSL and
      CSH opcodes dispatch when a model is installed. `Timestamp`, the near
      horizon read by the `WAI` opcode, the loop condition in `RunSub`, the
      block-move loop check and the replacement machinery's stall pricing do
      not: they read the block.
    - `Reset` and `Power` are absent from that list because they touch this
      device in neither arm. Between them they clear the whole processor and
      the whole interrupt controller and leave every field here exactly as it
      was; only `Init` puts the clock back to zero.
    - The savestate needs no shuttle, which no other seam has managed. The
      four entries this device owns are written straight through the block by
      `SFVARN`, so the format stays the emulator's without a capture-and-apply
      pair to keep in step with it; the load-time repair `speed &= 0x01`
      followed by `REDOSPEEDCACHE` writes the model's own fields in place.
    - `DcTimerHorizon`, `DcRebaseTimer` and `DcUserEvent` are the edges an
      installed scheduler stops at, each a line lifted out of the function
      that used to contain it and left where it was. A native scheduler
      therefore stops for whichever timer the machine has -- a native
      controller when `0013`'s model is installed, this processor's own
      register file when it is not.
    - Removes `DcSetSpeed` and `DcSetRunFlag`, which `0014` added for the
      reference shim. Both are redundant once the block is public: the shim
      writes the two fields the same way a native machine will.
    - Removes eight data members and adds none, so the processor object's
      layout and the `MDFN_ALIGN(128)` assertion in `Init` only get easier to
      satisfy. Changes nothing when no model is registered.

All local files and patches are GPL-3.0-or-later. Upstream per-file notices
and licenses remain authoritative for upstream sources.
