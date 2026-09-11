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
- `include/dc_beetle_reference_boundary.h` and
  `src/dc_beetle_reference_boundary.cpp`: the emulator's own instruction
  boundary, for the tests only, and the thinnest shim in the series by a
  wide margin. The four before it had to *drive* something -- a clock, a
  timer, an encoder handed a scanline -- because the device only revealed
  itself while running. This one is asked. The boundary holds two latched
  values and otherwise computes, so the whole of it is reachable by writing
  four inputs and reading one answer, and the test behind it is an
  enumeration rather than a stimulus. It holds no state of its own at all;
  the handle exists so the one-at-a-time rule has something to enforce
  itself with. It claims the emulator's processor -- its request register,
  its mask, its status register and the two latched values -- so only one
  may exist at a time, none may exist while a real machine is running, and
  it must not share a process with a live reference timer, which owns the
  same registers. It is deleted with the emulator.
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
- `dc_beetle_boundary_state`, `dc_beetle_boundary_model` and the
  `DC_BEETLE_BOUNDARY_*` decision values in
  `include/dc_beetle_execution_hook.h`: the sixth device-model boundary,
  installed by `0017`. This one is a plain call table with no shared block
  and no mirror, which is the whole of what makes it the sixth shape: the
  interrupt controller is *read* from the instruction loop and became a
  mirror, the scheduler is *written* from it and became a block of memory,
  and this runs twice per instruction with no memory traffic behind it at
  all. The processor hands over the request register, the mask and its own I
  flag and takes back a small integer. `HuC6280::DcInterruptDecision` is
  defined in terms of the values here rather than beside them.
- `dc_beetle_pad_state`, `dc_beetle_pad_port_state`, `dc_beetle_pad_model`
  and the `DC_BEETLE_PAD_DEVICE_*` values in
  `include/dc_beetle_execution_hook.h`, with the remembered settings and port
  assignments in `src/dc_beetle_execution_hook.cpp`: the seventh device-model
  boundary, installed by `0019`. Its shape is the sixth's -- a plain call
  table -- but what crosses it is not: a model is *told* its configuration
  rather than reading it, because the emulator reports each piece exactly once
  at the load that builds its devices. Deliberately absent from the table is
  any way for the host's input to arrive, which belongs to the frontend and
  reaches a model directly.

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
16. `patches/0016-huc6280-instruction-boundary-reference-observation.patch`
    - Observation only, for the decision made between two instructions.
      Installs nothing and routes nothing to any model.
    - Unlike the three observation patches before it, this one does not only
      add: it *lifts*. `DcSampleInterrupt` is the two assignments at the top
      of `LastCycle` and `DcSelectInterrupt` is the vector-selection rule at
      the top of `RunSub`, each moved into a named method that its original
      caller now calls. There is therefore no second copy of the rule for a
      differential test to compare against -- what it drives is the code the
      game runs.
    - `DcInterruptDecision` names the five answers and `DcInterruptVector`
      maps them to `0xFFFE`, `0xFFFA`, `0xFFF8` and `0xFFF6`. `RunSub` keeps
      every cycle, push and fetch it had; only the deciding moved.
    - The decision is assigned to a variable declared beside `old_PC` rather
      than where it is used. That is not style: a block move resumes by
      `goto` into the middle of the loop, and a jump may not cross an
      initialization.
    - `DcAcknowledgeInterruptSample` is the one line that takes the reset
      line back out of the latch, shared by `RunSub` and `DcAcknowledgeReset`
      instead of written twice.
    - `DcBoundaryObservation` and `DcObserveBoundary` read out the request
      register, the mask and both latched values; `DcSetBoundaryLines` and
      `DcSetBoundarySample` write the four inputs directly, so a test can
      enumerate states no machine could be driven into -- including the ones
      only a savestate can produce.
    - Adds no data members. Changes nothing for any caller that does not use
      these.
