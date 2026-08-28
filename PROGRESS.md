# Project Progress

Last updated: 2026-08-27

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
| How the remaining gameplay becomes readable native C++ | [docs/NATIVE_RUNTIME_MIGRATION.md](docs/NATIVE_RUNTIME_MIGRATION.md) |
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

**Milestone 4, native gameplay port and native-runtime migration.**

As of 2026-08-25, the project no longer treats the routine registry as the
route to completion. The registry remains a high-fidelity reverse-engineering
and differential-testing tool, while the delivery target is a selectable
native `PceRuntime` with human-readable C++ state and subsystem boundaries.
The emulator is the oracle during migration and may remain as an optional
compatibility mode; it is not required by the completed native gameplay path.
The implementation order is actor scheduler/collision/physics, actor families
and table rules, score/UI, music/audio scheduling, then scene/startup flow.
See [docs/NATIVE_RUNTIME_MIGRATION.md](docs/NATIVE_RUNTIME_MIGRATION.md).

The last generated registry measurement (2026-08-22, 593 admissions) runs
**699,919,959 of 1,292,371,731 master cycles natively on the canonical
3,600-frame route — 54.16% of the route, 98.51% of everything outside the
frame-sync wait** — with 475 routines entered, zero interrupt-horizon declines
and zero abandoned transcripts, against a state census byte-identical to the
assembly-only baseline. The `00:E85D` square root, `05:6D27` live orbit arm,
`00:E5BC` multiply partial, and `02:4FC4` side-gate deadline check were
admitted after that measurement, so the current registry holds 598 entries;
regenerate [docs/NATIVE_REGISTRY.md](docs/NATIVE_REGISTRY.md)
with the next canonical census.

