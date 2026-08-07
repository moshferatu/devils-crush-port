# Milestone 2 Coverage Manifest

Last updated: 2026-08-04

## Purpose and classification rules

This is the finite scope-freeze artifact for Milestone 2. It inventories the
software behavior described by the
[official USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf),
the additional fixed actors discovered while mapping the cartridge, and the
instrumentation boundaries required by `PLAN.md`.

Every behavior unit below has exactly one primary classification:

| Code | Classification | Meaning |
| --- | --- | --- |
| **D** | Dynamically verified | A deterministic Beetle route executes the stated behavior and a checked regression guards its semantic evidence. |
| **S** | Statically ROM-mapped | Exact USA-ROM instructions, data, callers, and state contract are documented and guarded, but the stated event outcome is not claimed as dynamically executed. |
| **F** | Deferred | The behavior is explicitly assigned to a later milestone or release-acceptance pass. It is not a Milestone 2 claim. |

There is no “unknown,” “partial,” or unclassified bucket. A mechanism can
appear in more than one row when its ordinary and completion behaviors have
different classifications. “D” means the precise claim in that row was
executed; it does not silently promote every branch of the mechanism.

The detailed addresses, timelines, hashes, and static byte signatures behind
the evidence names are in [ROUTINE_MAP.md](ROUTINE_MAP.md). Generated traces
and ROM-derived assets remain ignored local artifacts.

## Manual frontend, controls, and session inventory

| ID | Manual-facing behavior unit | Class | Evidence / disposition |
| --- | --- | :---: | --- |
| `M-FE-01` | USA ROM boot and title screen | D | `dc_rom_canonical`, `dc_rom_smoke` |
| `M-FE-02` | Play Mode, Speed, and New Game option screens | D | Canonical screen-flow entries and the one-player/default-speed visible save validation |
| `M-FE-03` | Two-player selection and turn alternation | F | Milestone 6 acceptance; the ROM UI remains emulated |
| `M-FE-04` | Slow-speed gameplay selection | F | Milestone 6 acceptance; both-speed exhaustive comparison is outside Milestone 2 |
| `M-FE-05` | Data Load and backup-memory save/reload | D | `dc_rom_backup_reload`; [BACKUP_RAM_VALIDATION.md](BACKUP_RAM_VALIDATION.md) verifies the visible cross-process flow |
| `M-FE-06` | Five save slots displayed and one unused slot saved/loaded | D | Visible backup validation observes all five slots and round-trips slot 1 |
| `M-FE-07` | Full-capacity save deletion prompt and all five populated slots | F | Milestone 6 save acceptance |
| `M-FE-08` | Password creation and password-based restore without backup RAM | F | Milestone 6 acceptance; the legacy UI remains emulated |
| `M-FE-09` | Left and right flippers | D | `dc_rom_controls_physics` |
| `M-FE-10` | Button-I Devil Shooter/plunger | D | `dc_rom_controls_physics`, `dc_rom_devil_shooter` |
| `M-FE-11` | Button-II nudge applied to ball motion | D | Three deterministic nudge pairs in `dc_rom_controls_physics` |
| `M-FE-12` | Tilt lock/gate while ordinary controls remain eligible | D | `$2CC7=0` for all 3,600 closure-route frames |
| `M-FE-13` | Actual tilt accumulation, lockout, and recovery | F | Milestone 4 native session port and Milestone 6 acceptance |
| `M-FE-14` | Pause, High Scores screen, save-screen entry, and resume | D | Visible backup validation |
| `M-FE-15` | RUN+SELECT reset to title | F | Milestone 6 frontend acceptance |
| `M-FE-16` | Ordinary drain with a remaining ball | D | `dc_rom_session_scoring` |
| `M-FE-17` | No-balls game-over transition | F | Milestone 4 session port; exhaustive drain variants are explicitly outside Milestone 2 |
| `M-FE-18` | Last Chance number match and awarded extra ball | F | Milestone 4 session port and Milestone 6 acceptance |
| `M-FE-19` | High-score qualification, three-character name entry, and cancel | F | Milestone 6 frontend acceptance |

