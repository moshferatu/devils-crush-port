# Native Runtime Migration

## Goal

Finish the port as a human-readable C++ game runtime. A completed normal-play
frame must not execute HuC6280 gameplay code. The existing emulator and native
replacement registry remain the behavioral oracle while this work is in
progress, and may remain available as an optional compatibility mode.

Native code should describe the game, not the instruction stream. Prefer
`update_ball`, `resolve_collision`, `advance_actor`, and `apply_score_event`
over CPU registers, program counters, or copied branch labels. Exact helpers
are acceptable at a narrow boundary when they establish behavior, but they are
private implementation details rather than the public model.

## Architecture

Add a selectable native implementation of `PceRuntime`. It consumes the same
`InputFrame`, supports the existing save/capture interface, and returns the
same framebuffer/audio frame contract as the emulated runtime.

Its core uses named state and domain boundaries:

- `GameState`: scene, frame timing, RNG, player/session state, and shared
  table state.
- `BallState` and `PhysicsState`: position, velocity, collision/contact, and
  tilt state.
- `ActorState`: typed actor kind, slot lifecycle, animation, collision, and
  behavior state.
- `RulesState`: targets, gates, bonuses, scoring, ball transitions, and
  bonus-stage state.
- Renderer and audio schedulers: semantic events and ROM-derived assets, not
  emulated device writes as their primary interface.

Raw work-RAM-compatible views may exist at the adapter boundary while a domain
is being migrated. Do not expose raw offsets as the native domain API.

## Live bootstrap shadow

`--native-gameplay-bootstrap-shadow` is the first opt-in bridge into the
playable executable. At each completed interactive frame it captures the
emulator's work-RAM image through `GameStateView`, builds a fresh
`NativeGameplayRuntime`, verifies that its ball, score, and table-rule state
exactly retain the accepted seed, then runs one private candidate frame against
the cached decoded playfield map. The report distinguishes candidate clear
completions from collision-resolver boundaries and records their native
actor/audio handoffs. The shadow is deliberately read-only: the emulator
continues to own simulation, collision resolution, presentation, and audio
while the native frame adapter is incomplete.

The per-session report distinguishes normal unavailable seeds (for example,
before a ball and score are both live) from invalid work-RAM images, rejected
bootstraps, and seed mismatches. This validates the live capture boundary
without concealing an incomplete native frame behind a generic fallback.

The same read-only mode also observes the exact `01:CD74` entry. Its live MPR
snapshot and typed work-RAM table pointers decode and cache the selected
playfield collision map, while `GameStateView::ball_motion_tuning` captures
DD09's signed drag divisors and gravity. The report distinguishes frame
boundaries that have all clear-physics inputs from ones that precede a map
probe. It separately captures the exact entry and completion of `00:F1B4`
(one ball-position substep) and `01:DD09` (drag/gravity), comparing each
human-readable native result directly with the live ball record. F1B4's
adapter keeps DC95's transient signed 8.8 increment separate from the frame
velocity, so high-speed two- and four-substep motion is checked accurately.
It also composes the live `DC95 → F1B4* → DD09` path: a session report counts
only epochs whose split plan, every position integration, and final
drag/gravity output remain exactly on the human-readable no-contact path.
Any state-changing collision or control route is reported as such, rather than
being mislabeled as a native mismatch. The collision-response owner remains
deferred. This remains a shadow: no native result is written back to the
emulator.

The same report now retains the boundary of each `CB2A` collision sweep. Its
actor scan must finish before the ROM can enter `CD74`, and the shadow records
that probe's returned carry as an exact solid/clear result before classifying
the later route. This is observational only: it identifies the next native
collision-owner seam without assigning a generic bounce or target effect.
The bridge now also runs a dedicated readable normal `CB48`/`CC1A` actor scan
at that exact `CB2A` boundary. It preserves the live cursor, countdown, and
zero-only scan gate rather than reusing CED2's fixed dynamic-slot policy, then
uses CB48's staged `$65:$67` collision probe, actor records, and MPR snapshot
for geometry/mask work. That substep probe is deliberately distinct from the
persisted ball record used for physics-epoch bootstrap.
Its report compares the typed actor-solid result with the source's
actor-versus-`CD74` boundary. Only after that comparison is stable may the
native C6/C7 physics loop take actor-first scan ownership; target responses,
starting with the Devil Shooter catch/retraction path, remain separate named
behavior work.

`CBAB` is not folded into that broader-sweep verdict. It is a nested,
caller-selected single-slot query and can confirm an actor before the outer
route later reaches `CD74`. The shadow records its next `CC1A` scan, then
uses CBAB's direct `CBC2` carry handoff to retain only probes that actually
flow into `CBCF`. This prevents a later broad-`CB48` `CBCF` contact from being
mistakenly paired with a stale flipper probe, while keeping a real one-slot
contact available for the future typed flipper-collision owner.

For a solid playfield probe, the shadow also records the following `CF33`
entry's shared selector and associates its later rearm or damping exit with
that exact signed-CF3F or clear-selector-CF5B leg. For signed selectors it
also reports the ball collision-marker split: marked entries hand off to CDF8,
while unmarked entries take CF3F. The result is still only a trace contract:
the existing readable response kernel is not allowed to claim playfield
ownership until the live counts establish which leg and exit belong to each
table-contact case. A `CF33` leg that returns directly to the next substep is
reported separately; it is neither a rearm nor a damped contact response.

`NativeGameplayState::resolve_cf33_playfield_contact` now evaluates the
confirmed-solid playfield seam without claiming its enclosing frame. It
captures the ball's actor-local collision marker and applies CF33's actual
split: a signed selector reaches CDF8 only while that marker is set; an
unmarked signed selector continues through CF3F's normal reflected response.
It otherwise returns CF5B's separating continuation or its resolved oriented
ball in named form. CF5B's and unmarked-CF3F's proven outcomes resume the
retained C6/C7 cursor and release actor/audio work only after DD09. The marked
CDF8's marked clear arm is now a proven same-frame cursor transition, not a
completed frame. The first live shadow corpus observed seven CDF8 entries, all
with `$77.3` clear; a later cabinet session also observed the first
`$77.3`-set consuming entry. The shadow cross-tabs each CDF8 entry against the
active signed playfield `CF33` marker, keeping marked handoffs, impossible
unmarked handoffs, and other CDF8 callers separate. CDF8 restores F1B4's cached
position internally, applies its selected clear placement, and returns through
CB7E, which clears the collision marker before C6/C7 retries the unconsumed
substep at that accepted position. A long cabinet session exercised 18 marked,
clear-arm CDF8 handoffs: all 18 retained their cached fractional coordinates,
moved away from the cache by the accepted whole-pixel placement, cleared the
marker, and re-entered `DC95`. The consuming arm still requires parent-route
attribution before it becomes a native transition.

The bootstrap shadow also retains every clear result and compares both its ball
state and its expected `BallFrameSchedule` owner at the next emulator frame
boundary. Owner departures are counted by their actual named owner, so a
normal-motion candidate that hands control to table, launch, or off-table code
is not conflated with a same-owner physics divergence. That host-boundary
comparison is a diagnostic, not a correctness gate: a host frame may begin or
end in the middle of the ball coroutine, and may therefore contain zero or several
`DC95`, `F1B4`, or `DD09` entries. The direct `DC95 -> F1B4* -> DD09`
collision-loop shadow is the physics-epoch proof: it counts only epochs whose
split plan, every position integration, and final drag/gravity state match the
live no-contact path. `NativeGameplayRuntime::advance_physics_epoch` now names
that contract explicitly; an eventual native scheduler must call it at a
recovered physics-epoch boundary, never simply once per emulator frame.

The shadow also carries a separate private `NativeGameplayRuntime` across
successive live `DC95` entries. After a clear native epoch, its completed ball
state is compared with the next actual `DC95` seed before it advances again;
a mismatch discards only that diagnostic chain and reboots from the live
state. Solid contacts deliberately end the chain at the existing CF33/CDF8
resolver boundary. This directly measures whether readable state can survive
several real physics epochs, independent of the presentation-frame cadence.
While a clear chain is pending, the shadow also records each source `C52C`
actor dispatch by its typed handler address. Divergent chains report
those handler counts against their total observed entries, identifying the
actor behaviors that must be made state-owning before native epochs can retain
their result across the source actor pass.