`00:E728` unsigned fixed-point division is now also admitted from the long
`player-session-6` capture: all 240 calls (two callsites) preserve every
31,971-frame fingerprint, increasing that session's native non-wait coverage
from 97.46% to 97.48%.

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
**Slice:** establish the native-runtime foundation and its frame-by-frame
oracle contract. The typed actor scheduler selects records due for updates in
table order and chooses dynamic-slot reservations. The first native behavior,
shared non-ball actor motion, now has a typed position/velocity update and a
native actor-state owner. Single-ball position integration and the unobstructed
adaptive one/two/four collision-substep loop are native; drag/gravity and the
collision-velocity response families are native as well. Confirmed-contact
bookkeeping, collision-space bounds rejection, and the no-contact physics
frame path are native. Eight-by-eight collision-mask solidity is also native.
The recovered post-handler SAT filing step is native too: it preserves C606's
three bounded presentation queues, fixed slot-zero seam, prequeue presentation-refresh
dependency, and reverse/reverse/forward drain order while keeping dynamic
handler execution as the next separate scheduler owner. Its typed completed-
handler seam carries the attribute sampled before F605's presentation refresh
separately from the reread queue-selection attribute. F605 recomputes transformed
presentation coordinates and clears a selected visibility flag, so future native
behavior handlers can join the SAT path without acquiring assembly-shaped dispatch
semantics. The shorter C539 actor-pass preamble now has its own typed seed:
it resets presentation queues and the completed ball-substep counter, while a
named freeze gate retains or clears the inherited actor-enable mask. Dynamic
handler execution remains outside that seed, while `ShortActorPassPresentation`
now owns the queue state across each completed named handler. The first BS-4
native rules are now also present: a strict three-objective capture seeds their
fixed actor-order scheduler, retaining 100-unit `$2E` contacts, terminal
1,000-unit awards, and the multiplier-20 all-clear handoff. The paired-source
linked target now has a named allocator handoff, 100-unit `$3B` contact, and
four-child request; each allocator-selected orbiting child has its own
100-unit release contact and recovered initial orbit phase, with clear turns
advancing modulo 32 through the native signed orbit-offset table. Strict capture
admits exactly one `$6B7E`/`$6CA0` target with its copied source/objective identity
and live-source marker, plus every clear `$6D27` child whose parent link,
normalized phase, and parent-relative table position agree; bootstrap seeds that
coherent linked generation atomically. Its `$3B00,X` one-to-32-tick refresh cadence now requests new shared-RNG
motion on expiry; motion integration, animation, allocator timing, and the shared result
presentation remain separate owners. Row-major
collision-map selection now preserves both tile identity and its
collision attribute. An owned asset boundary decodes and validates the
parallel tile, attribute, and mask planes, while a table-world probe adapter
owns the fixed playfield coordinate conversion. Capture seeding from the
supported ROM now has a typed work-RAM source for the live table pointers;
the native mapped-ROM adapter can resolve a configured layout through the
current MPR map. A capture adapter records both pieces at the actual playfield
probe, and a diagnostic replay of `player-session-6` found the single main
table source. Its named native layout has 94 rows and 250 masks. Decoding that
live main-table asset and scanning it after every native ball substep are now
available. The first solid sample is promoted to a typed contact handoff with
the entry/impact ball states and tile identity, so collision resolution can
remain native without reinterpreting scan scratch data. Velocity response
selection and table-specific contact-event selection still remain. Once an
event is selected, the contacted native actor now owns the exact reaction
phase/countdown cadence and response-code sequence.
The collision-table record transition that supplies those events is also now
native: alternating pass selection, header/continuation records, orientation,
cursor updates, and unit target motion use typed state rather than work-RAM
offsets. `ActorState` connects a header-selected table event to its target's
reaction cadence. ROM record decoding and table-specific collision-event
selection remain separate rules work: the ROM adapter now resolves a selector
through its collision-entry and motion streams, including restart, terminal,
and next-entry control markers, into typed records for the actor layer.
The actor layer now commits those typed cursor transitions and applies ready
records, while terminal and invalid data remain explicit non-record outcomes.
`advance_actor_collision_table_from_asset` now makes that sequence one native
update without leaking MPR mapping into actor state: each configured actor
owns its table selector and cursors, then the driver decodes one selected ROM
record and returns the typed result to the actor owner. Table-specific
selection of an actor remains rules work.
`GameStateView` now decodes the corresponding complete typed actor seed from
capture RAM: table selector, pass/event/orientation, both cursors, target
position, and reaction phase/countdown. It keeps those raw offsets at the
adapter boundary; `seed_actor_collision_state` copies that named state into an
already-owned native actor without making capture RAM part of gameplay state.
The first actor-rule model is now native as well: `SmallSoldierState` owns its
confirmed-contact two-hit completion rule, 30/50 stored-score events, group
decrement/release effects, and the explicit subtype-`$0F` animation deferral.
`ActorState` now owns the rule for named `small_soldier` actors and returns
those effects while safely releasing a completed actor slot. Actor-family
scheduling, presentation, and score-state consumption remain separate domains.
The paired Side-Star source lifecycle is also named: each source owns its
five-child budget and live count, modulo-four appearance, and an explicit
regeneration handoff after the verified 200-unit completion award.
`ActorState` registers spawned children and commits the owning source's
terminal transition. Only source presentation timing remains separate rules
work.
The first native `ScoreState` now consumes ordered stored-score events with
the original ordinary-score ceiling and an explicit presentation-dirty flag;
Small-Soldier hit, child-completion, and source-completion awards feed it in
their observed order. `SmallSoldierRulesUpdate` now composes actor, Side-Star,
and score transitions while retaining each original score-limit decision.
Collision-ready Small Soldiers now have a behavior-specific capture boundary:
it validates the known type and collision handlers before decoding their
two-hit counter, subtype, and Side-Star identity. The paired fixed Side-Stars
also have a typed source capture for their live/pending generation counts,
appearance, and spawn delay. Bootstrap reconnects a source only when its
live count exactly matches every captured child for that side; otherwise the
valid child remains independent rather than fabricating generation state.
Captured sources now advance their 48-tick delay natively and emit a typed
child-allocation request, while the dynamic-slot allocator remains a separate
owner.
An active shared collision effect is restored only when its strict typed
capture names a recovered Small-Soldier source slot; the overloaded source
association nibble alone never fabricates an effect.
`SmallSoldierRuleScheduler` now preserves the confirmed-contact mark until the
later actor update consumes it, joins that update to the native score/source
rule, and rejects duplicate or stale dynamic-slot marks. The subtype animation
completion remains a separate scheduled phase. Each accepted hit also carries
the proven `$21` audio event; deferred completion does not re-emit it, while
the future audio scheduler remains responsible for playback.
`NativeGameplayState` now owns the connected native physics-contact, actor,
score, and Small-Soldier scheduler domains. It commits contact bookkeeping
before queuing the behavior-specific actor phase, creating a direct native
table-frame seam. Clear playfield physics frames also commit there, while a
solid impact becomes a pending later response/rule handoff which admits one
resolved ball state before the next frame. This does not introduce a second
executable or use the emulator transcript layer.
Its actor phase now performs the corresponding native slot-order walk:
shared-motion actors advance on due frames, and collision-queued Small Soldiers
resolve only when their due actor turn arrives.
`NativeGameplayRuntime` now owns one validated bootstrap and advances a clear
native physics pass plus the following actor phase as one frame candidate,
returning the physics result, exact actor-order updates, and gameplay-audio
handoff. A solid playfield contact stops at the existing pending resolver
boundary without advancing actors. After the native collision resolver supplies
the resolved ball, the runner completes actor order and audio without creating
a second frame number, preserving one continuous native frame transaction. It
also completes the proven CEB5 clear-retry path and CDF8 accepted-placement
path in that original frame; CED2/CD74 scans, unresolved assets, solid
candidates, and CDF8's RNG boundary remain explicitly pending until their
native geometry/placement result is known. This avoids treating retry recovery
as a second frame or silently replacing an unknown collision rule.
It deliberately leaves response selection, presentation, synthesis, and scene flow
with their current owners until they can join the same native frame runtime.
It now also drains proven nonzero gameplay audio commands as source-tagged
events in that same execution order. This is the native gameplay-to-audio
handoff; PSG scheduling and synthesis remain a later, separate owner.
`GameplayAudioDispatch` now makes the recovered synchronous game-side gate
explicit: `$FF` is ignored, while only `$31`, `$27`, and `$47` are suppressed
during the `$2CC7` audio lock. It retains every source-tagged submission and
actor order for a future descriptor-loader owner without reproducing audio-RAM
latches or treating raw command IDs as gameplay-mechanism names.
The lower-left Dragon's Head now has the same native actor-order boundary:
its named eight-hit state applies the 50-unit/`$2E` hit, requests the proven
impact/appearance handoff, and re-arms only when presentation completes. The
eighth hit adds the separate verified 50-unit award and enters an explicit
mouth-opening transition instead of inventing its unported animation timing.
Its strict capture adapter now admits only the clear, steady type-`$82` slot
`$13` dispatcher with a one-through-eight hit counter, leaving pending hits,
terminal opening, and presentation transitions with their native owners.
The central Giant Face's crown and mouth surfaces now also resolve at their
fixed `$39/$38` turns in native C++: crown and closed-mouth hits award three
stored units with command `$20` and wait for the shared impact presentation;
the form owner can explicitly open the mouth, after which a capture awards
100 units, positions the ball at `(144,344)`, emits `$41`, and requests bonus
stage 4. The hidden animator at `$37` now also owns its verified native
one-turn startup delay and eight-turn display cadence: lower mode cycles forms
`1,2,0`, while upper mode cycles `4,5,3`. The `$36` controller now consumes a
helmet wake through its one-turn delay and four native visual steps, stores
form 1, and starts the lower animator. The same controller now also retains
the independent transformation signal through wake and selects forms `2,3,4,5,6,7`
in order: form 4 starts the upper animator, form 6 opens the mouth, and the
final form stops animation and restores dormant controller state. The paired helmet channels
at `$2A/$2B` are native when seeded from fixed setup or their strict paired
idle capture: their four-turn,
four-appearance idle cycle now resolves a confirmed contact into the verified
3-unit/`$20` award and Giant-Face wake request. Their opening presentation,
`$25` ball-return handoff, and side-specific route completion remain named
owners rather than guessed ball physics.
`capture_dormant_giant_face` admits the matching clear four-record snapshot
only when the controller, hidden animator, mouth, and crown are at their fixed
steady dispatchers and every decoded Giant-Face request, form, display,
animation, and mouth latch is inactive. `capture_awake_lower_giant_face` and
`capture_awake_upper_giant_face` also admit the settled post-wake loops
(persisted forms one through five): no
pending request, clear collision surfaces, a valid one-through-eight animator
countdown, and matching lower- or upper-form display state. It restores typed
controller progress and animator state without fabricating a CPU continuation.
Wake, transformation, collision, and presentation snapshots remain outside
this deliberately safe native handoff.
The mouth's open-form bit and its captured-ball/bonus-stage request bit are
decoded independently, so either active state remains outside the handoff.
In particular, setting the open-form bit alone immediately enters the
long `03:7749` mouth continuation and schedules its `01:C5C9` descendant;
the type-`$84` mouth record is therefore not a standalone sixth-form snapshot.
`NativeGameplayBootstrap` now applies one coherent frame-boundary seed for the
typed live ball, active score, and table latches, including the Giant Face at
either strict dormant state or its settled lower/upper form loops and the
independently collision-ready helmet pair. It also restores the
clear
steady Dragon's Head, Giant Skeleton, Tower, either clear Front Spider, Side
Spider, Skeleton Insect, Tower-Insect generation, or Bumper, and either proven
Eyeball wait independently when their respective capture boundaries are
present. The dynamic Tower-Insect source, its live children, and any linked
collision-effect child remain a single validated generation rather than
independently seeded slots. Independently collision-ready Small Soldiers also
seed their named native contact rule, while deliberately leaving their
unrecovered Side-Star generation unregistered. It provides the native runtime shell
with a safe startup transaction without fabricating active actor state.
The same transaction now accepts every validated stable Dragon's-Egg lifecycle,
including its one captured hatchling, while rejecting inconsistent full-cluster
latches before it reaches native ownership.
The six fixed Dragon's Eggs now have their first readable native rule slice:
one actor-order hit disables its own egg, awards 10 stored units, emits `$39`,
and requests a hatchling allocation. The allocator-selected dynamic hatchling
now copies its parent egg's recovered position, carries its observed wrapping
animation timer, and resolves its distinct 200-unit/`$34` terminal contact
before releasing only that child. The fixed slot `$1F` group controller
then separately observes all six destroyed eggs, awards 100 stored units, and
requests the existing reset sequence. Once its separate timing/presentation
owner supplies the reset signal, the native scheduler reinitializes fixed
slots `$20-$25` in actor order rather than collapsing the reset into a single
state assignment. Allocation, hatchling flight, and reset presentation remain
explicit later owners.
`capture_dragon_egg_cluster` seeds the complete, clear six-target steady
snapshot. `capture_partial_dragon_egg_cluster` now additionally accepts the
stable mixed state after one or more eggs reached `02:4387`, only when the
source live count agrees with the fixed records, reset is clear, and no
`02:441E` hatchling remains live. `capture_completed_dragon_egg_cluster`
also accepts the all-disabled, pre-reset snapshot only when the source
completion latch is set and the slot-`$1F` controller remains at `02:42F6`;
it seeds the already-awarded native completed state rather than replaying the
100-unit award. `capture_dragon_egg_cluster_with_hatchling` now restores the
separate narrow in-flight boundary: exactly five steady eggs, one disabled
parent, and one live `02:441E` child. It seeds that existing dynamic record
at its captured position/timer without replaying allocation. Multiple-child
ownership and reset dispatch remain outside the capture boundary.
`capture_dragon_egg_reset_cluster` also seeds the native per-slot reset after
the separate owner raised its signal: it accepts a zero-through-five-live mix
of steady and disabled fixed records only when the count, completion latch,
controller, reset signal, and absence of hatchlings agree. Native actor order
then reinitializes only the remaining disabled targets.
The fixed Giant Skeleton's Eyeball now consumes the completed Egg controller
state on its later slot `$2F` turn. Its readable one-shot opening handoff is
separate from the Giant Skeleton's repeatable target behavior. After its
immediate opening setup is acknowledged by presentation, its recovered
eight-tick countdown now runs natively. After its separate expiry/presentation
handoff selects the final branch, native state also owns the verified
224- or 992-turn mouth-animation wait, including its four-step `0/1/0/1`
appearance cadence and terminal reset handoff. The source mode and final reset
presentation remain explicit future owners. `capture_giant_skeleton_eyeball_actor` can now seed that native
countdown from a live type-`$82`, state-`2`, `02:4DF7` continuation with two
through eight ticks remaining. `capture_giant_skeleton_eyeball_final_animation_actor`
also admits direct `02:4F3D` final waits only while both countdowns remain
strictly away from their source transitions, transferring the named 16-bit wait
and 0/1 appearance. One-tick expiry, dormant setup, and final-animation entry
states retain their existing owners. After the final presentation owner
acknowledges completion, the native lifecycle now follows the source's direct
return to `02:4DC8` and reinitializes the Eyeball's dormant state.
The lower-right Giant Skeleton now has a native fixed-slot update as well:
its five appearances come from the recovered ball-direction classifier, and a
confirmed hit is a live, repeatable 40-unit/`$2E` event with an explicit
impact-presentation handoff. It is intentionally seeded by table setup/capture
until that fixed-table lifecycle joins the native runtime. Its strict capture
adapter now admits only the clear, steady type-`$83` slot `$30` dispatcher and
its live direction-selected appearance, leaving a pending hit with the native
collision owner rather than dropping its award during startup.
Its collision owner now also composes CDF8's complete typed placement loop:
three cumulative candidates per motion quadrant, caller-owned RNG direction
selection, and repeatable seven-step vertical fallback. CEB5 early clears
commit only the accepted candidate; CED2/CD74 full scans remain pending until
their explicit clear/solid/unresolved result advances the same state. This is
placement control only: target-contact response selection remains separate.
The gameplay owner can now execute that pending actor-first/playfield scan at
the explicit mapped-ROM and captured-state boundary and retain its descriptive
result alongside the placement decision.
The paired lower-table Bumpers now also have a native nine-hit rule: six
primary appearance hits, two secondary-damage hits, then a scored terminal
six-child release request. Confirmed contacts are now held for Bumper slots
`$14/$15` and consumed in the native actor-order phase. Their fixed-slot setup,
allocation, and presentation remain separate actor-family work. The new
`capture_bumper_actor` boundary admits only the two steady fixed targets,
validates their type, handler, side, and phase/counter relationship, then
provides the named rule state required to seed that native scheduler.
The paired Front Spiders now have the same native contact-to-actor-order
boundary: their one-hit rule sets a side safety, awards 40 units, emits `$34`,
and requests the observed four-stage transition. They remain unavailable until
the presentation owner supplies an explicit completion, preserving the
recovered timing boundary without guessing animation frames. Their steady
capture boundary now validates fixed slot/type/handler/side identity before
providing the native scheduler's collision-ready state.
The adjacent Side Spiders now have their own native one-hit rule rather than
being conflated with Front Spiders: a contact sets shared latch `$2D32`, awards
100 units, emits `$34`, and waits for the shared appearance controller before
rearming. Their fixed `$28/$29` contacts also resolve in native actor order;
their steady capture adapter validates slot/type/handler/side identity before
seeding that native state.
`TableRuleLatches` now owns the cross-target signals behind the three Skeleton
Insects. In native actor order, a Front-Spider hit writes its typed safety
latch before the corresponding later Skeleton Insect consumes it, performs its
one-shot 40-unit award, and requests its known startup transition. The center
insect's 100-unit Tower-completion path uses the same state model and remains
ready for the later Tower source owner. The capture boundary now decodes the
four shared table latches together and accepts each Skeleton Insect only at
its exact steady handler, allowing an existing source-ready snapshot to seed
the native actor-order rule without treating an unported animation state as
collision-ready.
The Tower's first qualifying contact now also runs natively at slot `$2D`:
it applies the proven 50-unit/`$33` event, sets the center-insect latch, and
then permits the same-pass `$32` Skeleton Insect's 100-unit event. Its visual
handoff is now native and named: three countdown updates followed by steady
dispatch installation. Later hits decrement its named five-step gate
without repeating the source award. The zero-gate `03:6136` wait now runs
natively too: no-hit ticks preserve the pending state, while only a later
negative Tower collision emits the typed BS-5/session transition request,
`$41` audio, stage flag, reset gate, and next Tower/insect-cycle handoff. The
shared scene dispatcher remains a separate native owner rather than an
approximation.
Tower capture now validates the fixed steady dispatcher and clear collision
state before carrying its local hit progress and remaining gate count into
native state, keeping an unported collision or presentation transition out of
the ready-target path.
The adjacent Tower Guard now derives its every-frame completion selector
natively from that same gate: nonzero writes closed `$84`, while zero writes
open `$8E` for the BS-5 entrance. Its ordinary mask-collision response remains
an explicit later owner. `capture_tower_guard_actor` accepts only its proven
`03:60A6` selector dispatcher and derives that selector from the captured
Tower gate, rather than copying the Guard's implementation-local field.
When both fixed records have a coherent steady capture, bootstrap now enables
the Guard in native actor order; every `$2E` turn emits the current selector
directly from the seeded Tower gate rather than restoring a redundant value.
`TowerInsectRuleScheduler` now owns the source's four-child budget and live
count, each child's two-hit 10-unit/`$29` rule, its terminal 50-unit release,
and the later 200-unit group-clear handoff. `capture_tower_insect_generation`
now atomically seeds either the ordinary one-to-48-turn source countdown or
the exhausted-budget wait with matching steady children, while retaining
startup, in-flight allocation, collision, and zero-count handoff states with
their original owners. The source coroutine
is inert until the Tower/insect-cycle latch initializes it, then controls the
initial one-turn request and later 48-turn spacing of explicit spawn requests;
allocator request-slot selection remains a separate native owner. The shared
native collision-effect allocator now claims a typed
eight-turn child from an allocator-selected slot, copies its source
appearance, and protects reused source slots during cleanup; burst drawing
remains a presentation concern. A strict live capture can now seed its
remaining lifetime after the linked source has been claimed natively. The selected slot is now claimed as a typed native Tower-insect actor
before a child is committed, carries its `$3900` appearance into native actor
state and shared effects, validates that a captured child matches its source,
and retains its source-copied `$3D00` association so a live shared effect can
be seeded with the child. It releases on its terminal hit. The completed
source now waits for that latch before native reinitialization, rather than
respawning merely because its group-clear award was delivered.