## Manual scoring and ball-state inventory

| ID | Manual-facing behavior unit | Class | Evidence / disposition |
| --- | --- | :---: | --- |
| `M-SC-01` | Monster/obstacle awards enter the selected binary score | D | 33 calls in `dc_rom_session_scoring`; broader award set in `dc_rom_score` |
| `M-SC-02` | Stored score to nine visible digits | D | `09:7266/72FD` proves two fixed zero glyphs and `stored × 100` |
| `M-SC-03` | Displayed number of balls | D | `$2D52 - 1` changes panel digits from 02 to 01 across the closure drain |
| `M-SC-04` | BP standard-bonus state and rule updates | S | Exact table-rule producers and fixed presentation controllers are mapped; exhaustive indicator presentation is deferred |
| `M-SC-05` | BT Bonus Tip state and rule updates | S | Exact completion/state contracts are guarded across mapped mechanism families |
| `M-SC-06` | BM Bonus Magnified state and rule updates | S | Exact completion/state contracts are guarded across mapped mechanism families |
| `M-SC-07` | Perfect-stage Bonus Ball, blue ball, and quadrupled normal-table scoring | F | Milestone 4 scoring/session port and Milestone 6 acceptance |
| `M-SC-08` | Bonus-stage earned and special-award transfers | D | All six ordinary scored results and all six natural perfect routes |
| `M-SC-09` | Score saturation and devil-destruction ending | D | `dc_rom_maximum_score_rng`; panel mapping establishes 999,999,900 visible points |

## Manual playfield chart inventory

The manual numbers 1 through 28 are exhaustive for its named normal-table
chart. Aliases in parentheses are the conservative names used by the routine
map.