`BallFrameSchedule` decodes slot-zero's continuation into normal motion, the
initial C6C2 substep transition, table-control wait, or off-table transition.
The latter three stay deferred instead of being sent through DC95 physics, so a
different ball coroutine is not mistaken for a failed normal-physics step. The
initial transition retains C708's exact unsigned X=$00F0 gate: the high-side
arm returns to its caller, while the low-side arm clears its collision
selectors, installs C721, and tail-enters DC95. That decision is captured in
named form only when C6C2's retained cursor is already complete; otherwise the
source must first finish the cursor, because its movement can change the gate
input. It remains deferred until the initial owner executes that cursor and
scheduler handoff. C6C2 and C721 captures now also retain the coherent
DC95-prepared cursor: its signed 8.8 increment plus the completed and requested
substep counts. `advance_prepared_ball_motion_substeps` can therefore finish a
clear suffix without re-splitting the captured velocity or inventing a host
frame boundary. `begin_prepared_ball_collision_loop` now gives the same cursor
to the existing C6/C7 contact-loop owner, retaining its source substep index
and sixteen-trip cap while it uses the captured increment rather than a new
velocity-derived split. `NativeGameplayState::resume_scheduled_ball_motion`
now makes that capture-to-runtime decision explicit: the resumed C721
normal-motion owner and readable C6C2 suffix with coherent cursors enter their
respective prepared loops; C6C2's scheduler intent remains distinct from an
already-installed C721 owner, while table/off-table owners stay deferred and an
incoherent normal-motion capture is unavailable. The underlying
`resume_prepared_ball_collision_loop` sends a captured C721 suffix through the
retained playfield-contact/DD09 owner and keeps a solid result pending for
CF33/CDF8. `resume_initial_ball_motion` now carries C6C2's clear suffix through
the same retained loop, DD09, its fresh DC95 split, and the post-loop gate in
that source order. It deliberately returns the resulting scheduler intent
rather than writing C721 or returning through the emulator's actor scheduler;
the typed `BallFrameSchedule` now owns those actions: it carries the ball's
collision-shape directory/phase selection, clears both on the C721 arm, and
retains them on the caller-return arm. The caller-return arm is now known to
be slot zero's completed-handler path: C520 calls C606, whose slot-zero guard
immediately returns without filing a presentation queue. `NativeGameplayRuntime`
records that typed no-op and retains the next C6C2/C721 schedule; it does not
yet advance the wider actor scan, presentation frame, or enable player
execution.

## Implemented foundation

The first migration slice establishes two reusable pieces without pretending
that the current game is already native:

- `native_runtime::GameStateView` decodes the verified actor scheduler fields
  from an exact work-RAM image into typed actor records. It names activity,
  attributes, collision state, handler identity, and signed horizontal and
  vertical 8.8 motion while keeping raw offsets private to the adapter.
- `GameStateView::ball` keeps the dedicated slot-zero ball record distinct
  from ordinary actor coordinates: it preserves both signed three-byte 16.8
  world positions, signed 8.8 velocity, and the ball's physics flags.
  `capture_active_ball` transfers that complete active observation into
  `BallState`, so native physics can be seeded without dropping a fractional
  coordinate byte or interpreting raw work-RAM addresses.
- `GameStateView::active_score` decodes the selected player's 24-bit binary
  score accumulator and presentation-dirty latch. `capture_active_score`
  transfers that observation to `ScoreState`, leaving player selection and
  display policy outside the native score owner.
- `GameStateView` also decodes the live pointers to the playfield's tile,
  collision-attribute, and mask planes. A future ROM-mapping adapter will use
  them to seed `CollisionMapAsset`; native physics and rules do not consume
  the pointers directly.
- `native_runtime::MappedRomView` and the mapped `CollisionMapAsset` decoder
  now form that ROM-mapping adapter. They isolate logical addresses and MPR
  mirroring at the asset boundary; table configuration still supplies each
  selected map's row and mask counts.
- `capture_playfield_collision_source` records the currently active MPR map
  with those live pointers at the emulator's playfield-probe boundary. It is
  the evidence adapter used to derive and validate table configurations from
  captures, not part of the native gameplay loop.

To collect that evidence from a deterministic replay, request the playfield
probe explicitly and write the distinct sources, for example:

`devils-crush --replay-session out/player-session-6/inputs.csv --observe-hook 01:CD74 --routine-trace out/player-session-6/cd74.entries.csv --playfield-collision-sources out/player-session-6/collision-sources.csv`

This is a diagnostic extraction mode: it replays the saved input/state but
does not assert the capture's stored work-RAM hashes. Certification replays
continue to assert them normally.

The current main-table capture has one source: tile IDs at `ABC6`, attributes
at `A006`, and masks at `B686` under MPR page 5 bank `43`. The tile/attribute
plane separation is 3,008 bytes (94 rows × 32 columns); the attribute plane's
highest value is 249, giving the main table 250 eight-row masks. These named
dimensions are now `kMainPlayfieldCollisionMapRows` and
`kMainPlayfieldCollisionMaskCount`; `decode_main_playfield_collision_map`
combines them with a live pointer record and `MappedRomView` to produce the
owned collision asset.
- `native_runtime::ActorSchedulerView` turns that fixed actor table into an
  ordered list of actors due for an update. It preserves the observed
  scheduling policy while making the next step a named actor-behaviour call,
  not a computed jump through ROM code. It also exposes the first-free
  dynamic-slot reservation decision and its named reservation mode, while
  leaving mutation to the future native actor-state owner.
- `file_actor_for_presentation` owns the recovered post-handler `01:C606`
  filing decision. It retains the entry-attribute camera-bit eligibility gate,
  three bounded SAT queues, bit-zero's
  family-owned prequeue presentation-refresh dependency, the fixed slot-zero presentation
  seam, and the driver's reverse/reverse/forward drain order. Dynamic handler
  execution remains a separate native scheduler responsibility, so this does
  not pretend that the composite `01:C539` loop is already fully ported.
  `ActorPostHandlerPresentation` is the explicit seam between those owners:
  it records the attribute value sampled before the refresh and the value C606
  rereads afterward. The recovered F605 helper recalculates transformed
  presentation coordinates and clears a visibility flag before this reread,
  so the future native handler result can enter the real queueing path without
  a ROM-vector-shaped dispatcher.
  `capture_actor_post_handler_presentation` now derives that exact typed
  result from verified before/after handler-boundary snapshots. It remains a
  pure observation adapter, so it cannot advance a C539 pass from a guessed
  final attribute value. `capture_scheduled_actor_handler_boundary` further
  requires the entire before-snapshot actor record to equal C539's decoded
  scheduled record, preventing a reused slot from another pass from joining
  the wrong native behavior observation. The capture-preserving
  `join_native_gameplay_actor_behavior_boundary` retains that full-record
  equality through the behavior/presentation join, rejecting same-slot handler
  or attribute drift before a shadow completion can be replayed. `ShortActorPass`
  repeats that full-record check against its current cursor before C606-equivalent
  filing can advance, keeping a manually assembled or stale completion from
  crossing a scheduler boundary.
- `ActorSchedulerView::begin_short_actor_pass` now names the reusable C539
  entry contract: it resets the presentation queues and completed ball-substep
  count, while `ActorFrameGate` preserves the inherited actor-enable mask or
  clears it when the frame is frozen. The composite dispatch and its dynamic
  handlers remain separate native behavior owners.
  `capture_short_actor_pass` packages that reset seed with the ordered due
  actors from the same read-only state view, giving future capture wiring one
  coherent shadow input rather than independently sampled pieces. The existing
  gameplay-bootstrap diagnostic reaches that bundle through a dedicated
  observe-only C539 entry hook, with probes at both direct JSR callers so an
  unselected short pass is not confused with a capture gap. It reports
  coverage only; it neither dispatches a native behavior nor changes the
  emulator's scheduler ownership.