17. `patches/0017-huc6280-instruction-boundary-device-model.patch`
    - The sixth device-model boundary, and the first that replaces a rule
      rather than a device. Routes the four entry points `0016` lifted --
      `DcSampleInterrupt`, `DcSelectInterrupt`, `DcAcknowledgeInterruptSample`
      and the new `DcResetInterruptSample` -- to
      `dc_beetle_active_boundary_model` when one is registered, and leaves
      the emulator's own code in the `else` arm untouched.
    - `DcSelectInterrupt` is the one dispatch in this fork on a path taken
      once per instruction with nothing rare about it. It is affordable for
      the reason the other two hot seams were not: its inputs are four values
      the processor already holds in registers and its output is a small
      integer, so no memory either side cares about is touched in between.
      The rejected alternative was to keep the emulator's own
      `(IRQSample | IRQlow)` gate inline and dispatch only when it opens --
      cheaper, but it would put a copy of the rule's first clause back in the
      fork, which is exactly what `0016` was shaped to avoid.
    - `DcInterruptVector` stops being `static` and dispatches too. The map
      from a decision to an address is part of the rule and not of the
      machine's memory map: a model that decided differently about which line
      won would have to answer for the address as well.
    - `Reset`'s two `IRQSample = IQRESET` assignments become
      `DcResetInterruptSample`. The one in the interrupt controller's arm is
      the only place in the processor where two installed models meet: the
      request register, the mask and the timer belong to `0013`'s controller
      and the latch belongs to this one.
    - The savestate is a capture-and-apply pair, unlike `0015`'s block. The
      two entries are captured into `IFlagSample` and `IRQSample` before the
      format is written and applied back after `REDOPIMCACHE` has run -- the
      processor's own copy of the I flag, not the boundary's latched one,
      which is why they are separate fields at all. `StateAction` is the one
      caller that reaches the latch without an instruction having ended,
      which is why it restores a boundary rather than taking one.
    - `DcObserveBoundary` reads the two latched values back through the model
      when one is registered. There is no mirror to refresh, so the
      processor's own two fields are stale for as long as a model is
      installed, and an observer that read them directly would be wrong.
    - `DcSetBoundarySample`, which `0016` added for the reference shim,
      applies through the model instead when one is installed. `HuC6280` gains
      no data members and loses none. Changes nothing when no model is
      registered.

18. `patches/0018-huc6280-idle-fast-forward.patch`
    - The seventh entry on this boundary and the first that is a *capability*
      rather than a replacement. Nothing is taken away from the emulator and
      nothing is computed on its behalf; with no model registered the machine
      is byte-for-byte the machine it was, and the census proves it.
    - It exists for the one thing a routine replacement structurally cannot
      express. A transcript declares its cost when it is offered; a frame-sync
      wait ends when the raster reaches a scanline, so its cost is not known
      then. That is why 45.2% of the census route was still being interpreted
      24 master cycles at a time long after the code around it went native.
    - The split runs down the middle of the decision. *Whether the address is
      a spin that changes nothing* is a claim about a cartridge and is asked
      of the model. *How far the machine may be advanced while nothing
      executes* is a claim about the machine and is answered here: whole
      iterations that end **strictly before** the scheduler's near horizon.
      Never up to it -- leaving the crossing itself to a real instruction is
      what keeps the placement of an event relative to an instruction boundary
      the machine's decision rather than this seam's.
    - The gate is two comparisons against values the processor already holds,
      behind a null check, so an installed model costs a predicted branch per
      instruction and a call only at the watched address. It sits after
      `HookInstructionPC` is assigned and before the opcode fetch, and it does
      not `continue`: a skip advances the clock and then falls through into
      the ordinary instruction, so every hook below it still sees a real
      instruction stream.
    - `dc_beetle_idle_state` hands over registers and two direct pointers --
      the instruction stream at the watched address and the processor's zero
      page, which on a HuC6280 is at logical `$2000` rather than at zero. Both
      come from `FastPageR`, so a model may verify what it is looking at
      without a bus access or a side effect. `code_bytes` bounds the first
      against its own 8 KiB page.
    - **The horizon is only half the bound, and the other half cost a census
      to find.** A request the mask already admits has not been latched yet,
      so the decision at the top of the instruction loop answers "next
      instruction" and the spin looks idle -- but the next instruction's own
      end-of-cycle sample would catch it and the boundary after that would
      take the vector. The first working version skipped past that boundary
      and delivered one interrupt a turn late; `(IRQlow & IRQMask) != 0` is
      the whole fix, and it costs 1,452 skipped iterations out of 21 million.
      One condition says nothing can happen before the horizon, the other says
      nothing has happened already; both are the machine's, which is why both
      are here rather than in the model.
    - What that mistake ruled *out* first is worth keeping, because two
      plausible explanations were wrong: advance granularity and the omitted
      end-of-instruction samples together are not the cause (a per-instruction
      replay of the exact cycles *and* samples diverges identically), and nor
      is proximity to the horizon (a whole iteration of slack diverges too).
      Both were red herrings pointing at the sampling; the fault was in the
      *deciding*, which happens once per pass of the instruction loop and
      which a skip silently spans.
    - Over the 3,600-frame census route the model refuses nothing across
      3,171,424 arrivals, recognises exactly the 3,567 waits, and skips
      21,077,517 of the route's 24,248,940 iterations. The route reproduces
      `9036a9de...` with the capability installed and with it handed back.
      `0017`'s boundary coverage is the sharpest evidence that only time was
      removed: every counter describing what the boundary decided is identical
      either way, and the per-instruction totals fall by 42,155,034 -- two
      instructions per skipped iteration, exactly.
    - Adds no data members to `HuC6280`. Changes nothing for a machine with no
      model registered.