| ID | Manual chart item and precise behavior unit | Class | Evidence / disposition |
| --- | --- | :---: | --- |
| `M-T01-A` | 1 Devil Shooter: serve, charge, and release the ball | D | `dc_rom_devil_shooter`, `dc_rom_controls_physics` |
| `M-T01-B` | Devil Shooter automatic reset and RNG selector | D | `dc_rom_remaining_normal_rng` |
| `M-T02-04-A` | 2-4 Skeleton Insects: routed source activation and hits | D | `dc_rom_skeleton_insects`, `dc_rom_front_spider`, `dc_rom_dragon_eggs` |
| `M-T02-04-B` | Skeleton-Insect family completion/exit-lane combinations | S | Exact completion signatures in the same regressions |
| `M-T05-A` | 5 Devil's Coffin: session-gated return cycle | D | `dc_rom_devils_coffin` |
| `M-T05-B` | Full eight-hit destruction/BM outcome | S | ROM-bounded completion contract |
| `M-T06-A` | 6 Dragon's Egg: one egg hit, hatchling, score, and release | D | `dc_rom_dragon_eggs` |
| `M-T06-B` | All-six egg completion and reset signal | S | ROM-bounded controller branch |
| `M-T07-A` | 7 Dragon's Head: live collision and score response | D | `dc_rom_dragon_giant_skeleton` |
| `M-T07-B` | Dragon total-destruction mouth/opening result | S | Exact terminal branch retained by the regression |
| `M-T08A-A` | 8a Giant Skeleton: routed live hit | D | `dc_rom_dragon_giant_skeleton` |
| `M-T08A-B` | Giant Skeleton mouth-opening completion | S | Exact completion state machine |
| `M-T08B` | 8b Giant Skeleton's Eyeball: all-eggs signal and opening/reset | S | `dc_rom_giant_skeleton_eyeball` |
| `M-T09A-A` | 9a Tower: routed hit and Tower-owned gate state | D | `dc_rom_tower_family` |
| `M-T09A-B` | Tower total-destruction/BT cycle | S | Exact later states in the Tower regression |
| `M-T09B-A` | 9b Insects Living in Tower: allocation, two-hit rule, score, audio, release | D | `dc_rom_two_hit_target` |
| `M-T09B-B` | All-insects group-clear award and regeneration | S | Exact inactive group-clear branch |
| `M-T10-A` | 10 Guard: closed selector and Tower gate coupling | D | `dc_rom_tower_guard` |
| `M-T10-B` | Five qualifying Tower passes and BS-5 entrance | D | `dc_rom_tower_bonus_entrance` |
| `M-T11A-A` | 11a Giant Face: persisted form/request controller and seven-level transformation registry | S | `dc_rom_giant_face` |
| `M-T11A-B` | Open-mouth capture and BS-4 entrance | S | Exact mouth branch in `dc_rom_giant_face` |
| `M-T11B` | 11b Crown on Giant Face: 300-point hit and form request | S | `dc_rom_giant_face`, `dc_rom_giant_face_helmet` |
| `M-T12-14-A` | 12a-14 Giant-Face/Wall-Pocket access surfaces: layout and idle state | D | `dc_rom_giant_face_helmet`, `dc_rom_side_star_access`, `dc_rom_walled_star_access` |
| `M-T12-14-B` | Their collision awards, Giant-Face transitions, and fixed/random bonus entrances | S | Exact inactive branches in those regressions |
| `M-T15-A` | 15 Small Soldier: two natural contacts, score, audio, counter, and release | D | `dc_rom_collision_boundary` |
| `M-T15-B` | Other Small-Soldier subtype/group completion combinations | S | `dc_rom_branching_two_hit`, `dc_rom_small_soldier_sources` |
| `M-T16` | 16 Giant Door/Wooden Gate: four-hit rule and opening | S | `dc_rom_giant_door` |
| `M-T17` | 17 Skeleton Knight: queue, hits, destruction, and Giant-Door handshake | S | `dc_rom_knights` |
| `M-T18-A` | 18 Small Slimes (Gray Blobs): natural activation and RNG motion timers | D | `dc_rom_gray_blobs`, `dc_rom_gray_blob_rng` |
| `M-T18-B` | Gray-Blob destruction and Slime-Roulette guard outcome | S | Exact inactive hit/completion contracts |
| `M-T19-A` | 19 Slime Roulette (Three Blobs): color controller and equal-color reset RNG | D | `dc_rom_three_blobs_controller`, `dc_rom_three_blobs_reset_rng` |
| `M-T19-B` | All-red/full completion variants | S | `dc_rom_three_blobs` |
| `M-T20-A` | 20 Sorcerer: zero-live controller, redeployment, and RNG selector | D | `dc_rom_sorcerer_rng` |
| `M-T20-B` | Sorcerer two-hit destruction and group completion | S | `dc_rom_sorcerers` |
| `M-T21-A` | 21 Small Skeleton (Little Skull): gray release and binary RNG direction | D | `dc_rom_little_skull_rng` |
| `M-T21-B` | Little-Skull collision, pair clear, regeneration, and Big-Star speed change | S | `dc_rom_big_star_feature_network` |
| `M-T22` | 22 Devil's Gate (Walled Star): five-hit destruction and access handshake | S | `dc_rom_walled_star`, `dc_rom_walled_star_access` |
| `M-T23` | 23 Acquiring Magic (Big Star): seven speeds and eight stopped-arrow consumers | S | `dc_rom_big_star_feature_network` |
| `M-T24-27-A` | 24-27 BS-3/BS-6/BS-1/BS-5 Wall Pockets: records, routes, selectors, and idle state | D | `dc_rom_four_star_wall_pockets` |
| `M-T24-27-B` | Normal pocket return and stopped-arrow special stage entry | S | Both exact branches in the same regression |
| `M-T28-A` | 28 Tube/bonus return: normal-table restoration after bonus mode | D | `dc_rom_bonus_return` |
| `M-T28-B` | Random all-six-stage access pocket selection | S | `dc_rom_walled_star_access` |

## Additional cartridge playfield inventory

These actors are named by the detailed mapping work rather than the USA
manual's numbered chart. Together with `dc_rom_actor_layout`, they close the
88-record/59-initializer census without inventing manual names.