- The ordinary C510 walk exposes a more frequently selected, narrower
  observation boundary: C52C dispatches a dynamic handler, returns at C520,
  then enters C606 with the handler's final attributes. The gameplay shadow
  now retains the C52C entry slot until its observed completion and accepts a
  C606 successor only when its X register and actor attribute still match.
  C606 calls without that predecessor (including the slot-zero route) are
  deliberately reported as unrelated rather than forced into the dynamic
  handoff. This verifies a real scheduler/presentation seam without claiming
  either the C510 scan or a ROM-selected actor behavior.
- Each accepted C52C/C606 pair is additionally counted by the handler vector
  present at C52C entry. That pre-dispatch identity is intentional: a dynamic
  handler may rewrite its own vector before C606 sees the actor again. The
  256-entry diagnostic reports its top 16 handler counts and is still
  read-only, recording only strictly paired observations. It identifies an
  active behavior family without treating the C510 scan, C52C dispatch, or
  that handler as natively owned.
- The same C52C completion now compares the pre-dispatch and post-handler
  typed `ActorSlot` for that same slot. Its compact state-effect record names
  activity, attributes, behavior counter, handler vector, horizontal/vertical
  motion, and the coupled collision domain. The shadow reports the eight
  highest-frequency handlers with those effect counts. This remains
  observation-only: it provides the evidence needed to choose one coherent
  behavior-family owner rather than promoting a handler merely because it is
  frequently called.
- The same strictly paired C606 calls now bracket the recovered filing model.
  `GameStateView::ActorPresentationQueueImage` captures the auxiliary queue,
  the shared primary queue, and all three source cursors at C606 entry;
  `capture_actor_presentation_queue_state` normalizes its upward/upward/
  downward storage into the readable queue state. At C606 completion the
  shadow combines the entry attribute with the post-F605 reread attribute,
  runs `file_completed_actor_for_presentation`, and compares only the final
  queue state. This certifies the queue effect without suppressing F605,
  writing RAM, or taking scheduler/presentation ownership.
- With each C606 filing proven in isolation, the gameplay shadow also brackets
  the enclosing C510 scan. C510 is a loop head, so its zero-slot arrival
  supplies the reset queue image. `ActorSchedulerView::scan_main_actor_run`
  recovers each native transcript's contiguous skipped slots and C52C target
  (or terminal exit), so the shadow checks that readable target directly. The
  dispatch is charged only when that C52C invocation returns at
  C520; nested C52C entries inside its handler therefore cannot consume the
  pending C510 scan. C510 is also its own loop-back target, so the registry
  records an observer call for every loop head; the `C52B` RTS closes all of
  those same-stack records together. Only the X=0 record began the aggregate
  pass, so only its `C52B` completion closes the bracket; later loop-head
  completions and inner C52C/C606 transcript closures leave the live pass
  intact. `MainActorPass` accepts a scan run only at that live boundary and
  requires its matching post-handler C606 filing before it can accept the next
  run, so handler mutations of later actor records remain visible to the
  readable owner. The terminal boundary compares its aggregate final queue
  state. A
  same-slot disagreement preserves both gate-byte snapshots and a bounded
  read-only work-RAM census of the instruction that wrote either pending gate
  between C510 and C52C. This is a pass-level oracle only: the ROM scheduler,
  dynamic handlers, F605 refresh, and presentation path still execute live.
- `ShortActorPassPresentation` owns C606's filing state for one C539-started
  pass. It accepts one completed named-handler observation at a time and
  carries the resulting queues to the next slot, without claiming the
  still-unported ROM dispatch or treating a frozen pass as an empty pass.
- `ShortActorPass` now supplies the first typed behavior-dispatch boundary.
  It presents only the next decoded actor record to an injected named behavior
  owner, advances only after that owner returns a matching post-handler
  presentation result, and leaves the pass untouched when a family is not yet
  available. This is shadow-only infrastructure: no ROM handler address is a
  native dispatch key and no live C539 replacement is installed.
  Its prefix-run result reports completed native slots and retains the first
  blocked actor, giving shadow validation a coverage metric without allowing
  it to skip an unported family.
- `NativeGameplayState::advance_actor_phase_slot` now exposes the same named
  behavior ordering as the full recovered phase, but for exactly one selected
  slot. The full-phase API delegates to the shared range implementation, so
  its existing slot-order semantics remain the oracle while a future dispatcher
  gains a non-ROM-address behavior entry point. This is still shadow-only and
  is not connected to a live C539 registry replacement.
- `NativeGameplayActorBehaviorDispatcher` joins a C539-decoded actor record
  to that one-slot native behavior operation only when the native state was
  already seeded for the same slot. Its result deliberately excludes C606
  presentation filing: post-handler attributes remain a captured dependency,
  so this bridge cannot accidentally treat the pre-dispatch attribute byte as
  the final presentation state. `join_native_gameplay_actor_behavior_presentation`
  now accepts that capture only when its slot agrees with both the decoded
  actor and the native update, retaining the final C606 handoff as a strict
  shadow contract rather than an inferred completion.
  `ShortActorPass::complete_native_gameplay_behavior` then requires the pass
  cursor to agree with that same slot before using the captured result for
  C606 filing; it remains an observation-completion path, not a live behavior
  dispatcher. Its completion-replay helper validates an ordered capture
  prefix, retaining the first mismatch and any surplus observations rather
  than letting a later actor consume them. When an observed completion fails
  at the current cursor, the replay result also retains that rejected typed
  completion beside the blocked actor, making shadow divergence diagnosable
  without granting a live scheduler path any new authority. Its typed block
  reason separately identifies an exhausted capture prefix (awaiting the next
  completion) from a present but invalid completion, so a coverage gap is not
  misreported as identity divergence. Multi-actor replay coverage also proves
  that a later capture cannot skip an unobserved middle actor. A completed
  replay also retains its first surplus completion, keeping adjacent-pass
  capture data explicit without classifying it as a current-pass failure. The
  replay result validates these terminal forms as disjoint, so capture wiring
  can reject contradictory diagnostic state before it reaches reporting.
- `ActorSchedulerView` also names the later collision-retry full scanner's
  exact slot ordering and attribute eligibility. It supplies dynamic actor
  candidates 4 through 127 to future typed geometry, without treating the
  update-activity flag as a collision decision. Its vertical-scan adapter
  retains only the candidates within the recovered signed 127-pixel window,
  while taking the not-yet-certified 16-bit scan source explicitly and leaving
  descriptor, mask, and contact decisions to their later native stages.
- `BonusStage4State` now owns the three fixed eight-hit objectives reached
  after the Giant Face's captured-ball handoff. Its strict stable capture
  accepts only the three steady objective records, then its scheduler delivers
  confirmed contacts at their fixed actor turns. Each hit preserves its
  100-unit `$2E` event; a terminal hit also applies the 1,000-unit destruction
  award and requests its separate presentation owner. Clearing all three
  records produces the proven multiplier-20 result handoff without guessing
  the shared animation or award-transfer cadence.