19. `patches/0019-joypad-device-model.patch`
    - The eighth entry, the sixth that is a device model, and the first that
      is an *ownership* change rather than a port. The strobe protocol this
      port speaks has been native since phase 1, inside the cartridge's own
      pad reader at `00:E195`; what it read *through* was still the emulator's
      chain -- a frontend button buffer, five `PCE_Input_Device` objects, a
      multitap counter -- and this is that chain.
    - A registered model owns the whole port. `INPUT_Read` and `INPUT_Write`
      dispatch to it, `INPUT_Frame` is the latch, `INPUT_AdjustTS` the frame
      rebase, `PCEINPUT_Power` the reset, and `PCEINPUT_TransformInput` the
      soft-reset suppression. The five device objects receive nothing while
      one is installed and survive only as the shuttle `INPUT_StateAction`
      writes the emulator's savestate format through.
    - **Where the host's input arrives is deliberately not on this boundary.**
      A model reads it from whoever owns it, which is the frontend; this seam
      carries only the machine's half -- when the latch happens, what is on
      each port, and the two settings that decide how the port answers. The
      consequence is the point: with a model installed nothing in `input.cpp`
      consults `data_ptr` at all, so a host may starve the emulator's input
      path entirely and the machine is unchanged. The Devil's Crush host does
      exactly that, which is what turns "the model is installed" into a
      measurement rather than a claim.
    - Two pieces of configuration are pushed rather than pulled, because the
      emulator reports each exactly once at the load that builds its devices:
      `dc_beetle_set_pad_settings` from `SyncSettings` (multitap, soft-reset
      suppression, and whether a CD unit is attached -- the last being a bit
      of the port's answer rather than a device's) and `dc_beetle_set_pad_port`
      from `PCEINPUT_SetInput`. The boundary remembers both and replays them
      into whichever model is current, so an install may happen either side of
      a ROM load.
    - `PCE_Input_Device` gains two virtuals with no-op bases, overridden by
      the gamepad, so a device that serializes nothing costs nothing. The port
      section's own three fields move through `INPUT_DcExportState` and
      `INPUT_DcImportState`, and the shuttle runs inside `INPUT_StateAction`
      rather than beside it, because the port's state is that file's statics.
    - Over the 3,600-frame census route the model answers 35,991 reads and
      43,188 writes, latches twice a frame, resets the multitap 3,599 times
      and steps it 17,995, and answers 540 reads with a button held -- with
      the emulator's input callback returning zero throughout. The route
      reproduces `9036a9de...` with the model installed and with it handed
      back; feeding the model nothing instead produces a different census
      entirely, which is what makes the match evidence.
    - Adds no data members to any emulator device. Changes nothing for a
      machine with no model registered.