| ID | Behavior unit | Class | Evidence / disposition |
| --- | --- | :---: | --- |
| `C-ACT-01` | All 88 fixed records, 59 initializer identities, and 127-slot ownership | D | `dc_rom_actor_layout`, `dc_rom_actor_activation` |
| `C-ACT-02` | Camera-window activation, persistence, and both selective table loaders | D | `dc_rom_actor_activation`, `dc_rom_scene_layout`, `dc_rom_selective_scene`, `dc_rom_bonus_return` |
| `C-ACT-03` | Lower-table bumper hit phases and terminal child release | D | `dc_rom_bumper_family`, `dc_rom_bumper_terminal` |
| `C-ACT-04` | Front-Spider hits and side-safety latches | D | `dc_rom_front_spider` |
| `C-ACT-05` | Side-Spider collision/transition result | S | `dc_rom_side_spiders` |
| `C-ACT-06` | Upper Wall-Pocket hands, timers, response RNG, and continuation | D | `dc_rom_upper_wall_pocket_hands`, both upper-hand RNG regressions |
| `C-ACT-07` | Undead-Head post-trigger selector and launch RNG | D | `dc_rom_undead_head_selector_rng`, `dc_rom_remaining_normal_rng` |
| `C-ACT-08` | Giant-Face mouth release RNG | D | `dc_rom_remaining_normal_rng` |
| `C-ACT-09` | Three final-ball Mercy Stars: eligibility/use/safety contract | S | `dc_rom_mercy_stars` |
| `C-ACT-10` | Shared impact-burst allocation/presentation effect | D | `dc_rom_impact_burst` |
| `C-ACT-11` | Fixed system and presentation controllers | D | `dc_rom_fixed_system_controllers` |
| `C-ACT-12` | Game-to-audio command dispatch and representative producers | D | `dc_rom_audio_command` and mechanism regressions |

## Bonus-stage inventory

For Milestone 2, each stage requires one bounded event family, timeout,
completion, ordinary scored-but-incomplete result, and natural perfect
result. That finite baseline is complete.

| ID | Behavior unit | Class | Evidence / disposition |
| --- | --- | :---: | --- |
| `M-BS-01` | BS-1 targets/supports, timeout, scored result, completion, and natural perfect | D | Four BS-1 regressions |
| `M-BS-02` | BS-2 emitters, all 28 board cells, timeout/result, completion, and full-contact perfect | D | Four BS-2 regressions |
| `M-BS-03` | BS-3 emitter/objective family, timeout/result, completion, and natural perfect | D | `dc_rom_bonus_stage3`, `dc_rom_bonus_stage3_natural_perfect`, scored-result regression |
| `M-BS-04` | BS-4 linked-target family/RNG, timeout/result, completion, and natural perfect | D | Three BS-4 regressions plus scored-result regression |
| `M-BS-05` | BS-5 15-target field, timeout/result, completion, and natural perfect | D | BS-5 regressions plus scored-result regression |
| `M-BS-06` | BS-6 objective pair/hazard RNG, timeout/result, completion, and natural perfect | D | Three BS-6 regressions plus scored-result regression |
| `M-BS-07` | Additional event families and redundant success/failure/perfect variants | F | Exercise during each Milestone 4 native port or Milestone 6 acceptance |
| `M-BS-08` | Perfect-stage blue Bonus Ball and quadrupled normal-table scoring | F | Same disposition as `M-SC-07` |

## Instrumentation and native-bridge inventory