The last slice closed the unsigned square root at `00:E85D-E8A4`: a call-free,
eight-pass restoring leaf isolated from 665 natural calls in `player-session-4`,
all with contract `01:C9D3 -> 00:E85D -> 00:E8A3`. Across every call its
reconstructed `$30:$2F` operands (2 through 11,629) produce exactly
`floor(sqrt(operand))`. It is registry-admitted, driven by a portable CMake
fixture across all 65,536 unsigned operands, and the default registry preserves
all 6,605 per-frame work-RAM hashes with the calls native.

`player-session-5` was a useful initial screen but no longer replays through
the current host. Cabinet-mode v3 `player-session-6` remains the richest
table-behavior capture: it has 1,744 calls to the short actor scan at
`01:C539`, totaling 117,359,643 cycles and 2,379,778 writes. They retain the
same dynamic-handler and queue-filing interior, so the whole scope remains
assembly-backed; the current trace is recorded in
[docs/ROUTE_SCREENING.md](docs/ROUTE_SCREENING.md). Its stored work-RAM hashes
are stale under the current default registry (the normal replay first differs
at frame 4,787), so collision-source extraction uses diagnostic mode and does
not present that historical capture as certification.

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

The existing `03:6054` frame-table animation leaf is now widened from its four
Tower-insect slots to every session-6 actor slot with a valid table index. Its
ordinary registry deployment preserves all 31,971 fingerprints and raises that
key from 31,299 to 69,680 native calls, adding 6,678,228 native master cycles
without expanding a caller-owned actor scope.