20. `patches/0020-huc6280-replacement-abandonment.patch`
    - The smallest patch in the series and the only one that reports rather
      than does. It tells a host when the core has *given up* on a checkpointed
      transcript, which is the one thing about replacement the host could not
      previously observe.
    - The core already handled the case correctly: a transcript is abandoned
      when the machine does not come back to a resume point in the state the
      transcript declared -- an interrupt handler that returns a register
      changed, or that does not return there at all -- and the original
      assembly then runs from the last checkpoint applied. That is why nothing
      needed telling for as long as transcripts only *replaced* work.
    - A transcript that also *declares* work is the case this exists for.
      Absorbing a return is one: the host stages the promise when the
      transaction is installed and redeems it when the machine lands on the far
      side of the return, because a transaction's writes are delivered by the
      core after its hook returns. An abandoned transcript never lands, and
      before this the promise stayed queued until a later arrival at the same
      address and stack pointer redeemed it against a live call. Over the
      3,600-frame census route that fault reported the wrong `Y` on 82,201 of
      `01:C510`'s 176,861 calls and the wrong `MPR4`/`MPR5` on 42,753, with
      every cycle column still right -- visible in a routine-call trace and in
      nothing else. Withdrawing on the core's own `RTS` was tried first and
      fixed three calls in eight, because a cut-short actor scan usually leaves
      through the dispatch at `01:C52C` and never reaches its return at all.
    - `PopReplacementFrame` gains a `completed` argument and is the single
      place that decides which ending this is: reaching the last checkpoint
      with the transcript still live is completion, and every other way out is
      not. The three call sites say which they are rather than the function
      guessing from state it has already half-unwound.
    - The transcript is named by its host, not by the core. A
      `transcript_token` on the result is stored, saved and restored across
      suspensions, and handed back on abandonment; the core never reads it,
      compares it, or generates one. A savestate load forgets every token it
      restored, because those name transcripts belonging to whichever host
      wrote the state.
    - `dc_beetle_replacement_abandonments` counts every abandonment whether or
      not a hook is listening and whether or not the transcript was named, so
      it measures the core rather than the host's bookkeeping. The census route
      abandons 21,538 transcripts. With the report in place `01:C510` absorbs
      its return: `01:C52B` falls from 73,536 emulated master cycles to 819,
      all 176,861 replaced calls agree with the emulated trace on every column,
      and the state census is unchanged at `9036a9de…`.
    - Adds one 64-bit field to `HuC6280` and one to each of its four suspended
      frames. Changes nothing for a host that registers no hook, and nothing at
      all for a host whose transcripts carry no token.
21. `patches/0021-huc6280-abandonment-reason.patch`
    - Smaller again than `0020`, and the same kind of thing: it adds nothing to
      what the core does and three facts to what it says. The abandonment hook
      gains the resume address, its physical bank, and a reason.
    - The resume address is the point of it. `0020` said *that* a transcript was
      given up and left the host to guess *where*, and the two are not the same
      question: a transcript is installed at a routine's entry and abandoned at
      whichever checkpoint it had reached, so the address the original assembly
      is interpreted from is neither the entry nor anything the host declared.
      It is already in `PC` by the time every abandoning caller reaches
      `PopReplacementFrame`, because applying a checkpoint is what puts it
      there, so the patch reads it rather than passing it.
    - The reason has to be passed, because the three decisions are made in three
      places and look identical once control arrives: the machine came back to a
      resume point carrying a register the checkpoint did not declare
      (`REGISTERS`); a checkpoint asked to advance to an offset the core could
      not deliver (`ADVANCE`); or a checkpoint was asked for when none was left
      (`EXHAUSTED`). Zero is reserved for the ending that is not an abandonment
      at all, so a completed transcript borrows no reason from the enumeration.
    - Which one it is decides what a host can do about it. `REGISTERS` is a
      claim about an interrupt handler and is answered by declaring less;
      `ADVANCE` is a transcript mispricing its own writes; `EXHAUSTED` is the
      core being asked to resume a transcript that has nothing left to say.
      Counting them apart is what turns 21,657 abandonments from a number into
      a list of addresses to work on. On the census route every one of the
      21,657 is `REGISTERS`, so the other two are reported and never seen.
    - A `detail` byte carries what the reason has to add, which for `REGISTERS`
      is the set of registers that disagreed. The comparison it comes from used
      to stop at the first mismatch, because a `bool` was all it produced; it
      now tests all five so that the byte is the whole answer rather than the
      earliest one. `PC` and `S` are absent by construction -- they are how a
      resume point is recognised, so a machine that comes back with either
      changed does not reach this comparison at all.
    - Changes no signature a host outside this fork uses, because the only one
      it changes is the abandonment hook `0020` introduced. A host that
      registers no hook is unaffected; the counter is unchanged.