| ID | Required boundary | Class | Evidence / disposition |
| --- | --- | :---: | --- |
| `I-01` | Complete work-RAM frames, ordered routine entries/calls/writes, callers, returns, and cycle budgets | D | Closure routes and trace schema regressions |
| `I-02` | Controller-to-launch/flipper/nudge-to-physics chain | D | `dc_rom_controls_physics` |
| `I-03` | Confirmed-contact-to-collision/rule/score/audio/release chain | D | `dc_rom_collision_boundary` |
| `I-04` | Drain/session/score-display continuation | D | `dc_rom_session_scoring` |
| `I-05` | All 12 normal-table and nine bonus/ending RNG consumers | D | RNG consumer regressions |
| `I-06` | Transactional copied-RAM native invocation and shadow comparison | D | `dc_rom_native_replacement` |
| `I-07` | Faithful end-to-end replacement of semantic `03:6966@all`, `03:7FB1@all`, composed Small-Soldier no-hit `03:7F57@all`, `00:F1B4@all`, `00:F241@all`, ordinary `00:F55C@all`, RNG `00:E8B3@all`, flipper input/activation `01:D466@all`, flipper phase step `01:D500@all`, phase-indexed flipper geometry `01:D081@all`, shared actor-coroutine scheduler `01:C5C9@all`, session/nudge gate `01:C3CB@all`, and idle session-input gate `01:C49E@all`, plus the bounded rotation/damping fixtures | D | Semantic collision, controls, position, score, RNG, and scheduler regressions plus `dc_rom_native_replacement`, `dc_rom_motion_damping_nonzero`, and `dc_rom_motion_damping_negative` |
| `I-08` | Faithful mid-routine interrupt delivery for interrupt-bearing replacements | D | Position integration/rollback, score, RNG, flipper input/activation, phase-step, geometry, collision-response, coroutine-scheduler, nudge-gate, and session-gate regressions exercise checkpoint transcripts for `00:F1B4`, `00:F241`, `00:F55C`, `00:E8B3`, `01:D466`, `01:D500`, `01:D081`, `03:7FB1`, `01:C5C9`, `01:C3CB`, and `01:C49E` with exact full-route artifacts |

## Explicit post-Milestone-2 acceptance inventory

These requirements remain visible here so they cannot be mistaken for
unclassified work:

| ID | Deferred behavior | Class | Target |
| --- | --- | :---: | --- |
| `A-01` | Both speeds, both player counts, turn alternation, every drain, game over | F | Milestones 4 and 6 |
| `A-02` | Extra balls, Last Chance, blue-ball multiplier, tilt/lockout, known stuck-ball behavior | F | Milestones 4 and 6 |
| `A-03` | Password restore, full five-save lifecycle/capacity, reset, high-score name entry | F | Milestone 6 |
| `A-04` | Exhaustive dynamic reproduction of every statically mapped normal-table branch | F | Port-by-port in Milestone 4 |
| `A-05` | Remaining bonus event families and redundant outcomes | F | Milestones 4 and 6 |
| `A-06` | Cabinet full-table rendering, DMD, backglass, monitor assignment, and bonus switching | F | Milestones 3 and 5 |
| `A-07` | Final packaging, frontend integration, display recovery, remapping, volume, and pause polish | F | Milestone 6 |

## Completeness audit

The USA manual's software sections are exhausted as follows:

| Manual section | Manifest coverage |
| --- | --- |
| Play Mode, Speed, New Game, Password, Data Load | `M-FE-02` through `M-FE-08` |
| Controls, pause, reset, save/password access | `M-FE-09` through `M-FE-15` |
| Last Chance, game over, High Scores/name entry | `M-FE-17` through `M-FE-19` |
| Score display, BP/BT/BM, Bonus Balls | `M-SC-01` through `M-SC-09` |
| Numbered playfield chart 1-28 | `M-T01-A` through `M-T28-B` |
| Backup-memory save flow and five files | `M-FE-05` through `M-FE-07` |
| Six bonus stages reached by the chart's entrances | `M-BS-01` through `M-BS-08` |
| 999,999,900 devil-destruction ending | `M-SC-09` |

Hardware insertion, cleaning, warranty, and TurboGrafx accessory-care text
does not describe game software behavior and is outside the project scope.
Every software behavior described by the manual is therefore **D**, **S**, or
**F** above. Newly discovered branches must be added as a new classified row;
they do not reopen Milestone 2 unless they reveal an omitted manual-facing
behavior family.