- `BonusStage4LinkedTargetState` names the paired-source handoff once the
  shared allocator has selected a dynamic record. It retains the copied
  objective identity, the wrapped sixteen-pixel source offset, the target's
  100-unit `$3B` contact, and its exactly four orbiting-child requests. Those
  requests are deliberately deferred: the contact sets appearance four and
  state selector six, which the live trace proves installs `$6C21`; only its
  scheduler-resumed `$6C24` continuation exposes the four allocator requests.
  That continuation keeps the target collision-latched until the fourth child
  has been allocated, then rearms the target; the native state model therefore
  rejects an intervening second contact instead of inventing a duplicate spawn.
  Each
  allocator-selected child has its own 100-unit release contact and recovered
  four-way initial orbit phase (24/16/8/0) and signed 32-entry orbit-offset
  asset. Clear child turns advance that phase modulo 32 and reposition from
  that table. A cleared parent liveness marker and a child collision both
  take the recovered common 100-unit release continuation, with distinct
  typed dispositions rather than a guessed cleanup. Its
  one-to-32-tick target timer now requests a new shared-RNG vector on expiry;
  shared-vector lookup and integration, target-hit animation, and allocator timing remain intentionally
  separate owners until their live capture boundaries are established.
  `capture_bonus_stage_4_linked_target` now provides that boundary for exactly
  one pre-contact target or the proven pending-hit handoff: it accepts the
  observed `$6B7E`/`$6CA0` dispatches, or only `$6C21` with appearance four and
  a still-latched contact bit, plus their copied source/objective pair and the
  source's live-target marker. It refuses other target-hit animation and
  ambiguous dynamic-target sets.
  `capture_bonus_stage_4_orbiting_child` similarly admits only a clear `$6D27`
  child whose type-$C2 parent is at `$6CA0`, with normalized phase and exact
  parent-relative table position; its hit and parent-loss transitions remain
  separate from that steady-state capture seam.
  `BonusStage4LinkedTargetRuleScheduler` now carries a strictly captured
  dynamic target through the normal native contact queue at its allocator
  slot, emits its `$3B` event in actor order, and retains the later
  presentation completion and allocator-selected child claims as explicit
  native runtime seams. Its clear actor turns also now advance the recovered
  one-to-32-tick motion countdown and expose a typed random-vector request
  at expiry; shared vector selection/integration remains a separate owner.
  Claimed orbiting children are retained by that scheduler at their selected
  dynamic slots: clear child turns advance their recovered orbit table, while
  a confirmed child contact takes `$6D7F`'s score-and-release path without
  fabricating an audio event. `capture_bonus_stage_4_linked_target_generation`
  now restores a strict target together with every clear `$6D27` child tied
  to it, so a post-`$6C24` capture seeds one coherent native generation.
  Pending-contact and parent-loss child transitions remain unavailable until
  their owning transitions are native.
  `NativeGameplayBootstrap` transfers this target
  independently of the three fixed objectives, so a valid dynamic record is
  not discarded merely because another BS-4 family is unavailable.
- `native_runtime::advance_actor_motion` is the first state-owning native
  behavior. It advances a non-ball actor's optional horizontal and vertical
  motion, steers its signed velocity toward named targets, and reports an exit
  from the 320-by-720 playfield. Its input and output are typed world state;
  no handler vector, register state, or work-RAM write is part of the API.
- `native_runtime::ActorState` now owns the fixed native actor records. It
  claims and releases dynamic slots, keeps the due-update lifecycle, and
  applies named behaviors in slot order. A later adapter will seed it from
  `ActorSchedulerView`; normal native frames will mutate `ActorState` only.
- `capture_shared_actor_motion` is the generic capture boundary for active
  actor positions, signed velocity, and the scheduler's due decision. It
  converts the typed snapshot into native 24.8 motion state without assuming
  a handler's integration, steering, collision, or family rule policy.
- `native_runtime::advance_ball_motion` owns the ball's signed 16.8 position
  integration. It keeps the cartridge's 24-bit coordinate wrap and
  fractional-only motion marker, but exposes those as `BallState` rather than
  a six-byte cache and individual work-RAM writes.
- `native_runtime::rollback_ball_position` owns the exact complementary
  position-only rollback after a rejected collision step. It restores the two
  16.8 coordinates while deliberately retaining the resolver's velocity and
  motion flags; normal derivation and response selection remain separate.
- `native_runtime::retry_ball_collision` owns the next recovered retry
  operation: classify the incoming motion signs, roll back to the pre-impact
  position, then apply one typed whole-pixel offset while retaining fractional
  coordinates and response state. `collision_retry_asset` decodes the proven
  four-vector table at `01:CEFB` through the active MPR mapping; the following
  retry probe now has a typed clear/full-scan handoff. `NativeGameplayState`
  commits only the proven clear outcome after its owning resolver selects this
  branch, while retaining a full-scan retry exactly once for the actor/playfield
  resolver; the full geometry scanner remains a separate resolver stage.
- `native_runtime::split_ball_motion_substeps` owns the collision loop's
  adaptive one-, two-, or four-step plan. It retains the cartridge's signed
  truncation for per-step motion, so a fast ball is tested between position
  increments without exposing staged scratch bytes.
- `native_runtime::advance_ball_motion_substeps` owns the unobstructed path
  through that loop. It applies each planned increment but retains the frame
  velocity for later damping; collision detection will be inserted between
  those explicit native steps.
- `native_runtime::apply_ball_drag_and_gravity` owns the following ball
  velocity update: signed drag on each moving axis and vertical gravity. It
  validates the otherwise-invalid zero-divisor state without exposing the
  cartridge's scratch bytes to native callers.
- `native_runtime::apply_collision_response` owns one signed 8.8 collision
  velocity response. It names normal, reduced, half, and minimal response
  families, including the original asymmetric normal-mode cap, independently
  of work-RAM offsets or a routine return address.
- `native_runtime::apply_oriented_collision_response` now owns CF33's
  readable response kernel: project a ball vector into a selected contact
  basis, run the local normal through the recovered response family, then
  project it back to table coordinates. Its named pre- or post-response
  reflection preserves the distinct signed CF3F and clear-selector CF5B legs,
  rather than assuming their asymmetric normal tuning is interchangeable.
  Target geometry still owns selection of that basis and response family, so
  this does not infer a generic tile normal or fabricate a target rule.
- `decode_collision_response_basis` is the ROM adapter for CF33's D081
  geometry lookup. It decodes the low-five-bit contact orientation through
  the mapped `01:D2AE` coefficient table into a signed 8.8 basis, retaining
  the source orientation and selected table index as evidence.
- `GameStateView::collision_response_input` captures CF33's shared `$78`
  selector together with `collision_response_table_state`, while
  `make_collision_response_config` recovers CFD8's remaining selector inputs:
  orientation bits 5--6 choose normal/reduced/half/minimal, and named table
  latches supply normal-to-reduced promotion, reduced quarter scale, and
  ordinary-response tuning. This is deliberately distinct from the
  actor-local collision-table orientation at `$1D00,X`; the target-contact
  owner can now obtain the exact shared response input without exposing raw
  work-RAM offsets to gameplay.
- `NativeGameplayState::confirm_oriented_actor_contact` is the integration
  boundary for that kernel. A geometry-owning target supplies the selected
  basis/config; its asset overload reads the captured CF33 response input.
  A newly dispatched contact updates the native ball exactly once before its
  normal actor-rule queue is established. The signed leg requires an unmarked
  target and negates before CFD8; the clear-selector CF5B leg responds only to
  a nonnegative projected normal and negates after CFD8. Repeated or rejected
  contacts cannot apply another response. CF5B's separating marker-clear
  return, CDF8 retry, unavailable mapped ROM, or unavailable captured response
  state leaves gameplay unchanged.