22. `patches/0022-huc6280-break-flag-resume.patch`
    - Stops the resume comparison testing a flag that has no meaning in the
      live register, which is what `0021`'s census found every one of the
      census route's 21,657 abandonments to be. Afterwards the route abandons
      nothing at all.
    - B is set in `P` by any `PHP`/`PLP` pair, because `PHP` pushes
      `P | B_FLAG` and `PLP` pops the byte back whole. It is cleared again by
      any interrupt, because that push forces it off and `RTI` restores what
      was pushed. So a transcript spanning a `PLP` declares B set, the handler
      that interrupts it hands the machine back with B clear, and the core gave
      the transcript up over the disagreement.
    - Nothing on this processor reads it. The two instructions that push `P`
      force the bit themselves in opposite directions, and the only other
      reader of `P` is `REDOPIMCACHE`, which caches `I`. That is what makes
      ignoring it exact rather than approximate: this is not a tolerance, it is
      the observation that the comparison was testing a bit no execution can
      depend on.
    - Applying a checkpoint leaves the machine's own B standing instead of
      writing the transcript's, so a modelling artifact cannot reach the
      register the emulator saves.
    - Confined to the checkpointed path. An atomic transcript is applied whole
      inside one loop iteration, is never resumed and never abandoned, and is
      untouched here.
    - The state census over the 3,600-frame route is byte-identical either side
      of this patch, and the eight `absorbed_return_trace` regressions -- which
      compare an emulated route against a replaced one inside the same run
      rather than against a stored hash -- pass. The host drops B from the
      status it reports and from the status it declares, for the same reason
      this patch stops comparing it; a transcript models `PHP`/`PLP` exactly
      and so cannot help carrying a bit the interrupt it cannot see would have
      cleared.
23. `patches/0023-huc6280-interrupt-flag-sample.patch`
    - Lets a checkpoint describe an `SEI`, which is the one instruction it
      could not. The checkpoint gains `samples_interrupt_flag_clear`; when it
      is set the core applies the mask clear across `DcSampleInterrupt` and
      writes the retired status immediately afterwards.
    - The ambiguity is real rather than a modelling shortcut. `LastCycle` runs
      before the opcode's writeback, so an `SEI` samples the window it is
      about to close: an interrupt already pending is taken at that boundary
      and pushes the `P` the `SEI` just wrote. One status byte answers both
      questions for every other instruction and cannot answer both for this
      one -- publishing the open mask corrupts the pushed byte, publishing the
      closed one suppresses an interrupt the hardware takes.
    - A mask clear needs nothing here, and that asymmetry is the reason this
      patch is only about `SEI`. A transcript expresses `CLI` and `PLP` by
      publishing the old mask on the instruction and the new one on the
      checkpoint after it, and both answers come out right from the one byte,
      because a boundary that closes no window has nothing to disagree about.
    - Additive and inert by default. A checkpoint that leaves the field zero
      takes exactly the path it took before, which is every checkpoint the
      fork's hosts produced until now; the host refuses at the boundary any
      checkpoint that claims the clear sample without setting the mask.
    - The state census over the 3,600-frame route is byte-identical either
      side of this patch, as is a 4,205,705-write trace of the whole stack
      page `$2100-21FF` -- which is where a wrong answer would land, since
      both readings differ only in the byte an interrupt pushes. The count of
      interrupt-boundary decisions is unchanged at 66,291,003, so the machine
      judged its interrupts at the same points either way.

37. `patches/0037-huc6280-execution-accounting.patch`
    - Observes exclusive divided clock ownership, actual opcode dispatches, IRQ entry, transcript installation and retained/rejected core offers. Nested device steals restore their caller ownership. Counters are diagnostic and never serialized.
    - Moves interpreter-profile native classification to the actual core validation point, so retained-capability opcode execution is visible again. Changes no instruction, stack write, device effect or interrupt sample.
    - See [execution accounting](../../docs/EXECUTION_ACCOUNTING.md) for measurement definitions and verification.

38. `patches/0038-huc6280-native-interrupt-entry.patch`
    - Routes all maskable IRQ entries through the native CPU sequence when the native boundary is installed. The adapter supplies live registers, bus access, clock advances, mask publication and sampling; the native routine owns entry ordering, stack arithmetic and flag updates.
    - Keeps original entry available with `--emulated-boundary`. Native and core entry counts are separate, and native entry advances are translated ownership with device stalls still counted exclusively.
    - See [native IRQ entry](../../docs/NATIVE_INTERRUPT_ENTRY.md) for timing, full-corpus measurements and regression evidence.

All local files and patches are GPL-3.0-or-later. Upstream per-file notices
and licenses remain authoritative for upstream sources.