The committed `00:E5EC`, `00:E813`, and `00:E825` arithmetic helpers close the
last independently bounded leaf exposed by this capture. The remaining
high-cost `01:C539` actor scan and its `01:C549-C573` interior are evidence for
the first native domain, not candidates for another instruction-shaped
replacement.

## Next actions

- [ ] Add a selectable native `PceRuntime` and a domain-level frame oracle that
      compares player-session replays against the emulator.
- [x] Define typed state views and the native actor-scheduler selection
      boundary. It now exposes actors due for updates without CPU continuations.
- [ ] Seed native actor state, shared motion, the active ball, and active score from capture
      observations, then port ball and actor collision response; use
      `player-session-6` as its first regression corpus. Active shared-motion
      capture transfers verified positions, signed velocities, and due state;
      active-ball capture preserves signed 16.8 positions, signed velocity,
      and physics flags; active-score capture preserves the selected 24-bit
      stored score and presentation latch; `NativeGameplayState` makes each
      captured ball and score authoritative without exposing RAM offsets. The
      position-only rollback and direction-based position retry are native;
      the supported ROM's signed retry vectors are decoded through the active
      MPR map. Its observed carry-sensitive retry probe now returns a typed
      clear/full-scan handoff, and `NativeGameplayState` composes the complete
      CDF8 candidate loop while retaining each full scan exactly once for its
      actor/playfield resolver. Full-scan geometry, normal derivation,
      behavior-specific motion policy, and collision response selection remain
      intentionally outside these generic boundaries.
- [ ] Continue observing the ROM only to name behavior, recover tables, and
      create focused fixtures for native domains. Do not add instruction-shaped
      entries inside composite owners merely to improve the registry metric.
- [ ] Finish display assignment and playfield composition. Everything except
      the last item is done; see the archive for the closed checklist.
  - [ ] **Deferred at user request:** map the two-player/current-player RAM
        contract and add the P1/P2 discriminator to the DMD.
- [ ] Port actor families and table-rule variants after the scheduler/physics
      domain is validated, then move to score/UI, audio, and scene flow.
- [ ] **Milestone 4 deferred:** reproduce additional normal-table and
      bonus-stage rule-family variants as native domains are completed.

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
- The final gameplay implementation is human-readable C++ with typed state and
  subsystem boundaries. Hybrid C/C++ replacement adapters remain temporary
  oracle scaffolding and are not the desired end-state style.
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