- `make_confirmed_contact_target` captures CBCF's target handoff: only an
  active overlapping actor yields a target, and its low six-bit active type
  becomes the response kind (`$82` selects CF33's kind 2). This deliberately
  leaves the retry scan's later dispatch handoff unconnected until a natural
  CED2-to-CBCF trace establishes that ownership.
- `NativeGameplayState::confirm_cf33_actor_contact` is the corresponding
  source-specific contact seam: it consumes the captured active type-`$82`
  actor plus the shared response input and routes the recovered signed or
  clear response leg through CF33. Marked signed targets and other actor types
  remain unchanged for their own CBCF response owner.
- `NativeGameplayState::resolve_cf33_actor_contact` makes that marked signed
  branch explicit: it returns a typed `position_retry_ready` handoff carrying
  the live impact ball and F1B4's prior position for CDF8. The existing
  response-only entrypoint remains a compatibility adapter; neither API
  aliases this actor-contact handoff to the playfield pending-contact state.
- `NativeGameplayState::begin_actor_contact_collision_retry` now owns that
  CDF8 handoff through the same named placement loop and CED2 scan boundary as
  a playfield retry, but with independent pending state. A clear candidate
  commits only the actor-contact retry's ball result; it cannot clear or
  satisfy a playfield contact. Its CED2 scan derives the preserved parent Y
  high byte from the captured live impact ball, rather than accepting an
  untyped scratch-byte argument.
- `GameStateView::ball_collision_position_cache` and
  `capture_collision_retry_ball` now recover CDF8's actual input pair: the
  live confirmed-impact ball plus F1B4's pre-integration signed 16.8 position
  cache. This prepares the marked signed CF33 retry handoff without pretending
  that an actor-contact retry is the existing playfield-only pending contact.
- `native_runtime::evaluate_collision_bounds` owns the collision sweep's
  typed fast rejection window. An intersecting result is explicitly only a
  mask-lookup candidate, keeping geometry, mask data, and target behavior as
  separate native concerns.
- `evaluate_actor_vertical_proximity` names the actor scan's first geometry
  rejection as a signed 127-pixel vertical window. It preserves the exact
  inclusive near range while leaving descriptor selection and mask lookup to
  later actor-collision stages.
- `prepare_actor_collision_geometry` converts a decoded four-byte actor shape
  descriptor into its typed collision window after that vertical gate. It
  retains the table's 32-pixel ball bias and one-byte horizontal-world
  rejection, while ROM directory selection, final bounds testing, masks, and
  contact rules remain separate native stages.
- `resolve_actor_collision_shape` now owns the ROM-side directory chain for
  that descriptor: four captured directories select two phase tables, whose
  primary pointer selects the shape. The mapped-ROM adapter retains the
  secondary table as evidence but native geometry consumes only the decoded
  signed offsets and extents.
- `prepare_actor_collision_mask_coordinates` owns CC1A's next, post-bounds
  preparation step. It turns a typed in-range window into the local 8-by-8
  mask cell, ROM-table cells, and rounded horizontal span, while exposing the
  conditional side-flag update as a named session-domain effect. The final
  mapped mask adapter consumes the three captured table roots under the active
  MPR map and returns a typed solid/empty decision; contact dispatch remains a
  separate stage.
- `scan_actor_collision` composes the recovered vertical gate, descriptor
  asset, geometry, bounds, mask coordinates, and mapped mask into one typed
  result for an explicitly supplied retry probe. Its
  `make_actor_collision_scan_context` boundary captures all shared CED2 table
  roots plus the session side-flag input from `GameStateView`, so a future
  retry owner does not reopen raw work-RAM decoding.
  `make_collision_retry_full_scan_probe` names the exact CEB5-to-CED2 input:
  CEB5 produces the carry-adjusted X low byte and retried Y low byte, while
  the inherited Y high byte remains an explicit parent-scan input.
  `scan_collision_retry_full_actors` composes that input with CED2's actor
  sweep, returning a pure actor result before its playfield continuation. The
  enclosing `scan_collision_retry_full` now preserves CED2's actor-first
  order, then evaluates the already-native CD74-equivalent map only when no
  actor is solid. It exposes both solid sources without selecting a response.
  `resolve_collision_retry_full_scan_outcome` converts that typed result into
  CED2's carry-like clear/solid/unresolved condition for the native placement
  loop, while retaining a separate target-response boundary.
  The separate
  `scan_actor_collision_sweep` now owns CED2's eligible dynamic-slot order,
  cross-candidate CC1A side-flag carry, and first-solid early exit, but remains
  a pure result because playfield and target-response handoff still need a
  natural trace. It retains every
  intermediate decision and never converts an unresolved asset into a clear
  result. A solid result still requires a target-specific contact response
  before it mutates native gameplay state.
- `CollisionRetryOffsetAttempts` now owns CDF8's complete mapped 48-byte
  placement table as three ordered whole-pixel candidates per motion quadrant.
  The existing `CollisionRetryOffsets` decoder remains a compatibility view of
  each quadrant's first candidate while the caller's multi-attempt placement
  loop is ported. `make_collision_retry_attempt_sequence` now ports the
  deterministic inner loop: it restores once per quadrant and makes the three
  incremental candidates cumulatively, leaving only random quadrant selection
  outside the native boundary. `make_collision_retry_vertical_fallback_sequence`
  also owns CDF8's deterministic CE98 fallback pass: seven cumulative
  one-pixel upward probes, including the live subtraction carry. Its repeated
  pass restart remains an explicit caller decision.
- `select_collision_retry_direction` now ports CDF8's next-quadrant control:
  it maps the caller-owned bound-four RNG result through its high two bits,
  rejects a direction already represented in the `$70` high-nibble mask, and
  selects the CE98 fallback only after all four quadrants have been tried.
- `CollisionRetryPlacementState` composes the recovered CDF8 placement flow
  without importing an assembly-shaped program counter: it advances one typed
  candidate after each CED2 clear/solid/unresolved result, waits for the
  caller-owned RNG at the proven direction boundary, and restarts CE98's
  seven-step vertical pass from its last rejected position. It remains a pure
  placement owner; target response is intentionally separate. The native
  gameplay owner now composes it with CEB5's short probe and the typed CED2
  result, retaining full scans until resolution and mutating the ball only for
  an accepted placement. It can also execute the pending CED2/CD74 scan from
  an explicit `MappedRomView`, `GameStateView`, and inherited vertical byte;
  its update returns both the descriptive scan and the resulting placement
  decision, preserving an unresolved asset as a pending candidate rather than
  manufacturing a clear result.
- `native_runtime::CollisionMask` and `evaluate_collision_mask` own a target
  shape's local eight-by-eight solidity test. The ROM-derived mask becomes
  data; high-bit-first column selection and the distinction between a clear
  candidate and a solid contact are native behavior.
- `native_runtime::CollisionMap` owns row-major playfield-map selection. Its
  parallel tile-ID and collision-attribute planes preserve the tile identity
  needed by rules while selecting a local solidity mask cell; camera conversion
  and ROM asset decoding remain separate adapter boundaries.
- `native_runtime::CollisionMapAsset` owns decoded versions of those three ROM
  planes. It validates row dimensions and every attribute-to-mask reference
  before exposing an immutable `CollisionMap` view to gameplay code.
- `native_runtime::evaluate_playfield_collision` owns the fixed table-world to
  map-relative coordinate conversion before selecting the tile. The ball and
  rules layers therefore exchange named table coordinates rather than the
  original collision scratch bytes.
- `native_runtime::scan_playfield_collisions` inserts map probing into the
  native ball's one/two/four substep plan and stops at the first solid sample,
  matching CD74's immediate handoff. It names that contact without yet
  choosing a velocity response or a table rule.
- `native_runtime::PlayfieldContact` carries that first impact's entry state,
  impact state, map cell, and tile identity into the collision resolver. It
  deliberately does not infer a normal or mutate velocity, so later response
  and table-rule behavior can be recovered and tested independently.
- `native_runtime::PhysicsState` now evaluates its adaptive ball-movement
  prefix against a native playfield map. A clear frame continues through drag
  and gravity; a solid tile returns an explicit collision-resolution handoff
  before either is applied. Contact history remains alongside the ball, while
  normal derivation and target rules remain separate future stages.
- `native_runtime::BallCollisionLoop` now makes the source loop's resume
  contract explicit: it retains the consumed-substep count, the current DC95
  split, and the sixteen-attempt cap. A CF8F separating outcome keeps the same
  substep pending after position rollback; a normal CF33 outcome consumes that
  substep and re-splits the response velocity before the loop continues. This
  is the shared state needed to join playfield response to the rest of the
  physics frame without prematurely entering actor/audio work.
- `NativeGameplayState::resume_cf33_playfield_contact` now owns that joined
  clear-selector seam. It retains signed CF33 as the existing CDF8 handoff,
  but applies CF8F's retry or CF5B/DC95's re-split through the saved cursor
  until the next solid contact or DD09. `NativeGameplayRuntime` exposes the
  same-frame update and advances actor/audio work only for the latter.
- `native_runtime::dispatch_contact` owns the confirmed-contact bookkeeping:
  same-target suppression, the per-ball response latch, target marking, and
  response-kind selection. Target-specific behavior remains a later table-rule
  layer rather than a hidden dynamic dispatch.
- `native_runtime::CollisionReactionState` gives an actor its own confirmed-
  contact reaction cadence: the table-selected event provides a response
  sequence, while the target owns phase and the four-tick phase delay. It is
  intentionally separate from ball velocity, score, and event selection.
- `native_runtime::CollisionTableState` interprets a target's decoded
  collision-table records as named state transitions. It owns alternating
  passes, reaction-event and orientation selection, cursor progression, and
  unit target motion. `ActorState` feeds a header-selected table event into
  that target's existing reaction cadence; ROM decoding and table-rule
  selection remain adapters.
- `native_runtime::resolve_collision_table_record` is the ROM adapter for
  those records. It maps a selector to a collision-entry stream and its motion
  stream, then turns restart, end, and next-entry markers into named results.
  `ActorState` consumes the decoded result and owns both cursors; neither the
  actor nor its collision behavior receives an MPR address.
- `native_runtime::advance_actor_collision_table_from_asset` composes those
  two pieces for one native update. It reads the actor's typed program and
  cursors, resolves one selector record at the ROM asset boundary, and
  supplies the resulting typed outcome to `ActorState`; it does not expose
  mapped ROM or selector addresses to actor behavior.
- `GameStateView::ActorSlot` now decodes the collision-table program, mutable
  table state, and reaction cadence alongside scheduler and motion fields.
  `seed_actor_collision_state` transfers that complete typed snapshot into an
  already configured native actor. Behavior selection and slot ownership stay
  outside the capture adapter.
- `native_runtime::SmallSoldierState` is the first recovered actor-rule model:
  it consumes confirmed contacts, preserves the two-hit rule, emits the 30 and
  50 stored-score events, and makes the subtype `$0F` animation deferral an
  explicit state rather than a hidden dispatch transition.
- `ActorState` now owns that rule for `ActorBehavior::small_soldier` records.
  A confirmed-contact call produces typed score/group/release effects and a
  terminal step releases only that actor; score and table-group ownership stay
  with their future native domains.
- `capture_small_soldier_collision_actor` is the behavior-specific migration
  boundary for collision-ready Small Soldiers. It admits only proven type-$82
  handlers, then interprets the otherwise-overloaded counter, subtype, and
  Side-Star selector fields. It seeds an already-owned native actor without
  fabricating a source generation; startup and deferred-completion states stay
  outside this capture slice.
- `SmallSoldierRuleScheduler` now carries a dispatched contact mark through
  the actor-order boundary before invoking the native actor/source/score rule.
  It rejects duplicate marks and discards a released slot's stale mark, while
  keeping subtype completion as its distinct scheduled animation phase.
  Every accepted hit now carries the proven `$21` gameplay-side audio event;
  the later subtype completion deliberately carries none, and playback remains
  the separate native audio-scheduler owner.
- `NativeGameplayState` now composes the recovered native physics-contact,
  actor, score, and scheduled Small-Soldier rule domains. Its contact entry
  commits ball bookkeeping first, then queues the behavior-specific rule for
  actor order. It also commits clear native physics frames, retains a solid-map
  impact as a pending response/table-rule handoff, and only accepts one later
  resolver result for that impact. `seed_ball` establishes an authoritative
  captured or served ball while clearing the prior ball's contact lifetime and
  pending impact atomically; `seed_score` similarly establishes the captured
  active accumulator as the native score authority. Its actor phase now walks
  due native slots
  in order, applying shared motion and queued Small-Soldier contacts at their
  recovered phase boundary; it is the seed of the native table-frame owner,
  not a second executable or an emulator-transcript adapter. It also exposes a
  drainable, source-tagged stream of proven nonzero gameplay audio commands in
  that same native execution order. This creates the real handoff for the
  eventual native audio scheduler without inventing PSG writes or samples.
- `GameplayAudioDispatch` is the next synchronous boundary: it applies the
  recovered game-side `$FF` sentinel rejection and the `$2CC7` lock's narrow
  `$31`/`$27`/`$47` suppression policy before a future native descriptor loader
  consumes an event. Dispatch outcomes retain actor/source order, so audio
  scheduling need not infer a gameplay mechanism from a command ID or expose
  emulated command latches as its API.
- `SmallSoldierSourceState` now owns each Side-Star's five-child group,
  modulo-four appearance, delayed-spawn countdown, and explicit regeneration
  handoff. Child allocation and terminal completion yield typed source effects,
  including the verified 200-unit all-five award. `ActorState` registers
  spawned children with that source and returns its committed transition;
  typed capture can also reconnect an already-accounted complete set of
  collision-ready children without changing the captured counters. Source
  presentation timing remains a later rule. A captured source's 48-tick
  delayed-spawn clock is native and emits an allocation request after restoring
  the next delay; the dynamic-slot allocator and child deployment stay
  separate owners. A collision-ready child now also seeds its captured
  appearance and shared-effect association. Bootstrap restores an effect only
  when that effect's own strict capture names the exact Small-Soldier slot;
  it never infers an effect from the overloaded association nibble.
- `ScoreState` is the first native score owner. It preserves the ordinary
  stored-unit ceiling and emits presentation dirtiness without RAM offsets;
  `small_soldier_score_events` supplies the hit, child-completion, and final
  source-completion awards in their observed order. `SmallSoldierRulesUpdate`
  now composes a confirmed actor contact (or deferred completion) with those
  individual score calls, retaining every limit decision instead of summing
  awards into a single opaque mutation.
- `BumperState` now owns the paired lower-table Bumpers' recovered nine-hit
  progression: six primary appearance hits, two secondary-damage hits, and a
  terminal 200-unit six-child release request. `BumperRulesUpdate` applies
  each award through the same ordinary score owner, so score-cap handling does
  not alter the target's independently proven terminal transition.
  `BumperRuleScheduler` retains each dispatched contact until fixed slot `$14`
  or `$15` reaches its native actor-order turn in `NativeGameplayState`.
  `capture_bumper_actor` is the corresponding steady-state capture boundary:
  it accepts only the fixed type-`$82` slots at handler `03:78C7`, validates
  their side and phase-counter invariants, and supplies the scheduler's named
  progression without interpreting those overloaded bytes for other actors.
  Fixed-slot setup, child allocation, and presentation remain separate
  migration work.
- `DragonHeadState` now owns lower-left fixed slot `$13`'s recovered eight-hit
  rule. `DragonHeadRuleScheduler` delivers a confirmed contact in actor order,
  applies the ordinary 50-unit/`$2E` hit and its impact/appearance request, and
  permits a new contact only after the presentation owner completes it. The
  eighth hit applies the separate terminal 50-unit award and reports a
  mouth-opening transition handoff; its multi-stage animation is deliberately
  not approximated as an elapsed-frame counter.
  `capture_dragon_head_actor` accepts only the clear steady type-`$82`
  `02:5A4B` record with its valid one-through-eight family hit counter, so
  terminal and transition snapshots cannot seed a falsely ready target.
- `GiantFaceState` now owns the central Giant Face's two collision surfaces at
  fixed crown slot `$39` and mouth slot `$38`. Its scheduler retains a
  confirmed contact until the owning fixed actor turn: crown and closed-mouth
  contacts award three stored units, emit `$20`, request the shared impact
  effect, and wait for presentation to rearm. The separately-owned form
  transformation can explicitly open the mouth; its next contact awards 100
  units, places the ball at `(144,344)`, emits `$41`, and requests bonus stage
  4. `GiantFaceAnimatorState` now owns the hidden animator's fixed slot `$37`
  cadence as a separate display owner: its one-turn startup delay leads into
  eight-turn cycles of forms `1,2,0` or `4,5,3`. `GiantFaceControllerState`
  also owns `$36`'s completed wake: its one-turn delay and four visual steps
  store form 1, clear the named wake request, and start lower-form animation
  in real slot order. It retains the independent transformation signal until
  wake completes, then selects forms `2,3,4,5,6,7` in source order: form 4
  starts the upper animator, form 6 opens the collision owner's mouth, and
  form 7 stops animation and returns the controller to dormant state. This is
  named game-domain state, not a guessed timer or a handler-address dispatch.
  `capture_dormant_giant_face` transfers only the matching clear four-slot
  steady snapshot, after validating that the decoded form, request, display,
  animation, and mouth latches are all dormant. Its mouth snapshot preserves
  bit zero (open form) separately from bit seven (captured-ball bonus-stage-4
  request), and rejects either one at this startup boundary. Pending collisions
  and every active wake/transformation/presentation state remain with their
  owners. The form-six open bit is not a steady exception: its next mouth turn
  enters the long `03:7749` continuation and schedules an `01:C5C9` descendant,
  so a type-`$84` mouth record alone cannot be restored without its live CPU
  continuation.
- `GiantFaceHelmetState` owns the two mirrored type-`$84` activation channels
  after their fixed setup/capture establishes them. Their `$2A/$2B` turns
  retain the verified four-tick, four-appearance idle cycle; a confirmed
  collision (after the separate special-condition check) awards three stored
  units, emits `$20`, and requests the Giant Face wake state. Opening
  presentation completion separately emits `$25` and requests a ball return
  through the side-specific routing owner. Route geometry and ball-launch
  physics are deliberately not fabricated at this rule boundary.
- `DragonEggClusterState` now owns the six fixed `$20-$25` target lifecycles
  and their separate slot-`$1F` controller turn. A confirmed egg hit disables
  only that target, awards 10 stored units, emits `$39`, and requests a
  hatchling allocation. The allocator-selected dynamic hatchling now copies
  its parent's recovered start position, advances its observed animation
  timer, and resolves the distinct terminal 200-unit/`$34` contact before its
  dynamic record is released. Only a later controller turn converts all six
  destroyed eggs into the 100-unit group-completion/reset handoff. Once the
  separate reset-timing owner supplies its signal, `DragonEggRuleScheduler`
  reinitializes fixed slots `$20-$25` in their normal actor order, clearing
  completion only after all six are live. Dynamic hatchling flight motion,
  reset timing, and presentation remain separate owners rather than guessed
  local state.
  `capture_dragon_egg_cluster` accepts the complete clear six-target
  `02:4345` snapshot. `capture_partial_dragon_egg_cluster` additionally
  accepts a one-through-five-live mixed cluster only after each destroyed egg
  reached `02:4387`, the source live count agrees with those fixed records,
  reset is clear, and no `02:441E` hatchling is live.
  `capture_completed_dragon_egg_cluster` also accepts the all-disabled,
  pre-reset interval when `$2D20.0` records completion and the slot-`$1F`
  controller is still at `02:42F6`; native state starts already completed, so
  the controller award is not duplicated.
  `capture_dragon_egg_cluster_with_hatchling` also admits the narrow
  in-flight handoff of five steady targets, one disabled parent, and exactly
  one live `02:441E` child. It claims the captured dynamic record directly,
  preserving its position and animation timer without replaying allocation.
  Multiple-child ownership and reset-timing dispatch remain excluded.
  Once the separate reset owner has supplied its signal,
  `capture_dragon_egg_reset_cluster` accepts a zero-through-five-live mix of
  `02:4345` and `02:4387` fixed records only when count, completion latch,
  controller, reset signal, and hatchling absence agree. Native actor order
  resumes only the still-disabled fixed targets.
- `GiantSkeletonEyeballState` now owns fixed slot `$2F`'s semantic
  Dragon's-Egg completion handoff. After the slot-`$1F` controller establishes
  completion, the later Eyeball turn emits exactly one opening-transition
  request. When its immediate setup/presentation owner acknowledges that
  request, the native state runs the proven eight-tick opening countdown and
  emits a distinct expiry handoff. Once the separate owner selects the final
  branch, native state runs its source-selected 224- or 992-turn mouth wait,
  four-step `0/1/0/1` appearance cadence, and reset handoff. The source mode,
  entry presentation, and final reset presentation remain separate rather than
  being guessed as generic frame counters. The
  Eyeball is explicitly seeded after table setup/capture while that fixed
  lifecycle is still outside the native frame initializer.
  `capture_giant_skeleton_eyeball_actor` admits the already-initialized
  type-`$82` `02:4DF7` opening wait with state `2` and its proven two-through-
  eight-tick timer, allowing native takeover during that countdown without
  fabricating dormant setup or the unported expiry-entry branch.
  `capture_giant_skeleton_eyeball_final_animation_actor` likewise admits only
  direct `02:4F3D` final waits away from their appearance and phase
  transitions, carrying the named 16-bit wait and current 0/1 appearance into
  the native scheduler. After the separate close-presentation acknowledgment,
  the native lifecycle follows the source's direct return to `02:4DC8` and
  restores the Eyeball's dormant state.
- `GiantSkeletonState` now owns lower-right fixed slot `$30`'s live update.
  Its native direction classifier derives the ROM table's appearance selector
  from the ball and fixed target positions every actor turn, rather than
  treating the five appearances as RNG. A confirmed contact remains repeatable:
  it awards 40 stored units, emits `$2E`, and requests the common impact
  presentation before the next live update. `capture_giant_skeleton_actor`
  admits only the clear steady type-`$83` `02:5EEC` record and its valid live
  appearance, so a latched repeatable hit cannot be silently lost during native
  startup. Fixed-table setup still remains a separate owner; its Eyeball
  opening sequence remains later work.
- `FrontSpiderState` now owns the paired lower-table Front Spiders' one-hit
  rule: a confirmed contact sets its side-selected safety latch, awards 40
  stored units, emits command `$34`, and requests the recovered four-stage
  appearance transition. `FrontSpiderRuleScheduler` carries the contact to
  fixed slot `$26` or `$27` in actor order and blocks another hit until the
  appearance owner explicitly reports completion; it does not invent an
  elapsed-frame approximation for that animation. `capture_front_spider_actor`
  accepts only the fixed type-`$83` actors at steady handler `03:6490`, then
  interprets their family-local side byte to seed the native rule without
  treating an appearance-transition snapshot as collision-ready.
- `SideSpiderState` separately owns the outer side-shoot pair's one-hit rule:
  a contact sets the shared transition latch, awards 100 stored units, emits
  `$34`, and becomes unavailable pending its presentation transition.
  `SideSpiderRuleScheduler` delivers that result at fixed slots `$28/$29` in
  actor order. Its five-state appearance controller is deliberately retained
  as a later presentation owner rather than converted into guessed frame time.
  `capture_side_spider_actor` accepts only the matching fixed type-`$83`
  steady handler `03:651F` and its family-local side byte, so transition
  snapshots cannot seed an incorrectly ready native target.
- `TableRuleLatches` now names the recovered cross-target signals instead of
  leaving them as shared RAM bytes. `SkeletonInsectRuleScheduler` consumes the
  Front-Spider safety or Tower-completion source at fixed slots `$31-$33`,
  after those source slots have run, and emits the one-shot 40/100/40-unit
  awards with the observed 3/16/2-round transition requests. The detailed
  post-trigger collision and presentation states remain later native work.
  `GameStateView::table_rule_latches` captures the group atomically and
  `capture_skeleton_insect_actor` admits only each target's known steady
  handler, so a capture cannot seed an unported transition as a ready target.
- `TowerRuleScheduler` owns the recovered first qualifying Tower contact at
  fixed slot `$2D`: it emits the 50-unit score and `$33` audio event, advances
  its source progress, and supplies the center Skeleton-Insect latch before
  the later `$32` actor turn. The `03:611C/03:611F` presentation handoff is a
  native four-update phase: three countdown updates then the steady-dispatch
  install. Subsequent armed passes now decrement the
  named five-step gate, and zero produces a pending BS-5-selection state.
  The recovered `03:6136` prefix now keeps no-hit ticks pending and turns only
  a later negative Tower collision into a typed BS-5/session-transition
  request with `$41` audio, stage flag `$3C80`, a reset gate, and a fresh
  Tower/insect cycle. The shared timer/audio/scene dispatcher remains a later
  native owner.
  `capture_tower_actor` adds the corresponding strict steady capture boundary,
  including named Tower-local hit progress and remaining gate count.
- `select_tower_guard_selector` now replaces the adjacent fixed Guard's
  `03:60A6` selector with a native derived rule: a nonzero shared Tower gate
  writes closed `$84`, while zero writes open `$8E` for the BS-5 entrance. The
  Guard's independent mask collision response remains a later owner.
  `capture_tower_guard_actor` admits it only at that steady dispatcher and
  derives the captured selector from the Tower gate instead of treating the
  Guard's local completion byte as independent gameplay state.
- `TowerInsectRuleScheduler` now owns one four-child source generation from
  fixed source `$2C`: explicit source-timed spawn requests consume its budget,
  each child has the proven two-hit 10-unit/`$29` rule, and a terminal child
  adds 50, releases its slot, and decrements the shared live count. The final
  live child produces the later 200-unit source group-clear handoff.
  `capture_tower_insect_generation` atomically validates each steady child and
  either the source's positive live count after all four spawns or its ordinary
  one-to-48-turn spawn countdown without exceeding its four-child capacity.
  After the
  native 200-unit source handoff,
  `restart_tower_insect_generation` preserves the completed generation until
  the explicit Tower/insect-cycle latch is supplied, then recreates its
  four-child budget. The default native source is inert until the same latch
  initializes it, after which its one-turn initial request and subsequent
  48-turn spawn cadence are native state transitions; dynamic request-slot
  selection and collision-effect drawing remain explicit owners rather than
  assembly-shaped native code. The shared collision-effect allocator is now a
  typed native child lifecycle too: an allocator-selected slot becomes a
  source-associated type-$8F effect with copied appearance and an eight-turn
  lifetime; drawing the burst remains separate. `capture_collision_effect`
  also seeds a steady in-flight effect after its linked source is native.
  An allocator-selected slot is now claimed as a
  typed native Tower-insect record before the rule child is committed, and is
  released only after its terminal hit updates the source count. Its `$3900`
  appearance is now carried through both live capture and ordinary native
  allocation. Capture also validates that each live child matches its fixed
  source selector, so a shared effect copies the real source appearance. It
  also carries the source-copied `$3D00` association (`$8C`), accepting only
  the shared effect's temporary low-nibble `$0F` variation; this lets a live
  Tower-insect generation and its effect be seeded together.
- `native_runtime::FrameObservation` and `compare_frame_observations` compare
  frame number, work RAM, framebuffer shape/pixels, audio rate, and samples in
  deterministic first-difference order. `FrameSnapshot` owns an observation
  across the next runtime update, which is required when the emulator and
  native runtimes advance independently. The future `NativePceRuntime` will
  provide the candidate observation; the current emulator provides reference
  observations.
- `capture_emulator_frame` is the reference-side adapter from `PceRuntime` and
  `EmulatedFrame` to that owned snapshot, so replay code has one frame-boundary
  capture path rather than borrowing buffers across updates.
- `NativeGameplayRuntime` is the first candidate-side frame seam. It owns a
  freshly validated `NativeGameplayBootstrap`, advances a clear recovered
  physics pass followed by native actor order, and returns the physics result,
  ordered actor updates, and gameplay-audio handoff. A solid playfield contact
  returns at the pending collision-resolver boundary without advancing actors,
  so this runner cannot silently replace unrecovered response logic. The older
  `complete_playfield_collision` seam is now compatibility-only: CF33's
  separating and response-ready outcomes must resume the retained C6/C7 loop
  before actor/audio work, rather than treating a response ball as a completed
  frame. Its `retry_pending_playfield_collision` seam likewise remains a
  temporary CEB5 boundary. It also carries CDF8's
  recovered placement loop through its typed CED2/CD74 scan and shared-RNG
  boundaries: duplicate RNG directions remain pending. A distinct direction
  can accept a clear candidate, but the playfield runtime now reports that as
  a typed `rearmed` result instead of committing the temporary candidate or
  releasing actor/audio work: the observed CDF8 route returns through
  `01:CB7E`'s clear `$77.3` arm to restore the cached position and rearm the
  unconsumed C6/C7 substep. `resume_rearmed_collision_retry` preserves that
  cursor and runs the same numbered frame until its next contact or DD09;
  only the latter releases native actor/audio work. The consuming arm is now observed at
  CDF8 entry, but its parent collision route remains separately classified by
  the live shadow. Unresolved geometry and further candidates remain
  explicitly pending. It deliberately does not claim response selection,
  rendering, audio synthesis, or scene flow; those domains must join this
  runner before it can become the selectable `NativePceRuntime`. Its temporary
  actor-only test seam also rejects pending contacts, preserving that boundary
  across every runtime entry point.
- `NativeGameplayBootstrap` combines the already-proven live-ball, active-score,
  and table-latch snapshots into one validated startup transaction, then applies
  them to a fresh `NativeGameplayState`. It also includes the Giant Face at
  either its strict dormant capture or settled lower/upper form loops (forms
  one through five), restoring controller progress and the animator's typed
  form and countdown. Its two helmet openings are independently restored only
  at their collision-ready idle cadence; wake, transformation, mouth, helmet
  activation, and presentation transitions remain excluded. It includes the
  Dragon's-Egg cluster at
  any independently validated stable lifecycle point (including its one
  captured hatchling), the clear Dragon's Head steady dispatcher, and either
  independently validated Giant Skeleton steady state or Eyeball native wait,
  plus the Tower's clear steady dispatcher and, when both fixed records are
  steady, its Guard's derived actor-phase selector, and either independently clear
  Front Spider, Side Spider, Skeleton Insect, Tower-Insect generation, or
  Bumper, plus independently collision-ready Small Soldiers and any stable
  Side-Star source whose live-child counter exactly matches its captures. A transitioning
  target never prevents another valid fixed target from entering native
  ownership; the dynamic Tower-Insect source, its children, and any linked
  collision-effect child remain one atomic capture because their allocator
  identities are coupled. A transitioning Small-Soldier source never blocks
  an independently collision-ready child, but bootstrap only reconnects
  children to a proven source generation rather than fabricating one.
  An active Face stays with its current owner until every controller/animation
  state can be recovered. This includes form six: its visible open state begins
  a retained mouth continuation rather than a restartable native boundary.

The foundation has ROM-free core tests. The runtime selector, capture seeding,
table asset decoding, collision-map/target integration, and target-specific
behavior remain the next implementation work; the emulator still owns normal
gameplay execution today.

## Migration order

1. **Oracle and runtime shell.** Add `NativePceRuntime`, mode selection, and
   a replay harness that runs identical inputs through the emulator and native
   runtime. Compare per-frame work RAM, framebuffer, audio, and save/restore
   behavior.
2. **Actor scheduler, collision, and physics.** Replace the composite actor
   ownership represented by `01:C539` and its dynamic handlers with a typed
   scheduler. The initial `ShortActorPass` now joins C539's decoded frame seed,
   table-order due-actor list, and C606 presentation filing contract; it rejects
   out-of-order handler completions without changing the live emulator path.
   `NativeGameplayState::advance_actor_phase_slot` supplies the corresponding
   one-actor named behavior unit while preserving the full-phase oracle's
   ordering, and `NativeGameplayActorBehaviorDispatcher` binds it only to a
  matching, seeded C539 slot. Presentation adaptation and differential replay
  are still required before this becomes a runtime replacement. This is the
   first major payoff and the prerequisite for removing the remaining high-cost
   actor interiors.
3. **Actor families and rules.** Move targets, enemies, gates, scoring,
   ball/turn transitions, and bonus stages into named domain modules. Use the
   registry only to recover behavior and feed focused differential fixtures.
4. **Presentation and audio.** Replace score-panel/UI and music scheduling
   with native state-driven render/audio systems. Preserve output fingerprints
   before considering an emulator path retired.
5. **Scene flow and compatibility.** Move boot-to-gameplay flow, saves, and
   remaining front-end logic; keep an explicit compatibility list for anything
   intentionally retained.

## Validation rules

- The emulator is authoritative until a native domain passes its replay corpus.
- A domain requires focused semantic tests plus player-session replay coverage.
- Compare work RAM, framebuffer, audio, input handling, scene transitions, and
  save/restore state at frame boundaries.
- Add captures for gameplay states not covered by the current corpus; do not
  use coverage percentage alone as evidence of semantic completeness.
- Delete or disable a hybrid replacement path only after the corresponding
  native domain is selected in normal play and passes its oracle corpus.

## Definition of done

The port reaches 100% native gameplay execution when the native runtime plays
the certified capture corpus without HuC6280 gameplay instruction execution
and produces the required observable results. A separately selectable emulator
mode does not prevent that claim; any code that still requires emulated
execution in the native-runtime path does.
